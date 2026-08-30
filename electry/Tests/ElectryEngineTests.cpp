#include "DSP/ElectryEngine.h"
#include "DSP/ElectryFx.h"
#include "DSP/ElectryVisuals.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace electry
{
// Narrow inspection seam for the JUCE-free regression suite.
struct ElectryEngineTestAccess
{
    static float scaleLengthMetres(const ElectryEngine& engine) noexcept
    {
        return engine.scaleLengthMetres();
    }

    struct TouchGeometry
    {
        bool valid { false };
        float fraction { 0.0f };
        float soundingPeriod { 0.0f };
        float verticalRawPeriod { 0.0f };
        float horizontalRawPeriod { 0.0f };
        float verticalRawTarget { 0.0f };
        float verticalCompensatedPeriod { 0.0f };
        float horizontalCompensatedPeriod { 0.0f };
        float verticalReadDelay { 0.0f };
        float horizontalReadDelay { 0.0f };
    };

    static TouchGeometry touchGeometry(
        const ElectryEngine& engine, int stringIndex) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return {};
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        return {
            true,
            voice.touchFraction,
            voice.lastCompensatedPeriod,
            voice.vertical.currentDelay,
            voice.horizontal.currentDelay,
            voice.vertical.targetDelay,
            voice.compensatedPeriodVertical,
            voice.compensatedPeriodHorizontal,
            ElectryEngine::touchReadDelay(
                voice, voice.vertical, voice.compensatedPeriodVertical),
            ElectryEngine::touchReadDelay(
                voice, voice.horizontal, voice.compensatedPeriodHorizontal)
        };
    }

#if ELECTRY_POSITIONED_FRET_COLLISION
    static constexpr float followingFretDelayScale() noexcept
    {
        return ElectryEngine::followingFretDelayScale;
    }

    static float applyPositionedFretCollision(
        float bridgeSample, float followingFretSample, float clearance,
        float contact, float& excess) noexcept
    {
        return ElectryEngine::applyPositionedFretCollision(
            bridgeSample, followingFretSample, clearance, contact, excess);
    }

    static std::array<float, 2> followingFretLinearWeights(
        float delaySamples) noexcept
    {
        ElectryEngine::DelayTap tap;
        tap.setDelay(delaySamples);
        return { tap.linear0, tap.linear1 };
    }

    static float followingFretLinearReadSentinel(
        float delaySamples) noexcept
    {
        ElectryEngine::PolarisationLoop loop;
        loop.clear();
        loop.writeIndex = 3;
        ElectryEngine::DelayTap tap;
        tap.setDelay(delaySamples);
        constexpr int mask = ElectryEngine::delayLineSize - 1;
        const int index = loop.writeIndex - tap.offset;
        loop.line[static_cast<std::size_t>(index & mask)] = 2.0f;
        loop.line[static_cast<std::size_t>((index + 1) & mask)] = 10.0f;
        return loop.readLinearTap(tap);
    }

    static float followingFretCachedDelay(
        const ElectryEngine& engine, int stringIndex) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return 0.0f;
        const auto& tap = engine.voices_[static_cast<std::size_t>(stringIndex)]
                              .artifactFollowingFretTap;
        return static_cast<float>(tap.offset) - tap.linear1;
    }

    static double positionedFretHarmonicGain(int harmonic) noexcept
    {
        constexpr int period = 4096;
        constexpr double twoPi = 6.28318530717958647692;
        ElectryEngine::PolarisationLoop loop;
        loop.clear();
        loop.currentDelay = static_cast<float>(period);
        for (std::size_t index = 0; index < loop.line.size(); ++index)
        {
            loop.line[index] = static_cast<float>(std::sin(
                twoPi * static_cast<double>(harmonic)
                * static_cast<double>(index) / static_cast<double>(period)));
        }

        ElectryEngine::DelayTap tap;
        tap.setDelay(loop.currentDelay
                     * ElectryEngine::followingFretDelayScale);
        double inputEnergy = 0.0;
        double outputEnergy = 0.0;
        for (int index = 0; index < period; ++index)
        {
            loop.writeIndex = index;
            const float bridge = loop.readFractional(loop.currentDelay);
            const float following = loop.readLinearTap(tap);
            float excess = 0.0f;
            const float output = ElectryEngine::applyPositionedFretCollision(
                bridge, following, 0.0f, 1.0f, excess);
            inputEnergy += static_cast<double>(bridge) * bridge;
            outputEnergy += static_cast<double>(output) * output;
        }
        return std::sqrt(outputEnergy / inputEnergy);
    }
#endif

#if ELECTRY_ENERGY_ATTACK_PITCH
    struct AttackPitchState
    {
        float tensionRatio { 0.0f };
        float pendingEnergyJoules { 0.0f };
        float pendingElasticScalePerJoule { 0.0f };
        float frequencyFactor { 1.0f };
        float tensionNewtons { 0.0f };
        bool clearOnRelease { false };
    };

    static AttackPitchState attackPitchState(
        const ElectryEngine& engine, int stringIndex) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return {};
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        return {
            voice.attackPitchTensionRatio,
            voice.pendingAttackPitchEnergyJoules,
            voice.pendingAttackPitchElasticScalePerJoule,
            voice.attackPitchFrequencyFactor,
            voice.stringTensionNewtons,
            voice.pendingAttackPitchClearOnRelease
        };
    }

    static void setAttackPitchAfterLoopCacheAdvanced(
        ElectryEngine& engine, int stringIndex, float frequencyFactor) noexcept
    {
        auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        voice.attackPitchTensionRatio =
            frequencyFactor * frequencyFactor - 1.0f;
        // Reproduce a legato/damping solve consuming the loop-pitch cache
        // before this sub-quantum attack decay accumulates for the pickup.
        voice.lastAttackPitchFrequencyFactor = frequencyFactor;
        engine.configureVoicePitch(voice, false);
    }

    static float stringGauge(const ElectryEngine& engine) noexcept
    {
        return engine.smoothedParameters_.stringGauge;
    }
#endif

    struct VoiceSnapshot
    {
        bool valid { false };
        bool active { false };
        bool keyDown { false };
        bool releasing { false };
        int stringIndex { -1 };
        int midiNote { -1 };
        ElectryEngine::ExpressionId expressionId {
            ElectryEngine::legacyExpressionId
        };
        bool expressionPitchBendFrozen { false };
        float frozenExpressionPitchBendSemitones { 0.0f };
        int fret { -1 };
        PlayStyle playStyle { PlayStyle::Sustain };
        PlayStyle dampingStyle { PlayStyle::Sustain };
        bool strokeIsUp { false };
        bool pendingRepickActive { false };
        PlayStyle pendingPlayStyle { PlayStyle::Sustain };
        bool pendingStrokeIsUp { false };
        bool pendingPreservesVibratoFinger { false };
        std::uint32_t pendingStrokeVariationState { 0u };
        std::uint64_t pendingStartOrder { 0 };
        float verticalDelayTarget { 0.0f };
        float verticalDelayCurrent { 0.0f };
        float horizontalDelayTarget { 0.0f };
        float horizontalDelayCurrent { 0.0f };
        float polarisationCoupling { 0.0f };
        float dispersionLowCoefficient { 0.0f };
        float dispersionHighCoefficient { 0.0f };
        float inharmonicity { 0.0f };
        float dispersionLowPartial { 0.0f };
        float dispersionHighPartial { 0.0f };
        float bodyConductance { 0.0f };
        float bodyLossFactor { 1.0f };
        float loopGain { 0.0f };
        float baseFrequency { 0.0f };
        float lastCompensatedSemitones { 0.0f };
        float lastCompensatedPeriod { 0.0f };
        std::uint64_t startOrder { 0 };
        std::uint64_t strumChordId { 0 };
        std::uint64_t ageSamples { 0 };
        int startDelaySamples { 0 };
        bool pendingContactPreservesRing { false };
        bool sympatheticReady { false };
        float sympatheticEnergy { 0.0f };
        float excitationCombDelay { 0.0f };
        float excitationCombWidth { 0.0f };
        float strokeContactOffsetMetres { 0.0f };
        float strokeForceGain { 1.0f };
        float strokeAngleOffset { 0.0f };
        float strokeWidthScale { 1.0f };
        std::uint32_t strokeVariationState { 0u };
        // The two the picking hand's force and contact patch reach.
        float excitationAmplitude { 0.0f };
        float excitationTransientAmplitude { 0.0f };
        float excitationModalCoefficient { 0.0f };
        float excitationReleaseCoefficient { 0.0f };
#if ELECTRY_ANALYTIC_RELEASE_IC
        float analyticReleaseAmplitude { 0.0f };
        float analyticReleasePluckFraction { 0.0f };
        float analyticReleaseHalfWidthFraction { 0.0f };
        bool analyticReleaseFreshContact { false };
#endif
        float verticalWeight { 0.0f };
        float horizontalWeight { 0.0f };
        int excitationLength { 0 };
        float excitationLoadScale { 0.0f };
        float excitationSlipScale { 0.0f };
        bool excitationInContact { false };
        bool excitationInRelease { false };
        float contactFeedbackGain { 1.0f };
        std::uint32_t artifactNoiseState { 0u };
        int artifactCollisionRemaining { 0 };
        int artifactCollisionLength { 0 };
        float loopDampingCoefficient { 0.0f };
        float vibratoPhase { 0.0f };
        float vibratoRateScale { 1.0f };
        float vibratoDepthScale { 1.0f };
        float vibratoSemitones { 0.0f };
        std::uint32_t vibratoSeed { 0u };
        std::uint32_t vibratoCycle { 0u };
    };

    static VoiceSnapshot snapshot(const ElectryEngine& engine, int stringIndex)
    {
        VoiceSnapshot result;
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return result;
        const auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        result.valid = true;
        result.active = voice.active;
        result.keyDown = voice.keyDown;
        result.releasing = voice.releasing;
        result.stringIndex = voice.stringIndex;
        result.midiNote = voice.midiNote;
        result.expressionId = voice.expressionId;
        result.expressionPitchBendFrozen = voice.expressionPitchBendFrozen;
        result.frozenExpressionPitchBendSemitones =
            voice.frozenExpressionPitchBendSemitones;
        result.fret = voice.fret;
        result.playStyle = voice.playStyle;
        result.dampingStyle = voice.dampingStyle;
        result.strokeIsUp = voice.strokeIsUp;
        result.pendingRepickActive = voice.pendingRepick.active;
        result.pendingPlayStyle = voice.pendingRepick.playStyle;
        result.pendingStrokeIsUp = voice.pendingRepick.strokeIsUp;
        result.pendingPreservesVibratoFinger =
            voice.pendingRepick.preservesVibratoFinger;
        result.pendingStrokeVariationState =
            voice.pendingRepick.strokeVariationState;
        result.pendingStartOrder = voice.pendingRepick.startOrder;
        result.verticalDelayTarget = voice.vertical.targetDelay;
        result.verticalDelayCurrent = voice.vertical.currentDelay;
        result.horizontalDelayTarget = voice.horizontal.targetDelay;
        result.horizontalDelayCurrent = voice.horizontal.currentDelay;
        result.polarisationCoupling = voice.polarisationCoupling;
        result.dispersionLowCoefficient = voice.vertical.dispersionLowCoefficient;
        result.dispersionHighCoefficient = voice.vertical.dispersionHighCoefficient;
        result.inharmonicity = voice.inharmonicity;
        result.dispersionLowPartial = voice.dispersionLowPartial;
        result.dispersionHighPartial = voice.dispersionHighPartial;
        result.bodyConductance = voice.bodyConductance;
        result.bodyLossFactor = voice.bodyLossFactor;
        result.loopGain = voice.vertical.loopGain;
        result.baseFrequency = voice.baseFrequency;
        result.lastCompensatedSemitones = voice.lastCompensatedSemitones;
        result.lastCompensatedPeriod = voice.lastCompensatedPeriod;
        result.startOrder = voice.startOrder;
        result.strumChordId = voice.strumChordId;
        result.ageSamples = voice.ageSamples;
        result.startDelaySamples = voice.startDelaySamples;
        result.pendingContactPreservesRing =
            voice.pendingContactPreservesRing;
        result.sympatheticReady = voice.sympatheticReady;
        result.sympatheticEnergy = voice.sympatheticEnergy;
        result.excitationCombDelay = voice.excitationCombDelay;
        result.excitationCombWidth = voice.excitationCombWidth;
        result.strokeContactOffsetMetres = voice.strokeContactOffsetMetres;
        result.strokeForceGain = voice.strokeForceGain;
        result.strokeAngleOffset = voice.strokeAngleOffset;
        result.strokeWidthScale = voice.strokeWidthScale;
        result.strokeVariationState = voice.strokeVariationState;
        result.excitationAmplitude = voice.excitationAmplitude;
        result.excitationTransientAmplitude =
            voice.excitationTransientAmplitude;
        result.excitationModalCoefficient =
            voice.excitationModalCoefficient;
        result.excitationReleaseCoefficient =
            voice.excitationReleaseCoefficient;
#if ELECTRY_ANALYTIC_RELEASE_IC
        result.analyticReleaseAmplitude = voice.analyticReleaseAmplitude;
        result.analyticReleasePluckFraction =
            voice.analyticReleasePluckFraction;
        result.analyticReleaseHalfWidthFraction =
            voice.analyticReleaseHalfWidthFraction;
        result.analyticReleaseFreshContact =
            voice.analyticReleaseFreshContact;
#endif
        result.verticalWeight = voice.verticalWeight;
        result.horizontalWeight = voice.horizontalWeight;
        result.excitationLength = voice.excitationLength;
        result.excitationLoadScale = voice.excitationLoadScale;
        result.excitationSlipScale = voice.excitationSlipScale;
        result.excitationInContact =
            voice.excitationPhase == ElectryEngine::ExcitationPhase::Contact;
        result.excitationInRelease =
            voice.excitationPhase == ElectryEngine::ExcitationPhase::Release;
        result.contactFeedbackGain = voice.contactFeedbackGain;
        result.artifactNoiseState = voice.artifactNoiseState;
        result.artifactCollisionRemaining = voice.artifactCollisionRemaining;
        result.artifactCollisionLength = voice.artifactCollisionLength;
        result.loopDampingCoefficient = voice.vertical.loopDampingCoefficient;
        result.vibratoPhase = voice.vibratoPhase;
        result.vibratoRateScale = voice.vibratoRateScale;
        result.vibratoDepthScale = voice.vibratoDepthScale;
        result.vibratoSemitones = voice.vibratoSemitones;
        result.vibratoSeed = voice.vibratoSeed;
        result.vibratoCycle = voice.vibratoCycle;
        return result;
    }

    // The shared depth envelope the fretting hand's vibrato rides on, read
    // straight off the engine so the onset's shape can be measured without
    // the oscillation on top of it.
    static float vibratoDepthEnvelope(const ElectryEngine& engine) noexcept
    {
        return engine.vibratoAmount_;
    }

    // The performance-gesture target setVibrato() writes, read straight off the
    // engine so its own sanitisation guard can be checked directly rather
    // than only through whatever it happens to do to a rendered voice.
    static float vibratoTarget(const ElectryEngine& engine) noexcept
    {
        return engine.vibratoTarget_;
    }

    static float tremoloPickingVelocity(
        const ElectryEngine& engine) noexcept
    {
        return engine.tremoloPickingVelocity_;
    }

    static double tremoloPickingPhase(const ElectryEngine& engine) noexcept
    {
        return engine.tremoloPickingPhase_;
    }

    static std::uint64_t noteSequence(const ElectryEngine& engine) noexcept
    {
        return engine.noteSequence_;
    }

    // The bipolar bend target setPitchBend() writes, read straight off the
    // engine so its own sanitisation guard - fold non-finite input to zero,
    // clamp to [-1, 1], then double to the +/-2 semitone bend range - can be
    // checked directly rather than only through a rendered voice's pitch.
    static float pitchBendTarget(const ElectryEngine& engine) noexcept
    {
        return engine.pitchBendTarget_;
    }

    static void snapPitchBendToTarget(ElectryEngine& engine) noexcept
    {
        engine.pitchBendSemitones_ = engine.pitchBendTarget_;
    }

    static float expressionPitchBendTarget(
        const ElectryEngine& engine,
        ElectryEngine::ExpressionId expressionId) noexcept
    {
        return engine.expressionPitchBendTargets_[
            static_cast<std::size_t>(expressionId)];
    }

    static float expressionPitchBendSemitones(
        const ElectryEngine& engine,
        ElectryEngine::ExpressionId expressionId) noexcept
    {
        return engine.expressionPitchBendSemitones_[
            static_cast<std::size_t>(expressionId)];
    }

    static float expressionMasterPitchBendTarget(
        const ElectryEngine& engine,
        ElectryEngine::ExpressionId expressionId) noexcept
    {
        return engine.expressionMasterPitchBendTargets_[
            static_cast<std::size_t>(expressionId)];
    }

    static float expressionMasterPitchBendSemitones(
        const ElectryEngine& engine,
        ElectryEngine::ExpressionId expressionId) noexcept
    {
        return engine.expressionMasterPitchBendSemitones_[
            static_cast<std::size_t>(expressionId)];
    }

    static ElectryEngine::ExpressionId heldExpressionId(
        const ElectryEngine& engine, int stringIndex) noexcept
    {
        return engine.heldExpressionIds_[static_cast<std::size_t>(stringIndex)];
    }

    // The bridge-pickup resonance target setResonance() writes, the
    // acoustic-return level target setAcousticReturnLevel() writes, and the
    // palm-mute pressure setPalmMutePressure() writes, read straight off the
    // engine so each guard - fold non-finite input to zero, then clamp to
    // [0, 1] - can be checked directly rather than only through whatever it
    // happens to do to a rendered voice.
    static float resonanceTarget(const ElectryEngine& engine) noexcept
    {
        return engine.resonanceTarget_;
    }

    static float returnLevelTarget(const ElectryEngine& engine) noexcept
    {
        return engine.returnLevelTarget_;
    }

    static float palmMutePressure(const ElectryEngine& engine) noexcept
    {
        return engine.palmMutePressure_;
    }

    static float palmMuteBlend(const ElectryEngine& engine) noexcept
    {
        return engine.palmMuteBlend_;
    }

    // The acoustic-return FIFO pushAcousticReturn() appends to and the render
    // loop drains, read straight off the engine so its guards and fixed
    // silent lead-in can be checked independently of the later string drive.
    static int feedbackAvailable(const ElectryEngine& engine) noexcept
    {
        return engine.feedbackAvailable_;
    }

    static float feedbackCurrent(const ElectryEngine& engine) noexcept
    {
        return engine.feedbackCurrent_;
    }

    // The sample `offset` places after the ring's current read pointer, i.e.
    // the order pushAcousticReturn's own writes will be handed to the render
    // loop.
    static float feedbackRingSample(const ElectryEngine& engine,
                                    int offset) noexcept
    {
        const int index = (engine.feedbackReadIndex_ + offset)
                         & (ElectryEngine::feedbackRingSize - 1);
        return engine.feedbackRing_[static_cast<std::size_t>(index)];
    }

    static constexpr int feedbackRingCapacity() noexcept
    {
        return ElectryEngine::feedbackRingSize;
    }

    static bool channelsLinked(const ElectryEngine& engine) noexcept
    {
        return engine.channelsLinked_;
    }

    // The played strings' share of the bridge bus: the gain each voice reads
    // the others' summed force at, and the row-sum norm that bounds it. Read
    // off the engine rather than recomputed, so the stability contract is
    // asserted on the number that actually runs.
    static float bridgeCouplingGain(const ElectryEngine& engine) noexcept
    {
        return engine.bridgeCouplingInjection_;
    }

    static float bridgeCouplingRowSum(const ElectryEngine& engine) noexcept
    {
        return engine.bridgeCouplingRowSum_;
    }

    static float sympatheticHandGainTarget(
        const ElectryEngine& engine) noexcept
    {
        return engine.sympatheticHandGainTarget_;
    }

    static float sympatheticHandGain(const ElectryEngine& engine) noexcept
    {
        return engine.sympatheticHandGain_;
    }

    static void setSympatheticEnergy(ElectryEngine& engine, int stringIndex,
                                     float energy) noexcept
    {
        engine.voices_[static_cast<std::size_t>(stringIndex)]
            .sympatheticEnergy = energy;
    }

    static float voiceOutputEnergy(const ElectryEngine& engine,
                                   int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .outputEnergy;
    }

    static void retriggerVoice(
        ElectryEngine& engine, int stringIndex, int midiNote, float velocity,
        PlayStyle playStyle = PlayStyle::Sustain,
        int startDelaySamples = 0) noexcept
    {
        engine.startVoice(
            engine.voices_[static_cast<std::size_t>(stringIndex)], midiNote,
            velocity, playStyle, false, startDelaySamples,
            engine.strokeVariationStateFor(engine.noteSequence_ + 1,
                                           stringIndex));
    }

    // The hand's loss dip sits inside the feedback loop, so its magnitude must
    // never exceed one at any frequency. Read from the live voice rather than
    // recomputed, so the check is on what actually runs.
    static void handDip(const ElectryEngine& engine, int stringIndex,
                        double& b0, double& b1, double& b2, double& a1,
                        double& a2, bool& active) noexcept
    {
        const auto& loop = engine.voices_[static_cast<std::size_t>(stringIndex)].vertical;
        b0 = loop.handDip.b0;
        b1 = loop.handDip.b1;
        b2 = loop.handDip.b2;
        a1 = loop.handDip.a1;
        a2 = loop.handDip.a2;
        active = loop.handDipActive;
    }

#if ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2
    struct FittedLossState
    {
        double b0, b1, b2, a1, a2;
        float peakDb;
        bool active;

        bool operator==(const FittedLossState&) const = default;
    };

    static FittedLossState fittedLossState(
        const ElectryEngine& engine, int stringIndex,
        bool horizontal = false) noexcept
    {
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& loop = horizontal ? voice.horizontal : voice.vertical;
        return { loop.fittedLossDip.b0, loop.fittedLossDip.b1,
                 loop.fittedLossDip.b2, loop.fittedLossDip.a1,
                 loop.fittedLossDip.a2,
                 loop.fittedLossDepth * loop.fittedLossShape.dipFullDepthDb,
                 loop.fittedLossDipActive };
    }

    static void legatoRetargetVoice(ElectryEngine& engine, int stringIndex,
                                    int midiNote,
                                    PlayStyle style = PlayStyle::Slide) noexcept
    {
        engine.legatoRetarget(
            engine.voices_[static_cast<std::size_t>(stringIndex)], midiNote,
            0.85f, style);
    }
#endif

    static float handLossDepth(const ElectryEngine& engine,
                               int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .vertical.handLossDepth;
    }

    static float solvedHandLossDepth(const ElectryEngine& engine,
                                     int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .vertical.handLossSolvedDepth;
    }

    static std::int64_t lastHandContactClock(
        const ElectryEngine& engine) noexcept
    {
        return engine.lastHandContactClock_;
    }

    static std::uint64_t lastHandContactOrder(
        const ElectryEngine& engine) noexcept
    {
        return engine.lastHandContactOrder_;
    }

    static PlayStyle lastHandContactPlayStyle(
        const ElectryEngine& engine) noexcept
    {
        return engine.lastHandContactPlayStyle_;
    }

    static bool pickupPathActive(const ElectryEngine& engine, bool neck) noexcept
    {
        return neck ? engine.neckPathActive_ : engine.bridgePathActive_;
    }

    // Is the humbucker's second coil running on this string's selected pickup?
    static bool coilPairActive(const ElectryEngine& engine,
                               int stringIndex, bool neck = false) noexcept
    {
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        return (neck ? voice.coilPairNeck : voice.coilPairBridge).paired;
    }

    static std::array<bool, 3> pickupHistory(
        const ElectryEngine& engine, int stringIndex, bool neck) noexcept
    {
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& aperture = neck ? voice.apertureNeck : voice.apertureBridge;
        const auto& coil = neck ? voice.coilPairNeck : voice.coilPairBridge;
        const float previousFlux = neck
            ? voice.previousFluxNeck : voice.previousFluxBridge;
        const float emf = neck
            ? voice.emfLowpassNeck.state : voice.emfLowpassBridge.state;
        return {
            std::any_of(
                aperture.cumulativeHistory.begin(),
                aperture.cumulativeHistory.end(),
                [] (double sample) { return sample != 0.0; }),
            std::any_of(coil.history.begin(), coil.history.end(),
                        [] (float sample) { return sample != 0.0f; }),
            previousFlux != 0.0f || emf != 0.0f
        };
    }

    struct PickupGeometry
    {
        float waveSpeedRatio { 1.0f };
        float neckTapDelay { 0.0f };
        float bridgeTapDelay { 0.0f };
        float apertureDelay { 0.0f };
        float coilDelay { 0.0f };
    };

    static PickupGeometry pickupGeometry(const ElectryEngine& engine,
                                         int stringIndex) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return {};
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto tapDelay = [] (const ElectryEngine::DelayTap& tap)
        {
            const float fractional = -tap.c0 + tap.c2 + 2.0f * tap.c3;
            return static_cast<float>(tap.offset) - fractional;
        };
        return {
            voice.pickupWaveSpeedRatio,
            tapDelay(voice.pickupTapNeck),
            tapDelay(voice.pickupTapBridge),
            static_cast<float>(voice.apertureBridge.windowWhole)
                + static_cast<float>(voice.apertureBridge.windowFraction),
            static_cast<float>(voice.coilPairBridge.spacingWhole)
                + voice.coilPairBridge.spacingFraction
        };
    }

    static bool refreshPickupWaveSpeedPreservesHistory(
        ElectryEngine& engine, int stringIndex, float ratio) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return false;
        auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto aperture = voice.apertureBridge;
        const auto coil = voice.coilPairBridge;
        const float previousFluxBridge = voice.previousFluxBridge;
        const float emfBridge = voice.emfLowpassBridge.state;
        const bool populated = std::any_of(
            aperture.cumulativeHistory.begin(), aperture.cumulativeHistory.end(),
            [] (double sample) { return sample != 0.0; })
            && std::any_of(coil.history.begin(), coil.history.end(),
                           [] (float sample) { return sample != 0.0f; })
            && previousFluxBridge != 0.0f && emfBridge != 0.0f;

        voice.pickupWaveSpeedRatio = ratio;
        engine.configureVoicePickups(voice);

        return populated
            && voice.apertureBridge.cumulativeHistory
                    == aperture.cumulativeHistory
            && voice.apertureBridge.cumulative == aperture.cumulative
            && voice.apertureBridge.writeIndex == aperture.writeIndex
            && voice.coilPairBridge.history == coil.history
            && voice.coilPairBridge.writeIndex == coil.writeIndex
            && voice.previousFluxBridge == previousFluxBridge
            && voice.emfLowpassBridge.state == emfBridge;
    }

    // The pickup's spatial transfer at one frequency, evaluated in closed form
    // from the two stages the voice is actually running: the humbucker's coil
    // pair and the per-coil aperture window. Both are FIR, so this is exact
    // rather than a fit.
    //
    // The position comb is deliberately not included. It is a separate stage
    // with nulls of its own at every multiple of c/2x, several of which land
    // between 2 and 8 kHz, so folding it in would leave "the deepest notch"
    // ambiguous. And the notch is read here rather than off a rendered
    // spectrum because a null between two harmonics is sampled only as finely
    // as the string's fundamental spacing.
    static double apertureChainMagnitude(const ElectryEngine& engine,
                                         int stringIndex, double frequencyHz,
                                         bool includeCoilPair)
    {
        constexpr double pi = 3.14159265358979323846;
        const auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& window = voice.apertureBridge;
        const auto& pair = voice.coilPairBridge;
        const double omega = 2.0 * pi * frequencyHz / engine.sampleRate_;

        // Rectangular window: unit taps at 0..W-1 plus a fractional tap at W,
        // scaled by the inverse window length.
        double real = 0.0;
        double imaginary = 0.0;
        for (int k = 0; k < window.windowWhole; ++k)
        {
            real += std::cos(omega * k);
            imaginary -= std::sin(omega * k);
        }
        const double edge = omega * static_cast<double>(window.windowWhole);
        real += window.windowFraction * std::cos(edge);
        imaginary -= window.windowFraction * std::sin(edge);
        double magnitude = std::hypot(real, imaginary) * window.inverseWindow;

        if (includeCoilPair && pair.paired)
        {
            const double near = omega * static_cast<double>(pair.spacingWhole);
            const double far = omega * static_cast<double>(pair.spacingWhole + 1);
            const double nearWeight = 1.0
                - static_cast<double>(pair.spacingFraction);
            const double farWeight = static_cast<double>(pair.spacingFraction);
            const double balance = static_cast<double>(pair.balance);
            const double pairReal = 1.0
                + balance * (nearWeight * std::cos(near)
                             + farWeight * std::cos(far));
            const double pairImaginary =
                -balance * (nearWeight * std::sin(near)
                            + farWeight * std::sin(far));
            magnitude *= std::hypot(pairReal, pairImaginary)
                       * static_cast<double>(pair.normalise);
        }
        return magnitude;
    }

    static int oversamplingFactor(const ElectryEngine& engine) noexcept
    {
        return engine.oversamplingFactor_;
    }

    static double hostSampleRate(const ElectryEngine& engine) noexcept
    {
        return engine.hostSampleRate_;
    }

    static double internalSampleRate(const ElectryEngine& engine) noexcept
    {
        return engine.sampleRate_;
    }

    static float voiceDelayRetention(const ElectryEngine& engine) noexcept
    {
        return engine.voiceDelayRetention_;
    }

    static float onePoleMagnitude(float coefficient, float omega) noexcept
    {
        return ElectryEngine::onePoleMagnitude(coefficient, omega);
    }

    static double realisedVoiceFundamentalT60(
        const ElectryEngine& engine, int stringIndex) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return 0.0;
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& loop = voice.vertical;
        const double rate = engine.sampleRate_;
        const double period = voice.lastCompensatedPeriod;
        if (! (period > 0.0))
            return 0.0;

        const float omega = static_cast<float>(
            2.0 * 3.14159265358979323846 / period);
        float handMagnitude = 1.0f;
        float unusedPhase = 0.0f;
        ElectryEngine::handLossResponse(
            loop.handLossDepth, loop.handLossShape, omega,
            handMagnitude, unusedPhase);
#if ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2
        float fittedMagnitude = 1.0f;
        ElectryEngine::handLossResponse(
            loop.fittedLossDepth, loop.fittedLossShape, omega,
            fittedMagnitude, unusedPhase);
#else
        constexpr float fittedMagnitude = 1.0f;
#endif
        const double perRoundTrip = loop.loopGain
            * ElectryEngine::onePoleMagnitude(
                loop.loopDampingCoefficient, omega)
            * handMagnitude * fittedMagnitude;
        if (! (perRoundTrip > 0.0 && perRoundTrip < 1.0))
            return 0.0;
        return -3.0 * period / (rate * std::log10(perRoundTrip));
    }

    static int chordPerformanceDelaySamples(
        const ElectryEngine& engine) noexcept
    {
        return engine.chordPerformanceDelaySamples_;
    }

    static double minimumSupportedSampleRate() noexcept
    {
        return ElectryEngine::minimumSupportedSampleRate;
    }

    static double maximumSupportedSampleRate() noexcept
    {
        return ElectryEngine::maximumSupportedSampleRate;
    }

    static float dispersionDeficit(const ElectryEngine& engine,
                                    int stringIndex, float partial) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return 0.0f;
        const auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& loop = voice.vertical;
        const float omega = 2.0f * 3.14159265358979323846f
                          * voice.lastConfiguredFrequency
                          / static_cast<float>(engine.sampleRate_);
        const float omegaPartial = std::min(
            omega * partial, 3.14159265358979323846f * 0.95f);
        const auto sectionDeficit = [&] (float coefficient)
        {
            return ElectryEngine::allpassPhaseDelay(coefficient, omega)
                 - ElectryEngine::allpassPhaseDelay(coefficient, omegaPartial);
        };
        return 4.0f * sectionDeficit(loop.dispersionLowCoefficient)
             + 4.0f * sectionDeficit(loop.dispersionHighCoefficient);
    }

    static std::array<float, 9> loopFilterState(
        const ElectryEngine& engine, int stringIndex,
        bool horizontal = false) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return {};
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& loop = horizontal ? voice.horizontal : voice.vertical;
        return {
            loop.damping.state,
            loop.dispersion1.state, loop.dispersion2.state,
            loop.dispersion3.state, loop.dispersion4.state,
            loop.dispersion5.state, loop.dispersion6.state,
            loop.dispersion7.state, loop.dispersion8.state
        };
    }

    static double loopLineEnergy(const ElectryEngine& engine,
                                 int stringIndex,
                                 bool horizontal = false) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return 0.0;
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& line = (horizontal ? voice.horizontal : voice.vertical).line;
        double energy = 0.0;
        for (const float sample : line)
            energy += static_cast<double>(sample) * sample;
        return energy;
    }

    static std::array<double, 2> loopHandDipState(
        const ElectryEngine& engine, int stringIndex,
        bool horizontal = false) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return {};
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& dip = (horizontal ? voice.horizontal : voice.vertical).handDip;
        return { dip.z1, dip.z2 };
    }

#if ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2
    static std::array<double, 2> loopFittedLossDipState(
        const ElectryEngine& engine, int stringIndex,
        bool horizontal = false) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return {};
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& dip =
            (horizontal ? voice.horizontal : voice.vertical).fittedLossDip;
        return { dip.z1, dip.z2 };
    }
#endif

    static double modalMagnitudeAt(float frequencyHz, float q, float modeGain,
                                   float sampleRate,
                                   float evaluationFrequencyHz) noexcept
    {
        ElectryEngine::ModalResonator resonator;
        resonator.configure(frequencyHz, q, modeGain, sampleRate);

        const double omega = 2.0 * 3.14159265358979323846
                           * static_cast<double>(evaluationFrequencyHz)
                           / static_cast<double>(sampleRate);
        const double denominatorReal = 1.0
            + static_cast<double>(resonator.a1) * std::cos(omega)
            + static_cast<double>(resonator.a2) * std::cos(2.0 * omega);
        const double denominatorImag =
            -static_cast<double>(resonator.a1) * std::sin(omega)
            -static_cast<double>(resonator.a2) * std::sin(2.0 * omega);
        return std::abs(static_cast<double>(resonator.gain))
             / std::max(std::hypot(denominatorReal, denominatorImag), 1.0e-20);
    }

#if ELECTRY_MEASURED_BODY_RESPONSE
    static constexpr int bodyModeCount() noexcept
    {
        return ElectryEngine::bodyModeCount;
    }

    static float bodyModeFrequency(const ElectryEngine& engine,
                                   int mode) noexcept
    {
        return engine.bodyModeOmega_[static_cast<std::size_t>(mode)]
             / (2.0f * 3.14159265358979323846f);
    }

    static float bodyModeQ(const ElectryEngine& engine, int mode) noexcept
    {
        const auto index = static_cast<std::size_t>(mode);
        return engine.bodyModeOmega_[index] / engine.bodyModeDamping_[index];
    }

    static float bodyModeLevel(const ElectryEngine& engine, int mode) noexcept
    {
        return engine.bodyModeLevels_[static_cast<std::size_t>(mode)];
    }

    static float bodyOutputLevel(const ElectryEngine& engine) noexcept
    {
        return engine.smoothedBodyLevel_;
    }

    static void muteDirectBodyPath(ElectryEngine& engine) noexcept
    {
        engine.smoothedBodyLevel_ = 0.0f;
        for (auto& mode : engine.bodyModes_)
            mode.gain = 0.0f;
    }

    static float intrinsicT60BeforeMeasuredBody(
        const ElectryEngine& engine, int stringIndex, int fret) noexcept
    {
        const auto& spec = ElectryEngine::stringSpecs()[
            static_cast<std::size_t>(stringIndex)];
        const auto& p = engine.smoothedParameters_;
        float t60 = spec.t60Seconds;
        t60 *= lerp(1.08f, 0.38f, p.stringAge);
        t60 *= 1.16f;
        t60 *= lerp(0.88f, 1.18f, p.stringGauge);
        t60 *= 1.05f * 1.04f;
        t60 *= engine.deadSpotFactor(stringIndex, fret);
        return clampf(t60, 0.02f, 26.0f);
    }
#endif

    static constexpr int delayLineCapacity() noexcept
    {
        return ElectryEngine::delayLineSize;
    }

#if ELECTRY_ANALYTIC_RELEASE_IC
    struct AnalyticReleaseSeed
    {
        float delay { 0.0f };
        float fractionalRead { 0.0f };
        std::vector<float> cells;
    };

    static AnalyticReleaseSeed analyticReleaseSeed(
        float delay, float peak, float pluckFraction,
        float halfWidthFraction = 0.0f, float baselineScale = 0.0f)
    {
        ElectryEngine::PolarisationLoop loop;
        loop.clear();
        loop.writeIndex = 317;
        loop.currentDelay = delay;
        if (baselineScale != 0.0f)
        {
            for (std::size_t index = 0; index < loop.line.size(); ++index)
            {
                const int centred = static_cast<int>(index % 17u) - 8;
                loop.line[index] = baselineScale
                                 * static_cast<float>(centred);
            }
        }
        ElectryEngine::seedReleasedDisplacement(
            loop, peak, pluckFraction, halfWidthFraction, 1.0f);

        AnalyticReleaseSeed result;
        result.delay = delay;
        result.fractionalRead = loop.readFractional(delay);
        const int cellCount = static_cast<int>(std::ceil(delay)) + 1;
        result.cells.reserve(static_cast<std::size_t>(cellCount));
        constexpr int mask = ElectryEngine::delayLineSize - 1;
        for (int cell = 0; cell < cellCount; ++cell)
        {
            result.cells.push_back(loop.line[static_cast<std::size_t>(
                (loop.writeIndex - 1 - cell) & mask)]);
        }
        return result;
    }

    struct AnalyticPickupTrajectory
    {
        std::array<float, 32> displacement {};
        std::array<float, 32> faradayDifference {};
    };

    static AnalyticPickupTrajectory analyticReleasePickupTrajectory(
        float period, float peak, float pluckFraction, float tapDelay,
        float apertureLength, float coilSpacing)
    {
        ElectryEngine::Voice voice;
        voice.vertical.clear();
        voice.horizontal.clear();
        voice.vertical.writeIndex = 317;
        voice.horizontal.writeIndex = 317;
        voice.vertical.currentDelay = period;
        voice.horizontal.currentDelay = period;
        voice.analyticReleaseAmplitude = peak;
        voice.analyticReleasePluckFraction = pluckFraction;
        voice.analyticReleaseHalfWidthFraction = 0.0f;
        voice.excitationPolarity = 1.0f;
        voice.verticalWeight = 1.0f;
        voice.horizontalWeight = 0.0f;
        ElectryEngine::seedReleasedDisplacement(
            voice.vertical, peak, pluckFraction, 0.0f, 1.0f);
        voice.pickupTapNeck.setDelay(tapDelay);
        voice.apertureNeck.setWindow(apertureLength);
        voice.coilPairNeck.setSpacing(coilSpacing, 0.60f);
        ElectryEngine::primeReleasedPickupHistory(
            voice, voice.pickupTapNeck,
            voice.apertureNeck, voice.coilPairNeck);

        AnalyticPickupTrajectory result;
        float previous = 0.0f;
        constexpr int mask = ElectryEngine::delayLineSize - 1;
        for (std::size_t sample = 0; sample < result.displacement.size();
             ++sample)
        {
            const float direct = voice.vertical.readFractional(period);
            voice.vertical.line[static_cast<std::size_t>(
                voice.vertical.writeIndex & mask)] = direct;
            const float tapInput = 0.85f * (direct - 0.60f
                * voice.vertical.readTap(voice.pickupTapNeck));
            const float displacement = voice.apertureNeck.process(
                voice.coilPairNeck.process(tapInput));
            if (sample == 0)
                previous = displacement;
            result.displacement[sample] = displacement;
            result.faradayDifference[sample] = displacement - previous;
            previous = displacement;
            voice.vertical.writeIndex = (voice.vertical.writeIndex + 1) & mask;
        }
        return result;
    }

    static double playedLoopSquaredSum(const ElectryEngine& engine,
                                       int stringIndex) noexcept
    {
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        double sum = 0.0;
        for (const auto* loop : { &voice.vertical, &voice.horizontal })
            for (const float sample : loop->line)
                sum += static_cast<double>(sample) * sample;
        return sum;
    }
#endif

    struct DelayTapSnapshot
    {
        int offset { 0 };
        float c0 { 0.0f }, c1 { 0.0f }, c2 { 0.0f }, c3 { 0.0f };
    };

    // A fresh DelayTap solved for one requested delay, so its own clamp and
    // cubic-Lagrange coefficient solve can be checked directly rather than
    // only through whatever played or sympathetic pickup position a
    // configured voice happens to land on.
    static DelayTapSnapshot delayTapAt(float delaySamples) noexcept
    {
        ElectryEngine::DelayTap tap;
        tap.setDelay(delaySamples);
        return { tap.offset, tap.c0, tap.c1, tap.c2, tap.c3 };
    }

    static int stringForNote(const ElectryEngine& engine, int midiNote)
    {
        for (int s = 0; s < ElectryEngine::stringCount; ++s)
        {
            const auto& voice = engine.voices_[static_cast<std::size_t>(s)];
            if (voice.active && voice.midiNote == midiNote)
                return s;
        }
        return -1;
    }

    static int chosenString(const ElectryEngine& engine, int midiNote,
                            PlayStyle style,
                            ElectryEngine::ExpressionId expressionId =
                                ElectryEngine::legacyExpressionId) noexcept
    {
        return engine.chooseString(midiNote, style, expressionId);
    }

    static void markPendingRepick(ElectryEngine& engine,
                                  int stringIndex) noexcept
    {
        engine.voices_[static_cast<std::size_t>(stringIndex)]
            .pendingRepick.active = true;
    }

    // Select one deterministic picking-hand draw without having to render and
    // discard preceding notes. reset() starts the counter at zero and noteOn()
    // increments it before drawing, so `precedingNotes` names that exact state.
    static void setPrecedingNoteCount(ElectryEngine& engine,
                                      std::uint64_t precedingNotes) noexcept
    {
        engine.noteSequence_ = precedingNotes;
    }

    static float handContactScale(const ElectryEngine& engine,
                                  int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .handContactScale;
    }

    static float palmImpactVelocity(const ElectryEngine& engine,
                                    int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .palmImpactVel;
    }

    static float palmImpactState(const ElectryEngine& engine,
                                 int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .palmImpactState;
    }

    static float excitationPulseCoefficient(const ElectryEngine& engine,
                                             int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .excitationPulseCoefficient;
    }

    static float noiseBandCoefficient(const ElectryEngine& engine,
                                      int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .noiseBandCoefficient;
    }

    // Counterfactual used only by the paired regression below: keep the same
    // stroke and replace its hand-contact rate multiplier with the old unity
    // behaviour, then refresh the two dependent solves before audio runs.
    static void forceUnityHandContact(ElectryEngine& engine,
                                      int stringIndex) noexcept
    {
        auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        voice.handContactScale = 1.0f;
        engine.configureVoiceDamping(voice, voice.dampingStyle);
        engine.configureVoicePitch(voice, false);
    }

    static void refitVoiceDampingAtCachedCoordinate(
        ElectryEngine& engine, int stringIndex, PlayStyle style) noexcept
    {
        auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        engine.configureVoiceDamping(voice, style,
                                     voice.lastDampedFrequency,
                                     voice.lastDampedLiveFret);
    }

    static void silenceVoice(ElectryEngine& engine, int stringIndex) noexcept
    {
        const auto index = static_cast<std::size_t>(stringIndex);
        engine.heldMidiNotes_[index] = -1;
        engine.heldNoteCounts_[index] = 0;
        engine.heldExpressionIds_[index] = ElectryEngine::legacyExpressionId;
        engine.silenceVoice(
            engine.voices_[index]);
        engine.updateActiveVoiceCount();
    }

    // The touching finger's live depth and position, so the harmonic checks
    // can assert on the filter that actually runs rather than on a copy of it.
    static float touchDepth(const ElectryEngine& engine, int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)].touchDepth;
    }

    static float touchFraction(const ElectryEngine& engine,
                               int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)].touchFraction;
    }

    // How far through its travel a legato glide is, so the slide's timing can
    // be read from the glide itself rather than inferred from audio.
    static float legatoBlend(const ElectryEngine& engine,
                             int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)].legatoBlend;
    }

    static float legatoFromFrequency(const ElectryEngine& engine,
                                     int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .legatoFromFrequency;
    }

    static float programmedLegatoFrequency(const ElectryEngine& engine,
                                            int stringIndex) noexcept
    {
        const auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        if (voice.legatoBlend >= 1.0f || voice.legatoFromFrequency <= 0.0f)
            return voice.baseFrequency;
        const float fromSemitones = 12.0f * std::log2(
            voice.legatoFromFrequency / voice.baseFrequency);
        return voice.baseFrequency * std::exp2(
            fromSemitones * (1.0f - smoothStep(voice.legatoBlend)) / 12.0f);
    }

    static float lastConfiguredSemitones(const ElectryEngine& engine,
                                         int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .lastConfiguredSemitones;
    }

    static float lastDampedFrequency(const ElectryEngine& engine,
                                     int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .lastDampedFrequency;
    }

    static void setLegatoWithOpposingBend(ElectryEngine& engine,
                                          int stringIndex,
                                          float blend) noexcept
    {
        auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        voice.legatoBlend = blend;
        const float legatoOffset = 12.0f * std::log2(
            voice.legatoFromFrequency / voice.baseFrequency)
            * (1.0f - smoothStep(voice.legatoBlend));
        engine.pitchBendSemitones_ = -legatoOffset;
        engine.configureVoicePitch(voice, false);
    }

    // Fundamental implied by the delay and every phase-compensated loop filter.
    // This solves currentDelay + filterPhase(f) = sampleRate / f, so a raw
    // delay move that merely compensates a filter refit is not mistaken for a
    // pitch jump and a stationary raw delay under a new filter is not accepted
    // as continuous.
    static float effectiveLoopFrequency(const ElectryEngine& engine,
                                        int stringIndex,
                                        bool horizontal = false,
                                        bool useTargetDelay = false) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return std::numeric_limits<float>::quiet_NaN();
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& loop = horizontal ? voice.horizontal : voice.vertical;
        const float sampleRate = static_cast<float>(engine.sampleRate_);
        const auto errorAt = [&] (float frequency)
        {
            const float omega = 2.0f * 3.14159265358979323846f
                              * frequency / sampleRate;
            float dipMagnitude = 1.0f, dipPhase = 0.0f;
            ElectryEngine::handLossResponse(
                loop.handLossDepth, loop.handLossShape, omega,
                dipMagnitude, dipPhase);
            const float dipDelay = omega > 1.0e-9f
                ? -dipPhase / omega : 0.0f;
#if ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2
            float fittedMagnitude = 1.0f, fittedPhase = 0.0f;
            ElectryEngine::handLossResponse(
                loop.fittedLossDepth, loop.fittedLossShape, omega,
                fittedMagnitude, fittedPhase);
            const float fittedDelay = omega > 1.0e-9f
                ? -fittedPhase / omega : 0.0f;
#else
            constexpr float fittedDelay = 0.0f;
#endif
            const float phaseDelay = ElectryEngine::onePolePhaseDelay(
                    loop.loopDampingCoefficient, omega)
                + fittedDelay + dipDelay
                + 4.0f * ElectryEngine::allpassPhaseDelay(
                    loop.dispersionLowCoefficient, omega)
                + 4.0f * ElectryEngine::allpassPhaseDelay(
                    loop.dispersionHighCoefficient, omega);
            const float delay = useTargetDelay ? loop.targetDelay
                                               : loop.currentDelay;
            return delay + phaseDelay - sampleRate / frequency;
        };

        float low = 20.0f;
        float high = 0.24f * sampleRate;
        if (! (errorAt(low) <= 0.0f && errorAt(high) >= 0.0f))
            return std::numeric_limits<float>::quiet_NaN();
        for (int iteration = 0; iteration < 32; ++iteration)
        {
            const float mid = 0.5f * (low + high);
            if (errorAt(mid) < 0.0f)
                low = mid;
            else
                high = mid;
        }
        return 0.5f * (low + high);
    }

    static int hostFramesPerControlPeriod(
        const ElectryEngine& engine) noexcept
    {
        return std::max(1, ElectryEngine::controlPeriod
                             / engine.oversamplingFactor_);
    }

    static void renderOneInternalSample(ElectryEngine& engine) noexcept
    {
        static_cast<void>(engine.renderInternalSample(0.0f));
    }

    // The slide's friction level, so the check that a silent Finger Noise
    // control means an exactly absent scrape reads the engine rather than the
    // audio.
    static float slideNoiseAmplitude(const ElectryEngine& engine,
                                     int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .slideNoiseAmplitude;
    }

    static float slideNoiseLevel(const ElectryEngine& engine,
                                 int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .slideNoiseLevel;
    }

    static float slideAverageBandCentreHz(const ElectryEngine& engine,
                                          int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .slideAverageBandCentreHz;
    }

    static std::array<float, 2> slideBandCoefficients(
        const ElectryEngine& engine, int stringIndex) noexcept
    {
        const auto& voice =
            engine.voices_[static_cast<std::size_t>(stringIndex)];
        return { voice.slideBandHigh, voice.slideBandLow };
    }

    static void updateSlideControlAt(ElectryEngine& engine, int stringIndex,
                                     float blend) noexcept
    {
        auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        voice.legatoBlend = clampf(blend - voice.legatoIncrement, 0.0f, 1.0f);
        engine.updateVoiceControl(voice);
    }

    static float releaseGainTarget(const ElectryEngine& engine,
                                   int stringIndex) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(stringIndex)]
            .releaseGainTarget;
    }

    // The rate the winding ridges are passing under the finger, recovered from
    // the one-pole that actually forms the friction band. The engine stores
    // the coefficient rather than the frequency, so the test inverts the same
    // relation the engine used.
    static double slideBandCentreHz(const ElectryEngine& engine,
                                    int stringIndex) noexcept
    {
        const float coefficient = engine
            .voices_[static_cast<std::size_t>(stringIndex)].slideBandLow;
        if (coefficient <= 0.0f || coefficient >= 1.0f)
            return 0.0;
        return -std::log(static_cast<double>(coefficient))
             * engine.sampleRate_
             / (2.0 * 3.14159265358979323846 * 0.6);
    }

    // Where the fretting hand's index finger currently sits, so the allocator
    // checks can assert on the state that drove the choice rather than only on
    // the choice itself.
    static float frettingHandPosition(const ElectryEngine& engine) noexcept
    {
        return engine.frettingHandPosition_;
    }

    // The parameter guard setParameters() runs before anything else sees a
    // host's automation, so the fallback and clamp behaviour can be asserted
    // on directly rather than only inferred from the audio it protects.
    static EngineParameters sanitise(const EngineParameters& parameters) noexcept
    {
        return ElectryEngine::sanitise(parameters);
    }

    // The pitch a bridge-coupled string is running at - its open note, shifted
    // by the wheel's MIDI interval - computed the way
    // configureSympatheticString computes it, so the decay checks can invert
    // the loop's response at exactly the frequency it was solved for.
    static float sympatheticFrequency(const ElectryEngine& engine,
                                      int stringIndex) noexcept
    {
        const auto& spec =
            ElectryEngine::stringSpecs()[static_cast<std::size_t>(stringIndex)];
        return ElectryEngine::midiToHz(static_cast<float>(spec.openMidiNote))
             * std::exp2(engine.pitchBendSemitones_ / 12.0f);
    }
};
} // namespace electry

namespace
{
using electry::ElectryEngine;
using electry::EngineParameters;
using electry::PickStyle;
using electry::PickupSelector;
using electry::PlayStyle;
using TestAccess = electry::ElectryEngineTestAccess;

int pickKeyswitch(PickStyle pick)
{
    return ElectryEngine::firstKeyswitchNote + static_cast<int>(pick);
}

int styleKeyswitch(PlayStyle style)
{
    return ElectryEngine::firstPlayStyleKeyswitchNote + static_cast<int>(style);
}

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct ContactWaves
{
    double towardNut { 0.0 };
    double towardBridge { 0.0 };
};

// Equal-admittance, memoryless shunt contact. `amount` removes only the common
// component of the two incident displacement waves: its scattering matrix is
// 1/2 [[Q+1, Q-1], [Q-1, Q+1]] with Q = 1-amount, as in the reciprocal pluck
// junction. Its eigenvalues are 1-amount and one, so 0 is identity, exchanging
// the ports changes nothing, and every amount in [0, 1] is passive in
// squared-wave energy.
ContactWaves scatterPassiveContact(ContactWaves incident,
                                   double amount) noexcept
{
    const double commonLoss = 0.5 * amount
                            * (incident.towardNut + incident.towardBridge);
    return {
        incident.towardNut - commonLoss,
        incident.towardBridge - commonLoss
    };
}

template <std::size_t RailLength>
struct TwoRailContactString
{
    std::array<double, RailLength> towardNut {};
    std::array<double, RailLength> towardBridge {};

    void step(std::size_t junction, double contactAmount) noexcept
    {
        std::array<double, RailLength> nextTowardNut {};
        std::array<double, RailLength> nextTowardBridge {};

        for (std::size_t x = 0; x + 1 < RailLength; ++x)
            nextTowardNut[x + 1] = towardNut[x];
        for (std::size_t x = 1; x < RailLength; ++x)
            nextTowardBridge[x - 1] = towardBridge[x];

        // Ideal fixed ends. The two sign inversions cancel over one round trip,
        // which is what lets the free string fold into one ordinary ring.
        nextTowardNut[0] = -towardBridge[0];
        nextTowardBridge[RailLength - 1] = -towardNut[RailLength - 1];

        const auto outgoing = scatterPassiveContact(
            { towardNut[junction - 1], towardBridge[junction] },
            contactAmount);
        nextTowardNut[junction] = outgoing.towardNut;
        nextTowardBridge[junction - 1] = outgoing.towardBridge;

        towardNut = nextTowardNut;
        towardBridge = nextTowardBridge;
    }

    [[nodiscard]] double displacement(std::size_t x) const noexcept
    {
        return towardNut[x] + towardBridge[x];
    }

    [[nodiscard]] std::array<double, 2 * RailLength> folded() const noexcept
    {
        std::array<double, 2 * RailLength> result {};
        for (std::size_t x = 0; x < RailLength; ++x)
        {
            result[x] = towardNut[x];
            result[2 * RailLength - 1 - x] = -towardBridge[x];
        }
        return result;
    }
};

template <std::size_t RailLength>
struct FoldedContactRing
{
    std::array<double, 2 * RailLength> wave {};

    void step(std::size_t junction, double contactAmount) noexcept
    {
        std::array<double, 2 * RailLength> next {};
        for (std::size_t i = 0; i < wave.size(); ++i)
            next[(i + 1) % wave.size()] = wave[i];

        // Folding maps towardNut[x] to wave[x] and towardBridge[x] to
        // -wave[2L-1-x]. Gather both incident waves before propagation, then
        // replace both corresponding outgoing cells after it. Updating only the
        // write-head cell is therefore not a local two-port contact.
        const std::size_t bridgeIncident = 2 * RailLength - 1 - junction;
        const auto outgoing = scatterPassiveContact(
            { wave[junction - 1], -wave[bridgeIncident] }, contactAmount);
        next[junction] = outgoing.towardNut;
        next[2 * RailLength - junction] = -outgoing.towardBridge;
        wave = next;
    }

    [[nodiscard]] double displacement(std::size_t x) const noexcept
    {
        return wave[x] - wave[2 * RailLength - 1 - x];
    }
};

struct StereoBuffer
{
    std::vector<float> left;
    std::vector<float> right;

    explicit StereoBuffer(int samples)
        : left(static_cast<std::size_t>(samples), 0.0f),
          right(static_cast<std::size_t>(samples), 0.0f) {}

    [[nodiscard]] int size() const noexcept
    {
        return static_cast<int>(left.size());
    }
};

void renderInto(ElectryEngine& engine, StereoBuffer& buffer, int blockSize = 512)
{
    int rendered = 0;
    const int total = buffer.size();
    while (rendered < total)
    {
        const int samples = std::min(blockSize, total - rendered);
        engine.process(buffer.left.data() + rendered,
                       buffer.right.data() + rendered, samples);
        rendered += samples;
    }
}

StereoBuffer renderNote(ElectryEngine& engine, double sampleRate, int midiNote,
                        float velocity, PlayStyle playStyle,
                        double seconds, double noteOffAfterSeconds = -1.0,
                        PickStyle pickStyle = PickStyle::Down)
{
    engine.reset();
    engine.noteOn(pickKeyswitch(pickStyle), 1.0f);
    engine.noteOn(styleKeyswitch(playStyle), 1.0f);
    engine.noteOn(midiNote, velocity);

    const int totalSamples = static_cast<int>(seconds * sampleRate);
    StereoBuffer buffer(totalSamples);

    if (noteOffAfterSeconds > 0.0 && noteOffAfterSeconds < seconds)
    {
        const int offSample = static_cast<int>(noteOffAfterSeconds * sampleRate);
        engine.process(buffer.left.data(), buffer.right.data(), offSample);
        engine.noteOff(midiNote);
        engine.process(buffer.left.data() + offSample,
                       buffer.right.data() + offSample,
                       totalSamples - offSample);
    }
    else
    {
        renderInto(engine, buffer);
    }
    return buffer;
}

bool allFinite(const StereoBuffer& buffer)
{
    for (const float sample : buffer.left)
        if (! std::isfinite(sample))
            return false;
    for (const float sample : buffer.right)
        if (! std::isfinite(sample))
            return false;
    return true;
}

float peakAbs(const std::vector<float>& data, int start = 0, int end = -1)
{
    const int last = end < 0 ? static_cast<int>(data.size())
                             : std::min<int>(end, static_cast<int>(data.size()));
    float peak = 0.0f;
    for (int i = std::max(0, start); i < last; ++i)
        peak = std::max(peak, std::abs(data[static_cast<std::size_t>(i)]));
    return peak;
}

double rmsInRange(const std::vector<float>& data, int start, int end)
{
    const int first = std::max(0, start);
    const int last = std::min<int>(end, static_cast<int>(data.size()));
    if (last <= first)
        return 0.0;
    double sum = 0.0;
    for (int i = first; i < last; ++i)
        sum += static_cast<double>(data[static_cast<std::size_t>(i)])
             * static_cast<double>(data[static_cast<std::size_t>(i)]);
    return std::sqrt(sum / static_cast<double>(last - first));
}

double normalisedDifferenceRms(const std::vector<float>& a,
                               const std::vector<float>& b,
                               int start, int end)
{
    const int first = std::max(0, start);
    const int last = std::min<int>({ end, static_cast<int>(a.size()),
                                    static_cast<int>(b.size()) });
    if (last <= first)
        return 0.0;

    double difference = 0.0;
    double reference = 0.0;
    for (int i = first; i < last; ++i)
    {
        const double av = a[static_cast<std::size_t>(i)];
        const double bv = b[static_cast<std::size_t>(i)];
        const double delta = av - bv;
        difference += delta * delta;
        reference += 0.5 * (av * av + bv * bv);
    }
    return reference > 0.0 ? std::sqrt(difference / reference) : 0.0;
}

// Hann-windowed DFT magnitude at an arbitrary frequency, evaluated with a
// phasor recurrence so the tests stay fast.
double dftMagnitude(const std::vector<float>& data, int start, int length,
                    double sampleRate, double frequency)
{
    const int first = std::max(0, start);
    const int last = std::min<int>(first + length, static_cast<int>(data.size()));
    const int n = last - first;
    if (n < 16)
        return 0.0;

    const double omega = 2.0 * 3.14159265358979323846 * frequency / sampleRate;
    const double stepReal = std::cos(omega);
    const double stepImag = -std::sin(omega);
    double phasorReal = 1.0;
    double phasorImag = 0.0;
    double sumReal = 0.0;
    double sumImag = 0.0;
    const double windowStep = 3.14159265358979323846 / static_cast<double>(n - 1);

    for (int i = 0; i < n; ++i)
    {
        const double window = std::sin(windowStep * i);
        const double sample = window * window
            * static_cast<double>(data[static_cast<std::size_t>(first + i)]);
        sumReal += sample * phasorReal;
        sumImag += sample * phasorImag;
        const double nextReal = phasorReal * stepReal - phasorImag * stepImag;
        phasorImag = phasorReal * stepImag + phasorImag * stepReal;
        phasorReal = nextReal;
    }
    return std::sqrt(sumReal * sumReal + sumImag * sumImag);
}

// Locate the strongest spectral component near an expected fundamental by a
// coarse-to-fine scan, returning its frequency in Hz.
double measureFrequency(const std::vector<float>& data, int start, int length,
                        double sampleRate, double searchCentreHz,
                        double searchSpanCents = 120.0)
{
    const int first = std::max(0, start);
    const int last = std::min<int>(first + length,
                                   static_cast<int>(data.size()));
    float peak = 0.0f;
    for (int index = first; index < last; ++index)
        peak = std::max(peak,
                        std::abs(data[static_cast<std::size_t>(index)]));
    expect(peak > 1.0e-7f,
           "frequency estimator received a silent or negligible capture");

    const auto scan = [&] (double centre, double spanCents, double stepCents)
    {
        double bestFrequency = centre;
        double bestMagnitude = -1.0;
        for (double cents = -spanCents; cents <= spanCents; cents += stepCents)
        {
            const double frequency = centre * std::pow(2.0, cents / 1200.0);
            // A magnetic pickup produces induced EMF, so its fundamental can
            // be weaker than the first few partials. Score a short harmonic
            // series instead of assuming displacement-like fundamental
            // dominance; the narrow scan still prevents octave ambiguity.
            double magnitude = 0.0;
            for (int partial = 1; partial <= 5; ++partial)
            {
                const double partialFrequency = frequency * partial;
                if (partialFrequency >= 0.45 * sampleRate)
                    break;
                magnitude += dftMagnitude(data, start, length, sampleRate,
                                          partialFrequency)
                           / std::sqrt(static_cast<double>(partial));
            }
            if (magnitude > bestMagnitude)
            {
                bestMagnitude = magnitude;
                bestFrequency = frequency;
            }
        }
        return bestFrequency;
    };

    const double coarse = scan(searchCentreHz, searchSpanCents, 6.0);
    return scan(coarse, 6.0, 0.5);
}

// An inharmonic dispersive string does not keep every partial on an ideal
// harmonic FFT bin. Search each partial locally so an articulation test
// measures its amplitude rather than grid or stiffness offset.
double trackedPartialMagnitude(const std::vector<float>& data, int start,
                               int length, double sampleRate,
                               double fundamentalHz, int partial)
{
    const double nominal = fundamentalHz * static_cast<double>(partial);
    const auto scan = [&] (double centre, double spanCents, double stepCents,
                           double& bestFrequency)
    {
        double bestMagnitude = -1.0;
        bestFrequency = centre;
        for (double cents = -spanCents; cents <= spanCents; cents += stepCents)
        {
            const double frequency = centre * std::pow(2.0, cents / 1200.0);
            if (frequency >= 0.48 * sampleRate)
                continue;
            const double magnitude = dftMagnitude(data, start, length,
                                                  sampleRate, frequency);
            if (magnitude > bestMagnitude)
            {
                bestMagnitude = magnitude;
                bestFrequency = frequency;
            }
        }
        return bestMagnitude;
    };

    const double neighbourSpacingCents = 1200.0 * std::log2(
        static_cast<double>(partial + 1) / static_cast<double>(partial));
    const double searchSpanCents = std::min(80.0,
                                            0.45 * neighbourSpacingCents);
    double coarseFrequency = nominal;
    (void) scan(nominal, searchSpanCents, 2.0, coarseFrequency);
    double fineFrequency = coarseFrequency;
    return scan(coarseFrequency, 2.0, 0.25, fineFrequency);
}

double centsBetween(double frequencyA, double frequencyB)
{
    return 1200.0 * std::log2(frequencyA / frequencyB);
}

// Energy-weighted mean frequency across the tone's partial series. Sampling
// the spectrum at the partials measures the envelope rather than window
// leakage between harmonic lines.
double spectralCentroid(const std::vector<float>& data, int start, int length,
                        double sampleRate, double fundamentalHz)
{
    // Track the note before sampling its partials. Dispersion and fractional-
    // delay fitting can put the measured fundamental slightly off a nominal
    // FFT bin; treating that offset as missing upper harmonics would skew the
    // envelope measurement.
    fundamentalHz = measureFrequency(data, start, length, sampleRate,
                                     fundamentalHz);
    double weighted = 0.0;
    double total = 0.0;
    for (int partial = 1; partial <= 48; ++partial)
    {
        const double frequency = fundamentalHz * partial;
        if (frequency > std::min(6500.0, 0.45 * sampleRate))
            break;
        const double magnitude = dftMagnitude(data, start, length, sampleRate,
                                              frequency);
        weighted += magnitude * frequency;
        total += magnitude;
    }
    return total > 0.0 ? weighted / total : 0.0;
}

double midiHz(int midiNote)
{
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

// Magnitude ratio between the partials above and below a split frequency:
// a direct measure of what a passive tone control removes.
double highBandRatio(const std::vector<float>& data, int start, int length,
                     double sampleRate, double fundamentalHz)
{
    fundamentalHz = measureFrequency(data, start, length, sampleRate,
                                     fundamentalHz);
    double low = 0.0;
    double high = 0.0;
    for (int partial = 1; partial <= 48; ++partial)
    {
        const double frequency = fundamentalHz * partial;
        if (frequency > std::min(6500.0, 0.45 * sampleRate))
            break;
        const double magnitude = dftMagnitude(data, start, length, sampleRate,
                                              frequency);
        if (frequency < 900.0)
            low += magnitude;
        else
            high += magnitude;
    }
    return low > 0.0 ? high / low : 0.0;
}

struct HarmonicBalance
{
    double lowPowerShare { 0.0 };
    double highPowerShare { 0.0 };
    double strongestFirstEightDb { -300.0 };
    double lowMagnitude { 0.0 };
};

double decibels(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-15));
}

// Stiff wound-string partials sit slightly above an ideal harmonic series.
// A short scan prevents valid dispersion from looking like missing spectral
// energy while remaining narrow enough not to count unrelated noise.
double scannedPartialMagnitude(const std::vector<float>& data, int start,
                               int length, double sampleRate,
                               double fundamentalHz, int partial)
{
    const double idealFrequency = fundamentalHz * static_cast<double>(partial);
    double best = 0.0;
    for (double cents = -8.0; cents <= 56.0; cents += 8.0)
    {
        const double frequency = idealFrequency * std::pow(2.0, cents / 1200.0);
        best = std::max(best, dftMagnitude(data, start, length, sampleRate,
                                           frequency));
    }
    return best;
}

HarmonicBalance measureHarmonicBalance(const std::vector<float>& data,
                                       int start, int length,
                                       double sampleRate,
                                       double fundamentalHz)
{
    double lowPower = 0.0;
    double middlePower = 0.0;
    double highPower = 0.0;
    double fundamentalMagnitude = 0.0;
    double strongestFirstEight = 0.0;

    for (int partial = 1; partial <= 64; ++partial)
    {
        const double frequency = fundamentalHz * static_cast<double>(partial);
        if (frequency >= std::min(6500.0, 0.45 * sampleRate))
            break;
        const double magnitude = scannedPartialMagnitude(
            data, start, length, sampleRate, fundamentalHz, partial);
        const double power = magnitude * magnitude;
        if (frequency < 250.0)
            lowPower += power;
        else if (frequency < 1000.0)
            middlePower += power;
        else
            highPower += power;

        if (partial == 1)
            fundamentalMagnitude = magnitude;
        else if (partial <= 8)
            strongestFirstEight = std::max(strongestFirstEight, magnitude);
    }

    const double totalPower = lowPower + middlePower + highPower;
    HarmonicBalance result;
    result.lowPowerShare = lowPower / std::max(totalPower, 1.0e-30);
    result.highPowerShare = highPower / std::max(totalPower, 1.0e-30);
    result.strongestFirstEightDb = decibels(
        strongestFirstEight / std::max(fundamentalMagnitude, 1.0e-15));
    result.lowMagnitude = std::sqrt(lowPower);
    return result;
}

// ---------------------------------------------------------------------------

void testPassiveContactFoldedRingReference()
{
    constexpr std::array<ContactWaves, 5> incidentCases {{
        { 0.0, 0.0 }, { 1.0, 0.0 }, { 0.0, -1.0 },
        { 0.75, -0.25 }, { -0.60, 0.90 }
    }};
    constexpr std::array<double, 6> contactAmounts {
        0.0, 0.10, 0.35, 0.60, 0.85, 1.0
    };

    for (const auto incident : incidentCases)
    {
        const auto identity = scatterPassiveContact(incident, 0.0);
        expect(identity.towardNut == incident.towardNut
                   && identity.towardBridge == incident.towardBridge,
               "zero contact was not the identity scattering junction");

        for (const double amount : contactAmounts)
        {
            const auto outgoing = scatterPassiveContact(incident, amount);
            const auto reversed = scatterPassiveContact(
                { incident.towardBridge, incident.towardNut }, amount);
            expect(std::abs(outgoing.towardNut - reversed.towardBridge)
                           < 1.0e-15
                       && std::abs(outgoing.towardBridge - reversed.towardNut)
                           < 1.0e-15,
                   "contact scattering was not reciprocal");

            const double incomingEnergy = incident.towardNut * incident.towardNut
                                        + incident.towardBridge
                                            * incident.towardBridge;
            const double outgoingEnergy = outgoing.towardNut * outgoing.towardNut
                                        + outgoing.towardBridge
                                            * outgoing.towardBridge;
            expect(outgoingEnergy <= incomingEnergy
                                        + 1.0e-12
                                            * std::max(1.0, incomingEnergy),
                   "contact scattering created squared-wave energy");
        }
    }

    constexpr std::size_t railLength = 7;
    constexpr std::size_t junction = 3;
    constexpr std::size_t probe = 5;
    const auto maximumStateDifference = [] (const auto& a, const auto& b)
    {
        double difference = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i)
            difference = std::max(difference, std::abs(a[i] - b[i]));
        return difference;
    };

    // Start from an arbitrary state rather than a specially aligned impulse:
    // a free two-rail fixed-end string must be exactly one signed ring rotation
    // for every possible pair of travelling-wave rails.
    TwoRailContactString<railLength> freeRails;
    for (std::size_t x = 0; x < railLength; ++x)
    {
        freeRails.towardNut[x] = 0.07 * static_cast<double>(x + 1);
        freeRails.towardBridge[x] = -0.05 * static_cast<double>(railLength - x);
    }
    FoldedContactRing<railLength> freeRing { freeRails.folded() };
    double maximumFreeDifference = 0.0;
    for (std::size_t step = 0; step < 4 * railLength; ++step)
    {
        freeRails.step(junction, 0.0);
        freeRing.step(junction, 0.0);
        maximumFreeDifference = std::max(
            maximumFreeDifference,
            maximumStateDifference(freeRails.folded(), freeRing.wave));
        maximumFreeDifference = std::max(
            maximumFreeDifference,
            std::abs(freeRails.displacement(probe)
                     - freeRing.displacement(probe)));
    }
    expect(maximumFreeDifference < 1.0e-15,
           "a free fixed-end two-rail string did not fold into one ring");

    // Put an impulse directly before the contact and hold the junction for two
    // round trips. The signed paired gather/scatter in FoldedContactRing must
    // reproduce both outgoing waves, not merely attenuate the one cell crossing
    // a loop seam. This is the smallest reference for a vibrating-string repick.
    TwoRailContactString<railLength> contactRails;
    contactRails.towardNut[junction - 1] = 1.0;
    FoldedContactRing<railLength> contactRing { contactRails.folded() };
    double maximumContactDifference = 0.0;
    for (std::size_t step = 0; step < 4 * railLength; ++step)
    {
        const double amount = step < 2 * railLength ? 0.65 : 0.0;
        contactRails.step(junction, amount);
        contactRing.step(junction, amount);
        maximumContactDifference = std::max(
            maximumContactDifference,
            maximumStateDifference(contactRails.folded(), contactRing.wave));
        maximumContactDifference = std::max(
            maximumContactDifference,
            std::abs(contactRails.displacement(probe)
                     - contactRing.displacement(probe)));
    }
    expect(maximumContactDifference < 1.0e-15,
           "paired folded-ring contact did not match the two-rail impulse response");

    // Point-touch rendering and cubic-delay interpolation have their own
    // production regressions. This fixture only proves that the reciprocal
    // two-rail contact reference folds exactly into one signed ring.
    std::cout << "PROBE passive contact folded-ring reference: free max delta "
              << maximumFreeDifference << ", contact max delta "
              << maximumContactDifference
              << '\n';
}

#if ELECTRY_ANALYTIC_RELEASE_IC
void testAnalyticReleasePickupSpatialHistory()
{
    struct Case
    {
        double period;
        double tapDelay;
    };
    // Default-Build open G3 at the 96 kHz internal rate: P=489.80162, bridge
    // tap=26.66537, coil spacing=6.63144 and aperture=1.67531 samples. Its
    // pickup lands beside the default pluck kink, so the first samples exercise
    // cubic interpolation instead of remaining on one linear triangle segment.
    // The rounded-period companion covers both delay classes.
    constexpr std::array<Case, 2> cases {{
        { 490.0, 26.6762 },
        { 489.80162, 26.66537 }
    }};
    constexpr double peak = 0.08;
    constexpr double pluck = 0.1069;
    constexpr double perpendicularPickupWeight = 0.85;
    constexpr double combDepth = 0.60;
    constexpr double coilBalance = 0.60;
    constexpr double coilSpacing = 6.63144;
    constexpr double apertureLength = 1.67531;

    for (const auto& test : cases)
    {
        // Independent d'Alembert reference: the held triangle is extended as
        // one odd, periodic travelling rail, then sampled directly at the
        // pickup positions. It deliberately shares no delay-line or pickup
        // implementation with the engine path below.
        const auto railAt = [&] (double cell)
        {
            double phase = (cell + 0.5) / test.period;
            phase -= std::floor(phase);
            const double position = 2.0 * std::min(phase, 1.0 - phase);
            const double triangle = position <= pluck
                ? position / pluck
                : (1.0 - position) / (1.0 - pluck);
            return 0.5 * peak * (phase < 0.5 ? 1.0 : -1.0) * triangle;
        };
        const auto cubicRailAt = [&] (double delay, double sample)
        {
            const double ceiling = std::ceil(delay);
            const double t = ceiling - delay;
            const double tMinus1 = t - 1.0;
            const double tMinus2 = t - 2.0;
            const double tPlus1 = t + 1.0;
            const double y0 = railAt(ceiling - sample);
            const double y1 = railAt(ceiling - 1.0 - sample);
            const double y2 = railAt(ceiling - 2.0 - sample);
            const double y3 = railAt(ceiling - 3.0 - sample);
            return (y0 * (-t * tMinus1 * tMinus2)
                    + y3 * (tPlus1 * t * tMinus1)) / 6.0
                 + (y1 * (tPlus1 * tMinus1 * tMinus2)
                    - y2 * (tPlus1 * t * tMinus2)) * 0.5;
        };
        const auto rawPickupAt = [&] (double sample)
        {
            return perpendicularPickupWeight
                 * (cubicRailAt(test.period, sample)
                    - combDepth * cubicRailAt(test.tapDelay, sample));
        };
        const auto coilPairAt = [&] (double sample)
        {
            const int whole = static_cast<int>(coilSpacing);
            const double fraction = coilSpacing - whole;
            const double recent = rawPickupAt(sample - whole);
            const double older = rawPickupAt(sample - whole - 1);
            const double secondCoil = recent + fraction * (older - recent);
            return (rawPickupAt(sample) + coilBalance * secondCoil)
                 / (1.0 + coilBalance);
        };
        const auto apertureAt = [&] (int sample)
        {
            const int whole = static_cast<int>(apertureLength);
            const double fraction = apertureLength - whole;
            double sum = 0.0;
            for (int offset = 0; offset < whole; ++offset)
                sum += coilPairAt(sample - offset);
            sum += fraction * coilPairAt(sample - whole);
            return sum / apertureLength;
        };

        const auto actual = TestAccess::analyticReleasePickupTrajectory(
            static_cast<float>(test.period), static_cast<float>(peak),
            static_cast<float>(pluck), static_cast<float>(test.tapDelay),
            static_cast<float>(apertureLength),
            static_cast<float>(coilSpacing));
        double maximumDisplacementError = 0.0;
        double squaredDisplacementError = 0.0;
        double maximumFaradayError = 0.0;
        for (std::size_t sample = 0; sample < actual.displacement.size();
             ++sample)
        {
            const double expected = apertureAt(static_cast<int>(sample));
            const double displacementError =
                static_cast<double>(actual.displacement[sample]) - expected;
            maximumDisplacementError = std::max(
                maximumDisplacementError, std::abs(displacementError));
            squaredDisplacementError += displacementError * displacementError;
            const double expectedDifference = sample == 0
                ? 0.0
                : expected - apertureAt(static_cast<int>(sample) - 1);
            maximumFaradayError = std::max(
                maximumFaradayError,
                std::abs(static_cast<double>(
                             actual.faradayDifference[sample])
                         - expectedDifference));
        }
        const double rmsDisplacementError = std::sqrt(
            squaredDisplacementError / actual.displacement.size());
        std::cout << "PROBE analytic pickup history period " << test.period
                  << ": max " << maximumDisplacementError
                  << ", RMS " << rmsDisplacementError
                  << ", Faraday max " << maximumFaradayError << '\n';
        expect(maximumDisplacementError < 2.0e-6,
               "analytic release pickup history missed its spatial trajectory");
        expect(maximumFaradayError < 2.0e-6,
               "analytic release pickup history made a false Faraday edge");
        expect(std::abs(actual.faradayDifference[0]) < 1.0e-9f,
               "static analytic release made a sample-zero Faraday impulse");
    }
}

void testAnalyticReleasedStringInitialCondition()
{
    constexpr double pi = 3.14159265358979323846;
    constexpr int period = 2048;
    constexpr int railLength = period / 2;
    constexpr float peak = 0.08f;
    constexpr float pluck = 0.183f;
    const auto seeded = TestAccess::analyticReleaseSeed(
        static_cast<float>(period), peak, pluck);

    // Keep the CPU-only geometry hoist sample-identical to the former helper,
    // which prepared these invariants again for every delay-line cell.
    const auto legacyRailAtCell = [] (float cell, float delay,
                                      float peakAmplitude,
                                      float pluckFraction,
                                      float halfWidthFraction,
                                      float polarityAndWeight)
    {
        delay = electry::clampf(
            delay, 4.0f,
            static_cast<float>(TestAccess::delayLineCapacity() - 8));
        const float centre = electry::clampf(
            pluckFraction, 0.001f, 0.999f);
        const float width = electry::clampf(
            halfWidthFraction, 0.0f, 0.49f);
        const std::array<float, 3> positions {
            electry::clampf(centre - width, 0.001f, 0.999f),
            centre,
            electry::clampf(centre + width, 0.001f, 0.999f)
        };
        constexpr std::array<float, 3> contactWeights {
            0.25f, 0.5f, 0.25f
        };
        const float centreCompliance = centre * (1.0f - centre);
        float phase = (cell + 0.5f) / delay;
        phase -= std::floor(phase);
        const float position = 2.0f * std::min(phase, 1.0f - phase);
        float triangle = 0.0f;
        for (std::size_t point = 0; point < positions.size(); ++point)
        {
            const float pick = positions[point];
            const float displacement = position <= pick
                ? position / pick
                : (1.0f - position) / (1.0f - pick);
            const float relativeCompliance = pick * (1.0f - pick)
                                           / centreCompliance;
            triangle += contactWeights[point] * relativeCompliance
                      * displacement;
        }
        const float foldedSign = phase < 0.5f ? 1.0f : -1.0f;
        return 0.5f * peakAmplitude * polarityAndWeight
             * foldedSign * triangle;
    };

    expect(seeded.cells.size() == static_cast<std::size_t>(period + 1),
           "analytic release did not initialise one cubic wrap guard");
    double mean = 0.0;
    double maximumRailDifference = 0.0;
    double maximumShapeDifference = 0.0;
    std::vector<double> displacement(static_cast<std::size_t>(railLength));
    for (int x = 0; x < railLength; ++x)
    {
        const float towardNut = seeded.cells[static_cast<std::size_t>(x)];
        const float towardBridge =
            -seeded.cells[static_cast<std::size_t>(period - 1 - x)];
        const double position = (static_cast<double>(x) + 0.5)
                              / static_cast<double>(railLength);
        const double expected = peak * (position <= pluck
            ? position / pluck : (1.0 - position) / (1.0 - pluck));
        const double actual = static_cast<double>(towardNut)
                            + static_cast<double>(towardBridge);
        displacement[static_cast<std::size_t>(x)] = actual;
        maximumRailDifference = std::max(
            maximumRailDifference,
            std::abs(static_cast<double>(towardNut)
                     - static_cast<double>(towardBridge)));
        maximumShapeDifference = std::max(
            maximumShapeDifference, std::abs(actual - expected));
    }
    for (int cell = 0; cell < period; ++cell)
        mean += seeded.cells[static_cast<std::size_t>(cell)];
    mean /= static_cast<double>(period);
    expect(maximumRailDifference < 2.0e-8,
           "analytic release rails do not encode zero initial velocity");
    expect(maximumShapeDifference < 2.0e-7,
           "folded analytic release did not reconstruct its triangular shape");
    expect(std::abs(mean) < 1.0e-9,
           "folded analytic release introduced delay-line DC");
    bool pointSeedBitExact = true;
    for (std::size_t cell = 0; cell < seeded.cells.size(); ++cell)
    {
        pointSeedBitExact = pointSeedBitExact
            && seeded.cells[cell] == legacyRailAtCell(
                   static_cast<float>(cell), static_cast<float>(period),
                   peak, pluck, 0.0f, 1.0f);
    }
    expect(pointSeedBitExact,
           "prepared analytic point release changed a delay-line sample");

    constexpr float contactHalfWidth = 0.012f;
    const auto patchSeed = TestAccess::analyticReleaseSeed(
        static_cast<float>(period), peak, pluck, contactHalfWidth);
    bool patchSeedBitExact = true;
    for (std::size_t cell = 0; cell < patchSeed.cells.size(); ++cell)
    {
        patchSeedBitExact = patchSeedBitExact
            && patchSeed.cells[cell] == legacyRailAtCell(
                   static_cast<float>(cell), static_cast<float>(period),
                   peak, pluck, contactHalfWidth, 1.0f);
    }
    expect(patchSeedBitExact,
           "prepared finite-width release changed a delay-line sample");
    double maximumPatchDifference = 0.0;
    const std::array<double, 3> patchPositions {
        pluck - contactHalfWidth, pluck, pluck + contactHalfWidth
    };
    constexpr std::array<double, 3> patchWeights { 0.25, 0.5, 0.25 };
    const double centreCompliance = pluck * (1.0 - pluck);
    for (int x = 0; x < railLength; ++x)
    {
        const double position = (static_cast<double>(x) + 0.5)
                              / static_cast<double>(railLength);
        double expected = 0.0;
        for (std::size_t point = 0; point < patchPositions.size(); ++point)
        {
            const double pick = patchPositions[point];
            const double triangle = position <= pick
                ? position / pick : (1.0 - position) / (1.0 - pick);
            expected += patchWeights[point]
                      * pick * (1.0 - pick) / centreCompliance
                      * triangle;
        }
        expected *= peak;
        const double actual =
            static_cast<double>(patchSeed.cells[static_cast<std::size_t>(x)])
          - static_cast<double>(patchSeed.cells[
                static_cast<std::size_t>(period - 1 - x)]);
        maximumPatchDifference = std::max(
            maximumPatchDifference, std::abs(actual - expected));
    }
    expect(maximumPatchDifference < 2.0e-7,
           "finite contact patch omitted point-wise string compliance");

    const auto baseline = TestAccess::analyticReleaseSeed(
        static_cast<float>(period), 0.0f, pluck, 0.0f, 0.003f);
    const auto baselinePlusSeed = TestAccess::analyticReleaseSeed(
        static_cast<float>(period), peak, pluck, 0.0f, 0.003f);
    double maximumAdditiveDifference = 0.0;
    for (std::size_t cell = 0; cell < seeded.cells.size(); ++cell)
    {
        const double delta = static_cast<double>(baselinePlusSeed.cells[cell])
                           - static_cast<double>(baseline.cells[cell]);
        maximumAdditiveDifference = std::max(
            maximumAdditiveDifference,
            std::abs(delta - static_cast<double>(seeded.cells[cell])));
    }
    expect(maximumAdditiveDifference < 1.0e-8,
           "analytic release overwrote rather than added to a ringing state");

    // Midpoint quadrature over the reconstructed string must recover the
    // closed-form sine coefficients of a triangular displacement.
    for (int partial = 1; partial <= 16; ++partial)
    {
        double measured = 0.0;
        for (int x = 0; x < railLength; ++x)
        {
            const double position = (static_cast<double>(x) + 0.5)
                                  / static_cast<double>(railLength);
            measured += displacement[static_cast<std::size_t>(x)]
                      * std::sin(partial * pi * position);
        }
        measured *= 2.0 / static_cast<double>(railLength);
        const double harmonic = partial * pi;
        const double expected = 2.0 * peak
            * std::sin(harmonic * pluck)
            / (pluck * (1.0 - pluck) * harmonic * harmonic);
        const double tolerance = std::max(2.0e-6, 0.015 * std::abs(expected));
        expect(std::abs(measured - expected) < tolerance,
               "analytic release partial " + std::to_string(partial)
                   + " missed the triangular 1/n^2 coefficient");
    }

    // A non-integer loop read needs the sample one cell beyond the nominal
    // history. Recompute the same cubic interpolation independently and make
    // the guard's contribution observable.
    constexpr float fractionalDelay = 127.25f;
    const auto fractional = TestAccess::analyticReleaseSeed(
        fractionalDelay, peak, pluck);
    const int ceiling = static_cast<int>(std::ceil(fractionalDelay));
    expect(fractional.cells.size()
               == static_cast<std::size_t>(ceiling + 1)
               && std::abs(fractional.cells.back()) > 1.0e-7f,
           "fractional analytic release did not populate its cubic guard");
    const float t = static_cast<float>(ceiling) - fractionalDelay;
    const float tm1 = t - 1.0f;
    const float tm2 = t - 2.0f;
    const float tp1 = t + 1.0f;
    const float y0 = fractional.cells[static_cast<std::size_t>(ceiling)];
    const float y1 = fractional.cells[static_cast<std::size_t>(ceiling - 1)];
    const float y2 = fractional.cells[static_cast<std::size_t>(ceiling - 2)];
    const float y3 = fractional.cells[static_cast<std::size_t>(ceiling - 3)];
    const float expectedRead =
        (y0 * (-t * tm1 * tm2) + y3 * (tp1 * t * tm1)) * (1.0f / 6.0f)
      + (y1 * (tp1 * tm1 * tm2) - y2 * (tp1 * t * tm2)) * 0.5f;
    expect(std::abs(fractional.fractionalRead - expectedRead) < 1.0e-8f,
           "fractional analytic release guard did not feed the loop read");

    // Above 96 kHz the engine renders one internal sample per host frame, so
    // the Contact -> Release boundary and the following seed sample are
    // independently observable.
    constexpr double sampleRate = 192000.0;
    EngineParameters parameters;
    parameters.strumSpreadSeconds = 0.040f;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    // Nothing is seeded during strum lookahead. Cancelling before the actual
    // contact clears both the fresh-contact latch and the pending amplitude.
    ElectryEngine cancelled;
    cancelled.prepare(sampleRate, 512);
    cancelled.setParameters(parameters);
    cancelled.reset();
    cancelled.noteOn(28, 0.90f);
    const int cancelledString = TestAccess::stringForNote(cancelled, 28);
    const auto waiting = TestAccess::snapshot(cancelled, cancelledString);
    expect(waiting.startDelaySamples > 0
               && waiting.analyticReleaseFreshContact
               && waiting.analyticReleaseAmplitude == 0.0f
               && TestAccess::playedLoopSquaredSum(cancelled,
                                                    cancelledString) == 0.0,
           "analytic release changed the string before delayed contact");
    cancelled.noteOff(28);
    const auto afterCancel = TestAccess::snapshot(cancelled, cancelledString);
    expect(! afterCancel.active
               && afterCancel.analyticReleaseAmplitude == 0.0f
               && ! afterCancel.analyticReleaseFreshContact,
           "cancelled analytic release left pending state");

    ElectryEngine delayed;
    delayed.prepare(sampleRate, 512);
    delayed.setParameters(parameters);
    delayed.reset();
    delayed.noteOn(28, 0.90f);
    const int stringIndex = TestAccess::stringForNote(delayed, 28);
    StereoBuffer one(1);
    int guard = 0;
    while (TestAccess::snapshot(delayed, stringIndex).startDelaySamples > 0
           && guard++ < static_cast<int>(sampleRate))
        renderInto(delayed, one, 1);
    auto contact = TestAccess::snapshot(delayed, stringIndex);
    expect(contact.excitationInContact
               && contact.analyticReleaseAmplitude > 0.0f
               && contact.excitationAmplitude == 0.0f
               && contact.excitationTransientAmplitude > 0.0f,
           "fresh plectrum contact did not arm only the analytic sustained path");
    while (TestAccess::snapshot(delayed, stringIndex).excitationInContact
           && guard++ < static_cast<int>(sampleRate))
        renderInto(delayed, one, 1);
    const auto atRelease = TestAccess::snapshot(delayed, stringIndex);
    expect(atRelease.excitationInRelease
               && atRelease.analyticReleaseAmplitude > 0.0f
               && TestAccess::playedLoopSquaredSum(delayed, stringIndex) == 0.0,
           "analytic release seeded before the first Release sample");

    StereoBuffer seedFrame(1);
    renderInto(delayed, seedFrame, 1);
    const auto afterSeed = TestAccess::snapshot(delayed, stringIndex);
    expect(afterSeed.analyticReleaseAmplitude == 0.0f
               && TestAccess::playedLoopSquaredSum(delayed, stringIndex) > 0.0,
           "analytic release was not consumed exactly once");
    expect(peakAbs(seedFrame.left) < 1.0e-7f
               && peakAbs(seedFrame.right) < 1.0e-7f,
           "static analytic displacement made a false pickup derivative impulse");

    // Scheduling a second public Note On while the first pick is still in
    // contact must not erase that first contact's already-solved release.
    auto stagedRepick = std::make_unique<ElectryEngine>();
    auto stagedParameters = parameters;
    stagedParameters.strumSpreadSeconds = 0.020f;
    stagedRepick->prepare(sampleRate, 512);
    stagedRepick->setParameters(stagedParameters);
    stagedRepick->reset();
    stagedRepick->noteOn(28, 0.90f);
    const int stagedString = TestAccess::stringForNote(*stagedRepick, 28);
    guard = 0;
    while (TestAccess::snapshot(*stagedRepick, stagedString).startDelaySamples > 0
           && guard++ < static_cast<int>(sampleRate))
        renderInto(*stagedRepick, one, 1);
    const float firstContactAmplitude =
        TestAccess::snapshot(*stagedRepick, stagedString)
            .analyticReleaseAmplitude;
    stagedRepick->noteOn(28, 0.70f);
    const auto scheduledRepick = TestAccess::snapshot(*stagedRepick,
                                                       stagedString);
    expect(scheduledRepick.pendingRepickActive
               && scheduledRepick.startDelaySamples > 0
               && scheduledRepick.analyticReleaseAmplitude
                      == firstContactAmplitude
               && firstContactAmplitude > 0.0f,
           "delayed same-note scheduling erased the armed first release");
    stagedRepick->noteOff(28);
    expect(TestAccess::snapshot(*stagedRepick, stagedString)
               .pendingRepickActive,
           "first matching Note Off cancelled an overlapping repick owner");
    stagedRepick->noteOff(28);
    const auto cancelledRepick = TestAccess::snapshot(*stagedRepick,
                                                       stagedString);
    expect(! cancelledRepick.pendingRepickActive
               && cancelledRepick.startDelaySamples == 0
               && cancelledRepick.analyticReleaseAmplitude
                      == firstContactAmplitude,
           "repick cancellation erased the armed first release");
    guard = 0;
    while (TestAccess::snapshot(*stagedRepick, stagedString)
               .analyticReleaseAmplitude > 0.0f
           && guard++ < static_cast<int>(sampleRate))
        renderInto(*stagedRepick, one, 1);
    expect(TestAccess::playedLoopSquaredSum(*stagedRepick, stagedString) > 0.0,
           "armed first release did not seed after repick cancellation");

    // Replacing a silent delayed allocation changes the future contact, not
    // the fact that the physical string is still fresh.
    auto replacedPreroll = std::make_unique<ElectryEngine>();
    replacedPreroll->prepare(sampleRate, 512);
    replacedPreroll->setParameters(parameters);
    replacedPreroll->reset();
    replacedPreroll->noteOn(28, 0.55f);
    replacedPreroll->noteOn(28, 0.90f);
    const int replacedString = TestAccess::stringForNote(*replacedPreroll, 28);
    guard = 0;
    while (TestAccess::snapshot(*replacedPreroll, replacedString)
               .startDelaySamples > 0
           && guard++ < static_cast<int>(sampleRate))
        renderInto(*replacedPreroll, one, 1);
    const auto replacedContact = TestAccess::snapshot(*replacedPreroll,
                                                       replacedString);
    expect(replacedContact.excitationInContact
               && replacedContact.analyticReleaseAmplitude > 0.0f
               && replacedContact.excitationAmplitude == 0.0f,
           "replaced silent pre-roll lost fresh analytic eligibility");

    auto cancelledReplacement = std::make_unique<ElectryEngine>();
    cancelledReplacement->prepare(sampleRate, 512);
    cancelledReplacement->setParameters(parameters);
    cancelledReplacement->reset();
    cancelledReplacement->noteOn(28, 0.55f);
    cancelledReplacement->noteOn(28, 0.90f);
    const int cancelledReplacementString = TestAccess::stringForNote(
        *cancelledReplacement, 28);
    cancelledReplacement->noteOff(28);
    cancelledReplacement->noteOff(28);
    const auto replacementCancel = TestAccess::snapshot(
        *cancelledReplacement, cancelledReplacementString);
    expect(! replacementCancel.active
               && ! replacementCancel.pendingRepickActive
               && replacementCancel.analyticReleaseAmplitude == 0.0f
               && ! replacementCancel.analyticReleaseFreshContact,
           "cancelled replaced pre-roll retained analytic state");

    // Both and Neck selectors prime the same static displacement baseline as
    // the default Bridge path; selector culling must not resurrect its step.
    auto immediate = parameters;
    immediate.strumSpreadSeconds = 0.0f;
    for (const auto selector : { PickupSelector::Both, PickupSelector::Neck })
    {
        auto selected = std::make_unique<ElectryEngine>();
        auto selectedParameters = immediate;
        selectedParameters.pickupSelector = selector;
        selected->prepare(sampleRate, 512);
        selected->setParameters(selectedParameters);
        selected->reset();
        selected->noteOn(40, 0.90f);
        const int selectedString = TestAccess::stringForNote(*selected, 40);
        bool sawSelectedContact = false;
        guard = 0;
        while (guard++ < static_cast<int>(sampleRate))
        {
            const bool inContact = TestAccess::snapshot(*selected,
                                                        selectedString)
                                       .excitationInContact;
            sawSelectedContact = sawSelectedContact || inContact;
            if (sawSelectedContact && ! inContact)
                break;
            renderInto(*selected, one, 1);
        }
        expect(sawSelectedContact,
               "Both/Neck priming fixture never reached pick contact");
        StereoBuffer selectedSeed(1);
        renderInto(*selected, selectedSeed, 1);
        expect(peakAbs(selectedSeed.left) < 1.0e-7f
                   && peakAbs(selectedSeed.right) < 1.0e-7f,
               "analytic pickup priming leaked under Both/Neck selection");
        expect(TestAccess::pickupPathActive(*selected, true)
                   && (selector == PickupSelector::Both
                           ? TestAccess::pickupPathActive(*selected, false)
                           : ! TestAccess::pickupPathActive(*selected, false)),
               "Both/Neck priming fixture did not exercise its selector");
    }

    // A ringing repick and a fretting-hand Hammer both retain the legacy
    // modal excitation; this tranche intentionally changes fresh picks only.
    TestAccess::retriggerVoice(delayed, stringIndex, 28, 0.85f);
    const auto repick = TestAccess::snapshot(delayed, stringIndex);
    expect(repick.analyticReleaseAmplitude == 0.0f
               && repick.excitationAmplitude > 0.0f,
           "ringing repick entered the fresh analytic-release path");

    auto hammer = std::make_unique<ElectryEngine>();
    hammer->prepare(sampleRate, 512);
    hammer->setParameters(immediate);
    hammer->reset();
    hammer->noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
    hammer->noteOn(40, 0.85f);
    const int hammerString = TestAccess::stringForNote(*hammer, 40);
    const auto hammerState = TestAccess::snapshot(*hammer, hammerString);
    expect(hammerState.analyticReleaseAmplitude == 0.0f
               && hammerState.excitationAmplitude > 0.0f,
           "Hammer incorrectly entered the plectrum release candidate");

    // Tension, and therefore compliance, follows a bend already present when
    // the pick loads the string: T is proportional to f squared.
    const auto bentAmplitude = [&] (bool member, float semitones)
    {
        auto bent = std::make_unique<ElectryEngine>();
        bent->prepare(sampleRate, 512);
        bent->setParameters(immediate);
        constexpr ElectryEngine::ExpressionId memberId = 7;
        if (member)
            bent->setExpressionPitchBend(memberId, semitones);
        else
            bent->setPitchBend(0.5f * semitones);
        bent->reset();
        bent->noteOn(40, 0.90f,
                    member ? memberId : ElectryEngine::legacyExpressionId);
        const int bentString = TestAccess::stringForNote(*bent, 40);
        return TestAccess::snapshot(*bent, bentString)
            .analyticReleaseAmplitude;
    };
    const float unbentGlobal = bentAmplitude(false, 0.0f);
    const float bentGlobal = bentAmplitude(false, 2.0f);
    const float unbentMember = bentAmplitude(true, 0.0f);
    const float bentMember = bentAmplitude(true, 2.0f);
    const float expectedBentRatio = std::exp2(-4.0f / 12.0f);
    expect(unbentGlobal > 0.0f && unbentMember > 0.0f
               && std::abs(bentGlobal / unbentGlobal - expectedBentRatio)
                      < 2.0e-4f
               && std::abs(bentMember / unbentMember - expectedBentRatio)
                      < 2.0e-4f,
           "pre-bent global/MPE attack did not scale compliance by f squared");

    // An inactive but audibly coupled ring is not a string at rest. Its first
    // played contact stays on the additive legacy excitation path.
    auto coupled = std::make_unique<ElectryEngine>();
    auto coupledParameters = immediate;
    coupledParameters.sympatheticAmount = 1.0f;
    coupled->prepare(48000.0, 512);
    coupled->setParameters(coupledParameters);
    coupled->reset();
    coupled->noteOn(45, 0.95f);
    StereoBuffer establishCoupling(static_cast<int>(0.7 * 48000.0));
    renderInto(*coupled, establishCoupling);
    const int highString = ElectryEngine::stringCount - 1;
    const auto coupledBefore = TestAccess::snapshot(*coupled, highString);
    expect(! coupledBefore.active && coupledBefore.sympatheticReady
               && coupledBefore.sympatheticEnergy > 1.0e-11f,
           "analytic retained-ring fixture did not establish sympathy");
    coupled->noteOn(64, 0.90f);
    const auto coupledAttack = TestAccess::snapshot(*coupled, highString);
    expect(coupledAttack.analyticReleaseAmplitude == 0.0f
               && coupledAttack.excitationAmplitude > 0.0f,
           "audible retained sympathetic ring entered the fresh seed path");
}

void testAnalyticReleaseMaximumRateAttackCost()
{
    constexpr double sampleRate = 384000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.strumSpreadSeconds = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
        engine.noteOn(note, 0.90f);

    // Stop after the Contact sample that transitions every string to Release.
    // At 384 kHz there is one internal sample per host frame, so the following
    // 512-frame host block begins with all eight O(delay) seeds together.
    StereoBuffer one(1);
    int guard = 0;
    while (TestAccess::snapshot(engine, 0).excitationInContact
           && guard++ < static_cast<int>(sampleRate))
        renderInto(engine, one, 1);
    bool simultaneousRelease = true;
    for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount;
         ++stringIndex)
    {
        const auto voice = TestAccess::snapshot(engine, stringIndex);
        simultaneousRelease = simultaneousRelease
            && voice.excitationInRelease
            && voice.analyticReleaseAmplitude > 0.0f;
    }
    expect(simultaneousRelease,
           "maximum-rate analytic seeds did not share one Release boundary");

    StereoBuffer attack(512);
    const auto begin = std::chrono::steady_clock::now();
    renderInto(engine, attack, 512);
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - begin).count();
    const double realtimeRatio = elapsed
        / (static_cast<double>(attack.size()) / sampleRate);
    expect(allFinite(attack) && peakAbs(attack.left) > 1.0e-6f,
           "maximum-rate analytic-release attack was silent or non-finite");
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    constexpr double ceiling = 40.0;
#else
    constexpr double ceiling = 8.0;
#endif
    expect(realtimeRatio < ceiling,
           "simultaneous maximum-rate analytic seeds exceeded the portable "
           "CPU ceiling");
    std::cout << "PROBE analytic release 384 kHz simultaneous-seed 512-frame "
                 "block: " << 1.0e3 * elapsed << " ms, "
              << realtimeRatio << "x realtime\n";
}
#endif

void testModalResonatorPeakGain()
{
    // These span the open low strings, the complete solid-body mode table,
    // and the Q/gain ranges used by both the body and sympathetic banks. The
    // old numerator exceeded the requested gain by 100x or more down here.
    struct Case
    {
        float frequency;
        float q;
        float gain;
    };
    constexpr std::array<Case, 7> cases {{
        { 41.20f, 34.0f, 0.026f },
        { 61.74f, 28.0f, 0.040f },
        { 92.0f, 30.0f, 1.20f },
        { 112.0f, 24.0f, 1.00f },
        { 220.0f, 18.0f, 0.68f },
        { 488.0f, 12.0f, 0.32f },
        { 690.0f, 9.0f, 0.46f },
    }};

    constexpr std::array<float, 3> internalSampleRates {
        96000.0f, 192000.0f, 384000.0f
    };
    for (const float internalSampleRate : internalSampleRates)
    {
        for (const auto& test : cases)
        {
            const double actual = TestAccess::modalMagnitudeAt(
                test.frequency, test.q, test.gain, internalSampleRate,
                test.frequency);
            const double relativeError = std::abs(actual - test.gain)
                                       / std::max<double>(test.gain, 1.0e-12);
            expect(relativeError < 0.005,
                   "modal resonator did not reproduce its requested peak gain at "
                       + std::to_string(test.frequency) + " Hz / "
                       + std::to_string(internalSampleRate) + " Hz sample rate"
                       + " (requested " + std::to_string(test.gain)
                       + ", actual " + std::to_string(actual) + ")");
        }
    }
}

#if ELECTRY_MEASURED_BODY_RESPONSE
void testMeasuredBodyResponsePhysics()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::array<float, 3> walnutFrequencies {
        108.2f, 200.5f, 420.6f
    };
    constexpr std::array<float, 3> walnutLossFactors {
        0.119f, 0.073f, 0.046f
    };
    constexpr std::array<float, 3> ashFrequencies {
        119.0f, 204.7f, 440.4f
    };
    constexpr std::array<float, 3> ashLossFactors {
        0.114f, 0.072f, 0.026f
    };
    static_assert(TestAccess::bodyModeCount() == 3);

    for (const float internalSampleRate : { 96000.0f, 192000.0f, 384000.0f })
    {
        for (const auto* endpoint : { &walnutFrequencies, &ashFrequencies })
        {
            const auto* lossFactors = endpoint == &walnutFrequencies
                ? &walnutLossFactors : &ashLossFactors;
            for (std::size_t mode = 0; mode < endpoint->size(); ++mode)
            {
                const float q = 1.0f / (*lossFactors)[mode];
                const double actual = TestAccess::modalMagnitudeAt(
                    (*endpoint)[mode], q, 1.0f, internalSampleRate,
                    (*endpoint)[mode]);
                expect(std::abs(actual - 1.0) < 0.005,
                       "Ray body resonator missed its requested peak");
            }
        }
    }

    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.bodySize = 0.5f;
    parameters.bodyResonance = 1.0f;
    parameters.artifactAmount = 0.0f;

    const auto configure = [&] (float wood)
    {
        parameters.bodyWood = wood;
        engine.setParameters(parameters);
        engine.reset();
    };
    const auto checkModes = [&] (const std::array<float, 3>& frequencies,
                                 const std::array<float, 3>& lossFactors,
                                 const char* material)
    {
        for (int mode = 0; mode < TestAccess::bodyModeCount(); ++mode)
        {
            const auto index = static_cast<std::size_t>(mode);
            const float expectedQ = 1.0f / lossFactors[index];
            expect(std::abs(TestAccess::bodyModeFrequency(engine, mode)
                            - frequencies[index]) < 0.002f,
                   std::string("measured ") + material
                       + " body-mode frequency missed Ray BP anchor "
                       + std::to_string(mode));
            expect(std::abs(TestAccess::bodyModeQ(engine, mode) / expectedQ
                            - 1.0f) < 2.0e-6f,
                   std::string("measured ") + material
                       + " body-mode Q missed Ray BP tan-delta anchor "
                       + std::to_string(mode));
            expect(TestAccess::bodyModeLevel(engine, mode) > 0.0f
                       && TestAccess::bodyModeLevel(engine, mode) <= 1.20f,
                   "unmeasured modal residue escaped conservative "
                   "direct-voicing bounds");
        }
    };

    configure(0.0f);
    checkModes(walnutFrequencies, walnutLossFactors, "walnut");
    configure(1.0f);
    checkModes(ashFrequencies, ashLossFactors, "ash");

    // Only the measured endpoints exist. Geometric interpolation is the
    // positive-domain morph convention and must not manufacture new poles.
    configure(0.5f);
    std::array<float, 3> midpointFrequencies {};
    std::array<float, 3> midpointLossFactors {};
    for (std::size_t mode = 0; mode < midpointFrequencies.size(); ++mode)
    {
        midpointFrequencies[mode] = std::sqrt(
            walnutFrequencies[mode] * ashFrequencies[mode]);
        midpointLossFactors[mode] = std::sqrt(
            walnutLossFactors[mode] * ashLossFactors[mode]);
    }
    checkModes(midpointFrequencies, midpointLossFactors, "midpoint");

    // Ray held geometry, joint and hardware fixed. With no independent Shape,
    // Construction or Size measurements those controls are exact no-ops in
    // this experiment, including the complete rendered signal.
    const auto renderInactiveAxes = [&] (float size, float shape,
                                         float construction)
    {
        ElectryEngine inactiveEngine;
        inactiveEngine.prepare(sampleRate, 512);
        EngineParameters inactiveParameters;
        inactiveParameters.bodyWood = 0.5f;
        inactiveParameters.bodySize = size;
        inactiveParameters.bodyShape = shape;
        inactiveParameters.construction = construction;
        inactiveParameters.bodyResonance = 1.0f;
        inactiveParameters.sympatheticAmount = 0.0f;
        inactiveParameters.artifactAmount = 0.0f;
        inactiveParameters.pickNoise = 0.0f;
        inactiveParameters.fingerNoise = 0.0f;
        inactiveParameters.releaseNoise = 0.0f;
        inactiveEngine.setParameters(inactiveParameters);
        return renderNote(inactiveEngine, sampleRate, 45, 0.8f,
                          PlayStyle::Sustain, 0.75);
    };
    const auto inactiveLow = renderInactiveAxes(0.0f, 0.0f, 0.0f);
    const auto inactiveHigh = renderInactiveAxes(1.0f, 1.0f, 1.0f);
    expect(inactiveLow.left == inactiveHigh.left
               && inactiveLow.right == inactiveHigh.right,
           "inactive body axes invented a Ray material response");

    expect(TestAccess::bodyOutputLevel(engine) == 2.0f,
           "measured direct body pickup did not use its conservative scalar");

    // Elliott measured the pickup's body-borne transfer against the complete
    // pickup voltage, not against an arbitrary internal gain. Measure that
    // same end-to-end residual through Electry's coils, output guard and DC
    // path. The shipping model is deliberately only this quiet direct colour.
    const auto renderDirectCase = [&] (
        bool muteDirect, int midiNote, PickupSelector pickup,
        float shape, float construction, float wood)
    {
        ElectryEngine renderEngine;
        renderEngine.prepare(sampleRate, 512);
        EngineParameters renderParameters;
        renderParameters.pickupSelector = pickup;
        renderParameters.bodyWood = wood;
        renderParameters.bodySize = 0.5f;
        renderParameters.bodyShape = shape;
        renderParameters.construction = construction;
        renderParameters.bodyResonance = 1.0f;
        renderParameters.artifactAmount = 0.0f;
        renderParameters.pickNoise = 0.0f;
        renderParameters.fingerNoise = 0.0f;
        renderParameters.releaseNoise = 0.0f;
        renderEngine.setParameters(renderParameters);
        renderEngine.reset();
        if (muteDirect)
            TestAccess::muteDirectBodyPath(renderEngine);
        renderEngine.noteOn(midiNote, 0.8f);
        StereoBuffer rendered(static_cast<int>(0.8 * sampleRate));
        renderInto(renderEngine, rendered);
        return rendered;
    };
    struct DirectRatios
    {
        double attackDb;
        double settledDb;
    };
    const auto directRatiosAt = [&] (
        int midiNote, PickupSelector pickup,
        float shape, float construction, float wood)
    {
        const auto totalBody = renderDirectCase(
            false, midiNote, pickup, shape, construction, wood);
        const auto noDirectBody = renderDirectCase(
            true, midiNote, pickup, shape, construction, wood);
        const auto ratioDb = [&] (int start, int end)
        {
            double directEnergy = 0.0;
            double totalEnergy = 0.0;
            for (int sample = start; sample < end; ++sample)
            {
                const auto index = static_cast<std::size_t>(sample);
                const double residual = static_cast<double>(
                    totalBody.left[index]) - noDirectBody.left[index];
                directEnergy += residual * residual;
                totalEnergy += static_cast<double>(totalBody.left[index])
                             * static_cast<double>(totalBody.left[index]);
            }
            return 10.0 * std::log10(
                directEnergy / std::max(totalEnergy, 1.0e-30));
        };
        return DirectRatios {
            ratioDb(0, static_cast<int>(0.03 * sampleRate)),
            ratioDb(static_cast<int>(0.03 * sampleRate), totalBody.size())
        };
    };
    const auto directA2 = directRatiosAt(
        45, PickupSelector::Bridge, 0.5f, 0.5f, 0.5f);
    std::cout << "PROBE direct body/total pickup RMS: " << directA2.settledDb
              << " dB\n";
    expect(directA2.settledDb < -30.0 && directA2.settledDb > -50.0,
           "direct body pickup escaped the measured quiet-transfer region");

    // A single broadband A2 fixture could hide an oversized resonant note or
    // pickup mix. Sweep the three Ray BP modal neighbourhoods through every
    // selector and both material endpoints. Elliott's exceptional narrow
    // feature near 700 Hz does not license a loud broadband body signal.
    double maximumAttackDb = -std::numeric_limits<double>::infinity();
    double maximumSettledDb = -std::numeric_limits<double>::infinity();
    const auto includeDirectRatios = [&] (const DirectRatios& ratios)
    {
        expect(std::isfinite(ratios.attackDb)
                   && std::isfinite(ratios.settledDb),
               "direct body pickup sweep produced a non-finite ratio");
        maximumAttackDb = std::max(maximumAttackDb, ratios.attackDb);
        maximumSettledDb = std::max(maximumSettledDb, ratios.settledDb);
    };
    for (const int midiNote : { 45, 55, 69 })
    {
        for (const auto pickup : { PickupSelector::Neck,
                                  PickupSelector::Both,
                                  PickupSelector::Bridge })
            includeDirectRatios(directRatiosAt(
                midiNote, pickup, 0.5f, 0.5f, 0.5f));
    }
    for (const int midiNote : { 45, 69 })
    {
        includeDirectRatios(directRatiosAt(
            midiNote, PickupSelector::Both, 0.0f, 0.0f, 0.0f));
        includeDirectRatios(directRatiosAt(
            midiNote, PickupSelector::Both, 1.0f, 1.0f, 1.0f));
    }
    std::cout << "PROBE direct body sweep attack/settled maxima: "
              << maximumAttackDb << ", " << maximumSettledDb << " dB\n";
    expect(maximumAttackDb < -30.0 && maximumSettledDb < -24.0,
           "direct body pickup became a broad parallel body-EQ signal");

    // Ray supplies no fret-specific complex mobility and found no significant
    // fundamental-decay difference. Even at full Body Resonance, the material
    // experiment must leave the string termination exactly unchanged.
    parameters.bodyShape = 0.0f;
    parameters.construction = 0.0f;
    parameters.bodyWood = 0.0f;
    parameters.bodySize = 0.5f;
    parameters.bodyResonance = 1.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(30, 0.8f);
    const int stringIndex = TestAccess::stringForNote(engine, 30);
    const auto loaded = TestAccess::snapshot(engine, stringIndex);
    expect(loaded.bodyConductance == 0.0f
               && loaded.bodyLossFactor == 1.0f,
           "Ray material colour invented a string-termination loss");

    parameters.bodyResonance = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(30, 0.8f);
    const auto bypassed = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 30));
    expect(bypassed.bodyLossFactor == 1.0f
               && TestAccess::bodyOutputLevel(engine) == 0.0f,
           "zero Body Resonance did not bypass direct body colour exactly");

    // Host automation must reach that same exact direct-path bypass without
    // resetting a held string; the smoother must not leave a tiny residue.
    parameters.bodyResonance = 1.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(30, 0.8f);
    StereoBuffer beforeAutomation(static_cast<int>(0.05 * sampleRate));
    renderInto(engine, beforeAutomation);
    parameters.bodyResonance = 0.0f;
    engine.setParameters(parameters);
    StereoBuffer afterAutomation(static_cast<int>(0.30 * sampleRate));
    renderInto(engine, afterAutomation);
    const auto automatedBypass = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 30));
    expect(automatedBypass.active
               && automatedBypass.bodyLossFactor == 1.0f
               && TestAccess::bodyOutputLevel(engine) == 0.0f,
           "automated zero Body Resonance did not reach exact held-note bypass");
}
#endif

void testLowRegisterGuitarEnvelope()
{
    constexpr double sampleRate = 48000.0;
    EngineParameters cleanParameters;
    cleanParameters.pickupSelector = PickupSelector::Bridge;
    cleanParameters.outputMode = electry::OutputMode::Mono;
    // Measure the pitched string itself. Incidental noises and sympathetic
    // hardware must not be what makes an otherwise thin E1/B1 pass.
    cleanParameters.artifactAmount = 0.0f;
    cleanParameters.pickNoise = 0.0f;
    cleanParameters.fingerNoise = 0.0f;
    cleanParameters.releaseNoise = 0.0f;

    struct NoteCase
    {
        int midiNote;
        const char* name;
        double minimumAttackLowShare;
        double minimumSustainLowShare;
    };
    constexpr std::array<NoteCase, 2> notes {{
        { 28, "E1", 0.20, 0.25 },
        { 35, "B1", 0.15, 0.20 },
    }};

    for (const auto& note : notes)
    {
        const auto validate = [&] (const EngineParameters& parameters,
                                   const char* variant,
                                   double maximumPeakToSustainDb)
        {
            ElectryEngine engine;
            engine.prepare(sampleRate, 512);
            engine.setParameters(parameters);
            const auto render = renderNote(
                engine, sampleRate, note.midiNote, 0.8f,
                PlayStyle::Sustain, 4.1);
            const double fundamental = midiHz(note.midiNote);
            const auto attack = measureHarmonicBalance(
                render.left, static_cast<int>(0.03 * sampleRate),
                static_cast<int>(0.15 * sampleRate), sampleRate, fundamental);
            const auto sustain = measureHarmonicBalance(
                render.left, static_cast<int>(0.25 * sampleRate),
                static_cast<int>(0.40 * sampleRate), sampleRate, fundamental);
            const auto late = measureHarmonicBalance(
                render.left, static_cast<int>(3.25 * sampleRate),
                static_cast<int>(0.40 * sampleRate), sampleRate, fundamental);
            const std::string prefix = std::string(note.name) + " " + variant;

            expect(attack.lowPowerShare >= note.minimumAttackLowShare,
                   prefix + " attack still lacks low partials ("
                       + std::to_string(100.0 * attack.lowPowerShare) + "%)");
            expect(attack.highPowerShare <= 0.50,
                   prefix + " attack remains clavinet-bright ("
                       + std::to_string(100.0 * attack.highPowerShare)
                       + "% above 1 kHz)");
            expect(sustain.lowPowerShare >= note.minimumSustainLowShare,
                   prefix + " sustain still lacks low partials ("
                       + std::to_string(100.0 * sustain.lowPowerShare) + "%)");
            expect(sustain.highPowerShare <= 0.35,
                   prefix + " sustain remains upper-harmonic dominated ("
                       + std::to_string(100.0 * sustain.highPowerShare)
                       + "% above 1 kHz)");
            expect(attack.strongestFirstEightDb <= 16.0,
                   prefix + " attack partial exceeds the fundamental by "
                       + std::to_string(attack.strongestFirstEightDb) + " dB");
            expect(sustain.strongestFirstEightDb <= 12.0,
                   prefix + " sustained partial exceeds the fundamental by "
                       + std::to_string(sustain.strongestFirstEightDb) + " dB");

            const double attackToSustain = decibels(
                peakAbs(render.left, 0, static_cast<int>(0.20 * sampleRate))
                / std::max(rmsInRange(
                    render.left, static_cast<int>(0.20 * sampleRate),
                    static_cast<int>(0.70 * sampleRate)), 1.0e-15));
            expect(attackToSustain <= maximumPeakToSustainDb,
                   prefix + " is too transient-heavy (peak/sustain "
                       + std::to_string(attackToSustain) + " dB)");

            const double earlyRms = rmsInRange(
                render.left, static_cast<int>(0.05 * sampleRate),
                static_cast<int>(0.20 * sampleRate));
            const auto expectTailAbove = [&] (double begin, double end,
                                              double minimumDb)
            {
                const double tailRms = rmsInRange(
                    render.left, static_cast<int>(begin * sampleRate),
                    static_cast<int>(end * sampleRate));
                const double relativeDb = decibels(
                    tailRms / std::max(earlyRms, 1.0e-15));
                expect(relativeDb >= minimumDb,
                       prefix + " string dies too early at "
                           + std::to_string(begin) + "-" + std::to_string(end)
                           + " s (" + std::to_string(relativeDb) + " dB)");
            };
            expectTailAbove(0.50, 1.00, -8.0);
            expectTailAbove(1.00, 2.00, -15.0);
            expectTailAbove(2.00, 4.00, -26.0);

            const double lowDecayDb = decibels(
                late.lowMagnitude / std::max(sustain.lowMagnitude, 1.0e-15));
            const double apparentT60 = lowDecayDb < -1.0e-6
                ? -60.0 * 3.0 / lowDecayDb
                : 1.0e6;
            // Calibrated against a dry electric low-E reference recording, whose
            // overall level falls about 24 dB over the eight seconds after the
            // attack and whose fundamental partial decays more slowly still.
            expect(apparentT60 >= 4.0 && apparentT60 <= 26.0,
                   prefix + " low-partial T60 left the guitar range ("
                       + std::to_string(apparentT60) + " s)");
        };

        validate(cleanParameters, "clean physical string", 15.0);
        // The normal preset retains a small direct plectrum/contact transient;
        // allow that realistic edge without returning to the old 22 dB
        // clavinet-like attack-to-sustain ratio.
        validate(EngineParameters {}, "default output", 17.0);

    }
}

void testOpenLowStringLevelBalance()
{
    constexpr double sampleRate = 48000.0;
    constexpr double renderSeconds = 0.75;

    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.outputMode = electry::OutputMode::Mono;

    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(parameters);

    const auto e1 = renderNote(engine, sampleRate, 28, 0.8f,
                               PlayStyle::Sustain, renderSeconds);
    const auto b1 = renderNote(engine, sampleRate, 35, 0.8f,
                               PlayStyle::Sustain, renderSeconds);
    const auto e2 = renderNote(engine, sampleRate, 40, 0.8f,
                               PlayStyle::Sustain, renderSeconds);
    const auto a2 = renderNote(engine, sampleRate, 45, 0.8f,
                               PlayStyle::Sustain, renderSeconds);

    const std::array<const StereoBuffer*, 4> renders {{ &e1, &b1, &e2, &a2 }};
    struct Window
    {
        const char* name;
        double beginSeconds;
        double endSeconds;
        double minimumE1;
        double minimumB1;
    };
    constexpr std::array<Window, 3> windows {{
        { "attack", 0.004, 0.115, 0.003981, 0.003981 },
        { "body", 0.050, 0.200, 0.003981, 0.003981 },
        { "sustain", 0.200, 0.700, 0.002512, 0.002512 },
    }};

    for (const auto& window : windows)
    {
        std::array<double, 4> rms {};
        for (std::size_t i = 0; i < renders.size(); ++i)
        {
            rms[i] = rmsInRange(
                renders[i]->left,
                static_cast<int>(window.beginSeconds * sampleRate),
                static_cast<int>(window.endSeconds * sampleRate));
        }

        expect(rms[0] >= window.minimumE1,
               std::string(window.name) + " E1 is effectively silent (RMS "
                   + std::to_string(rms[0]) + ")");
        expect(rms[1] >= window.minimumB1,
               std::string(window.name) + " B1 is effectively silent (RMS "
                   + std::to_string(rms[1]) + ")");

        const double reference = std::max(rms[3], 1.0e-15);
        const std::array<double, 3> ratios {
            rms[0] / reference, rms[1] / reference, rms[2] / reference
        };
        constexpr std::array<double, 3> minimumRatios {
            0.5012, 0.6310, 0.7079 // -6, -4, and -3 dB versus A2.
        };
        for (std::size_t index = 0; index < ratios.size(); ++index)
        {
            expect(ratios[index] >= minimumRatios[index],
                   std::string(window.name) + " low string "
                       + std::to_string(index) + " is under-balanced versus A2 ("
                       + std::to_string(decibels(ratios[index])) + " dB)");
            expect(ratios[index] <= 1.585,
                   std::string(window.name) + " low string "
                       + std::to_string(index) + " is over-compensated versus A2 ("
                       + std::to_string(decibels(ratios[index])) + " dB)");
        }
    }

    expect(peakAbs(e1.left) < 0.50f && peakAbs(b1.left) < 0.50f,
           "low-register level compensation is driving every note into the guard");
}

void testInternalOversamplingPolicy()
{
    struct RateCase { double hostRate; int expectedFactor; };
    constexpr std::array<RateCase, 5> rates {{
        { 44100.0, 2 }, { 48000.0, 2 }, { 96000.0, 2 },
        { 192000.0, 1 }, { 384000.0, 1 },
    }};

    for (const auto& rate : rates)
    {
        ElectryEngine engine;
        engine.prepare(rate.hostRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);

        expect(TestAccess::oversamplingFactor(engine) == rate.expectedFactor,
               "wrong internal oversampling factor at "
                   + std::to_string(rate.hostRate) + " Hz");
        expect(TestAccess::hostSampleRate(engine) == rate.hostRate,
               "host sample rate was not retained by prepare()");
        expect(TestAccess::internalSampleRate(engine)
                   == rate.hostRate * rate.expectedFactor,
               "internal sample clock does not match host rate times factor");

        engine.noteOn(45, 0.8f);
        constexpr int hostSamples = 1024;
        StereoBuffer buffer(hostSamples);
        renderInto(engine, buffer, 127);
        expect(allFinite(buffer),
               "oversampled render became non-finite at "
                   + std::to_string(rate.hostRate) + " Hz");
        const float peak = peakAbs(buffer.left);
        expect(peak > 1.0e-5f && peak < 0.80f,
               "oversampled render was silent or unbounded at "
                   + std::to_string(rate.hostRate) + " Hz");

        const int stringIndex = TestAccess::stringForNote(engine, 45);
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        expect(snapshot.ageSamples
                   == static_cast<std::uint64_t>(hostSamples * rate.expectedFactor),
               "host samples did not advance the physical clock exactly");
    }

    // Compare one 2x render with one native high-rate render. Both must retain
    // the played pitch after the halfband FIR and host-rate decimation.
    for (const double sampleRate : { 48000.0, 192000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);
        const auto buffer = renderNote(engine, sampleRate, 45, 0.35f,
                                       PlayStyle::Sustain, 0.8);
        const double expected = midiHz(45);
        const double measured = measureFrequency(
            buffer.left, static_cast<int>(0.30 * sampleRate),
            static_cast<int>(0.35 * sampleRate), sampleRate, expected);
        expect(std::abs(centsBetween(measured, expected)) < 10.0,
               "oversampling introduced gross pitch drift at "
                   + std::to_string(sampleRate) + " Hz");
    }
}

void testStringDelaySmoothingTimeConstant()
{
    for (const double hostRate : {
             44100.0, 48000.0, 88200.0, 96000.0,
             96001.0, 192000.0, 384000.0 })
    {
        ElectryEngine engine;
        engine.prepare(hostRate, 512);

        const double retention = TestAccess::voiceDelayRetention(engine);
        expect(retention > 0.0 && retention < 1.0,
               "string-delay retention is outside (0, 1) at "
                   + std::to_string(hostRate) + " Hz");

        const double timeConstant = -1.0
            / (TestAccess::internalSampleRate(engine)
               * std::log(retention));
        expect(std::abs(timeConstant - 0.006) < 0.000006,
               "string-delay smoothing is not 6 ms at "
                   + std::to_string(hostRate) + " Hz ("
                   + std::to_string(timeConstant) + " s)");
    }

    // Exercise the actual per-internal-sample recurrences, including both
    // sides of the 96 kHz oversampling seam. One time constant must leave
    // e^-1 of a known played and sympathetic delay step; coefficient
    // introspection alone would not catch a moved cadence or missed path.
    for (const double hostRate : { 44100.0, 96000.0, 96001.0, 384000.0 })
    {
        ElectryEngine engine;
        engine.prepare(hostRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.bodyResonance = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.sympatheticAmount = 1.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.9f);
        StereoBuffer wake(static_cast<int>(0.15 * hostRate));
        renderInto(engine, wake);

        const int playedString = TestAccess::stringForNote(engine, 45);
        constexpr int sympatheticString = 0;
        const auto beforePlayed = TestAccess::snapshot(engine, playedString);
        const auto beforeSympathetic = TestAccess::snapshot(
            engine, sympatheticString);
        expect(playedString >= 0 && beforeSympathetic.sympatheticReady
                   && beforeSympathetic.sympatheticEnergy > 1.0e-11f,
               "invalid string-delay cadence fixture at "
                   + std::to_string(hostRate) + " Hz");

        engine.setPitchBend(1.0f);
        TestAccess::snapPitchBendToTarget(engine);
        int alignmentSamples = 0;
        auto steppedPlayed = beforePlayed;
        auto steppedSympathetic = beforeSympathetic;
        while (alignmentSamples <= 16
               && (steppedPlayed.verticalDelayTarget
                       == beforePlayed.verticalDelayTarget
                   || steppedSympathetic.verticalDelayTarget
                       == beforeSympathetic.verticalDelayTarget))
        {
            TestAccess::renderOneInternalSample(engine);
            steppedPlayed = TestAccess::snapshot(engine, playedString);
            steppedSympathetic = TestAccess::snapshot(
                engine, sympatheticString);
            ++alignmentSamples;
        }

        const auto verticalResidual = [] (const TestAccess::VoiceSnapshot& voice)
        {
            return std::abs(static_cast<double>(voice.verticalDelayCurrent)
                            - voice.verticalDelayTarget);
        };
        const auto horizontalResidual = [] (
            const TestAccess::VoiceSnapshot& voice)
        {
            return std::abs(static_cast<double>(voice.horizontalDelayCurrent)
                            - voice.horizontalDelayTarget);
        };
        const double playedVerticalStart = verticalResidual(steppedPlayed);
        const double playedHorizontalStart = horizontalResidual(steppedPlayed);
        const double sympatheticStart = verticalResidual(steppedSympathetic);
        expect(steppedPlayed.verticalDelayTarget
                       != beforePlayed.verticalDelayTarget
                   && steppedPlayed.horizontalDelayTarget
                       != beforePlayed.horizontalDelayTarget
                   && steppedSympathetic.verticalDelayTarget
                       != beforeSympathetic.verticalDelayTarget
                   && playedVerticalStart > 1.0
                   && playedHorizontalStart > 1.0
                   && sympatheticStart > 1.0,
               "the played/sympathetic delay step was not established at "
                   + std::to_string(hostRate) + " Hz");

        const int timeConstantSamples = static_cast<int>(std::lround(
            0.006 * TestAccess::internalSampleRate(engine)));
        for (int sample = 0; sample < timeConstantSamples; ++sample)
            TestAccess::renderOneInternalSample(engine);
        const auto oneTauPlayed = TestAccess::snapshot(engine, playedString);
        const auto oneTauSympathetic = TestAccess::snapshot(
            engine, sympatheticString);
        constexpr double inverseE = 0.36787944117144233;
        const double playedVerticalRatio = verticalResidual(oneTauPlayed)
                                         / playedVerticalStart;
        const double playedHorizontalRatio = horizontalResidual(oneTauPlayed)
                                           / playedHorizontalStart;
        const double sympatheticRatio = verticalResidual(oneTauSympathetic)
                                      / sympatheticStart;
        expect(std::abs(playedVerticalRatio - inverseE) < 0.01
                   && std::abs(playedHorizontalRatio - inverseE) < 0.01
                   && std::abs(sympatheticRatio - inverseE) < 0.01,
               "the realised played-polarisation/sympathetic delay glide is not 6 ms at "
                   + std::to_string(hostRate) + " Hz ("
                   + std::to_string(playedVerticalRatio) + ", "
                   + std::to_string(playedHorizontalRatio) + ", "
                   + std::to_string(sympatheticRatio) + " of the step)");

        for (int sample = 0; sample < 18 * timeConstantSamples; ++sample)
            TestAccess::renderOneInternalSample(engine);
        const auto settledPlayed = TestAccess::snapshot(engine, playedString);
        const auto settledSympathetic = TestAccess::snapshot(
            engine, sympatheticString);
        expect(settledPlayed.verticalDelayCurrent
                       == settledPlayed.verticalDelayTarget
                   && settledPlayed.horizontalDelayCurrent
                       == settledPlayed.horizontalDelayTarget
                   && settledSympathetic.verticalDelayCurrent
                       == settledSympathetic.verticalDelayTarget,
               "the target-anchored delay glide did not reach its exact target at "
                   + std::to_string(hostRate) + " Hz ("
                   + std::to_string(verticalResidual(settledPlayed)) + ", "
                   + std::to_string(horizontalResidual(settledPlayed)) + ", "
                   + std::to_string(verticalResidual(settledSympathetic))
                   + " samples)");

        // A ready loop below the render floor has no audible pitch to glide.
        // Retuning it must snap now, rather than waking later at a stale bend.
        TestAccess::setSympatheticEnergy(engine, sympatheticString, 0.0f);
        engine.setPitchBend(-1.0f);
        TestAccess::snapPitchBendToTarget(engine);
        auto silentRetune = settledSympathetic;
        for (int sample = 0; sample <= 16
             && silentRetune.verticalDelayTarget
                    == settledSympathetic.verticalDelayTarget; ++sample)
        {
            TestAccess::setSympatheticEnergy(
                engine, sympatheticString, 0.0f);
            TestAccess::renderOneInternalSample(engine);
            silentRetune = TestAccess::snapshot(engine, sympatheticString);
        }
        expect(silentRetune.verticalDelayTarget
                       != settledSympathetic.verticalDelayTarget
                   && silentRetune.verticalDelayCurrent
                       == silentRetune.verticalDelayTarget,
               "an inaudible sympathetic string retained stale pitch at "
                   + std::to_string(hostRate) + " Hz");
    }
}

// prepare() clamps a hostile host sample rate to [minimumSupportedSampleRate,
// maximumSupportedSampleRate] (falling back to 48 kHz first if it is not even
// finite) before any delay line is sized from it - see the comment on those
// two constants in ElectryEngine.h. Every other test only ever calls
// prepare() with a sane host rate, so that guard was previously exercised
// only by inspection.
void testPrepareClampsHostileSampleRate()
{
    const double minimumRate = TestAccess::minimumSupportedSampleRate();
    const double maximumRate = TestAccess::maximumSupportedSampleRate();

    struct HostileCase { double requested; double expectedClamped; const char* name; };
    const std::array<HostileCase, 5> cases {{
        { std::numeric_limits<double>::quiet_NaN(), 48000.0, "NaN" },
        { -1.0e9, minimumRate, "large negative" },
        { 0.0, minimumRate, "zero" },
        { 1.0, minimumRate, "below the floor" },
        { 1.0e9, maximumRate, "far above the ceiling" },
    }};

    for (const auto& hostileCase : cases)
    {
        ElectryEngine engine;
        engine.prepare(hostileCase.requested, 512);

        expect(TestAccess::hostSampleRate(engine) == hostileCase.expectedClamped,
               std::string("prepare() did not clamp a ") + hostileCase.name
                   + " sample rate to the documented bound");
        expect(TestAccess::internalSampleRate(engine)
                   == TestAccess::hostSampleRate(engine)
                          * static_cast<double>(
                                TestAccess::oversamplingFactor(engine)),
               std::string("internal clock did not track the clamped rate for ")
                   + hostileCase.name);

        EngineParameters parameters;
        engine.setParameters(parameters);
        engine.noteOn(45, 0.8f);
        StereoBuffer buffer(256);
        renderInto(engine, buffer);
        expect(allFinite(buffer),
               std::string("a ") + hostileCase.name
                   + " sample rate produced non-finite audio after clamping");
        expect(peakAbs(buffer.left) < 16.0f,
               std::string("a ") + hostileCase.name
                   + " sample rate bypassed the output guardrail");
    }
}

void testRenderMatrixFiniteAndBounded()
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        engine.setParameters(parameters);

        for (int styleIndex = 0;
             styleIndex < ElectryEngine::playStyleKeyswitchCount; ++styleIndex)
        for (int pickIndex = 0;
             pickIndex < ElectryEngine::pickStyleKeyswitchCount; ++pickIndex)
        {
            const auto style = static_cast<PlayStyle>(styleIndex);
            const auto pick = static_cast<PickStyle>(pickIndex);
            const std::string comboName = "style " + std::to_string(styleIndex)
                                        + " pick " + std::to_string(pickIndex);
            auto buffer = renderNote(engine, sampleRate, 45, 0.9f, style,
                                     0.5, 0.35, pick);
            expect(allFinite(buffer),
                   "non-finite output at rate " + std::to_string(sampleRate)
                       + " " + comboName);
            const float peak = peakAbs(buffer.left);
            expect(peak < 0.80f,
                   "output beyond guard at rate " + std::to_string(sampleRate)
                       + " " + comboName);
            expect(peak > 1.0e-4f,
                   comboName + " is silent at rate "
                       + std::to_string(sampleRate));

            if (sampleRate == 48000.0)
            {
                const auto low = renderNote(
                    engine, sampleRate, 28, 0.9f, style, 0.5, 0.35, pick);
                const float lowPeak = peakAbs(low.left);
                expect(allFinite(low),
                       "non-finite Drop-E output for " + comboName);
                expect(lowPeak > 1.0e-4f && lowPeak < 0.80f,
                       "Drop-E combination is silent or driving the guard: "
                           + comboName + " (peak " + std::to_string(lowPeak)
                           + ")");
            }
        }
    }
}

void testPitchAccuracy()
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);

        for (const int midiNote : { 28, 35, 40, 45, 50, 55, 59, 64, 69, 76, 86 })
        {
            auto buffer = renderNote(engine, sampleRate, midiNote, 0.3f,
                                     PlayStyle::Sustain, 1.1);
            const int start = static_cast<int>(0.45 * sampleRate);
            const int window = static_cast<int>(0.5 * sampleRate);
            const double expected = midiHz(midiNote);
            const double measured = measureFrequency(buffer.left, start, window,
                                                     sampleRate, expected);
            const double cents = centsBetween(measured, expected);
            expect(std::abs(cents) < 8.0,
                   "note " + std::to_string(midiNote) + " at rate "
                       + std::to_string(sampleRate) + " off by "
                       + std::to_string(cents) + " cents");
        }
    }
}

void testDropELowNoteAtMaximumRate()
{
    constexpr double sampleRate = 384000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);
    engine.setPitchBend(-1.0f);
    engine.noteOn(28, 0.25f);

    StereoBuffer buffer(static_cast<int>(1.25 * sampleRate));
    renderInto(engine, buffer);
    expect(allFinite(buffer), "Drop-E render became non-finite at 384 kHz");

    const int stringIndex = TestAccess::stringForNote(engine, 28);
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(stringIndex == 0 && snapshot.fret == 0,
           "open E1 was not allocated to the eighth string");
    expect(snapshot.verticalDelayTarget > 9000.0f,
           "maximum-rate Drop-E delay did not exercise the expanded line");
    expect(snapshot.verticalDelayTarget
               < static_cast<float>(TestAccess::delayLineCapacity() - 8),
           "maximum-rate Drop-E delay exceeded the delay-line capacity");

    const double expected = midiHz(26); // E1 with the wheel at -2 semitones.
    const double measured = measureFrequency(
        buffer.left, static_cast<int>(0.55 * sampleRate),
        static_cast<int>(0.60 * sampleRate), sampleRate, expected);
    expect(std::abs(centsBetween(measured, expected)) < 10.0,
           "384 kHz Drop-E wheel-down pitch is inaccurate");
}

// prepare()'s sample-rate guard - a non-finite rate falls back to 48 kHz,
// and any finite rate is then clamped to [minimumSupportedSampleRate,
// maximumSupportedSampleRate], 8 kHz and 384 kHz respectively - was only
// ever driven with rates already inside that range (44.1/48/96/192/384 kHz
// across the suite). Confirms the guard actually lands on the same internal
// clock as an explicit prepare() at the fallback/clamped rate, by comparing
// the resulting delay-line target for the same open note, and that the
// engine keeps rendering finite audio.
void testPrepareSanitisesSampleRate()
{
    const auto openStringDelayTarget = [] (double sampleRate)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 256);
        EngineParameters parameters;
        engine.setParameters(parameters);
        engine.noteOn(40, 0.8f);

        StereoBuffer buffer(2048);
        renderInto(engine, buffer);
        expect(allFinite(buffer),
               "a sanitised prepare() sample rate produced non-finite audio");

        const int stringIndex = TestAccess::stringForNote(engine, 40);
        return TestAccess::snapshot(engine, stringIndex).verticalDelayTarget;
    };

    constexpr double minimumSupportedSampleRate = 8000.0;
    constexpr double maximumSupportedSampleRate = 384000.0;

    expect(openStringDelayTarget(std::nan(""))
               == openStringDelayTarget(48000.0),
           "a NaN sample rate did not fall back to the 48 kHz default");
    expect(openStringDelayTarget(1.0e9)
               == openStringDelayTarget(maximumSupportedSampleRate),
           "a sample rate above the ceiling was not clamped to it");
    expect(openStringDelayTarget(1.0)
               == openStringDelayTarget(minimumSupportedSampleRate),
           "a sample rate below the floor was not clamped to it");
    expect(openStringDelayTarget(-48000.0)
               == openStringDelayTarget(minimumSupportedSampleRate),
           "a negative sample rate was not clamped to the floor");
}

// process()'s own guard - a null left or right pointer, or a non-positive
// sample count, is a no-op, and an unprepared engine fills the caller's
// buffer with silence and returns rather than touching any voice state - was
// never driven directly anywhere in the suite: every call site above always
// passes two valid pointers, a positive length and an already-prepared
// engine. ElectryFx::process() carries the identical guard shape and has its
// own direct coverage in testHostileInput(); this closes the same gap here.
void testProcessRejectsInvalidBuffers()
{
    ElectryEngine engine;
    engine.prepare(48000.0, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(pickKeyswitch(PickStyle::Down), 1.0f);
    engine.noteOn(40, 0.9f);

    // Nonzero sentinels, distinct per channel, so a regression that clears
    // the surviving channel instead of leaving it untouched is caught -
    // zero-initialised buffers would hide exactly that bug.
    std::vector<float> left(64, 0.31f);
    std::vector<float> right(64, -0.47f);
    const std::vector<float> leftSentinel = left;
    const std::vector<float> rightSentinel = right;

    // A null pointer, on either side, must be a no-op rather than a crash -
    // including leaving the other, valid channel's buffer untouched.
    engine.process(nullptr, right.data(), static_cast<int>(right.size()));
    engine.process(left.data(), nullptr, static_cast<int>(left.size()));
    expect(left == leftSentinel && right == rightSentinel,
           "a null-pointer process() call wrote into the other, valid channel");

    // A non-positive sample count must also be a no-op.
    engine.process(left.data(), right.data(), 0);
    engine.process(left.data(), right.data(), -4);
    expect(left == leftSentinel && right == rightSentinel,
           "a non-positive sample count still wrote into the buffers");

    expect(engine.getActiveVoiceCount() == 1,
           "an invalid process() call disturbed the sounding voice");

    // A genuinely valid call still renders audibly, showing the guards above
    // rejected only the hostile shapes and not every call.
    StereoBuffer buffer(2048);
    renderInto(engine, buffer);
    expect(allFinite(buffer) && peakAbs(buffer.left) > 1.0e-4f,
           "a valid process() call after the hostile ones produced no audio");

    // An unprepared engine fills the caller's buffer with silence and
    // returns, rather than touching voice state sized for whatever the
    // engine was (or was never) prepared at.
    ElectryEngine fresh;
    std::vector<float> unpreparedLeft(256, 0.7f);
    std::vector<float> unpreparedRight(256, -0.7f);
    fresh.process(unpreparedLeft.data(), unpreparedRight.data(),
                 static_cast<int>(unpreparedLeft.size()));
    expect(std::all_of(unpreparedLeft.begin(), unpreparedLeft.end(),
                       [] (float sample) { return sample == 0.0f; })
               && std::all_of(unpreparedRight.begin(), unpreparedRight.end(),
                              [] (float sample) { return sample == 0.0f; }),
           "an unprepared engine did not fill the buffer with silence");
    expect(fresh.getActiveVoiceCount() == 0,
           "an unprepared engine's process() call created a voice");
}

void testDeterminism()
{
    constexpr double sampleRate = 48000.0;
    const auto renderSequence = [] ()
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 256);
        EngineParameters parameters;
        engine.setParameters(parameters);
        engine.reset();

        StereoBuffer buffer(static_cast<int>(1.6 * sampleRate));
        engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
        engine.noteOn(pickKeyswitch(PickStyle::Up), 1.0f);
        engine.noteOn(45, 0.85f);
        engine.process(buffer.left.data(), buffer.right.data(), 12000);
        engine.noteOn(styleKeyswitch(PlayStyle::Harmonics), 1.0f);
        engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
        engine.noteOn(52, 0.6f);
        engine.process(buffer.left.data() + 12000,
                       buffer.right.data() + 12000, 24000);
        engine.noteOff(45);
        engine.noteOff(52);
        engine.process(buffer.left.data() + 36000, buffer.right.data() + 36000,
                       buffer.size() - 36000);
        return buffer;
    };

    const auto first = renderSequence();
    const auto second = renderSequence();
    bool identical = true;
    for (std::size_t i = 0; i < first.left.size(); ++i)
        if (first.left[i] != second.left[i])
        {
            identical = false;
            break;
        }
    expect(identical, "identical MIDI does not render identical audio");

    const float peak = peakAbs(first.left);
    expect(peak > 1.0e-4f, "determinism fixture rendered silence");
}

void testKeyswitchesSelectStylesSilently()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();

    expect(ElectryEngine::firstKeyswitchNote == 12,
           "keyswitch range does not start at MIDI 12");
    expect(ElectryEngine::pickStyleKeyswitchCount == 3
               && ElectryEngine::playStyleKeyswitchCount == 7
               && ElectryEngine::keyswitchCount == 10,
           "the two keyswitch banks do not expose 3 picking and 7 play styles");
    expect(ElectryEngine::firstPlayStyleKeyswitchNote == 15,
           "the play-style bank does not follow the picking bank at MIDI 15");
    expect(ElectryEngine::lowestPlayableNote == 28
               && ElectryEngine::highestPlayableNote == 86,
           "Drop-E playable range is not MIDI 28..86");

    expect(engine.getCurrentPickStyle() == PickStyle::Down
               && engine.getCurrentPlayStyle() == PlayStyle::Sustain,
           "default styles are not a sustained downstroke");

    // Keyswitches alone must never make sound.
    StereoBuffer buffer(static_cast<int>(0.25 * sampleRate));
    for (int keyswitch = 0; keyswitch < ElectryEngine::keyswitchCount; ++keyswitch)
        engine.noteOn(ElectryEngine::firstKeyswitchNote + keyswitch, 1.0f);
    renderInto(engine, buffer);
    expect(peakAbs(buffer.left) == 0.0f, "keyswitch notes produced audio");
    expect(engine.getActiveVoiceCount() == 0, "keyswitch notes created voices");

    // Walking every keyswitch leaves the last of each bank latched.
    expect(engine.getCurrentPickStyle() == PickStyle::Alternate
               && engine.getCurrentPlayStyle() == PlayStyle::Dead,
           "walking the keyswitch banks did not latch the last of each");

    // The two banks are independent: a play-style switch keeps the picking
    // style, and a picking switch keeps the play style.
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    expect(engine.getCurrentPlayStyle() == PlayStyle::PalmMute
               && engine.getCurrentPickStyle() == PickStyle::Alternate,
           "a play-style keyswitch disturbed the latched picking style");
    engine.noteOn(pickKeyswitch(PickStyle::Up), 1.0f);
    expect(engine.getCurrentPickStyle() == PickStyle::Up
               && engine.getCurrentPlayStyle() == PlayStyle::PalmMute,
           "a picking keyswitch disturbed the latched play style");

    // Keyswitch note-offs are ignored; the styles persist for played notes.
    engine.noteOff(styleKeyswitch(PlayStyle::PalmMute));
    engine.noteOff(pickKeyswitch(PickStyle::Up));
    expect(engine.getCurrentPlayStyle() == PlayStyle::PalmMute
               && engine.getCurrentPickStyle() == PickStyle::Up,
           "keyswitch note-offs cleared the latched styles");

    engine.noteOn(52, 0.8f);
    const auto stringIndex = TestAccess::stringForNote(engine, 52);
    expect(stringIndex >= 0, "played note did not allocate a string");
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(snapshot.playStyle == PlayStyle::PalmMute && snapshot.strokeIsUp,
           "played note did not inherit the latched style combination");

    // The notes between the keyswitch banks and the playable range are dead:
    // they neither sound nor disturb either latch.
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Harmonics), 1.0f);
    for (int note = ElectryEngine::firstKeyswitchNote
                  + ElectryEngine::keyswitchCount;
         note < ElectryEngine::lowestPlayableNote; ++note)
        engine.noteOn(note, 0.9f);
    expect(engine.getActiveVoiceCount() == 0,
           "a note between the keyswitches and the playable range sounded");
    expect(engine.getCurrentPlayStyle() == PlayStyle::Harmonics
               && engine.getCurrentPickStyle() == PickStyle::Down,
           "a dead-zone note disturbed a latched style");

    // The range boundaries are unambiguous: 18 is the final silent keyswitch
    // and 28 is the sounding open low E.
    engine.reset();
    engine.noteOn(18, 0.9f);
    expect(engine.getCurrentPlayStyle() == PlayStyle::Harmonics,
           "MIDI 18 did not select the final play style");
    expect(engine.getActiveVoiceCount() == 0,
           "final keyswitch note created a voice");
    engine.noteOn(28, 0.8f);
    expect(TestAccess::stringForNote(engine, 28) == 0,
           "MIDI 28 did not play open E1 on the lowest string");

    // Notes outside both the keyswitch and playable ranges are ignored.
    engine.noteOn(0, 0.9f);
    engine.noteOn(87, 0.9f);
    expect(engine.getActiveVoiceCount() == 1,
           "notes outside keyswitches and E1..D6 were not ignored");
}

void testOverlappingSameNoteOffKeepsLatestRepickHeld()
{
    ElectryEngine engine;
    engine.prepare(48000.0, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);

    // A DAW can place the next Note On before the previous Note Off when two
    // repeated notes meet at one timestamp. Both starts still have matching
    // ends; the older end must not release the freshly repicked physical
    // string.
    engine.noteOn(28, 0.90f);
    engine.noteOn(28, 0.95f);
    const int stringIndex = TestAccess::stringForNote(engine, 28);
    expect(stringIndex == 0, "overlapping E1 repick left the lowest string");

    engine.noteOff(28);
    const auto afterOlderOff = TestAccess::snapshot(engine, stringIndex);
    expect(afterOlderOff.keyDown && ! afterOlderOff.releasing,
           "an older Note Off released the latest same-note Palm repick");

    engine.noteOff(28);
    const auto afterLatestOff = TestAccess::snapshot(engine, stringIndex);
    expect(! afterLatestOff.keyDown && afterLatestOff.releasing,
           "the final matching Note Off did not release the Palm repick");
}

void testHeldStringRepickKeys()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::array<int, ElectryEngine::stringCount> openNotes {
        28, 35, 40, 45, 50, 55, 59, 64
    };

    expect(ElectryEngine::firstRepickNote == 88
               && ElectryEngine::repickNoteCount == 8
               && ElectryEngine::firstRepickNote
                      + ElectryEngine::repickNoteCount - 1 == 95,
           "held-string repick mapping drifted from MIDI E6..B6");

    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.sympatheticAmount = 0.0f;
    engine.setParameters(parameters);

    // The eight command notes are picking-hand gestures, not pitches. With no
    // physically held string they are silent and allocate no voice.
    engine.reset();
    for (int trigger = 0; trigger < ElectryEngine::repickNoteCount; ++trigger)
        engine.noteOn(ElectryEngine::firstRepickNote + trigger, 0.9f);
    StereoBuffer silence(256);
    renderInto(engine, silence);
    expect(engine.getActiveVoiceCount() == 0 && peakAbs(silence.left) == 0.0f,
           "an unowned held-string repick key produced a voice or sound");

    // E6..B6 map directly from the lowest physical string to the highest. Age
    // resets only if the requested held string really took a fresh attack.
    for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount; ++stringIndex)
    {
        engine.reset();
        const int note = openNotes[static_cast<std::size_t>(stringIndex)];
        engine.noteOn(note, 0.75f);
        StereoBuffer aged(128);
        renderInto(engine, aged);
        const auto before = TestAccess::snapshot(engine, stringIndex);

        engine.noteOn(ElectryEngine::firstRepickNote + stringIndex, 0.95f);
        const auto after = TestAccess::snapshot(engine, stringIndex);
        expect(before.active && before.ageSamples > 0 && after.active
                   && after.midiNote == note && after.ageSamples == 0
                   && engine.getActiveVoiceCount() == 1,
               "held-string repick key did not retrigger only physical string "
                   + std::to_string(stringIndex));
    }

    // Repicks are new wrist strokes and capture the current articulation, but
    // neither their Note Offs nor any number of triggers add fretting ownership.
    engine.reset();
    engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    engine.noteOn(openNotes[0], 0.80f);
    expect(! TestAccess::snapshot(engine, 0).strokeIsUp,
           "held-string Alternate fixture did not begin down");

    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(ElectryEngine::firstRepickNote, 0.90f);
    const auto mutedUp = TestAccess::snapshot(engine, 0);
    expect(mutedUp.playStyle == PlayStyle::PalmMute && mutedUp.strokeIsUp,
           "held-string repick did not capture same-sample Mute or advance Alternate");
    engine.noteOff(ElectryEngine::firstRepickNote);
    expect(TestAccess::snapshot(engine, 0).keyDown,
           "held-string repick Note Off released the fretted pitch");

    engine.noteOn(styleKeyswitch(PlayStyle::Dead), 1.0f);
    engine.noteOn(ElectryEngine::firstRepickNote, 0.95f);
    const auto deadDown = TestAccess::snapshot(engine, 0);
    expect(deadDown.playStyle == PlayStyle::Dead && ! deadDown.strokeIsUp,
           "second held-string repick did not capture Dead or alternate down");

    engine.noteOn(ElectryEngine::firstRepickNote, 0.85f);
    expect(TestAccess::snapshot(engine, 0).strokeIsUp,
           "third held-string repick did not continue the Alternate sequence");
    engine.noteOff(openNotes[0]);
    const auto released = TestAccess::snapshot(engine, 0);
    expect(! released.keyDown && released.releasing,
           "repeated held-string repicks added unmatched fretting-key ownership");

    // A physically held key outlives the audible waveguide. Dead A2 decays all
    // the way through normal retirement, remains visible at zero level, blocks
    // that stopped string from being reused or sympathetically opened, and can
    // still be restarted by its picking-hand key.
    ElectryEngine retired;
    retired.prepare(sampleRate, 512);
    auto retirementParameters = parameters;
    retirementParameters.sympatheticAmount = 0.80f;
    retired.setParameters(retirementParameters);
    retired.reset();
    retired.noteOn(styleKeyswitch(PlayStyle::Dead), 1.0f);
    retired.noteOn(45, 0.90f); // open A2, physical string index 3

    double waited = 0.0;
    while (retired.getActiveVoiceCount() > 0 && waited < 8.0)
    {
        StereoBuffer decay(static_cast<int>(0.25 * sampleRate));
        renderInto(retired, decay);
        waited += 0.25;
    }
    expect(retired.getActiveVoiceCount() == 0,
           "held Dead string did not reach normal audio retirement");

    std::array<electry::StringVisualState, ElectryEngine::stringCount> visuals {};
    retired.getStringVisualState(visuals);
    expect(visuals[3].sounding && ! visuals[3].sympathetic
               && visuals[3].midiNote == 45 && visuals[3].fret == 0
               && visuals[3].level == 0.0f,
           "retired held string lost its zero-level fretting-hand display");

    // An ordinary overlapping Note On after retirement also reuses the held
    // string and retains the older owner when its newer matching Note Off lands.
    retired.noteOn(45, 0.85f);
    expect(TestAccess::stringForNote(retired, 45) == 3,
           "ordinary overlap did not reuse its retired held string");
    retired.noteOff(45);
    expect(TestAccess::snapshot(retired, 3).keyDown,
           "newer Note Off cleared the older retired-string owner");
    waited = 0.0;
    while (retired.getActiveVoiceCount() > 0 && waited < 8.0)
    {
        StereoBuffer decay(static_cast<int>(0.25 * sampleRate));
        renderInto(retired, decay);
        waited += 0.25;
    }
    expect(retired.getActiveVoiceCount() == 0,
           "overlapped held Dead string did not retire again");

    retired.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    retired.noteOn(47, 0.85f);
    expect(TestAccess::stringForNote(retired, 47) == 2,
           "allocator reused a retired but physically held string");
    StereoBuffer coupled(static_cast<int>(0.15 * sampleRate));
    renderInto(retired, coupled);
    expect(! TestAccess::snapshot(retired, 3).sympatheticReady,
           "retired held string reopened as an unfretted sympathetic string");

    retired.noteOn(styleKeyswitch(PlayStyle::Dead), 1.0f);
    retired.noteOn(ElectryEngine::firstRepickNote + 3, 0.95f);
    const auto revived = TestAccess::snapshot(retired, 3);
    expect(revived.active && revived.keyDown && revived.midiNote == 45
               && revived.playStyle == PlayStyle::Dead,
           "per-string key did not revive its fully retired held Dead note");
    retired.noteOff(ElectryEngine::firstRepickNote + 3);
    expect(TestAccess::snapshot(retired, 3).keyDown,
           "revived string was released by its picking-hand key-up");
    retired.noteOff(45);
    const auto revivedRelease = TestAccess::snapshot(retired, 3);
    expect(! revivedRelease.keyDown && revivedRelease.releasing,
           "original Note Off did not release the revived held string");
}

void testHeldTremoloPickingGesture()
{
    constexpr double sampleRate = 48000.0;
    constexpr int heldNote = ElectryEngine::lowestPlayableNote;
    constexpr int framesPerStroke = 4000; // 12 strokes/s at 48 kHz

    expect(ElectryEngine::tremoloGestureNote == 23
               && ElectryEngine::isTremoloGestureNote(23)
               && ! ElectryEngine::isPlayableNote(23),
           "the momentary tremolo gesture drifted from MIDI B0");

    EngineParameters parameters;
    parameters.sympatheticAmount = 0.0f;
    parameters.strumSpreadSeconds = 0.0f;
    parameters.tremoloRateHz = 12.0f;

    auto engineStorage = std::make_unique<ElectryEngine>();
    auto& engine = *engineStorage;
    engine.prepare(sampleRate, 512);
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    engine.noteOn(heldNote, 0.82f);
    StereoBuffer aged(128);
    renderInto(engine, aged);
    const auto before = TestAccess::snapshot(engine, 0);

    engine.beginTremoloPicking(0.93f);
    StereoBuffer immediate(1);
    renderInto(engine, immediate);
    const auto first = TestAccess::snapshot(engine, 0);
    expect(before.active && ! before.strokeIsUp
               && first.startOrder > before.startOrder && first.strokeIsUp,
           "B0 did not immediately repick the held string through Alternate");
    expect(std::abs(TestAccess::tremoloPickingVelocity(engine) - 0.93f)
               < 1.0e-6f,
           "the tremolo gesture did not retain its velocity as pick force");

    StereoBuffer beforeBoundary(framesPerStroke - 1);
    renderInto(engine, beforeBoundary);
    const auto waiting = TestAccess::snapshot(engine, 0);
    expect(waiting.startOrder == first.startOrder && waiting.strokeIsUp,
           "12-strokes/s tremolo repeated before its 4000-frame boundary");

    StereoBuffer onBoundary(1);
    renderInto(engine, onBoundary);
    const auto second = TestAccess::snapshot(engine, 0);
    expect(second.startOrder > first.startOrder && ! second.strokeIsUp,
           "12-strokes/s tremolo missed its sample-accurate second contact");

    engine.endTremoloPicking();
    const auto stoppedOrder = second.startOrder;
    StereoBuffer stopped(framesPerStroke + 256);
    renderInto(engine, stopped);
    expect(TestAccess::snapshot(engine, 0).startOrder == stoppedOrder
               && TestAccess::tremoloPickingVelocity(engine) == 0.0f
               && TestAccess::snapshot(engine, 0).keyDown,
           "releasing B0 either left picking armed or released the fretting key");

    // A note arriving at the same MIDI boundary is already the first attack.
    // The armed wrist must consume that contact instead of doubling it.
    auto simultaneousStorage = std::make_unique<ElectryEngine>();
    auto& simultaneous = *simultaneousStorage;
    simultaneous.prepare(sampleRate, 512);
    simultaneous.setParameters(parameters);
    simultaneous.reset();
    simultaneous.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    simultaneous.beginTremoloPicking(0.9f);
    simultaneous.noteOn(heldNote, 0.82f);
    const auto normalStart = TestAccess::snapshot(simultaneous, 0);
    StereoBuffer firstFrame(1);
    renderInto(simultaneous, firstFrame);
    const auto afterFirstFrame = TestAccess::snapshot(simultaneous, 0);
    expect(normalStart.startOrder == afterFirstFrame.startOrder
               && ! afterFirstFrame.strokeIsUp,
           "same-boundary B0 doubled a newly fretted note");

    // An empty armed wrist may be anywhere in its cycle when the fretting hand
    // enters. The played note is the new first contact, so the old empty phase
    // must not create a near-immediate flam after it.
    auto preArmedStorage = std::make_unique<ElectryEngine>();
    auto& preArmed = *preArmedStorage;
    preArmed.prepare(sampleRate, 512);
    preArmed.setParameters(parameters);
    preArmed.reset();
    preArmed.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    preArmed.beginTremoloPicking(0.9f);
    StereoBuffer emptyWrist(framesPerStroke - 50);
    renderInto(preArmed, emptyWrist);
    preArmed.noteOn(heldNote, 0.82f);
    const auto preArmedFirst = TestAccess::snapshot(preArmed, 0);
    StereoBuffer fullInterval(framesPerStroke);
    renderInto(preArmed, fullInterval);
    expect(TestAccess::snapshot(preArmed, 0).startOrder
               == preArmedFirst.startOrder,
           "pre-held B0 repeated from its empty phase instead of the note");
    StereoBuffer preArmedBoundary(1);
    renderInto(preArmed, preArmedBoundary);
    const auto preArmedSecond = TestAccess::snapshot(preArmed, 0);
    expect(preArmedSecond.startOrder > preArmedFirst.startOrder
               && ! preArmedFirst.strokeIsUp && preArmedSecond.strokeIsUp,
           "pre-held B0 missed the full interval after the played contact");

    // With no physically held key the gesture is silent and allocates no
    // voice. This also proves that velocity is not being treated as a pitch.
    auto emptyStorage = std::make_unique<ElectryEngine>();
    auto& empty = *emptyStorage;
    empty.prepare(sampleRate, 512);
    empty.setParameters(parameters);
    empty.reset();
    empty.beginTremoloPicking(1.0f);
    StereoBuffer silence(static_cast<int>(0.12 * sampleRate));
    renderInto(empty, silence);
    expect(empty.getActiveVoiceCount() == 0 && peakAbs(silence.left) == 0.0f,
           "B0 tremolo produced sound without a physically held string");
    empty.beginTremoloPicking(std::numeric_limits<float>::quiet_NaN());
    expect(TestAccess::tremoloPickingVelocity(empty) == 0.0f
               && TestAccess::tremoloPickingPhase(empty) == 0.0,
           "non-finite tremolo force did not fold to a stopped gesture");

    // The phase runs on the internal clock, but its observable interval is a
    // host-time quantity at every supported oversampling mode.
    for (const double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        auto timedStorage = std::make_unique<ElectryEngine>();
        auto& timed = *timedStorage;
        timed.prepare(rate, 257);
        auto timedParameters = parameters;
        timedParameters.tremoloRateHz = 10.0f;
        timed.setParameters(timedParameters);
        timed.reset();
        timed.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
        timed.noteOn(heldNote, 0.82f);
        StereoBuffer preRoll(7);
        renderInto(timed, preRoll, 3);
        timed.beginTremoloPicking(0.9f);
        StereoBuffer firstContact(1);
        renderInto(timed, firstContact);
        const auto firstTimed = TestAccess::snapshot(timed, 0);
        const int interval = static_cast<int>(rate / 10.0);
        StereoBuffer beforeTimed(interval - 1);
        renderInto(timed, beforeTimed, 113);
        expect(TestAccess::snapshot(timed, 0).startOrder
                   == firstTimed.startOrder,
               "10-strokes/s tremolo repeated early at host rate "
                   + std::to_string(rate));
        StereoBuffer timedBoundary(1);
        renderInto(timed, timedBoundary);
        expect(TestAccess::snapshot(timed, 0).startOrder
                   > firstTimed.startOrder,
               "10-strokes/s tremolo missed its boundary at host rate "
                   + std::to_string(rate));
    }

    // Rendering in arbitrary host blocks cannot move a wrist contact.
    const auto prepareBlocked = [&] (ElectryEngine& blocked)
    {
        blocked.prepare(sampleRate, 512);
        blocked.setParameters(parameters);
        blocked.reset();
        blocked.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
        blocked.noteOn(heldNote, 0.82f);
        StereoBuffer preRoll(97);
        renderInto(blocked, preRoll, 31);
        blocked.beginTremoloPicking(0.9f);
    };
    auto oneBlockStorage = std::make_unique<ElectryEngine>();
    auto irregularBlocksStorage = std::make_unique<ElectryEngine>();
    auto& oneBlock = *oneBlockStorage;
    auto& irregularBlocks = *irregularBlocksStorage;
    prepareBlocked(oneBlock);
    prepareBlocked(irregularBlocks);
    StereoBuffer contiguous(static_cast<int>(0.27 * sampleRate));
    StereoBuffer fragmented(contiguous.size());
    renderInto(oneBlock, contiguous, contiguous.size());
    renderInto(irregularBlocks, fragmented, 37);
    expect(contiguous.left == fragmented.left
               && contiguous.right == fragmented.right
               && TestAccess::snapshot(oneBlock, 0).startOrder
                      == TestAccess::snapshot(irregularBlocks, 0).startOrder,
           "tremolo timing or audio changed with host block partitioning");

    // Two strings belong to one wrist stroke: they share direction and move
    // Alternate only once per tick.
    auto chordStorage = std::make_unique<ElectryEngine>();
    auto& chord = *chordStorage;
    chord.prepare(sampleRate, 512);
    chord.setParameters(parameters);
    chord.reset();
    chord.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    const std::array<ElectryEngine::NoteOnEvent, 2> chordNotes {{
        { 28, 0.82f }, { 35, 0.82f }
    }};
    chord.noteOnChord(chordNotes);
    StereoBuffer chordAge(64);
    renderInto(chord, chordAge);
    chord.beginTremoloPicking(0.9f);
    StereoBuffer chordFirst(1);
    renderInto(chord, chordFirst);
    const auto lowUp = TestAccess::snapshot(chord, 0);
    const auto nextUp = TestAccess::snapshot(chord, 1);
    StereoBuffer chordWait(framesPerStroke - 1);
    renderInto(chord, chordWait);
    StereoBuffer chordSecond(1);
    renderInto(chord, chordSecond);
    const auto lowDown = TestAccess::snapshot(chord, 0);
    const auto nextDown = TestAccess::snapshot(chord, 1);
    expect(lowUp.strokeIsUp && nextUp.strokeIsUp
               && ! lowDown.strokeIsUp && ! nextDown.strokeIsUp,
           "poly tremolo used separate wrist clocks or over-consumed Alternate");

    // A legato slide is only the fretting hand. Landing one exactly on B0's
    // grid must not swallow the wrist contact due on every other held string.
    auto slideBoundaryStorage = std::make_unique<ElectryEngine>();
    auto& slideBoundary = *slideBoundaryStorage;
    slideBoundary.prepare(sampleRate, 512);
    slideBoundary.setParameters(parameters);
    slideBoundary.reset();
    slideBoundary.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    slideBoundary.noteOnChord(chordNotes);
    StereoBuffer slideAge(64);
    renderInto(slideBoundary, slideAge);
    slideBoundary.beginTremoloPicking(0.9f);
    StereoBuffer slideFirst(1);
    renderInto(slideBoundary, slideFirst);
    const auto slideFirstHigh = TestAccess::snapshot(slideBoundary, 1);
    StereoBuffer untilSlideBoundary(framesPerStroke - 1);
    renderInto(slideBoundary, untilSlideBoundary);
    slideBoundary.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
    const std::array<ElectryEngine::NoteOnEvent, 1> slideNote {{
        { 30, 0.82f }
    }};
    slideBoundary.noteOnChord(slideNote);
    expect(TestAccess::snapshot(slideBoundary, 0).midiNote == 30,
           "same-grid Slide fixture did not retarget the low string");
    StereoBuffer slideGridContact(1);
    renderInto(slideBoundary, slideGridContact);
    const auto afterSlideHigh = TestAccess::snapshot(slideBoundary, 1);
    expect(slideFirstHigh.strokeIsUp
               && afterSlideHigh.startOrder > slideFirstHigh.startOrder
               && ! afterSlideHigh.strokeIsUp,
           "a same-grid legato Slide swallowed B0 on another held string");

    // At the widest Strum setting a two-string traversal lasts 60 ms, longer
    // than a 20-strokes/s grid interval. The 50 ms tick is deliberately
    // skipped instead of overwriting the high string's pending contact; the
    // following 100 ms tick resumes once the pick has crossed it.
    auto wideChordStorage = std::make_unique<ElectryEngine>();
    auto& wideChord = *wideChordStorage;
    auto wideParameters = parameters;
    wideParameters.strumSpreadSeconds = 0.040f;
    wideParameters.tremoloRateHz = 20.0f;
    wideChord.prepare(sampleRate, 512);
    wideChord.setParameters(wideParameters);
    wideChord.reset();
    wideChord.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    wideChord.noteOnChord(chordNotes);
    StereoBuffer finishInitialTraversal(static_cast<int>(0.08 * sampleRate));
    renderInto(wideChord, finishInitialTraversal);
    wideChord.beginTremoloPicking(0.9f);
    StereoBuffer startWide(1);
    renderInto(wideChord, startWide);
    const auto firstWideSequence = TestAccess::noteSequence(wideChord);
    StereoBuffer acrossSkippedTick(static_cast<int>(0.055 * sampleRate));
    renderInto(wideChord, acrossSkippedTick);
    expect(TestAccess::noteSequence(wideChord) == firstWideSequence,
           "wide-chord tremolo replaced a pick contact still in flight");
    StereoBuffer throughNextTick(static_cast<int>(0.050 * sampleRate));
    renderInto(wideChord, throughNextTick);
    expect(TestAccess::noteSequence(wideChord) == firstWideSequence + 2,
           "wide-chord tremolo did not resume after its traversal completed");

    // B0's own delayed contacts must not restart its clock. A non-integer rate
    // exposes both an accidental extra 20 ms scalar pre-roll and lost
    // fractional remainder in successive contact intervals.
    auto spreadGridStorage = std::make_unique<ElectryEngine>();
    auto& spreadGrid = *spreadGridStorage;
    auto spreadGridParameters = parameters;
    spreadGridParameters.strumSpreadSeconds = 0.004f;
    spreadGridParameters.tremoloRateHz = 13.0f;
    spreadGrid.prepare(sampleRate, 512);
    spreadGrid.setParameters(spreadGridParameters);
    spreadGrid.reset();
    spreadGrid.noteOn(heldNote, 0.82f);
    StereoBuffer settleSpread(static_cast<int>(0.05 * sampleRate));
    renderInto(spreadGrid, settleSpread);
    spreadGrid.beginTremoloPicking(0.9f);
    std::array<int, 3> contactFrames {};
    int contactCount = 0;
    auto previousOrder = TestAccess::snapshot(spreadGrid, 0).startOrder;
    StereoBuffer oneFrame(1);
    for (int frame = 0; frame < 12000 && contactCount < 3; ++frame)
    {
        renderInto(spreadGrid, oneFrame);
        const auto order = TestAccess::snapshot(spreadGrid, 0).startOrder;
        if (order != previousOrder)
        {
            previousOrder = order;
            contactFrames[static_cast<std::size_t>(contactCount++)] = frame;
        }
    }
    expect(contactCount == 3,
           "spread B0 fixture did not produce three automatic contacts");
    const double expectedSpreadInterval = sampleRate / 13.0;
    for (int contact = 1; contact < contactCount; ++contact)
        expect(std::abs(static_cast<double>(contactFrames[contact]
                                            - contactFrames[contact - 1])
                        - expectedSpreadInterval) <= 1.0,
               "automatic B0 contact restarted its fractional wrist clock");

    // A sustain-pedal tail is sounding but no longer physically fingered; B0
    // must leave it untouched, just like the one-shot per-string commands.
    auto sustainOnlyStorage = std::make_unique<ElectryEngine>();
    auto& sustainOnly = *sustainOnlyStorage;
    sustainOnly.prepare(sampleRate, 512);
    sustainOnly.setParameters(parameters);
    sustainOnly.reset();
    sustainOnly.noteOn(heldNote, 0.82f);
    sustainOnly.setSustainPedal(true);
    sustainOnly.noteOff(heldNote);
    const auto sustainOrder = TestAccess::snapshot(sustainOnly, 0).startOrder;
    sustainOnly.beginTremoloPicking(0.9f);
    StereoBuffer sustainProbe(framesPerStroke + 8);
    renderInto(sustainOnly, sustainProbe);
    expect(TestAccess::snapshot(sustainOnly, 0).startOrder == sustainOrder,
           "B0 repicked a sustain-only string without a physical key owner");
}

void testHammerLatchedRepicksUsePickingHand()
{
    constexpr double sampleRate = 48000.0;
    constexpr int heldNote = 47;

    EngineParameters parameters;
    parameters.sympatheticAmount = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.strumSpreadSeconds = 0.0f;

    const auto makeHeld = [&] (const EngineParameters& heldParameters)
    {
        auto engine = std::make_unique<ElectryEngine>();
        engine->prepare(sampleRate, 512);
        engine->setParameters(heldParameters);
        engine->reset();
        engine->noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
        engine->noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        engine->noteOn(heldNote, 0.82f);
        StereoBuffer establish(128);
        renderInto(*engine, establish);
        engine->noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
        return engine;
    };
    const auto sameFinger = [] (const auto& left, const auto& right)
    {
        return left.vibratoSeed == right.vibratoSeed
            && left.vibratoCycle == right.vibratoCycle
            && left.vibratoPhase == right.vibratoPhase
            && left.vibratoRateScale == right.vibratoRateScale
            && left.vibratoDepthScale == right.vibratoDepthScale
            && left.vibratoSemitones == right.vibratoSemitones;
    };

    // E6..B6 explicitly move the picking hand. Hammer may remain latched for
    // later fretting gestures, but it cannot turn that wrist into another tap.
    auto manual = makeHeld(parameters);
    const int stringIndex = TestAccess::stringForNote(*manual, heldNote);
    const auto beforeManual = TestAccess::snapshot(*manual, stringIndex);
    manual->noteOn(ElectryEngine::firstRepickNote + stringIndex, 0.93f);
    const auto manualUp = TestAccess::snapshot(*manual, stringIndex);
    manual->noteOn(ElectryEngine::firstRepickNote + stringIndex, 0.93f);
    const auto manualDown = TestAccess::snapshot(*manual, stringIndex);
    expect(stringIndex >= 0 && manualUp.startOrder > beforeManual.startOrder
               && manualUp.playStyle == PlayStyle::Sustain
               && manualUp.strokeIsUp && manualUp.excitationInContact
               && manualUp.contactFeedbackGain < 1.0f
               && manualDown.startOrder > manualUp.startOrder
               && ! manualDown.strokeIsUp && manualDown.excitationInContact,
           "Hammer latch turned a one-shot held-string repick into a finger tap");
    expect(manual->getCurrentPlayStyle() == PlayStyle::Hammer
               && sameFinger(manualDown, beforeManual),
           "one-shot Hammer repicks changed the latch or fretting finger");

    // Pick Noise is deliberately absent from an ordinary Hammer. It must be
    // audible here because the dedicated command is a real plectrum contact.
    const auto renderRepick = [&] (float pickNoise)
    {
        auto engine = makeHeld(parameters);
        auto changed = parameters;
        changed.pickNoise = pickNoise;
        engine->setParameters(changed);
        StereoBuffer settle(static_cast<int>(0.080 * sampleRate));
        renderInto(*engine, settle);
        const int heldString = TestAccess::stringForNote(*engine, heldNote);
        engine->noteOn(ElectryEngine::firstRepickNote + heldString, 0.93f);
        StereoBuffer result(static_cast<int>(0.050 * sampleRate));
        renderInto(*engine, result);
        return result;
    };
    const auto quietPick = renderRepick(0.0f);
    const auto noisyPick = renderRepick(1.0f);
    expect(normalisedDifferenceRms(quietPick.left, noisyPick.left, 0,
                                   quietPick.size()) > 0.001,
           "Pick Noise remained inaudible on a Hammer-latched repick");

    // B0 reaches the same path on the next sample and likewise preserves the
    // held finger while Alternate supplies the wrist direction.
    auto automatic = makeHeld(parameters);
    const int automaticString = TestAccess::stringForNote(*automatic, heldNote);
    const auto beforeAutomatic = TestAccess::snapshot(*automatic,
                                                       automaticString);
    automatic->beginTremoloPicking(0.93f);
    StereoBuffer automaticContact(1);
    renderInto(*automatic, automaticContact);
    automatic->endTremoloPicking();
    const auto afterAutomatic = TestAccess::snapshot(*automatic,
                                                      automaticString);
    expect(afterAutomatic.startOrder > beforeAutomatic.startOrder
               && afterAutomatic.playStyle == PlayStyle::Sustain
               && afterAutomatic.strokeIsUp
               && afterAutomatic.excitationInContact
               && afterAutomatic.contactFeedbackGain < 1.0f
               && automatic->getCurrentPlayStyle() == PlayStyle::Hammer
               && sameFinger(afterAutomatic, beforeAutomatic),
           "Hammer latch turned B0 into a finger-tap clock");

    // On a held shape the same correction must enter the shared Strum
    // scheduler, not create simultaneous per-string taps.
    auto strummedParameters = parameters;
    strummedParameters.strumSpreadSeconds = 0.003f;
    ElectryEngine strummed;
    strummed.prepare(sampleRate, 512);
    strummed.setParameters(strummedParameters);
    strummed.reset();
    strummed.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    const std::array<ElectryEngine::NoteOnEvent, 2> notes {{
        { 47, 0.82f }, { 52, 0.82f }
    }};
    strummed.noteOnChord(notes);
    StereoBuffer establishChord(static_cast<int>(0.080 * sampleRate));
    renderInto(strummed, establishChord);
    const int lowString = TestAccess::stringForNote(strummed, 47);
    const int highString = TestAccess::stringForNote(strummed, 52);
    const auto lowFinger = TestAccess::snapshot(strummed, lowString);
    strummed.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
    strummed.beginTremoloPicking(0.90f);
    StereoBuffer scheduleChord(1);
    renderInto(strummed, scheduleChord);
    strummed.endTremoloPicking();
    const auto lowPending = TestAccess::snapshot(strummed, lowString);
    const auto highPending = TestAccess::snapshot(strummed, highString);
    expect(lowPending.pendingRepickActive && highPending.pendingRepickActive
               && lowPending.pendingPlayStyle == PlayStyle::Sustain
               && highPending.pendingPlayStyle == PlayStyle::Sustain
               && lowPending.pendingStrokeIsUp && highPending.pendingStrokeIsUp
               && lowPending.pendingStrokeVariationState
                      == highPending.pendingStrokeVariationState
               && lowPending.startDelaySamples > 0
               && highPending.startDelaySamples > 0
               && lowPending.startDelaySamples != highPending.startDelaySamples
               && sameFinger(lowPending, lowFinger),
           "Hammer-latched B0 did not schedule one shared travelling pick");
}

void testAlternateStrokeSequence()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);

    const auto advanceToNextStroke = [&]
    {
        StereoBuffer gap(static_cast<int>(0.050 * sampleRate));
        renderInto(engine, gap);
    };

    expect(engine.getCurrentPickStyle() == PickStyle::Alternate,
           "Alternate did not latch");

    // Rejected performance events must not consume a stroke direction.
    engine.noteOn(100, 1.0f);
    engine.noteOn(40, 0.0f);

    // A delayed note released before the pick reaches it is rejected too. It
    // must not turn the next real Palm stroke into an upstroke when no physical
    // downstroke ever landed.
    {
        ElectryEngine cancelled;
        cancelled.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.strumSpreadSeconds = 0.040f;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        cancelled.setParameters(parameters);
        cancelled.reset();
        cancelled.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
        cancelled.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
        cancelled.noteOn(28, 0.90f);
        const int cancelledString = TestAccess::stringForNote(cancelled, 28);
        expect(TestAccess::snapshot(cancelled, cancelledString).startDelaySamples > 0,
               "cancelled Alternate fixture did not schedule a delayed stroke");
        cancelled.noteOff(28);
        StereoBuffer noStroke(static_cast<int>(0.050 * sampleRate));
        renderInto(cancelled, noStroke);
        cancelled.noteOn(40, 0.90f);
        const auto firstReal = TestAccess::snapshot(
            cancelled, TestAccess::stringForNote(cancelled, 40));
        expect(firstReal.valid && ! firstReal.strokeIsUp,
               "a cancelled pre-contact Palm note consumed an Alternate stroke");
    }

    // A hammer or slide can move the fretting finger while a reserved pick is
    // still travelling. Cancelling one moved finger must not return the shared
    // Alternate direction while another member's original pick is still due.
    {
        ElectryEngine partiallyCancelled;
        partiallyCancelled.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.strumSpreadSeconds = 0.040f;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        partiallyCancelled.setParameters(parameters);
        partiallyCancelled.reset();
        partiallyCancelled.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
        partiallyCancelled.noteOn(35, 0.90f);
        partiallyCancelled.noteOn(40, 0.90f);
        partiallyCancelled.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
        partiallyCancelled.noteOn(37, 0.90f);
        partiallyCancelled.noteOn(42, 0.90f);

        const auto movedLow = TestAccess::snapshot(partiallyCancelled, 1);
        const auto movedHigh = TestAccess::snapshot(partiallyCancelled, 2);
        expect(movedLow.startDelaySamples > 0 && movedHigh.startDelaySamples > 0
                   && movedLow.pendingRepickActive
                   && movedHigh.pendingRepickActive
                   && movedLow.pendingPlayStyle == PlayStyle::Sustain
                   && movedHigh.pendingPlayStyle == PlayStyle::Sustain,
               "moved-finger cancellation fixture lost its travelling picks");

        partiallyCancelled.noteOff(37);
        const int factor = TestAccess::oversamplingFactor(partiallyCancelled);
        StereoBuffer remainingContact(
            (movedHigh.startDelaySamples - 1) / factor + 1);
        renderInto(partiallyCancelled, remainingContact);
        const auto contactedHigh = TestAccess::snapshot(partiallyCancelled, 2);
        expect(contactedHigh.startDelaySamples == 0
                   && ! contactedHigh.pendingRepickActive
                   && contactedHigh.excitationInContact,
               "surviving moved-finger pick did not reach the string");
        partiallyCancelled.noteOff(42);
        StereoBuffer nextStrokeGap(static_cast<int>(0.050 * sampleRate));
        renderInto(partiallyCancelled, nextStrokeGap);
        partiallyCancelled.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        partiallyCancelled.noteOn(45, 0.90f);
        const auto nextStroke = TestAccess::snapshot(
            partiallyCancelled,
            TestAccess::stringForNote(partiallyCancelled, 45));
        expect(nextStroke.valid && nextStroke.strokeIsUp,
               "cancelling one moved finger returned another travelling "
               "pick's Alternate stroke");
    }

    engine.noteOn(40, 0.8f);
    const auto first = TestAccess::snapshot(engine,
                                            TestAccess::stringForNote(engine, 40));
    advanceToNextStroke();
    engine.noteOn(45, 0.8f);
    const auto second = TestAccess::snapshot(engine,
                                             TestAccess::stringForNote(engine, 45));
    advanceToNextStroke();
    engine.noteOn(50, 0.8f);
    const auto third = TestAccess::snapshot(engine,
                                            TestAccess::stringForNote(engine, 50));

    expect(first.valid && ! first.strokeIsUp,
           "Alternate did not begin with a downstroke");
    expect(second.valid && second.strokeIsUp,
           "Alternate did not alternate to an upstroke");
    expect(third.valid && ! third.strokeIsUp,
           "Alternate did not alternate back to a downstroke");
    expect(engine.getCurrentPickStyle() == PickStyle::Alternate,
           "resolved strokes replaced the latched Alternate picking style");

    // Alternate picking composes with any play style: the palm-muted phrase
    // keeps alternating.
    advanceToNextStroke();
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(55, 0.8f);
    const auto mutedUp = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 55));
    expect(mutedUp.valid && mutedUp.playStyle == PlayStyle::PalmMute
               && mutedUp.strokeIsUp,
           "Alternate did not continue through a play-style change");

    // A hammered note has no pick, so it neither takes a stroke nor consumes
    // one: the sequence resumes where it left off.
    advanceToNextStroke();
    engine.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
    engine.noteOn(57, 0.8f);
    const auto hammered = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 57));
    expect(hammered.valid && hammered.playStyle == PlayStyle::Hammer
               && ! hammered.strokeIsUp,
           "a hammered note took a pick stroke");
    advanceToNextStroke();
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(59, 0.8f);
    const auto resumed = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 59));
    expect(resumed.valid && ! resumed.strokeIsUp,
           "a hammered note consumed a stroke from the alternate sequence");

    // Pressing the keyswitch again begins a fresh phrase on a downstroke.
    // The pending stroke here is an UPstroke (down/up/down/up have been
    // consumed and the hammered note took none), so a reset that failed to
    // clear the phase would render an upstroke and fail this check.
    advanceToNextStroke();
    engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    engine.noteOn(64, 0.8f);
    const auto restarted = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 64));
    expect(restarted.valid && ! restarted.strokeIsUp,
           "reselecting Alternate did not reset its phase");
}

void testAlternateChordSharesOneStroke()
{
    // One wrist stroke crosses every string of a chord. Alternate chooses the
    // direction once for that stroke; alternating again at each note-on makes
    // one physical strum internally read down/up/down/up even while its travel
    // still moves in only one direction.
    constexpr double sampleRate = 48000.0;
    constexpr std::array<int, 4> chord { 28, 35, 40, 45 };

    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.strumSpreadSeconds = 0.012f;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);

    const auto expectChordStroke = [&] (bool expectedUp, const char* label)
    {
        for (const int note : chord)
            engine.noteOn(note, 0.90f);
        for (int stringIndex = 0; stringIndex < 4; ++stringIndex)
        {
            const auto voice = TestAccess::snapshot(engine, stringIndex);
            const auto effectiveStyle = voice.pendingRepickActive
                ? voice.pendingPlayStyle : voice.playStyle;
            const bool effectiveStrokeIsUp = voice.pendingRepickActive
                ? voice.pendingStrokeIsUp : voice.strokeIsUp;
            expect(voice.valid && effectiveStyle == PlayStyle::PalmMute
                       && effectiveStrokeIsUp == expectedUp,
                   std::string("alternate palm chord did not share its ")
                       + label + " stroke on string "
                       + std::to_string(stringIndex));
        }
    };

    expectChordStroke(false, "down");
    StereoBuffer first(static_cast<int>(0.20 * sampleRate));
    renderInto(engine, first);
    for (const int note : chord)
        engine.noteOff(note);
    StereoBuffer gap(static_cast<int>(0.06 * sampleRate));
    renderInto(engine, gap);
    expectChordStroke(true, "up");

    // The chord window is measured from the first note, not rolled forward by
    // every arrival. Five unique strings ten milliseconds apart span 40 ms, so
    // the fifth is a new Alternate stroke even though every adjacent gap is
    // shorter than the 35 ms assembly window.
    ElectryEngine bounded;
    bounded.prepare(sampleRate, 512);
    bounded.setParameters(parameters);
    bounded.reset();
    bounded.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    bounded.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    constexpr std::array<int, 5> staircase { 28, 35, 40, 45, 50 };
    for (std::size_t index = 0; index < staircase.size(); ++index)
    {
        bounded.noteOn(staircase[index], 0.90f);
        if (index + 1 < staircase.size())
        {
            StereoBuffer tenMilliseconds(static_cast<int>(0.010 * sampleRate));
            renderInto(bounded, tenMilliseconds);
        }
    }
    const auto fifth = TestAccess::snapshot(
        bounded, TestAccess::stringForNote(bounded, staircase.back()));
    expect(fifth.valid && fifth.strokeIsUp,
           "the 35 ms chord window rolled past 40 ms of cross-string Palm notes");
}

void testChordSharesPickingHandVariation()
{
    // Pick position, force, angle and contact width belong to the wrist stroke,
    // not independently to each string it crosses. The downstream excitation
    // remains per-string; this checks only those four latent hand coordinates.
    constexpr double sampleRate = 48000.0;
    constexpr std::array<int, 3> notes { 28, 35, 40 };
    constexpr std::array<ElectryEngine::NoteOnEvent, notes.size()> events {{
        { 28, 0.85f }, { 35, 0.85f }, { 40, 0.85f }
    }};
    using Variation = std::array<float, 4>;
    const auto variation = [] (const TestAccess::VoiceSnapshot& voice)
    {
        return Variation {
            voice.strokeContactOffsetMetres,
            voice.strokeForceGain,
            voice.strokeAngleOffset,
            voice.strokeWidthScale
        };
    };
    const auto nearVariation = [] (const Variation& actual,
                                   const Variation& expected)
    {
        for (std::size_t index = 0; index < actual.size(); ++index)
            if (std::abs(actual[index] - expected[index]) > 1.0e-6f)
                return false;
        return true;
    };

    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.strumSpreadSeconds = 0.010f;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    engine.setParameters(parameters);
    engine.reset();

    engine.noteOnChord(events);
    const Variation firstStroke = variation(TestAccess::snapshot(engine, 0));
    std::uint64_t previousOrder = 0;
    for (int stringIndex = 0; stringIndex < 3; ++stringIndex)
    {
        const auto voice = TestAccess::snapshot(engine, stringIndex);
        expect(variation(voice) == firstStroke,
               "a complete chord drew a different picking hand per string");
        expect(voice.startOrder > previousOrder,
               "sharing one picking hand collapsed per-note ordering");
        previousOrder = voice.startOrder;
    }

    // The scalar fallback discovers its edge causally, but it is the same hand
    // and the first event must retain the exact legacy single-note draw.
    engine.reset();
    for (const int note : notes)
        engine.noteOn(note, 0.85f);
    for (int stringIndex = 0; stringIndex < 3; ++stringIndex)
        expect(variation(TestAccess::snapshot(engine, stringIndex))
                   == firstStroke,
               "a scalar chord did not share the complete chord's hand");

    // Up enters from the opposite physical edge, so its solved anchor is
    // string 2 while the first canonical event remains string 0. Complete and
    // causally re-anchored scalar paths must both retain that event's old draw.
    engine.reset();
    engine.noteOn(pickKeyswitch(PickStyle::Up), 1.0f);
    engine.noteOnChord(events);
    for (int stringIndex = 0; stringIndex < 3; ++stringIndex)
        expect(variation(TestAccess::snapshot(engine, stringIndex))
                   == firstStroke,
               "an up-strum seeded its hand from the solved chord anchor");
    engine.reset();
    engine.noteOn(pickKeyswitch(PickStyle::Up), 1.0f);
    for (const int note : notes)
        engine.noteOn(note, 0.85f);
    for (int stringIndex = 0; stringIndex < 3; ++stringIndex)
        expect(variation(TestAccess::snapshot(engine, stringIndex))
                   == firstStroke,
               "a scalar up-strum changed its hand while re-anchoring");

    // A later traversal is a new wrist motion, shared within itself but not a
    // frozen gesture repeated forever.
    StereoBuffer finishFirst(static_cast<int>(0.05 * sampleRate));
    renderInto(engine, finishFirst);
    engine.noteOnChord(events);
    StereoBuffer finishSecond(static_cast<int>(0.05 * sampleRate));
    renderInto(engine, finishSecond);
    const Variation secondStroke = variation(TestAccess::snapshot(engine, 0));
    expect(secondStroke != firstStroke,
           "successive chord strokes reused one picking-hand variation");
    for (int stringIndex = 1; stringIndex < 3; ++stringIndex)
        expect(variation(TestAccess::snapshot(engine, stringIndex))
                   == secondStroke,
               "a repeated chord stopped sharing its picking hand");

    // A same-note member commits only when the travelling pick reaches it. A
    // newer chord may start first, so the pending contact must carry stroke A
    // instead of reading the engine's now-current stroke B at commit time.
    parameters.strumSpreadSeconds = 0.040f;
    engine.setParameters(parameters);
    engine.reset();
    constexpr std::array<ElectryEngine::NoteOnEvent, 2> heldEvents {{
        { 28, 0.85f }, { 35, 0.85f }
    }};
    engine.noteOnChord(heldEvents);
    StereoBuffer settleHeld(static_cast<int>(0.08 * sampleRate));
    renderInto(engine, settleHeld);
    engine.noteOnChord(heldEvents);
    const Variation pendingStroke = variation(TestAccess::snapshot(engine, 0));
    expect(TestAccess::snapshot(engine, 1).pendingRepickActive,
           "pending-stroke fixture did not delay its second string");
    constexpr std::array<ElectryEngine::NoteOnEvent, 1> newerEvent {{
        { 40, 0.85f }
    }};
    engine.noteOnChord(newerEvent);
    const Variation newerStroke = variation(TestAccess::snapshot(engine, 2));
    expect(newerStroke != pendingStroke,
           "pending-stroke fixture did not advance to a distinct hand");
    StereoBuffer commitPending(static_cast<int>(0.08 * sampleRate));
    renderInto(engine, commitPending);
    expect(variation(TestAccess::snapshot(engine, 1)) == pendingStroke,
           "a delayed string adopted a newer chord's picking hand");

    // Pin the old single-note states as well as their relational behaviour.
    // These exact values are the default-seed draws before chord sharing.
    engine.setParameters(EngineParameters {});
    engine.reset();
    engine.noteOn(40, 0.85f);
    expect(nearVariation(
               variation(TestAccess::snapshot(engine, 2)), Variation {
                   -0.0045802216f, 0.992183566f, -0.0217061788f, 0.972075164f
               }),
           "the first legacy single-note picking-hand draw changed");
    engine.noteOn(40, 0.85f);
    expect(nearVariation(
               variation(TestAccess::snapshot(engine, 2)), Variation {
                   -0.000900611922f, 0.96214819f, 0.124393389f, 1.00315654f
               }),
           "the second legacy single-note picking-hand draw changed");

    // Double's second player starts from a nonzero deterministic seed. It gets
    // a different hand, but the same one-stroke ownership and solo identity.
    engine.setVariationSeed(0x9e3779b9u);
    engine.reset();
    engine.noteOnChord(events);
    const Variation seededStroke = variation(TestAccess::snapshot(engine, 0));
    expect(seededStroke != firstStroke,
           "a separately seeded player reused the primary picking hand");
    for (int stringIndex = 1; stringIndex < 3; ++stringIndex)
        expect(variation(TestAccess::snapshot(engine, stringIndex))
                   == seededStroke,
               "a separately seeded chord did not share one picking hand");
    engine.reset();
    engine.noteOn(28, 0.85f);
    expect(variation(TestAccess::snapshot(engine, 0)) == seededStroke,
           "chord sharing changed a separately seeded solo note");
}

void testRapidSameStringRepicksAreSeparateStrokes()
{
    // The chord window groups different strings crossed by one wrist motion.
    // It must not merge a new attack on a string the current chord already
    // crossed: 25 ms tremolo picking is fast, but it is still three strokes.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();
    engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);

    const auto repick = [&]
    {
        engine.noteOn(28, 0.90f);
        return TestAccess::snapshot(engine,
                                    TestAccess::stringForNote(engine, 28));
    };
    const auto first = repick();
    StereoBuffer firstGap(static_cast<int>(0.025 * sampleRate));
    renderInto(engine, firstGap);
    const auto second = repick();
    StereoBuffer secondGap(static_cast<int>(0.025 * sampleRate));
    renderInto(engine, secondGap);
    const auto third = repick();

    expect(first.valid && ! first.strokeIsUp,
           "rapid Alternate repicks did not begin down");
    expect(second.valid && second.strokeIsUp,
           "a 25 ms same-string repick was merged into the preceding chord");
    expect(third.valid && ! third.strokeIsUp,
           "rapid same-string repicks did not alternate back down");
}

void testZeroSpreadAlternateRunChangesStrings()
{
    // With no programmed strum, only truly simultaneous note-ons are a block
    // chord. A rapid cross-string riff is still a sequence of wrist strokes;
    // merging its 25 ms events into the chord window turns Alternate into
    // repeated downstrokes exactly where a metal performance needs it most.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();
    engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);

    constexpr std::array<int, 3> notes {{ 28, 35, 40 }};
    std::array<bool, 3> strokes {};
    for (std::size_t i = 0; i < strokes.size(); ++i)
    {
        const int note = notes[i];
        engine.noteOn(note, 0.90f);
        strokes[i] = TestAccess::snapshot(
            engine, TestAccess::stringForNote(engine, note)).strokeIsUp;
        if (i + 1 < strokes.size())
        {
            StereoBuffer gap(static_cast<int>(0.025 * sampleRate));
            renderInto(engine, gap);
        }
    }
    expect(! strokes[0] && strokes[1] && ! strokes[2],
           "zero-spread cross-string Palm run did not alternate down/up/down");

    // Same-sample note-ons remain one physical block stroke at zero spread.
    engine.reset();
    engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    for (const int note : notes)
        engine.noteOn(note, 0.90f);
    for (int stringIndex = 0; stringIndex < 3; ++stringIndex)
        expect(! TestAccess::snapshot(engine, stringIndex).strokeIsUp,
               "same-sample zero-spread Palm chord split into multiple strokes");
}

void testArticulationsSoundDistinct()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    engine.setParameters(parameters);

    const int attackStart = static_cast<int>(0.002 * sampleRate);
    const int attackWindow = static_cast<int>(0.06 * sampleRate);

    const auto renderStyle = [&] (PlayStyle style,
                                  PickStyle pick = PickStyle::Down)
    {
        return renderNote(engine, sampleRate, 48, 0.85f, style, 1.2, -1.0,
                          pick);
    };

    const auto down = renderStyle(PlayStyle::Sustain);
    const auto up = renderStyle(PlayStyle::Sustain, PickStyle::Up);
    const auto hammer = renderStyle(PlayStyle::Hammer);
    const auto muted = renderStyle(PlayStyle::PalmMute);
    const auto harmonic = renderStyle(PlayStyle::Harmonics);

    const double f0 = midiHz(48);
    const double downCentroid = spectralCentroid(down.left, attackStart,
                                                 attackWindow, sampleRate, f0);
    const double upCentroid = spectralCentroid(up.left, attackStart,
                                               attackWindow, sampleRate, f0);
    const double hammerCentroid = spectralCentroid(hammer.left, attackStart,
                                                   attackWindow, sampleRate, f0);
    const double harmonicCentroid = spectralCentroid(
        harmonic.left, attackStart, attackWindow, sampleRate, f0);

    // The hammered attack is fingered, not picked: it must be darker than
    // both pick strokes. The harmonic's node touch removes the low modes, so
    // its attack sits clearly brighter than the fretted downstroke.
    //
    // The margin is 0.95 rather than 0.9 because the picked attack's own
    // spectrum is now calibrated against a dry electric low-E reference
    // recording, and is darker than it used to be. The ordering these checks
    // exist to pin is unchanged; the absolute gap between a fingered and a
    // picked attack is simply smaller than it was against the previous,
    // brighter picked voicing.
    expect(hammerCentroid < downCentroid * 0.95,
           "hammer-on attack is not darker than a downstroke (down "
               + std::to_string(downCentroid) + " Hz, hammer "
               + std::to_string(hammerCentroid) + " Hz)");
    expect(hammerCentroid < upCentroid * 0.9,
           "hammer-on attack is not darker than an upstroke (up "
               + std::to_string(upCentroid) + " Hz, hammer "
               + std::to_string(hammerCentroid) + " Hz)");
    expect(harmonicCentroid > downCentroid * 1.05,
           "harmonic attack is not brighter than a downstroke (down "
               + std::to_string(downCentroid) + " Hz, harmonic "
               + std::to_string(harmonicCentroid) + " Hz)");
    expect(upCentroid > downCentroid * 1.01,
           "upstroke attack is not brighter than a downstroke (down "
               + std::to_string(downCentroid) + " Hz, up "
               + std::to_string(upCentroid) + " Hz)");

    // The hammered attack is also quieter than the picked one.
    const double downAttackRms = rmsInRange(down.left, attackStart,
                                            attackStart + attackWindow);
    const double hammerAttackRms = rmsInRange(hammer.left, attackStart,
                                              attackStart + attackWindow);
    expect(hammerAttackRms < downAttackRms * 0.85,
           "hammer-on attack is not softer than a downstroke (down "
               + std::to_string(downAttackRms) + ", hammer "
               + std::to_string(hammerAttackRms) + ")");

    // Palm muting kills the sustain: compare late energy.
    const int lateStart = static_cast<int>(0.8 * sampleRate);
    const int lateEnd = static_cast<int>(1.1 * sampleRate);
    const double downLate = rmsInRange(down.left, lateStart, lateEnd);
    const double mutedLate = rmsInRange(muted.left, lateStart, lateEnd);
    expect(mutedLate < downLate * 0.25,
           "muted notes do not decay dramatically faster than open notes");

    // Down and up strokes must not be identical renders.
    double difference = 0.0;
    double reference = 0.0;
    for (int i = 0; i < static_cast<int>(0.2 * sampleRate); ++i)
    {
        const double a = down.left[static_cast<std::size_t>(i)];
        const double b = up.left[static_cast<std::size_t>(i)];
        difference += (a - b) * (a - b);
        reference += a * a;
    }
    expect(difference > 0.01 * reference,
           "downstroke and upstroke render nearly identical audio");
}

void testStyleAndStrokeCombinations()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    engine.setParameters(parameters);

    // The stroke composes with every picked style. An upstroke palm mute is
    // still a mute - the decay is the style's - but it is a genuinely
    // different render with the upstroke's brighter, inverted contact.
    const auto mutedDown = renderNote(engine, sampleRate, 45, 0.85f,
                                      PlayStyle::PalmMute, 0.9);
    const auto mutedUp = renderNote(engine, sampleRate, 45, 0.85f,
                                    PlayStyle::PalmMute, 0.9, -1.0,
                                    PickStyle::Up);
    const double comboDifference = normalisedDifferenceRms(
        mutedDown.left, mutedUp.left, 0, static_cast<int>(0.2 * sampleRate));
    expect(comboDifference > 0.05,
           "up and down palm mutes render nearly identical audio ("
               + std::to_string(comboDifference) + ")");

    const int muteLateStart = static_cast<int>(0.36 * sampleRate);
    const int muteLateEnd = static_cast<int>(0.56 * sampleRate);
    const auto openDown = renderNote(engine, sampleRate, 45, 0.85f,
                                     PlayStyle::Sustain, 0.9);
    const double openLate = rmsInRange(openDown.left, muteLateStart, muteLateEnd);
    for (const auto* muted : { &mutedDown, &mutedUp })
    {
        const double mutedLate = rmsInRange(muted->left, muteLateStart,
                                            muteLateEnd);
        expect(mutedLate < openLate * 0.30,
               "a palm mute did not keep its damping under both strokes");
    }

    const int attackStart = static_cast<int>(0.002 * sampleRate);
    const int attackWindow = static_cast<int>(0.06 * sampleRate);
    const double downMuteCentroid = spectralCentroid(
        mutedDown.left, attackStart, attackWindow, sampleRate, midiHz(45));
    const double upMuteCentroid = spectralCentroid(
        mutedUp.left, attackStart, attackWindow, sampleRate, midiHz(45));
    expect(upMuteCentroid > downMuteCentroid * 1.01,
           "an upstroke palm mute is not brighter than a downstroke one (down "
               + std::to_string(downMuteCentroid) + " Hz, up "
               + std::to_string(upMuteCentroid) + " Hz)");

    // The harmonic keeps its octave under either stroke.
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);
    const int harmonicStart = static_cast<int>(0.35 * sampleRate);
    const int harmonicWindow = static_cast<int>(0.55 * sampleRate);
    const double naturalExpected = midiHz(57);
    for (const auto pick : { PickStyle::Down, PickStyle::Up })
    {
        const auto natural = renderNote(engine, sampleRate, 45, 0.35f,
                                        PlayStyle::Harmonics, 1.1, -1.0, pick);
        const double naturalMeasured = measureFrequency(
            natural.left, harmonicStart, harmonicWindow, sampleRate,
            naturalExpected);
        expect(std::abs(centsBetween(naturalMeasured, naturalExpected)) < 10.0,
               "natural harmonic does not sound one octave above the played "
               "note under both strokes");
    }

    // A hammered note has no pick, so the latched picking style cannot change
    // it: the renders are bit-identical.
    const auto hammerDown = renderNote(engine, sampleRate, 45, 0.85f,
                                       PlayStyle::Hammer, 0.6);
    const auto hammerUp = renderNote(engine, sampleRate, 45, 0.85f,
                                     PlayStyle::Hammer, 0.6, -1.0,
                                     PickStyle::Up);
    expect(hammerDown.left == hammerUp.left,
           "the latched picking style changed a hammered note");

    // Pick Position, Hardness and Noise describe a plectrum. Moving all three
    // from end to end must leave a fretting-hand tap exactly alone; an exact
    // render comparison catches level, pulse, comb and noise leaks together.
    const auto hammerAtPickControls = [&] (float value)
    {
        ElectryEngine fingered;
        fingered.prepare(sampleRate, 512);
        EngineParameters fingeredParameters;
        fingeredParameters.artifactAmount = 0.0f;
        fingeredParameters.bodyResonance = 0.0f;
        fingeredParameters.sympatheticAmount = 0.0f;
        fingeredParameters.pickPosition = value;
        fingeredParameters.pickHardness = value;
        fingeredParameters.pickNoise = value;
        fingeredParameters.fingerNoise = 0.55f;
        fingeredParameters.releaseNoise = 0.0f;
        fingered.setParameters(fingeredParameters);
        return renderNote(fingered, sampleRate, 45, 0.85f,
                          PlayStyle::Hammer, 0.6);
    };
    const auto hammerAtSoftPick = hammerAtPickControls(0.0f);
    const auto hammerAtHardPick = hammerAtPickControls(1.0f);
    expect(hammerAtSoftPick.left == hammerAtHardPick.left
               && hammerAtSoftPick.right == hammerAtHardPick.right,
           "plectrum controls changed a Hammer articulation with no plectrum");
}

/** A note played without a plectrum draws no picking-hand variation.

    Every physical plectrum stroke draws four numbers describing where the hand
    put the pick: how hard, how wide the contact patch, at what angle, and how
    far along the string. They are a pure function of the first note counter,
    so two otherwise identical solo notes at different points in a sequence get
    different draws - which is the point of them, on a picked note.

    A hammer-on is the fretting hand landing on the fingerboard and a legato
    slide is a finger already down that simply moves. The engine says so itself:
    both clear the plectrum's own contact terms. The draw used to be applied to
    them anyway, so repeated hammer-ons and slides carried a picking hand's
    spread of level, pulse length and comb position with no pick in the stroke.

    The two engines below differ only in that one has already played a note, so
    the note under test is drawn at a different point in the sequence. The
    picked case is asserted in the other direction, because an engine that had
    simply deleted the variation would satisfy the fingered clause alone. */
void testFingeredNotesDrawNoPickingHandVariation()
{
    constexpr double sampleRate = 48000.0;
    constexpr int hammered = 45;
    constexpr int earlier = 64;   // a different string, so the draw is all that moves

    // The state of the string under test after a note-on, at two different
    // points in the stroke sequence.
    const auto attackAt = [&] (PlayStyle style, bool afterAnotherNote)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.artifactAmount = 0.0f;
        parameters.strumSpreadSeconds = 0.0f;
        engine.setParameters(parameters);
        engine.reset();

        if (afterAnotherNote)
        {
            engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
            engine.noteOn(earlier, 0.85f);
        }
        // A slide is only legato once it lands on a string that is already
        // sounding; starting a phrase on the Slide keyswitch with nothing to
        // slide from is an ordinary pick stroke, and rightly draws a stroke.
        if (style == PlayStyle::Slide)
        {
            engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
            engine.noteOn(hammered - 2, 0.85f);
        }
        engine.noteOn(styleKeyswitch(style), 1.0f);
        engine.noteOn(hammered, 0.85f);

        TestAccess::VoiceSnapshot found;
        for (int string = 0; string < ElectryEngine::stringCount; ++string)
        {
            const auto voice = TestAccess::snapshot(engine, string);
            if (voice.active && voice.midiNote == hammered)
                found = voice;
        }
        return found;
    };

    for (const auto style : { PlayStyle::Hammer, PlayStyle::Slide })
    {
        const auto first = attackAt(style, false);
        const auto later = attackAt(style, true);
        const auto name = style == PlayStyle::Hammer ? std::string("a hammer-on")
                                                     : std::string("a slide");
        expect(first.valid && later.valid,
               name + " did not sound on the string it was played on");
        if (! (first.valid && later.valid))
            continue;

        expect(first.excitationAmplitude == later.excitationAmplitude,
               name + " changed level with the note counter, so a picking hand's "
                      "contact force is being drawn for a fingered note ("
                   + std::to_string(first.excitationAmplitude) + " against "
                   + std::to_string(later.excitationAmplitude) + ")");
        expect(first.excitationLength == later.excitationLength,
               name + " changed pulse length with the note counter, so a picking "
                      "hand's contact patch is being drawn for a fingered note ("
                   + std::to_string(first.excitationLength) + " against "
                   + std::to_string(later.excitationLength) + ")");
        const float firstCombFraction = first.excitationCombDelay
                                      / first.lastCompensatedPeriod;
        const float laterCombFraction = later.excitationCombDelay
                                      / later.lastCompensatedPeriod;
        expect(std::abs(firstCombFraction - laterCombFraction) < 1.0e-6f,
               name + " changed comb position with the note counter, so a picking "
                      "hand's contact offset is being drawn for a fingered note ("
                   + std::to_string(firstCombFraction) + " against "
                   + std::to_string(laterCombFraction) + ")");
    }

    // ... and a picked note still varies, or the clause above is vacuous.
    const auto pickedFirst = attackAt(PlayStyle::Sustain, false);
    const auto pickedLater = attackAt(PlayStyle::Sustain, true);
    expect(pickedFirst.valid && pickedLater.valid,
           "the picked control note did not sound");
    expect(pickedFirst.excitationAmplitude != pickedLater.excitationAmplitude
               || pickedFirst.excitationLength != pickedLater.excitationLength
               || pickedFirst.excitationCombDelay != pickedLater.excitationCombDelay,
           "two picked notes at different points in the sequence got the same "
           "stroke, so the picking-hand variation has gone altogether");
}

void testExplicitLegacyExpressionIdIsBitExact()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine implicit;
    ElectryEngine explicitId;
    implicit.prepare(sampleRate, 512);
    explicitId.prepare(sampleRate, 512);
    implicit.setPitchBend(0.35f);
    explicitId.setPitchBend(0.35f);
    // ID 0 belongs to the global wheel and is not a per-note target.
    explicitId.setExpressionPitchBend(ElectryEngine::legacyExpressionId, 12.0f);

    const std::array<ElectryEngine::NoteOnEvent, 3> implicitChord {{
        { 40, 0.83f }, { 45, 0.81f }, { 52, 0.79f }
    }};
    const std::array<ElectryEngine::NoteOnEvent, 3> explicitChord {{
        { 40, 0.83f, ElectryEngine::legacyExpressionId },
        { 45, 0.81f, ElectryEngine::legacyExpressionId },
        { 52, 0.79f, ElectryEngine::legacyExpressionId }
    }};
    implicit.noteOnChord(implicitChord);
    explicitId.noteOnChord(explicitChord);

    StereoBuffer implicitAttack(4096);
    StereoBuffer explicitAttack(4096);
    renderInto(implicit, implicitAttack, 127);
    renderInto(explicitId, explicitAttack, 127);
    expect(implicitAttack.left == explicitAttack.left
               && implicitAttack.right == explicitAttack.right,
           "explicit expression ID 0 changed the legacy chord render");

    for (const int note : { 40, 45, 52 })
    {
        implicit.noteOff(note);
        explicitId.noteOff(note, ElectryEngine::legacyExpressionId);
    }
    StereoBuffer implicitRelease(1024);
    StereoBuffer explicitRelease(1024);
    renderInto(implicit, implicitRelease, 113);
    renderInto(explicitId, explicitRelease, 113);
    expect(implicitRelease.left == explicitRelease.left
               && implicitRelease.right == explicitRelease.right,
           "explicit expression ID 0 changed the legacy release render");
}

void testExpressionPitchBendIsSelective()
{
    constexpr double sampleRate = 48000.0;
    constexpr ElectryEngine::ExpressionId lowerId = 2;
    constexpr ElectryEngine::ExpressionId upperId = 15;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.noteOn(45, 0.8f, lowerId);
    engine.noteOn(52, 0.8f, upperId);

    const auto stringFor = [&] (ElectryEngine::ExpressionId expressionId)
    {
        for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount;
             ++stringIndex)
            if (TestAccess::snapshot(engine, stringIndex).expressionId
                == expressionId)
                return stringIndex;
        return -1;
    };
    const int lowerString = stringFor(lowerId);
    const int upperString = stringFor(upperId);
    expect(lowerString >= 0 && upperString >= 0 && lowerString != upperString,
           "two expression IDs did not own two playable strings");
    if (lowerString < 0 || upperString < 0 || lowerString == upperString)
        return;

    const int controlFrames = TestAccess::hostFramesPerControlPeriod(engine);
    StereoBuffer establish(controlFrames);
    renderInto(engine, establish, controlFrames);
    const float lowerBefore =
        TestAccess::snapshot(engine, lowerString).verticalDelayTarget;
    const float upperBefore =
        TestAccess::snapshot(engine, upperString).verticalDelayTarget;

    // The legacy global wheel must not leak into member-channel voices, while
    // the selected member begins the same Bend Time glide on the next tick.
    engine.setPitchBend(1.0f);
    engine.setExpressionPitchBend(lowerId, 2.0f);
    StereoBuffer firstBendTick(controlFrames);
    renderInto(engine, firstBendTick, controlFrames);
    const float lowerAfter =
        TestAccess::snapshot(engine, lowerString).verticalDelayTarget;
    const float upperAfter =
        TestAccess::snapshot(engine, upperString).verticalDelayTarget;
    expect(lowerAfter < lowerBefore,
           "the selected expression ID did not begin bending upward");
    expect(upperAfter == upperBefore,
           "one member channel's bend moved another member channel");
    expect(TestAccess::expressionPitchBendTarget(engine, lowerId) == 2.0f
               && TestAccess::expressionPitchBendSemitones(engine, lowerId)
                      > 0.0f
               && TestAccess::expressionPitchBendSemitones(engine, upperId)
                      == 0.0f,
           "the fixed per-expression bend state did not glide selectively");

    // A lifted MPE finger leaves a short physical string tail. Later wheel
    // traffic for the now-idle/reused channel must not retune that old tail,
    // while a new finger must start exactly at the cached channel target.
    const float performedBend =
        TestAccess::expressionPitchBendSemitones(engine, lowerId);
    engine.setSustainPedal(true);
    engine.noteOff(45, lowerId);
    const auto released = TestAccess::snapshot(engine, lowerString);
    expect(released.expressionPitchBendFrozen
               && std::abs(released.frozenExpressionPitchBendSemitones
                           - performedBend) < 1.0e-6f,
           "an MPE release did not freeze its performed pitch interval");
    engine.setExpressionPitchBend(lowerId, -2.0f);
    engine.snapExpressionPitchBendToTarget(lowerId);
    StereoBuffer releasedTail(controlFrames);
    renderInto(engine, releasedTail, controlFrames);
    const auto untouchedTail = TestAccess::snapshot(engine, lowerString);
    expect(untouchedTail.expressionPitchBendFrozen
               && std::abs(untouchedTail.frozenExpressionPitchBendSemitones
                           - performedBend) < 1.0e-6f,
           "idle member-channel pitch traffic retuned a released string tail");
    expect(TestAccess::expressionPitchBendTarget(engine, lowerId) == -2.0f
               && TestAccess::expressionPitchBendSemitones(engine, lowerId)
                      == -2.0f,
           "snapping an idle expression did not reach its cached target");

    // The Zone Master remains an active controller after Note Off, including
    // for a CC64-held tail. It is a separate interval from the member bend:
    // moving it must retune the old lower-zone string without thawing that
    // string's member interval or reaching an upper-zone expression ID.
    const float releasedPitch = untouchedTail.lastCompensatedSemitones;
    const float upperPitch = TestAccess::snapshot(engine, upperString)
                                 .lastCompensatedSemitones;
    engine.setExpressionMasterPitchBend(lowerId, 1.0f);
    StereoBuffer lowerMasterTick(controlFrames);
    renderInto(engine, lowerMasterTick, controlFrames);
    const auto masterMovedTail = TestAccess::snapshot(engine, lowerString);
    expect(masterMovedTail.lastCompensatedSemitones > releasedPitch
               && masterMovedTail.expressionPitchBendFrozen
               && std::abs(masterMovedTail.frozenExpressionPitchBendSemitones
                           - performedBend) < 1.0e-6f,
           "a lower-zone master bend did not move a CC64-held release tail "
           "independently of its frozen member bend");
    expect(TestAccess::snapshot(engine, upperString)
                   .lastCompensatedSemitones == upperPitch,
           "a lower-zone master bend leaked into an upper-zone expression");
    expect(TestAccess::expressionMasterPitchBendTarget(engine, lowerId) == 1.0f
               && TestAccess::expressionMasterPitchBendSemitones(engine,
                                                                  lowerId)
                      > 0.0f,
           "the fixed per-expression master state did not glide selectively");

    const float upperBeforeMaster = TestAccess::snapshot(engine, upperString)
                                        .lastCompensatedSemitones;
    engine.setExpressionMasterPitchBend(upperId, -1.0f);
    StereoBuffer upperMasterTick(controlFrames);
    renderInto(engine, upperMasterTick, controlFrames);
    expect(TestAccess::snapshot(engine, upperString)
                   .lastCompensatedSemitones < upperBeforeMaster,
           "an upper-zone expression did not follow its own master bend");
    engine.noteOn(45, 0.8f, lowerId);
    expect(! TestAccess::snapshot(engine, lowerString)
                 .expressionPitchBendFrozen,
           "a reused MPE finger inherited the preceding release-tail freeze");
}

void testDelayedRepickPreservesFrozenExpressionPitch()
{
    constexpr double sampleRate = 48000.0;
    constexpr int note = 28;
    constexpr ElectryEngine::ExpressionId expressionId = 2;
    constexpr float performedBend = 1.25f;

    ElectryEngine engine;
    EngineParameters parameters;
    parameters.strumSpreadSeconds = 0.020f;
    engine.prepare(sampleRate, 512);
    engine.setParameters(parameters);
    engine.reset();
    engine.setExpressionPitchBend(expressionId, performedBend);
    engine.snapExpressionPitchBendToTarget(expressionId);
    engine.noteOn(note, 0.90f, expressionId);
    StereoBuffer establish(static_cast<int>(0.080 * sampleRate));
    renderInto(engine, establish);

    const int stringIndex = TestAccess::stringForNote(engine, note);
    expect(stringIndex >= 0,
           "the delayed MPE repick fixture did not allocate its string");
    if (stringIndex < 0)
        return;

    // The second owner reserves a future pick contact. Releasing both owners
    // under CC64 freezes the old finger's performed member bend, but leaves
    // that physical contact alive so the reserved pick can still arrive.
    engine.noteOn(note, 0.70f, expressionId);
    const auto scheduled = TestAccess::snapshot(engine, stringIndex);
    expect(scheduled.pendingRepickActive && scheduled.startDelaySamples > 0,
           "the overlapping MPE repick was not delayed for the regression");
    engine.setSustainPedal(true);
    engine.noteOff(note, expressionId);
    engine.noteOff(note, expressionId);
    const auto frozenBeforeContact = TestAccess::snapshot(engine, stringIndex);
    expect(! frozenBeforeContact.keyDown
               && frozenBeforeContact.pendingRepickActive
               && frozenBeforeContact.expressionPitchBendFrozen
               && std::abs(frozenBeforeContact
                               .frozenExpressionPitchBendSemitones
                           - performedBend) < 1.0e-6f,
           "CC64 did not freeze the member bend while a repick was pending");

    // Reuse the now-idle member channel before contact. The ringing string is
    // frozen at +1.25 semitones even though the channel cache now says -2.
    // Stop one internal sample before contact so the pitch solve performed by
    // startVoice() itself can be inspected before the next control tick.
    engine.setExpressionPitchBend(expressionId, -2.0f);
    engine.snapExpressionPitchBendToTarget(expressionId);
    int guard = 0;
    while (TestAccess::snapshot(engine, stringIndex).startDelaySamples > 1
           && guard++ < static_cast<int>(sampleRate))
        TestAccess::renderOneInternalSample(engine);
    const auto immediatelyBeforeContact =
        TestAccess::snapshot(engine, stringIndex);
#if ELECTRY_ENERGY_ATTACK_PITCH
    const auto energyBeforeContact =
        TestAccess::attackPitchState(engine, stringIndex);
    const float nominalTargetBeforeContact =
        TestAccess::effectiveLoopFrequency(engine, stringIndex, false, true)
        / energyBeforeContact.frequencyFactor;
#endif
    expect(immediatelyBeforeContact.pendingRepickActive
               && immediatelyBeforeContact.startDelaySamples == 1
               && std::abs(immediatelyBeforeContact.lastCompensatedSemitones
                           - performedBend) < 1.0e-6f,
           "idle member pitch traffic changed the frozen repick pre-roll");
    TestAccess::renderOneInternalSample(engine);
    const auto frozenAfterContact = TestAccess::snapshot(engine, stringIndex);
#if ELECTRY_ENERGY_ATTACK_PITCH
    const auto energyAfterContact =
        TestAccess::attackPitchState(engine, stringIndex);
    const float nominalTargetAfterContact =
        TestAccess::effectiveLoopFrequency(engine, stringIndex, false, true)
        / energyAfterContact.frequencyFactor;
    const double contactPitchMoveCents = centsBetween(
        nominalTargetAfterContact, nominalTargetBeforeContact);
    const bool contactPitchPreserved =
        std::abs(contactPitchMoveCents) < 0.08;
#else
    constexpr double contactPitchMoveCents = 0.0;
    const bool contactPitchPreserved =
        std::abs(frozenAfterContact.verticalDelayTarget
                 - immediatelyBeforeContact.verticalDelayTarget) < 1.0e-5f;
#endif
    expect(! frozenAfterContact.pendingRepickActive
               && frozenAfterContact.expressionPitchBendFrozen
               && std::abs(frozenAfterContact
                               .frozenExpressionPitchBendSemitones
                           - performedBend) < 1.0e-6f
               && std::abs(frozenAfterContact.lastCompensatedSemitones
                           - performedBend) < 1.0e-6f
               && contactPitchPreserved,
           "delayed pick contact transiently retuned a CC64-held member bend ("
               + std::to_string(contactPitchMoveCents) + " cents)");

    StereoBuffer idleWheel(TestAccess::hostFramesPerControlPeriod(engine));
    renderInto(engine, idleWheel);
    const auto afterIdleWheel = TestAccess::snapshot(engine, stringIndex);
    expect(afterIdleWheel.expressionPitchBendFrozen
               && std::abs(afterIdleWheel.frozenExpressionPitchBendSemitones
                           - performedBend) < 1.0e-6f,
           "idle member pitch traffic retuned the contacted CC64 tail");

    // The converse path reclaims a frozen CC64 tail with a genuinely new
    // finger. Its old ring stays at the frozen pitch during pick travel, but
    // contact must thaw it and configure the cached member target immediately.
    ElectryEngine reclaimed;
    reclaimed.prepare(sampleRate, 512);
    reclaimed.setParameters(parameters);
    reclaimed.reset();
    reclaimed.setExpressionPitchBend(expressionId, performedBend);
    reclaimed.snapExpressionPitchBendToTarget(expressionId);
    reclaimed.noteOn(note, 0.90f, expressionId);
    StereoBuffer reclaimedEstablish(static_cast<int>(0.080 * sampleRate));
    renderInto(reclaimed, reclaimedEstablish);
    const int reclaimedString = TestAccess::stringForNote(reclaimed, note);
    expect(reclaimedString >= 0,
           "the reclaimed MPE repick fixture did not allocate its string");
    if (reclaimedString < 0)
        return;

    reclaimed.setSustainPedal(true);
    reclaimed.noteOff(note, expressionId);
    reclaimed.setExpressionPitchBend(expressionId, -2.0f);
    reclaimed.snapExpressionPitchBendToTarget(expressionId);
    reclaimed.noteOn(note, 0.70f, expressionId);
    const auto reclaimScheduled =
        TestAccess::snapshot(reclaimed, reclaimedString);
    expect(reclaimScheduled.keyDown && reclaimScheduled.pendingRepickActive
               && reclaimScheduled.expressionPitchBendFrozen,
           "the new-finger reclaim did not preserve its old ring in pre-roll");

    guard = 0;
    while (TestAccess::snapshot(reclaimed, reclaimedString).startDelaySamples
               > 1
           && guard++ < static_cast<int>(sampleRate))
        TestAccess::renderOneInternalSample(reclaimed);
    const auto reclaimBeforeContact =
        TestAccess::snapshot(reclaimed, reclaimedString);
    expect(reclaimBeforeContact.pendingRepickActive
               && reclaimBeforeContact.startDelaySamples == 1
               && reclaimBeforeContact.expressionPitchBendFrozen
               && std::abs(reclaimBeforeContact.lastCompensatedSemitones
                           - performedBend) < 1.0e-6f,
           "a new finger retuned the frozen old ring before pick contact");
    TestAccess::renderOneInternalSample(reclaimed);
    const auto reclaimAfterContact =
        TestAccess::snapshot(reclaimed, reclaimedString);
    expect(! reclaimAfterContact.pendingRepickActive
               && reclaimAfterContact.keyDown
               && ! reclaimAfterContact.expressionPitchBendFrozen
               && std::abs(reclaimAfterContact.lastCompensatedSemitones
                           + 2.0f) < 1.0e-6f
               && reclaimAfterContact.verticalDelayTarget
                      > reclaimBeforeContact.verticalDelayTarget + 1.0e-3f,
           "new-finger contact retained the old frozen member bend instead "
           "of its cached target");
}

void testSamePitchExpressionOwnersReleaseIndependently()
{
    constexpr double sampleRate = 48000.0;
    constexpr ElectryEngine::ExpressionId firstId = 3;
    constexpr ElectryEngine::ExpressionId secondId = 4;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    const std::array<ElectryEngine::NoteOnEvent, 2> unison {{
        { 64, 0.82f, firstId }, { 64, 0.78f, secondId }
    }};
    engine.noteOnChord(unison);

    int firstString = -1;
    int secondString = -1;
    for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount;
         ++stringIndex)
    {
        const auto voice = TestAccess::snapshot(engine, stringIndex);
        if (voice.active && voice.midiNote == 64
            && voice.expressionId == firstId)
            firstString = stringIndex;
        if (voice.active && voice.midiNote == 64
            && voice.expressionId == secondId)
            secondString = stringIndex;
    }
    expect(firstString >= 0 && secondString >= 0 && firstString != secondString,
           "same-pitch member channels collapsed onto one physical string");
    if (firstString < 0 || secondString < 0 || firstString == secondString)
        return;

    engine.noteOff(64, firstId);
    const auto released = TestAccess::snapshot(engine, firstString);
    const auto held = TestAccess::snapshot(engine, secondString);
    expect(! released.keyDown && released.releasing,
           "note-off did not release its same-pitch expression owner");
    expect(held.keyDown && ! held.releasing
               && held.expressionId == secondId
               && TestAccess::heldExpressionId(engine, secondString) == secondId,
           "one same-pitch note-off released the other expression owner");

    engine.noteOn(67, 0.8f, secondId);
    const auto moved = TestAccess::snapshot(engine, secondString);
    expect(moved.active && moved.keyDown && moved.midiNote == 67
               && moved.expressionId == secondId,
           "a member channel moving pitch abandoned its physical owner");
    engine.noteOff(64, secondId);
    expect(TestAccess::snapshot(engine, secondString).keyDown,
           "the old pitch's note-off released a moved expression owner");

    for (const auto style : { PlayStyle::Hammer, PlayStyle::Slide })
    {
        ElectryEngine legatoEngine;
        legatoEngine.prepare(sampleRate, 512);
        legatoEngine.noteOn(
            ElectryEngine::firstPlayStyleKeyswitchNote
                + static_cast<int>(style),
            1.0f);
        legatoEngine.noteOn(65, 0.82f, firstId);
        legatoEngine.noteOn(65, 0.78f, secondId);

        bool foundFirst = false;
        bool foundSecond = false;
        for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount;
             ++stringIndex)
        {
            const auto voice = TestAccess::snapshot(legatoEngine, stringIndex);
            foundFirst = foundFirst
                || (voice.active && voice.expressionId == firstId);
            foundSecond = foundSecond
                || (voice.active && voice.expressionId == secondId);
        }
        expect(foundFirst && foundSecond
                   && legatoEngine.getActiveVoiceCount() == 2,
               "a scalar Hammer/Slide stole another expression owner's "
               "same-pitch string");
    }
}

void testPitchWheelUsesUniformSemitoneInterval()
{
    // Standard MIDI pitch bend is one interval for the whole instrument. E1
    // and D3 must therefore reach the same +/-2-semitone targets rather than
    // pulling a chord apart according to per-string compliance.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.bendTimeSeconds = 0.06f;
    engine.setParameters(parameters);

    for (const int openNote : { 28, 50 })
    {
        engine.reset();
        engine.setPitchBend(0.0f);
        engine.noteOn(openNote, 0.35f);
        StereoBuffer centre(static_cast<int>(0.6 * sampleRate));
        renderInto(engine, centre);

        const double before = measureFrequency(
            centre.left, static_cast<int>(0.2 * sampleRate),
            static_cast<int>(0.35 * sampleRate), sampleRate, midiHz(openNote),
            260.0);

        engine.setPitchBend(1.0f);
        StereoBuffer up(static_cast<int>(1.0 * sampleRate));
        renderInto(engine, up);
        const double upHz = measureFrequency(
            up.left, static_cast<int>(0.4 * sampleRate),
            static_cast<int>(0.5 * sampleRate), sampleRate,
            midiHz(openNote), 260.0);

        engine.setPitchBend(-1.0f);
        StereoBuffer down(static_cast<int>(1.0 * sampleRate));
        renderInto(engine, down);
        const double downHz = measureFrequency(
            down.left, static_cast<int>(0.4 * sampleRate),
            static_cast<int>(0.5 * sampleRate), sampleRate,
            midiHz(openNote), 260.0);

        const double centsUp = centsBetween(upHz, before);
        const double centsDown = centsBetween(downHz, before);
        expect(std::abs(centsUp - 200.0) < 15.0,
               "note " + std::to_string(openNote)
                   + " full-up wheel travel measured "
                   + std::to_string(centsUp) + " cents, expected 200");
        expect(std::abs(centsDown + 200.0) < 15.0,
               "note " + std::to_string(openNote)
                   + " full-down wheel travel measured "
                   + std::to_string(centsDown) + " cents, expected -200");
    }
}

void testReleaseDampingUsesPerformedPitch()
{
    // The release gain is applied once per completed string period. Its
    // per-loop target therefore has to follow the pitch sounding when the
    // finger lifts and while a live wheel keeps moving the tail, or a
    // two-semitone move changes the nominal 60 ms damping time by about
    // twelve percent.
    constexpr double sampleRate = 48000.0;
    constexpr int note = 47;
    constexpr ElectryEngine::ExpressionId expressionId = 9;
    const auto targetFor = [] (const ElectryEngine& engine,
                               const TestAccess::VoiceSnapshot& voice)
    {
        return std::pow(10.0f, -3.0f * voice.lastCompensatedPeriod
            / (0.060f * static_cast<float>(
                TestAccess::internalSampleRate(engine))));
    };

    const auto check = [&] (float legacyWheel, float memberSemitones,
                            float masterSemitones)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.bendTimeSeconds = 0.04f;
        engine.setParameters(parameters);
        engine.reset();

        const bool expression = memberSemitones != 0.0f
                             || masterSemitones != 0.0f;
        if (expression)
        {
            engine.setExpressionPitchBend(expressionId, memberSemitones);
            engine.snapExpressionPitchBendToTarget(expressionId);
            engine.setExpressionMasterPitchBend(expressionId,
                                                 masterSemitones);
        }
        else
        {
            engine.setPitchBend(legacyWheel);
        }
        engine.noteOn(note, 0.8f, expression
            ? expressionId : ElectryEngine::legacyExpressionId);
        StereoBuffer settle(static_cast<int>(0.30 * sampleRate));
        renderInto(engine, settle);

        const int stringIndex = TestAccess::stringForNote(engine, note);
        const auto before = TestAccess::snapshot(engine, stringIndex);
        engine.noteOff(note, expression
            ? expressionId : ElectryEngine::legacyExpressionId);
        const float expected = targetFor(engine, before);
        const float armedTarget = stringIndex >= 0
            ? TestAccess::releaseGainTarget(engine, stringIndex) : 0.0f;
        expect(stringIndex >= 0
                   && std::abs(armedTarget - expected) < 2.0e-7f,
               "note release damping ignored its performed pitch at "
                   + std::to_string(before.lastCompensatedSemitones)
                   + " semitones");

        // A released MPE member is frozen, but its zone master and the legacy
        // wheel intentionally remain live. Member traffic must leave both the
        // pitch and loss alone; either live control must move them together.
        if (expression)
        {
            engine.setExpressionPitchBend(expressionId, -4.0f);
            engine.snapExpressionPitchBendToTarget(expressionId);
            StereoBuffer memberTraffic(64);
            renderInto(engine, memberTraffic);
            const auto frozen = TestAccess::snapshot(engine, stringIndex);
            expect(std::abs(frozen.lastCompensatedSemitones
                            - before.lastCompensatedSemitones) < 1.0e-6f
                       && std::abs(TestAccess::releaseGainTarget(
                                       engine, stringIndex) - armedTarget)
                              < 2.0e-7f,
                   "released MPE member traffic changed its frozen pitch or "
                   "damping");
            engine.setExpressionMasterPitchBend(expressionId, 2.0f);
        }
        else if (legacyWheel == 0.0f)
        {
            engine.setPitchBend(1.0f);
        }
        else
        {
            return;
        }

        StereoBuffer movingTail(1024);
        renderInto(engine, movingTail);
        const auto moved = TestAccess::snapshot(engine, stringIndex);
        const float movedExpected = targetFor(engine, moved);
        expect(moved.active && moved.releasing
                   && std::abs(TestAccess::releaseGainTarget(
                                   engine, stringIndex) - movedExpected)
                          < 2.0e-7f
                   && std::abs(movedExpected - expected) > 1.0e-4f,
               expression
                   ? "release damping did not follow live MPE master bend"
                   : "release damping did not follow live legacy wheel bend");
    };

    check(0.0f, 0.0f, 0.0f);
    check(1.0f, 0.0f, 0.0f);
    check(-1.0f, 0.0f, 0.0f);
    check(0.0f, 3.5f, -1.25f);
}

void testHeldDampingUsesPerformedPitch()
{
    constexpr double sampleRate = 48000.0;
    constexpr int sourceNote = 47; // B2, fret 2 on the A string
    constexpr int targetNote = 49; // C#3, two-fret slide on the same string

    const auto configured = []
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.bodyResonance = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.bendTimeSeconds = 0.04f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(sourceNote, 0.8f);
        StereoBuffer settle(static_cast<int>(0.20 * sampleRate));
        renderInto(engine, settle);
        return engine;
    };
    const auto expectSameDecay = [] (double reference, double actual,
                                     const std::string& context)
    {
        expect(reference > 0.0 && actual > 0.0
                   && std::abs(actual / reference - 1.0) < 0.005,
               context + " changed held-string T60 from "
                   + std::to_string(reference) + " s to "
                   + std::to_string(actual) + " s");
    };

    {
        auto engine = configured();
        const int stringIndex = TestAccess::stringForNote(engine, sourceNote);
        const double rest =
            TestAccess::realisedVoiceFundamentalT60(engine, stringIndex);
        const float cachedDamping =
            TestAccess::lastDampedFrequency(engine, stringIndex);
        engine.setPitchBend(0.01f); // two cents: below the six-cent fit quantum
        StereoBuffer subQuantum(512);
        renderInto(engine, subQuantum);
        expect(TestAccess::lastDampedFrequency(engine, stringIndex)
                   == cachedDamping,
               "a sub-quantum bend unnecessarily re-solved held damping");
        engine.setPitchBend(1.0f);
        StereoBuffer bend(static_cast<int>(0.12 * sampleRate));
        renderInto(engine, bend);
        expectSameDecay(
            rest,
            TestAccess::realisedVoiceFundamentalT60(engine, stringIndex),
            "a settled two-semitone bend");
    }

    {
        auto engine = configured();
        const int stringIndex = TestAccess::stringForNote(engine, sourceNote);
        const double rest =
            TestAccess::realisedVoiceFundamentalT60(engine, stringIndex);
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(targetNote, 0.8f);
        expect(TestAccess::stringForNote(engine, targetNote) == stringIndex,
               "the live-damping slide fixture changed physical strings");
        expectSameDecay(
            rest,
            TestAccess::realisedVoiceFundamentalT60(engine, stringIndex),
            "a slide at source contact");

        StereoBuffer middle(static_cast<int>(0.022 * sampleRate));
        renderInto(engine, middle);
        expectSameDecay(
            rest,
            TestAccess::realisedVoiceFundamentalT60(engine, stringIndex),
            "a slide at mid travel");

        StereoBuffer destination(static_cast<int>(0.08 * sampleRate));
        renderInto(engine, destination);
        expectSameDecay(
            rest,
            TestAccess::realisedVoiceFundamentalT60(engine, stringIndex),
            "a slide at its destination");
    }
}

void testStiffnessFollowsLiveTension()
{
    constexpr double sampleRate = 48000.0;
    constexpr int note = 47; // stopped B2 on the A string
    constexpr ElectryEngine::ExpressionId expressionId = 5;

    const auto stiffnessAt = [&] (bool memberExpression, float semitones)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        engine.setParameters(parameters);
        if (memberExpression)
            engine.setExpressionPitchBend(expressionId, semitones);
        else
            engine.setPitchBend(0.5f * semitones);
        engine.reset();
        engine.noteOn(note, 0.8f, memberExpression
            ? expressionId : ElectryEngine::legacyExpressionId);
        const int stringIndex = TestAccess::stringForNote(engine, note);
        expect(stringIndex == 3,
               "the live-tension stiffness fixture missed the A string");
        return stringIndex >= 0
            ? TestAccess::snapshot(engine, stringIndex).inharmonicity : 0.0f;
    };

    const float unbentGlobal = stiffnessAt(false, 0.0f);
    const float unbentMember = stiffnessAt(true, 0.0f);
    for (const float semitones : { -2.0f, 2.0f })
    {
        const float expectedRatio = std::exp2(-semitones / 6.0f);
        const float global = stiffnessAt(false, semitones);
        const float member = stiffnessAt(true, semitones);
        expect(unbentGlobal > 0.0f && unbentMember > 0.0f
                   && std::abs(global / unbentGlobal - expectedRatio) < 2.0e-5f
                   && std::abs(member / unbentMember - expectedRatio) < 2.0e-5f,
               "global/MPE bend left stiffness outside the live 1/T law at "
                   + std::to_string(semitones) + " semitones");
    }

#if ELECTRY_ENERGY_ATTACK_PITCH
    // The attack-pitch experiment moves the compensated period but is
    // deliberately not part of the static construction fit. Keep a live
    // attack factor present while a member bend actually re-fits B, so using
    // the experiment's f0 here cannot pass as an unchanged-cache result.
    {
        ElectryEngine energy;
        energy.prepare(sampleRate, 512);
        energy.setParameters(EngineParameters {});
        energy.reset();
        energy.noteOn(note, 0.8f, expressionId);
        const int energyString = TestAccess::stringForNote(energy, note);
        expect(energyString == 3,
               "the energy live-tension fixture missed the A string");
        const int safeEnergyString = std::max(energyString, 0);
        const float before = TestAccess::snapshot(
            energy, safeEnergyString).inharmonicity;
        energy.setExpressionPitchBend(expressionId, 2.0f);
        energy.snapExpressionPitchBendToTarget(expressionId);
        TestAccess::setAttackPitchAfterLoopCacheAdvanced(
            energy, safeEnergyString, 1.003f);
        const auto after = TestAccess::snapshot(energy, safeEnergyString);
        const auto attack = TestAccess::attackPitchState(
            energy, safeEnergyString);
        expect(attack.frequencyFactor > 1.002f
                   && std::abs(after.inharmonicity / before
                                   - std::exp2(-2.0f / 6.0f)) < 2.0e-5f,
               "energy attack pitch leaked into the static stiffness fit");
    }
#endif

    // Fretting-hand vibrato reaches the same tension coordinate through its
    // own public gesture. Score only ticks that actually re-fit the quantised
    // dispersion grid, where B and the sampled excursion are synchronous.
    ElectryEngine vibrato;
    vibrato.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.vibratoDepth = 1.0f;
    vibrato.setParameters(parameters);
    vibrato.reset();
    vibrato.noteOn(note, 0.8f);
    const int stringIndex = TestAccess::stringForNote(vibrato, note);
    expect(stringIndex == 3,
           "the vibrato-stiffness fixture missed the A string");
    const float resting = stringIndex >= 0
        ? TestAccess::snapshot(vibrato, stringIndex).inharmonicity : 0.0f;
    float previous = resting;
    float deepest = resting;
    float widestSemitones = 0.0f;
    float worstRatioError = 0.0f;
    int refits = 0;
    vibrato.setVibrato(1.0f);
    const int tickFrames = TestAccess::hostFramesPerControlPeriod(vibrato);
    StereoBuffer tick(tickFrames);
    for (int rendered = 0; rendered < static_cast<int>(1.0 * sampleRate);
         rendered += tickFrames)
    {
        renderInto(vibrato, tick, tickFrames);
        const auto voice = TestAccess::snapshot(vibrato, stringIndex);
        deepest = std::min(deepest, voice.inharmonicity);
        widestSemitones = std::max(widestSemitones, voice.vibratoSemitones);
        if (voice.inharmonicity == previous)
            continue;
        ++refits;
        const float expectedRatio = std::exp2(-voice.vibratoSemitones / 6.0f);
        worstRatioError = std::max(
            worstRatioError,
            std::abs(voice.inharmonicity / resting / expectedRatio - 1.0f));
        previous = voice.inharmonicity;
    }
    expect(resting > 0.0f && refits >= 3 && widestSemitones > 0.25f
               && deepest < 0.97f * resting && worstRatioError < 2.0e-4f,
           "fretting-hand vibrato did not carry stiffness through live tension");
}

void testPitchWheelGlideFollowsBendTime()
{
    // The strings travel to the wheel over the Bend Time parameter: shortly
    // after a full-range move, a slow bend has covered far less of the
    // distance than a fast one, and both settle on the same target.
    constexpr double sampleRate = 48000.0;

    const auto travelAfter = [&] (float bendTimeSeconds, double measureAt)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.bendTimeSeconds = bendTimeSeconds;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(28, 0.35f); // E1
        StereoBuffer settle(static_cast<int>(0.6 * sampleRate));
        renderInto(engine, settle);
        engine.setPitchBend(1.0f);
        StereoBuffer bent(static_cast<int>((measureAt + 0.16) * sampleRate));
        renderInto(engine, bent);
        const double searchCentre = midiHz(28) * std::pow(2.0, 1.0 / 12.0);
        const double before = measureFrequency(
            settle.left, static_cast<int>(0.2 * sampleRate),
            static_cast<int>(0.35 * sampleRate), sampleRate, searchCentre,
            160.0);
        const double during = measureFrequency(
            bent.left, static_cast<int>(measureAt * sampleRate),
            static_cast<int>(0.15 * sampleRate), sampleRate,
            searchCentre, 160.0);
        return centsBetween(during, before);
    };

    const double fast = travelAfter(0.05f, 0.25);
    const double slow = travelAfter(1.60f, 0.25);
    expect(fast > 175.0,
           "a fast bend time did not reach the wheel target promptly ("
               + std::to_string(fast) + " cents)");
    expect(slow < fast - 60.0,
           "a slow bend time did not slow the wheel's travel (fast "
               + std::to_string(fast) + ", slow " + std::to_string(slow)
               + " cents)");
    const double slowSettled = travelAfter(1.60f, 5.5);
    expect(std::abs(slowSettled - 200.0) < 15.0,
           "a slow bend did not settle on the wheel target ("
               + std::to_string(slowSettled) + " cents)");

    // The dispersion fit is quantised for realtime cost. Crossing one of its
    // cells may change filter phase, but the delay-line coordinate must absorb
    // that phase change instead of making a rising wheel briefly fall.
    {
        constexpr double traceSampleRate = 44100.0;
        ElectryEngine engine;
        engine.prepare(traceSampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.bodyResonance = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.bendTimeSeconds = 0.28f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.85f);
        StereoBuffer establish(static_cast<int>(0.30 * traceSampleRate));
        renderInto(engine, establish);

        constexpr int stringIndex = 3;
        const float start = TestAccess::effectiveLoopFrequency(
            engine, stringIndex);
        float highWater = start;
        double worstDrawdownCents = 0.0;
        engine.setPitchBend(1.0f);
        const int traceFrames = TestAccess::hostFramesPerControlPeriod(engine);
        StereoBuffer frame(traceFrames);
        const int totalFrames = static_cast<int>(0.45 * traceSampleRate);
        for (int rendered = 0; rendered < totalFrames;
             rendered += traceFrames)
        {
            renderInto(engine, frame, traceFrames);
            const float current = TestAccess::effectiveLoopFrequency(
                engine, stringIndex);
            highWater = std::max(highWater, current);
            worstDrawdownCents = std::max(
                worstDrawdownCents, centsBetween(highWater, current));
        }
        const float final = TestAccess::effectiveLoopFrequency(
            engine, stringIndex);
        expect(worstDrawdownCents < 0.05,
               "a rising pitch-wheel glide fell "
                   + std::to_string(worstDrawdownCents)
                   + " cents at a dispersion refit");
        expect(centsBetween(final, start) > 190.0,
               "the monotone wheel fixture did not reach its target");
    }
}

void testPitchWheelBendsSympatheticStrings()
{
    // A ringing coupled string follows the same MIDI interval as a played
    // string.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    parameters.sympatheticAmount = 0.9f;
    parameters.bendTimeSeconds = 0.06f;
    engine.setParameters(parameters);
    engine.reset();

    // A2's third partial rings the open high E through the bridge. The
    // played string is released before the wheel moves, so what is measured
    // afterwards is the coupled ring alone, not the played string's own bent
    // partial landing nearby.
    engine.noteOn(45, 0.95f);
    StereoBuffer ringing(static_cast<int>(0.7 * sampleRate));
    renderInto(engine, ringing);
    const int highString = ElectryEngine::stringCount - 1;
    expect(TestAccess::snapshot(engine, highString).sympatheticReady,
           "the open high E did not ring for the wheel to bend");
    engine.noteOff(45);
    StereoBuffer damping(static_cast<int>(0.4 * sampleRate));
    renderInto(engine, damping);

    engine.setPitchBend(1.0f);
    StereoBuffer bent(static_cast<int>(1.4 * sampleRate));
    renderInto(engine, bent);

    constexpr double openHighE = 329.62756;
    const double bentHighE = openHighE * std::pow(2.0, 2.0 / 12.0);
    const int start = static_cast<int>(0.6 * sampleRate);
    const int length = static_cast<int>(0.7 * sampleRate);
    const double atOpen = dftMagnitude(bent.left, start, length, sampleRate,
                                       openHighE);
    const double atBent = dftMagnitude(bent.left, start, length, sampleRate,
                                       bentHighE);
    expect(atBent > 2.0 * atOpen,
           "the coupled high E did not follow the wheel ("
               + std::to_string(atOpen) + " at open pitch, "
               + std::to_string(atBent) + " at the bent pitch)");
}

void testHeldLegatoSourceOutranksPlainReleaseTails()
{
    constexpr double sampleRate = 44100.0;
    constexpr int targetNote = 45;
    constexpr int sourceNote = 40;
    constexpr int tailString = 3;
    constexpr int sourceString = 2;

    const auto wait = [] (ElectryEngine& engine, double seconds)
    {
        StereoBuffer audio(static_cast<int>(seconds * sampleRate));
        renderInto(engine, audio);
    };
    const auto prepareCollision = [&] (
        bool releaseTarget = true, bool sustainTarget = false,
        ElectryEngine::ExpressionId sourceExpression =
            ElectryEngine::legacyExpressionId,
        ElectryEngine::ExpressionId targetExpression =
            ElectryEngine::legacyExpressionId)
    {
        auto engine = std::make_unique<ElectryEngine>();
        engine->prepare(sampleRate, 512);
        engine->reset();
        engine->noteOn(targetNote, 0.95f, targetExpression);
        wait(*engine, 0.08);
        if (releaseTarget)
        {
            if (sustainTarget)
                engine->setSustainPedal(true);
            engine->noteOff(targetNote, targetExpression);
        }
        wait(*engine, 0.01);
        engine->noteOn(sourceNote, 0.95f, sourceExpression);
        wait(*engine, 0.02);
        expect(TestAccess::stringForNote(*engine, targetNote) == tailString
                   && TestAccess::stringForNote(*engine, sourceNote)
                          == sourceString,
               "the collision fixture missed released s3/held s2");
        return engine;
    };
    const auto target = [&] (ElectryEngine& engine, PlayStyle style,
                             ElectryEngine::ExpressionId expressionId =
                                 ElectryEngine::legacyExpressionId)
    {
        engine.noteOn(styleKeyswitch(style), 1.0f);
        engine.noteOn(targetNote, 0.85f, expressionId);
    };
    const auto chosen = [] (
        const ElectryEngine& engine, PlayStyle style,
        ElectryEngine::ExpressionId expressionId =
            ElectryEngine::legacyExpressionId)
    {
        return TestAccess::chosenString(engine, targetNote, style,
                                        expressionId);
    };

    for (const auto style : { PlayStyle::Slide, PlayStyle::Hammer })
    {
        auto engine = prepareCollision();
        const auto tailBefore = TestAccess::snapshot(*engine, tailString);
        const auto sourceBefore = TestAccess::snapshot(*engine, sourceString);
        expect(tailBefore.active && tailBefore.releasing
                   && tailBefore.midiNote == targetNote
                   && sourceBefore.active && sourceBefore.keyDown
                   && sourceBefore.midiNote == sourceNote,
               "the collision fixture lost its released tail or held source");

        target(*engine, style);
        const auto moved = TestAccess::snapshot(*engine, sourceString);
        const auto tail = TestAccess::snapshot(*engine, tailString);
        expect(moved.active && moved.keyDown && moved.midiNote == targetNote
                   && moved.playStyle == style
                   && TestAccess::legatoBlend(*engine, sourceString) == 0.0f
                   && std::abs(TestAccess::legatoFromFrequency(*engine,
                                                               sourceString)
                               - midiHz(sourceNote)) < 1.0e-3,
               std::string(style == PlayStyle::Slide ? "Slide" : "Hammer")
                   + " did not continue the held reachable source");
        expect(tail.active && tail.releasing && ! tail.keyDown
                   && tail.midiNote == targetNote
                   && tail.startOrder == tailBefore.startOrder,
               "held legato consumed the released same-target tail");
        const float slide = TestAccess::slideNoiseAmplitude(*engine,
                                                            sourceString);
        expect(style == PlayStyle::Slide
                   ? slide > 0.0f && moved.excitationAmplitude == 0.0f
                   : slide == 0.0f && moved.excitationAmplitude > 0.0f,
               "the held source began the wrong legato contact");

        StereoBuffer audible(static_cast<int>(0.20 * sampleRate));
        renderInto(*engine, audible, 256);
        expect(allFinite(audible) && peakAbs(audible.left) > 1.0e-5f,
               "the held-legato collision correction produced invalid audio");
    }

    // The same ownership error is not limited to a tail already sounding the
    // destination. MIDI 46 is one fret from the released A2 string but six
    // frets from the held E2 string; the explicit finger must still win. Use
    // the one-event chord path used by every scalar note in the demo renderer.
    constexpr int differentTarget = 46;
    for (const auto style : { PlayStyle::Slide, PlayStyle::Hammer })
    {
        auto engine = prepareCollision();
        const auto tailBefore = TestAccess::snapshot(*engine, tailString);
        expect(TestAccess::chosenString(*engine, differentTarget, style)
                   == sourceString,
               std::string(style == PlayStyle::Slide ? "Slide" : "Hammer")
                   + " preferred a closer different-note release tail");
        engine->noteOn(styleKeyswitch(style), 1.0f);
        const std::array<ElectryEngine::NoteOnEvent, 1> event {{
            { differentTarget, 0.85f }
        }};
        engine->noteOnChord(event);

        const auto moved = TestAccess::snapshot(*engine, sourceString);
        const auto tail = TestAccess::snapshot(*engine, tailString);
        expect(moved.active && moved.keyDown
                   && moved.midiNote == differentTarget
                   && moved.playStyle == style
                   && TestAccess::legatoBlend(*engine, sourceString) == 0.0f
                   && std::abs(TestAccess::legatoFromFrequency(
                                  *engine, sourceString)
                               - midiHz(sourceNote)) < 1.0e-3,
               "different-target legato did not continue its held source");
        expect(tail.active && tail.releasing && ! tail.keyDown
                   && tail.midiNote == targetNote
                   && tail.startOrder == tailBefore.startOrder,
               "different-target legato consumed the plain release tail");
        const float slide = TestAccess::slideNoiseAmplitude(*engine,
                                                            sourceString);
        expect(style == PlayStyle::Slide
                   ? slide > 0.0f && moved.excitationAmplitude == 0.0f
                   : slide == 0.0f && moved.excitationAmplitude > 0.0f,
               "different-target held source began the wrong contact");
    }

    // Demo 17's opening lead leaves MIDI 67 ringing without a finger on
    // string 5, then holds MIDI 62 on string 4 and asks that finger to Slide
    // to 69. The closer 67 tail must remain a tail rather than becoming the
    // performed gesture.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupSelector = PickupSelector::Bridge;
        parameters.pickupType = 0.42f;
        parameters.toneKnob = 0.92f;
        parameters.bodyResonance = 0.42f;
        parameters.stringAge = 0.06f;
        parameters.pickPosition = 0.27f;
        parameters.pickHardness = 0.78f;
        parameters.fingerNoise = 0.52f;
        parameters.artifactAmount = 0.12f;
        parameters.bendTimeSeconds = 0.16f;
        parameters.sympatheticAmount = 0.28f;
        parameters.outputGain = 1.55f;
        parameters.outputMode = electry::OutputMode::Stereo;
        engine.setParameters(parameters);
        engine.reset();
        const auto trigger = [&] (int note, float velocity)
        {
            const std::array<ElectryEngine::NoteOnEvent, 1> event {{
                { note, velocity }
            }};
            engine.noteOnChord(event);
        };
        wait(engine, 0.25);
        for (const int note : { 64, 67, 69, 72, 70, 67 })
        {
            trigger(note, note == 72 ? 0.98f : 0.86f);
            wait(engine, 0.27);
            engine.noteOff(note);
            wait(engine, 0.05);
        }
        trigger(62, 0.90f);
        wait(engine, 0.24);

        constexpr int demoSourceString = 4;
        constexpr int demoTailString = 5;
        const auto sourceBefore = TestAccess::snapshot(engine,
                                                       demoSourceString);
        const auto tailBefore = TestAccess::snapshot(engine, demoTailString);
        expect(sourceBefore.active && sourceBefore.keyDown
                   && ! sourceBefore.releasing && sourceBefore.midiNote == 62
                   && sourceBefore.fret == 12
                   && tailBefore.active && ! tailBefore.keyDown
                   && tailBefore.releasing && tailBefore.midiNote == 67
                   && tailBefore.fret == 12,
               "demo-17 opening Slide fixture missed held s4/released s5");

        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        trigger(69, 0.84f);
        const auto moved = TestAccess::snapshot(engine, demoSourceString);
        const auto tail = TestAccess::snapshot(engine, demoTailString);
        expect(moved.active && moved.keyDown && moved.midiNote == 69
                   && moved.playStyle == PlayStyle::Slide
                   && TestAccess::legatoBlend(engine, demoSourceString) == 0.0f
                   && std::abs(TestAccess::legatoFromFrequency(
                                  engine, demoSourceString)
                               - midiHz(62)) < 1.0e-3
                   && TestAccess::slideNoiseAmplitude(engine,
                                                      demoSourceString) > 0.0f
                   && moved.excitationAmplitude == 0.0f,
               "demo-17 opening Slide did not move its held MIDI-62 finger");
        expect(tail.active && ! tail.keyDown && tail.releasing
                   && tail.midiNote == 67
                   && tail.startOrder == tailBefore.startOrder,
               "demo-17 opening Slide consumed its MIDI-67 release tail");
    }

    // Every neighbouring ownership path keeps its established priority.
    auto noHeld = prepareCollision();
    noHeld->noteOff(sourceNote);
    expect(chosen(*noHeld, PlayStyle::Slide) == tailString,
           "the no-held-source same-target repick fallback changed");

    auto pedalSource = prepareCollision();
    pedalSource->setSustainPedal(true);
    pedalSource->noteOff(sourceNote);
    expect(chosen(*pedalSource, PlayStyle::Slide) == tailString,
           "a sustain-only source was mistaken for a held finger");

    auto sustain = prepareCollision();
    expect(chosen(*sustain, PlayStyle::Sustain) == tailString,
           "a non-legato style abandoned the same-target repick fallback");

    auto repeated = prepareCollision(false);
    expect(chosen(*repeated, PlayStyle::Slide) == tailString,
           "a repeated held target was mistaken for a legato continuation");

    constexpr ElectryEngine::ExpressionId expressionId = 2;
    auto mpe = prepareCollision(true, false,
                                ElectryEngine::legacyExpressionId,
                                expressionId);
    expect(chosen(*mpe, PlayStyle::Slide, expressionId) == tailString,
           "the held-priority rule disturbed explicit MPE string ownership");

    auto sustained = prepareCollision(true, true);
    expect(chosen(*sustained, PlayStyle::Slide) == tailString,
           "a sustained same-target voice yielded to held legato");

    auto pending = prepareCollision();
    TestAccess::markPendingRepick(*pending, tailString);
    expect(chosen(*pending, PlayStyle::Slide) == tailString,
           "a pending same-target repick yielded to held legato");

    auto pendingSource = prepareCollision();
    TestAccess::markPendingRepick(*pendingSource, sourceString);
    expect(chosen(*pendingSource, PlayStyle::Slide) == sourceString,
           "a pending contact hid its held legato source");

    auto nearest = prepareCollision();
    nearest->noteOn(43, 0.8f);
    expect(TestAccess::stringForNote(*nearest, 43) == 1
               && chosen(*nearest, PlayStyle::Slide) == 1
               && TestAccess::chosenString(*nearest, differentTarget,
                                           PlayStyle::Slide) == 1,
           "held-tail priority replaced closest-string legato selection");

    // Hammer's established reach is strictly below ten frets. The E2 finger
    // is exactly ten frets from MIDI 50, so it may not displace the reachable
    // A2-string release tail merely because it is held.
    auto hammerReach = prepareCollision();
    expect(TestAccess::chosenString(*hammerReach, 50, PlayStyle::Hammer)
               == tailString,
           "held-tail priority extended Hammer reach to ten frets");
}

void testHammerOnLegatoContinuity()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);
    engine.reset();

    // Pick G3 on the E-string region, then hammer to A3.
    engine.noteOn(43, 0.7f);
    StereoBuffer buffer(static_cast<int>(2.2 * sampleRate));
    engine.process(buffer.left.data(), buffer.right.data(),
                   static_cast<int>(0.5 * sampleRate));

    const int stringBefore = TestAccess::stringForNote(engine, 43);
    expect(stringBefore >= 0, "picked note did not allocate a string");

    engine.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
    engine.noteOn(45, 0.7f);

    const int stringAfter = TestAccess::stringForNote(engine, 45);
    expect(stringAfter == stringBefore,
           "hammer-on did not continue on the same string");
    expect(engine.getActiveVoiceCount() == 1,
           "hammer-on created a second voice");

    const int transition = static_cast<int>(0.5 * sampleRate);
    engine.process(buffer.left.data() + transition,
                   buffer.right.data() + transition,
                   buffer.size() - transition);

    // The hammered note settles on the new pitch.
    const double expected = midiHz(45);
    const double measured = measureFrequency(
        buffer.left, static_cast<int>(1.2 * sampleRate),
        static_cast<int>(0.8 * sampleRate), sampleRate, expected);
    expect(std::abs(centsBetween(measured, expected)) < 10.0,
           "hammer-on did not settle on the target pitch");

    // No hard discontinuity at the hammer point: compare the largest
    // sample-to-sample step around the transition with the signal scale.
    float maximumStep = 0.0f;
    for (int i = transition - 32; i < transition + static_cast<int>(0.02 * sampleRate); ++i)
        maximumStep = std::max(maximumStep,
                               std::abs(buffer.left[static_cast<std::size_t>(i + 1)]
                                        - buffer.left[static_cast<std::size_t>(i)]));
    const float scale = peakAbs(buffer.left, transition - 4800, transition);
    expect(maximumStep < std::max(0.05f, 1.2f * scale),
           "hammer-on transition produced a hard discontinuity");
}

void testPullOffLegatoDirection()
{
    constexpr double sampleRate = 48000.0;
    constexpr int hammerStart = 50;
    constexpr int pullOffStart = 54;
    constexpr int targetNote = 52;
    constexpr int transition = static_cast<int>(0.25 * sampleRate);
    constexpr int totalSamples = static_cast<int>(0.65 * sampleRate);

    struct LegatoRender
    {
        StereoBuffer audio { totalSamples };
        TestAccess::VoiceSnapshot voice;
        int stringBefore { -1 };
        int stringAfter { -1 };
        int activeVoices { 0 };
        double internalSampleRate { 0.0 };
    };

    const auto render = [] (int start, int target,
                            PickStyle pickStyle = PickStyle::Down)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        engine.setParameters(parameters);
        engine.reset();

        LegatoRender result;
        result.internalSampleRate = TestAccess::internalSampleRate(engine);
        engine.noteOn(start, 0.7f);
        engine.process(result.audio.left.data(), result.audio.right.data(), transition);
        result.stringBefore = TestAccess::stringForNote(engine, start);

        engine.noteOn(ElectryEngine::firstKeyswitchNote
                          + static_cast<int>(pickStyle), 1.0f);
        engine.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
        engine.noteOn(target, 0.7f);
        result.stringAfter = TestAccess::stringForNote(engine, target);
        result.activeVoices = engine.getActiveVoiceCount();
        result.voice = TestAccess::snapshot(engine, result.stringAfter);
        engine.process(result.audio.left.data() + transition,
                       result.audio.right.data() + transition,
                       totalSamples - transition);
        return result;
    };

    const auto hammer = render(hammerStart, targetNote);
    const auto pullOff = render(pullOffStart, targetNote);
    const auto pullOffWithUpLatched = render(
        pullOffStart, targetNote, PickStyle::Up);
    expect(allFinite(pullOff.audio), "pull-off render became non-finite");
    expect(pullOff.audio.left == pullOffWithUpLatched.audio.left
               && pullOff.audio.right == pullOffWithUpLatched.audio.right,
           "the latched plectrum direction leaked into a pull-off");
    expect(pullOff.stringBefore >= 0
               && pullOff.stringAfter == pullOff.stringBefore
               && pullOff.stringAfter == hammer.stringAfter
               && pullOff.activeVoices == 1,
           "pull-off did not preserve the sounding string state");

    const double pullOffRms = rmsInRange(
        pullOff.audio.left, transition, transition + static_cast<int>(0.030 * sampleRate));
    expect(pullOffRms > 1.0e-5, "pull-off transition was inaudible");

    const double directionDifference = normalisedDifferenceRms(
        hammer.audio.left, pullOff.audio.left, transition,
        transition + static_cast<int>(0.030 * sampleRate));
    expect(directionDifference > 0.05,
           "pull-off audio was not directionally distinct from a hammer-on ("
               + std::to_string(directionDifference) + ")");

    expect(hammer.voice.verticalWeight > hammer.voice.horizontalWeight,
           "ascending hammer-on voicing changed");
    expect(pullOff.voice.horizontalWeight > pullOff.voice.verticalWeight,
           "descending legato did not use a lateral release");
    const float expectedProjectionRatio = hammer.voice.lastCompensatedPeriod
                                        / pullOff.voice.lastCompensatedPeriod;
#if ELECTRY_DECOUPLED_PICK_RELEASE
    constexpr float twoPi = 6.28318530717958647692f;
    const auto effectiveReleaseAmplitude = [] (const auto& result)
    {
        const float omega = twoPi
            / std::max(result.voice.lastCompensatedPeriod, 1.0f);
        return result.voice.excitationAmplitude
            * TestAccess::onePoleMagnitude(
                result.voice.excitationReleaseCoefficient, omega);
    };
    const float actualProjectionRatio = effectiveReleaseAmplitude(hammer)
                                      / effectiveReleaseAmplitude(pullOff);
#else
    const float actualProjectionRatio = hammer.voice.excitationAmplitude
                                      / pullOff.voice.excitationAmplitude;
#endif
    expect(std::abs(actualProjectionRatio / expectedProjectionRatio - 1.0f)
               < 2.0e-5f,
           "hammer/pull effective release amplitude did not follow the "
           "settled source target period (ratio "
               + std::to_string(actualProjectionRatio)
               + ", expected " + std::to_string(expectedProjectionRatio)
               + ")");
#if ! ELECTRY_ENERGY_ATTACK_PITCH
    const float expectedMidiRatio = static_cast<float>(
        midiHz(pullOffStart) / midiHz(hammerStart));
    expect(std::abs(expectedProjectionRatio / expectedMidiRatio - 1.0f)
               < 2.0e-5f,
           "settled hammer/pull source periods diverged from their MIDI "
           "ratio");
#endif
    expect(std::abs(pullOff.voice.excitationCombDelay
                    - pullOff.voice.lastCompensatedPeriod)
               < 1.0e-6f * pullOff.voice.lastCompensatedPeriod,
           "pull-off excitation did not originate at the live old-fret endpoint");
#if ! ELECTRY_ENERGY_ATTACK_PITCH
    const float expectedReleasedPeriod = static_cast<float>(
        pullOff.internalSampleRate / midiHz(pullOffStart));
    expect(std::abs(pullOff.voice.excitationCombDelay
                    - expectedReleasedPeriod)
               < 1.0e-5f * expectedReleasedPeriod,
           "pull-off image delay is not the source fret's physical period");
#endif

    const auto openPullOff = render(49, 45);
    expect(allFinite(openPullOff.audio)
               && openPullOff.stringBefore >= 0
               && openPullOff.stringAfter == openPullOff.stringBefore
               && openPullOff.activeVoices == 1
               && openPullOff.voice.fret == 0,
           "pull-off to an open string did not preserve the sounding string");
    expect(std::abs(openPullOff.voice.excitationCombDelay
                    - openPullOff.voice.lastCompensatedPeriod)
               < 1.0e-6f * openPullOff.voice.lastCompensatedPeriod,
           "open-string pull-off did not retain the live lifted-finger endpoint");
#if ! ELECTRY_ENERGY_ATTACK_PITCH
    const float expectedOpenReleasedPeriod = static_cast<float>(
        openPullOff.internalSampleRate / midiHz(49));
    expect(std::abs(openPullOff.voice.excitationCombDelay
                    - expectedOpenReleasedPeriod)
               < 1.0e-5f * expectedOpenReleasedPeriod,
           "open pull-off image delay is not the lifted fret's physical period");
#endif
    expect(rmsInRange(openPullOff.audio.left, transition,
                      transition + static_cast<int>(0.030 * sampleRate)) > 1.0e-5,
           "open-string pull-off transition was inaudible");

    const auto expectSettledPitch = [] (const LegatoRender& result, int midiNote,
                                        const char* label)
    {
        const int start = transition + static_cast<int>(0.18 * sampleRate);
        const int length = static_cast<int>(0.18 * sampleRate);
        const double expected = midiHz(midiNote);
        const double measured = measureFrequency(
            result.audio.left, start, length, sampleRate, expected);
        expect(std::abs(centsBetween(measured, expected)) < 10.0,
               std::string(label) + " did not settle on the target pitch");
    };
    expectSettledPitch(pullOff, targetNote, "fretted pull-off");
    expectSettledPitch(openPullOff, 45, "open-string pull-off");

    // A simultaneous second note makes the host use the chord allocator. It
    // must retain the same open-string continuation as the scalar path.
    ElectryEngine chordEngine;
    chordEngine.prepare(sampleRate, 512);
    chordEngine.setParameters(EngineParameters {});
    chordEngine.noteOn(49, 0.7f);
    const int chordStringBefore = TestAccess::stringForNote(chordEngine, 49);
    chordEngine.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
    const std::array chordEvents {
        ElectryEngine::NoteOnEvent { 45, 0.7f },
        ElectryEngine::NoteOnEvent { 64, 0.7f }
    };
    chordEngine.noteOnChord(chordEvents);
    const int chordStringAfter = TestAccess::stringForNote(chordEngine, 45);
    expect(chordStringBefore >= 0 && chordStringAfter == chordStringBefore
               && TestAccess::snapshot(chordEngine, chordStringAfter).fret == 0,
           "chord allocation displaced an open-string pull-off");

    float maximumStep = 0.0f;
    for (int i = transition - 32;
         i < transition + static_cast<int>(0.020 * sampleRate); ++i)
    {
        maximumStep = std::max(
            maximumStep,
            std::abs(pullOff.audio.left[static_cast<std::size_t>(i + 1)]
                     - pullOff.audio.left[static_cast<std::size_t>(i)]));
    }
    const float scale = peakAbs(pullOff.audio.left, transition - 4800, transition);
    expect(maximumStep < std::max(0.05f, 1.2f * scale),
           "pull-off transition produced a hard discontinuity");
}

#if ELECTRY_DECOUPLED_PICK_RELEASE
void testFingerReleaseUsesContactPeriodAcrossRates()
{
    constexpr int targetNote = 52;
    constexpr float twoPi = 6.28318530717958647692f;

    struct Contact
    {
        TestAccess::VoiceSnapshot voice;
        bool finite { false };
        float peak { 0.0f };
    };
    const auto renderContact = [=] (double sampleRate, int sourceNote)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        engine.setParameters(parameters);
        engine.reset();

        engine.noteOn(sourceNote, 0.7f);
        engine.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
        engine.noteOn(targetNote, 0.7f);
        const int stringIndex = TestAccess::stringForNote(engine, targetNote);

        Contact result;
        result.voice = TestAccess::snapshot(engine, stringIndex);
        StereoBuffer audio(static_cast<int>(0.080 * sampleRate));
        renderInto(engine, audio);
        result.finite = allFinite(audio);
        result.peak = peakAbs(audio.left);
        return result;
    };
    const auto effectiveAmplitude = [] (const Contact& contact)
    {
        const float omega = twoPi
            / std::max(contact.voice.lastCompensatedPeriod, 1.0f);
        return contact.voice.excitationAmplitude
            * TestAccess::onePoleMagnitude(
                contact.voice.excitationReleaseCoefficient, omega);
    };

    for (const double sampleRate : { 44100.0, 48000.0, 96000.0,
                                     192000.0, 384000.0 })
    {
        const Contact hammer = renderContact(sampleRate, 50);
        const Contact pullOff = renderContact(sampleRate, 54);
        const float expected = hammer.voice.lastCompensatedPeriod
                             / pullOff.voice.lastCompensatedPeriod;
        const float actual = effectiveAmplitude(hammer)
                           / effectiveAmplitude(pullOff);
        const std::string at = " at "
            + std::to_string(static_cast<int>(sampleRate)) + " Hz";
        expect(hammer.voice.valid && pullOff.voice.valid
                   && hammer.voice.lastCompensatedPeriod > 0.0f
                   && pullOff.voice.lastCompensatedPeriod > 0.0f,
               "finger release fixture did not retain its contact periods" + at);
        expect(std::isfinite(actual)
                   && std::abs(actual / expected - 1.0f) < 2.0e-5f,
               "finger release makeup used the written destination instead "
               "of the contact period" + at + " (ratio "
                   + std::to_string(actual) + ", expected "
                   + std::to_string(expected) + ")");
        expect(hammer.finite && pullOff.finite
                   && hammer.peak > 1.0e-5f && pullOff.peak > 1.0e-5f,
               "finger release became silent or non-finite" + at);
    }
}
#endif

void testHardPickingStaysInTune()
{
    // A single uncontrolled E1 recording cannot calibrate an independent
    // nonlinear pitch excursion for all eight strings. The former extrapolation
    // raised hard E1 by about 30 cents but B1/E2 by only 9-13 cents, leaving the
    // lowest metal chord roughly 20 cents sour for its first 200 ms. Pin the
    // audible contract instead: maximum velocity may change level, spectrum and
    // contact, but every isolated open string must keep the written pitch and
    // a simultaneously rendered low chord must keep those intervals together.
    constexpr double sampleRate = 48000.0;
    constexpr std::array<int, ElectryEngine::stringCount> openNotes {
        28, 35, 40, 45, 50, 55, 59, 64
    };
    constexpr int analysisStart = static_cast<int>(0.030 * sampleRate);
    constexpr int analysisLength = static_cast<int>(0.180 * sampleRate);

    const auto renderHard = [&] (int note, PlayStyle style)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.velocityAmount = 1.0f;
        engine.setParameters(parameters);
        return renderNote(engine, sampleRate, note, 1.0f, style, 0.40);
    };
    const auto scanFundamental = [&] (const StereoBuffer& audio, int note,
                                      int start, int length)
    {
        double best = midiHz(note);
        double bestMagnitude = -1.0;
        for (double cents = -60.0; cents <= 60.0; cents += 0.25)
        {
            const double frequency = midiHz(note)
                * std::pow(2.0, cents / 1200.0);
            const double magnitude = dftMagnitude(
                audio.left, start, length, sampleRate, frequency);
            if (magnitude > bestMagnitude)
            {
                bestMagnitude = magnitude;
                best = frequency;
            }
        }
        return best;
    };
    const auto fundamentalPeriodicity = [&] (const StereoBuffer& audio,
                                              int note, int start, int length)
    {
        double mean = 0.0;
        for (int i = 0; i < length; ++i)
            mean += audio.left[static_cast<std::size_t>(start + i)];
        mean /= static_cast<double>(length);

        const double nominal = midiHz(note);
        const double lowFrequency = nominal * std::pow(2.0, -60.0 / 1200.0);
        const double highFrequency = nominal * std::pow(2.0, 60.0 / 1200.0);
        const int firstLag = static_cast<int>(std::ceil(sampleRate / highFrequency));
        const int lastLag = static_cast<int>(std::floor(sampleRate / lowFrequency));
        double strongest = -1.0;
        for (int lag = firstLag; lag <= lastLag; ++lag)
        {
            double product = 0.0;
            double earlyPower = 0.0;
            double latePower = 0.0;
            for (int i = 0; i + lag < length; ++i)
            {
                const double early = audio.left[
                    static_cast<std::size_t>(start + i)] - mean;
                const double late = audio.left[
                    static_cast<std::size_t>(start + i + lag)] - mean;
                product += early * late;
                earlyPower += early * early;
                latePower += late * late;
            }
            strongest = std::max(
                strongest,
                product / std::sqrt(std::max(earlyPower * latePower, 1.0e-30)));
        }
        return strongest;
    };
    const auto harmonicErrorFor = [&] (int note, PlayStyle style)
    {
        const auto hard = renderHard(note, style);
        expect(rmsInRange(hard.left, analysisStart,
                          analysisStart + analysisLength) > 1.0e-5,
               "hard-pick tuning fixture rendered too little body at MIDI "
                   + std::to_string(note));
        const double measured = measureFrequency(
            hard.left, analysisStart, analysisLength, sampleRate, midiHz(note));
        return centsBetween(measured, midiHz(note));
    };

    std::array<double, ElectryEngine::stringCount> errors {};
    for (std::size_t index = 0; index < openNotes.size(); ++index)
    {
        const int note = openNotes[index];
        errors[index] = harmonicErrorFor(note, PlayStyle::Sustain);
        expect(std::abs(errors[index]) < 8.0,
               "a maximum-velocity open string is out of tune at MIDI "
                   + std::to_string(note) + " (" + std::to_string(errors[index])
                   + " cents)");
    }

    const auto [lowest, highest] = std::minmax_element(errors.begin(), errors.end());
    expect(*highest - *lowest < 6.0,
           "maximum-velocity open strings pull apart by "
               + std::to_string(*highest - *lowest) + " cents");
    std::cout << "PROBE maximum-velocity open-string tuning spread: "
              << *lowest << ".." << *highest << " cents\n";

    // Also exercise allocation, summing and the default played-string bridge
    // coupling. Isolated strings alone would not prove that the low voicing a
    // player actually hears survives the complete polyphonic path.
    ElectryEngine chordEngine;
    chordEngine.prepare(sampleRate, 512);
    EngineParameters chordParameters;
    chordParameters.pickNoise = 0.0f;
    chordParameters.fingerNoise = 0.0f;
    chordParameters.releaseNoise = 0.0f;
    chordParameters.artifactAmount = 0.0f;
    chordParameters.velocityAmount = 1.0f;
    chordEngine.setParameters(chordParameters);
    chordEngine.reset();
    chordEngine.noteOn(pickKeyswitch(PickStyle::Down), 1.0f);
    chordEngine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    for (int index = 0; index < 3; ++index)
        chordEngine.noteOn(openNotes[static_cast<std::size_t>(index)], 1.0f);
    StereoBuffer lowChord(static_cast<int>(0.40 * sampleRate));
    renderInto(chordEngine, lowChord);

    std::array<double, 3> chordErrors {};
    for (std::size_t index = 0; index < chordErrors.size(); ++index)
    {
        const int note = openNotes[index];
        chordErrors[index] = centsBetween(
            scanFundamental(lowChord, note, analysisStart, analysisLength),
            midiHz(note));
        expect(std::abs(chordErrors[index]) < 8.0,
               "the simultaneous maximum-velocity low chord is out of tune at "
               "MIDI " + std::to_string(note) + " ("
                   + std::to_string(chordErrors[index]) + " cents)");
    }
    const auto [chordLow, chordHigh] = std::minmax_element(
        chordErrors.begin(), chordErrors.end());
    expect(*chordHigh - *chordLow < 6.0,
           "the simultaneous maximum-velocity low chord pulls apart by "
               + std::to_string(*chordHigh - *chordLow) + " cents");
    std::cout << "PROBE simultaneous low-chord tuning spread: "
              << *chordLow << ".." << *chordHigh << " cents\n";

    // Mute and Dead are the priority hard-style articulations. Their dark,
    // shortened bodies still retain a measurable fundamental; they may not
    // reintroduce the same low-chord interval error through a style-specific
    // path.
    constexpr int mutedStart = static_cast<int>(0.080 * sampleRate);
    constexpr int mutedLength = static_cast<int>(0.250 * sampleRate);
    const auto fundamentalErrorFor = [&] (int note, PlayStyle style)
    {
        const auto hard = renderHard(note, style);
        expect(rmsInRange(hard.left, mutedStart,
                          mutedStart + mutedLength) > 1.0e-5,
               "hard-style tuning fixture rendered too little body at MIDI "
                   + std::to_string(note));
        const double periodicity = fundamentalPeriodicity(
            hard, note, mutedStart, mutedLength);
        expect(periodicity > 0.60,
               "hard-style tuning fixture lacks a tonal fundamental at MIDI "
                   + std::to_string(note) + " (periodicity "
                   + std::to_string(periodicity) + ")");
        return centsBetween(
            scanFundamental(hard, note, mutedStart, mutedLength), midiHz(note));
    };
    int hardStylePitchChecks = 0;
    for (const auto style : { PlayStyle::PalmMute, PlayStyle::Dead })
    {
        std::array<double, 3> lowChordErrors {};
        for (std::size_t index = 0; index < lowChordErrors.size(); ++index)
        {
            const int note = openNotes[index];
            lowChordErrors[index] = fundamentalErrorFor(note, style);
            ++hardStylePitchChecks;
            expect(std::abs(lowChordErrors[index]) < 8.0,
                   "a maximum-velocity hard-style open string is out of tune "
                   "at MIDI " + std::to_string(note) + " ("
                       + std::to_string(lowChordErrors[index]) + " cents)");
        }
        const auto [styleLow, styleHigh] = std::minmax_element(
            lowChordErrors.begin(), lowChordErrors.end());
        expect(*styleHigh - *styleLow < 6.0,
               "maximum-velocity hard-style low strings pull apart by "
                   + std::to_string(*styleHigh - *styleLow) + " cents");
    }
    expect(hardStylePitchChecks == 6,
           "the hard-style tuning matrix did not cover all six low-string cases");
}

#if ELECTRY_ENERGY_ATTACK_PITCH
void testEnergyAttackPitchExperiment()
{
    using AttackState = TestAccess::AttackPitchState;
    constexpr double sampleRate = 48000.0;
    constexpr float maximumCents = 7.0f;
    constexpr std::array<float, 4> velocities { 0.25f, 0.50f, 0.75f, 1.0f };

    EngineParameters parameters;
    parameters.sympatheticAmount = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    const auto tensionRatio = [] (const AttackState& state)
    {
        return state.tensionRatio;
    };
    const auto pitchCents = [] (const AttackState& state)
    {
        return 1200.0 * std::log2(std::max(
            static_cast<double>(state.frequencyFactor), 1.0));
    };
    const auto prepare = [&] (ElectryEngine& engine, double rate)
    {
        engine.prepare(rate, 512);
        engine.setParameters(parameters);
        engine.reset();
    };
    const auto commitReleasedPick = [&] (ElectryEngine& engine,
                                          int stringIndex)
    {
        int guard = 0;
        const int maximumContactSamples = static_cast<int>(
            0.020 * TestAccess::internalSampleRate(engine));
        while (TestAccess::snapshot(engine, stringIndex).excitationInContact
               && guard++ < maximumContactSamples)
        {
            const auto live = TestAccess::attackPitchState(engine, stringIndex);
            expect(live.tensionRatio >= 0.0f
                       && live.pendingEnergyJoules > 0.0f,
                   "energy-pitch state committed before physical pick release");
            TestAccess::renderOneInternalSample(engine);
        }
        const auto atRelease = TestAccess::snapshot(engine, stringIndex);
        const auto pending = TestAccess::attackPitchState(engine, stringIndex);
        expect(guard < maximumContactSamples && atRelease.excitationInRelease
                   && pending.tensionRatio == 0.0f
                   && pending.pendingEnergyJoules > 0.0f,
               "energy-pitch seed did not remain pending through Contact");
        TestAccess::renderOneInternalSample(engine);
        return TestAccess::attackPitchState(engine, stringIndex);
    };

    struct FreshStroke
    {
        AttackState state;
        AttackState seed;
        float verticalBefore { 0.0f };
        float horizontalBefore { 0.0f };
        float verticalAfter { 0.0f };
        float horizontalAfter { 0.0f };
        float scaleLength { 0.0f };
        float stringGauge { 0.0f };
    };
    const auto freshStroke = [&] (int note, float velocity, double rate,
                                   int controlPhaseOffset = 0)
    {
        ElectryEngine engine;
        prepare(engine, rate);
        for (int i = 0; i < controlPhaseOffset; ++i)
            TestAccess::renderOneInternalSample(engine);
        engine.noteOn(note, velocity);
        const int stringIndex = TestAccess::stringForNote(engine, note);
        const auto before = TestAccess::attackPitchState(engine, stringIndex);
        expect(stringIndex >= 0
                   && TestAccess::snapshot(engine, stringIndex)
                          .excitationInContact
                   && before.tensionRatio == 0.0f
                   && before.pendingEnergyJoules > 0.0f
                   && before.frequencyFactor == 1.0f,
               "fresh Sustain did not arm a release-only energy-pitch seed");
        const float verticalBefore = TestAccess::effectiveLoopFrequency(
            engine, stringIndex, false, true);
        const float horizontalBefore = TestAccess::effectiveLoopFrequency(
            engine, stringIndex, true, true);
        const float scaleLength = TestAccess::scaleLengthMetres(engine);
        const float gauge = TestAccess::stringGauge(engine);
        const auto committed = commitReleasedPick(engine, stringIndex);
        return FreshStroke {
            committed,
            before,
            verticalBefore,
            horizontalBefore,
            TestAccess::effectiveLoopFrequency(
                engine, stringIndex, false, true),
            TestAccess::effectiveLoopFrequency(
                engine, stringIndex, true, true),
            scaleLength,
            gauge
        };
    };

    // E2 is the exact nominal endpoint of the usable ordinary EG-IPT cells.
    // Check Bank's energy-to-tension conversion, including the wound steel
    // core rather than the complete inertial diameter, before looking at
    // rendered pitch.
    const auto e2 = freshStroke(40, 1.0f, sampleRate);
    const float gaugeScale = 1.0f
        + (11.0f / 9.0f - 1.0f) * e2.stringGauge;
    const float totalDiameter = 1.0668e-3f * gaugeScale;
    const float coreDiameter = totalDiameter * 0.28f;
    constexpr float pi = 3.14159265358979323846f;
    constexpr float youngsModulus = 2.0e11f;
    const float coreArea = pi * 0.25f * coreDiameter * coreDiameter;
    const float expectedScale = youngsModulus * coreArea
        / (2.0f * e2.scaleLength * e2.state.tensionNewtons
           * e2.state.tensionNewtons);
    const float fullDiameterScale = expectedScale / (0.28f * 0.28f);
    const float q = tensionRatio(e2.state);
    const float maximumRatio = std::exp2(maximumCents / 600.0f) - 1.0f;
    const float expectedQ = std::min(
        e2.seed.pendingEnergyJoules
            * e2.seed.pendingElasticScalePerJoule,
        maximumRatio);
    expect(std::isfinite(e2.seed.pendingEnergyJoules)
               && e2.seed.pendingEnergyJoules > 0.0f
               && std::isfinite(e2.seed.pendingElasticScalePerJoule)
               && std::abs(e2.seed.pendingElasticScalePerJoule / expectedScale
                           - 1.0f) < 2.0e-5f
               && e2.seed.pendingElasticScalePerJoule
                      < 0.10f * fullDiameterScale,
           "Bank energy scale did not use the wound string's axial core");
    expect(q >= 0.0f
               && std::abs(q - expectedQ) < 2.0e-8f
               && std::abs(e2.state.frequencyFactor
                           - std::sqrt(1.0f + q)) < 2.0e-6f
               && pitchCents(e2.state) > 0.0
               && pitchCents(e2.state) <= maximumCents + 1.0e-3,
           "Bank energy-to-frequency law escaped its finite seven-cent bound");

    // The dynamic sounding-period correction is common to both
    // polarisations. Their established fixed split remains, but neither axis
    // may receive a different attack-pitch ratio.
    expect(std::abs(centsBetween(
               e2.verticalAfter,
               e2.verticalBefore * e2.state.frequencyFactor)) < 0.05
               && std::abs(centsBetween(
                      e2.horizontalAfter,
                      e2.horizontalBefore * e2.state.frequencyFactor)) < 0.05,
           "energy-pitch correction was not common to both polarisations");

    // Reset-identical strokes isolate MIDI force from deterministic player
    // variation. Growth is strict until the explicit safety clamp is reached;
    // saturation is allowed only at that bound.
    for (const int note : { 28, 35, 40 })
    {
        std::array<double, velocities.size()> ratios {};
        std::array<double, velocities.size()> cents {};
        for (std::size_t i = 0; i < velocities.size(); ++i)
        {
            const auto stroke = freshStroke(
                note, velocities[i], sampleRate).state;
            ratios[i] = tensionRatio(stroke);
            cents[i] = pitchCents(stroke);
            expect(std::isfinite(ratios[i]) && ratios[i] >= 0.0
                       && cents[i] <= maximumCents + 1.0e-3,
                   "velocity sweep produced unbounded energy-pitch state");
            if (i > 0)
            {
                const bool rose = ratios[i] > ratios[i - 1];
                const bool safelyClamped = cents[i] > maximumCents - 0.01
                    && std::abs(cents[i] - cents[i - 1]) < 0.01;
                expect(rose || safelyClamped,
                       "energy-pitch velocity response was not monotone");
            }
        }
        expect(ratios.front() < ratios.back(),
               "energy-pitch velocity range collapsed at MIDI "
                   + std::to_string(note));

        if (note == 40)
        {
            const double response = EngineParameters {}.velocityAmount;
            for (std::size_t i = 0; i + 1 < velocities.size(); ++i)
            {
                const double force = 0.05 + 0.95 * velocities[i];
                const double expected = std::pow(force, 2.0 * response);
                expect(std::abs(ratios[i] / ratios.back() - expected) < 2.0e-4,
                       "uncapped E2 energy did not follow squared pluck force");
            }
        }
    }

    // A pending seed ignores the control tick's phase. Once released, the
    // stored tension increment is non-increasing, follows the configured
    // Lee-derived exponential, and its physical-time decay agrees
    // across every supported production rate used by the regression suite.
    const auto phaseZero = freshStroke(40, 0.95f, sampleRate, 0).state;
    const auto phaseSeven = freshStroke(40, 0.95f, sampleRate, 7).state;
    expect(std::abs(centsBetween(phaseZero.frequencyFactor,
                                 phaseSeven.frequencyFactor)) < 1.0e-3,
           "energy-pitch seed depended on control-tick phase");

    std::array<double, 4> decayedCents {};
    std::size_t rateIndex = 0;
    for (const double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        ElectryEngine engine;
        prepare(engine, rate);
        engine.noteOn(40, 0.95f);
        const int stringIndex = TestAccess::stringForNote(engine, 40);
        auto state = commitReleasedPick(engine, stringIndex);
        const float initialRatio = state.tensionRatio;
        float previousRatio = state.tensionRatio;
        const int decaySamples = static_cast<int>(std::lround(
            0.120 * TestAccess::internalSampleRate(engine)));
        for (int sample = 0; sample < decaySamples; ++sample)
        {
            TestAccess::renderOneInternalSample(engine);
            state = TestAccess::attackPitchState(engine, stringIndex);
            expect(std::isfinite(state.tensionRatio)
                       && state.tensionRatio <= previousRatio * 1.000001f,
                   "released tension increment increased without contact");
            previousRatio = state.tensionRatio;
        }
        const float expectedRatio = initialRatio
            * std::exp(-0.120f / 0.3049f);
        expect(std::abs(state.tensionRatio / expectedRatio - 1.0f) < 0.002f,
               "energy-pitch relaxation missed its configured time constant");
        decayedCents[rateIndex++] = pitchCents(state);
    }
    const auto [minimumDecay, maximumDecay] = std::minmax_element(
        decayedCents.begin(), decayedCents.end());
    expect(*maximumDecay - *minimumDecay < 0.5,
           "energy-pitch relaxation changed with sample rate");

    // Only ordinary Sustain is identified by the frozen source comparison.
    // Other articulations remain exact no-seed paths, while a hammer or slide
    // on an already-ringing string carries (and later decays) its old energy.
    for (const auto style : { PlayStyle::PalmMute, PlayStyle::Dead,
                              PlayStyle::Harmonics, PlayStyle::Pinch })
    {
        ElectryEngine engine;
        prepare(engine, sampleRate);
        engine.noteOn(styleKeyswitch(style), 1.0f);
        engine.noteOn(40, 0.95f);
        const int stringIndex = TestAccess::stringForNote(engine, 40);
        auto state = TestAccess::attackPitchState(engine, stringIndex);
        expect(state.tensionRatio == 0.0f
                   && state.pendingEnergyJoules == 0.0f
                   && state.clearOnRelease
                   && state.frequencyFactor == 1.0f,
               "unsupported articulation "
                   + std::to_string(static_cast<int>(style))
                   + " acquired Sustain energy pitch (ratio "
                   + std::to_string(state.tensionRatio) + ", pending "
                   + std::to_string(state.pendingEnergyJoules) + ", factor "
                   + std::to_string(state.frequencyFactor) + ")");
        int guard = 0;
        while (TestAccess::snapshot(engine, stringIndex).excitationInContact
               && guard++ < static_cast<int>(0.020 * sampleRate))
            TestAccess::renderOneInternalSample(engine);
        TestAccess::renderOneInternalSample(engine);
        state = TestAccess::attackPitchState(engine, stringIndex);
        expect(state.tensionRatio == 0.0f
                   && ! state.clearOnRelease
                   && state.frequencyFactor == 1.0f,
               "unsupported articulation did not clear at physical release");
    }

    ElectryEngine legato;
    prepare(legato, sampleRate);
    legato.noteOn(40, 0.95f);
    const int legatoString = TestAccess::stringForNote(legato, 40);
    const auto beforeLegato = commitReleasedPick(legato, legatoString);
    legato.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
    legato.noteOn(41, 0.8f);
    const auto afterHammer = TestAccess::attackPitchState(
        legato, legatoString);
    legato.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
    legato.noteOn(42, 0.8f);
    const auto afterSlide = TestAccess::attackPitchState(
        legato, legatoString);
    expect(afterHammer.pendingEnergyJoules == 0.0f
               && afterSlide.pendingEnergyJoules == 0.0f
               && std::abs(tensionRatio(afterHammer)
                           - tensionRatio(beforeLegato)) < 2.0e-7f
               && std::abs(tensionRatio(afterSlide)
                           - tensionRatio(afterHammer)) < 2.0e-7f,
           "hammer or legato slide created fresh energy-pitch work");

    // A finger can land during the preceding pick's Contact. The finger's
    // Release must not be mistaken for that now-cancelled plectrum release.
    for (const auto style : { PlayStyle::Hammer, PlayStyle::Slide })
    {
        ElectryEngine interrupted;
        prepare(interrupted, sampleRate);
        interrupted.noteOn(40, 0.95f);
        const int interruptedString = TestAccess::stringForNote(
            interrupted, 40);
        const auto armed = TestAccess::attackPitchState(
            interrupted, interruptedString);
        expect(armed.pendingEnergyJoules > 0.0f,
               "interrupted-contact fixture did not arm a pick seed");
        interrupted.noteOn(styleKeyswitch(style), 1.0f);
        interrupted.noteOn(41, 0.80f);
        auto cancelled = TestAccess::attackPitchState(
            interrupted, interruptedString);
        expect(cancelled.tensionRatio == 0.0f
                   && cancelled.pendingEnergyJoules == 0.0f
                   && cancelled.pendingElasticScalePerJoule == 0.0f
                   && ! cancelled.clearOnRelease,
               "legato contact retained an interrupted plectrum seed");
        TestAccess::renderOneInternalSample(interrupted);
        cancelled = TestAccess::attackPitchState(
            interrupted, interruptedString);
        expect(cancelled.tensionRatio == 0.0f,
               "finger Release committed an interrupted plectrum seed");
    }

    ElectryEngine delayed;
    auto delayedParameters = parameters;
    delayedParameters.strumSpreadSeconds = 0.020f;
    delayed.prepare(sampleRate, 512);
    delayed.setParameters(delayedParameters);
    delayed.reset();
    delayed.noteOn(40, 0.95f);
    StereoBuffer establish(static_cast<int>(0.080 * sampleRate));
    renderInto(delayed, establish);
    const int delayedString = TestAccess::stringForNote(delayed, 40);
    delayed.noteOn(40, 0.70f);
    auto delayedVoice = TestAccess::snapshot(delayed, delayedString);
    auto delayedState = TestAccess::attackPitchState(delayed, delayedString);
    expect(delayedVoice.startDelaySamples > 0
               && delayedState.pendingEnergyJoules == 0.0f,
           "scheduling a delayed repick changed energy before contact");
    while (TestAccess::snapshot(delayed, delayedString).startDelaySamples > 1)
    {
        TestAccess::renderOneInternalSample(delayed);
        delayedState = TestAccess::attackPitchState(delayed, delayedString);
        expect(delayedState.pendingEnergyJoules == 0.0f,
               "delayed repick armed energy before physical contact");
    }
    TestAccess::renderOneInternalSample(delayed);
    delayedVoice = TestAccess::snapshot(delayed, delayedString);
    delayedState = TestAccess::attackPitchState(delayed, delayedString);
    expect(delayedVoice.excitationInContact
               && delayedState.pendingEnergyJoules > 0.0f,
           "physical delayed contact did not arm its release seed");

    int contactGuard = 0;
    while (TestAccess::snapshot(delayed, delayedString).excitationInContact
           && contactGuard++ < static_cast<int>(0.020 * sampleRate))
        TestAccess::renderOneInternalSample(delayed);
    const auto beforeRepickCommit = TestAccess::attackPitchState(
        delayed, delayedString);
    const float expectedRepickRatio = std::min(
        std::max(beforeRepickCommit.tensionRatio,
                 beforeRepickCommit.pendingEnergyJoules
                     * beforeRepickCommit.pendingElasticScalePerJoule),
        maximumRatio);
    TestAccess::renderOneInternalSample(delayed);
    const auto afterRepickCommit = TestAccess::attackPitchState(
        delayed, delayedString);
    expect(contactGuard < static_cast<int>(0.020 * sampleRate)
               && afterRepickCommit.pendingEnergyJoules == 0.0f
               && std::abs(afterRepickCommit.tensionRatio
                               - expectedRepickRatio) < 2.0e-6f,
           "repick release did not retain the larger bounded tension seed");

    legato.reset();
    const auto cleared = TestAccess::attackPitchState(legato, legatoString);
    expect(cleared.tensionRatio == 0.0f
               && cleared.pendingEnergyJoules == 0.0f
               && cleared.pendingElasticScalePerJoule == 0.0f
               && ! cleared.clearOnRelease
               && cleared.frequencyFactor == 1.0f,
           "reset retained candidate energy-pitch state");
}
#endif

void testAttackStateTransitions()
{
    constexpr double sampleRate = 48000.0;
    const auto quietParameters = []
    {
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.velocityAmount = 1.0f;
        return parameters;
    };

    // A stolen string that jumps an octave is choked in amplitude. The delay
    // line and every recursive loop filter are one ringing state, so all must
    // retain the same 0.28 amplitude; the retirement follower carries squared
    // amplitude and must retain 0.28^2 energy. Delay the new contact by one
    // sample so its excitation cannot hide an incomplete choke.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        auto parameters = quietParameters();
        parameters.palmMute = 0.55f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(30, 1.0f);
        StereoBuffer ringing(static_cast<int>(0.080 * sampleRate));
        renderInto(engine, ringing);
        const float outputBefore = TestAccess::voiceOutputEnergy(engine, 0);
        std::array<double, 2> lineBefore {};
        std::array<std::array<float, 9>, 2> filtersBefore {};
        std::array<std::array<double, 2>, 2> handDipBefore {};
#if ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2
        std::array<std::array<double, 2>, 2> fittedDipBefore {};
#endif
        for (std::size_t axis = 0; axis < 2; ++axis)
        {
            const bool horizontal = axis == 1;
            lineBefore[axis] =
                TestAccess::loopLineEnergy(engine, 0, horizontal);
            filtersBefore[axis] =
                TestAccess::loopFilterState(engine, 0, horizontal);
            handDipBefore[axis] =
                TestAccess::loopHandDipState(engine, 0, horizontal);
#if ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2
            fittedDipBefore[axis] =
                TestAccess::loopFittedLossDipState(engine, 0, horizontal);
#endif
        }
#if ELECTRY_ENERGY_ATTACK_PITCH
        const float pitchRatioBefore =
            TestAccess::attackPitchState(engine, 0).tensionRatio;
#endif

        TestAccess::retriggerVoice(
            engine, 0, 42, 0.01f, PlayStyle::Sustain, 1);

        constexpr float retainedAmplitude = 0.28f;
        constexpr float retainedEnergy = 0.28f * 0.28f;
        const auto stolen = TestAccess::snapshot(engine, 0);
        const float outputAfter = TestAccess::voiceOutputEnergy(engine, 0);
        expect(stolen.midiNote == 42 && stolen.startDelaySamples == 1,
               "the hard-to-soft jump did not exercise the oldest string");
        expect(outputBefore > 0.0f,
               "the hard note had no follower energy to choke");
        expect(std::abs(outputAfter / outputBefore - retainedEnergy) < 1.0e-4f,
               "a large pitch jump did not scale the retirement follower by "
                   "0.28 squared");
        for (std::size_t axis = 0; axis < 2; ++axis)
        {
            const bool horizontal = axis == 1;
            const std::string axisName = horizontal ? "horizontal" : "vertical";
            const double lineAfter =
                TestAccess::loopLineEnergy(engine, 0, horizontal);
            expect(lineBefore[axis] > 0.0
                       && std::abs(lineAfter / lineBefore[axis]
                                       - retainedEnergy) < 1.0e-6,
                   "the " + axisName
                       + " delay line escaped the large-jump choke");

            const auto filtersAfter =
                TestAccess::loopFilterState(engine, 0, horizontal);
            for (std::size_t state = 0; state < filtersAfter.size(); ++state)
            {
                expect(filtersBefore[axis][state] != 0.0f,
                       "the " + axisName
                           + " loop-filter fixture was not populated");
                expect(filtersAfter[state]
                           == retainedAmplitude * filtersBefore[axis][state],
                       "the " + axisName
                           + " loop-filter state escaped the large-jump choke");
            }

            const auto handDipAfter =
                TestAccess::loopHandDipState(engine, 0, horizontal);
            for (std::size_t state = 0; state < handDipAfter.size(); ++state)
            {
                expect(handDipBefore[axis][state] != 0.0,
                       "the " + axisName
                           + " hand-dip fixture was not populated");
                expect(handDipAfter[state]
                           == retainedAmplitude * handDipBefore[axis][state],
                       "the " + axisName
                           + " hand-dip state escaped the large-jump choke");
            }
#if ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2
            const auto fittedDipAfter =
                TestAccess::loopFittedLossDipState(engine, 0, horizontal);
            for (std::size_t state = 0; state < fittedDipAfter.size(); ++state)
            {
                expect(fittedDipBefore[axis][state] != 0.0,
                       "the " + axisName
                           + " fitted-loss fixture was not populated");
                expect(fittedDipAfter[state]
                           == retainedAmplitude * fittedDipBefore[axis][state],
                       "the " + axisName
                           + " fitted-loss state escaped the large-jump choke");
            }
#endif
        }
#if ELECTRY_ENERGY_ATTACK_PITCH
        const float pitchRatioAfter =
            TestAccess::attackPitchState(engine, 0).tensionRatio;
        expect(pitchRatioBefore > 0.0f
                   && std::abs(pitchRatioAfter / pitchRatioBefore
                                   - retainedEnergy) < 1.0e-4f,
               "a stolen string did not attenuate its tension increment with "
               "the retained transverse energy");
#endif
    }

    // A delayed repick is still the old ringing stroke until the pick reaches
    // it. All audible state must remain exact through that pre-roll, then the
    // reserved attack must still arrive at contact.
    {
        ElectryEngine control;
        ElectryEngine pendingRepick;
        auto parameters = quietParameters();
        parameters.strumSpreadSeconds = 0.020f;
        for (auto* engine : { &control, &pendingRepick })
        {
            engine->prepare(sampleRate, 512);
            engine->setParameters(parameters);
            engine->reset();
            engine->noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
            engine->noteOn(28, 1.0f);
            StereoBuffer ringing(static_cast<int>(0.080 * sampleRate));
            renderInto(*engine, ringing);
        }
        pendingRepick.noteOn(28, 0.20f);
        const auto pending = TestAccess::snapshot(pendingRepick, 0);
        expect(pending.startDelaySamples
                   > static_cast<int>(0.005 * sampleRate),
               "the same-note repick was not delayed for the regression");

        StereoBuffer controlWaiting(static_cast<int>(0.005 * sampleRate));
        StereoBuffer repickWaiting(static_cast<int>(0.005 * sampleRate));
        renderInto(control, controlWaiting);
        renderInto(pendingRepick, repickWaiting);
        expect(TestAccess::snapshot(pendingRepick, 0).startDelaySamples > 0,
               "the delayed repick arrived before its scheduled contact");
        expect(controlWaiting.left == repickWaiting.left
                   && controlWaiting.right == repickWaiting.right,
               "scheduling a delayed Palm repick changed the old ring before "
               "pick contact");

        StereoBuffer controlThroughContact(static_cast<int>(0.030 * sampleRate));
        StereoBuffer repickThroughContact(static_cast<int>(0.030 * sampleRate));
        renderInto(control, controlThroughContact);
        renderInto(pendingRepick, repickThroughContact);
        expect(TestAccess::snapshot(pendingRepick, 0).startDelaySamples == 0
                   && repickThroughContact.left != controlThroughContact.left,
               "the staged Palm repick never committed at pick contact");
    }

    // A fretting-hand move can land while a strummed repick is already
    // travelling toward that string. The finger changes the speaking length;
    // it cannot make the reserved plectrum disappear. Its captured stroke must
    // reach the latest moving pitch after the original remaining travel time.
    for (const auto legatoStyle : { PlayStyle::Hammer, PlayStyle::Slide })
    {
        ElectryEngine engine;
        auto parameters = quietParameters();
        parameters.strumSpreadSeconds = 0.040f;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
        constexpr std::array<ElectryEngine::NoteOnEvent, 2> chord {{
            { 28, 0.85f }, { 35, 0.85f }
        }};
        engine.noteOnChord(chord);
        StereoBuffer establish(static_cast<int>(0.080 * sampleRate));
        renderInto(engine, establish);
        engine.noteOnChord(chord);
        const auto reserved = TestAccess::snapshot(engine, 1);
        const int factor = TestAccess::oversamplingFactor(engine);
        const int moveLeadFrames = static_cast<int>(0.005 * sampleRate);
        const int approachFrames = std::max(
            0, reserved.startDelaySamples / factor - moveLeadFrames);
        StereoBuffer approach(approachFrames);
        renderInto(engine, approach);
        const auto travelling = TestAccess::snapshot(engine, 1);
        expect(travelling.pendingRepickActive
                   && travelling.startDelaySamples > 0
                   && travelling.strumChordId != 0
                   && travelling.pendingContactPreservesRing
                   && travelling.pendingPlayStyle == PlayStyle::Sustain,
               "the legato/repick fixture had no travelling plectrum");

        engine.noteOn(styleKeyswitch(legatoStyle), 1.0f);
        engine.noteOn(37, 0.80f);
        const auto moved = TestAccess::snapshot(engine, 1);
        expect(moved.midiNote == 37 && moved.pendingRepickActive,
               "a same-string legato move erased the travelling plectrum");
        expect(moved.startDelaySamples == travelling.startDelaySamples
                   && moved.strumChordId == travelling.strumChordId
                   && moved.pendingContactPreservesRing,
               "a same-string legato move rewrote the travelling pick");

        const int framesBeforeContact =
            (moved.startDelaySamples - 1) / factor;
        StereoBuffer beforeContact(framesBeforeContact);
        renderInto(engine, beforeContact);
        const auto nearlyThere = TestAccess::snapshot(engine, 1);
        const float blendBeforeContact = TestAccess::legatoBlend(engine, 1);
        expect(nearlyThere.pendingRepickActive
                   && nearlyThere.startDelaySamples > 0
                   && nearlyThere.startDelaySamples <= factor
                   && blendBeforeContact > 0.0f
                   && blendBeforeContact < 1.0f,
               "the legato/repick fixture missed the pre-contact boundary");

        StereoBuffer contact(1);
        renderInto(engine, contact);
        const auto contacted = TestAccess::snapshot(engine, 1);
        expect(contacted.midiNote == 37 && ! contacted.pendingRepickActive
                   && contacted.startDelaySamples == 0
                   && contacted.playStyle == PlayStyle::Sustain
                   && contacted.excitationInContact,
               "the travelling pick did not contact the legato target fret");
        expect(TestAccess::legatoBlend(engine, 1) >= blendBeforeContact
                   && TestAccess::legatoBlend(engine, 1) < 1.0f,
               "pick contact snapped an unfinished legato glide to its fret");

        // The pick and its contact patch are fixed distances from the bridge.
        // If the plectrum reaches the string during a fret glide, convert those
        // metres with the fractional fret under the finger at contact, not the
        // written destination that the glide has not reached yet.
        const float contactBlend = TestAccess::legatoBlend(engine, 1);
        const float contactOffset = 12.0f * std::log2(
            TestAccess::legatoFromFrequency(engine, 1)
            / contacted.baseFrequency)
            * (1.0f - electry::smoothStep(contactBlend));
        const float liveFret = static_cast<float>(contacted.fret)
                             + contactOffset;
        const float fretStretch = std::exp2(liveFret / 12.0f);
        const float openLength = TestAccess::scaleLengthMetres(engine);
        const float openFraction = electry::lerp(
            0.025f, 0.48f, parameters.pickPosition)
            + contacted.strokeContactOffsetMetres / openLength;
        const float expectedCentre = electry::clampf(
                                       openFraction * fretStretch,
                                       0.02f, 0.98f)
                                   * contacted.lastCompensatedPeriod;
        const float contactMetres = 0.001f * electry::lerp(
            1.5f, 0.5f, parameters.pickHardness);
        const float expectedHalfWidth = 0.5f
            * contactMetres / std::max(openLength / fretStretch, 0.05f)
            * contacted.lastCompensatedPeriod;
        expect(std::abs(contacted.excitationCombDelay - expectedCentre)
                       < 1.0e-6f * expectedCentre
                   && std::abs(contacted.excitationCombWidth
                               - expectedHalfWidth)
                       < 1.0e-6f * expectedHalfWidth,
               "a travelling pick used the written fret instead of its "
               "performed metre geometry");

        engine.noteOff(35);
        expect(TestAccess::snapshot(engine, 1).keyDown,
               "the old fret released the moved string");
        engine.noteOff(37);
        expect(TestAccess::snapshot(engine, 1).releasing,
               "the moved fret did not retain release ownership");
    }

    // A fully damped held string has no PendingRepick until a legato gesture
    // gives its delayed revival a ringing state. Promote that reserved contact
    // before the finger overwrites its captured wrist data.
    {
        ElectryEngine engine;
        auto parameters = quietParameters();
        parameters.strumSpreadSeconds = 0.040f;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(styleKeyswitch(PlayStyle::Dead), 1.0f);
        engine.noteOn(35, 0.90f);
        StereoBuffer retire(static_cast<int>(2.50 * sampleRate));
        renderInto(engine, retire);
        expect(! TestAccess::snapshot(engine, 1).active,
               "the held Dead fixture did not retire before its revival");

        engine.noteOn(ElectryEngine::firstRepickNote + 1, 0.95f);
        const auto reserved = TestAccess::snapshot(engine, 1);
        expect(reserved.active && reserved.startDelaySamples > 0
                   && ! reserved.pendingRepickActive
                   && reserved.playStyle == PlayStyle::Dead,
               "the retired string did not reserve a fresh delayed pick");
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(47, 0.80f);
        const auto moved = TestAccess::snapshot(engine, 1);
        expect(moved.midiNote == 47 && moved.pendingRepickActive
                   && moved.pendingPlayStyle == PlayStyle::Dead
                   && moved.pendingStrokeIsUp == reserved.strokeIsUp
                   && moved.pendingStrokeVariationState
                          == reserved.strokeVariationState
                   && moved.pendingStartOrder == reserved.startOrder
                   && moved.startDelaySamples == reserved.startDelaySamples
                   && moved.strumChordId == reserved.strumChordId
                   && moved.pendingContactPreservesRing,
               "legato lost a retired string's reserved plectrum state");

        const int factor = TestAccess::oversamplingFactor(engine);
        StereoBuffer beforeContact((moved.startDelaySamples - 1) / factor);
        renderInto(engine, beforeContact);
        expect(TestAccess::snapshot(engine, 1).pendingRepickActive
                   && TestAccess::legatoBlend(engine, 1) < 1.0f,
               "the retired-string contact did not remain pending mid-slide");
        StereoBuffer contact(1);
        renderInto(engine, contact);
        const auto contacted = TestAccess::snapshot(engine, 1);
        expect(contacted.midiNote == 47 && ! contacted.pendingRepickActive
                   && contacted.playStyle == PlayStyle::Dead
                   && contacted.excitationInContact
                   && TestAccess::legatoBlend(engine, 1) < 1.0f,
               "the retired string's pick missed its moving target fret");
    }

    // A released/refretted note reserves a new finger as well as a later pick.
    // If legato moves it before contact, assign that new finger at the move and
    // then preserve it through the plectrum; never revive the released one.
    {
        ElectryEngine engine;
        auto parameters = quietParameters();
        parameters.strumSpreadSeconds = 0.040f;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(37, 0.85f);
        StereoBuffer establish(static_cast<int>(0.10 * sampleRate));
        renderInto(engine, establish);
        const auto oldFinger = TestAccess::snapshot(engine, 1);
        expect(oldFinger.active && oldFinger.midiNote == 37,
               "the delayed-refret finger fixture missed physical string 1");
        engine.noteOff(37);
        engine.noteOn(37, 0.82f);
        const auto refret = TestAccess::snapshot(engine, 1);
        expect(refret.pendingRepickActive
                   && refret.startDelaySamples > 0
                   && ! refret.pendingPreservesVibratoFinger,
               "the delayed refret did not reserve a fresh finger");

        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(49, 0.80f);
        const auto moved = TestAccess::snapshot(engine, 1);
        expect(moved.pendingRepickActive
                   && moved.pendingPreservesVibratoFinger
                   && moved.vibratoSeed != oldFinger.vibratoSeed,
               "legato reused the released finger before pending contact");
        const auto movedSeed = moved.vibratoSeed;
        const int factor = TestAccess::oversamplingFactor(engine);
        StereoBuffer throughContact(
            (moved.startDelaySamples - 1) / factor + 1);
        renderInto(engine, throughContact);
        const auto contacted = TestAccess::snapshot(engine, 1);
        expect(! contacted.pendingRepickActive
                   && contacted.vibratoSeed == movedSeed
                   && TestAccess::legatoBlend(engine, 1) < 1.0f,
               "pending contact replaced the newly assigned moving finger");
    }
}

void testPickupsToneAndBuildMorph()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    const int start = static_cast<int>(0.05 * sampleRate);
    const int window = static_cast<int>(0.4 * sampleRate);

    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.artifactAmount = 0.0f;

    parameters.pickupSelector = PickupSelector::Bridge;
    engine.setParameters(parameters);
    const auto bridge = renderNote(engine, sampleRate, 45, 0.7f,
                                   PlayStyle::Sustain, 0.8);

    parameters.pickupSelector = PickupSelector::Neck;
    engine.setParameters(parameters);
    const auto neck = renderNote(engine, sampleRate, 45, 0.7f,
                                 PlayStyle::Sustain, 0.8);

    const double f0 = midiHz(45);
    // The bridge position senses far less of the fundamental's antinode, so
    // its energy-weighted centroid sits clearly higher than the neck's.
    const double bridgeCentroid = spectralCentroid(bridge.left, start, window,
                                                   sampleRate, f0);
    const double neckCentroid = spectralCentroid(neck.left, start, window,
                                                 sampleRate, f0);
    expect(bridgeCentroid > neckCentroid * 1.08,
           "bridge pickup is not brighter than neck pickup (bridge "
               + std::to_string(bridgeCentroid) + " Hz, neck "
               + std::to_string(neckCentroid) + " Hz)");

    // Rolling the tone control off darkens the output.
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.toneKnob = 0.05f;
    engine.setParameters(parameters);
    const auto dark = renderNote(engine, sampleRate, 45, 0.7f,
                                 PlayStyle::Sustain, 0.8);
    const double openRatio = highBandRatio(bridge.left, start, window,
                                           sampleRate, f0);
    const double darkRatio = highBandRatio(dark.left, start, window,
                                           sampleRate, f0);
    expect(darkRatio < openRatio * 0.6,
           "tone control does not darken the pickup output (open ratio "
               + std::to_string(openRatio) + ", rolled "
               + std::to_string(darkRatio) + ")");

    // Build changes construction only. Pickup construction remains fixed and
    // independently playable, so this comparison cannot buy its contrast by
    // silently switching a humbucker into a single coil.
    EngineParameters slabBuild;
    slabBuild.pickupType = 0.32f;
    slabBuild.toneKnob = 0.8f;
    slabBuild.bodyResonance = 0.55f;
    slabBuild.pickNoise = 0.0f;
    applyGuitarBuild(slabBuild, 0.0f);
    engine.setParameters(slabBuild);
    const auto slabRender = renderNote(engine, sampleRate, 45, 0.7f,
                                       PlayStyle::Sustain, 0.8);

    auto continuousBuild = slabBuild;
    applyGuitarBuild(continuousBuild, 1.0f);
    engine.setParameters(continuousBuild);
    const auto continuousRender = renderNote(engine, sampleRate, 45, 0.7f,
                                             PlayStyle::Sustain, 0.8);

    const double buildDifference = normalisedDifferenceRms(
        slabRender.left, continuousRender.left, start, start + window);
#if ELECTRY_MEASURED_BODY_RESPONSE
    expect(buildDifference > 0.05 && buildDifference < 0.50,
           "measured Guitar Build endpoints escaped the subtle structural range ("
               + std::to_string(buildDifference) + ")");
#else
    expect(buildDifference > 0.08,
           "Guitar Build endpoints are not audibly distinct ("
               + std::to_string(buildDifference) + ")");
#endif

    // Both endpoints stay in tune.
    for (const auto* render : { &slabRender, &continuousRender })
    {
        const double measured = measureFrequency(
            render->left, static_cast<int>(0.3 * sampleRate),
            static_cast<int>(0.4 * sampleRate), sampleRate, midiHz(45));
        expect(std::abs(centsBetween(measured, midiHz(45))) < 8.0,
               "guitar-model endpoint detuned the instrument");
    }
}

void testArtifactsControl()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    // Pinned for the same reason as testMaterialAndControlAudibility: the bounds
    // below say the artifact layer is audible but subtle, and "how audible" is
    // measured relative to the note it sits on. On the shipped defaults - a thick
    // blank, heaviest set, tone back - the same 0.18 measures 0.0019 against this
    // 0.002 floor, not because the artifact path changed but because the note it
    // is compared against is darker and louder. Raising the Artifacts default
    // would restore the ratio; that is a voicing decision, not this test's.
    EngineParameters parameters;
    parameters.bodyWood = 0.5f;
    parameters.bodySize = 0.5f;
    parameters.bodyShape = 0.5f;
    parameters.construction = 0.5f;
    parameters.scaleLength = 0.5f;
    parameters.pickupType = 0.5f;
    parameters.toneKnob = 0.8f;
    parameters.stringGauge = 0.5f;
    parameters.stringAge = 0.15f;
    parameters.pickPosition = 0.35f;
    parameters.pickHardness = 0.6f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    const auto renderAt = [&] (float amount)
    {
        parameters.artifactAmount = amount;
        engine.setParameters(parameters);
        return renderNote(engine, sampleRate, 45, 0.95f,
                          PlayStyle::Sustain, 0.75);
    };

    const auto clean = renderAt(0.0f);
    const auto subtle = renderAt(0.18f);
    const auto medium = renderAt(0.60f);
    const auto full = renderAt(1.0f);
    const int start = static_cast<int>(0.020 * sampleRate);
    const int end = static_cast<int>(0.60 * sampleRate);
    const double subtleDifference = normalisedDifferenceRms(
        subtle.left, clean.left, start, end);
    const double mediumDifference = normalisedDifferenceRms(
        medium.left, clean.left, start, end);
    const double fullDifference = normalisedDifferenceRms(
        full.left, clean.left, start, end);

    expect(subtleDifference > 0.002 && subtleDifference < 0.15,
           "default artifacts are inaudible or no longer subtle (difference "
               + std::to_string(subtleDifference) + ")");
    expect(mediumDifference > subtleDifference * 1.35
               && fullDifference > mediumDifference * 1.12,
           "Artifacts control does not increase imperfection energy monotonically ("
               + std::to_string(subtleDifference) + ", "
               + std::to_string(mediumDifference) + ", "
               + std::to_string(fullDifference) + ")");

    const auto repeatedFull = renderAt(1.0f);
    expect(full.left == repeatedFull.left && full.right == repeatedFull.right,
           "artifact PRNG path is not sample-deterministic");

    // A maximum-force low string must actually reach the collision branch.
    // The PRNG stream is seeded before this snapshot and advances only when
    // the string exceeds the configured fret clearance, so this catches a
    // collision window that expires while the travelling wave is still zero.
    for (const auto style : { PlayStyle::PalmMute, PlayStyle::Dead })
    {
        for (const int note : { 28, 40 })
        {
            ElectryEngine collision;
            collision.prepare(sampleRate, 512);
            collision.setParameters(parameters);
            collision.reset();
            collision.noteOn(styleKeyswitch(style), 1.0f);
            collision.noteOn(note, 1.0f);
            const int stringIndex = TestAccess::stringForNote(collision, note);
            const auto before = TestAccess::snapshot(collision, stringIndex);
            StereoBuffer contact(static_cast<int>(0.150 * sampleRate));
            renderInto(collision, contact);
            const auto after = TestAccess::snapshot(collision, stringIndex);
            expect(after.artifactNoiseState != before.artifactNoiseState,
                   "maximum-force low-string artifact window made no fret contact (style "
                       + std::to_string(static_cast<int>(style)) + ", note "
                       + std::to_string(note) + ")");
        }
    }

    engine.reset();
    StereoBuffer silence(4096);
    renderInto(engine, silence);
    expect(peakAbs(silence.left) == 0.0f,
           "Artifacts control generated ambience without a played note");

    // Worst-case low-register strum remains bounded with every imperfection
    // path open and the mandatory oversampling clock active.
    engine.reset();
    constexpr std::array<int, ElectryEngine::stringCount> openNotes {
        28, 35, 40, 45, 50, 55, 59, 64
    };
    for (const int note : openNotes)
        engine.noteOn(note, 1.0f);
    StereoBuffer strum(static_cast<int>(0.8 * sampleRate));
    renderInto(engine, strum);
    expect(allFinite(strum) && peakAbs(strum.left) < 0.80f,
           "maximum-artifact eight-string strum became unstable");
}

#if ELECTRY_POSITIONED_FRET_COLLISION
void testPositionedFretCollision()
{
    const double delayScale = TestAccess::followingFretDelayScale();
    expect(std::abs(delayScale - std::exp2(-1.0 / 12.0)) < 1.0e-7,
           "following-fret collision tap does not follow equal temperament");
    const auto linearWeights =
        TestAccess::followingFretLinearWeights(123.25f);
    expect(std::abs(linearWeights[0] - 0.25f) < 1.0e-7f
               && std::abs(linearWeights[1] - 0.75f) < 1.0e-7f,
           "following-fret cached linear interpolation weights are wrong");
    expect(TestAccess::followingFretLinearReadSentinel(123.25f) == 8.0f,
           "following-fret production linear read reversed its tap geometry");

    // Exercise the production collision law with pure harmonic phases. At the
    // following-fret fraction, harmonic 9 is almost an antinode while harmonic
    // 18 is almost a node. The latter must survive instead of every partial
    // seeing the old seam-wide limiter.
    const double harmonic9Gain = TestAccess::positionedFretHarmonicGain(9);
    const double harmonic18Gain = TestAccess::positionedFretHarmonicGain(18);
    expect(harmonic9Gain < 0.25 && harmonic18Gain > 0.99
               && harmonic9Gain < harmonic18Gain * 0.30,
           "following-fret law did not preserve the node/antinode contrast (H9 "
               + std::to_string(harmonic9Gain) + ", H18 "
               + std::to_string(harmonic18Gain) + ")");
    for (int harmonic = 1; harmonic <= 32; ++harmonic)
        expect(TestAccess::positionedFretHarmonicGain(harmonic) <= 1.0001,
               "following-fret law amplified harmonic "
                   + std::to_string(harmonic));

    float excess = -1.0f;
    const float belowClearance =
        TestAccess::applyPositionedFretCollision(
            0.20f, -0.40f, 0.61f, 1.0f, excess);
    expect(belowClearance == 0.20f && excess == 0.0f,
           "following-fret collision changed a sample below clearance");

    const float zeroContact = TestAccess::applyPositionedFretCollision(
        0.50f, -0.50f, 0.10f, 0.0f, excess);
    expect(zeroContact == 0.50f,
           "following-fret loss was not identity at zero contact");

    const auto kneeCorrection = [&] (float amount)
    {
        constexpr float clearance = 0.10f;
        const float bridge = clearance + amount;
        float kneeExcess = 0.0f;
        const float output = TestAccess::applyPositionedFretCollision(
            bridge, 0.0f, clearance, 1.0f, kneeExcess);
        return bridge - output;
    };
    const float correction1 = kneeCorrection(0.001f);
    const float correction2 = kneeCorrection(0.002f);
    expect(correction1 > 0.0f
               && correction2 / correction1 > 3.8f
               && correction2 / correction1 < 4.1f,
           "following-fret loss no longer has a zero-slope quadratic knee");

    // The moving tap remains in the fractional delay's valid range at every
    // supported production rate, including the non-round 96.001 kHz seam.
    for (const double sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0,
                                     96001.0, 192000.0, 384000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 1.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(28, 1.0f);
        const int stringIndex = TestAccess::stringForNote(engine, 28);
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        const float tapDelay = snapshot.verticalDelayCurrent
                             * TestAccess::followingFretDelayScale();
        expect(tapDelay >= 4.0f
                   && tapDelay <= TestAccess::delayLineCapacity() - 8.0f,
               "following-fret tap escaped its delay line at "
                   + std::to_string(sampleRate) + " Hz");
        StereoBuffer contact(static_cast<int>(0.150 * sampleRate));
        renderInto(engine, contact);
        const auto after = TestAccess::snapshot(engine, stringIndex);
        expect(allFinite(contact) && peakAbs(contact.left) > 1.0e-5f,
               "following-fret contact render was silent or non-finite at "
                   + std::to_string(sampleRate) + " Hz");
        expect(after.artifactNoiseState != snapshot.artifactNoiseState,
               "following-fret contact branch was not reached at "
                   + std::to_string(sampleRate) + " Hz");
    }

    EngineParameters parameters;
    parameters.artifactAmount = 1.0f;
    parameters.bendTimeSeconds = 0.02f;
    ElectryEngine engine;
    engine.prepare(48000.0, 512);
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(85, 1.0f);
    const auto fret21 = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 85));
    expect(fret21.fret == 21 && fret21.artifactCollisionLength > 0
               && fret21.artifactCollisionRemaining
                    == fret21.artifactCollisionLength,
           "fret 21 did not arm its following-fret collision");

    // The cached point follows the smoothed physical period during its brief
    // contact opportunity instead of remaining at the attack coordinate.
    engine.reset();
    engine.noteOn(28, 1.0f);
    const int movingString = TestAccess::stringForNote(engine, 28);
    StereoBuffer establishTap(1024);
    renderInto(engine, establishTap);
    const auto beforeBend = TestAccess::snapshot(engine, movingString);
    const float cachedBeforeBend =
        TestAccess::followingFretCachedDelay(engine, movingString);
    engine.setPitchBend(1.0f);
    StereoBuffer moveTap(512);
    renderInto(engine, moveTap);
    const auto afterBend = TestAccess::snapshot(engine, movingString);
    const float cachedAfterBend =
        TestAccess::followingFretCachedDelay(engine, movingString);
    expect(beforeBend.artifactCollisionRemaining > 0
               && afterBend.artifactCollisionRemaining > 0
               && cachedAfterBend < cachedBeforeBend,
           "following-fret tap did not track an in-window upward pitch bend ("
               + std::to_string(beforeBend.artifactCollisionRemaining) + " -> "
               + std::to_string(afterBend.artifactCollisionRemaining) + ", "
               + std::to_string(cachedBeforeBend) + " -> "
               + std::to_string(cachedAfterBend) + ")");

    engine.reset();
    engine.noteOn(86, 1.0f);
    const auto fret22 = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 86));
    expect(fret22.fret == 22 && fret22.artifactCollisionLength == 0
               && fret22.artifactCollisionRemaining == 0,
           "last-fret note armed a non-existent following fret");

    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Dead), 1.0f);
    engine.noteOn(86, 1.0f);
    const auto deadFret22 = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 86));
    expect(deadFret22.fret == 22 && deadFret22.artifactCollisionLength > 0
               && deadFret22.artifactCollisionRemaining
                    == deadFret22.artifactCollisionLength,
           "last-fret Dead note lost its distributed-contact fallback");

    std::cout << "PROBE positioned following-fret H9/H18 gains: "
              << harmonic9Gain << "/" << harmonic18Gain << '\n';
}
#endif

void testAdvancedDispersionAndBodyConductance()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.scaleLength = 0.0f;
    parameters.stringGauge = 1.0f;
    parameters.bodyResonance = 1.0f;
    parameters.bodyShape = 0.0f;
    parameters.artifactAmount = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(28, 0.8f);

    const int stringIndex = TestAccess::stringForNote(engine, 28);
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(snapshot.inharmonicity > 1.0e-7f,
           "physical wound-string inharmonicity was not configured");
    expect(snapshot.dispersionLowCoefficient <= 0.0f
               && snapshot.dispersionLowCoefficient >= -0.9951f
               && snapshot.dispersionHighCoefficient <= 0.0f
               && snapshot.dispersionHighCoefficient >= -0.9951f,
           "eight-stage dispersion coefficients escaped their stable bounds");

    const auto expectedDeficit = [&] (float partial)
    {
        const float period = static_cast<float>(TestAccess::internalSampleRate(engine))
                           / snapshot.baseFrequency;
        const float stretch = std::sqrt(
            (1.0f + snapshot.inharmonicity * partial * partial)
            / (1.0f + snapshot.inharmonicity));
        return period * (1.0f - 1.0f / stretch);
    };
    for (const float partial : { snapshot.dispersionLowPartial,
                                 snapshot.dispersionHighPartial })
    {
        const float wanted = expectedDeficit(partial);
        const float actual = TestAccess::dispersionDeficit(
            engine, stringIndex, partial);
        const float relativeError = std::abs(actual - wanted)
            / std::max(wanted, 1.0e-6f);
        expect(relativeError < 0.20f,
               "two-point dispersion fit missed partial "
                   + std::to_string(partial) + " (wanted "
                   + std::to_string(wanted) + ", actual "
                   + std::to_string(actual) + ")");
    }

#if ELECTRY_MEASURED_BODY_RESPONSE
    expect(snapshot.bodyConductance == 0.0f
               && snapshot.bodyLossFactor == 1.0f,
           "Ray direct-colour model invented termination loading");
#else
    expect(snapshot.bodyConductance >= 0.0f && snapshot.bodyConductance <= 1.0f,
           "modal bridge conductance escaped its passive range");
    expect(snapshot.bodyLossFactor > 0.0f && snapshot.bodyLossFactor <= 1.0f,
           "modal bridge conductance added string energy");
#endif

    parameters.bodyResonance = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(28, 0.8f);
    const auto bypassed = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 28));
    expect(std::abs(bypassed.bodyLossFactor - 1.0f) < 1.0e-6f,
           "zero Body Resonance did not exactly bypass structural loss");
}

void testMonoStereoOutputField()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.bodyResonance = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.outputMode = electry::OutputMode::Mono;
    engine.setParameters(parameters);
    const auto monoLow = renderNote(engine, sampleRate, 28, 0.8f,
                                    PlayStyle::Sustain, 0.65);
    expect(monoLow.left == monoLow.right,
           "Mono output mode is not exact dual mono");

    parameters.outputMode = electry::OutputMode::Stereo;
    engine.setParameters(parameters);
    const auto stereoLow = renderNote(engine, sampleRate, 28, 0.8f,
                                      PlayStyle::Sustain, 0.65);
    const auto repeatedLow = renderNote(engine, sampleRate, 28, 0.8f,
                                        PlayStyle::Sustain, 0.65);
    expect(stereoLow.left == repeatedLow.left
               && stereoLow.right == repeatedLow.right,
           "Stereo divided-pickup field is not deterministic");

    const int start = static_cast<int>(0.025 * sampleRate);
    const int end = static_cast<int>(0.55 * sampleRate);
    const double lowLeft = rmsInRange(stereoLow.left, start, end);
    const double lowRight = rmsInRange(stereoLow.right, start, end);
    expect(lowLeft > lowRight * 1.10,
           "Stereo low E does not favour the low-string side (L "
               + std::to_string(lowLeft) + ", R "
               + std::to_string(lowRight) + ")");

    const auto stereoHigh = renderNote(engine, sampleRate, 64, 0.8f,
                                       PlayStyle::Sustain, 0.65);
    const double highLeft = rmsInRange(stereoHigh.left, start, end);
    const double highRight = rmsInRange(stereoHigh.right, start, end);
    expect(highRight > highLeft * 1.10,
           "Stereo high E does not favour the high-string side (L "
               + std::to_string(highLeft) + ", R "
               + std::to_string(highRight) + ")");

    std::vector<float> folded(stereoLow.left.size());
    std::vector<float> side(stereoLow.left.size());
    for (std::size_t sample = 0; sample < folded.size(); ++sample)
    {
        folded[sample] = 0.5f * (stereoLow.left[sample]
                               + stereoLow.right[sample]);
        side[sample] = 0.5f * (stereoLow.left[sample]
                             - stereoLow.right[sample]);
    }
    const double foldDifference = normalisedDifferenceRms(
        folded, monoLow.left, start, end);
    expect(foldDifference < 0.12,
           "Stereo field does not fold coherently to Mono (difference "
               + std::to_string(foldDifference) + ")");

    const double midRms = rmsInRange(folded, start, end);
    const double sideRms = rmsInRange(side, start, end);
    expect(sideRms > midRms * 0.08 && sideRms < midRms * 0.35,
           "Stereo side field is inaudible or excessive (ratio "
               + std::to_string(sideRms / std::max(midRms, 1.0e-12)) + ")");

    const double monoEnergy = std::pow(
        rmsInRange(monoLow.left, start, end), 2.0);
    const double stereoEnergy = 0.5
        * (lowLeft * lowLeft + lowRight * lowRight);
    const double energyRatio = stereoEnergy / std::max(monoEnergy, 1.0e-12);
    expect(energyRatio > 0.80 && energyRatio < 1.20,
           "Stereo output field changed average energy excessively (ratio "
               + std::to_string(energyRatio) + ")");
}

// A guitarist's picking dynamics span 25-30 dB. Electry used to render 5.2 dB
// of that, and the missing range was not in the amplitude law: player effort
// drove the contact spectrum over the same range as the level, so the extra
// energy of a hard stroke went into partials that had decayed before the
// attack was over. The two halves of the fix are separated below - the
// v=1..v=127 span is the target, the v=64..v=127 span is what identifies the
// decoupling as the change that delivers it.
void testVelocityDynamicRange()
{
    constexpr double sampleRate = 48000.0;
    constexpr int note = 40;
    constexpr int windowSamples = static_cast<int>(0.050 * sampleRate);

    // Fresh engine per velocity: a shared engine would let the previous
    // stroke's residual ring into the peak of the next one.
    const auto peakDbAt = [&] (int midiVelocity, float velocityAmount)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.velocityAmount = velocityAmount;
        engine.setParameters(parameters);
        const auto rendered = renderNote(
            engine, sampleRate, note,
            static_cast<float>(midiVelocity) / 127.0f,
            PlayStyle::Sustain, 0.050);
        const double peak = std::max(peakAbs(rendered.left, 0, windowSamples),
                                     peakAbs(rendered.right, 0, windowSamples));
        return decibels(std::max(peak, 1.0e-12));
    };

    // "At the shipping defaults": the default Velocity Response is part of the
    // change, so it is read from the engine rather than repeated here.
    const float shippingResponse = EngineParameters {}.velocityAmount;
    expect(std::abs(shippingResponse - 0.85f) < 1.0e-6f,
           "default Velocity Response is not 0.85 ("
               + std::to_string(shippingResponse) + ")");

    const double atOne = peakDbAt(1, shippingResponse);
    const double atSixtyFour = peakDbAt(64, shippingResponse);
    const double atFull = peakDbAt(127, shippingResponse);

    // Today 5.218 dB at the shipping default.
    expect(atFull - atOne >= 18.0,
           "velocity spans too little of the keyboard (v=1 "
               + std::to_string(atOne) + " dBFS, v=127 "
               + std::to_string(atFull) + " dBFS, span "
               + std::to_string(atFull - atOne) + " dB)");

    // The amplitude law on its own reaches 1.686 dB here, against 1.487 dB
    // before it; only breaking the level-to-effort coupling moves this. The
    // ceiling with the release rate frozen outright is 4.630 dB, and freezing
    // it outright makes velocity darken the attack instead of brightening it,
    // so the bar sits below that.
    expect(atFull - atSixtyFour >= 3.0,
           "the upper half of the keyboard is still flat (v=64 "
               + std::to_string(atSixtyFour) + " dBFS, v=127 "
               + std::to_string(atFull) + " dBFS, span "
               + std::to_string(atFull - atSixtyFour) + " dB)");

    // A regression guard rather than a target: the amplitude law alone turns
    // the top of the keyboard over above v=104, and so does the shipping
    // engine on this grid.
    double previous = -1.0e9;
    int firstDrop = -1;
    double dropFrom = 0.0;
    double dropTo = 0.0;
    for (int step = 0; step < 16; ++step)
    {
        const int midiVelocity = 1
            + static_cast<int>(std::lround(step * 126.0 / 15.0));
        const double level = peakDbAt(midiVelocity, shippingResponse);
        if (level <= previous && firstDrop < 0)
        {
            firstDrop = midiVelocity;
            dropFrom = previous;
            dropTo = level;
        }
        previous = level;
    }
    expect(firstDrop < 0,
           "velocity level is not monotone (v=" + std::to_string(firstDrop)
               + " fell from " + std::to_string(dropFrom) + " to "
               + std::to_string(dropTo) + " dBFS)");

    // The loudest stroke must stay where it is: this is a dynamic range that
    // grows downwards, not an output-level change.
    expect(std::abs(atFull - (-25.690)) <= 1.5,
           "full velocity moved away from its calibrated level ("
               + std::to_string(atFull) + " dBFS)");

    // The decoupling must not be implemented by flattening the attack into
    // silence at low velocity, in either direction: the 2-8 kHz against
    // sub-500 Hz band ratio of the attack has to stay put. Today it moves
    // 3.431 dB across the same pair.
    const auto attackBandRatioDb = [&] (int midiVelocity, float velocityAmount)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.velocityAmount = velocityAmount;
        engine.setParameters(parameters);
        const auto rendered = renderNote(
            engine, sampleRate, note,
            static_cast<float>(midiVelocity) / 127.0f,
            PlayStyle::Sustain, 0.050);
        constexpr int transform = 2048;
        double high = 0.0;
        double low = 0.0;
        for (int start = 0; start + transform <= windowSamples; start += 512)
        {
            for (int bin = 1; bin < transform / 2; ++bin)
            {
                const double frequency = bin * sampleRate / transform;
                const bool isHigh = frequency >= 2000.0 && frequency <= 8000.0;
                const bool isLow = frequency < 500.0;
                if (! isHigh && ! isLow)
                    continue;
                const double magnitude = dftMagnitude(
                    rendered.left, start, transform, sampleRate, frequency);
                (isHigh ? high : low) += magnitude * magnitude;
            }
        }
        return 10.0 * std::log10(std::max(high, 1.0e-30)
                                 / std::max(low, 1.0e-30));
    };
    const double quietBand = attackBandRatioDb(16, shippingResponse);
    const double loudBand = attackBandRatioDb(127, shippingResponse);
    expect(std::abs(loudBand - quietBand) <= 4.0,
           "velocity moved the attack's band balance too far (v=16 "
               + std::to_string(quietBand) + " dB, v=127 "
               + std::to_string(loudBand) + " dB)");

    // Velocity Response at zero removes MIDI velocity from the instrument
    // exactly, which the exponent form preserves because force^0 is one.
    const double flatLow = peakDbAt(1, 0.0f);
    const double flatHigh = peakDbAt(127, 0.0f);
    expect(std::abs(flatHigh - flatLow) < 1.0e-9,
           "zero velocity response still changes the rendered level (spread "
               + std::to_string(flatHigh - flatLow) + " dB)");
}

// A repeated note must not be a repeated render. The protocol deliberately
// leaves 12 s between strokes so the string has decayed and the measurement
// sees the excitation rather than the previous stroke's residual ring, and it
// silences every noise control and the Artifacts detune so that what is
// measured is the picking hand and nothing that already varied.
void testPickingHandVariation()
{
    constexpr double sampleRate = 48000.0;
    constexpr int note = 40;
    constexpr int strokeCount = 12;
    constexpr int captureSamples = static_cast<int>(0.150 * sampleRate);

    struct Repeats
    {
        std::vector<std::vector<float>> strokes;
        std::vector<double> peaksDb;
        std::vector<double> centroids;
    };

    const auto repeat = [&] (double gapSeconds, PickStyle pickStyle)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.artifactAmount = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(pickKeyswitch(pickStyle), 1.0f);

        Repeats result;
        StereoBuffer gap(static_cast<int>(gapSeconds * sampleRate));
        for (int stroke = 0; stroke < strokeCount; ++stroke)
        {
            engine.noteOn(note, 0.80f);
            renderInto(engine, gap);
            std::vector<float> attack(gap.left.begin(),
                                      gap.left.begin() + captureSamples);
            result.peaksDb.push_back(
                decibels(std::max<double>(peakAbs(attack), 1.0e-12)));
            result.centroids.push_back(spectralCentroid(
                attack, 0, captureSamples, sampleRate, midiHz(note)));
            result.strokes.push_back(std::move(attack));
        }
        return result;
    };

    // Energy of the difference between two strokes' attacks, against the energy
    // of the earlier one.
    const auto differenceDb = [] (const std::vector<float>& later,
                                  const std::vector<float>& earlier)
    {
        double difference = 0.0;
        double reference = 0.0;
        for (std::size_t i = 0; i < later.size(); ++i)
        {
            const double d = static_cast<double>(later[i]) - earlier[i];
            difference += d * d;
            reference += static_cast<double>(earlier[i]) * earlier[i];
        }
        return 10.0 * std::log10(std::max(difference, 1.0e-30)
                                 / std::max(reference, 1.0e-30));
    };

    const auto spreadOf = [] (const std::vector<double>& values)
    {
        const auto bounds = std::minmax_element(values.begin(), values.end());
        return *bounds.second - *bounds.first;
    };

    // Successive strokes, or - under Alternate - strokes two apart, which hold
    // the up/down colouring constant so what is left is the hand.
    const auto pairDbs = [&] (const Repeats& repeats, int lag)
    {
        std::vector<double> result;
        for (std::size_t i = 0; i + static_cast<std::size_t>(lag)
                                    < repeats.strokes.size(); ++i)
            result.push_back(differenceDb(
                repeats.strokes[i + static_cast<std::size_t>(lag)],
                repeats.strokes[i]));
        return result;
    };

    const auto meanOf = [] (const std::vector<double>& values)
    {
        double total = 0.0;
        for (const double value : values)
            total += value;
        return total / static_cast<double>(values.size());
    };

    const auto latched = repeat(12.0, PickStyle::Down);
    const auto latchedPairs = pairDbs(latched, 1);
    const double latchedMean = meanOf(latchedPairs);

    // Without the per-stroke draws the successive pairs fall from -43.5 dB to
    // -94.3 dB, a mean of -84.6 dB: the twelve strokes are the same stroke, and
    // what little separates them is converging residual state. The band is
    // scored on the mean because the draws are independent, so two consecutive
    // strokes may land close by chance - one pair of the twelve reads -30.8 dB;
    // every individual pair still has to stay under the band's top.
    expect(latchedMean >= -24.0 && latchedMean <= -8.0,
           "repeated strokes do not differ by a hand's worth (mean successive "
               "difference " + std::to_string(latchedMean) + " dB)");
    expect(*std::max_element(latchedPairs.begin(), latchedPairs.end()) <= -8.0,
           "repeated strokes differ too much to be the same note (loudest "
               "successive difference "
               + std::to_string(*std::max_element(latchedPairs.begin(),
                                                  latchedPairs.end())) + " dB)");

    // The variation must be audible as a hand rather than as a level control.
    // Without the draws, 0.0120 dB.
    const double peakSpread = spreadOf(latched.peaksDb);
    expect(peakSpread >= 0.6 && peakSpread <= 3.0,
           "stroke-to-stroke level variation is outside a player's range ("
               + std::to_string(peakSpread) + " dB across 12 strokes)");

    // And it must move the tone, not only the level: without the draws, 0.380 Hz
    // on a 456 Hz centroid.
    const double centroidSpread = spreadOf(latched.centroids);
    expect(centroidSpread >= 12.0,
           "repeated strokes are spectrally identical (centroid spread "
               + std::to_string(centroidSpread) + " Hz)");

    // Alternate picking already varies stroke to stroke by more than the signal
    // itself, so the hand's variation has to ride on top of the up/down
    // colouring rather than replace it. Strokes two apart share a direction;
    // without the draws they converge to a mean of -86.1 dB, exactly as the
    // latched case does, so a change that only varies a latched Down fails
    // here.
    const auto alternating = repeat(12.0, PickStyle::Alternate);
    const auto alternatingPairs = pairDbs(alternating, 2);
    const double alternatingMean = meanOf(alternatingPairs);
    expect(alternatingMean >= -24.0 && alternatingMean <= -8.0,
           "alternate-picked repeats of the same stroke direction do not vary "
               "(mean difference two apart " + std::to_string(alternatingMean)
               + " dB)");
    expect(*std::max_element(alternatingPairs.begin(), alternatingPairs.end())
               <= -8.0,
           "alternate-picked strokes two apart differ too much (loudest "
               + std::to_string(*std::max_element(alternatingPairs.begin(),
                                                  alternatingPairs.end()))
               + " dB)");

    // Half a second apart the string has not decayed, so each stroke lands on
    // the previous one's ring and the difference between them is dominated by
    // how far that residual state has converged. Without the draws it
    // converges: the twelfth pair reads -27.3 dB against -94.3 dB on the 12 s
    // protocol, 67 dB apart. With the excitation itself varying, the two
    // protocols have to
    // stay in the same broad range. A rapid re-pick inherits a ringing string
    // while the 12 s stroke does not, so they cannot agree exactly. The 12 dB
    // guard keeps that state dependence while still separating it widely from
    // the 67 dB convergence of a repeated, invariant excitation.
    const auto rapid = repeat(0.5, PickStyle::Down);
    const auto rapidPairs = pairDbs(rapid, 1);
    const double lastRapid = rapidPairs.back();
    const double lastLatched = latchedPairs.back();
    expect(std::abs(lastRapid - lastLatched) <= 12.0,
           "a repeated note still converges on itself (last pair "
               + std::to_string(lastRapid) + " dB at 0.5 s against "
               + std::to_string(lastLatched) + " dB at 12 s)");
}

void testVelocityExpression()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.velocityAmount = 1.0f;
    parameters.bodyResonance = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);

    const auto low = renderNote(engine, sampleRate, 45, 0.2f,
                                PlayStyle::Sustain, 0.55);
    const auto middle = renderNote(engine, sampleRate, 45, 0.6f,
                                   PlayStyle::Sustain, 0.55);
    const auto high = renderNote(engine, sampleRate, 45, 1.0f,
                                 PlayStyle::Sustain, 0.55);
    const int attackStart = static_cast<int>(0.004 * sampleRate);
    const int attackEnd = static_cast<int>(0.115 * sampleRate);
    const double lowRms = rmsInRange(low.left, attackStart, attackEnd);
    const double middleRms = rmsInRange(middle.left, attackStart, attackEnd);
    const double highRms = rmsInRange(high.left, attackStart, attackEnd);
    expect(middleRms > lowRms * 1.20 && highRms > middleRms * 1.20,
           "velocity amplitude is not clearly monotonic ("
               + std::to_string(lowRms) + ", " + std::to_string(middleRms)
               + ", " + std::to_string(highRms) + ")");

    const double lowCentroid = spectralCentroid(
        low.left, attackStart, attackEnd - attackStart, sampleRate, midiHz(45));
    const double highCentroid = spectralCentroid(
        high.left, attackStart, attackEnd - attackStart, sampleRate, midiHz(45));
    // Deliberately a smaller margin than the 10% this asked for before the
    // pick's own stiffness came to bound the contact spectrum: a harder stroke
    // is now mostly louder rather than proportionally sharper, and the
    // brightening that survives is the residual the plectrum's finite
    // compliance leaves. Measured 7.3% here, against 16.6% when effort and
    // level were the same curve. What the bound still catches is the
    // degenerate case - freezing the release rate outright reads 0.996, i.e. a
    // harder stroke that arrives darker.
    //
    // The 7.3% is the figure this read when the velocity work landed; the
    // humbucker's coil pair moved it to 6.7% two steps later, which is the
    // ratio the shipped engine measures on this fixture.
    expect(highCentroid > lowCentroid * 1.05,
           "velocity does not brighten the attack (low "
               + std::to_string(lowCentroid) + " Hz, high "
               + std::to_string(highCentroid) + " Hz)");

    // At zero response, MIDI velocity is deliberately removed from every
    // attack dimension, not merely from output gain.
    parameters.velocityAmount = 0.0f;
    engine.setParameters(parameters);
    const auto flatLow = renderNote(engine, sampleRate, 45, 0.2f,
                                    PlayStyle::Sustain, 0.30);
    const auto flatHigh = renderNote(engine, sampleRate, 45, 1.0f,
                                     PlayStyle::Sustain, 0.30);
    expect(flatLow.left == flatHigh.left && flatLow.right == flatHigh.right,
           "zero velocity response still changes the rendered attack");
}

void testMaterialAndControlAudibility()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    // This test asks whether each build axis is audible when it is swept, and
    // every threshold below was calibrated against one specific instrument. It
    // must therefore state that instrument rather than inherit whatever the
    // shipped defaults happen to be, or the thresholds silently come to mean
    // something else the moment the default voicing moves - which is exactly
    // what happened when the defaults became a thick blank with the heaviest set
    // and the tone backed off: seven checks here failed without one line of the
    // model changing, because a darker instrument makes every axis a smaller
    // fraction of its own signal. The mid-scale, tone-open instrument below is
    // the one the numbers were measured on.
    EngineParameters base;
    base.bodyWood = 0.5f;
    base.bodySize = 0.5f;
    base.bodyShape = 0.5f;
    base.construction = 0.5f;
    base.scaleLength = 0.5f;
    base.pickupType = 0.5f;
    base.toneKnob = 0.8f;
    base.stringGauge = 0.5f;
    base.stringAge = 0.15f;
    base.pickPosition = 0.35f;
    base.pickHardness = 0.6f;
    base.bodyResonance = 0.55f;
    base.artifactAmount = 0.0f;
    base.pickNoise = 0.0f;
    base.fingerNoise = 0.0f;
    base.releaseNoise = 0.0f;

    const auto compareAxis = [&] (auto setAxis, int midiNote)
    {
        auto lowParameters = base;
        setAxis(lowParameters, 0.0f);
        engine.setParameters(lowParameters);
        const auto low = renderNote(engine, sampleRate, midiNote, 0.8f,
                                    PlayStyle::Sustain, 0.75);
        auto highParameters = base;
        setAxis(highParameters, 1.0f);
        engine.setParameters(highParameters);
        const auto high = renderNote(engine, sampleRate, midiNote, 0.8f,
                                     PlayStyle::Sustain, 0.75);
        return normalisedDifferenceRms(
            low.left, high.left, static_cast<int>(0.035 * sampleRate),
            static_cast<int>(0.60 * sampleRate));
    };

    const auto wood = [] (EngineParameters& p, float v) { p.bodyWood = v; };
    const auto size = [] (EngineParameters& p, float v) { p.bodySize = v; };
    const auto shape = [] (EngineParameters& p, float v) { p.bodyShape = v; };
    const auto construction = [] (EngineParameters& p, float v) { p.construction = v; };
    const auto scale = [] (EngineParameters& p, float v) { p.scaleLength = v; };
    const auto gauge = [] (EngineParameters& p, float v) { p.stringGauge = v; };

    using AxisSetter = void (*) (EngineParameters&, float);
    const std::array<std::pair<const char*, AxisSetter>, 4> bodyAxes {{
        { "wood", wood }, { "size", size }, { "shape", shape },
        { "construction", construction }
    }};
    for (const auto& namedAxis : bodyAxes)
    {
        const double lowNoteDifference = compareAxis(namedAxis.second, 28);
        const double midNoteDifference = compareAxis(namedAxis.second, 45);
#if ELECTRY_MEASURED_BODY_RESPONSE
        const std::string axisName(namedAxis.first);
        std::cout << "PROBE measured body " << axisName
                  << " E1/A2 residuals: " << lowNoteDifference << ", "
                  << midNoteDifference << '\n';
        if (axisName != "wood")
        {
            expect(lowNoteDifference < 1.0e-9 && midNoteDifference < 1.0e-9,
                   std::string("inactive measured-body axis ") + axisName
                       + " invented a material response");
        }
        else
        {
            expect(std::isfinite(lowNoteDifference)
                       && std::isfinite(midNoteDifference)
                       && std::max(lowNoteDifference, midNoteDifference) > 0.01
                       && std::max(lowNoteDifference, midNoteDifference) < 0.50,
                   "Ray Body Wood became inaudible or escaped its "
                   "non-exaggeration rail");
        }
#else
        expect(std::min(lowNoteDifference, midNoteDifference) > 0.055,
               std::string("body ") + namedAxis.first
                   + " remains effectively inaudible on E1/A2 ("
                   + std::to_string(lowNoteDifference) + ", "
                   + std::to_string(midNoteDifference) + ")");
#endif
    }

    const double scaleDifference = compareAxis(scale, 28);
    const double gaugeDifference = compareAxis(gauge, 28);
    expect(scaleDifference > 0.025,
           "25.5-to-28-inch scale range does not change Drop-E timbre ("
               + std::to_string(scaleDifference) + ")");
    expect(gaugeDifference > 0.08,
           "string-gauge endpoints remain too similar ("
               + std::to_string(gaugeDifference) + ")");

    auto noBody = base;
    noBody.bodyResonance = 0.0f;
    engine.setParameters(noBody);
    const auto dry = renderNote(engine, sampleRate, 45, 0.8f,
                                PlayStyle::Sustain, 0.75);
    auto fullBody = base;
    fullBody.bodyResonance = 1.0f;
    engine.setParameters(fullBody);
    const auto resonant = renderNote(engine, sampleRate, 45, 0.8f,
                                     PlayStyle::Sustain, 0.75);
    const double bodyDifference = normalisedDifferenceRms(
        dry.left, resonant.left, static_cast<int>(0.035 * sampleRate),
        static_cast<int>(0.60 * sampleRate));
    std::cout << "PROBE Body Resonance endpoint residual: "
              << bodyDifference << '\n';
    // Exact modal normalisation keeps the structural path controlled: a
    // clearly audible endpoint change is required, but the test must not
    // reward the former oversized, clavinet-like body peaks.
#if ELECTRY_MEASURED_BODY_RESPONSE
    expect(bodyDifference > 0.02 && bodyDifference < 0.08,
           "quiet Ray Body Resonance escaped its direct-transfer range ("
               + std::to_string(bodyDifference) + ")");
#else
    expect(bodyDifference > 0.08,
           "Body Resonance full range is still too polite ("
               + std::to_string(bodyDifference) + ")");
#endif

    auto fresh = base;
    fresh.bodyResonance = 0.0f;
    fresh.stringAge = 0.0f;
    engine.setParameters(fresh);
    const auto freshRender = renderNote(engine, sampleRate, 45, 0.8f,
                                        PlayStyle::Sustain, 1.3);
    auto old = fresh;
    old.stringAge = 1.0f;
    engine.setParameters(old);
    const auto oldRender = renderNote(engine, sampleRate, 45, 0.8f,
                                      PlayStyle::Sustain, 1.3);
    const double freshLate = rmsInRange(
        freshRender.left, static_cast<int>(0.8 * sampleRate),
        static_cast<int>(1.2 * sampleRate));
    const double oldLate = rmsInRange(
        oldRender.left, static_cast<int>(0.8 * sampleRate),
        static_cast<int>(1.2 * sampleRate));
    expect(oldLate < freshLate * 0.35,
           "String Age does not strongly shorten/darken the tail (ratio "
               + std::to_string(oldLate / std::max(freshLate, 1.0e-12)) + ")");

    auto bridgePick = fresh;
    bridgePick.pickPosition = 0.0f;
    engine.setParameters(bridgePick);
    const auto bridgePicked = renderNote(engine, sampleRate, 45, 0.8f,
                                         PlayStyle::Sustain, 0.6);
    auto neckPick = fresh;
    neckPick.pickPosition = 1.0f;
    engine.setParameters(neckPick);
    const auto neckPicked = renderNote(engine, sampleRate, 45, 0.8f,
                                       PlayStyle::Sustain, 0.6);
    const double positionDifference = normalisedDifferenceRms(
        bridgePicked.left, neckPicked.left, static_cast<int>(0.005 * sampleRate),
        static_cast<int>(0.40 * sampleRate));
    expect(positionDifference > 0.35,
           "Pick Position range is not clearly audible ("
               + std::to_string(positionDifference) + ")");

    auto soft = fresh;
    soft.pickHardness = 0.0f;
    engine.setParameters(soft);
    const auto softPick = renderNote(engine, sampleRate, 45, 0.8f,
                                     PlayStyle::Sustain, 0.5);
#if ELECTRY_DECOUPLED_PICK_RELEASE
    const auto softVoice = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 45));
#endif
    auto hard = fresh;
    hard.pickHardness = 1.0f;
    engine.setParameters(hard);
    const auto hardPick = renderNote(engine, sampleRate, 45, 0.8f,
                                     PlayStyle::Sustain, 0.5);
#if ELECTRY_DECOUPLED_PICK_RELEASE
    const auto hardVoice = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 45));
#endif
    const int attackStart = static_cast<int>(0.004 * sampleRate);
    const int attackLength = static_cast<int>(0.10 * sampleRate);
    const double softCentroid = spectralCentroid(
        softPick.left, attackStart, attackLength, sampleRate, midiHz(45));
    const double hardCentroid = spectralCentroid(
        hardPick.left, attackStart, attackLength, sampleRate, midiHz(45));
    const double softRms = rmsInRange(softPick.left, attackStart,
                                      attackStart + attackLength);
    const double hardRms = rmsInRange(hardPick.left, attackStart,
                                      attackStart + attackLength);
    std::cout << "PROBE Pick Hardness centroid/RMS ratios: "
              << hardCentroid / std::max(softCentroid, 1.0e-12) << "/"
              << hardRms / std::max(softRms, 1.0e-12) << '\n';
#if ELECTRY_DECOUPLED_PICK_RELEASE
    // Remove each live release pole's exact response at f0. What remains must
    // be the same displacement for the same MIDI force: Pick Hardness changes
    // the plectrum's contact spectrum, not the player's force axis.
    constexpr float twoPi = 6.28318530717958647692f;
    const auto unfilteredDisplacement = [&] (const auto& voice)
    {
#if ELECTRY_ENERGY_ATTACK_PITCH
        // lastCompensatedPeriod intentionally follows the decaying physical
        // pitch experiment. The release pole was normalised at the written
        // pitch before that release committed, so remove it at the same
        // nominal coordinate when checking Pick Hardness independence.
        const float nominalFrequency = voice.baseFrequency * std::exp2(
            voice.lastCompensatedSemitones / 12.0f);
        const float omega = twoPi * nominalFrequency
            / static_cast<float>(TestAccess::internalSampleRate(engine));
#else
        const float omega = twoPi
            / std::max(voice.lastCompensatedPeriod, 1.0f);
#endif
        return voice.excitationAmplitude * TestAccess::onePoleMagnitude(
            voice.excitationReleaseCoefficient, omega);
    };
    const float softDisplacement = unfilteredDisplacement(softVoice);
    const float hardDisplacement = unfilteredDisplacement(hardVoice);
    expect(softVoice.valid && hardVoice.valid
               && softDisplacement > 0.0f
               && std::abs(hardDisplacement / softDisplacement - 1.0f)
                      < 2.0e-5f,
           "Pick Hardness still changes force-normalised displacement (ratio "
               + std::to_string(hardDisplacement
                                / std::max(softDisplacement, 1.0e-12f)) + ")");

    const auto bentDisplacement = [&] (float hardness, float bend)
    {
        auto bent = fresh;
        bent.pickHardness = hardness;
        engine.setParameters(bent);
        engine.setPitchBend(bend);
        engine.reset();
        engine.noteOn(ElectryEngine::lowestPlayableNote, 0.8f);
        const auto voice = TestAccess::snapshot(
            engine, TestAccess::stringForNote(
                engine, ElectryEngine::lowestPlayableNote));
        return std::pair { voice, unfilteredDisplacement(voice) };
    };
    const auto [centreBendVoice, centreBendDisplacement] =
        bentDisplacement(0.6f, 0.0f);
    const bool centreModalValid =
        centreBendVoice.excitationModalCoefficient > 0.0f
        && centreBendVoice.excitationModalCoefficient < 1.0f;
    expect(centreModalValid,
           "unbent modal spectrum produced an invalid pole coefficient");
    const float centreModalLog = centreModalValid
        ? std::log(centreBendVoice.excitationModalCoefficient) : -1.0f;
    for (const float bend : { -1.0f, 1.0f })
    {
        const auto [bentSoftVoice, bentSoft] = bentDisplacement(0.0f, bend);
        const auto [bentHardVoice, bentHard] = bentDisplacement(1.0f, bend);
        expect(bentSoftVoice.valid && bentHardVoice.valid && bentSoft > 0.0f
                   && std::abs(bentHard / bentSoft - 1.0f) < 2.0e-5f,
               "bent Pick Hardness changes force-normalised displacement (ratio "
                   + std::to_string(bentHard / std::max(bentSoft, 1.0e-12f))
                   + ")");

        const auto [bentVoice, bent] = bentDisplacement(0.6f, bend);
        const float expectedPeriodRatio = std::exp2(-2.0f * bend / 12.0f);
        const float actualDisplacementRatio = bent / centreBendDisplacement;
        expect(centreBendVoice.valid && bentVoice.valid
                   && centreBendDisplacement > 0.0f
                   && std::abs(actualDisplacementRatio / expectedPeriodRatio
                                   - 1.0f) < 2.0e-5f,
               "pre-bent modal projection did not follow the target period "
               "(ratio " + std::to_string(actualDisplacementRatio)
                   + ", expected " + std::to_string(expectedPeriodRatio)
                   + ")");

        const float expectedModalCutoffRatio = std::exp2(
            2.0f * bend / 12.0f);
        const bool bentModalValid =
            bentVoice.excitationModalCoefficient > 0.0f
            && bentVoice.excitationModalCoefficient < 1.0f;
        const float actualModalCutoffRatio = bentModalValid
            ? std::log(bentVoice.excitationModalCoefficient) / centreModalLog
            : 0.0f;
        expect(centreModalValid && bentModalValid
                   && std::isfinite(actualModalCutoffRatio)
                   && std::abs(actualModalCutoffRatio
                                   / expectedModalCutoffRatio - 1.0f)
                          < 2.0e-5f,
               "pre-bent modal spectrum did not follow the target period "
               "(cutoff ratio " + std::to_string(actualModalCutoffRatio)
                   + ", expected "
                   + std::to_string(expectedModalCutoffRatio) + ")");
    }
    engine.setPitchBend(0.0f);

    constexpr ElectryEngine::ExpressionId expressionId = 2;
    const auto expressionDisplacement = [&] (float hardness, float semitones)
    {
        auto bent = fresh;
        bent.pickHardness = hardness;
        engine.setParameters(bent);
        engine.setExpressionPitchBend(expressionId, semitones);
        engine.reset();
        engine.noteOn(45, 0.8f, expressionId);
        const auto voice = TestAccess::snapshot(
            engine, TestAccess::stringForNote(engine, 45));
        return std::pair { voice, unfilteredDisplacement(voice) };
    };
    for (const float semitones : { -24.0f, 24.0f })
    {
        const auto [bentSoftVoice, bentSoft] =
            expressionDisplacement(0.0f, semitones);
        const auto [bentHardVoice, bentHard] =
            expressionDisplacement(1.0f, semitones);
        expect(bentSoftVoice.valid && bentHardVoice.valid && bentSoft > 0.0f
                   && std::abs(bentHard / bentSoft - 1.0f) < 2.0e-5f,
               "MPE-bent Pick Hardness changes force-normalised displacement "
               "(ratio "
                   + std::to_string(bentHard / std::max(bentSoft, 1.0e-12f))
                   + ")");
    }
    engine.setExpressionPitchBend(expressionId, 0.0f);
#endif
    // As above, the picked attack's reference-calibrated spectrum compresses
    // the absolute centroid range this control spans. It remains clearly
    // audible - the position, level and material bounds around it are
    // unchanged - but the old 1.35 margin belonged to the brighter voicing.
    expect(hardCentroid > softCentroid * 1.08,
           "Pick Hardness does not sufficiently brighten the attack (ratio "
               + std::to_string(hardCentroid / std::max(softCentroid, 1.0e-12))
               + ")");
    expect(hardRms > softRms * 0.55 && hardRms < softRms * 1.70,
           "Pick Hardness changes loudness more than material (RMS ratio "
               + std::to_string(hardRms / std::max(softRms, 1.0e-12)) + ")");
}

void testGuitarBuildMacro()
{
#if ELECTRY_MEASURED_BODY_RESPONSE
    static constexpr std::array<std::array<float, 6>, 6> expected {{
        { 0.00f, 0.50f, 0.50f, 0.50f, 0.00f, 1.00f },
        { 0.20f, 0.50f, 0.50f, 0.50f, 0.20f, 0.80f },
        { 0.40f, 0.50f, 0.50f, 0.50f, 0.40f, 0.60f },
        { 0.60f, 0.50f, 0.50f, 0.50f, 0.60f, 0.40f },
        { 0.80f, 0.50f, 0.50f, 0.50f, 0.85f, 1.00f },
        { 1.00f, 0.50f, 0.50f, 0.50f, 1.00f, 0.00f },
    }};
#else
    static constexpr std::array<std::array<float, 6>, 6> expected {{
        { 0.75f, 0.65f, 1.00f, 1.00f, 0.00f, 0.10f },
        { 0.50f, 0.80f, 0.60f, 0.72f, 0.12f, 0.18f },
        { 0.05f, 0.55f, 0.95f, 0.05f, 0.00f, 0.25f },
        { 0.60f, 0.70f, 0.75f, 0.90f, 0.60f, 0.15f },
        { 0.00f, 0.00f, 0.00f, 0.00f, 0.85f, 1.00f },
        { 0.50f, 0.62f, 0.70f, 0.15f, 1.00f, 0.35f },
    }};
#endif

    const auto coordinates = [] (const EngineParameters& p)
    {
        return std::array<float, 6> { p.bodyWood, p.bodySize, p.bodyShape,
                                      p.construction, p.scaleLength,
                                      p.stringGauge };
    };

#if ELECTRY_MEASURED_BODY_RESPONSE
    const EngineParameters rawDefault;
    auto mappedDefault = rawDefault;
    applyGuitarBuild(mappedDefault, electry::defaultGuitarBuild);
    const auto rawDefaultCoordinates = coordinates(rawDefault);
    const auto mappedDefaultCoordinates = coordinates(mappedDefault);
    bool defaultsMatch = true;
    for (std::size_t coordinate = 0; coordinate < rawDefaultCoordinates.size();
         ++coordinate)
        defaultsMatch = defaultsMatch
            && std::abs(rawDefaultCoordinates[coordinate]
                            - mappedDefaultCoordinates[coordinate]) < 1.0e-6f;
    expect(defaultsMatch,
           "measured raw-engine default differs from Guitar Build default");
#endif

    for (std::size_t anchor = 0; anchor < expected.size(); ++anchor)
    {
        EngineParameters parameters;
        parameters.pickupType = 0.73f;
        parameters.toneKnob = 0.61f;
        parameters.bodyResonance = 0.47f;
        parameters.stringAge = 0.29f;
        applyGuitarBuild(parameters,
                         static_cast<float>(anchor)
                             / static_cast<float>(expected.size() - 1));
        const auto actual = coordinates(parameters);
#if ELECTRY_MEASURED_BODY_RESPONSE
        bool matches = true;
        for (std::size_t coordinate = 0; coordinate < actual.size(); ++coordinate)
            matches = matches
                && std::abs(actual[coordinate] - expected[anchor][coordinate])
                       < 1.0e-6f;
        expect(matches,
               "measured Guitar Build missed physical-path anchor "
                   + std::to_string(anchor));
#else
        expect(actual == expected[anchor],
               "Guitar Build missed exact structural anchor "
                   + std::to_string(anchor));
#endif
        expect(parameters.pickupType == 0.73f && parameters.toneKnob == 0.61f
                   && parameters.bodyResonance == 0.47f
                   && parameters.stringAge == 0.29f,
               "Guitar Build changed an independent pickup/player control");
    }

    EngineParameters midpoint;
    applyGuitarBuild(midpoint, 0.1f);
    const auto halfway = coordinates(midpoint);
    for (std::size_t coordinate = 0; coordinate < halfway.size(); ++coordinate)
        expect(std::abs(halfway[coordinate]
                            - 0.5f * (expected[0][coordinate]
                                      + expected[1][coordinate])) < 1.0e-5f,
               "Guitar Build does not interpolate smoothly between anchors");

    EngineParameters invalid;
    applyGuitarBuild(invalid, std::numeric_limits<float>::quiet_NaN());
#if ELECTRY_MEASURED_BODY_RESPONSE
    const auto invalidCoordinates = coordinates(invalid);
    bool invalidMatches = true;
    for (std::size_t coordinate = 0; coordinate < invalidCoordinates.size();
         ++coordinate)
        invalidMatches = invalidMatches
            && std::abs(invalidCoordinates[coordinate]
                            - expected[4][coordinate]) < 1.0e-6f;
    expect(invalidMatches,
           "invalid measured Guitar Build did not use its fitted default");
#else
    const EngineParameters defaults;
    expect(std::abs(invalid.bodyWood - defaults.bodyWood) < 1.0e-5f
               && std::abs(invalid.bodySize - defaults.bodySize) < 1.0e-5f
               && std::abs(invalid.bodyShape - defaults.bodyShape) < 1.0e-5f
               && std::abs(invalid.construction - defaults.construction) < 1.0e-5f
               && std::abs(invalid.scaleLength - defaults.scaleLength) < 1.0e-5f
               && std::abs(invalid.stringGauge - defaults.stringGauge) < 1.0e-5f,
           "invalid Guitar Build did not fall back to the fitted default");
#endif

    for (const auto& outside : { std::pair { -10.0f, std::size_t { 0 } },
                                 std::pair { 10.0f, expected.size() - 1 } })
    {
        EngineParameters clamped;
        applyGuitarBuild(clamped, outside.first);
        const auto actual = coordinates(clamped);
        for (std::size_t coordinate = 0; coordinate < actual.size(); ++coordinate)
            expect(std::abs(actual[coordinate]
                                - expected[outside.second][coordinate]) < 1.0e-5f,
                   "Guitar Build did not clamp a finite out-of-range value");
    }
}

void testGuitarBuildRangeIsAudible()
{
    constexpr double sampleRate = 48000.0;
    for (const int midiNote : { 28, 45 })
    {
        std::vector<StereoBuffer> renders;
        renders.reserve(6);
        for (std::size_t anchor = 0; anchor < 6; ++anchor)
        {
            ElectryEngine engine;
            engine.prepare(sampleRate, 512);
            EngineParameters parameters;
            parameters.pickupType = 0.32f;
            parameters.toneKnob = 0.8f;
            parameters.bodyResonance = 0.65f;
            parameters.stringAge = 0.15f;
            parameters.pickNoise = 0.0f;
            parameters.fingerNoise = 0.0f;
            parameters.releaseNoise = 0.0f;
            parameters.artifactAmount = 0.0f;
            applyGuitarBuild(parameters, static_cast<float>(anchor) / 5.0f);
            engine.setParameters(parameters);
            renders.push_back(renderNote(engine, sampleRate, midiNote, 0.9f,
                                         PlayStyle::Sustain, 0.92));
            expect(allFinite(renders.back()),
                   "Guitar Build produced non-finite audio");

            // Keep the contrast render as a hard metal pick, but measure
            // structural tuning in the settled body so the attack waveform is
            // not mistaken for a Build error.
            const auto tuningRender = renderNote(
                engine, sampleRate, midiNote, 0.3f, PlayStyle::Sustain, 1.1);
            const double measured = measureFrequency(
                tuningRender.left, static_cast<int>(0.45 * sampleRate),
                static_cast<int>(0.5 * sampleRate), sampleRate,
                midiHz(midiNote));
            const double errorCents = centsBetween(measured, midiHz(midiNote));
            expect(std::abs(errorCents) < 8.0,
                   "Guitar Build detuned anchor " + std::to_string(anchor)
                       + " on note " + std::to_string(midiNote) + " ("
                       + std::to_string(errorCents) + " cents)");
        }

        const int start = static_cast<int>(0.035 * sampleRate);
        const int end = static_cast<int>(0.60 * sampleRate);
        double minimumRms = std::numeric_limits<double>::max();
        double maximumRms = 0.0;
        for (const auto& render : renders)
        {
            const double level = rmsInRange(render.left, start, end);
            minimumRms = std::min(minimumRms, level);
            maximumRms = std::max(maximumRms, level);
        }
        const double levelSpreadDb = 20.0 * std::log10(
            maximumRms / std::max(minimumRms, 1.0e-12));
        expect(levelSpreadDb < 6.0,
               "Guitar Build requires a large output trim ("
                   + std::to_string(levelSpreadDb) + " dB)");

        for (std::size_t anchor = 1; anchor < renders.size(); ++anchor)
        {
            const double difference = normalisedDifferenceRms(
                renders[anchor - 1].left, renders[anchor].left, start, end);
            std::cout << "PROBE Guitar Build note " << midiNote << " anchor "
                      << anchor - 1 << "->" << anchor << ": "
                      << difference << '\n';
            // The former 0.06 rail was set while unsupported detuning inflated
            // E1 anchor 0->1 from a timbre difference to 1.220. The corrected
            // path instead requires every adjacent step to remain nonzero and
            // bounded, while the endpoint check protects the overall travel.
#if ELECTRY_MEASURED_BODY_RESPONSE
            constexpr double maximumAdjacentDifference =
#if ELECTRY_ENERGY_ATTACK_PITCH
                1.10;
#else
                0.75;
#endif
            expect(std::isfinite(difference)
                       && difference > 0.02
                       && difference < maximumAdjacentDifference,
                   "measured adjacent Guitar Build anchors collapsed or "
                   "escaped their non-exaggeration rail");
#else
            expect(difference > 0.04,
                   "adjacent Guitar Build anchors collapse to the same sound ("
                       + std::to_string(anchor - 1) + "->"
                       + std::to_string(anchor) + ": "
                       + std::to_string(difference) + ")");
#endif
        }
        const double overall = normalisedDifferenceRms(
            renders.front().left, renders.back().left, start, end);
#if ELECTRY_MEASURED_BODY_RESPONSE
        expect(std::isfinite(overall) && overall > 0.12 && overall < 0.90,
               "measured Guitar Build endpoints became inaudible or escaped "
               "their non-exaggeration rail");
#else
        expect(overall > 0.12,
               "Guitar Build endpoints are too similar ("
                   + std::to_string(overall) + ")");
#endif
        std::cout << "PROBE Guitar Build note " << midiNote
                  << ": endpoint difference " << overall
                  << ", level spread " << levelSpreadDb << " dB\n";
    }

}

void testNoiseComponentsAndSilence()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    // Complete silence with no input.
    engine.setParameters(EngineParameters {});
    engine.reset();
    StereoBuffer silent(static_cast<int>(0.5 * sampleRate));
    renderInto(engine, silent);
    expect(peakAbs(silent.left) == 0.0f && peakAbs(silent.right) == 0.0f,
           "engine does not render exact silence with no notes");

    // Pick noise adds energy in the pre-attack contact window.
    EngineParameters noNoise;
    noNoise.pickNoise = 0.0f;
    noNoise.fingerNoise = 0.0f;
    noNoise.releaseNoise = 0.0f;
    engine.setParameters(noNoise);
    const auto clean = renderNote(engine, sampleRate, 45, 0.8f,
                                  PlayStyle::Sustain, 0.9, 0.6);

    EngineParameters fullNoise;
    fullNoise.pickNoise = 1.0f;
    fullNoise.fingerNoise = 1.0f;
    fullNoise.releaseNoise = 1.0f;
    engine.setParameters(fullNoise);
    const auto noisy = renderNote(engine, sampleRate, 45, 0.8f,
                                  PlayStyle::Sustain, 0.9, 0.6);

    // The pick contact lasts about 1.5 ms at the default hardness before the
    // release pulse starts; inside 1.2 ms the noiseless render is silent.
    const int contactWindow = static_cast<int>(0.0012 * sampleRate);
    const double cleanContact = rmsInRange(clean.left, 0, contactWindow);
    const double noisyContact = rmsInRange(noisy.left, 0, contactWindow);
    expect(noisyContact > cleanContact * 1.5 + 1.0e-6,
           "plectrum contact noise is missing from the attack (clean "
               + std::to_string(cleanContact) + ", noisy "
               + std::to_string(noisyContact) + ")");

    // Release noise adds energy just after note-off. This render pair
    // differs only in the releaseNoise control, and rendering is
    // deterministic, so any pre-note-off difference is a defect and the
    // release-window difference is exactly the added noise.
    EngineParameters releaseOnly = noNoise;
    releaseOnly.releaseNoise = 1.0f;
    engine.setParameters(releaseOnly);
    const auto releaseNoisy = renderNote(engine, sampleRate, 45, 0.8f,
                                         PlayStyle::Sustain, 0.9, 0.6);

    const int releaseStart = static_cast<int>(0.6 * sampleRate);
    const int releaseEnd = releaseStart + static_cast<int>(0.015 * sampleRate);
    double differenceEnergy = 0.0;
    double cleanEnergy = 0.0;
    for (int i = releaseStart; i < releaseEnd; ++i)
    {
        const double difference = releaseNoisy.left[static_cast<std::size_t>(i)]
                                - clean.left[static_cast<std::size_t>(i)];
        differenceEnergy += difference * difference;
        cleanEnergy += clean.left[static_cast<std::size_t>(i)]
                     * clean.left[static_cast<std::size_t>(i)];
    }
    expect(differenceEnergy > 1.0e-9
               && differenceEnergy > 0.005 * std::max(cleanEnergy, 1.0e-12),
           "release noise is missing after note-off");
    double preOffDifference = 0.0;
    int firstDifferingSample = -1;
    for (int i = 0; i < releaseStart - 64; ++i)
    {
        const double difference = releaseNoisy.left[static_cast<std::size_t>(i)]
                                - clean.left[static_cast<std::size_t>(i)];
        if (difference != 0.0 && firstDifferingSample < 0)
            firstDifferingSample = i;
        preOffDifference += difference * difference;
    }
    expect(preOffDifference == 0.0,
           "release-noise level changed audio before the note-off (first at "
               + std::to_string(firstDifferingSample) + ", energy "
               + std::to_string(preOffDifference) + ")");

    // Note-off damps the string quickly.
    const double preRelease = rmsInRange(clean.left,
                                         releaseStart - static_cast<int>(0.1 * sampleRate),
                                         releaseStart);
    const double postRelease = rmsInRange(clean.left,
                                          releaseStart + static_cast<int>(0.35 * sampleRate),
                                          releaseStart + static_cast<int>(0.45 * sampleRate));
    expect(postRelease < preRelease * 0.05,
           "released note does not damp towards silence");
}

void testStringAllocationAndPolyphony()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();

    // The open C-major shape: x-3-2-0-1-0 from the A string.
    const std::array<int, 5> chord { 48, 52, 55, 60, 64 };
    for (const int note : chord)
        engine.noteOn(note, 0.8f);

    expect(engine.getActiveVoiceCount() == 5,
           "C-major chord did not allocate five strings");
    expect(TestAccess::stringForNote(engine, 48) == 3, "C3 is not on the A string");
    expect(TestAccess::stringForNote(engine, 52) == 4, "E3 is not on the D string");
    expect(TestAccess::stringForNote(engine, 55) == 5, "G3 is not on the G string");
    expect(TestAccess::stringForNote(engine, 60) == 6, "C4 is not on the B string");
    expect(TestAccess::stringForNote(engine, 64) == 7,
           "E4 is not on the high E string");

    // The three lower opens fill the remaining physical strings. A ninth
    // simultaneous note must steal while the voice count remains eight.
    engine.noteOn(40, 0.8f);
    expect(engine.getActiveVoiceCount() == 6, "open E2 did not use its string");
    engine.noteOn(35, 0.8f);
    expect(engine.getActiveVoiceCount() == 7, "open B1 did not use its string");
    engine.noteOn(28, 0.8f);
    expect(engine.getActiveVoiceCount() == 8, "open E1 did not use its string");
    engine.noteOn(50, 0.8f);
    expect(engine.getActiveVoiceCount() == 8,
           "ninth simultaneous note exceeded eight strings");

    // Every open note maps to its own physical string in Drop-E tuning.
    engine.reset();
    constexpr std::array<int, ElectryEngine::stringCount> openNotes {
        28, 35, 40, 45, 50, 55, 59, 64
    };
    for (int string = 0; string < ElectryEngine::stringCount; ++string)
    {
        const int note = openNotes[static_cast<std::size_t>(string)];
        engine.noteOn(note, 0.8f);
        expect(TestAccess::stringForNote(engine, note) == string,
               "open note " + std::to_string(note)
                   + " did not map to physical string " + std::to_string(string));
    }
    expect(engine.getActiveVoiceCount() == ElectryEngine::stringCount,
           "eight open notes did not fill all eight physical strings");

    // Retriggering a sounding note reuses its string.
    engine.reset();
    engine.noteOn(45, 0.8f);
    const int firstString = TestAccess::stringForNote(engine, 45);
    engine.noteOn(45, 0.8f);
    expect(engine.getActiveVoiceCount() == 1,
           "restruck note did not reuse its string");
    expect(TestAccess::stringForNote(engine, 45) == firstString,
           "restruck note moved to another string");
}

void testChordAssignmentIsPermutationInvariant()
{
    constexpr double sampleRate = 48000.0;
    constexpr int renderSamples = 2048;

    const auto check = []<std::size_t N> (
        const char* label, const std::array<int, N>& notes,
        const std::array<int, N>& expectedStrings,
        const std::array<int, N>& expectedFrets, float expectedHand)
    {
        auto order = notes;
        StereoBuffer reference(renderSamples);
        bool haveReference = false;
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        engine.setParameters(parameters);
        do
        {
            engine.reset();

            std::array<ElectryEngine::NoteOnEvent, N> events {};
            for (std::size_t i = 0; i < N; ++i)
                events[i] = { order[i], 0.8f };
            engine.noteOnChord(events);

            expect(engine.getActiveVoiceCount() == static_cast<int>(N),
                   std::string(label) + " lost a voice under permutation");
            for (std::size_t i = 0; i < N; ++i)
            {
                const int string = TestAccess::stringForNote(engine, notes[i]);
                const auto voice = TestAccess::snapshot(engine, string);
                expect(string == expectedStrings[i] && voice.fret == expectedFrets[i],
                       std::string(label) + " changed its string/fret assignment");
            }
            expect(std::abs(TestAccess::frettingHandPosition(engine) - expectedHand)
                       < 1.0e-6f,
                   std::string(label) + " moved the hand to the wrong position");

            StereoBuffer audio(renderSamples);
            renderInto(engine, audio);
            if (! haveReference)
            {
                reference = audio;
                haveReference = true;
                expect(peakAbs(audio.left) > 1.0e-5f,
                       std::string(label) + " permutation fixture was silent");
            }
            else
            {
                expect(audio.left == reference.left && audio.right == reference.right,
                       std::string(label) + " rendered differently under permutation");
            }
        }
        while (std::next_permutation(order.begin(), order.end()));
    };

    check("fifth-fret barre", std::array { 33, 40, 45 },
          std::array { 0, 1, 2 }, std::array { 5, 5, 5 }, 3.0f);
    check("high-register dyad", std::array { 64, 82 },
          std::array { 4, 7 }, std::array { 14, 18 }, 14.0f);
    check("open low chord", std::array { 28, 35, 69 },
          std::array { 0, 1, 7 }, std::array { 0, 0, 5 }, 3.0f);
    check("open C major", std::array { 48, 52, 55, 60, 64 },
          std::array { 3, 4, 5, 6, 7 }, std::array { 3, 2, 0, 1, 0 }, 0.0f);

    // Batching two owners of one MIDI pitch must retain the scalar note-on
    // overlap contract: one physical string, released only by the second off.
    auto duplicate = std::make_unique<ElectryEngine>();
    duplicate->prepare(sampleRate, 512);
    EngineParameters duplicateParameters;
    duplicateParameters.strumSpreadSeconds = 0.020f;
    duplicate->setParameters(duplicateParameters);
    const std::array duplicateEvents {
        ElectryEngine::NoteOnEvent { 45, 0.8f },
        ElectryEngine::NoteOnEvent { 45, 0.8f }
    };
    duplicate->noteOnChord(duplicateEvents);
    const int duplicateString = TestAccess::stringForNote(*duplicate, 45);
    expect(duplicate->getActiveVoiceCount() == 1 && duplicateString >= 0,
           "duplicate chord notes did not share one physical string");
    const auto duplicateContact =
        TestAccess::snapshot(*duplicate, duplicateString);
    expect(duplicateContact.startDelaySamples == 0
               && ! duplicateContact.pendingRepickActive,
           "a duplicate in a complete batch fell back to scalar strum pre-roll");
    duplicate->noteOff(45);
    expect(TestAccess::snapshot(*duplicate, duplicateString).keyDown,
           "the first duplicate Note Off released both owners");
    duplicate->noteOff(45);
    expect(! TestAccess::snapshot(*duplicate, duplicateString).keyDown,
           "the second duplicate Note Off left the string held");

    auto held = std::make_unique<ElectryEngine>();
    held->prepare(sampleRate, 512);
    held->setParameters(EngineParameters {});
    held->noteOn(45, 0.8f);
    const int heldString = TestAccess::stringForNote(*held, 45);
    const std::array addedChord {
        ElectryEngine::NoteOnEvent { 40, 0.8f },
        ElectryEngine::NoteOnEvent { 50, 0.8f },
        ElectryEngine::NoteOnEvent { 55, 0.8f }
    };
    held->noteOnChord(addedChord);
    const auto heldVoice = TestAccess::snapshot(*held, heldString);
    expect(heldVoice.active && heldVoice.keyDown && heldVoice.midiNote == 45,
           "a chord batch displaced an existing held-string owner");

    // A constrained treble note can require a repeated held pitch to move to
    // another string. Its old and new Note Ons must move together: neither is
    // allowed to disappear when the original string is reassigned.
    auto revoiced = std::make_unique<ElectryEngine>();
    revoiced->prepare(sampleRate, 512);
    revoiced->setParameters(EngineParameters {});
    revoiced->noteOn(64, 0.8f);
    StereoBuffer beforeRevoice(64);
    renderInto(*revoiced, beforeRevoice);
    const std::array revoicedChord {
        ElectryEngine::NoteOnEvent { 64, 0.8f },
        ElectryEngine::NoteOnEvent { 82, 0.8f }
    };
    revoiced->noteOnChord(revoicedChord);
    const int movedString = TestAccess::stringForNote(*revoiced, 64);
    const int constrainedString = TestAccess::stringForNote(*revoiced, 82);
    expect(movedString == 4 && constrainedString == 7,
           "a constrained dyad did not re-finger its repeated held pitch ("
               + std::to_string(movedString) + ", "
               + std::to_string(constrainedString) + ")");
    revoiced->noteOff(64);
    expect(TestAccess::snapshot(*revoiced, movedString).keyDown,
           "re-fingering lost the original held-note owner");
    revoiced->noteOff(64);
    expect(! TestAccess::snapshot(*revoiced, movedString).keyDown,
           "re-fingered overlap survived both matching Note Offs");

    // A free but impossible stretch is not preferable to taking a string that
    // is already releasing. This shape fits exactly under the four-fret hand.
    auto releasedObstacle = std::make_unique<ElectryEngine>();
    releasedObstacle->prepare(sampleRate, 512);
    releasedObstacle->setParameters(EngineParameters {});
    releasedObstacle->noteOn(35, 0.8f);
    releasedObstacle->noteOff(35);
    const std::array reachableChord {
        ElectryEngine::NoteOnEvent { 41, 0.8f },
        ElectryEngine::NoteOnEvent { 42, 0.8f }
    };
    releasedObstacle->noteOnChord(reachableChord);
    expect(TestAccess::stringForNote(*releasedObstacle, 41) == 1
               && TestAccess::snapshot(*releasedObstacle, 1).fret == 6
               && TestAccess::stringForNote(*releasedObstacle, 42) == 2
               && TestAccess::snapshot(*releasedObstacle, 2).fret == 2,
           "allocator preferred an eleven-fret stretch over a releasing string");
}

// chooseString()'s steal branch (all eight strings already sounding) was
// only ever exercised for the voice count staying at eight; the tie-break
// policy itself - a releasing voice always outranks a held one, and among
// voices of equal status the one that has been sounding the longest goes
// first - had no direct coverage.
void testVoiceStealingPriority()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});

    // Drop-E open notes for strings 0..7, struck in that order so string 2
    // (E2, fret 22) is the oldest of the strings that can reach MIDI note 62
    // within the 22-fret range (frets 34/27/22/17/12/7/3/-2 respectively -
    // only strings 2 through 6 qualify).
    constexpr std::array<int, ElectryEngine::stringCount> openNotes {
        28, 35, 40, 45, 50, 55, 59, 64
    };

    // With every string held and none releasing, the steal must fall to the
    // oldest of the reachable strings.
    engine.reset();
    for (const int note : openNotes)
        engine.noteOn(note, 0.8f);
    engine.noteOn(62, 0.8f);
    expect(engine.getActiveVoiceCount() == ElectryEngine::stringCount,
           "stealing a held voice changed the active voice count");
    expect(TestAccess::stringForNote(engine, 62) == 2,
           "steal did not choose the oldest of the reachable held strings");
    expect(TestAccess::stringForNote(engine, 40) == -1,
           "the stolen string still reports its original note as active");

    // Releasing a younger reachable string (5, not the oldest) must steal
    // that one instead: a releasing voice outranks every held voice
    // regardless of how long either has been sounding.
    engine.reset();
    for (const int note : openNotes)
        engine.noteOn(note, 0.8f);
    engine.noteOff(55); // string 5: keyDown false, releasing, still active
    engine.noteOn(62, 0.8f);
    expect(engine.getActiveVoiceCount() == ElectryEngine::stringCount,
           "stealing a releasing voice changed the active voice count");
    expect(TestAccess::stringForNote(engine, 62) == 5,
           "steal preferred an older held string over a releasing one");
}

// noteOn()'s velocity guard - `clampf(std::isfinite(velocity) ? velocity :
// 0.0f, 0.0f, 1.0f)` - was only ever fed ordinary in-range velocities
// elsewhere in the suite; nothing asserted that a non-finite or negative
// velocity is folded down to silence, or that a velocity above 1.0 clamps
// to 1.0 rather than being rejected or left unclamped.
void testNoteOnVelocitySanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});

    // NaN and infinities fail std::isfinite and fall back to 0.0f; a
    // negative velocity clamps to 0.0f. Either way the following
    // `velocity <= 0.0f` gate in noteOn() then starts no voice at all.
    const auto expectNoVoiceStarted = [&] (float velocity, const char* label)
    {
        engine.reset();
        engine.noteOn(45, velocity);
        expect(engine.getActiveVoiceCount() == 0,
               std::string("noteOn started a voice for a ") + label
                   + " velocity");
    };
    expectNoVoiceStarted(std::nanf(""), "NaN");
    expectNoVoiceStarted(std::numeric_limits<float>::infinity(),
                          "positive-infinite");
    expectNoVoiceStarted(-std::numeric_limits<float>::infinity(),
                          "negative-infinite");
    expectNoVoiceStarted(-0.4f, "negative");

    // A finite velocity above 1.0 clamps to exactly 1.0 rather than being
    // rejected or driving the excitation harder than a full-velocity note
    // would: every downstream use (makeVelocityProfile and its callers)
    // only ever sees the value noteOn() itself already clamped, so the two
    // renders must be bit-identical.
    ElectryEngine reference;
    reference.prepare(sampleRate, 512);
    reference.setParameters(EngineParameters {});
    const auto inRange = renderNote(reference, sampleRate, 45, 1.0f,
                                     PlayStyle::Sustain, 0.5);

    ElectryEngine overshoot;
    overshoot.prepare(sampleRate, 512);
    overshoot.setParameters(EngineParameters {});
    const auto outOfRange = renderNote(overshoot, sampleRate, 45, 5.0f,
                                        PlayStyle::Sustain, 0.5);

    bool identical = true;
    for (std::size_t i = 0; i < inRange.left.size(); ++i)
        if (inRange.left[i] != outOfRange.left[i]
            || inRange.right[i] != outOfRange.right[i])
        {
            identical = false;
            break;
        }
    expect(identical,
           "a velocity above 1.0 was not clamped to the same render as 1.0");
    expect(peakAbs(inRange.left) > 1.0e-4f,
           "velocity clamp fixture rendered silence");
}

// setVibrato()'s own guard - `clampf(std::isfinite(normalised) ? normalised :
// 0.0f, 0.0f, 1.0f)` - is the gesture counterpart to setPitchBend,
// setResonance, setAcousticReturnLevel and setPalmMutePressure, all four of
// which testParameterSanitisation() above already drives with NaN. setVibrato
// itself is only ever called with ordinary in-range pressures (0.0f, 1.0f, or
// a drawn 0..1 value) everywhere else in the suite, so nothing asserted that a
// non-finite or out-of-[0,1] gesture value is folded down rather than
// latched into the fretting-hand vibrato target and, from there, into every
// stopped string's pitch.
void testSetVibratoSanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});

    // NaN and either infinity fail std::isfinite and fall back to 0.0f exactly
    // - the same "no boundary to clamp to" fallback the sample-rate and
    // EngineParameters guards use.
    engine.setVibrato(std::nanf(""));
    expect(TestAccess::vibratoTarget(engine) == 0.0f,
           "a NaN vibrato gesture did not fall back to zero");
    engine.setVibrato(std::numeric_limits<float>::infinity());
    expect(TestAccess::vibratoTarget(engine) == 0.0f,
           "a positive-infinite vibrato gesture did not fall back to zero");
    engine.setVibrato(-std::numeric_limits<float>::infinity());
    expect(TestAccess::vibratoTarget(engine) == 0.0f,
           "a negative-infinite vibrato gesture did not fall back to zero");

    // A finite value outside [0, 1] clamps to the nearer boundary rather than
    // being rejected or left unclamped.
    engine.setVibrato(-3.0f);
    expect(TestAccess::vibratoTarget(engine) == 0.0f,
           "a negative vibrato gesture did not clamp to zero");
    engine.setVibrato(7.5f);
    expect(TestAccess::vibratoTarget(engine) == 1.0f,
           "a vibrato gesture above 1.0 did not clamp to one");

    // An ordinary value still passes straight through, confirming the guard
    // is a genuine clamp rather than a filter that also stops valid input.
    engine.setVibrato(0.4f);
    expect(TestAccess::vibratoTarget(engine) == 0.4f,
           "an in-range vibrato gesture was altered by the guard");

    // And a hostile pressure held on a genuinely fingered, sounding string
    // must still render finite, bounded audio end to end rather than only
    // sanitising the stored target.
    engine.reset();
    engine.noteOn(47, 0.9f); // A2 + 2 frets, not an open string, so vibrato applies
    engine.setVibrato(std::nanf(""));
    StereoBuffer buffer(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, buffer);
    expect(allFinite(buffer),
           "a hostile vibrato gesture produced non-finite audio");
}

// setPitchBend()'s own guard - `2.0f * clampf(std::isfinite(bend) ? bend :
// 0.0f, -1.0f, 1.0f)` - is exercised elsewhere in the suite only with
// ordinary in-range bends (testDropELowNoteAtMaximumRate uses -1.0f, several
// glide/wheel tests use 0.0f or 1.0f) or, in testParameterSanitisation, with a
// NaN whose only assertion is that the resulting audio stays finite. Nothing
// checks the guard's own two distinct behaviours - a non-finite bend folds to
// zero rather than latching NaN into the bend target, and a finite
// out-of-[-1,1] bend clamps to the nearer boundary before being doubled to
// the +/-2 semitone range - the way testSetVibratoSanitisation already does
// for the vibrato-gesture guard right above.
void testSetPitchBendSanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});

    // NaN and either infinity fail std::isfinite and fall back to 0.0f before
    // the clamp, doubling to a 0.0f target - the same "no boundary to clamp
    // to" fallback the sample-rate, EngineParameters and vibrato guards use.
    engine.setPitchBend(std::nanf(""));
    expect(TestAccess::pitchBendTarget(engine) == 0.0f,
           "a NaN bend did not fall back to zero");
    engine.setPitchBend(std::numeric_limits<float>::infinity());
    expect(TestAccess::pitchBendTarget(engine) == 0.0f,
           "a positive-infinite bend did not fall back to zero");
    engine.setPitchBend(-std::numeric_limits<float>::infinity());
    expect(TestAccess::pitchBendTarget(engine) == 0.0f,
           "a negative-infinite bend did not fall back to zero");

    // A finite value outside [-1, 1] clamps to the nearer boundary, then
    // doubles to the +/-2 semitone bend range, rather than being rejected or
    // latched unclamped.
    engine.setPitchBend(-4.0f);
    expect(TestAccess::pitchBendTarget(engine) == -2.0f,
           "a bend below -1.0 did not clamp to -2 semitones");
    engine.setPitchBend(9.0f);
    expect(TestAccess::pitchBendTarget(engine) == 2.0f,
           "a bend above 1.0 did not clamp to +2 semitones");

    // An ordinary in-range bend still passes through the doubling unaltered,
    // confirming the guard is a genuine clamp rather than a filter that also
    // stops valid input.
    engine.setPitchBend(0.25f);
    expect(TestAccess::pitchBendTarget(engine) == 0.5f,
           "an in-range bend was altered by the guard");

    // And a hostile bend held on a genuinely fretted, sounding string must
    // still render finite, bounded audio end to end rather than only
    // sanitising the stored target.
    engine.reset();
    engine.noteOn(47, 0.9f); // A2 + 2 frets, not an open string
    engine.setPitchBend(std::nanf(""));
    StereoBuffer bendBuffer(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, bendBuffer);
    expect(allFinite(bendBuffer),
           "a hostile pitch bend produced non-finite audio");
}

// setResonance(), setAcousticReturnLevel() and setPalmMutePressure() share the
// exact same guard shape as setVibrato() above - `std::isfinite(value) ?
// clampf(value, 0.0f, 1.0f) : 0.0f` - and, like setVibrato() before it was
// covered directly, are only ever driven elsewhere in the suite with ordinary
// in-range levels, or in testParameterSanitisation() with a NaN whose only
// assertion is that the resulting audio stays finite. Nothing asserted that a
// non-finite level folds to zero rather than latching NaN into the resonance
// target, the acoustic-return target or the palm-mute pressure, or that an
// out-of-range level clamps to the nearer boundary rather than passing
// through unclamped. All three guards are checked together here since they
// are, byte for byte, the same guard three times over.
void testSetResonanceReturnLevelAndPalmMutePressureSanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});

    // NaN and either infinity fail std::isfinite and fall back to 0.0f exactly
    // - the same "no boundary to clamp to" fallback the other performance
    // guards use.
    engine.setResonance(std::nanf(""));
    expect(TestAccess::resonanceTarget(engine) == 0.0f,
           "a NaN resonance level did not fall back to zero");
    engine.setResonance(std::numeric_limits<float>::infinity());
    expect(TestAccess::resonanceTarget(engine) == 0.0f,
           "a positive-infinite resonance level did not fall back to zero");
    engine.setResonance(-std::numeric_limits<float>::infinity());
    expect(TestAccess::resonanceTarget(engine) == 0.0f,
           "a negative-infinite resonance level did not fall back to zero");

    engine.setAcousticReturnLevel(std::nanf(""));
    expect(TestAccess::returnLevelTarget(engine) == 0.0f,
           "a NaN acoustic-return level did not fall back to zero");
    engine.setAcousticReturnLevel(std::numeric_limits<float>::infinity());
    expect(TestAccess::returnLevelTarget(engine) == 0.0f,
           "a positive-infinite acoustic-return level did not fall back to "
           "zero");

    engine.setPalmMutePressure(std::nanf(""));
    expect(TestAccess::palmMutePressure(engine) == 0.0f,
           "a NaN palm-mute pressure did not fall back to zero");
    engine.setPalmMutePressure(-std::numeric_limits<float>::infinity());
    expect(TestAccess::palmMutePressure(engine) == 0.0f,
           "a negative-infinite palm-mute pressure did not fall back to "
           "zero");

    // A finite value outside [0, 1] clamps to the nearer boundary rather than
    // being rejected or left unclamped.
    engine.setResonance(-3.0f);
    expect(TestAccess::resonanceTarget(engine) == 0.0f,
           "a negative resonance level did not clamp to zero");
    engine.setResonance(8.5f);
    expect(TestAccess::resonanceTarget(engine) == 1.0f,
           "a resonance level above 1.0 did not clamp to one");

    engine.setAcousticReturnLevel(-2.0f);
    expect(TestAccess::returnLevelTarget(engine) == 0.0f,
           "a negative acoustic-return level did not clamp to zero");
    engine.setAcousticReturnLevel(6.0f);
    expect(TestAccess::returnLevelTarget(engine) == 1.0f,
           "an acoustic-return level above 1.0 did not clamp to one");

    engine.setPalmMutePressure(-1.5f);
    expect(TestAccess::palmMutePressure(engine) == 0.0f,
           "a negative palm-mute pressure did not clamp to zero");
    engine.setPalmMutePressure(4.0f);
    expect(TestAccess::palmMutePressure(engine) == 1.0f,
           "a palm-mute pressure above 1.0 did not clamp to one");

    // An ordinary value still passes straight through, confirming each guard
    // is a genuine clamp rather than a filter that also stops valid input.
    engine.setResonance(0.35f);
    expect(TestAccess::resonanceTarget(engine) == 0.35f,
           "an in-range resonance level was altered by the guard");
    engine.setAcousticReturnLevel(0.65f);
    expect(TestAccess::returnLevelTarget(engine) == 0.65f,
           "an in-range acoustic-return level was altered by the guard");
    engine.setPalmMutePressure(0.5f);
    expect(TestAccess::palmMutePressure(engine) == 0.5f,
           "an in-range palm-mute pressure was altered by the guard");

    // And hostile levels held together on a genuinely fretted, sounding
    // string must still render finite, bounded audio end to end rather than
    // only sanitising the three stored targets.
    engine.reset();
    engine.setResonance(1.0f);
    engine.setAcousticReturnLevel(1.0f);
    engine.noteOn(47, 0.9f); // A2 + 2 frets, not an open string
    engine.setResonance(std::nanf(""));
    engine.setAcousticReturnLevel(std::nanf(""));
    engine.setPalmMutePressure(std::nanf(""));
    StereoBuffer levelBuffer(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, levelBuffer);
    expect(allFinite(levelBuffer),
           "hostile resonance/return/palm-mute levels produced non-finite "
           "audio");
}

// DelayTap::setDelay - the cubic-Lagrange fractional read shared by every
// played and sympathetic pickup tap - clamps its request to
// [4, delayLineSize - 8] before solving four interpolation coefficients from
// the clamped delay's fractional part. Nothing in the suite ever asked it for
// a delay outside that range directly, or checked the coefficients
// themselves rather than the pickup audio they eventually shape, so a clamp
// landing on the wrong boundary or a sign error in the
// Lagrange solve would still have passed every existing test.
void testDelayTapClampsAndInterpolates()
{
    // A request below the 4-sample floor (a cubic tap needs two samples on
    // each side) clamps to exactly the same coefficients an explicit
    // request for the floor itself would solve. Pin the floor snapshot
    // itself to offset 4 with unit-tap coefficients (rather than only
    // comparing two requests that are both subject to the same clamp), so a
    // floor that silently moved to, say, 5 would fail here even though
    // delayTapAt(1.0f) and delayTapAt(4.0f) would still agree with each
    // other.
    const auto belowFloor = TestAccess::delayTapAt(1.0f);
    const auto atFloor = TestAccess::delayTapAt(4.0f);
    expect(atFloor.offset == 4 && atFloor.c0 == 0.0f && atFloor.c1 == 1.0f
               && atFloor.c2 == 0.0f && atFloor.c3 == 0.0f,
           "the 4-sample floor itself did not solve to a unit tap at offset 4");
    expect(belowFloor.offset == atFloor.offset && belowFloor.c0 == atFloor.c0
               && belowFloor.c1 == atFloor.c1 && belowFloor.c2 == atFloor.c2
               && belowFloor.c3 == atFloor.c3,
           "a delay below the 4-sample floor was not clamped to it");

    // A request past the delayLineSize - 8 ceiling (room for the same
    // two-sample margin at the top of the ring) clamps the same way. Pin the
    // ceiling snapshot itself for the same reason as the floor above.
    const int ceilingOffset = TestAccess::delayLineCapacity() - 8;
    const float ceiling = static_cast<float>(ceilingOffset);
    const auto aboveCeiling = TestAccess::delayTapAt(ceiling + 500.0f);
    const auto atCeiling = TestAccess::delayTapAt(ceiling);
    expect(atCeiling.offset == ceilingOffset && atCeiling.c0 == 0.0f
               && atCeiling.c1 == 1.0f && atCeiling.c2 == 0.0f
               && atCeiling.c3 == 0.0f,
           "the delayLineSize-8 ceiling itself did not solve to a unit tap "
           "at the expected offset");
    expect(aboveCeiling.offset == atCeiling.offset
               && aboveCeiling.c0 == atCeiling.c0
               && aboveCeiling.c1 == atCeiling.c1
               && aboveCeiling.c2 == atCeiling.c2
               && aboveCeiling.c3 == atCeiling.c3,
           "a delay past the delayLineSize-8 ceiling was not clamped to it");

    // An exact integer delay needs no interpolation at all, so the four
    // weights must collapse to a single unit tap rather than spreading
    // across neighbouring samples.
    const auto exact = TestAccess::delayTapAt(10.0f);
    expect(exact.offset == 10,
           "an exact-integer delay solved the wrong tap offset");
    expect(exact.c0 == 0.0f && exact.c1 == 1.0f && exact.c2 == 0.0f
               && exact.c3 == 0.0f,
           "an exact-integer delay did not collapse to a single unit tap");

    // A fractional delay's offset lands at the request's ceiling (the read
    // arithmetic the loop actually uses), and its four weights are pinned to
    // the closed-form cubic Lagrange basis for t = ceil(10.25) - 10.25 =
    // 0.75, computed independently of DelayTap::setDelay's own formula, not
    // just their sum: an implementation that always returned the exact-tap
    // weights {0, 1, 0, 0} would still sum to unity while turning every
    // fractional read into an incorrect integer one.
    const auto fractional = TestAccess::delayTapAt(10.25f);
    expect(fractional.offset == 11,
           "a fractional delay's offset was not the request's ceiling");
    expect(std::abs(fractional.c0 - (-0.0390625f)) < 1.0e-6f
               && std::abs(fractional.c1 - 0.2734375f) < 1.0e-6f
               && std::abs(fractional.c2 - 0.8203125f) < 1.0e-6f
               && std::abs(fractional.c3 - (-0.0546875f)) < 1.0e-6f,
           "a fractional delay's interpolation weights did not match the "
           "closed-form Lagrange basis for t = 0.75");
    const float sum =
        fractional.c0 + fractional.c1 + fractional.c2 + fractional.c3;
    expect(std::abs(sum - 1.0f) < 1.0e-5f,
           "a fractional delay's interpolation weights did not sum to unity");
}

// The natural harmonic is a finger resting on a node, not a transposition.
// The distinction is measurable in three places: which partials survive, that
// the loop still runs at the fretted pitch (so the surviving partial decays at
// the rate that partial has when the note is picked ordinarily), and that the
// filter doing it cannot exceed unity gain anywhere.
void testTouchHarmonics()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);

    // The ideal touch law is (1 - d/2) + (d/2) z^-M. Both mix coefficients are
    // non-negative and sum to one, so its closed-form magnitude is bounded by
    // one at every frequency and depth. The complete render tests below own the
    // cubic fractional-read implementation and decay behavior separately.
    double worstMagnitude = 0.0;
    for (int depthStep = 0; depthStep <= 20; ++depthStep)
    {
        const double depth = 0.05 * depthStep;
        for (int phaseStep = 0; phaseStep <= 720; ++phaseStep)
        {
            const double angle = 3.14159265358979323846 * phaseStep / 360.0;
            const double real = (1.0 - 0.5 * depth) + 0.5 * depth * std::cos(angle);
            const double imag = -0.5 * depth * std::sin(angle);
            worstMagnitude = std::max(worstMagnitude, std::hypot(real, imag));
        }
    }
    expect(worstMagnitude <= 1.0 + 1.0e-12,
           "the touch filter exceeds unity gain somewhere ("
               + std::to_string(worstMagnitude) + ")");

    // The touch is exactly absent for every other articulation, so an ordinary
    // note pays neither the arithmetic nor a change of sound.
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(45, 0.8f);
    const int sustainString = TestAccess::stringForNote(engine, 45);
    expect(sustainString >= 0
               && TestAccess::touchDepth(engine, sustainString) == 0.0f,
           "an ordinary picked note put a finger on the string");

    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Harmonics), 1.0f);
    engine.noteOn(45, 0.8f);
    const int harmonicString = TestAccess::stringForNote(engine, 45);
    expect(harmonicString >= 0
               && TestAccess::touchDepth(engine, harmonicString) > 0.5f
               && std::abs(TestAccess::touchFraction(engine, harmonicString)
                           - 0.5f) < 1.0e-6f,
           "the harmonic did not place a finger on the midpoint node");
    {
        const auto geometry = TestAccess::touchGeometry(
            engine, harmonicString);
        const float expectedVerticalPeriod = geometry.verticalRawPeriod
            + geometry.soundingPeriod - geometry.verticalCompensatedPeriod;
        const float expectedHorizontalPeriod = geometry.horizontalRawPeriod
            + geometry.soundingPeriod - geometry.horizontalCompensatedPeriod;
        expect(geometry.valid
                   && std::abs(geometry.verticalReadDelay
                               - geometry.verticalRawPeriod
                               - geometry.fraction * expectedVerticalPeriod)
                          < 1.0e-6f * expectedVerticalPeriod
                   && std::abs(geometry.horizontalReadDelay
                               - geometry.horizontalRawPeriod
                               - geometry.fraction * expectedHorizontalPeriod)
                          < 1.0e-6f * expectedHorizontalPeriod,
               "the touch tap did not use each polarisation's complete live "
               "sounding period");
    }

    // Loop-filter phase changes the raw delay but cannot move a finger planted
    // at the midpoint. The previous p*raw-delay mapping misses this invariant
    // by more than a hundred samples on an aged open E1.
    const auto lowTouchGeometry = [] (float stringAge, float pitchBend)
    {
        ElectryEngine low;
        low.prepare(sampleRate, 512);
        EngineParameters lowParameters;
        lowParameters.stringAge = stringAge;
        lowParameters.artifactAmount = 0.0f;
        lowParameters.bodyResonance = 0.0f;
        lowParameters.pickNoise = 0.0f;
        lowParameters.fingerNoise = 0.0f;
        lowParameters.releaseNoise = 0.0f;
        low.setParameters(lowParameters);
        low.setPitchBend(pitchBend);
        low.reset();
        low.noteOn(styleKeyswitch(PlayStyle::Harmonics), 1.0f);
        low.noteOn(28, 0.8f);
        const int stringIndex = TestAccess::stringForNote(low, 28);
        expect(stringIndex >= 0, "the low touch fixture was not allocated");
        return TestAccess::touchGeometry(low, stringIndex);
    };
    const auto freshLowTouch = lowTouchGeometry(0.0f, 0.0f);
    const auto oldLowTouch = lowTouchGeometry(1.0f, 0.0f);
    const auto bentLowTouch = lowTouchGeometry(0.0f, 1.0f);
    const auto touchSeparation = [] (const auto& geometry)
    {
        return geometry.verticalReadDelay - geometry.verticalRawPeriod;
    };
    expect(std::abs(freshLowTouch.verticalRawPeriod
                    - oldLowTouch.verticalRawPeriod) > 1.0f,
           "the touch String Age fixture did not move loop-filter phase");
    expect(std::abs(touchSeparation(freshLowTouch)
                    - 0.5f * freshLowTouch.soundingPeriod)
                   < 1.0e-6f * freshLowTouch.soundingPeriod
               && std::abs(touchSeparation(oldLowTouch)
                           - 0.5f * oldLowTouch.soundingPeriod)
                   < 1.0e-6f * oldLowTouch.soundingPeriod
               && std::abs(touchSeparation(freshLowTouch)
                           - touchSeparation(oldLowTouch))
                   < 1.0e-6f * freshLowTouch.soundingPeriod,
           "String Age moved the physical midpoint touch");
    const float expectedBendScale = std::exp2(-2.0f / 12.0f);
    expect(std::abs(touchSeparation(bentLowTouch)
                        / touchSeparation(freshLowTouch)
                        - expectedBendScale) < 2.0e-5f,
           "a two-semitone pre-bend did not move the touch tap with 1/c");

    // A fixed picking hand can legitimately lie past the speaking string's
    // midpoint on a fretted note. Keep that geometric fraction and its modal
    // phase instead of snapping it to 49%. The existing positive pP image is
    // still within the delay-line allocation at every supported rate and bend
    // extreme, including the low string where the period is longest.
    for (const double hostRate : { 44100.0, 48000.0, 96000.0, 96001.0,
                                   192000.0, 384000.0 })
    {
        for (const float bend : { -1.0f, 0.0f, 1.0f })
        {
            ElectryEngine farSide;
            farSide.prepare(hostRate, 512);
            auto farParameters = parameters;
            farParameters.pickPosition = 1.0f;
            farSide.setParameters(farParameters);
            farSide.setPitchBend(bend);
            farSide.reset();
            farSide.noteOn(styleKeyswitch(PlayStyle::Pinch), 1.0f);
            farSide.noteOn(30, 0.8f); // fret 2 on the unique lowest string
            const int stringIndex = TestAccess::stringForNote(farSide, 30);
            const auto geometry = TestAccess::touchGeometry(
                farSide, std::max(stringIndex, 0));
            const float verticalPeriod = geometry.verticalRawPeriod
                + geometry.soundingPeriod
                - geometry.verticalCompensatedPeriod;
            const float horizontalPeriod = geometry.horizontalRawPeriod
                + geometry.soundingPeriod
                - geometry.horizontalCompensatedPeriod;
            const float expectedVertical = geometry.verticalRawPeriod
                + geometry.fraction * verticalPeriod;
            const float expectedHorizontal = geometry.horizontalRawPeriod
                + geometry.fraction * horizontalPeriod;
            const float maximumRead = static_cast<float>(
                TestAccess::delayLineCapacity() - 8);
            expect(stringIndex == 0 && geometry.fraction > 0.5f
                       && geometry.fraction < 0.98f,
                   "the far-side touch fixture did not retain its physical "
                   "lowest-string pick position");
            expect(std::abs(geometry.verticalReadDelay - expectedVertical)
                           < 1.0e-6f * verticalPeriod
                       && std::abs(geometry.horizontalReadDelay
                                   - expectedHorizontal)
                              < 1.0e-6f * horizontalPeriod,
                   "a far-side touch left the positive full-period pP image");
            expect(geometry.verticalReadDelay >= 4.0f
                       && geometry.horizontalReadDelay >= 4.0f
                       && geometry.verticalReadDelay <= maximumRead
                       && geometry.horizontalReadDelay <= maximumRead,
                   "a far-side touch exceeded the allocated delay line at "
                       + std::to_string(hostRate) + " Hz");
        }
    }

    // A wheel move after contact exercises the other half of the formula:
    // currentDelay is deliberately between its old and new targets. The touch
    // must follow that live period, not jump to the target and not fall back to
    // a fraction of raw delay.
    ElectryEngine gliding;
    gliding.prepare(sampleRate, 512);
    gliding.setParameters(parameters);
    gliding.reset();
    gliding.noteOn(styleKeyswitch(PlayStyle::Harmonics), 1.0f);
    gliding.noteOn(28, 0.8f);
    const int glidingString = TestAccess::stringForNote(gliding, 28);
    gliding.setPitchBend(1.0f);
    StereoBuffer bendStart(64);
    renderInto(gliding, bendStart);
    const auto glidingTouch = TestAccess::touchGeometry(
        gliding, glidingString);
    const float glidingPhysicalPeriod = glidingTouch.verticalRawPeriod
        + glidingTouch.soundingPeriod
        - glidingTouch.verticalCompensatedPeriod;
    expect(std::abs(glidingTouch.verticalRawPeriod
                    - glidingTouch.verticalRawTarget) > 1.0e-3f,
           "the in-flight touch fixture did not leave the delay smoother live");
    expect(std::abs(touchSeparation(glidingTouch)
                    - glidingTouch.fraction * glidingPhysicalPeriod)
                   < 1.0e-6f * glidingPhysicalPeriod
               && std::abs(glidingPhysicalPeriod
                           - glidingTouch.soundingPeriod) > 1.0e-3f,
           "the touch jumped away from the in-flight physical period");
    std::cout << "PROBE touch raw/full ratios: fresh E1 "
              << freshLowTouch.verticalRawPeriod
                    / freshLowTouch.soundingPeriod
              << ", old E1 "
              << oldLowTouch.verticalRawPeriod / oldLowTouch.soundingPeriod
              << '\n';
    {
        // The finger lifts and the extra reads stop; the harmonic keeps
        // ringing because the partials it removed cannot come back.
        StereoBuffer settle(static_cast<int>(0.5 * sampleRate));
        renderInto(engine, settle);
        expect(TestAccess::touchDepth(engine, harmonicString) == 0.0f,
               "the touching finger never lifted");
    }

    const double f0 = midiHz(45);
    const auto sustain = renderNote(engine, sampleRate, 45, 0.8f,
                                    PlayStyle::Sustain, 2.4);
    const auto harmonic = renderNote(engine, sampleRate, 45, 0.8f,
                                     PlayStyle::Harmonics, 2.4);

    const int bodyStart = static_cast<int>(0.10 * sampleRate);
    const int bodyLength = static_cast<int>(0.35 * sampleRate);
    const auto partial = [&] (const StereoBuffer& buffer, int n)
    {
        return dftMagnitude(buffer.left, bodyStart, bodyLength, sampleRate,
                            f0 * n);
    };

    // The ideal midpoint law puts odd partials at an antinode and even ones at
    // a node. The cubic temporal surrogate must still produce that strong
    // selection on the dispersive rendered string, which is why the octave
    // appears without retuning the loop.
    const double harmonicSecond = partial(harmonic, 2);
    for (const int odd : { 1, 3, 5 })
    {
        const double suppression = decibels(partial(harmonic, odd)
                                            / std::max(harmonicSecond, 1.0e-15));
        expect(suppression < -20.0,
               "partial " + std::to_string(odd)
                   + " survived the midpoint node touch at "
                   + std::to_string(suppression) + " dB");
    }
    expect(decibels(partial(harmonic, 4) / std::max(harmonicSecond, 1.0e-15))
               > -20.0,
           "the fourth partial, which has a node under the finger, was removed");

    // The fundamental is present in the ordinary picked note, so the
    // suppression above is the touch rather than a property of the string.
    expect(decibels(partial(sustain, 1)
                    / std::max(partial(sustain, 2), 1.0e-15)) > -20.0,
           "the picked reference note has no fundamental to suppress");

    // The loop still runs at the fretted pitch, so the surviving octave
    // partial decays at the rate that partial has when the string is picked
    // normally. A model that retuned the loop an octave up would give it the
    // decay of a much shorter string instead.
    const auto decayDb = [&] (const StereoBuffer& buffer)
    {
        const double early = dftMagnitude(buffer.left, bodyStart, bodyLength,
                                          sampleRate, 2.0 * f0);
        const double late = dftMagnitude(
            buffer.left, static_cast<int>(1.6 * sampleRate), bodyLength,
            sampleRate, 2.0 * f0);
        return decibels(late / std::max(early, 1.0e-15));
    };
    const double harmonicDecay = decayDb(harmonic);
    const double sustainDecay = decayDb(sustain);
    expect(std::abs(harmonicDecay - sustainDecay) < 2.0,
           "the harmonic's octave partial does not decay like the same partial "
           "of the picked note (harmonic " + std::to_string(harmonicDecay)
               + " dB, picked " + std::to_string(sustainDecay) + " dB)");
}

// A dead note is a real pick stroke with the fretting hand lying across the
// strings. On the open Drop-E eighth string the lawful real reference is not a
// noise click or a gate: it is a dark, strongly periodic E1 thunk whose body
// falls through roughly 250 ms. Keep that picked onset, residual low-string
// state and multiband decay apart from the bridge hand's Palm Mute style.
void testDeadNote()
{
    constexpr double sampleRate = 44100.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.outputMode = electry::OutputMode::Mono;
    parameters.sympatheticAmount = 0.0f;
    parameters.palmMute = 0.0f;
    parameters.strumSpreadSeconds = 0.0f;
    engine.setParameters(parameters);

    const int note = 28;
    const double f0 = midiHz(note);
    const auto picked = renderNote(engine, sampleRate, note, 0.85f,
                                   PlayStyle::Sustain, 0.6);
    const auto dead = renderNote(engine, sampleRate, note, 0.85f,
                                 PlayStyle::Dead, 0.6);

    // The pick lands exactly as hard: what is different is what happens after
    // it, so the first thirty milliseconds are within a few decibels.
    const int attackEnd = static_cast<int>(0.030 * sampleRate);
    const double pickedAttack = peakAbs(picked.left, 0, attackEnd);
    const double deadAttack = peakAbs(dead.left, 0, attackEnd);
    const double attackGap = decibels(deadAttack / std::max(pickedAttack, 1e-15));
    std::cerr << "PROBE dead attack " << attackGap << " dB\n";
    expect(std::abs(attackGap) < 4.0,
           "a dead note does not land like a picked one ("
               + std::to_string(attackGap) + " dB)");

    // The four CC0 E1 ghost hits span +1.17..-10.12 dB in 30-100 ms and
    // -6.20..-20.68 dB in 100-250 ms, each against its own 0-30 ms body. Keep
    // a small tolerance for the unknown force/contact and MP3 preview, while
    // rejecting both the old -34/-51 dB click and a merely sustained note.
    const int middleStart = attackEnd;
    const int middleEnd = static_cast<int>(0.100 * sampleRate);
    const int tailEnd = static_cast<int>(0.250 * sampleRate);
    const int afterEnd = static_cast<int>(0.380 * sampleRate);
    const double early = rmsInRange(dead.left, 0, attackEnd);
    const double middle = rmsInRange(dead.left, middleStart, middleEnd);
    const double tail = rmsInRange(dead.left, middleEnd, tailEnd);
    const double after = rmsInRange(dead.left, tailEnd, afterEnd);
    const double middleDb = decibels(middle / std::max(early, 1e-15));
    const double tailDb = decibels(tail / std::max(early, 1e-15));
    const double afterDb = decibels(after / std::max(early, 1e-15));
    expect(middleDb > -12.0 && middleDb < 2.5,
           "the E1 dead-note middle left the real ghost range ("
               + std::to_string(middleDb) + " dB)");
    expect(tailDb > -24.0 && tailDb < -5.0,
           "the E1 dead-note tail left the real ghost range ("
               + std::to_string(tailDb) + " dB)");
    expect(afterDb > -34.0 && afterDb < tailDb - 2.0,
           "the E1 dead-note body did not continue its natural decay ("
               + std::to_string(afterDb) + " dB)");

    // By 100-250 ms the real ghost has 99.95% or more of its measured power
    // below 500 Hz and a median centroid near 85 Hz. The harmonic-series
    // fixture uses a stricter 250 Hz split: it must be overwhelmingly low, but
    // the fundamental must still be measurable and in tune. This is the
    // low-string thunk the old "no partial survives" assertion erased.
    const auto tailBalance = measureHarmonicBalance(
        dead.left, middleEnd, tailEnd - middleEnd, sampleRate, f0);
    const double measured = measureFrequency(
        dead.left, middleEnd, tailEnd - middleEnd, sampleRate, f0);
    expect(tailBalance.lowPowerShare > 0.94,
           "the E1 dead-note tail stayed too bright ("
               + std::to_string(tailBalance.lowPowerShare) + " below 250 Hz)");
    expect(std::abs(centsBetween(measured, f0)) < 70.0,
           "the residual E1 dead-note body lost its physical pitch ("
               + std::to_string(centsBetween(measured, f0)) + " cents)");

    const double deadAgainstPicked = decibels(
        tail / std::max(rmsInRange(picked.left, middleEnd, tailEnd), 1e-15));
    expect(deadAgainstPicked < -8.0 && deadAgainstPicked > -27.0,
           "the dead-note tail no longer separates from Sustain ("
               + std::to_string(deadAgainstPicked) + " dB)");
    std::cout << "PROBE real-backed E1 dead envelope: 30-100 "
              << middleDb << " dB, 100-250 " << tailDb
              << " dB, 250-380 " << afterDb << " dB; tail "
              << tailBalance.lowPowerShare * 100.0 << "% below 250 Hz, "
              << deadAgainstPicked << " dB versus Sustain\n";

    // The reference ghosts are not isolated samples: Open is repicked as Palm,
    // then Dead is struck twice while the previous state is still in the
    // string. Recreate both annotated passes without inferred note-offs and
    // keep all four real-style repicks inside the same envelope bounds as the
    // isolated fixture.
    ElectryEngine phrase;
    phrase.prepare(sampleRate, 512);
    phrase.setParameters(parameters);
    phrase.reset();
    phrase.noteOn(pickKeyswitch(PickStyle::Down), 1.0f);
    phrase.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    phrase.noteOn(note, 0.95f);
    StereoBuffer openToPalm(static_cast<int>(1.196145 * sampleRate));
    renderInto(phrase, openToPalm);
    phrase.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    phrase.noteOn(note, 0.95f);
    StereoBuffer palmToGhost(static_cast<int>(1.332381 * sampleRate));
    renderInto(phrase, palmToGhost);
    phrase.noteOn(styleKeyswitch(PlayStyle::Dead), 1.0f);
    phrase.noteOn(note, 0.95f);
    StereoBuffer firstGhost(static_cast<int>(0.415987 * sampleRate));
    renderInto(phrase, firstGhost);
    phrase.noteOn(note, 0.95f);
    StereoBuffer secondGhost(static_cast<int>(0.419361 * sampleRate));
    renderInto(phrase, secondGhost);
    phrase.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    phrase.noteOn(note, 0.95f);
    StereoBuffer secondOpenToPalm(static_cast<int>(1.267870 * sampleRate));
    renderInto(phrase, secondOpenToPalm);
    phrase.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    phrase.noteOn(note, 0.95f);
    StereoBuffer secondPalmToGhost(static_cast<int>(1.362927 * sampleRate));
    renderInto(phrase, secondPalmToGhost);
    phrase.noteOn(styleKeyswitch(PlayStyle::Dead), 1.0f);
    phrase.noteOn(note, 0.95f);
    StereoBuffer thirdGhost(static_cast<int>(0.407302 * sampleRate));
    renderInto(phrase, thirdGhost);
    phrase.noteOn(note, 0.95f);
    StereoBuffer fourthGhost(static_cast<int>(0.380 * sampleRate));
    renderInto(phrase, fourthGhost);

    const auto phraseEnvelope = [&] (const StereoBuffer& audio)
    {
        const double phraseEarly = rmsInRange(audio.left, 0, attackEnd);
        return std::array<double, 3> {
            decibels(rmsInRange(audio.left, middleStart, middleEnd)
                     / std::max(phraseEarly, 1e-15)),
            decibels(rmsInRange(audio.left, middleEnd, tailEnd)
                     / std::max(phraseEarly, 1e-15)),
            decibels(rmsInRange(audio.left, tailEnd, afterEnd)
                     / std::max(phraseEarly, 1e-15))
        };
    };
    const std::array<std::array<double, 3>, 4> phraseEnvelopes {
        phraseEnvelope(firstGhost), phraseEnvelope(secondGhost),
        phraseEnvelope(thirdGhost), phraseEnvelope(fourthGhost)
    };
    for (const auto& envelope : phraseEnvelopes)
    {
        expect(envelope[0] > -12.0 && envelope[0] < 2.5
                   && envelope[1] > -24.0 && envelope[1] < -5.0
                   && envelope[2] > -34.0 && envelope[2] < envelope[1] - 2.0,
               "a stateful E1 ghost repick left the real envelope range");
    }
    // The aggregate median can hide the repeat context. In both real passes
    // the second Dead attack was louder in its own 0-30 ms reference window
    // yet lost substantially more normalized body afterward. Preserve that
    // six-value comparison as an explicit gap: future contact-state work may
    // improve it, but may not make it worse while still matching a median.
    constexpr std::array<std::array<double, 3>, 2> documentedRepickDelta {{
        {{ -6.908, -4.344, -3.268 }},
        {{ -5.093, -2.790, -2.889 }}
    }};
    double contextualSquaredError = 0.0;
    for (std::size_t pass = 0; pass < documentedRepickDelta.size(); ++pass)
    {
        const auto& firstInPass = phraseEnvelopes[2 * pass];
        const auto& secondInPass = phraseEnvelopes[2 * pass + 1];
        for (std::size_t window = 0; window < firstInPass.size(); ++window)
        {
            const double modelDelta = secondInPass[window]
                                    - firstInPass[window];
            const double error = modelDelta
                               - documentedRepickDelta[pass][window];
            contextualSquaredError += error * error;
        }
    }
    const double contextualRmse = std::sqrt(
        contextualSquaredError / 6.0);
    // Restoring absolute chord tuning moved this fixed-window score from 4.559
    // to 4.928 dB because the rejected moving-pitch path had contributed phase
    // to the old windows. All three aggregate envelopes remain inside the four
    // real hits' ranges. Re-freeze at the nearest tenth instead of retuning
    // Dead from one uncontrolled performance to buy back the old snapshot.
    constexpr double maximumContextualRmse =
#if ELECTRY_ENERGY_ATTACK_PITCH
        5.10;
#else
        5.0;
#endif
    expect(contextualRmse < maximumContextualRmse,
           "the stateful Dead first-to-repick contrast regressed ("
               + std::to_string(contextualRmse) + " dB RMSE)");
    std::array<double, 3> phraseMedian {};
    for (std::size_t window = 0; window < phraseMedian.size(); ++window)
    {
        std::array<double, 4> values {
            phraseEnvelopes[0][window], phraseEnvelopes[1][window],
            phraseEnvelopes[2][window], phraseEnvelopes[3][window]
        };
        std::sort(values.begin(), values.end());
        phraseMedian[window] = 0.5 * (values[1] + values[2]);
    }
    // Moving the excitation image into the physical-period coordinate changes
    // this upstream source snapshot without changing a Dead coefficient. The
    // shipping median moved from -8.114/-14.919/-23.040 dB to the values below;
    // against the public four-hit median (-3.57/-12.66/-20.75 dB), its
    // three-window RMSE improves from 3.21 to 2.51 dB. The broad per-hit real
    // ranges above remain the actual acceptance rails.
    constexpr std::array<double, 3> documentedMedian {
#if ELECTRY_ENERGY_ATTACK_PITCH
        -7.474, -14.286, -22.573
#else
        -7.466, -14.073, -22.059
#endif
    };
    for (std::size_t window = 0; window < phraseMedian.size(); ++window)
    {
        expect(std::abs(phraseMedian[window] - documentedMedian[window]) < 0.35,
               "the reproducible four-hit Dead median no longer matches the "
               "evaluation table in window " + std::to_string(window));
    }
    std::cout << "PROBE stateful two-pass Open/Palm/Dead/Dead E1 medians: "
              << phraseMedian[0] << "/" << phraseMedian[1] << "/"
              << phraseMedian[2] << " dB; first-to-repick contextual RMSE "
              << contextualRmse << " dB\n";

    // Palm Pressure is the independent bridge hand and may stack with Dead.
    // It must tighten that articulation continuously; the old >10% impact
    // threshold made 10.1% jump much farther than the equal step below it.
    const auto renderPressure = [&] (float pressure)
    {
        ElectryEngine probe;
        probe.prepare(sampleRate, 512);
        probe.setParameters(parameters);
        probe.setPalmMutePressure(pressure);
        return renderNote(probe, sampleRate, note, 0.85f,
                          PlayStyle::Dead, 0.25);
    };
    constexpr std::array<float, 7> pressures {
        0.0f, 0.099f, 0.100f, 0.101f, 0.25f, 0.50f, 1.0f
    };
    std::array<StereoBuffer, pressures.size()> pressureRenders {
        renderPressure(pressures[0]), renderPressure(pressures[1]),
        renderPressure(pressures[2]), renderPressure(pressures[3]),
        renderPressure(pressures[4]), renderPressure(pressures[5]),
        renderPressure(pressures[6])
    };
    std::array<double, pressures.size()> pressureRms {};
    for (std::size_t i = 0; i < pressures.size(); ++i)
    {
        pressureRms[i] = rmsInRange(
            pressureRenders[i].left, static_cast<int>(0.020 * sampleRate),
            static_cast<int>(0.100 * sampleRate));
        if (i > 0)
            expect(pressureRms[i] <= pressureRms[i - 1] * 1.001,
                   "Palm Pressure made Dead louder at "
                       + std::to_string(pressures[i]));
    }
    const double belowStep = normalisedDifferenceRms(
        pressureRenders[1].left, pressureRenders[2].left, 0,
        static_cast<int>(0.020 * sampleRate));
    const double aboveStep = normalisedDifferenceRms(
        pressureRenders[2].left, pressureRenders[3].left, 0,
        static_cast<int>(0.020 * sampleRate));
    expect(aboveStep < 3.0 * std::max(belowStep, 1.0e-12),
           "Dead + Palm Pressure still jumps across 10% ("
               + std::to_string(belowStep) + " -> "
               + std::to_string(aboveStep) + ")");
    std::cout << "PROBE Dead + Palm Pressure 20-100 ms: "
              << decibels(pressureRms.back()
                           / std::max(pressureRms.front(), 1e-15))
              << " dB at full pressure; 10% neighbour deltas "
              << belowStep << "/" << aboveStep << '\n';
}

// Channel pressure is the fretting hand leaning into the string it is holding.
// A finger is not the bar: it moves only what it is fingering, it can push a
// string sharp and cannot pull it below the fret, and it takes a moment to
// start.
void testFrettingHandVibrato()
{
    constexpr double sampleRate = 48000.0;

    // Track the sounding delay target, which is what the vibrato actually
    // drives; the audio follows it, and it is sampled far more cheaply and
    // precisely than a frequency estimate on a moving tone.
    struct Trace
    {
        std::vector<float> target;
        int stringIndex { -1 };
    };
    const auto traceOf = [&] (float pressure, double seconds)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(47, 0.85f);
        engine.setVibrato(pressure);

        Trace trace;
        trace.stringIndex = TestAccess::stringForNote(engine, 47);
        constexpr int chunk = 32;
        const int total = static_cast<int>(seconds * sampleRate);
        StereoBuffer scratch(chunk);
        for (int at = 0; at < total; at += chunk)
        {
            engine.process(scratch.left.data(), scratch.right.data(), chunk);
            trace.target.push_back(
                trace.stringIndex >= 0
                    ? TestAccess::snapshot(engine,
                                           trace.stringIndex).verticalDelayTarget
                    : 0.0f);
        }
        return trace;
    };

    const auto still = traceOf(0.0f, 1.6);
    const auto moving = traceOf(1.0f, 1.6);
    expect(still.stringIndex == 3 && moving.stringIndex == 3,
           "the vibrato fixture did not land on the open A string");

    // Vibrato Depth has to reach the engine while it is playing, not only at
    // the next reset. A host automating it, or any caller moving it through
    // setParameters() after prepare(), is the ordinary case rather than the
    // exotic one: the control tick propagates every other continuous
    // parameter, and this one was left off that list when the fretting hand
    // was added, so the depth stayed at whatever the last reset latched.
    //
    // The comparison is differential because depth 0 is not silence: the
    // excursion is lerp(0.10, 1.10, depth) semitones, so a note at depth 0
    // still moves about 10 cents. Two engines are reset identically at depth
    // 0 and only one is raised afterwards, so the single difference between
    // them is the propagation this guards.
    {
        const auto excursionCents = [&] (bool raiseAfterReset)
        {
            ElectryEngine engine;
            engine.prepare(sampleRate, 512);
            EngineParameters parameters;
            parameters.artifactAmount = 0.0f;
            parameters.sympatheticAmount = 0.0f;
            parameters.vibratoDepth = 0.0f;
            engine.setParameters(parameters);
            engine.reset();

            if (raiseAfterReset)
            {
                parameters.vibratoDepth = 1.0f;
                engine.setParameters(parameters);
            }

            engine.noteOn(47, 0.85f);
            engine.setVibrato(1.0f);

            const int stringIndex = TestAccess::stringForNote(engine, 47);
            constexpr int chunk = 32;
            const int total = static_cast<int>(1.6 * sampleRate);
            StereoBuffer scratch(chunk);
            std::vector<float> target;
            for (int at = 0; at < total; at += chunk)
            {
                engine.process(scratch.left.data(), scratch.right.data(),
                               chunk);
                target.push_back(
                    TestAccess::snapshot(engine,
                                         stringIndex).verticalDelayTarget);
            }

            double highest = 0.0;
            double lowest = 1.0e9;
            const std::size_t settled = target.size() / 2;
            for (std::size_t i = settled; i < target.size(); ++i)
            {
                highest = std::max(highest,
                                   static_cast<double>(target[i]));
                lowest = std::min(lowest, static_cast<double>(target[i]));
            }
            return 1200.0 * std::log2(highest / lowest);
        };

        const double latched = excursionCents(false);
        const double raised = excursionCents(true);
        expect(raised > latched * 2.0,
               "Vibrato Depth raised after reset never reached the engine: the "
               "held note moved " + std::to_string(raised)
                   + " cents against " + std::to_string(latched)
                   + " cents with the control left alone, so the parameter is "
                     "dead until the next reset");
    }

    // Zero pressure is an exact no-op: the same score with the control never
    // touched renders bit-for-bit the same audio.
    {
        ElectryEngine untouched;
        untouched.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        untouched.setParameters(parameters);
        untouched.reset();
        untouched.noteOn(47, 0.85f);
        StereoBuffer withoutControl(static_cast<int>(0.6 * sampleRate));
        renderInto(untouched, withoutControl);

        ElectryEngine silenced;
        silenced.prepare(sampleRate, 512);
        silenced.setParameters(parameters);
        silenced.reset();
        silenced.noteOn(47, 0.85f);
        silenced.setVibrato(0.0f);
        StereoBuffer withSilentControl(static_cast<int>(0.6 * sampleRate));
        renderInto(silenced, withSilentControl);

        bool identical = true;
        for (std::size_t i = 0; i < withoutControl.left.size(); ++i)
            identical = identical
                && withoutControl.left[i] == withSilentControl.left[i];
        expect(identical, "a zero vibrato gesture is not a bit-exact no-op");
    }

    // A picking-hand repick does not replace the fretting finger. Its contact
    // advances the audible stroke, but the finger's phase and per-cycle draw
    // must continue exactly; releasing and fretting the note again is the
    // event that assigns a new finger.
    {
        ElectryEngine engine;
        ElectryEngine control;
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.strumSpreadSeconds = 0.0f;
        for (auto* target : { &engine, &control })
        {
            target->prepare(sampleRate, 512);
            target->setParameters(parameters);
            target->reset();
            target->noteOn(47, 0.85f);
            target->setVibrato(1.0f);
        }
        StereoBuffer establish(static_cast<int>(0.45 * sampleRate));
        StereoBuffer controlEstablish(static_cast<int>(0.45 * sampleRate));
        renderInto(engine, establish);
        renderInto(control, controlEstablish);
        const int stringIndex = TestAccess::stringForNote(engine, 47);
        const auto beforeRepick = TestAccess::snapshot(engine, stringIndex);
        const auto sameFinger = [] (const auto& left, const auto& right)
        {
            return left.vibratoSeed == right.vibratoSeed
                && left.vibratoCycle == right.vibratoCycle
                && left.vibratoPhase == right.vibratoPhase
                && left.vibratoRateScale == right.vibratoRateScale
                && left.vibratoDepthScale == right.vibratoDepthScale
                && left.vibratoSemitones == right.vibratoSemitones;
        };

        engine.noteOn(ElectryEngine::firstRepickNote + stringIndex, 0.92f);
        const auto afterRepick = TestAccess::snapshot(engine, stringIndex);
        expect(afterRepick.startOrder > beforeRepick.startOrder,
               "the held-string trigger did not make a new pick contact");
        expect(sameFinger(afterRepick, beforeRepick),
               "a picking-hand repick replaced the fretting finger");

        // The same rule survives scheduling lookahead. The delayed pick must
        // retain the finger decision made at reservation time: a held-string
        // trigger preserves it, while Note Off followed by the same note is a
        // real refret and replaces it even though the scheduler has marked the
        // key down again by the time contact arrives.
        parameters.strumSpreadSeconds = 0.020f;
        engine.setParameters(parameters);
        control.setParameters(parameters);
        // The immediate E6..B6 contact above happened between host samples.
        // Advance its clock before asking B0 for another physical contact;
        // equal-clock contacts are intentionally de-duplicated.
        StereoBuffer advanceContactClock(1);
        StereoBuffer controlAdvanceContactClock(1);
        renderInto(engine, advanceContactClock);
        renderInto(control, controlAdvanceContactClock);
        engine.beginTremoloPicking(0.92f);
        StereoBuffer scheduleB0(1);
        StereoBuffer controlScheduleB0(1);
        renderInto(engine, scheduleB0);
        renderInto(control, controlScheduleB0);
        expect(TestAccess::snapshot(engine, stringIndex).pendingRepickActive,
               "the B0 finger-continuity fixture had no pending repick");
        StereoBuffer delayedRepick(static_cast<int>(0.050 * sampleRate));
        StereoBuffer controlDelay(static_cast<int>(0.050 * sampleRate));
        renderInto(engine, delayedRepick);
        renderInto(control, controlDelay);
        engine.endTremoloPicking();
        const auto afterDelayedRepick = TestAccess::snapshot(engine,
                                                              stringIndex);
        const auto withoutDelayedRepick = TestAccess::snapshot(control,
                                                                stringIndex);
        expect(afterDelayedRepick.startOrder > afterRepick.startOrder
                   && sameFinger(afterDelayedRepick, withoutDelayedRepick),
               "a delayed B0 repick replaced the fretting finger");

        engine.noteOff(47);
        engine.noteOn(47, 0.85f);
        engine.noteOn(47, 0.80f);
        expect(TestAccess::snapshot(engine, stringIndex).pendingRepickActive,
               "the overlapping delayed-refret fixture had no pending contact");
        StereoBuffer delayedRefret(static_cast<int>(0.050 * sampleRate));
        renderInto(engine, delayedRefret);
        const auto refretted = TestAccess::snapshot(engine, stringIndex);
        expect(refretted.startOrder > afterDelayedRepick.startOrder
                   && refretted.vibratoSeed
                          != afterDelayedRepick.vibratoSeed,
               "an overlapping delayed refret reused the old finger");
    }

    // Measure the vibrato against the same note without it rather than against
    // a constant. The ratio removes every shared loop/filter offset exactly:
    // both renders are the same note at the same velocity.
    std::vector<double> ratio;
    ratio.reserve(moving.target.size());
    for (std::size_t i = 0; i < std::min(moving.target.size(),
                                         still.target.size()); ++i)
        ratio.push_back(still.target[i] > 0.0f
            ? static_cast<double>(moving.target[i]) / still.target[i] : 1.0);

    // The note is pushed sharp and never flat: a shorter delay is a higher
    // pitch, so the ratio never rises above one.
    const std::size_t settled = ratio.size() / 2;
    double highest = 0.0;
    double lowest = 1.0e9;
    double mean = 0.0;
    for (std::size_t i = settled; i < ratio.size(); ++i)
    {
        highest = std::max(highest, ratio[i]);
        lowest = std::min(lowest, ratio[i]);
        mean += ratio[i];
    }
    mean /= static_cast<double>(ratio.size() - settled);
    expect(highest <= 1.0002,
           "the fretting-hand vibrato pulled the string flat of the fret "
           "(highest ratio " + std::to_string(highest) + ")");
    expect(mean < 0.9995,
           "the vibrato is centred on the fretted pitch rather than sharp of "
           "it (mean ratio " + std::to_string(mean) + ")");

    // Its depth is the modelled one: a shorter delay by 2^(-cents/1200). The
    // upper bound admits the per-cycle excursion draw, which is bounded at
    // +45% of the nominal 40 cents, so the deepest cycle in any window can
    // reach 58; a tighter ceiling here would be asserting the draw away.
    const double depthCents = 1200.0 * std::log2(1.0 / lowest);
    expect(depthCents > 25.0 && depthCents < 60.0,
           "the vibrato depth is not the modelled 40 cents ("
               + std::to_string(depthCents) + " cents)");

    // It oscillates at a player's rate rather than drifting: count how often
    // the trace crosses its own midpoint over the settled window.
    const double midpoint = 0.5 * (highest + lowest);
    int crossings = 0;
    for (std::size_t i = settled + 1; i < ratio.size(); ++i)
        if ((ratio[i - 1] < midpoint) != (ratio[i] < midpoint))
            ++crossings;
    const double windowSeconds = static_cast<double>(ratio.size() - settled)
                               * 32.0 / sampleRate;
    const double rate = 0.5 * crossings / windowSeconds;
    expect(rate > 4.0 && rate < 8.0,
           "the vibrato rate is not a player's ("
               + std::to_string(rate) + " Hz)");

    // The onset is real: the first 60 ms carries far less movement than the
    // settled part, because a player lands the note before starting to move.
    double earlySpread = 0.0;
    const std::size_t earlyEnd = static_cast<std::size_t>(
        0.06 * sampleRate / 32.0);
    for (std::size_t i = 0; i < std::min(earlyEnd, ratio.size()); ++i)
        earlySpread = std::max(earlySpread, std::abs(ratio[i] - 1.0));
    expect(earlySpread < 0.4 * (1.0 - lowest),
           "the vibrato started instantly instead of easing in (early "
               + std::to_string(earlySpread) + ", settled "
               + std::to_string(1.0 - lowest) + ")");

    // A finger is not the bar: the sympathetically ringing strings must not
    // move, where the wheel moves them. Compared on the coupled ring left by a
    // picked note with the coupling wide open.
    const auto coupledRing = [&] (bool useWheel, bool useVibrato)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 1.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(47, 0.9f);
        StereoBuffer lead(static_cast<int>(0.25 * sampleRate));
        renderInto(engine, lead);
        if (useWheel)
            engine.setPitchBend(1.0f);
        if (useVibrato)
            engine.setVibrato(1.0f);
        // Read the coupled open low E's own loop: only the bar retunes it.
        StereoBuffer settle(static_cast<int>(0.6 * sampleRate));
        renderInto(engine, settle);
        return TestAccess::snapshot(engine, 0).verticalDelayTarget;
    };
    const float restingCoupled = coupledRing(false, false);
    const float vibratoCoupled = coupledRing(false, true);
    const float barCoupled = coupledRing(true, false);
    expect(std::abs(vibratoCoupled - restingCoupled) < 1.0e-3f,
           "the fretting-hand vibrato bent a string nobody is fingering");
    expect(std::abs(barCoupled - restingCoupled) > 1.0f,
           "the bar fixture did not bend the coupled string, so the "
           "comparison above proves nothing");
}

// A hand is not an LFO. Four things separate them and all four are read off
// the engine's own vibrato offset rather than off a pitch estimate, because
// the quantity under test is a modulation shape and an audio estimator would
// smear it across several cycles.
//
// The shape: the pitch follows the square of the finger's displacement, so a
// cycle spends 36.4% of itself above half its own peak where a raised cosine
// spends exactly 50%. The scatter: the rate and the excursion are redrawn
// every cycle, so neither repeats. The hand: two stopped strings are two
// fingers and drift apart instead of moving in lockstep. And the onset: the
// depth leaves rest with zero slope rather than at its steepest.
void testVibratoIsAHandNotAnLfo()
{
    constexpr double sampleRate = 48000.0;
    // The engine runs at twice the host rate below 96 kHz and its control
    // block is 16 internal samples long, so eight host samples is exactly one
    // control tick: every trace below is sampled on the grid the vibrato is
    // computed on rather than interpolated off it.
    constexpr int chunk = 8;
    const double tick = static_cast<double>(chunk) / sampleRate;

    const auto minimaOf = [] (const std::vector<double>& trace)
    {
        std::vector<int> minima;
        for (std::size_t i = 1; i + 1 < trace.size(); ++i)
            if (trace[i] <= trace[i - 1] && trace[i] < trace[i + 1])
                minima.push_back(static_cast<int>(i));
        return minima;
    };

    // One fingered string's vibrato offset in semitones, one reading per
    // control tick, with the shared depth envelope alongside it.
    const auto traceOf = [&] (float vibratoDepth, double seconds,
                              std::vector<double>* envelope)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.vibratoDepth = vibratoDepth;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(47, 0.85f);
        StereoBuffer scratch(chunk);
        for (int at = 0; at < static_cast<int>(0.2 * sampleRate); at += chunk)
            engine.process(scratch.left.data(), scratch.right.data(), chunk);
        const int stringIndex = TestAccess::stringForNote(engine, 47);
        expect(stringIndex >= 0 && TestAccess::snapshot(engine, stringIndex).fret > 0,
               "the vibrato fixture is not on a stopped string");
        engine.setVibrato(1.0f);

        std::vector<double> trace;
        const int steps = static_cast<int>(seconds * sampleRate) / chunk;
        trace.reserve(static_cast<std::size_t>(steps));
        for (int step = 0; step < steps; ++step)
        {
            engine.process(scratch.left.data(), scratch.right.data(), chunk);
            trace.push_back(stringIndex >= 0
                ? static_cast<double>(
                      TestAccess::snapshot(engine, stringIndex).vibratoSemitones)
                : 0.0);
            if (envelope != nullptr)
                envelope->push_back(TestAccess::vibratoDepthEnvelope(engine));
        }
        return trace;
    };

    // ---- one cycle's shape, and the scatter between cycles ---------------
    const EngineParameters defaults;
    const auto trace = traceOf(defaults.vibratoDepth, 8.0, nullptr);
    const auto minima = minimaOf(trace);
    expect(minima.size() >= 25,
           "the vibrato fixture did not produce enough cycles to score ("
               + std::to_string(minima.size()) + ")");

    // The first six cycles are dropped: the depth envelope is still ramping
    // through them, so their peaks measure the onset rather than the draw.
    std::vector<double> periods, peakCents, aboveHalf;
    for (std::size_t k = 6; k + 1 < minima.size(); ++k)
    {
        const int from = minima[k];
        const int to = minima[k + 1];
        double peak = 0.0;
        for (int i = from; i <= to; ++i)
            peak = std::max(peak, trace[static_cast<std::size_t>(i)]);
        int above = 0;
        for (int i = from; i < to; ++i)
            if (trace[static_cast<std::size_t>(i)] > 0.5 * peak)
                ++above;
        periods.push_back((to - from) * tick);
        peakCents.push_back(100.0 * peak);
        aboveHalf.push_back(static_cast<double>(above) / (to - from));
    }
    expect(periods.size() >= 18,
           "fewer than the eighteen settled cycles this test scores ("
               + std::to_string(periods.size()) + ")");

    const auto meanOf = [] (const std::vector<double>& values)
    {
        double sum = 0.0;
        for (double value : values)
            sum += value;
        return sum / static_cast<double>(values.size());
    };

    const double periodMean = meanOf(periods);
    double periodVariance = 0.0;
    for (double period : periods)
        periodVariance += (period - periodMean) * (period - periodMean);
    const double periodDeviation =
        std::sqrt(periodVariance / static_cast<double>(periods.size()));
    expect(periodDeviation > 0.04 * periodMean,
           "the vibrato rate repeats itself like an oscillator (period "
           "deviation " + std::to_string(100.0 * periodDeviation / periodMean)
               + "% of the mean)");

    const double deepest = *std::max_element(peakCents.begin(), peakCents.end());
    const double shallowest = *std::min_element(peakCents.begin(), peakCents.end());
    expect(deepest - shallowest > 2.5,
           "the vibrato reaches the same depth every cycle (spread "
               + std::to_string(deepest - shallowest) + " cents)");

    // The x^2 law is pinned from both sides. A raised cosine gives exactly
    // 50%, its square 36.4%, and each cycle is scored against its own peak
    // because the depth is redrawn every cycle.
    const double halfFraction = meanOf(aboveHalf);
    expect(halfFraction > 0.32 && halfFraction < 0.40,
           "the vibrato is not following the square of the finger's "
           "displacement (cycle spends " + std::to_string(halfFraction)
               + " of itself above half its own peak)");

    // ---- the Vibrato Depth control's range -------------------------------
    const auto peakOf = [&] (float vibratoDepth)
    {
        const auto depthTrace = traceOf(vibratoDepth, 6.0, nullptr);
        double peak = 0.0;
        for (std::size_t i = depthTrace.size() / 3; i < depthTrace.size(); ++i)
            peak = std::max(peak, depthTrace[i]);
        return 100.0 * peak;
    };
    const double widest = peakOf(1.0f);
    const double narrowest = peakOf(0.0f);
    expect(widest >= 90.0,
           "a fully open Vibrato Depth does not reach a rock vibrato's arc ("
               + std::to_string(widest) + " cents)");
    expect(narrowest <= 15.0,
           "a closed Vibrato Depth is still a wide vibrato ("
               + std::to_string(narrowest) + " cents)");

    // ---- two fingered strings are two fingers ----------------------------
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(47, 0.85f);
        engine.noteOn(52, 0.85f);
        StereoBuffer scratch(chunk);
        for (int at = 0; at < static_cast<int>(0.2 * sampleRate); at += chunk)
            engine.process(scratch.left.data(), scratch.right.data(), chunk);
        const int lower = TestAccess::stringForNote(engine, 47);
        const int upper = TestAccess::stringForNote(engine, 52);
        expect(lower >= 0 && upper >= 0 && lower != upper,
               "the double stop did not land on two separate strings");
        engine.setVibrato(1.0f);

        std::vector<double> lowerTrace, upperTrace;
        const int steps = static_cast<int>(6.0 * sampleRate) / chunk;
        for (int step = 0; step < steps; ++step)
        {
            engine.process(scratch.left.data(), scratch.right.data(), chunk);
            lowerTrace.push_back(static_cast<double>(
                TestAccess::snapshot(engine, lower).vibratoSemitones));
            upperTrace.push_back(static_cast<double>(
                TestAccess::snapshot(engine, upper).vibratoSemitones));
        }
        const auto lowerMinima = minimaOf(lowerTrace);
        const auto upperMinima = minimaOf(upperTrace);
        expect(lowerMinima.size() >= 12 && upperMinima.size() >= 12,
               "the double-stop fixture did not oscillate on both strings");

        // For every settled cycle of the lower string, how far its rest point
        // sits from the upper string's nearest rest point, as a fraction of a
        // cycle. Scored on the mean rather than on every cycle: two
        // independent fingers do occasionally pass through the same phase, and
        // a floor under every cycle would assert that they never may.
        std::vector<double> separation;
        for (std::size_t k = 6; k + 1 < lowerMinima.size(); ++k)
        {
            const double period = lowerMinima[k + 1] - lowerMinima[k];
            double nearest = 1.0e9;
            for (int candidate : upperMinima)
                nearest = std::min(nearest,
                                   std::abs(static_cast<double>(candidate
                                                                - lowerMinima[k])));
            separation.push_back(nearest / period);
        }
        const double meanSeparation = meanOf(separation);
        expect(meanSeparation >= 0.08,
               "a double stop's two strings move as one finger (mean phase "
               "separation " + std::to_string(meanSeparation) + " cycles)");
    }

    // ---- the onset leaves rest rather than jumping -----------------------
    {
        std::vector<double> envelope;
        (void) traceOf(defaults.vibratoDepth, 1.5, &envelope);
        expect(! envelope.empty(), "the onset fixture produced no envelope");
        const double settled = envelope.back();
        expect(settled > 0.9, "the vibrato never reached full depth");
        std::size_t reached = envelope.size() - 1;
        for (std::size_t i = 0; i < envelope.size(); ++i)
            if (envelope[i] >= 0.9 * settled) { reached = i; break; }
        // A scale-free measure, so it does not have to name an onset time: at
        // a tenth of the way to 90% of settled depth, a one-pole is already
        // 20.6% of the way there whatever its time constant, because it is
        // steepest at t = 0. A smoothStep is still at rest.
        const std::size_t early = static_cast<std::size_t>(
            0.1 * static_cast<double>(reached + 1));
        const double earlyFraction = envelope[early] / settled;
        expect(earlyFraction <= 0.05,
               "the vibrato's depth leaves rest at its steepest instead of "
               "easing off it (" + std::to_string(100.0 * earlyFraction)
                   + "% of settled at a tenth of the time to 90%)");
    }

    // ---- and none of it moves a note nobody is pressing -------------------
    {
        const auto render = [&] (bool touchControl)
        {
            ElectryEngine engine;
            engine.prepare(sampleRate, 512);
            EngineParameters parameters;
            parameters.artifactAmount = 0.0f;
            parameters.sympatheticAmount = 0.0f;
            engine.setParameters(parameters);
            engine.reset();
            engine.noteOn(47, 0.85f);
            if (touchControl)
                engine.setVibrato(0.0f);
            StereoBuffer buffer(static_cast<int>(0.6 * sampleRate));
            renderInto(engine, buffer);
            return buffer.left;
        };
        expect(render(false) == render(true),
               "a zero vibrato gesture is not a bit-exact no-op");
    }
}

void testVibratoRequiresHeldFinger()
{
    static constexpr std::array<double, 5> sampleRates {
        44100.0, 48000.0, 96000.0, 192000.0, 384000.0
    };

    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    // A pre-held A#0 is performance intent, not a phantom finger. Its shared
    // onset must wait at rest through silence, then begin when a stopped key
    // actually gives the hand a string to rock.
    for (const double sampleRate : sampleRates)
    {
        ElectryEngine preHeld;
        preHeld.prepare(sampleRate, 512);
        preHeld.setParameters(parameters);
        preHeld.reset();
        preHeld.setVibrato(1.0f);
        const int controlFrames =
            TestAccess::hostFramesPerControlPeriod(preHeld);
        const int silentFrames = controlFrames * static_cast<int>(
            std::ceil(0.35 * sampleRate
                      / static_cast<double>(controlFrames)));
        StereoBuffer silence(silentFrames);
        renderInto(preHeld, silence);
        expect(TestAccess::vibratoDepthEnvelope(preHeld) == 0.0f,
               "a pre-held vibrato aged through silence at "
                   + std::to_string(sampleRate) + " Hz");

        preHeld.noteOn(45, 0.85f); // the open A has no fretting finger
        StereoBuffer openOnly(silentFrames);
        renderInto(preHeld, openOnly);
        expect(TestAccess::vibratoDepthEnvelope(preHeld) == 0.0f,
               "an open string aged a pre-held vibrato at "
                   + std::to_string(sampleRate) + " Hz");
        preHeld.noteOff(45);
        preHeld.noteOn(47, 0.85f);
        StereoBuffer preHeldTick(controlFrames);
        renderInto(preHeld, preHeldTick, controlFrames);

        ElectryEngine simultaneous;
        simultaneous.prepare(sampleRate, 512);
        simultaneous.setParameters(parameters);
        simultaneous.reset();
        simultaneous.noteOn(47, 0.85f);
        simultaneous.setVibrato(1.0f);
        StereoBuffer simultaneousTick(controlFrames);
        renderInto(simultaneous, simultaneousTick, controlFrames);

        const float preHeldAmount =
            TestAccess::vibratoDepthEnvelope(preHeld);
        const float simultaneousAmount =
            TestAccess::vibratoDepthEnvelope(simultaneous);
        expect(preHeldAmount > 0.0f
                   && preHeldAmount == simultaneousAmount,
               "a pre-held vibrato did not start with its new finger at "
                   + std::to_string(sampleRate) + " Hz");
    }

    // Sustain keeps a voice observable after key-up, but it is not a fretting
    // finger. Releasing one key stops only that string while a held sibling
    // keeps the shared hand moving; releasing the last stopped key returns the
    // hidden onset to rest even though A#0 itself remains held.
    {
        constexpr double sampleRate = 48000.0;
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(47, 0.85f);
        engine.noteOn(52, 0.85f);
        const int lower = TestAccess::stringForNote(engine, 47);
        const int upper = TestAccess::stringForNote(engine, 52);
        expect(lower >= 0 && upper >= 0 && lower != upper
                   && TestAccess::snapshot(engine, lower).fret > 0
                   && TestAccess::snapshot(engine, upper).fret > 0,
               "the vibrato release fixture did not allocate two stopped "
               "strings");
        engine.setVibrato(1.0f);
        const int controlFrames =
            TestAccess::hostFramesPerControlPeriod(engine);
        const int settleFrames = controlFrames * static_cast<int>(
            std::ceil(0.35 * sampleRate
                      / static_cast<double>(controlFrames)));
        StereoBuffer settle(settleFrames);
        renderInto(engine, settle);
        expect(TestAccess::vibratoDepthEnvelope(engine) > 0.99f,
               "the held-sibling fixture never reached full vibrato");

        engine.setSustainPedal(true);
        engine.noteOff(47);
        StereoBuffer oneReleaseTick(controlFrames);
        renderInto(engine, oneReleaseTick, controlFrames);
        const auto released = TestAccess::snapshot(engine, lower);
        const auto held = TestAccess::snapshot(engine, upper);
        expect(released.active && ! released.keyDown
                   && released.vibratoSemitones == 0.0f,
               "a sustain-held released string kept a vibrato finger");
        expect(held.active && held.keyDown
                   && TestAccess::vibratoDepthEnvelope(engine) > 0.99f,
               "releasing one string stopped its held vibrato sibling");

        engine.noteOff(52);
        StereoBuffer finalReleaseTick(controlFrames);
        renderInto(engine, finalReleaseTick, controlFrames);
        expect(TestAccess::vibratoDepthEnvelope(engine) == 0.0f
                   && TestAccess::snapshot(engine, lower).vibratoSemitones
                          == 0.0f
                   && TestAccess::snapshot(engine, upper).vibratoSemitones
                          == 0.0f,
               "releasing the last stopped key left hidden vibrato motion");
        expect(TestAccess::vibratoTarget(engine) == 1.0f,
               "releasing a fretted key cleared the held A#0 owner");
    }

    // MIDI can release and refret a string between two control ticks. The
    // ownership edge itself must retire the old finger; waiting for the next
    // tick would let the newly seeded finger inherit a full shared envelope
    // and the old finger's last pitch offset.
    {
        constexpr double sampleRate = 48000.0;
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(47, 0.85f);
        engine.setVibrato(1.0f);
        const int controlFrames =
            TestAccess::hostFramesPerControlPeriod(engine);
        const int settleFrames = controlFrames * static_cast<int>(
            std::ceil(0.35 * sampleRate
                      / static_cast<double>(controlFrames)));
        StereoBuffer settle(settleFrames);
        renderInto(engine, settle);
        StereoBuffer offGrid(1);
        renderInto(engine, offGrid, 1);

        const int stringIndex = TestAccess::stringForNote(engine, 47);
        engine.noteOff(47);
        expect(TestAccess::vibratoDepthEnvelope(engine) == 0.0f
                   && TestAccess::snapshot(engine, stringIndex)
                          .vibratoSemitones == 0.0f,
               "an off-grid final Note Off retained the old vibrato finger");
        engine.noteOn(47, 0.85f);
        expect(TestAccess::vibratoDepthEnvelope(engine) == 0.0f
                   && TestAccess::snapshot(engine, stringIndex)
                          .vibratoSemitones == 0.0f,
               "a same-boundary refret inherited the old vibrato onset");
        StereoBuffer firstTick(controlFrames);
        renderInto(engine, firstTick, controlFrames);
        expect(TestAccess::vibratoDepthEnvelope(engine) > 0.0f
                   && TestAccess::vibratoDepthEnvelope(engine) < 0.001f,
               "a same-boundary refret skipped its fresh onset");
    }

    // Pulling off to an open string removes the last fretting finger too. A
    // same-boundary hammer back must not make that lifecycle depend on whether
    // a control tick happened to land between the two MIDI events.
    {
        constexpr double sampleRate = 48000.0;
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(47, 0.85f);
        engine.setVibrato(1.0f);
        StereoBuffer settle(static_cast<int>(0.35 * sampleRate));
        renderInto(engine, settle);
        StereoBuffer offGrid(1);
        renderInto(engine, offGrid, 1);

        engine.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
        engine.noteOn(45, 0.78f);
        const int stringIndex = TestAccess::stringForNote(engine, 45);
        expect(stringIndex >= 0
                   && TestAccess::snapshot(engine, stringIndex).fret == 0
                   && TestAccess::snapshot(engine, stringIndex)
                          .vibratoSemitones == 0.0f
                   && TestAccess::vibratoDepthEnvelope(engine) == 0.0f,
               "an off-grid pull-off left a phantom vibrato finger");

        engine.noteOn(47, 0.78f);
        expect(TestAccess::snapshot(engine, stringIndex).fret > 0
                   && TestAccess::snapshot(engine, stringIndex)
                          .vibratoSemitones == 0.0f
                   && TestAccess::vibratoDepthEnvelope(engine) == 0.0f,
               "a same-boundary hammer inherited an open string's vibrato");
    }

    // A second owner of the same delayed refret is not another finger. Keep
    // the onset that has already begun while preserving the pending contact's
    // earlier decision to reseed against the released finger it replaced.
    {
        constexpr double sampleRate = 48000.0;
        ElectryEngine engine;
        EngineParameters delayedParameters = parameters;
        delayedParameters.strumSpreadSeconds = 0.020f;
        engine.prepare(sampleRate, 512);
        engine.setParameters(delayedParameters);
        engine.reset();
        engine.noteOn(47, 0.85f);
        engine.setVibrato(1.0f);
        StereoBuffer settle(static_cast<int>(0.35 * sampleRate));
        renderInto(engine, settle);
        engine.noteOff(47);
        engine.noteOn(47, 0.85f);
        StereoBuffer beforeOverlap(800);
        renderInto(engine, beforeOverlap, 800);
        const int stringIndex = TestAccess::stringForNote(engine, 47);
        const float begun = TestAccess::vibratoDepthEnvelope(engine);
        expect(TestAccess::snapshot(engine, stringIndex).pendingRepickActive
                   && begun > 0.0f,
               "the delayed-overlap fixture had no fresh onset in flight");

        engine.noteOn(47, 0.80f);
        expect(TestAccess::snapshot(engine, stringIndex).pendingRepickActive
                   && TestAccess::vibratoDepthEnvelope(engine) == begun,
               "an overlapping owner restarted a delayed finger's onset");
        engine.noteOff(47);
        expect(TestAccess::snapshot(engine, stringIndex).keyDown
                   && TestAccess::vibratoDepthEnvelope(engine) == begun,
               "one overlapping Note Off released the shared finger");
    }
}

// A slide is a finger that stays down and travels: the sounding length moves
// continuously, the travel time is a distance divided by a hand speed rather
// than a fixed number, and the winding drags under the finger the whole way.
void testSlideArticulation()
{
    constexpr double sampleRate = 48000.0;

    // A chained fretting gesture starts wherever the travelling finger is,
    // not at the destination the preceding Slide has not reached. Put the
    // finger exactly halfway through fret 1 -> 21, at fret 11: a Hammer to
    // that same live fret must keep string 0. Measuring from the stale written
    // destination instead makes it an unreachable ten-fret jump and moves the
    // note to another string. Pin both the scalar and complete-chord allocators,
    // which solve the same physical continuation through separate paths.
    const auto placeFingerAtSlideMidpoint = [] (ElectryEngine& engine)
    {
        engine.prepare(sampleRate, 64);
        engine.reset();
        engine.noteOn(29, 0.85f); // string 0, fret 1
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(49, 0.82f); // same string, written fret 21
        TestAccess::updateSlideControlAt(engine, 0, 0.5f);
        expect(TestAccess::stringForNote(engine, 49) == 0
                   && std::abs(TestAccess::programmedLegatoFrequency(engine, 0)
                               - midiHz(39)) < 1.0e-3,
               "invalid mid-slide Hammer-allocation fixture");
    };
    {
        ElectryEngine engine;
        placeFingerAtSlideMidpoint(engine);
        expect(TestAccess::chosenString(engine, 39, PlayStyle::Hammer) == 0,
               "scalar Hammer reach used the unfinished Slide destination");
        engine.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
        engine.noteOn(39, 0.80f);
        expect(TestAccess::stringForNote(engine, 39) == 0,
               "scalar Hammer left the string under the travelling finger");
    }
    {
        ElectryEngine engine;
        placeFingerAtSlideMidpoint(engine);
        engine.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
        const std::array<ElectryEngine::NoteOnEvent, 2> chord {{
            { 39, 0.80f }, { 45, 0.76f }
        }};
        engine.noteOnChord(chord);
        expect(TestAccess::stringForNote(engine, 39) == 0,
               "batched Hammer left the string under the travelling finger");
    }

    // The canonical play-style score leaves each release enough room to
    // retire, then demonstrates a short ascent and octave travel in both
    // directions on the wound E2 string. Every destination must be the same
    // unpicked moving finger, not a nearby tail or a repeated-note restrike.
    {
        constexpr double scoreSampleRate = 44100.0;
        ElectryEngine engine;
        engine.prepare(scoreSampleRate, 256);
        engine.reset();
        const auto trigger = [&] (int note, float velocity)
        {
            const std::array<ElectryEngine::NoteOnEvent, 1> event {{
                { note, velocity }
            }};
            engine.noteOnChord(event);
        };
        StereoBuffer establish(static_cast<int>(0.22 * scoreSampleRate));
        StereoBuffer hold(static_cast<int>(0.60 * scoreSampleRate));
        StereoBuffer gap(static_cast<int>(0.60 * scoreSampleRate));
        static constexpr std::array<std::array<int, 2>, 3> slides {{
            {{ 40, 42 }}, {{ 40, 52 }}, {{ 53, 41 }}
        }};
        for (const auto& notes : slides)
        {
            const int source = notes[0];
            const int target = notes[1];
            engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
            trigger(source, 0.90f);
            renderInto(engine, establish);
            const int sourceString = TestAccess::stringForNote(engine, source);
            engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
            trigger(target, 0.80f);
            const int targetString = TestAccess::stringForNote(engine, target);
            const auto moved = TestAccess::snapshot(engine, targetString);
            expect(sourceString == 2 && targetString == sourceString
                       && moved.active && moved.keyDown
                       && moved.playStyle == PlayStyle::Slide
                       && TestAccess::legatoBlend(engine, targetString) == 0.0f
                       && std::abs(TestAccess::legatoFromFrequency(
                                      engine, targetString)
                                   - midiHz(source)) < 1.0e-3
                       && TestAccess::slideNoiseAmplitude(engine, targetString)
                              > 0.0f
                       && moved.excitationAmplitude == 0.0f,
                   "canonical Slide score did not move its held wound string");
            renderInto(engine, hold);
            engine.noteOff(target);
            engine.noteOff(source);
            expect(! TestAccess::snapshot(engine, targetString).keyDown,
                   "canonical Slide score left its destination held");
            renderInto(engine, gap);
        }
    }

    // Broadband magnitude-weighted centroid over a log grid. The harmonic-
    // series centroid used elsewhere cannot see the scrape, which is noise
    // rather than a partial.
    const auto broadbandCentroid = [&] (const std::vector<float>& data,
                                        int start, int length)
    {
        double weighted = 0.0;
        double total = 0.0;
        for (double frequency = 300.0; frequency < 16000.0; frequency *= 1.09)
        {
            const double magnitude = dftMagnitude(data, start, length,
                                                  sampleRate, frequency);
            weighted += magnitude * frequency;
            total += magnitude;
        }
        return total > 0.0 ? weighted / total : 0.0;
    };

    struct Slide
    {
        StereoBuffer audio { 1 };
        int settleSamples { 0 };
        int stringIndex { -1 };
        int fret { -1 };
        int controlFrames { 1 };
        float frictionAmplitude { 0.0f };
        float excitationAmplitude { 0.0f };
        std::array<int, 3> sweepSamples {{ -1, -1, -1 }};
    };

    const auto renderSlide = [&] (int fromNote, int toNote, float fingerNoise,
                                  float bendTime)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.bodyResonance = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.fingerNoise = fingerNoise;
        parameters.bendTimeSeconds = bendTime;
        engine.setParameters(parameters);
        engine.reset();

        engine.noteOn(fromNote, 0.85f);
        StereoBuffer lead(static_cast<int>(0.30 * sampleRate));
        renderInto(engine, lead);

        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(toNote, 0.85f);

        Slide result;
        result.stringIndex = TestAccess::stringForNote(engine, toNote);
        result.controlFrames = TestAccess::hostFramesPerControlPeriod(engine);
        result.fret = result.stringIndex >= 0
            ? TestAccess::snapshot(engine, result.stringIndex).fret : -1;
        result.frictionAmplitude = result.stringIndex >= 0
            ? TestAccess::slideNoiseAmplitude(engine, result.stringIndex) : 0.0f;
        result.excitationAmplitude = result.stringIndex >= 0
            ? TestAccess::snapshot(engine, result.stringIndex)
                  .excitationAmplitude
            : 0.0f;

        // Render in short chunks so the moment the glide settles can be read
        // from the delay target the engine is actually driving: the glide is
        // monotone, so it has settled once the target stops moving.
        constexpr int chunk = 64;
        const int total = static_cast<int>(2.0 * sampleRate);
        result.audio = StereoBuffer(total);
        result.settleSamples = total;
        bool settled = false;
        for (int at = 0; at < total; at += chunk)
        {
            const int samples = std::min(chunk, total - at);
            engine.process(result.audio.left.data() + at,
                           result.audio.right.data() + at, samples);
            if (settled || result.stringIndex < 0)
                continue;
            constexpr std::array<float, 3> sweepBlends {{ 0.2f, 0.5f, 0.8f }};
            for (std::size_t point = 0; point < sweepBlends.size(); ++point)
            {
                if (result.sweepSamples[point] < 0
                    && TestAccess::legatoBlend(engine, result.stringIndex)
                           >= sweepBlends[point])
                    result.sweepSamples[point] = at + samples;
            }
            if (TestAccess::legatoBlend(engine, result.stringIndex) >= 1.0f)
            {
                result.settleSamples = at + samples;
                settled = true;
            }
        }
        return result;
    };

    // A2 open on the wound A string, sliding up to the twelfth fret.
    const auto longSlide = renderSlide(45, 57, 0.8f, 0.28f);
    expect(longSlide.stringIndex == 3 && longSlide.fret == 12,
           "the slide did not stay on the string it started from");

    // The pitch travels: a window in the middle of the slide sits strictly
    // between the two endpoints rather than at either of them.
    const double fromHz = midiHz(45);
    const double toHz = midiHz(57);
    const int slideStart = static_cast<int>(0.30 * sampleRate);
    const int midpoint = slideStart + longSlide.settleSamples / 2;
    const double middle = measureFrequency(
        longSlide.audio.left, midpoint, static_cast<int>(0.04 * sampleRate),
        sampleRate, std::sqrt(fromHz * toHz));
    expect(middle > fromHz * 1.15 && middle < toHz * 0.87,
           "the slide jumped instead of travelling (mid-slide pitch "
               + std::to_string(middle) + " Hz between " + std::to_string(fromHz)
               + " and " + std::to_string(toHz) + ")");

    const double settled = measureFrequency(
        longSlide.audio.left, slideStart + longSlide.settleSamples
            + static_cast<int>(0.2 * sampleRate),
        static_cast<int>(0.5 * sampleRate), sampleRate, toHz);
    expect(std::abs(centsBetween(settled, toHz)) < 12.0,
           "the slide did not arrive on its target pitch");

    // Note Off lifts the travelling finger. The sounding coordinate and its
    // winding scrape must stop at that boundary even when CC64 keeps the
    // string ringing; a held key is the control that still reaches the target.
    for (const bool sustainTail : { false, true })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.bodyResonance = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.fingerNoise = 0.8f;
        parameters.bendTimeSeconds = 0.28f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.85f);
        StereoBuffer establish(static_cast<int>(0.30 * sampleRate));
        renderInto(engine, establish);
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(57, 0.84f);

        constexpr int stringIndex = 3;
        const int controlFrames =
            TestAccess::hostFramesPerControlPeriod(engine);
        StereoBuffer tick(controlFrames);
        int travelFrames = 0;
        while (TestAccess::legatoBlend(engine, stringIndex) < 0.25f
               && travelFrames < static_cast<int>(0.15 * sampleRate))
        {
            renderInto(engine, tick, controlFrames);
            travelFrames += controlFrames;
        }
        const float releasedFrequency =
            TestAccess::programmedLegatoFrequency(engine, stringIndex);
        const float releasedB = TestAccess::snapshot(engine, stringIndex)
                                    .inharmonicity;
        const float releasedBlend = TestAccess::legatoBlend(engine, stringIndex);
        expect(releasedBlend >= 0.25f && releasedBlend < 0.40f
                   && TestAccess::slideNoiseAmplitude(engine, stringIndex) > 0.0f
                   && TestAccess::slideNoiseLevel(engine, stringIndex) > 0.0f,
               "the mid-slide release fixture missed the travelling finger");

        if (sustainTail)
            engine.setSustainPedal(true);
        engine.noteOff(57);
        expect(TestAccess::legatoBlend(engine, stringIndex) == releasedBlend,
               "Note Off moved the slide at its ownership boundary");
        expect(TestAccess::slideNoiseAmplitude(engine, stringIndex) == 0.0f
                   && TestAccess::slideNoiseLevel(engine, stringIndex) == 0.0f,
               "a released slide kept scraping without a fretting finger");

        const auto expectLiveReleaseRate = [&]
        {
            const float expected = std::pow(
                10.0f, -3.0f / (0.060f * releasedFrequency));
            expect(std::abs(TestAccess::releaseGainTarget(engine, stringIndex)
                            - expected) < 2.0e-7f,
                   "a mid-slide release used the abandoned destination pitch");
        };
        if (! sustainTail)
            expectLiveReleaseRate();

        const int observeFrames = static_cast<int>(
            (sustainTail ? 0.35 : 0.10) * sampleRate);
        int observedActiveTicks = 0;
        for (int rendered = 0; rendered < observeFrames;
             rendered += controlFrames)
        {
            renderInto(engine, tick, controlFrames);
            const auto voice = TestAccess::snapshot(engine, stringIndex);
            if (! voice.active)
                continue;
            ++observedActiveTicks;
            expect(TestAccess::legatoBlend(engine, stringIndex) == releasedBlend
                       && std::abs(centsBetween(
                              TestAccess::programmedLegatoFrequency(
                                  engine, stringIndex),
                              releasedFrequency)) < 0.001
                       && std::abs(voice.inharmonicity / releasedB - 1.0f)
                              < 0.001f,
                   "a released slide kept travelling toward its destination");
        }
        expect(observedActiveTicks > 0,
               "the mid-slide release fixture retired before observation");
        if (sustainTail)
        {
            const auto heldTail = TestAccess::snapshot(engine, stringIndex);
            expect(heldTail.active && ! heldTail.keyDown
                       && ! heldTail.releasing,
                   "CC64 did not retain the released slide tail");
            engine.setSustainPedal(false);
            expectLiveReleaseRate();
        }
    }

    // A travelling fret shortens the same tensioned string, so stiffness B
    // follows 1/L^2 continuously; selecting the destination fret must not
    // install its stiffness before the finger has moved there.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.bodyResonance = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.bendTimeSeconds = 0.28f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.85f);
        StereoBuffer establish(static_cast<int>(0.30 * sampleRate));
        renderInto(engine, establish);

        constexpr int stringIndex = 3;
        const float sourceB = TestAccess::snapshot(engine, stringIndex)
                                  .inharmonicity;
        const float sourcePitch = TestAccess::programmedLegatoFrequency(
            engine, stringIndex);
        const float sourceEffective = TestAccess::effectiveLoopFrequency(
            engine, stringIndex);
        ElectryEngine targetReference;
        targetReference.prepare(sampleRate, 512);
        targetReference.setParameters(parameters);
        targetReference.reset();
        targetReference.noteOn(47, 0.82f);
        StereoBuffer targetEstablish(static_cast<int>(0.30 * sampleRate));
        renderInto(targetReference, targetEstablish);
        const float targetB = TestAccess::snapshot(targetReference, stringIndex)
                                  .inharmonicity;
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(57, 0.84f);
        expect(TestAccess::snapshot(engine, stringIndex).inharmonicity
                   == sourceB,
               "a slide installed the destination stiffness at contact");
        expect(std::abs(centsBetween(
                   TestAccess::effectiveLoopFrequency(engine, stringIndex),
                   sourceEffective)) < 2.0,
               "the stiffness-continuity slide changed effective pitch at "
               "contact");

        ElectryEngine cancelledPitch;
        cancelledPitch.prepare(sampleRate, 512);
        cancelledPitch.setParameters(parameters);
        cancelledPitch.reset();
        cancelledPitch.noteOn(45, 0.85f);
        StereoBuffer cancelledEstablish(static_cast<int>(0.30 * sampleRate));
        renderInto(cancelledPitch, cancelledEstablish);
        cancelledPitch.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        cancelledPitch.noteOn(47, 0.84f);
        // Cache the contact state with +2 semitones cancelling the slide's
        // -2-semitone source offset, then move the finger one fret while +1
        // cancels its -1-semitone midpoint offset. Combined pitch is identical
        // at both fits. Holding it there requires tension proportional to L^2,
        // so B follows 1/L^4 rather than the pure slide's fixed-tension 1/L^2.
        TestAccess::setLegatoWithOpposingBend(
            cancelledPitch, stringIndex, 0.0f);
        const float cancelledBasePitch = static_cast<float>(midiHz(47));
        const float cancelledContactB = TestAccess::snapshot(
            cancelledPitch, stringIndex).inharmonicity;
        const float cancelledContactExpectedB = sourceB
            * sourcePitch * sourcePitch
            / (cancelledBasePitch * cancelledBasePitch);
        expect(std::abs(cancelledContactB / cancelledContactExpectedB - 1.0f)
                   < 0.01f,
               "the opposing bend left contact stiffness at unbent tension");
        TestAccess::setLegatoWithOpposingBend(
            cancelledPitch, stringIndex, 0.5f);
        const float cancelledB = TestAccess::snapshot(
            cancelledPitch, stringIndex).inharmonicity;
        const float cancelledLivePitch = TestAccess::programmedLegatoFrequency(
            cancelledPitch, stringIndex);
        const float cancelledExpectedB = sourceB
            * std::pow(cancelledLivePitch / sourcePitch, 4.0f)
            * sourcePitch * sourcePitch
            / (cancelledBasePitch * cancelledBasePitch);
        expect(std::abs(cancelledB / cancelledExpectedB - 1.0f) < 0.01f
                   && cancelledB > cancelledContactB * 1.20f,
               "an opposing bend left stiffness outside its live T/L law");

        const int traceFrames = TestAccess::hostFramesPerControlPeriod(engine);
        StereoBuffer trace(traceFrames);
        float previousB = sourceB;
        while (TestAccess::legatoBlend(engine, stringIndex) < 0.45f)
        {
            renderInto(engine, trace, traceFrames);
            const float currentB = TestAccess::snapshot(engine, stringIndex)
                                       .inharmonicity;
            const float livePitch = TestAccess::programmedLegatoFrequency(
                engine, stringIndex);
            const float expectedB = sourceB * livePitch * livePitch
                                  / (sourcePitch * sourcePitch);
            expect(currentB + 1.0e-8f >= previousB,
                   "ascending slide stiffness moved away from its live fret");
            expect(std::abs(currentB / expectedB - 1.0f) < 0.01f,
                   "ascending slide stiffness did not follow 1/L^2");
            previousB = currentB;
        }

        const float beforeRetargetB = previousB;
        const float beforeRetargetEffective =
            TestAccess::effectiveLoopFrequency(engine, stringIndex);
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(47, 0.82f);
        const float afterRetargetB = TestAccess::snapshot(engine, stringIndex)
                                         .inharmonicity;
        expect(std::abs(afterRetargetB / beforeRetargetB - 1.0f) < 0.01f,
               "a chained descending slide jumped to destination stiffness");
        expect(std::abs(centsBetween(
                   TestAccess::effectiveLoopFrequency(engine, stringIndex),
                   beforeRetargetEffective)) < 2.0,
               "a chained stiffness retarget changed effective pitch");

        previousB = afterRetargetB;
        while (TestAccess::legatoBlend(engine, stringIndex) < 1.0f)
        {
            renderInto(engine, trace, traceFrames);
            const float currentB = TestAccess::snapshot(engine, stringIndex)
                                       .inharmonicity;
            const float livePitch = TestAccess::programmedLegatoFrequency(
                engine, stringIndex);
            const float expectedB = sourceB * livePitch * livePitch
                                  / (sourcePitch * sourcePitch);
            expect(currentB <= previousB * 1.00001f,
                   "descending chained-slide stiffness reversed direction");
            expect(std::abs(currentB / expectedB - 1.0f) < 0.01f,
                   "descending slide stiffness did not follow 1/L^2");
            previousB = currentB;
        }
        expect(TestAccess::snapshot(engine, stringIndex).inharmonicity
                   == targetB,
               "a chained slide missed its exact endpoint stiffness");

        // Completion dirties the quantised fit once. A later sub-quantum
        // wheel tick must not keep re-fitting merely because this voice has
        // legato history.
        const float endpointFit = TestAccess::lastConfiguredSemitones(
            engine, stringIndex);
        engine.setPitchBend(0.01f);
        renderInto(engine, trace, traceFrames);
        expect(TestAccess::lastConfiguredSemitones(engine, stringIndex)
                   == endpointFit,
               "a completed slide re-fitted stiffness on a sub-quantum wheel "
               "tick");
    }

    // A second legato gesture can arrive while the first slide is still in
    // flight. Its source is the pitch under the finger at that sample, not the
    // unfinished slide's old destination; otherwise the delay target jumps to
    // that destination before beginning the next move. Slide and Hammer share
    // this state handoff even though their new travel times differ.
    for (const auto retargetStyle : { PlayStyle::Slide, PlayStyle::Hammer })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.bodyResonance = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.bendTimeSeconds = 0.28f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.85f);
        StereoBuffer establish(static_cast<int>(0.30 * sampleRate));
        renderInto(engine, establish);
        constexpr int stringIndex = 3;
        const float settledEffective = TestAccess::effectiveLoopFrequency(
            engine, stringIndex);
        const float settledHorizontal = TestAccess::effectiveLoopFrequency(
            engine, stringIndex, true);
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(57, 0.84f);
        const float firstRetargetEffective =
            TestAccess::effectiveLoopFrequency(engine, stringIndex);
        const float firstRetargetHorizontal =
            TestAccess::effectiveLoopFrequency(engine, stringIndex, true);
        expect(std::abs(centsBetween(firstRetargetEffective, settledEffective))
                   < 2.0,
               "a settled string changed effective pitch when a slide began: "
                   + std::to_string(centsBetween(firstRetargetEffective,
                                                 settledEffective))
                   + " cents");
        expect(std::abs(centsBetween(firstRetargetHorizontal,
                                     settledHorizontal)) < 2.0,
               "a settled string changed horizontal effective pitch when a "
               "slide began");
        StereoBuffer firstLeg(static_cast<int>(0.10 * sampleRate));
        renderInto(engine, firstLeg);

        const float before = TestAccess::programmedLegatoFrequency(
            engine, stringIndex);
        const float beforeEffective = TestAccess::effectiveLoopFrequency(
            engine, stringIndex);
        const float beforeHorizontal = TestAccess::effectiveLoopFrequency(
            engine, stringIndex, true);
        const float blendBefore = TestAccess::legatoBlend(engine, stringIndex);
        const double beforeMidi = 69.0
            + 12.0 * std::log2(static_cast<double>(before) / 440.0);
        expect(TestAccess::stringForNote(engine, 57) == stringIndex
                   && blendBefore > 0.0f && blendBefore < 1.0f
                   && beforeMidi > 48.0 && beforeMidi < 55.0,
               "invalid unfinished chained-legato fixture");

        engine.noteOn(styleKeyswitch(retargetStyle), 1.0f);
        engine.noteOn(52, 0.82f);
        const float after = TestAccess::programmedLegatoFrequency(
            engine, stringIndex);
        expect(std::abs(centsBetween(after, before)) < 0.1,
               std::string(retargetStyle == PlayStyle::Slide
                               ? "a chained slide" : "a mid-slide hammer")
                   + " jumped " + std::to_string(centsBetween(after, before))
                   + " cents at its retarget");
        expect(std::abs(centsBetween(
                   TestAccess::legatoFromFrequency(engine, stringIndex), before))
                   < 0.1,
               "a chained legato did not capture the live pitch as its source");
        const auto retargeted = TestAccess::snapshot(engine, stringIndex);
        const float afterEffective = TestAccess::effectiveLoopFrequency(
            engine, stringIndex);
        const float afterHorizontal = TestAccess::effectiveLoopFrequency(
            engine, stringIndex, true);
        expect(std::abs(centsBetween(afterEffective, beforeEffective)) < 2.0,
               std::string(retargetStyle == PlayStyle::Slide
                               ? "a chained slide" : "a mid-slide hammer")
                   + " changed effective pitch by "
                   + std::to_string(centsBetween(afterEffective,
                                                 beforeEffective))
                   + " cents at its retarget");
        expect(std::abs(centsBetween(afterHorizontal, beforeHorizontal)) < 2.0,
               "a chained legato changed horizontal effective pitch at its "
               "retarget");
        if (retargetStyle == PlayStyle::Hammer)
        {
            expect(retargeted.verticalWeight > retargeted.horizontalWeight,
                   "an ascending mid-slide Hammer was misclassified as a "
                   "pull-off from the unfinished destination");
        }

        constexpr int settleChunkFrames = 64;
        int settleFrames = 0;
        StereoBuffer settleChunk(settleChunkFrames);
        while (TestAccess::legatoBlend(engine, stringIndex) < 1.0f
               && settleFrames < static_cast<int>(0.45 * sampleRate))
        {
            renderInto(engine, settleChunk);
            settleFrames += settleChunkFrames;
        }
        if (retargetStyle == PlayStyle::Slide)
        {
            expect(settleFrames > static_cast<int>(0.06 * sampleRate)
                       && settleFrames < static_cast<int>(0.09 * sampleRate),
                   "a chained slide timed its new leg from the unfinished "
                   "destination instead of the live finger position");
        }
        const auto final = TestAccess::snapshot(engine, stringIndex);
        expect(TestAccess::legatoBlend(engine, stringIndex) == 1.0f
                   && final.midiNote == 52
                   && std::abs(centsBetween(final.baseFrequency, midiHz(52)))
                          < 0.1,
               "a chained legato did not settle on its new destination");
    }

    // Demo 17 redirects a descending top-string slide after 51 ms. The raw
    // delay target is phase-compensated whenever the dispersion fit crosses a
    // grid cell; its physical period must keep moving toward the new fret at
    // those refits instead of briefly reversing direction.
    {
        constexpr double demoSampleRate = 44100.0;
        ElectryEngine engine;
        engine.prepare(demoSampleRate, 512);
        EngineParameters parameters;
        parameters.pickupSelector = PickupSelector::Bridge;
        parameters.pickupType = 0.42f;
        parameters.toneKnob = 0.92f;
        parameters.bodyResonance = 0.42f;
        parameters.stringAge = 0.06f;
        parameters.pickPosition = 0.27f;
        parameters.pickHardness = 0.78f;
        parameters.fingerNoise = 0.52f;
        parameters.artifactAmount = 0.12f;
        parameters.bendTimeSeconds = 0.16f;
        parameters.sympatheticAmount = 0.28f;
        parameters.outputGain = 1.55f;
        engine.setParameters(parameters);
        engine.reset();
        StereoBuffer parameterLeadIn(static_cast<int>(0.25 * demoSampleRate));
        renderInto(engine, parameterLeadIn);
        engine.noteOn(76, 0.92f);
        StereoBuffer establish(static_cast<int>(0.22 * demoSampleRate));
        renderInto(engine, establish);
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(68, 0.82f);
        StereoBuffer firstLeg(static_cast<int>(0.051 * demoSampleRate));
        renderInto(engine, firstLeg);

        constexpr int stringIndex = 7;
        const float liveBefore = TestAccess::programmedLegatoFrequency(
            engine, stringIndex);
        const double liveBeforeMidi = 69.0
            + 12.0 * std::log2(static_cast<double>(liveBefore) / 440.0);
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(69, 0.82f);
        expect(TestAccess::stringForNote(engine, 69) == stringIndex
                   && liveBeforeMidi > 71.5 && liveBeforeMidi < 72.5,
               "invalid descending demo-17 chained-slide fixture");

        float previous = TestAccess::effectiveLoopFrequency(
            engine, stringIndex);
        float lowWater = previous;
        double worstWrongWayCents = 0.0;
        const int traceFrames = TestAccess::hostFramesPerControlPeriod(engine);
        StereoBuffer frame(traceFrames);
        int rendered = 0;
        while (TestAccess::legatoBlend(engine, stringIndex) < 1.0f
               && rendered < static_cast<int>(0.08 * demoSampleRate))
        {
            renderInto(engine, frame, traceFrames);
            const float current = TestAccess::effectiveLoopFrequency(
                engine, stringIndex);
            worstWrongWayCents = std::max(
                worstWrongWayCents, centsBetween(current, lowWater));
            lowWater = std::min(lowWater, current);
            previous = current;
            rendered += traceFrames;
        }
        expect(TestAccess::legatoBlend(engine, stringIndex) == 1.0f
                   && rendered > static_cast<int>(0.03 * demoSampleRate)
                   && rendered < static_cast<int>(0.05 * demoSampleRate),
               "the demo-17 chained slide did not reproduce its 38.7 ms "
               "remainder");
        double finalPitch = midiHz(69);
#if ELECTRY_ENERGY_ATTACK_PITCH
        finalPitch *= TestAccess::attackPitchState(engine, stringIndex)
                          .frequencyFactor;
#endif
        const double arrivalErrorCents = std::abs(
            centsBetween(previous, finalPitch));
        expect(arrivalErrorCents < 35.0,
               "the demo-17 delay lagged the arriving finger by "
                   + std::to_string(arrivalErrorCents) + " cents");
        int settling = 0;
        while (std::abs(centsBetween(previous, finalPitch)) >= 2.0
               && settling < static_cast<int>(0.05 * demoSampleRate))
        {
            renderInto(engine, frame, traceFrames);
            const float current = TestAccess::effectiveLoopFrequency(
                engine, stringIndex);
            worstWrongWayCents = std::max(
                worstWrongWayCents, centsBetween(current, lowWater));
            lowWater = std::min(lowWater, current);
            previous = current;
            settling += traceFrames;
        }
        expect(worstWrongWayCents < 0.05,
               "a descending chained slide reversed by "
                   + std::to_string(worstWrongWayCents)
                   + " cents at a loop-filter refit or delay glide");
        expect(std::abs(centsBetween(previous, finalPitch)) < 2.0,
               "the monotone demo-17 slide did not settle within 50 ms");
    }

    // The travel time is a distance over a hand speed, so a two-fret slide is
    // far shorter than a twelve-fret one. A fixed legato time would make them
    // equal.
    const auto shortSlide = renderSlide(45, 47, 0.8f, 0.28f);
    expect(shortSlide.settleSamples * 3 < longSlide.settleSamples,
           "a two-fret slide took nearly as long as a twelve-fret one (short "
               + std::to_string(shortSlide.settleSamples) + " samples, long "
               + std::to_string(longSlide.settleSamples) + ")");

    // The winding drags: the scrape is what Finger Noise controls, and at zero
    // it is exactly absent rather than merely quiet.
    const auto silentSlide = renderSlide(45, 57, 0.0f, 0.28f);
    expect(silentSlide.frictionAmplitude == 0.0f,
           "the slide scrape survived a silent Finger Noise control");
    expect(longSlide.frictionAmplitude > 0.0f,
           "the slide produced no scrape at all");
    expect(longSlide.excitationAmplitude == 0.0f
               && silentSlide.excitationAmplitude == 0.0f,
           "a travelling finger injected a new pick-like string attack");

    // The level already follows the normalized smoothstep velocity
    // 6 b (1 - b). The two friction poles must follow that same coordinate:
    // 1.125 times the average centre near each quarter and 1.5 times at the
    // midpoint, subject only to the existing 200 Hz and 0.40-rate clamps.
    // Probe the control update directly so block size and oversampling do not
    // turn an exact coefficient contract into an audio-grid tolerance.
    struct BandPoint
    {
        float blend { 0.0f };
        double centreHz { 0.0 };
        std::array<float, 2> coefficients {{ 0.0f, 0.0f }};
    };
    struct BandTrace
    {
        int stringIndex { -1 };
        double internalRate { 0.0 };
        float averageCentreHz { 0.0f };
        float frictionAmplitude { 0.0f };
        std::array<float, 2> contactCoefficients {{ 0.0f, 0.0f }};
        std::array<BandPoint, 3> points {};
    };
    const auto traceBand = [] (double hostRate, int fromNote, int toNote,
                               float fingerNoise, float bendTime,
                               PlayStyle style)
    {
        ElectryEngine engine;
        engine.prepare(hostRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.bodyResonance = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.fingerNoise = fingerNoise;
        parameters.bendTimeSeconds = bendTime;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(fromNote, 0.85f);
        StereoBuffer establish(static_cast<int>(0.30 * hostRate));
        renderInto(engine, establish);
        engine.noteOn(styleKeyswitch(style), 1.0f);
        engine.noteOn(toNote, 0.85f);

        BandTrace trace;
        trace.stringIndex = TestAccess::stringForNote(engine, toNote);
        trace.internalRate = TestAccess::internalSampleRate(engine);
        if (trace.stringIndex < 0)
            return trace;
        trace.averageCentreHz = TestAccess::slideAverageBandCentreHz(
            engine, trace.stringIndex);
        trace.frictionAmplitude = TestAccess::slideNoiseAmplitude(
            engine, trace.stringIndex);
        trace.contactCoefficients = TestAccess::slideBandCoefficients(
            engine, trace.stringIndex);
        constexpr std::array<float, 3> blends {{ 0.25f, 0.50f, 0.75f }};
        for (std::size_t point = 0; point < blends.size(); ++point)
        {
            TestAccess::updateSlideControlAt(
                engine, trace.stringIndex, blends[point]);
            trace.points[point].blend = TestAccess::legatoBlend(
                engine, trace.stringIndex);
            trace.points[point].centreHz = TestAccess::slideBandCentreHz(
                engine, trace.stringIndex);
            trace.points[point].coefficients =
                TestAccess::slideBandCoefficients(engine, trace.stringIndex);
        }
        return trace;
    };

    constexpr float twoPi = 6.28318530717958647692f;
    for (const double hostRate : { 44100.0, 48000.0, 96000.0,
                                   192000.0, 384000.0 })
    {
        const auto trace = traceBand(
            hostRate, 45, 57, 0.8f, 0.80f, PlayStyle::Slide);
        const float ceiling = 0.40f * static_cast<float>(trace.internalRate);
        expect(trace.stringIndex == 3 && trace.frictionAmplitude > 0.0f
                   && trace.averageCentreHz > 200.0f
                   && 1.5f * trace.averageCentreHz < ceiling,
               "invalid unclamped slide-band fixture at "
                   + std::to_string(hostRate) + " Hz");
        constexpr std::array<float, 3> expectedBlends {{
            0.25f, 0.50f, 0.75f
        }};
        for (std::size_t index = 0; index < trace.points.size(); ++index)
        {
            const auto& point = trace.points[index];
            const float motion = 6.0f * point.blend
                               * (1.0f - point.blend);
            const float expectedCentre = electry::clampf(
                trace.averageCentreHz * motion, 200.0f, ceiling);
            const float inverseRate = 1.0f
                / static_cast<float>(trace.internalRate);
            const float expectedHigh = std::exp(
                -twoPi * std::min(1.6f * expectedCentre,
                                  0.45f * static_cast<float>(trace.internalRate))
                * inverseRate);
            const float expectedLow = std::exp(
                -twoPi * 0.6f * expectedCentre * inverseRate);
            expect(std::abs(point.blend - expectedBlends[index]) < 2.0e-6f,
                   "the exact slide-band blend probe missed its target");
            expect(std::abs(point.centreHz - expectedCentre)
                       < std::max(0.05, 2.0e-4 * expectedCentre),
                   "the friction centre missed normalized finger speed at "
                       + std::to_string(hostRate) + " Hz");
            expect(std::abs(point.coefficients[0] - expectedHigh) < 2.0e-6f
                       && std::abs(point.coefficients[1] - expectedLow)
                              < 2.0e-6f,
                   "the friction poles missed their control-rate centre at "
                       + std::to_string(hostRate) + " Hz");
        }
        expect(std::abs(trace.points[0].centreHz / trace.averageCentreHz
                            - 1.125) < 2.0e-3
                   && std::abs(trace.points[1].centreHz
                                   / trace.averageCentreHz - 1.5) < 2.0e-3,
               "the friction centre did not follow the smoothstep-velocity "
               "ratios");
        expect(std::abs(trace.points[0].centreHz
                            / trace.points[2].centreHz - 1.0) < 2.0e-3,
               "symmetric slide speeds produced asymmetric friction bands");
    }

    const auto floorTrace = traceBand(
        48000.0, 85, 86, 0.8f, 2.0f, PlayStyle::Slide);
    expect(floorTrace.stringIndex == 7
               && floorTrace.averageCentreHz * 1.5f < 200.0f,
           "invalid low-clamp slide-band fixture");
    for (const auto& point : floorTrace.points)
        expect(std::abs(point.centreHz - 200.0) < 0.05,
               "the moving slide band escaped its 200 Hz floor");

    const auto ceilingTrace = traceBand(
        44100.0, 35, 47, 0.8f, 0.04f, PlayStyle::Slide);
    const double ceilingHz = 0.40 * ceilingTrace.internalRate;
    expect(ceilingTrace.stringIndex == 1
               && ceilingTrace.averageCentreHz < ceilingHz
               && ceilingTrace.averageCentreHz * 1.5f > ceilingHz,
           "invalid high-clamp slide-band fixture (string "
               + std::to_string(ceilingTrace.stringIndex) + ", average "
               + std::to_string(ceilingTrace.averageCentreHz) + " Hz, ceiling "
               + std::to_string(ceilingHz) + " Hz)");
    expect(ceilingTrace.points[0].centreHz < ceilingHz * 0.99
               && std::abs(ceilingTrace.points[1].centreHz - ceilingHz)
                      < ceilingHz * 2.0e-4
               && ceilingTrace.points[2].centreHz < ceilingHz * 0.99,
           "the moving slide band missed its rate-relative ceiling");

    const auto silentBand = traceBand(
        sampleRate, 45, 57, 0.0f, 0.80f, PlayStyle::Slide);
    expect(silentBand.averageCentreHz > 0.0f
               && silentBand.frictionAmplitude == 0.0f,
           "invalid zero-Finger-Noise slide-band fixture");
    for (const auto& point : silentBand.points)
        expect(point.coefficients == silentBand.contactCoefficients,
               "zero Finger Noise did not bypass slide-band updates exactly");

    const auto hammerBand = traceBand(
        sampleRate, 45, 47, 0.8f, 0.80f, PlayStyle::Hammer);
    expect(hammerBand.stringIndex == 3
               && hammerBand.averageCentreHz == 0.0f
               && hammerBand.frictionAmplitude == 0.0f,
           "invalid non-Slide band-bypass fixture");
    for (const auto& point : hammerBand.points)
        expect(point.coefficients == hammerBand.contactCoefficients,
               "a non-Slide gesture updated the friction band");

    const int frictionStart = slideStart;
    const int frictionLength = std::max(1024, longSlide.settleSamples);
    const auto scrapeEnergy = [&] (const Slide& withNoise, const Slide& without)
    {
        double sum = 0.0;
        for (int i = frictionStart; i < frictionStart + frictionLength; ++i)
        {
            const double difference =
                static_cast<double>(withNoise.audio.left[
                    static_cast<std::size_t>(i)])
                - static_cast<double>(without.audio.left[
                    static_cast<std::size_t>(i)]);
            sum += difference * difference;
        }
        return std::sqrt(sum / frictionLength);
    };
    const double woundScrape = scrapeEnergy(longSlide, silentSlide);

    // A plain string has no winding, so the same gesture barely makes a sound.
    const auto plainLoud = renderSlide(64, 71, 0.8f, 0.28f);
    const auto plainSilent = renderSlide(64, 71, 0.0f, 0.28f);
    expect(plainLoud.stringIndex == 7,
           "the plain-string slide did not stay on the top string");
    double plainSum = 0.0;
    for (int i = frictionStart; i < frictionStart + frictionLength; ++i)
    {
        const double difference =
            static_cast<double>(plainLoud.audio.left[static_cast<std::size_t>(i)])
            - static_cast<double>(plainSilent.audio.left[
                static_cast<std::size_t>(i)]);
        plainSum += difference * difference;
    }
    const double plainScrape = std::sqrt(plainSum / frictionLength);
    expect(plainScrape < woundScrape * 0.5,
           "a plain string squeaks as loudly as a wound one (wound "
               + std::to_string(woundScrape) + ", plain "
               + std::to_string(plainScrape) + ")");

    // The squeak's pitch is the rate the winding ridges pass under the finger,
    // so a fast hand squeaks high and a slow one low. This is asserted on the
    // band the engine actually configures rather than on the rendered audio,
    // and the reason is worth recording: the loaded pickup coil is a
    // second-order low-pass at a couple of kilohertz, so it flattens most of
    // the difference between a two-kilohertz squeak and an eight-kilohertz one
    // before it reaches the output. That is the instrument behaving correctly -
    // a real pickup does the same thing to a real squeak - but it means an
    // output-side centroid measures the coil rather than the friction.
    const auto bandCentre = [&] (float bendTime)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.fingerNoise = 0.8f;
        parameters.bendTimeSeconds = bendTime;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.85f);
        StereoBuffer lead(static_cast<int>(0.30 * sampleRate));
        renderInto(engine, lead);
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(57, 0.85f);
        const int stringIndex = TestAccess::stringForNote(engine, 57);
        if (stringIndex < 0)
            return 0.0;
        TestAccess::updateSlideControlAt(engine, stringIndex, 0.5f);
        return TestAccess::slideBandCentreHz(engine, stringIndex);
    };
    const double fastCentre = bandCentre(0.20f);
    const double slowCentre = bandCentre(0.80f);
    expect(fastCentre > slowCentre * 3.0,
           "the squeak's band does not follow the speed of the hand (fast "
               + std::to_string(fastCentre) + " Hz, slow "
               + std::to_string(slowCentre) + " Hz)");

    // It is audible, though: the same gesture taken at two speeds renders two
    // different sounds rather than one scaled in time.
    const auto fastSlide = renderSlide(45, 57, 0.8f, 0.20f);
    const auto fastSilent = renderSlide(45, 57, 0.0f, 0.20f);
    const auto slowSlide = renderSlide(45, 57, 0.8f, 0.80f);
    const auto slowSilent = renderSlide(45, 57, 0.0f, 0.80f);
    const auto differenceOf = [] (const Slide& a, const Slide& b)
    {
        std::vector<float> out(a.audio.left.size(), 0.0f);
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = a.audio.left[i] - b.audio.left[i];
        return out;
    };
    const auto fastFriction = differenceOf(fastSlide, fastSilent);
    const auto slowFriction = differenceOf(slowSlide, slowSilent);

    // Removing an otherwise identical silent scrape leaves only the contact
    // generator. The recovered poles above carry the directional oracle: a
    // loaded pickup followed by one finite noise realization does not preserve
    // reliable centroid ordering. Here each early/middle/late window instead
    // proves that the moving source remains audible and finite, and that no
    // control-period phase acquires a disproportionate first-difference spike.
    expect(slowSlide.sweepSamples == slowSilent.sweepSamples
               && slowSlide.sweepSamples[0] > 0
               && slowSlide.sweepSamples[2]
                      < static_cast<int>(slowFriction.size()),
           "invalid friction-only early/mid/late sweep fixture");
    const int sweepWindow = static_cast<int>(0.06 * sampleRate);
    std::array<double, 3> sweepCentres {};
    std::array<double, 3> sweepRms {};
    for (std::size_t point = 0; point < sweepCentres.size(); ++point)
    {
        const int start = std::max(
            0, slowSlide.sweepSamples[point] - sweepWindow / 2);
        sweepCentres[point] = broadbandCentroid(
            slowFriction, start, sweepWindow);
        double energy = 0.0;
        for (int sample = start; sample < start + sweepWindow; ++sample)
        {
            const double value = slowFriction[
                static_cast<std::size_t>(sample)];
            energy += value * value;
        }
        sweepRms[point] = std::sqrt(energy / sweepWindow);
    }
    std::cout << "PROBE slide friction early/mid/late centroids: "
              << sweepCentres[0] << "/" << sweepCentres[1] << "/"
              << sweepCentres[2] << " Hz; RMS " << sweepRms[0] << "/"
              << sweepRms[1] << "/" << sweepRms[2] << "\n";
    expect(std::isfinite(sweepCentres[0])
               && std::isfinite(sweepCentres[1])
               && std::isfinite(sweepCentres[2])
               && sweepCentres[0] > 300.0
               && sweepCentres[1] > 300.0
               && sweepCentres[2] > 300.0
               && sweepRms[0] > 1.0e-7
               && sweepRms[1] > 1.0e-7
               && sweepRms[2] > 1.0e-7,
           "friction-only output was absent or non-finite (early "
               + std::to_string(sweepCentres[0]) + " Hz, middle "
               + std::to_string(sweepCentres[1]) + " Hz, late "
               + std::to_string(sweepCentres[2]) + " Hz)");

    std::vector<double> phaseStepEnergy(
        static_cast<std::size_t>(slowSlide.controlFrames), 0.0);
    std::vector<int> phaseStepCount(
        static_cast<std::size_t>(slowSlide.controlFrames), 0);
    double maximumStep = 0.0;
    bool finiteSweep = true;
    const int sweepFirst = slowSlide.sweepSamples[0];
    const int sweepLast = slowSlide.sweepSamples[2];
    for (int sample = sweepFirst + 1; sample <= sweepLast; ++sample)
    {
        const double current = slowFriction[static_cast<std::size_t>(sample)];
        const double previous = slowFriction[
            static_cast<std::size_t>(sample - 1)];
        const double step = current - previous;
        finiteSweep = finiteSweep && std::isfinite(current)
                                  && std::isfinite(step);
        maximumStep = std::max(maximumStep, std::abs(step));
        const std::size_t phase = static_cast<std::size_t>(
            (sample - sweepFirst) % slowSlide.controlFrames);
        phaseStepEnergy[phase] += step * step;
        ++phaseStepCount[phase];
    }
    double phaseMean = 0.0;
    double phaseMaximum = 0.0;
    for (std::size_t phase = 0; phase < phaseStepEnergy.size(); ++phase)
    {
        const double rms = std::sqrt(
            phaseStepEnergy[phase]
            / std::max(phaseStepCount[phase], 1));
        phaseMean += rms;
        phaseMaximum = std::max(phaseMaximum, rms);
    }
    phaseMean /= static_cast<double>(phaseStepEnergy.size());
    const double phaseRatio = phaseMaximum
        / std::max(phaseMean, 1.0e-12);
    std::cout << "PROBE slide friction maximum step/control-phase ratio: "
              << maximumStep << "/" << phaseRatio << "\n";
    expect(finiteSweep && maximumStep < 0.25
               && phaseMaximum < 1.35 * phaseMean,
           "the moving friction band produced a non-finite, unbounded or "
           "control-period click (maximum step "
               + std::to_string(maximumStep) + ", phase ratio "
               + std::to_string(phaseRatio)
               + ")");

    const double fastCentroid = broadbandCentroid(
        fastFriction, slideStart, std::max(1024, fastSlide.settleSamples));
    const double slowCentroid = broadbandCentroid(
        slowFriction, slideStart, std::max(1024, slowSlide.settleSamples));
    expect(fastCentroid > 300.0 && slowCentroid > 300.0
               && std::abs(fastCentroid - slowCentroid) > 150.0,
           "two slides at different hand speeds produced the same friction "
           "(fast " + std::to_string(fastCentroid) + " Hz, slow "
               + std::to_string(slowCentroid) + " Hz)");
}

// The pinch harmonic is the same touch filter driven by the picking hand:
// the thumb catches the string at the pick's own position, so which partial
// squeals is a function of Pick Position rather than a fixed interval.
void testPinchHarmonic()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    const int note = 52;
    const double f0 = midiHz(note);
    const int start = static_cast<int>(0.02 * sampleRate);
    const int window = static_cast<int>(0.12 * sampleRate);

    const auto partialMagnitudes = [&] (const StereoBuffer& buffer)
    {
        std::array<double, 17> magnitudes {};
        for (int n = 1; n <= 16; ++n)
            magnitudes[static_cast<std::size_t>(n)] = trackedPartialMagnitude(
                buffer.left, start, window, sampleRate, f0, n);
        return magnitudes;
    };

    // Energy-weighted mean partial index: where in the harmonic series the
    // note's weight actually sits. A pinch moves it a long way up.
    const auto meanPartial = [] (const std::array<double, 17>& magnitudes)
    {
        double weighted = 0.0;
        double total = 0.0;
        for (int n = 1; n <= 16; ++n)
        {
            const double magnitude = magnitudes[static_cast<std::size_t>(n)];
            const double power = magnitude * magnitude;
            weighted += power * n;
            total += power;
        }
        return total > 0.0 ? weighted / total : 0.0;
    };

    const auto renderAt = [&] (float pickPosition, PlayStyle style)
    {
        parameters.pickPosition = pickPosition;
        engine.setParameters(parameters);
        return renderNote(engine, sampleRate, note, 0.9f, style, 1.0);
    };

    // At this fret the neck-end setting is a real far-side contact, not a
    // midpoint synonym. Pin both the absolute metre-derived fraction and the
    // fact that the thumb reads that same position before scoring its spectrum.
    parameters.pickPosition = 1.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Pinch), 1.0f);
    engine.noteOn(note, 0.9f);
    const int farString = TestAccess::stringForNote(engine, note);
    const auto farVoice = TestAccess::snapshot(engine, farString);
    const auto farTouch = TestAccess::touchGeometry(engine, farString);
    const float farExpected = electry::clampf(
        (0.48f + farVoice.strokeContactOffsetMetres
                     / TestAccess::scaleLengthMetres(engine))
            * std::exp2(static_cast<float>(farVoice.fret) / 12.0f),
        0.02f, 0.98f);
    expect(farTouch.fraction > 0.5f
               && std::abs(farTouch.fraction - farExpected) < 1.0e-6f
               && std::abs(farVoice.excitationCombDelay
                                / farVoice.lastCompensatedPeriod
                            - farTouch.fraction) < 1.0e-6f,
           "the far-side pinch did not keep the pick and thumb at their "
           "physical position");

    const auto pickedNearBridge = renderAt(0.18f, PlayStyle::Sustain);
    const auto pinchedNearBridge = renderAt(0.18f, PlayStyle::Pinch);
    const auto pinchedNearMidpoint = renderAt(0.90f, PlayStyle::Pinch);
    const auto pinchedFarSide = renderAt(1.0f, PlayStyle::Pinch);

    const auto strongestPartial = [] (const std::array<double, 17>& magnitudes)
    {
        int best = 1;
        double bestMagnitude = -1.0;
        for (int n = 1; n <= 16; ++n)
        {
            const double magnitude = magnitudes[static_cast<std::size_t>(n)];
            if (magnitude > bestMagnitude)
            {
                bestMagnitude = magnitude;
                best = n;
            }
        }
        return best;
    };

    const auto pickedPartials = partialMagnitudes(pickedNearBridge);
    const auto pinchedPartials = partialMagnitudes(pinchedNearBridge);
    const auto midpointPartials = partialMagnitudes(pinchedNearMidpoint);
    const auto farSidePartials = partialMagnitudes(pinchedFarSide);
    const double pickedMean = meanPartial(pickedPartials);
    const double pinchedMean = meanPartial(pinchedPartials);
    const double midpointMean = meanPartial(midpointPartials);
    const double farSideMean = meanPartial(farSidePartials);

    // Measured with per-partial peak tracking so stiffness and window leakage
    // cannot masquerade as articulation separation.
    // The direct node-selection checks below carry the stronger requirement:
    // the near-bridge peak must be at least the sixth partial and gain more
    // than 6 dB against the ordinary stroke. These mean-index rails only keep
    // the broad spectral direction from collapsing around that peak.
    expect(pinchedMean > pickedMean + 1.0,
           "the pinch did not move the note's weight up the harmonic series "
           "(picked " + std::to_string(pickedMean) + ", pinched "
               + std::to_string(pinchedMean) + ")");
    expect(midpointMean < pinchedMean - 1.25,
           "moving the picking hand near the midpoint did not move the squeal "
           "down the series (bridge " + std::to_string(pinchedMean)
               + ", midpoint " + std::to_string(midpointMean) + ")");
    expect(farSideMean > midpointMean + 1.5,
           "crossing the midpoint did not reverse the co-located pick/thumb "
           "mode sequence (midpoint " + std::to_string(midpointMean)
               + ", far side " + std::to_string(farSideMean) + ")");

    // The touch sits at the pick, so the surviving partial is the one with a
    // node there: around the eighth near the bridge, and a low even partial
    // with the hand just short of the midpoint. Crossing the midpoint then
    // walks the same physical mode sequence back upward with different phase;
    // it is not a synonym for the midpoint. Requiring exactly partial two used
    // to pin the filter-phase-shortened raw delay rather than a physical node
    // coordinate.
    const int bridgeSquealPartial = strongestPartial(pinchedPartials);
    const int midpointSquealPartial = strongestPartial(midpointPartials);
    const int farSideSquealPartial = strongestPartial(farSidePartials);
    expect(bridgeSquealPartial >= 6,
           "the near-bridge pinch did not select a high partial (strongest "
               + std::to_string(bridgeSquealPartial) + ")");
    expect((midpointSquealPartial == 2 || midpointSquealPartial == 4)
               && midpointSquealPartial < bridgeSquealPartial,
           "the near-midpoint pinch did not select a low even partial "
           "(strongest " + std::to_string(midpointSquealPartial) + ")");
    expect(farSideSquealPartial >= 6
               && farSideSquealPartial != midpointSquealPartial,
           "the far-side pinch collapsed back onto the midpoint spectrum "
           "(strongest " + std::to_string(farSideSquealPartial) + ")");

    // Measured against the ordinary pick stroke, the squeal partial gains a
    // long way on the fundamental. This is the effect itself rather than a
    // proxy for it.
    const auto partialOverFundamental = [] (
        const std::array<double, 17>& magnitudes, int n)
    {
        return decibels(
            magnitudes[static_cast<std::size_t>(n)]
            / std::max(magnitudes[1], 1.0e-15));
    };
    // A squeal peak can straddle neighbouring tracked partials, so score its
    // lift over every partial within 1 dB of the pinched peak instead of
    // letting a rounding-level winner swap own the articulation.
    const double pinchedPeak =
        pinchedPartials[static_cast<std::size_t>(bridgeSquealPartial)];
    double gain = -1.0e9;
    int gainPartial = bridgeSquealPartial;
    for (int n = 2; n <= 16; ++n)
    {
        const double magnitude = pinchedPartials[static_cast<std::size_t>(n)];
        if (magnitude < pinchedPeak * 0.891)  // within 1 dB of the peak
            continue;
        const double lift = partialOverFundamental(pinchedPartials, n)
                          - partialOverFundamental(pickedPartials, n);
        if (lift > gain)
        {
            gain = lift;
            gainPartial = n;
        }
    }
    expect(gain > 6.0,
           "the pinch did not lift its partial against the fundamental (gain "
               + std::to_string(gain) + " dB at partial "
               + std::to_string(gainPartial) + ")");
    std::cout << "PROBE pinch picked/bridge/midpoint/far means "
              << pickedMean << '/' << pinchedMean << '/' << midpointMean << '/'
              << farSideMean << ", strongest bridge/midpoint/far "
              << bridgeSquealPartial << '/' << midpointSquealPartial << '/'
              << farSideSquealPartial << ", gain " << gain
              << " dB at partial " << gainPartial << '\n';

    // It is its own articulation, not a relabelled one.
    const auto natural = renderAt(0.18f, PlayStyle::Harmonics);
    expect(normalisedDifferenceRms(pinchedNearBridge.left, pickedNearBridge.left,
                                   0, static_cast<int>(0.3 * sampleRate)) > 0.2,
           "a pinch renders nearly the same audio as an ordinary pick stroke");
    expect(normalisedDifferenceRms(pinchedNearBridge.left, natural.left, 0,
                                   static_cast<int>(0.3 * sampleRate)) > 0.2,
           "a pinch renders nearly the same audio as a natural harmonic");
    expect(normalisedDifferenceRms(pinchedFarSide.left,
                                   pinchedNearMidpoint.left, 0,
                                   static_cast<int>(0.3 * sampleRate)) > 0.2,
           "a far-side pinch rendered as a mirrored midpoint clamp");
}

// The fretting hand has a position and a reach, so the same pitch is not
// always fingered at the lowest fret that can produce it. The lead phrase
// below is the whole point of the change: under the lowest-fret rule its
// fourth note fell onto an open string in the middle of a line played at the
// fifth position, which is a different string, a different decay and a note
// the fretting hand is not touching at all.
void testFrettingHandPosition()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();

    // Open-position shapes are unchanged, because at the nut an open string
    // costs the hand nothing. This is the same C-major shape the allocation
    // test pins, repeated here so a hand-model regression is caught by the
    // check that owns the hand model.
    for (const int note : { 48, 52, 55, 60, 64 })
        engine.noteOn(note, 0.8f);
    expect(TestAccess::stringForNote(engine, 48) == 3
               && TestAccess::stringForNote(engine, 52) == 4
               && TestAccess::stringForNote(engine, 55) == 5
               && TestAccess::stringForNote(engine, 60) == 6
               && TestAccess::stringForNote(engine, 64) == 7,
           "the open C shape moved when the fretting hand was introduced");
    expect(TestAccess::frettingHandPosition(engine) == 0.0f,
           "an open-position chord moved the hand off the nut");

    // A descending lead phrase. Each note is picked, released, and given time
    // to retire, so every string is free when the next note arrives and the
    // only thing choosing between them is the hand.
    engine.reset();
    struct Placement
    {
        int string { -1 };
        int fret { -1 };
    };
    const auto playAndRelease = [&] (int note)
    {
        engine.noteOn(note, 0.8f);
        Placement placement;
        placement.string = TestAccess::stringForNote(engine, note);
        if (placement.string >= 0)
            placement.fret = TestAccess::snapshot(engine, placement.string).fret;
        engine.noteOff(note);
        // Let the damped string retire so every string is free again. It takes
        // under half a second, comfortably inside the second and a half after
        // which the hand would relax back to the nut.
        double waited = 0.0;
        while (engine.getActiveVoiceCount() > 0 && waited < 1.0)
        {
            StereoBuffer tail(static_cast<int>(0.25 * sampleRate));
            renderInto(engine, tail);
            waited += 0.25;
        }
        expect(engine.getActiveVoiceCount() == 0,
               "the released note did not retire between phrase notes");
        return placement;
    };

    // B4 from a cold hand is fretted at 7 on the top string, and the hand
    // settles two frets below it so the note sits under the middle fingers.
    const auto b4 = playAndRelease(71);
    expect(b4.string == 7 && b4.fret == 7,
           "B4 from the nut did not take the top string at the seventh fret");
    expect(std::abs(TestAccess::frettingHandPosition(engine) - 5.0f) < 1.0e-6f,
           "the hand did not settle below the note it had to reach for");

    const auto a4 = playAndRelease(69);
    expect(a4.string == 7 && a4.fret == 5,
           "A4 left the position the hand had just taken");

    // From here the lowest-fret rule and the hand disagree. G4 at the fifth
    // position is the eighth fret of the B string, not the third fret of the
    // top string, which is behind the index finger.
    const auto g4 = playAndRelease(67);
    expect(g4.string == 6 && g4.fret == 8,
           "G4 was fingered behind the hand instead of inside it");

    // The one that matters: E4 is an open string, and the old rule always took
    // it. In the fifth position it is the fifth fret of the B string.
    const auto e4 = playAndRelease(64);
    expect(e4.string == 6 && e4.fret == 5,
           "E4 fell back to the open string in the middle of a fretted phrase");

    const auto d4 = playAndRelease(62);
    expect(d4.string == 5 && d4.fret == 7,
           "D4 left the hand's position");

    // The phrase ends: nothing is held, and after a second and a half the hand
    // relaxes to the nut, so the same E4 is an open string again.
    StereoBuffer rest(static_cast<int>(2.0 * sampleRate));
    renderInto(engine, rest);
    const auto openAgain = playAndRelease(64);
    expect(openAgain.string == 7 && openAgain.fret == 0,
           "the hand did not return to the nut after the phrase ended");
    expect(TestAccess::frettingHandPosition(engine) == 0.0f,
           "the hand did not relax to the nut when the phrase ended");
}

void testSustainPedal()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    engine.setParameters(parameters);

    // Sustain pedal keeps a released note ringing.
    engine.reset();
    engine.setSustainPedal(true);
    engine.noteOn(45, 0.7f);
    StereoBuffer pedalBuffer(static_cast<int>(0.4 * sampleRate));
    renderInto(engine, pedalBuffer);
    engine.noteOff(45);
    renderInto(engine, pedalBuffer);
    expect(engine.getActiveVoiceCount() == 1,
           "sustain pedal did not hold the released note");

    const double heldRms = rmsInRange(pedalBuffer.left, 0, pedalBuffer.size());
    engine.setSustainPedal(false);
    StereoBuffer releasedBuffer(static_cast<int>(0.8 * sampleRate));
    renderInto(engine, releasedBuffer);
    const double lateRms = rmsInRange(releasedBuffer.left,
                                      static_cast<int>(0.6 * sampleRate),
                                      releasedBuffer.size());
    expect(lateRms < heldRms * 0.1,
           "note did not damp after the sustain pedal was released");
}

void testVibratoOnlyMovesFingeredStrings()
{
    constexpr double sampleRate = 48000.0;

    // The lowest playable note is the low string played open: nothing is
    // holding it down, so there is no contact for the hand to rock and a
    // fretting-hand vibrato cannot reach it. The bar can, but that is the
    // pitch wheel, not the A#0 fretting gesture.
    const auto renderNote = [&](int midiNote, float vibrato)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        engine.setParameters(parameters);
        engine.noteOn(midiNote, 0.8f);
        StereoBuffer settle(static_cast<int>(0.05 * sampleRate));
        renderInto(engine, settle);
        engine.setVibrato(vibrato);
        StereoBuffer buffer(static_cast<int>(0.40 * sampleRate));
        renderInto(engine, buffer);
        return buffer.left;
    };

    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.noteOn(ElectryEngine::lowestPlayableNote, 0.8f);
        StereoBuffer settle(512);
        renderInto(engine, settle);
        std::array<electry::StringVisualState, ElectryEngine::stringCount> strings {};
        engine.getStringVisualState(strings);
        bool open = false;
        for (const auto& string : strings)
            open = open || (string.sounding && string.fret == 0);
        expect(open,
               "the lowest playable note is not fingered at fret 0, so this "
               "test is no longer exercising an open string");
    }

    expect(renderNote(ElectryEngine::lowestPlayableNote, 1.0f)
               == renderNote(ElectryEngine::lowestPlayableNote, 0.0f),
           "the vibrato gesture bent an open string, which no finger can do");

    // The same gesture on a stopped note has to do something, or the check
    // above would pass simply by the control being dead. 47 is two frets up
    // the same string that 45 plays open.
    const auto fretted = renderNote(47, 1.0f);
    const auto still = renderNote(47, 0.0f);
    expect(fretted != still,
           "the vibrato gesture left a fingered string alone");
}

void testLegatoSlideDoesNotConsumeAPickStroke()
{
    constexpr double sampleRate = 48000.0;

    // A slide onto a string that is already sounding retargets it and strikes
    // nothing, so it must not advance the alternate sequence - the same reason
    // a hammer-on does not. Charging it a stroke would leave the next note
    // that really is picked on the wrong one.
    const auto strokeAfter = [&](bool slideInBetween)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        engine.setParameters(parameters);
        StereoBuffer buffer(static_cast<int>(0.05 * sampleRate));

        // Latch Alternate picking, then Sustain so the first note is picked.
        engine.noteOn(ElectryEngine::firstKeyswitchNote
                          + static_cast<int>(PickStyle::Alternate), 1.0f);
        engine.noteOn(ElectryEngine::firstPlayStyleKeyswitchNote
                          + static_cast<int>(PlayStyle::Sustain), 1.0f);
        engine.noteOn(45, 0.8f);
        renderInto(engine, buffer);

        if (slideInBetween)
        {
            engine.noteOn(ElectryEngine::firstPlayStyleKeyswitchNote
                              + static_cast<int>(PlayStyle::Slide), 1.0f);
            // Same string, different pitch: this retargets rather than picks.
            engine.noteOn(47, 0.8f);
            renderInto(engine, buffer);
            engine.noteOn(ElectryEngine::firstPlayStyleKeyswitchNote
                              + static_cast<int>(PlayStyle::Sustain), 1.0f);
        }

        // A note far enough away to take a different string, so it is a real
        // pick rather than another retarget.
        engine.noteOn(69, 0.8f);
        renderInto(engine, buffer);
        std::array<electry::StringVisualState, ElectryEngine::stringCount> strings {};
        engine.getStringVisualState(strings);
        for (const auto& string : strings)
            if (string.sounding && string.midiNote == 69)
                return string.strokeUp ? 1 : 0;
        return -1;
    };

    const int withoutSlide = strokeAfter(false);
    const int withSlide = strokeAfter(true);
    expect(withoutSlide >= 0 && withSlide >= 0,
           "the picked note after the slide never sounded, so its stroke could "
           "not be read");
    expect(withSlide == withoutSlide,
           "a legato slide consumed an alternate pick stroke it never played, "
           "so the next picked note came out on the wrong stroke");

    // The same fretting-hand move cannot move a real wrist stroke that is
    // already in flight. Start with A2 ringing on string 3, schedule an E4
    // downstroke from string 7 with Strum Spread, then slide A2 to B2 at the
    // same clock. Re-anchoring the pending stroke from the slide's string used
    // to push E4 several string crossings later even though no second pick was
    // involved.
    ElectryEngine scheduled;
    scheduled.prepare(sampleRate, 512);
    EngineParameters scheduledParameters;
    scheduledParameters.artifactAmount = 0.0f;
    scheduledParameters.sympatheticAmount = 0.0f;
    scheduledParameters.strumSpreadSeconds = 0.020f;
    scheduled.setParameters(scheduledParameters);
    scheduled.reset();
    scheduled.noteOn(45, 0.8f);
    StereoBuffer firstContact(static_cast<int>(0.080 * sampleRate));
    renderInto(scheduled, firstContact);
    const int aString = TestAccess::stringForNote(scheduled, 45);

    scheduled.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    scheduled.noteOn(64, 0.8f);
    const int eString = TestAccess::stringForNote(scheduled, 64);
    const int delayBeforeSlide = eString >= 0
        ? TestAccess::snapshot(scheduled, eString).startDelaySamples : -1;

    scheduled.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
    scheduled.noteOn(47, 0.8f);
    const int delayAfterSlide = eString >= 0
        ? TestAccess::snapshot(scheduled, eString).startDelaySamples : -1;
    expect(aString == 3 && eString == 7 && delayBeforeSlide > 0,
           "the pending-strum slide fixture did not use the intended strings");
    expect(TestAccess::stringForNote(scheduled, 47) == aString,
           "the scheduler fixture did not keep the slide on its ringing string");
    expect(delayAfterSlide == delayBeforeSlide,
           "a legato slide re-anchored and delayed a pending plectrum stroke ("
               + std::to_string(delayBeforeSlide) + " to "
               + std::to_string(delayAfterSlide) + " samples)");
}

void testParameterSanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters hostile;
    hostile.bodyWood = std::numeric_limits<float>::quiet_NaN();
    hostile.bodySize = -12.0f;
    hostile.bodyShape = 400.0f;
    hostile.construction = std::numeric_limits<float>::infinity();
    hostile.scaleLength = -std::numeric_limits<float>::infinity();
    hostile.pickupType = 55.0f;
    hostile.toneKnob = std::numeric_limits<float>::quiet_NaN();
    hostile.bodyResonance = 1.0e9f;
    hostile.stringGauge = -1.0e9f;
    hostile.stringAge = std::numeric_limits<float>::quiet_NaN();
    hostile.pickPosition = 2.0f;
    hostile.pickHardness = -2.0f;
    hostile.pickNoise = std::numeric_limits<float>::infinity();
    hostile.fingerNoise = -0.5f;
    hostile.releaseNoise = 77.0f;
    hostile.muteDamping = std::numeric_limits<float>::quiet_NaN();
    hostile.bendTimeSeconds = -3.0f;
    hostile.velocityAmount = 9.0f;
    hostile.outputGain = std::numeric_limits<float>::quiet_NaN();
    hostile.artifactAmount = std::numeric_limits<float>::infinity();
    hostile.sympatheticAmount = std::numeric_limits<float>::quiet_NaN();
    hostile.palmMute = -7.0f;
    hostile.strumSpreadSeconds = std::numeric_limits<float>::infinity();
    hostile.tremoloRateHz = std::numeric_limits<float>::quiet_NaN();
    hostile.resonanceDepth = std::numeric_limits<float>::quiet_NaN();
    hostile.vibratoDepth = 12.0f;
    hostile.pickupSelector = static_cast<PickupSelector>(999);
    hostile.outputMode = static_cast<electry::OutputMode>(999);
    engine.setParameters(hostile);

    auto buffer = renderNote(engine, sampleRate, 45, 0.9f, PlayStyle::Sustain,
                             0.6);
    expect(allFinite(buffer), "hostile parameters produced non-finite audio");
    expect(peakAbs(buffer.left) < 2.1f, "hostile parameters bypassed the guard");

    engine.setPitchBend(std::numeric_limits<float>::quiet_NaN());
    engine.setResonance(std::numeric_limits<float>::quiet_NaN());
    engine.noteOn(45, std::numeric_limits<float>::quiet_NaN());
    StereoBuffer more(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, more);
    expect(allFinite(more), "hostile performance input produced non-finite audio");

    // The acoustic return path must swallow hostile input outright.
    engine.setResonance(1.0f);
    engine.setAcousticReturnLevel(std::numeric_limits<float>::quiet_NaN());
    engine.setAcousticReturnLevel(1.0f);
    engine.pushAcousticReturn(nullptr, nullptr, 128);
    std::array<float, 64> poison {};
    poison.fill(std::numeric_limits<float>::quiet_NaN());
    engine.pushAcousticReturn(poison.data(), nullptr, -5);
    engine.pushAcousticReturn(poison.data(), poison.data(),
                              static_cast<int>(poison.size()));
    for (int flood = 0; flood < 300; ++flood)
        engine.pushAcousticReturn(poison.data(), poison.data(),
                                  static_cast<int>(poison.size()));
    engine.noteOn(45, 0.9f);
    StereoBuffer poisoned(static_cast<int>(0.3 * sampleRate));
    renderInto(engine, poisoned);
    expect(allFinite(poisoned),
           "a hostile acoustic return produced non-finite audio");
}

// testParameterSanitisation() above only ever asserts on the audio that comes
// out the far end of a hostile parameter set, which the guard itself could
// pass through a subtly wrong path and still leave finite and quiet. This
// calls the guard directly and checks its two distinct behaviours: a
// non-finite field falls back to the shipping default, while a finite but
// out-of-range field clamps to the nearer boundary of its own valid interval
// instead. A future change that swapped one behaviour for the other, or
// dropped a field from the guard entirely, would not necessarily move any
// rendered sample enough to fail the audio-level test above.
void testParameterSanitisationFallsBackToDefaults()
{
    const EngineParameters defaults;

    EngineParameters hostile;
    hostile.bodyWood = std::numeric_limits<float>::quiet_NaN();
    hostile.bodySize = -12.0f;
    hostile.bodyShape = 400.0f;
    hostile.construction = std::numeric_limits<float>::infinity();
    hostile.scaleLength = -std::numeric_limits<float>::infinity();
    hostile.pickupType = 55.0f;
    hostile.toneKnob = std::numeric_limits<float>::quiet_NaN();
    hostile.bodyResonance = 1.0e9f;
    hostile.stringGauge = -1.0e9f;
    hostile.stringAge = std::numeric_limits<float>::quiet_NaN();
    hostile.pickPosition = 2.0f;
    hostile.pickHardness = -2.0f;
    hostile.pickNoise = std::numeric_limits<float>::infinity();
    hostile.fingerNoise = -0.5f;
    hostile.releaseNoise = 77.0f;
    hostile.muteDamping = std::numeric_limits<float>::quiet_NaN();
    hostile.bendTimeSeconds = -3.0f;
    hostile.velocityAmount = 9.0f;
    hostile.outputGain = std::numeric_limits<float>::quiet_NaN();
    hostile.artifactAmount = std::numeric_limits<float>::infinity();
    hostile.sympatheticAmount = std::numeric_limits<float>::quiet_NaN();
    hostile.palmMute = -7.0f;
    hostile.strumSpreadSeconds = std::numeric_limits<float>::infinity();
    hostile.tremoloRateHz = std::numeric_limits<float>::quiet_NaN();
    hostile.resonanceDepth = std::numeric_limits<float>::quiet_NaN();
    hostile.vibratoDepth = 12.0f;
    hostile.pickupSelector = static_cast<PickupSelector>(999);
    hostile.outputMode = static_cast<electry::OutputMode>(999);

    const EngineParameters clean = TestAccess::sanitise(hostile);

    // Non-finite input has no boundary to clamp to, so it falls back to the
    // shipping default exactly.
    expect(clean.bodyWood == defaults.bodyWood,
           "NaN bodyWood did not fall back to its default");
    expect(clean.construction == defaults.construction,
           "+inf construction did not fall back to its default");
    expect(clean.scaleLength == defaults.scaleLength,
           "-inf scaleLength did not fall back to its default");
    expect(clean.toneKnob == defaults.toneKnob,
           "NaN toneKnob did not fall back to its default");
    expect(clean.stringAge == defaults.stringAge,
           "NaN stringAge did not fall back to its default");
    expect(clean.pickNoise == defaults.pickNoise,
           "+inf pickNoise did not fall back to its default");
    expect(clean.muteDamping == defaults.muteDamping,
           "NaN muteDamping did not fall back to its default");
    expect(clean.outputGain == defaults.outputGain,
           "NaN outputGain did not fall back to its default");
    expect(clean.artifactAmount == defaults.artifactAmount,
           "+inf artifactAmount did not fall back to its default");
    expect(clean.sympatheticAmount == defaults.sympatheticAmount,
           "NaN sympatheticAmount did not fall back to its default");
    expect(clean.strumSpreadSeconds == defaults.strumSpreadSeconds,
           "+inf strumSpreadSeconds did not fall back to its default");
    expect(clean.tremoloRateHz == defaults.tremoloRateHz,
           "NaN tremoloRateHz did not fall back to its default");
    expect(clean.resonanceDepth == defaults.resonanceDepth,
           "NaN resonanceDepth did not fall back to its default");

    // Finite but out-of-[0,1] input clamps to the nearer boundary rather than
    // falling back to the default - the same lambda handles every 0..1 field
    // and this is what tells its two branches apart.
    expect(clean.bodySize == 0.0f, "negative bodySize did not clamp to 0");
    expect(clean.bodyShape == 1.0f, "bodyShape above 1 did not clamp to 1");
    expect(clean.pickupType == 1.0f, "pickupType above 1 did not clamp to 1");
    expect(clean.bodyResonance == 1.0f,
           "huge bodyResonance did not clamp to 1");
    expect(clean.stringGauge == 0.0f,
           "hugely negative stringGauge did not clamp to 0");
    expect(clean.pickPosition == 1.0f,
           "pickPosition above 1 did not clamp to 1");
    expect(clean.pickHardness == 0.0f,
           "negative pickHardness did not clamp to 0");
    expect(clean.fingerNoise == 0.0f,
           "negative fingerNoise did not clamp to 0");
    expect(clean.releaseNoise == 1.0f,
           "releaseNoise above 1 did not clamp to 1");
    expect(clean.velocityAmount == 1.0f,
           "velocityAmount above 1 did not clamp to 1");
    expect(clean.palmMute == 0.0f, "negative palmMute did not clamp to 0");
    expect(clean.vibratoDepth == 1.0f,
           "vibratoDepth above 1 did not clamp to 1");

    // Fields with their own valid interval clamp to their own bounds rather
    // than [0,1]. bendTimeSeconds, strumSpreadSeconds and outputGain each
    // have an independent sanitizer branch instead of sharing the [0,1]
    // lambda, so - like outputGain below - each needs its own non-finite
    // fallback and both finite-boundary cases to actually protect its whole
    // branch, not just whichever half the shared "hostile" fixture happens
    // to hit.
    expect(clean.bendTimeSeconds == 0.04f,
           "negative bendTimeSeconds did not clamp to the 0.04 s floor");

    EngineParameters bendTimeNonFinite;
    bendTimeNonFinite.bendTimeSeconds = std::numeric_limits<float>::quiet_NaN();
    expect(TestAccess::sanitise(bendTimeNonFinite).bendTimeSeconds
               == defaults.bendTimeSeconds,
           "NaN bendTimeSeconds did not fall back to its default");
    EngineParameters bendTimeAboveRange;
    bendTimeAboveRange.bendTimeSeconds = 5.0f;
    expect(TestAccess::sanitise(bendTimeAboveRange).bendTimeSeconds == 2.0f,
           "bendTimeSeconds above 2 s did not clamp to its 2 s ceiling");

    // strumSpreadSeconds' own [0, 0.040] branch: the "hostile" fixture above
    // only supplies +inf, proving the fallback-to-default arm but not either
    // finite boundary.
    EngineParameters spreadBelowRange;
    spreadBelowRange.strumSpreadSeconds = -0.01f;
    expect(TestAccess::sanitise(spreadBelowRange).strumSpreadSeconds == 0.0f,
           "negative strumSpreadSeconds did not clamp to its 0 floor");
    EngineParameters spreadAboveRange;
    spreadAboveRange.strumSpreadSeconds = 0.2f;
    expect(TestAccess::sanitise(spreadAboveRange).strumSpreadSeconds == 0.040f,
           "strumSpreadSeconds above 0.040 did not clamp to its ceiling");

    EngineParameters tremoloBelowRange;
    tremoloBelowRange.tremoloRateHz = 1.0f;
    expect(TestAccess::sanitise(tremoloBelowRange).tremoloRateHz == 4.0f,
           "tremoloRateHz below 4 did not clamp to its floor");
    EngineParameters tremoloAboveRange;
    tremoloAboveRange.tremoloRateHz = 80.0f;
    expect(TestAccess::sanitise(tremoloAboveRange).tremoloRateHz == 20.0f,
           "tremoloRateHz above 20 did not clamp to its ceiling");

    // outputGain has its own [0, 2] branch rather than the shared [0, 1]
    // lambda, so it needs its own finite-out-of-range boundary cases: the
    // NaN case above only proves the fallback-to-default path, not this one.
    EngineParameters gainBelowRange;
    gainBelowRange.outputGain = -1.0f;
    expect(TestAccess::sanitise(gainBelowRange).outputGain == 0.0f,
           "negative outputGain did not clamp to its 0 floor");
    EngineParameters gainAboveRange;
    gainAboveRange.outputGain = 3.0f;
    expect(TestAccess::sanitise(gainAboveRange).outputGain == 2.0f,
           "outputGain above 2 did not clamp to its 2 ceiling");

    // An invalid enumerator falls back to the default enumerator rather than
    // surviving as an out-of-range integer. The "hostile" fixture above only
    // supplies enumerators above the valid range (999), so each enum also
    // gets a below-range (negative) case here to protect its other bound.
    expect(clean.pickupSelector == defaults.pickupSelector,
           "out-of-range pickupSelector did not fall back to its default");
    expect(clean.outputMode == defaults.outputMode,
           "out-of-range outputMode did not fall back to its default");

    EngineParameters negativeSelector;
    negativeSelector.pickupSelector = static_cast<PickupSelector>(-1);
    expect(TestAccess::sanitise(negativeSelector).pickupSelector
               == defaults.pickupSelector,
           "negative pickupSelector did not fall back to its default");
    EngineParameters negativeOutputMode;
    negativeOutputMode.outputMode = static_cast<electry::OutputMode>(-1);
    expect(TestAccess::sanitise(negativeOutputMode).outputMode
               == defaults.outputMode,
           "negative outputMode did not fall back to its default");

    // A parameter set already inside every field's valid range is a guard's
    // no-op, not a smoothing stage - it must come back unchanged. Every field
    // gets its own distinct in-range value so the guard cannot pass this
    // case by unconditionally resetting an untested field to its default.
    EngineParameters valid = defaults;
    valid.bodyWood = 0.62f;
    valid.bodySize = 0.73f;
    valid.bodyShape = 0.15f;
    valid.construction = 0.44f;
    valid.scaleLength = 0.55f;
    valid.pickupType = 0.66f;
    valid.toneKnob = 0.22f;
    valid.bodyResonance = 0.81f;
    valid.stringGauge = 0.33f;
    valid.stringAge = 0.77f;
    valid.pickPosition = 0.11f;
    valid.pickHardness = 0.88f;
    valid.pickNoise = 0.29f;
    valid.fingerNoise = 0.64f;
    valid.releaseNoise = 0.19f;
    valid.muteDamping = 0.41f;
    valid.bendTimeSeconds = 0.9f;
    valid.velocityAmount = 0.37f;
    valid.outputGain = 1.4f;
    valid.artifactAmount = 0.53f;
    valid.sympatheticAmount = 0.68f;
    valid.palmMute = 0.24f;
    valid.strumSpreadSeconds = 0.02f;
    valid.tremoloRateHz = 13.7f;
    valid.resonanceDepth = 0.71f;
    valid.vibratoDepth = 0.09f;
    valid.pickupSelector = PickupSelector::Neck;
    valid.outputMode = electry::OutputMode::Stereo;
    const EngineParameters passedThrough = TestAccess::sanitise(valid);
    expect(passedThrough.bodyWood == valid.bodyWood,
           "in-range bodyWood was altered by the guard");
    expect(passedThrough.bodySize == valid.bodySize,
           "in-range bodySize was altered by the guard");
    expect(passedThrough.bodyShape == valid.bodyShape,
           "in-range bodyShape was altered by the guard");
    expect(passedThrough.construction == valid.construction,
           "in-range construction was altered by the guard");
    expect(passedThrough.scaleLength == valid.scaleLength,
           "in-range scaleLength was altered by the guard");
    expect(passedThrough.pickupType == valid.pickupType,
           "in-range pickupType was altered by the guard");
    expect(passedThrough.toneKnob == valid.toneKnob,
           "in-range toneKnob was altered by the guard");
    expect(passedThrough.bodyResonance == valid.bodyResonance,
           "in-range bodyResonance was altered by the guard");
    expect(passedThrough.stringGauge == valid.stringGauge,
           "in-range stringGauge was altered by the guard");
    expect(passedThrough.stringAge == valid.stringAge,
           "in-range stringAge was altered by the guard");
    expect(passedThrough.pickPosition == valid.pickPosition,
           "in-range pickPosition was altered by the guard");
    expect(passedThrough.pickHardness == valid.pickHardness,
           "in-range pickHardness was altered by the guard");
    expect(passedThrough.pickNoise == valid.pickNoise,
           "in-range pickNoise was altered by the guard");
    expect(passedThrough.fingerNoise == valid.fingerNoise,
           "in-range fingerNoise was altered by the guard");
    expect(passedThrough.releaseNoise == valid.releaseNoise,
           "in-range releaseNoise was altered by the guard");
    expect(passedThrough.muteDamping == valid.muteDamping,
           "in-range muteDamping was altered by the guard");
    expect(passedThrough.bendTimeSeconds == valid.bendTimeSeconds,
           "in-range bendTimeSeconds was altered by the guard");
    expect(passedThrough.velocityAmount == valid.velocityAmount,
           "in-range velocityAmount was altered by the guard");
    expect(passedThrough.outputGain == valid.outputGain,
           "in-range outputGain was altered by the guard");
    expect(passedThrough.artifactAmount == valid.artifactAmount,
           "in-range artifactAmount was altered by the guard");
    expect(passedThrough.sympatheticAmount == valid.sympatheticAmount,
           "in-range sympatheticAmount was altered by the guard");
    expect(passedThrough.palmMute == valid.palmMute,
           "in-range palmMute was altered by the guard");
    expect(passedThrough.strumSpreadSeconds == valid.strumSpreadSeconds,
           "in-range strumSpreadSeconds was altered by the guard");
    expect(passedThrough.tremoloRateHz == valid.tremoloRateHz,
           "in-range tremoloRateHz was altered by the guard");
    expect(passedThrough.resonanceDepth == valid.resonanceDepth,
           "in-range resonanceDepth was altered by the guard");
    expect(passedThrough.vibratoDepth == valid.vibratoDepth,
           "in-range vibratoDepth was altered by the guard");
    expect(passedThrough.pickupSelector == valid.pickupSelector,
           "a valid pickupSelector was altered by the guard");
    expect(passedThrough.outputMode == valid.outputMode,
           "a valid outputMode was altered by the guard");
}

// The nominal acoustic delay is sample-rate-derived at every supported host
// rate, and the returned sample remains exactly that far behind arbitrary
// one-sample process/push partitions. This pins the FIFO mechanism separately
// from the processor-level 64/256/1024 scheduler regression.
void testAcousticReturnUsesFixedNominalDelay()
{
    for (const double rate : { 44100.0, 48000.0, 96000.0, 384000.0 })
    {
        ElectryEngine engine;
        engine.prepare(rate, 1024);
        const int delay = engine.getAcousticReturnDelaySamples();
        const int expected = static_cast<int>(std::lround(
            rate * (256.0 / 44100.0)));
        expect(delay == expected,
               "the acoustic-return delay did not preserve its nominal "
               "duration at "
                   + std::to_string(rate) + " Hz");

        std::array<float, 1> left {};
        std::array<float, 1> right {};
        engine.process(left.data(), right.data(), 1);
        const std::array<float, 1> impulse { 1.0f };
        engine.pushAcousticReturn(impulse.data(), impulse.data(), 1);
        expect(TestAccess::feedbackAvailable(engine) == delay,
               "the acoustic FIFO changed depth after its impulse push");

        const std::array<float, 1> silence { 0.0f };
        for (int offset = 1; offset < delay; ++offset)
        {
            engine.process(left.data(), right.data(), 1);
            expect(TestAccess::feedbackCurrent(engine) == 0.0f,
                   "the acoustic impulse returned before the fixed delay");
            engine.pushAcousticReturn(silence.data(), silence.data(), 1);
            expect(TestAccess::feedbackAvailable(engine) == delay,
                   "the acoustic FIFO changed depth across a process/push pair");
        }
        engine.process(left.data(), right.data(), 1);
        expect(TestAccess::feedbackCurrent(engine) == 1.0f,
               "the acoustic impulse did not return at the fixed delay");
    }
}

// pushAcousticReturn()'s own guard - a null left pointer or a non-positive
// sample count is a no-op, a null right pointer duplicates left rather than
// being read through, a non-finite averaged sample folds to zero before it
// is stored, and a single push longer than the ring drops its oldest samples
// rather than overflowing - was only ever driven through
// testParameterSanitisation(), which checked that the eventual rendered
// audio stayed finite but never looked at the ring itself, so a guard that
// silently stored a NaN, averaged through a stale right pointer, or wrapped
// a negative count into a huge write would still have passed. This drives
// the guard directly against the ring it actually fills.
void testPushAcousticReturnSanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    const int delay = engine.getAcousticReturnDelaySamples();
    expect(delay == static_cast<int>(std::lround(
                        sampleRate * (256.0 / 44100.0))),
           "the acoustic-return delay was not derived from its nominal duration");
    expect(TestAccess::feedbackAvailable(engine) == delay,
           "reset did not prime the acoustic-return FIFO with its fixed delay");
    for (int i = 0; i < delay; ++i)
        expect(TestAccess::feedbackRingSample(engine, i) == 0.0f,
               "the fixed acoustic-return lead-in was not silent");

    // A null right channel duplicates left rather than being read through:
    // the stored value is exactly the left sample, not half of a read
    // through whatever the null happened to alias to. Seeding the ring with
    // this push also gives the invalid-push probes below a populated ring to
    // run against.
    std::array<float, 8> ordinary {
        1.0f, -0.5f, 0.25f, 0.0f, 0.75f, -1.0f, 0.125f, -0.25f
    };
    engine.pushAcousticReturn(ordinary.data(), nullptr,
                              static_cast<int>(ordinary.size()));
    expect(TestAccess::feedbackAvailable(engine)
               == delay + static_cast<int>(ordinary.size()),
           "a mono push did not fill the ring one sample per input sample");
    for (int i = 0; i < static_cast<int>(ordinary.size()); ++i)
        expect(std::abs(TestAccess::feedbackRingSample(engine, delay + i)
                        - ordinary[static_cast<std::size_t>(i)]) < 1.0e-6f,
               "a mono acoustic-return push was not stored as the left "
               "channel verbatim");

    // A null left pointer and a non-positive count are both a no-op against
    // a *populated* ring: neither its availability nor its stored samples
    // move. Testing this against an already-empty ring would only prove
    // availability stays at zero, which a guard that discarded the ring
    // before checking its arguments would also satisfy.
    const int seededAvailable = TestAccess::feedbackAvailable(engine);
    std::vector<float> seededSamples(static_cast<std::size_t>(seededAvailable));
    for (int i = 0; i < seededAvailable; ++i)
        seededSamples[static_cast<std::size_t>(i)]
            = TestAccess::feedbackRingSample(engine, i);
    const auto expectRingUnchanged = [&] (const char* what)
    {
        expect(TestAccess::feedbackAvailable(engine) == seededAvailable,
               std::string(what) + " changed a populated acoustic-return "
                   "ring's availability");
        for (int i = 0; i < seededAvailable; ++i)
            expect(TestAccess::feedbackRingSample(engine, i)
                       == seededSamples[static_cast<std::size_t>(i)],
                   std::string(what) + " altered a populated acoustic-return "
                       "ring's stored samples");
    };

    // A valid right buffer here pins the guard to left == nullptr
    // specifically: a guard accidentally narrowed to
    // "left == nullptr && right == nullptr" would fall through and
    // dereference the null left pointer in the averaging loop instead of
    // leaving the ring untouched.
    std::array<float, 4> ignoredByNullLeft { 0.3f, -0.3f, 0.6f, -0.6f };
    engine.pushAcousticReturn(nullptr, ignoredByNullLeft.data(),
                              static_cast<int>(ignoredByNullLeft.size()));
    expectRingUnchanged("a null left pointer with a valid right buffer");

    std::array<float, 4> ignoredByNegativeCount { 9.0f, 9.0f, 9.0f, 9.0f };
    engine.pushAcousticReturn(ignoredByNegativeCount.data(), nullptr, -3);
    expectRingUnchanged("a negative sample count");

    // Zero is checked separately from negative: a guard narrowed from
    // numSamples <= 0 to numSamples < 0 would let a zero count fall through
    // to the stale-ring-clearing block and discard this populated ring
    // before the (no-op) write loop runs.
    std::array<float, 4> ignoredByZeroCount { 9.0f, 9.0f, 9.0f, 9.0f };
    engine.pushAcousticReturn(ignoredByZeroCount.data(), nullptr, 0);
    expectRingUnchanged("a zero sample count");

    // A non-finite averaged sample folds to zero rather than propagating,
    // whichever channel it came from.
    std::array<float, 4> poisonLeft {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 1.0f,
        std::numeric_limits<float>::quiet_NaN()
    };
    std::array<float, 4> poisonRight {
        0.0f, 0.0f, std::numeric_limits<float>::infinity(), 0.0f
    };
    engine.reset();
    engine.pushAcousticReturn(poisonLeft.data(), poisonRight.data(),
                              static_cast<int>(poisonLeft.size()));
    expect(TestAccess::feedbackAvailable(engine)
               == delay + static_cast<int>(poisonLeft.size()),
           "a hostile acoustic-return push did not fill the ring");
    for (int i = 0; i < static_cast<int>(poisonLeft.size()); ++i)
        expect(TestAccess::feedbackRingSample(engine, delay + i) == 0.0f,
               "a non-finite averaged acoustic-return sample was stored "
               "rather than folded to zero");

    // The fold-to-zero rule must hold for a mono push too: every hostile
    // push above supplies a non-null right buffer, so a handler that took a
    // separate path for null-right and stored left verbatim (skipping the
    // finite check) would pass all of them while still injecting NaN or
    // infinity into the ring here.
    std::array<float, 4> poisonMono {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 1.0f,
        -std::numeric_limits<float>::infinity()
    };
    engine.reset();
    engine.pushAcousticReturn(poisonMono.data(), nullptr,
                              static_cast<int>(poisonMono.size()));
    expect(TestAccess::feedbackAvailable(engine)
               == delay + static_cast<int>(poisonMono.size()),
           "a hostile mono acoustic-return push did not fill the ring");
    for (int i = 0; i < static_cast<int>(poisonMono.size()); ++i)
        expect(TestAccess::feedbackRingSample(engine, delay + i)
                   == (i == 2 ? 1.0f : 0.0f),
               "a non-finite mono acoustic-return sample was stored rather "
               "than folded to zero");

    // An ordinary stereo push is the average of both channels, not just one
    // of them.
    std::array<float, 3> left { 1.0f, -1.0f, 0.5f };
    std::array<float, 3> right { -0.2f, 0.6f, -0.5f };
    engine.reset();
    engine.pushAcousticReturn(left.data(), right.data(),
                              static_cast<int>(left.size()));
    for (int i = 0; i < static_cast<int>(left.size()); ++i)
    {
        const float expected = 0.5f * (left[static_cast<std::size_t>(i)]
                                       + right[static_cast<std::size_t>(i)]);
        expect(std::abs(TestAccess::feedbackRingSample(engine, delay + i)
                        - expected)
                   < 1.0e-6f,
               "a stereo acoustic-return push was not the average of both "
               "channels");
    }

    // A single push longer than the ring must cap availability at the
    // ring's capacity and drop its oldest samples rather than overflow the
    // write index past what the read side can ever see: the ring must end
    // up holding exactly the last `capacity` samples pushed, in order.
    const int capacity = TestAccess::feedbackRingCapacity();
    std::vector<float> ramp(static_cast<std::size_t>(capacity) + 50);
    for (std::size_t i = 0; i < ramp.size(); ++i)
        ramp[i] = static_cast<float>(i);
    engine.reset();
    engine.pushAcousticReturn(ramp.data(), ramp.data(),
                              static_cast<int>(ramp.size()));
    expect(TestAccess::feedbackAvailable(engine) == capacity,
           "a push longer than the ring did not cap availability at its "
           "capacity");
    // Every retained offset is checked, not just the first and last: a
    // wraparound bug that duplicates, drops or reorders one interior sample
    // would leave both endpoints correct while still corrupting the ring.
    int firstMismatchOffset = -1;
    for (int offset = 0; offset < capacity; ++offset)
    {
        if (TestAccess::feedbackRingSample(engine, offset)
                != ramp[static_cast<std::size_t>(offset) + 50])
        {
            firstMismatchOffset = offset;
            break;
        }
    }
    expect(firstMismatchOffset == -1,
           "a push longer than the ring dropped, duplicated or reordered an "
           "interior sample at offset "
               + std::to_string(firstMismatchOffset));

    // And held together on a genuinely fretted, sounding string, the guard
    // must still leave finite, bounded audio behind it end to end.
    engine.setResonance(1.0f);
    engine.setAcousticReturnLevel(1.0f);
    engine.noteOn(47, 0.9f); // A2 + 2 frets, not an open string
    StereoBuffer buffer(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, buffer);
    expect(allFinite(buffer),
           "a saturated acoustic-return ring produced non-finite audio");
    // allFinite() alone only rejects NaN/infinity, so a regression that
    // removed or weakened the downstream amplitude limiting could still
    // leak the ramp's own values (as large as capacity + 49) through as
    // an arbitrarily large but finite signal. An ordinary sustained note
    // here peaks around 0.13; 1.0 leaves ample headroom while still
    // catching that failure mode.
    expect(peakAbs(buffer.left) < 1.0f && peakAbs(buffer.right) < 1.0f,
           "a saturated acoustic-return ring produced unbounded audio");
}

// ---------------------------------------------------------------------------
// Version 1.1: bridge-coupled sympathetic strings
// ---------------------------------------------------------------------------

void testLiveDampingRefitsPreservePitch()
{
    static constexpr std::array<double, 5> sampleRates {
        44100.0, 48000.0, 96000.0, 192000.0, 384000.0
    };

    EngineParameters parameters;
    parameters.sympatheticAmount = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    const auto loopFrequencies = [] (const ElectryEngine& engine,
                                     int stringIndex)
    {
        std::array frequencies {
            TestAccess::effectiveLoopFrequency(engine, stringIndex),
            TestAccess::effectiveLoopFrequency(engine, stringIndex, true)
        };
#if ELECTRY_ENERGY_ATTACK_PITCH
        // This test isolates damping-filter phase translation. Remove the
        // separately validated natural attack relaxation from both axes so a
        // long CC2 sweep is compared against the written-pitch coordinate.
        const float factor = TestAccess::attackPitchState(engine, stringIndex)
                                 .frequencyFactor;
        frequencies[0] /= factor;
        frequencies[1] /= factor;
#endif
        return frequencies;
    };
    const auto maximumCentsBetween = [] (const std::array<float, 2>& first,
                                         const std::array<float, 2>& second)
    {
        const double vertical = std::abs(centsBetween(first[0], second[0]));
        const double horizontal = std::abs(centsBetween(first[1], second[1]));
        if (! std::isfinite(vertical) || ! std::isfinite(horizontal))
            return std::numeric_limits<double>::infinity();
        return std::max(vertical, horizontal);
    };
    struct RingingFixture
    {
        int stringIndex;
        int controlFrames;
    };
    const auto prepareRinging = [&] (ElectryEngine& engine, double sampleRate,
                                     int midiNote, PlayStyle style,
                                     const EngineParameters& setup)
    {
        engine.prepare(sampleRate, 512);
        engine.setParameters(setup);
        engine.reset();
        engine.noteOn(styleKeyswitch(style), 1.0f);
        engine.noteOn(midiNote, 0.95f);
        const int controlFrames =
            TestAccess::hostFramesPerControlPeriod(engine);
        const int establishFrames = controlFrames * static_cast<int>(
            std::ceil(0.080 * sampleRate
                      / static_cast<double>(controlFrames)));
        StereoBuffer establish(establishFrames);
        renderInto(engine, establish);
        return RingingFixture {
            TestAccess::stringForNote(engine, midiNote), controlFrames
        };
    };

    // The newest plectrum contact moves one bridge hand across every ringing
    // string. Re-solving an older string's damping filters must not move that
    // string's sounding period: the delay line and filter phase are one
    // coordinate, even though its attack descriptor stays unchanged.
    for (const double sampleRate : sampleRates)
    {
        for (const auto& [fromStyle, toStyle] : {
                 std::pair { PlayStyle::Sustain, PlayStyle::PalmMute },
                 std::pair { PlayStyle::PalmMute, PlayStyle::Sustain } })
        {
            ElectryEngine engine;
            const auto fixture = prepareRinging(
                engine, sampleRate, 28, fromStyle, parameters);
            const auto before = loopFrequencies(engine, fixture.stringIndex);
            engine.noteOn(styleKeyswitch(toStyle), 1.0f);
            engine.noteOn(40, 0.95f);
            const auto oldVoice = TestAccess::snapshot(
                engine, fixture.stringIndex);
            const int newString = TestAccess::stringForNote(engine, 40);
            const auto after = loopFrequencies(engine, fixture.stringIndex);
            const double pitchMove = maximumCentsBetween(after, before);

            expect(fixture.stringIndex >= 0 && newString >= 0
                       && newString != fixture.stringIndex
                       && oldVoice.midiNote == 28
                       && oldVoice.dampingStyle == toStyle,
                   "invalid live shared-hand pitch-refit fixture at "
                       + std::to_string(sampleRate) + " Hz");
            expect(pitchMove < 0.25,
                   "a shared-hand damping refit moved a ringing period by "
                       + std::to_string(pitchMove)
                       + " cents at " + std::to_string(sampleRate) + " Hz");
        }
    }

    // CC2 is continuous, so walk every MIDI value in both directions and
    // inspect every control boundary. This catches both a single phase jump
    // and small coordinate errors that accumulate over a pressure sweep.
    for (const double sampleRate : sampleRates)
    {
        ElectryEngine engine;
        const auto fixture = prepareRinging(
            engine, sampleRate, 28, PlayStyle::Sustain, parameters);
        const auto reference = loopFrequencies(engine, fixture.stringIndex);

        // Read the hard 0->127 throw after its first host sample, before a
        // whole control period can advance the 6 ms delay smoother even 3%.
        // Finish that period before returning to zero so the adjacent sweep
        // below remains boundary-aligned.
        engine.setPalmMutePressure(1.0f);
        StereoBuffer firstEventFrame(1);
        renderInto(engine, firstEventFrame, 1);
        const double hardMove = maximumCentsBetween(
            loopFrequencies(engine, fixture.stringIndex), reference);
        expect(hardMove < 0.25,
               "a full CC2 throw moved E1 by "
                   + std::to_string(hardMove)
                   + " cents at " + std::to_string(sampleRate) + " Hz");
        if (fixture.controlFrames > 1)
        {
            StereoBuffer eventRemainder(fixture.controlFrames - 1);
            renderInto(engine, eventRemainder, fixture.controlFrames - 1);
        }
        engine.setPalmMutePressure(0.0f);
        StereoBuffer returnTick(fixture.controlFrames);
        renderInto(engine, returnTick, fixture.controlFrames);

        auto previous = reference;
        double worstStep = 0.0;
        double worstOffset = 0.0;
        StereoBuffer controlTick(fixture.controlFrames);

        const auto applyPressure = [&] (int cc)
        {
            engine.setPalmMutePressure(static_cast<float>(cc) / 127.0f);
            renderInto(engine, controlTick, fixture.controlFrames);
            const auto current = loopFrequencies(engine, fixture.stringIndex);
            worstStep = std::max(
                worstStep, maximumCentsBetween(current, previous));
            worstOffset = std::max(
                worstOffset, maximumCentsBetween(current, reference));
            previous = current;
        };

        for (int cc = 1; cc <= 127; ++cc)
            applyPressure(cc);
        const int holdTicks = static_cast<int>(std::ceil(
            0.030 * sampleRate
            / static_cast<double>(fixture.controlFrames)));
        for (int tick = 0; tick < holdTicks; ++tick)
            applyPressure(127);
        for (int cc = 126; cc >= 0; --cc)
            applyPressure(cc);

        const auto final = loopFrequencies(engine, fixture.stringIndex);
        expect(worstStep < 0.25,
               "one CC2 damping step moved a ringing period by "
                   + std::to_string(worstStep) + " cents at "
                   + std::to_string(sampleRate) + " Hz");
        expect(worstOffset < 0.5,
               "a CC2 sweep accumulated "
                   + std::to_string(worstOffset) + " cents at "
                   + std::to_string(sampleRate) + " Hz");
        expect(maximumCentsBetween(final, reference) < 0.25,
               "returning CC2 to zero did not restore the ringing period at "
                   + std::to_string(sampleRate) + " Hz");
    }

    // The feedback close in demo 14 holds B2 when CC2 makes its full throw.
    // Pin that exposed mid-register case directly rather than relying on the
    // lower E1's longer period to represent every filter-phase proportion.
    for (const double sampleRate : sampleRates)
    {
        ElectryEngine engine;
        const auto fixture = prepareRinging(
            engine, sampleRate, 47, PlayStyle::Sustain, parameters);
        const auto before = loopFrequencies(engine, fixture.stringIndex);
        engine.setPalmMutePressure(1.0f);
        StereoBuffer firstEventFrame(1);
        renderInto(engine, firstEventFrame, 1);
        const double pitchMove = maximumCentsBetween(
            loopFrequencies(engine, fixture.stringIndex), before);
        expect(pitchMove < 0.25,
               "a full CC2 throw moved B2 by "
                   + std::to_string(pitchMove)
                   + " cents at " + std::to_string(sampleRate) + " Hz");
    }

    // Pressure can arrive while the pitch wheel is moving. Its filter phase
    // correction must stay separate from the genuine target-period motion:
    // a residual-to-new-target restore would make this trace jump ahead of an
    // otherwise identical wheel glide on every damping refit.
    {
        constexpr double sampleRate = 44100.0;
        EngineParameters glideParameters = parameters;
        glideParameters.bendTimeSeconds = 0.04f;
        ElectryEngine reference;
        ElectryEngine pressured;
        const auto referenceFixture = prepareRinging(
            reference, sampleRate, 28, PlayStyle::Sustain, glideParameters);
        const auto pressuredFixture = prepareRinging(
            pressured, sampleRate, 28, PlayStyle::Sustain, glideParameters);
        reference.setPitchBend(1.0f);
        pressured.setPitchBend(1.0f);
        pressured.setPalmMutePressure(1.0f);

        StereoBuffer referenceTick(referenceFixture.controlFrames);
        StereoBuffer pressuredTick(pressuredFixture.controlFrames);
        double worstDifference = 0.0;
        const int traceTicks = static_cast<int>(
            std::ceil(0.18 * sampleRate
                      / static_cast<double>(referenceFixture.controlFrames)));
        for (int tick = 0; tick < traceTicks; ++tick)
        {
            renderInto(reference, referenceTick,
                       referenceFixture.controlFrames);
            renderInto(pressured, pressuredTick,
                       pressuredFixture.controlFrames);
            worstDifference = std::max(
                worstDifference,
                maximumCentsBetween(
                    loopFrequencies(pressured,
                                    pressuredFixture.stringIndex),
                    loopFrequencies(reference,
                                    referenceFixture.stringIndex)));
        }
        const float finalVertical = loopFrequencies(
            pressured, pressuredFixture.stringIndex)[0];
        expect(worstDifference < 0.5,
               "CC2 changed a live wheel glide by "
                   + std::to_string(worstDifference) + " cents");
        expect(centsBetween(finalVertical, midiHz(28)) > 190.0,
               "the simultaneous pressure/wheel fixture did not reach its "
               "pitch target");
    }
}

void testSharedHandRetunesActiveStringDamping()
{
    static constexpr double sampleRate = 48000.0;
    EngineParameters parameters;
    parameters.sympatheticAmount = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    const auto prepare = [&] (ElectryEngine& engine)
    {
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
    };

    auto openReference = std::make_unique<ElectryEngine>();
    prepare(*openReference);
    openReference->noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    openReference->noteOn(28, 0.95f);
    const int openReferenceString = TestAccess::stringForNote(*openReference, 28);
    const auto openTarget = TestAccess::snapshot(*openReference,
                                                 openReferenceString);
    const float openDepth = TestAccess::handLossDepth(*openReference,
                                                      openReferenceString);
    const float openSolvedDepth = TestAccess::solvedHandLossDepth(
        *openReference, openReferenceString);

    openReference.reset();
#if ! ELECTRY_ENERGY_ATTACK_PITCH
    auto palmReference = std::make_unique<ElectryEngine>();
    prepare(*palmReference);
    palmReference->noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    palmReference->noteOn(28, 0.95f);
    const int palmReferenceString = TestAccess::stringForNote(*palmReference, 28);
    const auto palmTarget = TestAccess::snapshot(*palmReference,
                                                 palmReferenceString);
    const float palmDepth = TestAccess::handLossDepth(*palmReference,
                                                      palmReferenceString);
    const float palmSolvedDepth = TestAccess::solvedHandLossDepth(
        *palmReference, palmReferenceString);
    palmReference.reset();
#endif

    // The most recent contact is one physical hand across the guitar. Lifting
    // it for an open E2 must reopen the already-ringing Palm E1 loop without
    // rewriting that E1 attack's articulation descriptor.
    auto palmToOpen = std::make_unique<ElectryEngine>();
    prepare(*palmToOpen);
    palmToOpen->noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    palmToOpen->noteOn(28, 0.95f);
    StereoBuffer palmBody(static_cast<int>(0.080 * sampleRate));
    renderInto(*palmToOpen, palmBody);
    const int oldPalmString = TestAccess::stringForNote(*palmToOpen, 28);
    palmToOpen->noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    palmToOpen->noteOn(40, 0.95f);
    const auto openedPalm = TestAccess::snapshot(*palmToOpen, oldPalmString);
    expect(openedPalm.playStyle == PlayStyle::PalmMute,
           "an open contact rewrote the older Palm attack style");
    expect(openedPalm.loopGain == openTarget.loopGain
               && openedPalm.loopDampingCoefficient
                      == openTarget.loopDampingCoefficient
               && TestAccess::handLossDepth(*palmToOpen, oldPalmString)
                      == openDepth
               && TestAccess::solvedHandLossDepth(*palmToOpen, oldPalmString)
                      == openSolvedDepth,
           "an open contact did not move the older Palm loop to the exact "
           "open-string damping target");

    // Planting the hand for Palm E2 must do the inverse to an older open E1.
    // Both E1 fixtures are the first identical stroke in their engines, so the
    // palm target also checks that the older note keeps its own contact-force
    // draw rather than borrowing E2's.
    auto openToPalm = std::make_unique<ElectryEngine>();
    prepare(*openToPalm);
    openToPalm->noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    openToPalm->noteOn(28, 0.95f);
    StereoBuffer openBody(static_cast<int>(0.080 * sampleRate));
    renderInto(*openToPalm, openBody);
    const int oldOpenString = TestAccess::stringForNote(*openToPalm, 28);
    openToPalm->noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    openToPalm->noteOn(40, 0.95f);
    const auto mutedOpen = TestAccess::snapshot(*openToPalm, oldOpenString);
#if ELECTRY_ENERGY_ATTACK_PITCH
    // The optional measured attack-tension glide makes this older ringing E1
    // slightly sharp. Its exact Palm target therefore belongs to its live f0,
    // not to the just-created, still-unpitched reference above.
    auto livePalmReference = std::make_unique<ElectryEngine>(*openToPalm);
    TestAccess::refitVoiceDampingAtCachedCoordinate(
        *livePalmReference, oldOpenString, PlayStyle::PalmMute);
    const auto expectedPalmTarget = TestAccess::snapshot(
        *livePalmReference, oldOpenString);
    const float expectedPalmDepth = TestAccess::handLossDepth(
        *livePalmReference, oldOpenString);
    const float expectedPalmSolvedDepth = TestAccess::solvedHandLossDepth(
        *livePalmReference, oldOpenString);
#else
    const auto& expectedPalmTarget = palmTarget;
    const float expectedPalmDepth = palmDepth;
    const float expectedPalmSolvedDepth = palmSolvedDepth;
#endif
    expect(mutedOpen.playStyle == PlayStyle::Sustain,
           "a Palm contact rewrote the older open attack style");
    expect(mutedOpen.loopGain == expectedPalmTarget.loopGain
               && mutedOpen.loopDampingCoefficient
                      == expectedPalmTarget.loopDampingCoefficient
               && TestAccess::handLossDepth(*openToPalm, oldOpenString)
                      == expectedPalmDepth
               && TestAccess::solvedHandLossDepth(*openToPalm, oldOpenString)
                      == expectedPalmSolvedDepth,
           "a Palm contact did not move the older open loop to the exact "
           "Palm damping target");

    // Hammer-ons, pull-offs and legato slides are fretting-hand gestures. They
    // can change one speaking length, but they cannot teleport the picking
    // hand off the bridge or reopen any other palm-muted string.
    struct LegatoGesture
    {
        PlayStyle style;
        int fromNote;
        int toNote;
        const char* label;
    };
    for (const auto gesture : {
             LegatoGesture { PlayStyle::Hammer, 28, 31, "hammer-on" },
             LegatoGesture { PlayStyle::Hammer, 31, 28, "pull-off" },
             LegatoGesture { PlayStyle::Slide, 28, 33, "slide" } })
    {
        auto legato = std::make_unique<ElectryEngine>();
        prepare(*legato);
        legato->noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
        const std::array<ElectryEngine::NoteOnEvent, 2> chord {{
            { gesture.fromNote, 0.92f }, { 40, 0.88f }
        }};
        legato->noteOnChord(chord);
        StereoBuffer establishPalm(static_cast<int>(0.080 * sampleRate));
        renderInto(*legato, establishPalm);

        const int movedString = TestAccess::stringForNote(
            *legato, gesture.fromNote);
        const int siblingString = TestAccess::stringForNote(*legato, 40);
        const float movedPitchBefore = TestAccess::effectiveLoopFrequency(
            *legato, movedString);
        const auto siblingBefore = TestAccess::snapshot(*legato, siblingString);
        const float siblingDepthBefore = TestAccess::handLossDepth(
            *legato, siblingString);
        const float siblingSolvedBefore = TestAccess::solvedHandLossDepth(
            *legato, siblingString);
        const auto handClockBefore = TestAccess::lastHandContactClock(*legato);
        const auto handOrderBefore = TestAccess::lastHandContactOrder(*legato);
        expect(movedString >= 0 && siblingString >= 0
                   && movedString != siblingString
                   && siblingDepthBefore > 0.0f
                   && siblingSolvedBefore > 0.0f,
               std::string("invalid Palm-to-") + gesture.label + " fixture");

        legato->noteOn(styleKeyswitch(gesture.style), 1.0f);
        legato->noteOn(gesture.toNote, 0.82f);
        const auto moved = TestAccess::snapshot(*legato, movedString);
        const auto sibling = TestAccess::snapshot(*legato, siblingString);
        const float movedPitchAfter = TestAccess::effectiveLoopFrequency(
            *legato, movedString);
        expect(moved.midiNote == gesture.toNote
                   && moved.playStyle == gesture.style
                   && moved.dampingStyle == PlayStyle::PalmMute
                   && TestAccess::handLossDepth(*legato, movedString) > 0.0f
                   && TestAccess::solvedHandLossDepth(*legato, movedString)
                          > 0.0f,
               std::string("a ") + gesture.label
                   + " lifted Palm damping from its target string");
        expect(std::abs(centsBetween(movedPitchAfter, movedPitchBefore)) < 2.0,
               std::string("a Palm-held ") + gesture.label
                   + " changed effective pitch at its retarget");
        expect(sibling.playStyle == siblingBefore.playStyle
                   && sibling.dampingStyle == siblingBefore.dampingStyle
                   && sibling.loopGain == siblingBefore.loopGain
                   && sibling.loopDampingCoefficient
                          == siblingBefore.loopDampingCoefficient
                   && TestAccess::handLossDepth(*legato, siblingString)
                          == siblingDepthBefore
                   && TestAccess::solvedHandLossDepth(*legato, siblingString)
                          == siblingSolvedBefore,
               std::string("a ") + gesture.label
                   + " moved the shared Palm hand off a sibling string");
        expect(TestAccess::lastHandContactClock(*legato) == handClockBefore
                   && TestAccess::lastHandContactOrder(*legato)
                          == handOrderBefore
                   && TestAccess::lastHandContactPlayStyle(*legato)
                          == PlayStyle::PalmMute,
               std::string("a ") + gesture.label
                   + " claimed picking-hand contact ownership");

        legato->noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        legato->noteOn(45, 0.90f);
        expect(TestAccess::snapshot(*legato, movedString).dampingStyle
                   == PlayStyle::Sustain
                   && TestAccess::snapshot(*legato, siblingString).dampingStyle
                          == PlayStyle::Sustain,
               std::string("a real pick did not reopen strings after the ")
                   + gesture.label);
    }

    // Palm is the one bridge-hand state a fretting gesture must retain. An
    // open hand stays open, while pressing a Dead string replaces that local
    // whole-fretting-hand choke just as it did before this correction.
    for (const auto initialStyle : { PlayStyle::Sustain, PlayStyle::Dead })
    {
        auto control = std::make_unique<ElectryEngine>();
        prepare(*control);
        control->noteOn(styleKeyswitch(initialStyle), 1.0f);
        constexpr std::array<ElectryEngine::NoteOnEvent, 2> chord {{
            { 28, 0.92f }, { 40, 0.88f }
        }};
        control->noteOnChord(chord);
        StereoBuffer establish(static_cast<int>(0.080 * sampleRate));
        renderInto(*control, establish);
        const int targetString = TestAccess::stringForNote(*control, 28);
        const int siblingString = TestAccess::stringForNote(*control, 40);
        control->noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
        control->noteOn(31, 0.82f);
        const auto target = TestAccess::snapshot(*control, targetString);
        const auto sibling = TestAccess::snapshot(*control, siblingString);
        expect(target.playStyle == PlayStyle::Hammer
                   && target.dampingStyle == PlayStyle::Hammer
                   && TestAccess::handLossDepth(*control, targetString) == 0.0f
                   && TestAccess::lastHandContactPlayStyle(*control)
                          == PlayStyle::Hammer,
               "Palm retention leaked into an open/Dead hammer target");
        expect(sibling.dampingStyle
                   == (initialStyle == PlayStyle::Dead
                           ? PlayStyle::Hammer : PlayStyle::Sustain),
               "an open/Dead hammer changed its established sibling semantics");
    }

    // The coefficient checks above pin the mechanism; this paired render pins
    // what it does to the old string. The new E2 is silenced immediately after
    // contact, leaving only E1 in the measurement. A keyswitch-only twin proves
    // that scheduling a style is not itself a physical hand movement.
    struct TransitionAudio
    {
        double changedHighPower { 0.0 };
        double controlHighPower { 0.0 };
        double changedHighShare { 0.0 };
        double controlHighShare { 0.0 };
        float changedStep { 0.0f };
        float controlStep { 0.0f };
    };
    const auto measureTransition = [&] (PlayStyle initialStyle,
                                        PlayStyle contactStyle)
    {
        auto changed = std::make_unique<ElectryEngine>();
        auto control = std::make_unique<ElectryEngine>();
        prepare(*changed);
        prepare(*control);
        for (auto* engine : { changed.get(), control.get() })
        {
            engine->noteOn(styleKeyswitch(initialStyle), 1.0f);
            engine->noteOn(28, 0.95f);
        }

        constexpr int bodySamples = static_cast<int>(0.080 * sampleRate);
        StereoBuffer changedBody(bodySamples);
        StereoBuffer controlBody(bodySamples);
        renderInto(*changed, changedBody);
        renderInto(*control, controlBody);
        expect(changedBody.left == controlBody.left
                   && changedBody.right == controlBody.right,
               "hand-transition audio fixtures diverged before contact");

        changed->noteOn(styleKeyswitch(contactStyle), 1.0f);
        control->noteOn(styleKeyswitch(contactStyle), 1.0f);
        changed->noteOn(40, 0.95f);
        const int contactedString = TestAccess::stringForNote(*changed, 40);
        expect(contactedString >= 0,
               "hand-transition audio fixture did not allocate E2");
        if (contactedString >= 0)
            TestAccess::silenceVoice(*changed, contactedString);

        constexpr int tailSamples = static_cast<int>(0.080 * sampleRate);
        StereoBuffer changedTail(tailSamples);
        StereoBuffer controlTail(tailSamples);
        renderInto(*changed, changedTail);
        renderInto(*control, controlTail);
        expect(allFinite(changedTail) && allFinite(controlTail)
                   && peakAbs(changedTail.left) < 1.0f
                   && peakAbs(controlTail.left) < 1.0f,
               "a shared-hand damping transition produced invalid audio");

        const auto maximumStep = [] (const std::vector<float>& body,
                                     const std::vector<float>& tail)
        {
            const int end = std::min<int>(
                static_cast<int>(tail.size()),
                static_cast<int>(0.030 * sampleRate));
            float largest = body.empty() || tail.empty()
                ? 0.0f : std::abs(tail.front() - body.back());
            for (int sample = 1; sample < end; ++sample)
                largest = std::max(
                    largest,
                    std::abs(tail[static_cast<std::size_t>(sample)]
                             - tail[static_cast<std::size_t>(sample - 1)]));
            return largest;
        };

        const auto spectrum = [] (const std::vector<float>& tail)
        {
            const int first = static_cast<int>(0.030 * sampleRate);
            const int length = std::min<int>(
                static_cast<int>(0.050 * sampleRate),
                static_cast<int>(tail.size()) - first);
            std::vector<float> centred(static_cast<std::size_t>(length));
            double mean = 0.0;
            for (int sample = 0; sample < length; ++sample)
                mean += tail[static_cast<std::size_t>(first + sample)];
            mean /= static_cast<double>(std::max(length, 1));
            for (int sample = 0; sample < length; ++sample)
                centred[static_cast<std::size_t>(sample)] = static_cast<float>(
                    tail[static_cast<std::size_t>(first + sample)] - mean);

            const auto bandPower = [&] (double lowHz, double highHz)
            {
                double power = 0.0;
                for (double frequency = lowHz; frequency <= highHz;
                     frequency *= 1.03)
                {
                    const double magnitude = dftMagnitude(
                        centred, 0, length, sampleRate, frequency);
                    power += magnitude * magnitude;
                }
                return power;
            };
            const double total = bandPower(20.0, 8000.0);
            const double high = bandPower(500.0, 8000.0);
            return std::pair { high, high / std::max(total, 1.0e-30) };
        };

        const auto changedSpectrum = spectrum(changedTail.left);
        const auto controlSpectrum = spectrum(controlTail.left);
        return TransitionAudio {
            changedSpectrum.first, controlSpectrum.first,
            changedSpectrum.second, controlSpectrum.second,
            maximumStep(changedBody.left, changedTail.left),
            maximumStep(controlBody.left, controlTail.left)
        };
    };

    const auto palmOpening = measureTransition(PlayStyle::PalmMute,
                                               PlayStyle::Sustain);
    expect(palmOpening.changedHighPower > palmOpening.controlHighPower
               && palmOpening.changedHighShare > palmOpening.controlHighShare,
           "lifting the Palm hand did not brighten the already-ringing E1");
    expect(palmOpening.changedStep
               <= 1.5f * palmOpening.controlStep + 0.005f,
           "lifting the Palm hand clicked at the damping transition");

    const auto openMuting = measureTransition(PlayStyle::Sustain,
                                              PlayStyle::PalmMute);
    expect(openMuting.changedHighPower < openMuting.controlHighPower
               && openMuting.changedHighShare < openMuting.controlHighShare,
           "planting the Palm hand did not darken the already-ringing E1");
    expect(openMuting.changedStep <= 1.5f * openMuting.controlStep + 0.005f,
           "planting the Palm hand clicked at the damping transition");
    std::cout << "PROBE old-E1 shared-hand transition: Palm->Open >500 Hz "
              << 10.0 * std::log10(palmOpening.changedHighPower
                                   / palmOpening.controlHighPower)
              << " dB, share "
              << 100.0 * (palmOpening.changedHighShare
                          - palmOpening.controlHighShare)
              << " points; Open->Palm >500 Hz "
              << 10.0 * std::log10(openMuting.changedHighPower
                                   / openMuting.controlHighPower)
              << " dB, share "
              << 100.0 * (openMuting.changedHighShare
                          - openMuting.controlHighShare)
              << " points; contact/control max steps "
              << palmOpening.changedStep << "/" << palmOpening.controlStep
              << " and " << openMuting.changedStep << "/"
              << openMuting.controlStep << '\n';
    palmToOpen.reset();
    openToPalm.reset();

    // Repeating the same hand position is not a new damping state. In
    // particular, it must not reset an older Palm loop's already-relaxed loss
    // depth merely because another muted string was picked.
    auto samePalm = std::make_unique<ElectryEngine>();
    auto palmTwin = std::make_unique<ElectryEngine>();
    prepare(*samePalm);
    prepare(*palmTwin);
    for (auto* engine : { samePalm.get(), palmTwin.get() })
    {
        engine->noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
        engine->noteOn(28, 0.95f);
    }
    StereoBuffer samePalmBody(static_cast<int>(0.080 * sampleRate));
    StereoBuffer palmTwinBody(static_cast<int>(0.080 * sampleRate));
    renderInto(*samePalm, samePalmBody);
    renderInto(*palmTwin, palmTwinBody);
    const int samePalmOldString = TestAccess::stringForNote(*samePalm, 28);
    const auto beforeSamePalm = TestAccess::snapshot(*samePalm,
                                                     samePalmOldString);
    const float beforeSamePalmDepth = TestAccess::handLossDepth(
        *samePalm, samePalmOldString);
    samePalm->noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    palmTwin->noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    samePalm->noteOn(40, 0.95f);
    const int samePalmNewString = TestAccess::stringForNote(*samePalm, 40);
    const auto afterSamePalm = TestAccess::snapshot(*samePalm,
                                                    samePalmOldString);
    expect(afterSamePalm.loopGain == beforeSamePalm.loopGain
               && afterSamePalm.loopDampingCoefficient
                      == beforeSamePalm.loopDampingCoefficient
               && TestAccess::handLossDepth(*samePalm, samePalmOldString)
                      == beforeSamePalmDepth,
           "a Palm-to-Palm contact reset the older Palm loop damping");
    TestAccess::silenceVoice(*samePalm, samePalmNewString);
    StereoBuffer samePalmTail(512);
    StereoBuffer palmTwinTail(512);
    renderInto(*samePalm, samePalmTail);
    renderInto(*palmTwin, palmTwinTail);
    expect(samePalmTail.left == palmTwinTail.left
               && samePalmTail.right == palmTwinTail.right,
           "a same-style Palm contact changed the older string's audio");
    samePalm.reset();
    palmTwin.reset();

    // Scheduling lookahead is not contact. Until the delayed Palm E2 arrives,
    // the old open E1 and its rendered samples must remain exactly identical to
    // a twin that received the keyswitch but no future note.
    parameters.strumSpreadSeconds = 0.020f;
    auto scheduled = std::make_unique<ElectryEngine>();
    auto unstaged = std::make_unique<ElectryEngine>();
    prepare(*scheduled);
    prepare(*unstaged);
    for (auto* engine : { scheduled.get(), unstaged.get() })
    {
        engine->noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        engine->noteOn(28, 0.95f);
    }
    StereoBuffer scheduledBody(static_cast<int>(0.080 * sampleRate));
    StereoBuffer unstagedBody(static_cast<int>(0.080 * sampleRate));
    renderInto(*scheduled, scheduledBody);
    renderInto(*unstaged, unstagedBody);
    expect(scheduledBody.left == unstagedBody.left
               && scheduledBody.right == unstagedBody.right,
           "future-contact fixture diverged before the future note was staged");
    const int scheduledOldString = TestAccess::stringForNote(*scheduled, 28);
    const auto beforeFuture = TestAccess::snapshot(*scheduled,
                                                   scheduledOldString);
    const float beforeFutureDepth = TestAccess::handLossDepth(
        *scheduled, scheduledOldString);
    scheduled->noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    unstaged->noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    scheduled->noteOn(40, 0.95f);
    const int futureString = TestAccess::stringForNote(*scheduled, 40);
    const int futureDelay = TestAccess::snapshot(*scheduled, futureString)
                                .startDelaySamples;
    const auto afterFuture = TestAccess::snapshot(*scheduled,
                                                  scheduledOldString);
    expect(futureString >= 0 && futureDelay > 0,
           "future-contact damping fixture did not enter pre-roll");
    expect(afterFuture.loopGain == beforeFuture.loopGain
               && afterFuture.loopDampingCoefficient
                      == beforeFuture.loopDampingCoefficient
               && TestAccess::handLossDepth(*scheduled, scheduledOldString)
                      == beforeFutureDepth,
           "a future Palm descriptor changed an older loop before contact");
    const int internalPerHost = static_cast<int>(std::lround(
        TestAccess::internalSampleRate(*scheduled) / sampleRate));
    const int preContactSamples = std::max(
        1, (futureDelay - 1) / std::max(internalPerHost, 1));
    StereoBuffer scheduledPreContact(preContactSamples);
    StereoBuffer unstagedPreContact(preContactSamples);
    renderInto(*scheduled, scheduledPreContact);
    renderInto(*unstaged, unstagedPreContact);
    expect(scheduledPreContact.left == unstagedPreContact.left
               && scheduledPreContact.right == unstagedPreContact.right,
           "a future Palm descriptor changed audio before physical contact");
    expect(TestAccess::snapshot(*scheduled, futureString).startDelaySamples > 0
               && TestAccess::snapshot(*scheduled, scheduledOldString).loopGain
                      == beforeFuture.loopGain,
           "the old open loop changed before the delayed Palm contact");
    StereoBuffer throughFutureContact(1);
    renderInto(*scheduled, throughFutureContact);
    const auto oldAfterContact = TestAccess::snapshot(*scheduled,
                                                       scheduledOldString);
    const auto futureAfterContact = TestAccess::snapshot(*scheduled,
                                                          futureString);
#if ELECTRY_ENERGY_ATTACK_PITCH
    auto delayedPalmReference = std::make_unique<ElectryEngine>(*scheduled);
    TestAccess::refitVoiceDampingAtCachedCoordinate(
        *delayedPalmReference, scheduledOldString, PlayStyle::PalmMute);
    const auto expectedDelayedPalm = TestAccess::snapshot(
        *delayedPalmReference, scheduledOldString);
    const float expectedDelayedPalmSolvedDepth =
        TestAccess::solvedHandLossDepth(*delayedPalmReference,
                                        scheduledOldString);
#else
    const auto& expectedDelayedPalm = palmTarget;
    const float expectedDelayedPalmSolvedDepth = palmSolvedDepth;
#endif
    expect(futureAfterContact.startDelaySamples == 0
               && futureAfterContact.dampingStyle == PlayStyle::PalmMute,
           "the delayed Palm voice did not restore its own damping at contact");
    expect(oldAfterContact.playStyle == PlayStyle::Sustain
               && oldAfterContact.dampingStyle == PlayStyle::PalmMute
               && oldAfterContact.loopGain == expectedDelayedPalm.loopGain
               && oldAfterContact.loopDampingCoefficient
                      == expectedDelayedPalm.loopDampingCoefficient
               && TestAccess::solvedHandLossDepth(*scheduled,
                                                   scheduledOldString)
                      == expectedDelayedPalmSolvedDepth,
           "the delayed Palm contact did not retune the old open loop at its "
           "physical contact");

    // A delayed fret retarget is the harder lookahead case: the future style
    // descriptor and the preceding ring occupy the same physical string. Its
    // damping must match an open-style twin until contact, then install the
    // scheduled Palm damping when the countdown reaches zero.
    auto palmRetarget = std::make_unique<ElectryEngine>();
    auto openRetarget = std::make_unique<ElectryEngine>();
    prepare(*palmRetarget);
    prepare(*openRetarget);
    for (auto* engine : { palmRetarget.get(), openRetarget.get() })
    {
        engine->noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        engine->noteOn(28, 0.95f);
    }
    StereoBuffer palmRetargetBody(static_cast<int>(0.080 * sampleRate));
    StereoBuffer openRetargetBody(static_cast<int>(0.080 * sampleRate));
    renderInto(*palmRetarget, palmRetargetBody);
    renderInto(*openRetarget, openRetargetBody);
    palmRetarget->noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    openRetarget->noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    palmRetarget->noteOn(29, 0.95f);
    openRetarget->noteOn(29, 0.95f);
    const int palmRetargetString = TestAccess::stringForNote(*palmRetarget, 29);
    const int openRetargetString = TestAccess::stringForNote(*openRetarget, 29);
    const auto palmPending = TestAccess::snapshot(*palmRetarget,
                                                  palmRetargetString);
    const auto openPending = TestAccess::snapshot(*openRetarget,
                                                  openRetargetString);
    expect(palmRetargetString == openRetargetString
               && palmPending.startDelaySamples > 0
               && palmPending.playStyle == PlayStyle::PalmMute
               && palmPending.dampingStyle == PlayStyle::Sustain
               && palmPending.loopGain == openPending.loopGain
               && palmPending.loopDampingCoefficient
                      == openPending.loopDampingCoefficient,
           "a future Palm fret retarget rewrote its preceding open ring");
    const int retargetPreContactSamples = std::max(
        1, (palmPending.startDelaySamples - 1)
               / std::max(internalPerHost, 1));
    StereoBuffer palmRetargetPreContact(retargetPreContactSamples);
    StereoBuffer openRetargetPreContact(retargetPreContactSamples);
    renderInto(*palmRetarget, palmRetargetPreContact);
    renderInto(*openRetarget, openRetargetPreContact);
    const auto palmStillPending = TestAccess::snapshot(*palmRetarget,
                                                        palmRetargetString);
    const auto openStillPending = TestAccess::snapshot(*openRetarget,
                                                        openRetargetString);
    expect(palmStillPending.startDelaySamples > 0
               && palmStillPending.dampingStyle == PlayStyle::Sustain
               && palmStillPending.loopGain == openStillPending.loopGain
               && palmStillPending.loopDampingCoefficient
                      == openStillPending.loopDampingCoefficient,
           "a future Palm fret retarget changed damping before contact");
    StereoBuffer palmRetargetContact(1);
    StereoBuffer openRetargetContact(1);
    renderInto(*palmRetarget, palmRetargetContact);
    renderInto(*openRetarget, openRetargetContact);
    expect(TestAccess::snapshot(*palmRetarget, palmRetargetString)
                   .startDelaySamples == 0
               && TestAccess::snapshot(*palmRetarget, palmRetargetString)
                      .dampingStyle == PlayStyle::PalmMute
               && TestAccess::snapshot(*openRetarget, openRetargetString)
                      .dampingStyle == PlayStyle::Sustain,
           "a delayed fret retarget did not install its own damping at contact");
}

void testSympatheticBridgeCoupling()
{
    constexpr double sampleRate = 48000.0;

    // Demo 08 strikes one chord twice before comparing it with exact bypass,
    // then starts its final single-note excitation only after that comparison
    // has stopped. Keep its overlapping owners and three physical boundaries
    // explicit so the advertised sections cannot bleed into one another.
    {
        constexpr double scoreSampleRate = 44100.0;
        static constexpr std::array<int, 6> notes {{
            28, 35, 40, 47, 52, 56
        }};

        ElectryEngine score;
        score.prepare(scoreSampleRate, 256);
        EngineParameters scoreParameters;
        scoreParameters.pickupSelector = electry::PickupSelector::Both;
        scoreParameters.outputMode = electry::OutputMode::Stereo;
        scoreParameters.strumSpreadSeconds = 0.022f;
        scoreParameters.sympatheticAmount = 0.85f;
        scoreParameters.bodyResonance = 0.55f;
        scoreParameters.stringAge = 0.05f;
        scoreParameters.toneKnob = 1.0f;
        score.setParameters(scoreParameters);
        score.reset();

        const auto waitSeconds = [&] (double seconds)
        {
            StereoBuffer buffer(static_cast<int>(seconds * scoreSampleRate));
            renderInto(score, buffer);
        };
        const auto releaseChord = [&]
        {
            for (const int note : notes)
                score.noteOff(note);
        };
        const auto strikeChord = [&] (float velocity)
        {
            std::array<ElectryEngine::NoteOnEvent, notes.size()> events;
            for (std::size_t index = 0; index < notes.size(); ++index)
                events[index] = { notes[index], velocity };
            score.noteOnChord(events);
        };
        const auto visualState = [&]
        {
            std::array<electry::StringVisualState,
                       ElectryEngine::stringCount> states;
            score.getStringVisualState(states);
            return states;
        };

        waitSeconds(0.25);
        score.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        strikeChord(0.90f);
        waitSeconds(1.30);
        score.noteOn(pickKeyswitch(PickStyle::Up), 1.0f);
        strikeChord(0.75f);
        waitSeconds(1.30);

        std::array<int, notes.size()> strings;
        for (std::size_t index = 0; index < notes.size(); ++index)
            strings[index] = TestAccess::stringForNote(score, notes[index]);
        releaseChord();
        expect(std::all_of(strings.begin(), strings.end(), [&] (int stringIndex)
                   { return TestAccess::snapshot(score, stringIndex).keyDown; }),
               "demo-08 first release did not preserve its overlapping owners");
        releaseChord();
        expect(std::all_of(strings.begin(), strings.end(), [&] (int stringIndex)
                   { return ! TestAccess::snapshot(score, stringIndex).keyDown; }),
               "demo-08 second release left a repeated-chord owner held");
        waitSeconds(0.35);

        scoreParameters.sympatheticAmount = 0.0f;
        score.setParameters(scoreParameters);
        waitSeconds(0.20);
        const auto bypassEntrance = visualState();
        expect(score.getActiveVoiceCount() == 0
                   && score.getSympatheticStringCount() == 0
                   && std::none_of(bypassEntrance.begin(), bypassEntrance.end(),
                                   [] (const auto& state) { return state.sounding; }),
               "demo-08 bypass comparison began before the first chord cleared");
        for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount;
             ++stringIndex)
            expect(! TestAccess::snapshot(score, stringIndex).sympatheticReady,
                   "demo-08 bypass comparison retained a sympathetic loop");

        score.noteOn(pickKeyswitch(PickStyle::Down), 1.0f);
        strikeChord(0.90f);
        waitSeconds(1.20);
        releaseChord();
        waitSeconds(0.40);
        const auto finalGap = visualState();
        expect(score.getActiveVoiceCount() == 0
                   && std::none_of(finalGap.begin(), finalGap.end(),
                                   [] (const auto& state) { return state.sounding; }),
               "demo-08 final note began over the bypassed chord's release tails");

        scoreParameters.sympatheticAmount = 0.95f;
        score.setParameters(scoreParameters);
        waitSeconds(0.15);
        const auto finalReady = visualState();
        expect(score.getActiveVoiceCount() == 0
                   && score.getSympatheticStringCount() == 0
                   && std::none_of(finalReady.begin(), finalReady.end(),
                                   [] (const auto& state) { return state.sounding; }),
               "demo-08 final single-note section inherited a sounding string");
        const std::array<ElectryEngine::NoteOnEvent, 1> finalNote {{
            { 28, 1.0f }
        }};
        score.noteOnChord(finalNote);
        const auto finalEntrance = visualState();
        expect(std::count_if(finalEntrance.begin(), finalEntrance.end(),
                             [] (const auto& state) { return state.sounding; }) == 1,
               "demo-08 final entrance was not one physically held string");
        score.noteOff(28);
    }

    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    // A2 is picked on its own open string; its third partial lands almost
    // exactly on the open high E, which is the string a real guitar rings
    // hardest through the bridge.
    constexpr double openHighE = 329.62756;
    const auto tailEnergyAt = [&] (float amount)
    {
        parameters.sympatheticAmount = amount;
        engine.setParameters(parameters);
        const auto buffer = renderNote(engine, sampleRate, 45, 0.95f,
                                       PlayStyle::Sustain, 2.6, 0.5);
        expect(allFinite(buffer),
               "sympathetic coupling produced non-finite audio");
        const int start = static_cast<int>(1.4 * sampleRate);
        const int length = static_cast<int>(1.0 * sampleRate);
        return dftMagnitude(buffer.left, start, length, sampleRate, openHighE);
    };

    const double bypassed = tailEnergyAt(0.0f);
    const double coupled = tailEnergyAt(0.6f);
    expect(coupled > 12.0 * std::max(bypassed, 1.0e-12),
           "bridge coupling did not ring the open high E after the played "
           "string was damped (" + std::to_string(bypassed) + " -> "
               + std::to_string(coupled) + ")");

    // Reaching the exact bypass while an idle string is already ringing must
    // drop the physical loop, not only its ready flag. Otherwise a later
    // played note can inherit delay-line and filter residue that the engine
    // has declared absent.
    {
        ElectryEngine liveBypass;
        liveBypass.prepare(sampleRate, 512);
        auto liveParameters = parameters;
        liveParameters.sympatheticAmount = 1.0f;
        liveBypass.setParameters(liveParameters);
        liveBypass.reset();
        liveBypass.noteOn(45, 0.95f);
        StereoBuffer establish(static_cast<int>(0.20 * sampleRate));
        renderInto(liveBypass, establish);
        const int highString = ElectryEngine::stringCount - 1;
        const auto populatedFilters = TestAccess::loopFilterState(
            liveBypass, highString);
        expect(TestAccess::snapshot(liveBypass, highString).sympatheticReady
                   && TestAccess::loopLineEnergy(liveBypass, highString) > 0.0
                   && std::all_of(populatedFilters.begin(),
                                  populatedFilters.end(),
                                  [] (float state) { return state != 0.0f; }),
               "the live sympathetic-bypass fixture did not establish a ring");

        liveParameters.sympatheticAmount = 0.0f;
        liveBypass.setParameters(liveParameters);
        StereoBuffer close(static_cast<int>(0.20 * sampleRate));
        renderInto(liveBypass, close);
        const auto clearedFilters = TestAccess::loopFilterState(
            liveBypass, highString);
        expect(! TestAccess::snapshot(liveBypass, highString).sympatheticReady
                   && TestAccess::loopLineEnergy(
                          liveBypass, highString) == 0.0
                   && std::all_of(clearedFilters.begin(),
                                  clearedFilters.end(),
                                  [] (float state) { return state == 0.0f; }),
               "exact sympathetic bypass retained an idle loop state");
    }

    // At zero the coupled waveguides are never configured, keeping the bypass
    // both silent and free of idle-string work.
    parameters.sympatheticAmount = 0.0f;
    engine.setParameters(parameters);
    auto silentTail = renderNote(engine, sampleRate, 45, 0.95f,
                                 PlayStyle::Sustain, 2.6, 0.5);
    for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount; ++stringIndex)
    {
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        expect(! snapshot.sympatheticReady,
               "a coupled string was configured while the control was at zero");
    }
    expect(peakAbs(silentTail.left, static_cast<int>(2.2 * sampleRate)) < 1.0e-6f,
           "bypassed engine still rang two seconds after the note died");

    // Coupling never invents ambience: with no note ever played the bus stays
    // at zero, so nothing can be injected.
    parameters.sympatheticAmount = 1.0f;
    engine.setParameters(parameters);
    engine.reset();
    StereoBuffer idle(8192);
    renderInto(engine, idle);
    expect(peakAbs(idle.left) == 0.0f && peakAbs(idle.right) == 0.0f,
           "sympathetic coupling generated output without a played note");

    // Determinism, including across engine reuse.
    const auto first = renderNote(engine, sampleRate, 45, 0.8f,
                                  PlayStyle::Sustain, 0.9, 0.4);
    const auto second = renderNote(engine, sampleRate, 45, 0.8f,
                                   PlayStyle::Sustain, 0.9, 0.4);
    expect(first.left == second.left && first.right == second.right,
           "sympathetic coupling is not sample-deterministic");

    // Dead is made by the fretting hand lying across the strings, so the
    // coupled strings must inherit the same calibrated 1.6 s contact as the
    // picked one. This exercises the actual keyswitch -> voice -> control
    // path. No contact yields gain 1/infinite T60; treating Dead as a full
    // bridge-hand stop yields 45 ms, and both are physically wrong here.
    parameters.sympatheticAmount = 0.20f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Dead), 1.0f);
    engine.noteOn(28, 0.95f);
    StereoBuffer deadControlTick(512);
    renderInto(engine, deadControlTick);
    const double deadHandGain = TestAccess::sympatheticHandGainTarget(engine);
    expect(deadHandGain > 0.0 && deadHandGain < 1.0,
           "Dead left the sympathetic hand gain at bypass or invalid");
    const double deadCoupledT60 = -3.0
        / (TestAccess::internalSampleRate(engine) * std::log10(deadHandGain));
    expect(std::isfinite(deadCoupledT60)
               && deadCoupledT60 > 1.2 && deadCoupledT60 < 2.1,
           "Dead did not apply its fretting-hand loss to the sympathetic "
           "strings (implied T60 " + std::to_string(deadCoupledT60) + " s)");

    // The shared hand follows performance order, not whichever historical
    // voice happens to have the strongest mute. Releasing a Palm note alone
    // must keep the hand down through the gap between chugs; a newer Sustain
    // must lift it, and a newer Dead must replace it with Dead's calibrated
    // contact instead of inheriting the older, stronger Palm position.
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(28, 0.95f);
    StereoBuffer palmBody(static_cast<int>(0.080 * sampleRate));
    renderInto(engine, palmBody);
    const double palmHandGain = TestAccess::sympatheticHandGainTarget(engine);
    expect(palmHandGain > 0.0 && palmHandGain < 1.0,
           "Palm did not lower the shared hand-gain target");
    engine.noteOff(28);
    StereoBuffer palmGap(32);
    renderInto(engine, palmGap);
    expect(TestAccess::sympatheticHandGainTarget(engine) == palmHandGain,
           "releasing a Palm note lifted the hand inside a chug gap");
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(40, 0.95f);
    StereoBuffer openAccentTick(32);
    renderInto(engine, openAccentTick);
    expect(TestAccess::sympatheticHandGainTarget(engine) == 1.0f,
           "an older Palm tail clamped a newer Sustain attack");

    // Retiring that newer voice is not another physical contact. The shared
    // hand must therefore stay at the Sustain position instead of rediscovering
    // an older held Palm voice and travelling backward in performance history.
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(28, 0.95f);
    renderInto(engine, palmBody);
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(40, 0.95f);
    renderInto(engine, openAccentTick);
    const int heldPalmString = TestAccess::stringForNote(engine, 28);
    const int retiringSustainString = TestAccess::stringForNote(engine, 40);
    expect(heldPalmString >= 0 && retiringSustainString >= 0,
           "shared-hand retirement fixture did not allocate both strings");
    engine.noteOff(40);
    StereoBuffer throughSustainRetirement(static_cast<int>(0.75 * sampleRate));
    renderInto(engine, throughSustainRetirement);
    expect(! TestAccess::snapshot(engine, retiringSustainString).active
               && TestAccess::snapshot(engine, heldPalmString).active,
           "shared-hand retirement fixture did not retire only Sustain E2");
    expect(TestAccess::sympatheticHandGainTarget(engine) == 1.0f,
           "retiring a newer Sustain contact restored an older Palm hand");

    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(28, 0.95f);
    renderInto(engine, palmBody);
    engine.noteOn(styleKeyswitch(PlayStyle::Dead), 1.0f);
    engine.noteOn(40, 0.95f);
    StereoBuffer deadAfterPalmTick(32);
    renderInto(engine, deadAfterPalmTick);
    const double deadAfterPalmGain =
        TestAccess::sympatheticHandGainTarget(engine);
    const double deadAfterPalmT60 = -3.0
        / (TestAccess::internalSampleRate(engine)
           * std::log10(deadAfterPalmGain));
    expect(deadAfterPalmGain > palmHandGain
               && std::isfinite(deadAfterPalmT60)
               && deadAfterPalmT60 > 1.2 && deadAfterPalmT60 < 2.1,
           "an older held Palm overrode a newer Dead hand position (implied "
           "T60 " + std::to_string(deadAfterPalmT60) + " s)");

    // A delayed fret change reuses the ringing physical string but installs
    // the future attack's style immediately. Its old contact timestamp must
    // not make that future Palm style move the shared hand during pre-roll.
    auto retargetParameters = parameters;
    retargetParameters.sympatheticAmount = 0.80f;
    retargetParameters.strumSpreadSeconds = 0.020f;
    engine.setParameters(retargetParameters);
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(28, 0.90f);
    StereoBuffer ringingSustain(static_cast<int>(0.080 * sampleRate));
    renderInto(engine, ringingSustain);
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(29, 0.90f);
    const int retargetString = TestAccess::stringForNote(engine, 29);
    expect(retargetString >= 0
               && TestAccess::snapshot(engine, retargetString)
                      .startDelaySamples > 0,
           "delayed fret-change hand fixture did not enter pre-roll");
    StereoBuffer beforeRetargetContact(32);
    renderInto(engine, beforeRetargetContact, 1);
    expect(TestAccess::sympatheticHandGainTarget(engine) == 1.0f,
           "a delayed fret change reused the old contact timestamp for its "
           "future Palm style");
    StereoBuffer throughRetargetContact(static_cast<int>(0.050 * sampleRate));
    renderInto(engine, throughRetargetContact);
    expect(TestAccess::snapshot(engine, retargetString).startDelaySamples == 0
               && TestAccess::sympatheticHandGainTarget(engine) < 1.0f,
           "the delayed fret-change Palm did not move the hand at contact");

    // The inverse matters too: installing a future Sustain descriptor must
    // not erase the Palm contact that is still physically ringing. If that
    // future pick is cancelled, no newer contact exists and the bridge hand
    // must remain where the old stroke left it.
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(28, 0.90f);
    renderInto(engine, ringingSustain);
    const double retainedPalmGain =
        TestAccess::sympatheticHandGainTarget(engine);
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(29, 0.90f);
    const int cancelledRetargetString = TestAccess::stringForNote(engine, 29);
    expect(cancelledRetargetString >= 0
               && TestAccess::snapshot(engine, cancelledRetargetString)
                      .startDelaySamples > 0,
           "inverse delayed fret-change fixture did not enter pre-roll");
    renderInto(engine, beforeRetargetContact, 1);
    expect(TestAccess::sympatheticHandGainTarget(engine) == retainedPalmGain,
           "a future Sustain descriptor lifted the preceding Palm contact");
    engine.noteOff(29);
    renderInto(engine, beforeRetargetContact, 1);
    expect(TestAccess::sympatheticHandGainTarget(engine) == retainedPalmGain,
           "cancelling a future Sustain contact erased the preceding Palm hand");

    // Two contacts can share an audio clock. Their contact-time order, not a
    // later MIDI reservation on one of those voices, breaks that tie.
    retargetParameters.strumSpreadSeconds = 0.0f;
    engine.setParameters(retargetParameters);
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(28, 0.90f);
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(40, 0.90f);
    retargetParameters.strumSpreadSeconds = 0.020f;
    engine.setParameters(retargetParameters);
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(29, 0.90f);
    const int tiedRetargetString = TestAccess::stringForNote(engine, 29);
    expect(tiedRetargetString >= 0
               && TestAccess::snapshot(engine, tiedRetargetString)
                      .startDelaySamples > 0,
           "same-clock contact-order fixture did not schedule its retarget");
    renderInto(engine, beforeRetargetContact, 1);
    expect(TestAccess::sympatheticHandGainTarget(engine) < 1.0f,
           "a future reservation rewrote a same-clock physical contact tie");

    // MIDI lookahead order is not physical hand order. Re-anchor a chord so a
    // later-scheduled Sustain E1 contacts first and the earlier-scheduled Palm
    // repick on high E contacts last; that last real contact must move the hand.
    auto contactOrderParameters = parameters;
    contactOrderParameters.sympatheticAmount = 0.80f;
    contactOrderParameters.strumSpreadSeconds = 0.020f;
    engine.setParameters(contactOrderParameters);
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(64, 0.90f);
    StereoBuffer highSustainBody(static_cast<int>(0.060 * sampleRate));
    renderInto(engine, highSustainBody);
    const int highIndex = TestAccess::stringForNote(engine, 64);
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(64, 0.90f);
    engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
    engine.noteOn(28, 0.90f);
    StereoBuffer throughLowContact(static_cast<int>(0.050 * sampleRate));
    renderInto(engine, throughLowContact);
    expect(highIndex >= 0
               && TestAccess::snapshot(engine, highIndex).startDelaySamples > 0
               && TestAccess::sympatheticHandGainTarget(engine) == 1.0f,
           "a reserved Palm repick moved the shared hand before contact");
    StereoBuffer throughLatePalm(static_cast<int>(0.180 * sampleRate));
    renderInto(engine, throughLatePalm);
    const auto latePalm = TestAccess::snapshot(engine, highIndex);
    expect(latePalm.valid && latePalm.startDelaySamples == 0
               && latePalm.playStyle == PlayStyle::PalmMute
               && TestAccess::sympatheticHandGainTarget(engine) < 1.0f,
           "the physically latest delayed Palm contact lost to MIDI order");

    // A fingered string never keeps its coupled ring: the hand stops it.
    engine.reset();
    engine.noteOn(45, 0.9f);
    StereoBuffer settle(static_cast<int>(0.4 * sampleRate));
    renderInto(engine, settle);
    const int highString = ElectryEngine::stringCount - 1;
    expect(TestAccess::snapshot(engine, highString).sympatheticReady,
           "the open high E did not begin ringing through the bridge");
    engine.noteOn(64, 0.9f);
    expect(! TestAccess::snapshot(engine, highString).sympatheticReady,
           "picking a coupled string did not hand it back to the player");

    // Worst case: every control at its extreme, every string struck hard, and
    // the coupled loops driven as hard as the model allows.
    for (const double rate : { 44100.0, 96000.0, 192000.0 })
    {
        ElectryEngine hot;
        hot.prepare(rate, 512);
        EngineParameters extreme;
        extreme.sympatheticAmount = 1.0f;
        extreme.artifactAmount = 1.0f;
        extreme.bodyResonance = 1.0f;
        extreme.stringAge = 0.0f;
        extreme.construction = 0.0f;
        extreme.outputGain = 2.0f;
        extreme.outputMode = electry::OutputMode::Stereo;
        hot.setParameters(extreme);
        hot.reset();
        for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
            hot.noteOn(note, 1.0f);
        StereoBuffer strum(static_cast<int>(0.9 * rate));
        renderInto(hot, strum);
        hot.allNotesOff();
        StereoBuffer ring(static_cast<int>(0.9 * rate));
        renderInto(hot, ring);
        expect(allFinite(strum) && allFinite(ring),
               "maximum coupling became non-finite at "
                   + std::to_string(rate) + " Hz");
        // The linked soft guard bounds any input to 1/sqrt(0.4356) = 1.516
        // before the output gain, so the maximum reachable peak at the
        // maximum +6 dB output is 3.03. Staying under that proves the coupled
        // loops cannot drive the model past its analytic ceiling.
        expect(peakAbs(strum.left) < 3.05f && peakAbs(ring.left) < 3.05f,
               "maximum coupling escaped the output guard at "
                   + std::to_string(rate) + " Hz");
        // The coupled strings must decay rather than sustain. With no plucked
        // voice left there is nothing writing to the bridge bus, so the ring
        // has to fall away and the engine has to reach exact silence.
        expect(peakAbs(ring.left) > 1.0e-3f,
               "the coupled strings did not ring after the notes ended at "
                   + std::to_string(rate) + " Hz");
        StereoBuffer firstTail(static_cast<int>(2.0 * rate));
        renderInto(hot, firstTail);
        StereoBuffer secondTail(static_cast<int>(2.0 * rate));
        renderInto(hot, secondTail);
        expect(allFinite(firstTail) && allFinite(secondTail),
               "maximum coupling became non-finite while ringing out at "
                   + std::to_string(rate) + " Hz");
        expect(peakAbs(firstTail.left) < 1.0e-3f
                   && peakAbs(secondTail.left) == 0.0f,
               "maximum coupling did not ring out to exact silence at "
                   + std::to_string(rate) + " Hz ("
                   + std::to_string(peakAbs(firstTail.left)) + " -> "
                   + std::to_string(peakAbs(secondTail.left)) + ")");
    }
}

// ---------------------------------------------------------------------------
// Version 1.1: continuous palm-mute pressure
// ---------------------------------------------------------------------------

void testPalmMuteContinuum()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;

    const auto lateRms = [&] (float amount)
    {
        parameters.palmMute = amount;
        engine.setParameters(parameters);
        const auto buffer = renderNote(engine, sampleRate, 45, 0.9f,
                                       PlayStyle::Sustain, 0.9);
        expect(allFinite(buffer) && peakAbs(buffer.left) < 0.80f,
               "palm mute produced non-finite or unbounded audio at "
                   + std::to_string(amount));
        return rmsInRange(buffer.left, static_cast<int>(0.30 * sampleRate),
                          static_cast<int>(0.60 * sampleRate));
    };

    const double open = lateRms(0.0f);
    const double half = lateRms(0.5f);
    const double full = lateRms(1.0f);
    expect(open > half * 2.0 && half > full * 2.0,
           "palm mute pressure does not shorten decay monotonically ("
               + std::to_string(open) + ", " + std::to_string(half) + ", "
               + std::to_string(full) + ")");

    // Zero pressure is a mathematical no-op, not merely a small effect: the
    // rendered audio must be identical to an engine that never saw the
    // control.
    EngineParameters untouched = parameters;
    untouched.palmMute = 0.0f;
    engine.setParameters(untouched);
    const auto reference = renderNote(engine, sampleRate, 45, 0.9f,
                                      PlayStyle::Sustain, 0.5);
    EngineParameters explicitZero = untouched;
    explicitZero.palmMute = 0.0f;
    engine.setParameters(explicitZero);
    const auto atZero = renderNote(engine, sampleRate, 45, 0.9f,
                                   PlayStyle::Sustain, 0.5);
    expect(reference.left == atZero.left,
           "palm mute at zero is not an exact bypass");

    // Damping is re-solved through the same loop-filter path as every other
    // decay control, so a muted string still sounds the played pitch.
    parameters.palmMute = 0.45f;
    engine.setParameters(parameters);
    const auto muted = renderNote(engine, sampleRate, 45, 0.9f,
                                  PlayStyle::Sustain, 0.5);
    const double expectedHz = midiHz(45);
    const double measured = measureFrequency(
        muted.left, static_cast<int>(0.02 * sampleRate),
        static_cast<int>(0.18 * sampleRate), sampleRate, expectedHz);
    expect(std::abs(centsBetween(measured, expectedHz)) < 12.0,
           "palm-muted string drifted out of tune ("
               + std::to_string(centsBetween(measured, expectedHz)) + " cents)");

    // The loop filter genuinely moves rather than a gain being applied after
    // the fact.
    parameters.palmMute = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(45, 0.9f);
    const float openDamping =
        TestAccess::snapshot(engine, TestAccess::stringForNote(engine, 45))
            .loopDampingCoefficient;
    parameters.palmMute = 0.9f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(45, 0.9f);
    const float mutedDamping =
        TestAccess::snapshot(engine, TestAccess::stringForNote(engine, 45))
            .loopDampingCoefficient;
    // A shorter decay target needs a heavier one-pole, because the solve
    // matches the ratio between the fundamental and high-frequency T60s.
    expect(mutedDamping > openDamping + 1.0e-3f,
           "palm mute did not change the solved loop-filter coefficient ("
               + std::to_string(openDamping) + " -> "
               + std::to_string(mutedDamping) + ")");

    // CC2 pressure adds to the parameter and is released cleanly.
    parameters.palmMute = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.setPalmMutePressure(1.0f);
    engine.noteOn(45, 0.9f);
    StereoBuffer pressed(static_cast<int>(0.9 * sampleRate));
    renderInto(engine, pressed);
    const double pressedLate = rmsInRange(pressed.left,
                                          static_cast<int>(0.30 * sampleRate),
                                          static_cast<int>(0.60 * sampleRate));
    expect(allFinite(pressed) && pressedLate < half * 2.0,
           "CC2 pressure did not damp the string");

    // A controller and note-on at the same MIDI offset are dispatched without
    // an audio sample between them. That attack must be identical to pressure
    // already present when the engine resets; otherwise the one-shot
    // excitation and palm impact were configured from a stale cached blend.
    const auto cc2Attack = [] (bool pressureBeforeReset)
    {
        ElectryEngine probe;
        probe.prepare(sampleRate, 512);
        probe.setParameters(EngineParameters {});
        if (pressureBeforeReset)
            probe.setPalmMutePressure(1.0f);
        probe.reset();
        if (! pressureBeforeReset)
            probe.setPalmMutePressure(1.0f);
        probe.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        probe.noteOn(45, 0.90f);
        StereoBuffer audio(static_cast<int>(0.20 * sampleRate));
        renderInto(probe, audio);
        return audio;
    };
    const auto sameSamplePressure = cc2Attack(false);
    const auto alreadyAppliedPressure = cc2Attack(true);
    expect(sameSamplePressure.left == alreadyAppliedPressure.left
               && sameSamplePressure.right == alreadyAppliedPressure.right,
           "same-sample CC2 did not palm-shape the complete note attack");

    engine.setPalmMutePressure(std::numeric_limits<float>::quiet_NaN());
    engine.reset();
    engine.noteOn(45, 0.9f);
    StereoBuffer afterHostile(static_cast<int>(0.3 * sampleRate));
    renderInto(engine, afterHostile);
    expect(allFinite(afterHostile),
           "hostile CC2 pressure produced non-finite audio");
}

void testPalmMuteHandContactDynamics()
{
    constexpr double sampleRate = 48000.0;
    constexpr int renderSamples = static_cast<int>(sampleRate);
    constexpr std::array<float, 3> velocities {{ 0.20f, 0.60f, 1.00f }};
    constexpr std::array<float, 5> sweep {{ 0.0f, 0.25f, 0.50f, 0.75f,
                                            1.0f }};

    EngineParameters base;
    base.velocityAmount = 1.0f;
    base.artifactAmount = 0.0f;
    base.sympatheticAmount = 0.0f;
    base.pickNoise = 0.0f;
    base.fingerNoise = 0.0f;
    base.releaseNoise = 0.0f;

    struct ContactRender
    {
        StereoBuffer audio;
        float contactScale;
    };

    const auto render = [&] (int midiNote, float velocity,
                             std::uint64_t precedingNotes,
                             const EngineParameters& parameters,
                             PlayStyle style, float continuousPressure,
                             bool forceUnityContact)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.setPalmMutePressure(continuousPressure);
        engine.reset();
        TestAccess::setPrecedingNoteCount(engine, precedingNotes);
        engine.noteOn(pickKeyswitch(PickStyle::Down), 1.0f);
        engine.noteOn(styleKeyswitch(style), 1.0f);
        engine.noteOn(midiNote, velocity);

        const int stringIndex = TestAccess::stringForNote(engine, midiNote);
        expect(stringIndex >= 0,
               "hand-contact fixture did not allocate its target string");
        const float contactScale = stringIndex >= 0
            ? TestAccess::handContactScale(engine, stringIndex) : 0.0f;
        if (forceUnityContact && stringIndex >= 0)
            TestAccess::forceUnityHandContact(engine, stringIndex);

        ContactRender result { StereoBuffer(renderSamples), contactScale };
        renderInto(engine, result.audio);
        expect(allFinite(result.audio),
               "hand-contact fixture produced non-finite audio");
        return result;
    };

    // Match the evaluation contract: the 0.5-1.0 s RMS is expressed relative
    // to the attack's own 0-50 ms RMS, so MIDI level does not masquerade as a
    // decay change.
    const auto normalisedTailDb = [&] (const StereoBuffer& audio)
    {
        const double early = rmsInRange(audio.left, 0,
                                        static_cast<int>(0.050 * sampleRate));
        const double tail = rmsInRange(audio.left,
                                       static_cast<int>(0.500 * sampleRate),
                                       renderSamples);
        return decibels(tail / std::max(early, 1.0e-15));
    };

    for (const int midiNote : { 28, 40 })
    {
        std::array<double, velocities.size()> tails {};
        for (std::size_t i = 0; i < velocities.size(); ++i)
            tails[i] = normalisedTailDb(render(
                midiNote, velocities[i], 0, base, PlayStyle::PalmMute, 0.0f,
                false).audio);

        const double spread = tails.front() - tails.back();
        expect(tails[0] > tails[1] && tails[1] > tails[2],
               "palm-mute contact does not tighten with velocity on note "
                   + std::to_string(midiNote) + " ("
                   + std::to_string(tails[0]) + ", "
                   + std::to_string(tails[1]) + ", "
                   + std::to_string(tails[2]) + " dB)");
        expect(spread >= 2.8 && spread <= 12.0,
               "soft-to-hard palm-mute tail spread left its calibrated range "
                   "on note " + std::to_string(midiNote) + " ("
                   + std::to_string(spread) + " dB)");
        std::cout << "PROBE palm-hand note " << midiNote
                  << " normalised tails: " << tails[0] << ", " << tails[1]
                  << ", " << tails[2] << " dB; spread " << spread << " dB\n";
    }

    // Velocity Response at zero removes velocity from the contact as exactly
    // as it removes it from excitation: this is an identity, not a tolerance.
    EngineParameters flat = base;
    flat.velocityAmount = 0.0f;
    const auto flatLow = render(28, 0.20f, 2, flat, PlayStyle::PalmMute,
                                0.0f, false);
    const auto flatHigh = render(28, 1.00f, 2, flat, PlayStyle::PalmMute,
                                 0.0f, false);
    expect(flatLow.audio.left == flatHigh.audio.left
               && flatLow.audio.right == flatHigh.audio.right,
           "zero Velocity Response still changes palm-mute audio");
    expect(flatLow.contactScale == flatHigh.contactScale,
           "zero Velocity Response still changes the latched hand contact");

    const auto expectNonincreasing = [&] (const std::array<double, sweep.size()>& db,
                                           const char* control)
    {
        for (std::size_t i = 1; i < db.size(); ++i)
            expect(db[i] <= db[i - 1],
                   std::string(control) + " made the same stroke's palm-mute "
                       "tail grow (" + std::to_string(db[i - 1]) + " -> "
                       + std::to_string(db[i]) + " dB)");
    };

    std::array<double, sweep.size()> muteDampingTails {};
    for (std::size_t i = 0; i < sweep.size(); ++i)
    {
        EngineParameters parameters = base;
        parameters.muteDamping = sweep[i];
        muteDampingTails[i] = normalisedTailDb(render(
            28, 0.90f, 4, parameters, PlayStyle::PalmMute, 0.0f, false).audio);
    }
    expectNonincreasing(muteDampingTails, "Palm Tightness");

    std::array<double, sweep.size()> pressureTails {};
    for (std::size_t i = 0; i < sweep.size(); ++i)
        pressureTails[i] = normalisedTailDb(render(
            28, 0.90f, 4, base, PlayStyle::Sustain, sweep[i], false).audio);
    expectNonincreasing(pressureTails, "continuous pressure");

    // MIDI CC2 must remain a continuous performance control at the exact
    // low-string solver boundaries. The former infeasible one-pole fit jumped
    // E1's first 50 ms by roughly 4.7 dB at 97->98 and E2 by 2.3 dB at
    // 116->117.
    for (const auto [midiNote, lowerCc, upperCc] :
         std::array<std::array<int, 3>, 2> {{{ 28, 97, 98 },
                                             { 40, 116, 117 }}})
    {
        const auto lower = render(midiNote, 0.90f, 4, base,
                                  PlayStyle::PalmMute,
                                  static_cast<float>(lowerCc) / 127.0f, false);
        const auto upper = render(midiNote, 0.90f, 4, base,
                                  PlayStyle::PalmMute,
                                  static_cast<float>(upperCc) / 127.0f, false);
        const double adjacentCcAttackDb = std::abs(decibels(
            rmsInRange(upper.audio.left, 0,
                       static_cast<int>(0.050 * sampleRate))
            / std::max(rmsInRange(lower.audio.left, 0,
                                  static_cast<int>(0.050 * sampleRate)),
                       1.0e-15)));
        expect(adjacentCcAttackDb < 0.5,
               "adjacent Palm Pressure values moved note "
                   + std::to_string(midiNote) + " attack by "
                   + std::to_string(adjacentCcAttackDb) + " dB");
        std::cout << "PROBE Palm note " << midiNote << " CC2 " << lowerCc
                  << "->" << upperCc << " attack delta: "
                  << adjacentCcAttackDb << " dB\n";
    }

    std::array<double, 12> variedTails {};
    std::array<float, 12> contactScales {};
    for (std::size_t i = 0; i < variedTails.size(); ++i)
    {
        const auto stroke = render(28, 0.90f, i, base, PlayStyle::PalmMute,
                                   0.0f, false);
        variedTails[i] = normalisedTailDb(stroke.audio);
        contactScales[i] = stroke.contactScale;
    }
    const auto [minimumTail, maximumTail] = std::minmax_element(
        variedTails.begin(), variedTails.end());
    const double variationDb = *maximumTail - *minimumTail;
    expect(variationDb >= 0.25 && variationDb <= 6.0,
           "twelve deterministic palm-mute strokes have implausible tail "
           "variation (" + std::to_string(variationDb) + " dB)");
    const auto [minimumScale, maximumScale] = std::minmax_element(
        contactScales.begin(), contactScales.end());
    expect(*maximumScale > *minimumScale,
           "deterministic picking-hand draws do not reach hand contact");
    std::cout << "PROBE 12 palm-mute strokes: " << variationDb
              << " dB tail range, hand-contact scale " << *minimumScale
              << " to " << *maximumScale << '\n';

    std::array<double, 12> unityDeltas {};
    for (std::size_t i = 0; i < unityDeltas.size(); ++i)
    {
        const auto physical = render(28, 1.0f, i, base,
                                     PlayStyle::PalmMute, 0.0f, false);
        const auto unity = render(28, 1.0f, i, base,
                                  PlayStyle::PalmMute, 0.0f, true);
        unityDeltas[i] = std::abs(normalisedTailDb(physical.audio)
                                  - normalisedTailDb(unity.audio));
    }
    std::sort(unityDeltas.begin(), unityDeltas.end());
    const double medianUnityDelta = 0.5 * (unityDeltas[5] + unityDeltas[6]);
    expect(medianUnityDelta <= 0.5,
           "full-velocity hand variation moved the median mute calibration by "
               + std::to_string(medianUnityDelta) + " dB");
    std::cout << "PROBE full-velocity median absolute tail delta versus unity "
                 "hand contact: " << medianUnityDelta << " dB\n";
}

void testTremoloStudyClearsPalmBeforeHighLead()
{
    constexpr double sampleRate = 44100.0;
    const auto inspect = [] (bool clearDuringRest)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 256);
        EngineParameters parameters;
        parameters.palmMute = 0.12f;
        engine.setParameters(parameters);
        engine.reset();

        parameters.palmMute = 0.0f;
        if (clearDuringRest)
            engine.setParameters(parameters);
        StereoBuffer rest(static_cast<int>(0.24 * sampleRate));
        renderInto(engine, rest);
        if (! clearDuringRest)
            engine.setParameters(parameters);

        engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        engine.noteOn(74, 0.86f);
        const int stringIndex = TestAccess::stringForNote(engine, 74);
        expect(stringIndex >= 0,
               "demo 22 high-lead fixture did not allocate MIDI 74");
        return std::pair {
            TestAccess::palmMuteBlend(engine),
            stringIndex >= 0
                ? TestAccess::palmImpactVelocity(engine, stringIndex) : 0.0f
        };
    };

    const auto lateClear = inspect(false);
    const auto restClear = inspect(true);
    expect(lateClear.first > 0.11f && lateClear.second > 0.0f,
           "demo 22 late-clear control no longer demonstrates stale Palm contact");
    expect(restClear.first == 0.0f && restClear.second == 0.0f,
           "demo 22 rest did not clear Palm before its unmuted high lead");
}

void testPalmMuteSpectralLoss()
{
    constexpr double sampleRate = 48000.0;
    constexpr int earlyLength = static_cast<int>(0.050 * sampleRate);
    constexpr int lateStart = static_cast<int>(0.150 * sampleRate);
    constexpr int lateLength = static_cast<int>(0.350 * sampleRate);

    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    // Sum tracked harmonic power on either side of the same 500 Hz split used
    // by the controlled Guitar-TECHS comparison. Capping both notes at 2.6 kHz
    // keeps the compared upper band identical while still spanning the hand
    // dip and the low-string metal-guitar body.
    const auto bandPowers = [] (const StereoBuffer& audio, int start,
                                int length, double analysisSampleRate,
                                double fundamentalHz)
    {
        std::array<double, 2> power {};
        for (int partial = 1; partial <= 64; ++partial)
        {
            const double frequency = fundamentalHz * partial;
            if (frequency >= 2600.0)
                break;
            const double magnitude = scannedPartialMagnitude(
                audio.left, start, length, analysisSampleRate,
                fundamentalHz, partial);
            power[frequency < 500.0 ? 0u : 1u] += magnitude * magnitude;
        }
        return power;
    };

    // The controlled F2 comparison says the missing Palm behaviour happens
    // before the late-body check below: its >500 Hz share contracts from the
    // 0-30 ms onset into the 30-80 ms body much faster than the same player's
    // ordinary note. Keep that paired difference so excitation brightness
    // cannot masquerade as time-varying hand loss.
    const auto upperShare = [] (const std::array<double, 2>& power)
    {
        return power[1] / std::max(power[0] + power[1], 1.0e-30);
    };
    struct F2Contraction
    {
        double paired { 0.0 };
        double openOnset { 0.0 };
        double openBody { 0.0 };
        double palmOnset { 0.0 };
        double palmBody { 0.0 };
        std::array<double, 11> palmOpenTrajectoryDb {};
    };
    const auto measureF2Contraction = [&] (double analysisSampleRate)
    {
        constexpr int boundaryNote = 41;
        const int onsetLength = static_cast<int>(
            0.030 * analysisSampleRate);
        const int bodyLength = static_cast<int>(
            0.050 * analysisSampleRate);
        ElectryEngine boundaryEngine;
        boundaryEngine.prepare(analysisSampleRate, 512);
        boundaryEngine.setParameters(parameters);
        const auto open = renderNote(
            boundaryEngine, analysisSampleRate, boundaryNote, 0.90f,
            PlayStyle::Sustain, 0.10);
        const auto palm = renderNote(
            boundaryEngine, analysisSampleRate, boundaryNote, 0.90f,
            PlayStyle::PalmMute, 0.10);
        const double fundamental = midiHz(boundaryNote);
        const double openOnset = upperShare(bandPowers(
            open, 0, onsetLength, analysisSampleRate, fundamental));
        const double openBody = upperShare(bandPowers(
            open, onsetLength, bodyLength, analysisSampleRate, fundamental));
        const double palmOnset = upperShare(bandPowers(
            palm, 0, onsetLength, analysisSampleRate, fundamental));
        const double palmBody = upperShare(bandPowers(
            palm, onsetLength, bodyLength, analysisSampleRate, fundamental));
        const double openChange = decibels(std::sqrt(
            openBody / std::max(openOnset, 1.0e-30)));
        const double palmChange = decibels(std::sqrt(
            palmBody / std::max(palmOnset, 1.0e-30)));
        std::array<double, 11> palmOpenTrajectoryDb {};
        if (analysisSampleRate == 44100.0)
        {
            // F2 needs more than the tempting 5-10 ms sub-cycle slices. Slide
            // one 30 ms (>2.5-cycle) window instead; the four-rate endpoint
            // sweep below already guards sample-rate invariance, so paying for
            // the overlapping harmonic scans once is enough.
            const int trajectoryLength = static_cast<int>(
                0.030 * analysisSampleRate);
            const int trajectoryStep = static_cast<int>(
                0.005 * analysisSampleRate);
            for (std::size_t i = 0; i < palmOpenTrajectoryDb.size(); ++i)
            {
                const int start = static_cast<int>(i) * trajectoryStep;
                const double openShare = upperShare(bandPowers(
                    open, start, trajectoryLength, analysisSampleRate,
                    fundamental));
                const double palmShare = upperShare(bandPowers(
                    palm, start, trajectoryLength, analysisSampleRate,
                    fundamental));
                palmOpenTrajectoryDb[i] = decibels(std::sqrt(
                    palmShare / std::max(openShare, 1.0e-30)));
            }
        }
        return F2Contraction {
            palmChange - openChange,
            openOnset, openBody, palmOnset, palmBody,
            palmOpenTrajectoryDb
        };
    };
    // Re-running this exact tracked-harmonic extractor on the licensed F2
    // ordinary/Palm cells gives -6.098930 dB for P1 and -15.289719 dB for P2.
    // Two player/guitar cells establish direction, not a robust population
    // rail. The retired finite-contact prototype overfit that sparse proxy and
    // audibly removed too much low body, so keep only the observed contraction
    // direction and the implementation's sample-rate invariance here. The
    // commissioned holdout remains the actual fit gate.
    std::array<double, 4> contractions {};
    std::size_t contractionIndex = 0;
    for (const double analysisSampleRate : { 44100.0, 48000.0,
                                             96000.0, 192000.0 })
    {
        const auto result = measureF2Contraction(analysisSampleRate);
        expect(std::isfinite(result.paired) && result.paired < -0.25,
               "F2 Palm early upper share did not contract at "
                   + std::to_string(analysisSampleRate) + " Hz");
        contractions[contractionIndex++] = result.paired;
        std::cout << "PROBE F2 Palm " << analysisSampleRate
                  << " Hz paired 0-30 -> 30-80 ms upper-share contraction: "
                  << result.paired << " dB; Open " << result.openOnset
                  << " -> " << result.openBody << ", Palm "
                  << result.palmOnset << " -> " << result.palmBody << '\n';
        if (analysisSampleRate == 44100.0)
        {
            // Two public player/guitar cells establish the mechanism's
            // direction, not a population rail for eleven highly overlapping
            // windows. Report the shape without freezing it until the
            // commissioned TRAIN clusters can set a defensible bound.
            for (const double value : result.palmOpenTrajectoryDb)
                expect(std::isfinite(value),
                       "F2 Palm/Open selective-loss trajectory was not finite");
            std::cout << "PROBE F2 Palm/Open 30 ms trajectory at 0..50 ms: ";
            for (const double value : result.palmOpenTrajectoryDb)
                std::cout << value << ' ';
            std::cout << "dB\n";
        }
    }
    const auto contractionExtremes = std::minmax_element(
        contractions.begin(), contractions.end());
    expect(*contractionExtremes.second - *contractionExtremes.first < 0.05,
           "F2 Palm early selective contraction changed with sample rate");

    for (const int midiNote : { 28, 40 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        const auto open = renderNote(engine, sampleRate, midiNote, 0.90f,
                                     PlayStyle::Sustain, 0.55);
        const auto muted = renderNote(engine, sampleRate, midiNote, 0.90f,
                                      PlayStyle::PalmMute, 0.55);
        const double fundamentalHz = midiHz(midiNote);
        const auto openEarly = bandPowers(
            open, 0, earlyLength, sampleRate, fundamentalHz);
        const auto mutedEarly = bandPowers(
            muted, 0, earlyLength, sampleRate, fundamentalHz);
        const auto openLate = bandPowers(open, lateStart, lateLength,
                                         sampleRate, fundamentalHz);
        const auto mutedLate = bandPowers(muted, lateStart, lateLength,
                                          sampleRate, fundamentalHz);
        const int bodyOnsetLength = static_cast<int>(0.030 * sampleRate);
        const int bodyLength = static_cast<int>(0.050 * sampleRate);
        const double mutedBodyOnsetDb = decibels(
            rmsInRange(muted.left, bodyOnsetLength,
                       bodyOnsetLength + bodyLength)
            / std::max(rmsInRange(muted.left, 0, bodyOnsetLength), 1.0e-30));

        const double lowPairedDecay = decibels(std::sqrt(
            mutedLate[0] / std::max(mutedEarly[0], 1.0e-30)))
            - decibels(std::sqrt(
                openLate[0] / std::max(openEarly[0], 1.0e-30)));
        const double highPairedDecay = decibels(std::sqrt(
            mutedLate[1] / std::max(mutedEarly[1], 1.0e-30)))
            - decibels(std::sqrt(
                openLate[1] / std::max(openEarly[1], 1.0e-30)));
        const double extraHighLoss = lowPairedDecay - highPairedDecay;
        const double earlyTiltDelta = decibels(std::sqrt(
            mutedEarly[1] / std::max(mutedEarly[0], 1.0e-30)))
            - decibels(std::sqrt(
                openEarly[1] / std::max(openEarly[0], 1.0e-30)));

        // The current defaults measure 13.427/14.362 dB of extra high-band
        // loss and 23.194/22.582 dB of absolute high-band loss on
        // E1/E2. These floors reject a materially weaker hand tilt. The
        // separate 30-80 ms floor rejects the opposite failure: a blanket
        // contact that erases the low body while satisfying a spectral ratio.
        const double minimumExtraLoss = midiNote == 28 ? 7.0 : 10.0;
        const double minimumHighLoss = midiNote == 28 ? 15.0 : 18.0;
        const double minimumBody = midiNote == 28 ? -4.5 : -3.75;
        expect(std::isfinite(extraHighLoss)
                   && extraHighLoss > minimumExtraLoss
                   && highPairedDecay < -minimumHighLoss,
               "palm mute lost too little >500 Hz body energy on "
                   + std::to_string(midiNote));
        expect(std::isfinite(mutedBodyOnsetDb)
                   && mutedBodyOnsetDb > minimumBody,
               "palm mute lost too much 30-80 ms body on "
                   + std::to_string(midiNote));
        // The early attack is also darker at the defaults. E2 is close enough
        // to flat that this is deliberately only a directional secondary rail;
        // the two stronger selective-loss and body checks above carry the
        // magnitude requirement.
        const double maximumEarlyTilt = midiNote == 28 ? -2.0 : 0.0;
        expect(std::isfinite(earlyTiltDelta)
                   && earlyTiltDelta < maximumEarlyTilt,
               "palm-mute attack was not darker than open on "
                   + std::to_string(midiNote));
        std::cout << "PROBE palm spectral note " << midiNote
                  << ": paired 150-500 ms low/high " << lowPairedDecay << "/"
                  << highPairedDecay << " dB, extra high loss "
                  << extraHighLoss << " dB; 0-50 ms high/low delta "
                  << earlyTiltDelta << " dB; 30-80/0-30 body "
                  << mutedBodyOnsetDb << " dB\n";
    }
}

void testRapidPalmMuteChugs()
{
    constexpr double sampleRate = 48000.0;
    constexpr int hitCount = 12;
    constexpr std::array<int, 2> notes {{ 28, 40 }};
    constexpr std::array<int, 7> ioisMs {{ 25, 30, 35, 40, 60, 80, 125 }};
    constexpr std::size_t repickCellCount = notes.size() * ioisMs.size();

    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    const auto prepareEngine = [&] (ElectryEngine& engine, PickStyle pickStyle)
    {
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(pickKeyswitch(pickStyle), 1.0f);
        engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    };

    struct Chugs
    {
        StereoBuffer audio;
        std::array<double, hitCount> rmsDb {};
        std::array<double, hitCount> peakDb {};

        explicit Chugs(int samples) : audio(samples) {}
    };

    const auto render = [&] (int note, int ioiMs, PickStyle pickStyle)
    {
        const int ioiSamples = static_cast<int>(ioiMs * sampleRate / 1000.0);
        Chugs result(hitCount * ioiSamples);

        ElectryEngine engine;
        prepareEngine(engine, pickStyle);

        // Never let one hit's meter cross the next onset. This matters at the
        // 25 ms tremolo edge and also keeps the last range inside the buffer.
        const int measurementSamples = std::min(
            ioiSamples, static_cast<int>(0.030 * sampleRate));
        for (int hit = 0; hit < hitCount; ++hit)
        {
            const int start = hit * ioiSamples;
            engine.noteOn(note, 0.90f);
            engine.process(result.audio.left.data() + start,
                           result.audio.right.data() + start, ioiSamples);
            const int end = start + measurementSamples;
            result.rmsDb[static_cast<std::size_t>(hit)] = decibels(
                std::max(rmsInRange(result.audio.left, start, end), 1.0e-12));
            result.peakDb[static_cast<std::size_t>(hit)] = decibels(
                std::max<double>(peakAbs(result.audio.left, start, end),
                                 1.0e-12));
        }
        return result;
    };

    const auto isolatedHitRmsDb = [&] (int note, int ioiMs, int strokeIndex)
    {
        const int ioiSamples = static_cast<int>(ioiMs * sampleRate / 1000.0);
        const int measurementSamples = std::min(
            ioiSamples, static_cast<int>(0.030 * sampleRate));
        ElectryEngine engine;
        prepareEngine(engine, PickStyle::Down);
        // startVoice() increments this counter before drawing the stroke. A
        // value of `strokeIndex` therefore recreates phrase hit
        // `strokeIndex` without carrying the preceding string vibration.
        TestAccess::setPrecedingNoteCount(
            engine, static_cast<std::uint64_t>(strokeIndex));
        engine.noteOn(note, 0.90f);
        StereoBuffer audio(measurementSamples);
        renderInto(engine, audio);
        return decibels(std::max(
            rmsInRange(audio.left, 0, measurementSamples), 1.0e-12));
    };

    const auto mean = [] (const auto& values, int first, int count)
    {
        double total = 0.0;
        for (int i = first; i < first + count; ++i)
            total += values[static_cast<std::size_t>(i)];
        return total / static_cast<double>(count);
    };

    std::array<double, repickCellCount> repickMeanErrors {};
    std::array<double, repickCellCount> repickWorstErrors {};
    std::size_t repickCell = 0;

    for (const int note : notes)
    for (const int ioiMs : ioisMs)
    {
        for (const auto pickStyle : { PickStyle::Down, PickStyle::Alternate })
        {
            const auto first = render(note, ioiMs, pickStyle);
            const auto replay = render(note, ioiMs, pickStyle);
            const char* stroke = pickStyle == PickStyle::Down
                ? "down" : "alternate";

            expect(first.audio.left == replay.audio.left
                       && first.audio.right == replay.audio.right,
                   "rapid palm mutes on note " + std::to_string(note)
                       + " are not sample-deterministic at "
                       + std::to_string(ioiMs) + " ms " + stroke);
            expect(allFinite(first.audio),
                   "rapid palm mutes on note " + std::to_string(note)
                       + " produced non-finite audio at "
                       + std::to_string(ioiMs) + " ms " + stroke);

            const auto [minimumPeak, maximumPeak] = std::minmax_element(
                first.peakDb.begin(), first.peakDb.end());
            const auto [minimumRms, maximumRms] = std::minmax_element(
                first.rmsDb.begin() + 1, first.rmsDb.end());
            const double earlyRms = mean(first.rmsDb, 1, 4);
            const double lateRms = mean(first.rmsDb, hitCount - 4, 4);
            const double driftDb = lateRms - earlyRms;

            const float sequencePeak = std::max(peakAbs(first.audio.left),
                                                peakAbs(first.audio.right));
            expect(*minimumPeak > -80.0 && sequencePeak < 0.80f,
                   "rapid palm-mute sequence on note " + std::to_string(note)
                       + " became silent or unbounded at "
                       + std::to_string(ioiMs) + " ms " + stroke + " ("
                       + std::to_string(*minimumPeak) + " dBFS quietest hit, "
                       + std::to_string(sequencePeak) + " whole-buffer peak)");
            // E2 Alternate at 60 ms is the widest current case at 11.91 dB;
            // 14 dB leaves two decibels for numeric movement while still
            // rejecting the 15.73 dB direct-contact candidate measured here.
            expect(*maximumPeak - *minimumPeak <= 14.0,
                   "rapid palm-mute peak spread on note "
                       + std::to_string(note) + " became unbounded at "
                       + std::to_string(ioiMs) + " ms " + stroke + " ("
                       + std::to_string(*maximumPeak - *minimumPeak) + " dB)");
            // Across E1/E2 and 25-125 ms the worst non-cold RMS spread is
            // 8.50 dB. Nine keeps a narrow guard because the rejected direct
            // contact operator already reached 9.10 dB on this same measure.
            expect(*maximumRms - *minimumRms <= 9.0,
                   "rapid palm-mute hit energy on note "
                       + std::to_string(note) + " left its bounded range at "
                       + std::to_string(ioiMs) + " ms " + stroke + " ("
                       + std::to_string(*maximumRms - *minimumRms)
                       + " dB RMS spread)");
            // The measured worst late-vs-early drift is +5.19 dB on E2 at
            // 30 ms. Six allows residual state to matter but rejects the
            // +7.05 dB drift of the direct-contact candidate.
            expect(std::abs(driftDb) <= 6.0,
                   "rapid palm mutes on note " + std::to_string(note)
                       + " progressively collapsed or built at "
                       + std::to_string(ioiMs) + " ms " + stroke + " ("
                       + std::to_string(driftDb) + " dB late-vs-early RMS)");

            std::cout << "PROBE rapid palm note " << note << ' ' << ioiMs
                      << " ms " << stroke
                      << ": " << *minimumPeak << ".." << *maximumPeak
                      << " dBFS peak, " << *minimumRms << ".." << *maximumRms
                      << " dBFS RMS, drift " << driftDb << " dB, whole peak "
                      << decibels(sequencePeak) << " dBFS\n";

            if (pickStyle == PickStyle::Down)
            {
                const double coldError = first.rmsDb[0]
                    - isolatedHitRmsDb(note, ioiMs, 0);
                expect(std::abs(coldError) < 1.0e-9,
                       "cold phrase hit did not match its isolated stroke");

                // Hit zero is the identity check above, not a repick. Score the
                // eleven attacks that actually meet an already-vibrating string.
                double meanError = 0.0;
                double worstAbsoluteError = 0.0;
                for (int hit = 1; hit < hitCount; ++hit)
                {
                    const double error = first.rmsDb[static_cast<std::size_t>(hit)]
                        - isolatedHitRmsDb(note, ioiMs, hit);
                    meanError += error;
                    worstAbsoluteError = std::max(worstAbsoluteError,
                                                  std::abs(error));
                }
                meanError /= static_cast<double>(hitCount - 1);
                repickMeanErrors[repickCell] = std::abs(meanError);
                repickWorstErrors[repickCell] = worstAbsoluteError;
                ++repickCell;
                std::cout << "PROBE rapid palm repick-state note " << note
                          << ' ' << ioiMs << " ms down: mean " << meanError
                          << " dB, worst absolute " << worstAbsoluteError
                          << " dB\n";
            }
        }
    }


    const auto median = [] (auto values)
    {
        std::sort(values.begin(), values.end());
        const std::size_t middle = values.size() / 2;
        return values.size() % 2 == 0
            ? 0.5 * (values[middle - 1] + values[middle])
            : values[middle];
    };
    expect(repickCell == repickCellCount,
           "rapid palm repick-state matrix did not cover every E1/E2 cell");
    std::cout << "PROBE rapid palm repick-state aggregate: median absolute mean "
              << median(repickMeanErrors) << " dB, median worst absolute "
              << median(repickWorstErrors) << " dB\n";
}

void testScoreMatchedC2PalmProxy()
{
    // HiMMP's CC-BY "In Solitude" multitrack provides four independent raw
    // Drop-C rhythm DIs and a score that marks this exact low-C2 Palm pattern:
    // four quarter-note strikes at 0, 300, 600 and 900 ms in bar 18 at
    // 200 BPM. It is a conventional six-string proxy, not an E1 eight-string
    // fit target, but matching its repick schedule gives the dry engine a
    // useful behavioural audit without importing or redistributing third-party
    // audio.
    constexpr double sampleRate = 44100.0;
    constexpr int hitCount = 4;
    constexpr int note = 36;
    constexpr int tailSamples = static_cast<int>(0.120 * sampleRate);
    constexpr std::array<int, hitCount> hitStarts {{
        0,
        static_cast<int>(0.300 * sampleRate),
        static_cast<int>(0.600 * sampleRate),
        static_cast<int>(0.900 * sampleRate)
    }};
    constexpr int totalSamples = hitStarts.back() + tailSamples;

    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    // The score does not identify stroke direction, so keep one repeatable
    // down-pick control; the separate rapid-Palm grid covers Alternate.
    engine.noteOn(pickKeyswitch(PickStyle::Down), 1.0f);
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);

    StereoBuffer phrase(totalSamples);
    int cursor = 0;
    for (const int start : hitStarts)
    {
        if (start > cursor)
            engine.process(phrase.left.data() + cursor,
                           phrase.right.data() + cursor, start - cursor);
        engine.noteOn(note, 0.90f);
        cursor = start;
    }
    engine.process(phrase.left.data() + cursor,
                   phrase.right.data() + cursor, totalSamples - cursor);
    expect(allFinite(phrase), "score-matched C2 Palm proxy was not finite");

    constexpr int shapePoints = 104;
    constexpr int halfWindow = static_cast<int>(0.0075 * sampleRate);
    constexpr int firstCentre = static_cast<int>(0.008 * sampleRate);
    constexpr int shapeStep = static_cast<int>(0.001 * sampleRate);
    std::array<std::array<double, shapePoints>, hitCount> shapes {};
    std::array<double, hitCount> bodyDb {};
    std::array<double, hitCount> tailDb {};
    std::array<double, hitCount> contractionDb {};

    const auto bandPowers = [&] (int start, int length)
    {
        std::array<double, 2> power {};
        constexpr double fundamentalHz = 65.40639133;
        for (int partial = 1; partial <= 64; ++partial)
        {
            const double frequency = fundamentalHz * partial;
            if (frequency >= 2600.0)
                break;
            const double magnitude = scannedPartialMagnitude(
                phrase.left, start, length, sampleRate,
                fundamentalHz, partial);
            power[frequency < 500.0 ? 0u : 1u] += magnitude * magnitude;
        }
        return power;
    };
    const auto upperShare = [] (const std::array<double, 2>& power)
    {
        return power[1] / std::max(power[0] + power[1], 1.0e-30);
    };

    for (int hit = 0; hit < hitCount; ++hit)
    {
        const int start = hitStarts[static_cast<std::size_t>(hit)];
        const int onsetEnd = start + static_cast<int>(0.030 * sampleRate);
        const int bodyEnd = start + static_cast<int>(0.080 * sampleRate);
        const int tailEnd = start + tailSamples;
        const double onsetRms = rmsInRange(phrase.left, start, onsetEnd);
        bodyDb[static_cast<std::size_t>(hit)] = decibels(
            rmsInRange(phrase.left, onsetEnd, bodyEnd)
            / std::max(onsetRms, 1.0e-15));
        tailDb[static_cast<std::size_t>(hit)] = decibels(
            rmsInRange(phrase.left, bodyEnd, tailEnd)
            / std::max(onsetRms, 1.0e-15));
        const auto onsetPower = bandPowers(start, onsetEnd - start);
        const auto bodyPower = bandPowers(onsetEnd, bodyEnd - onsetEnd);
        contractionDb[static_cast<std::size_t>(hit)] = decibels(std::sqrt(
            upperShare(bodyPower)
            / std::max(upperShare(onsetPower), 1.0e-30)));

        for (int point = 0; point < shapePoints; ++point)
        {
            const int centre = start + firstCentre + point * shapeStep;
            shapes[static_cast<std::size_t>(hit)]
                  [static_cast<std::size_t>(point)] =
                rmsInRange(phrase.left, centre - halfWindow,
                            centre + halfWindow)
                / std::max(onsetRms, 1.0e-15);
        }
    }

    const auto correlation = [] (const auto& a, const auto& b)
    {
        double meanA = 0.0, meanB = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            meanA += a[i];
            meanB += b[i];
        }
        meanA /= static_cast<double>(a.size());
        meanB /= static_cast<double>(b.size());
        double covariance = 0.0, powerA = 0.0, powerB = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            const double da = a[i] - meanA;
            const double db = b[i] - meanB;
            covariance += da * db;
            powerA += da * da;
            powerB += db * db;
        }
        return covariance / std::sqrt(std::max(powerA * powerB, 1.0e-30));
    };
    std::array<double, 6> correlations {};
    int pair = 0;
    for (int first = 0; first < hitCount; ++first)
        for (int second = first + 1; second < hitCount; ++second)
            correlations[static_cast<std::size_t>(pair++)] = correlation(
                shapes[static_cast<std::size_t>(first)],
                shapes[static_cast<std::size_t>(second)]);
    std::sort(correlations.begin(), correlations.end());
    const double medianCorrelation =
        0.5 * (correlations[2] + correlations[3]);

    std::cout << "PROBE score-matched C2 Palm four-hit envelope correlation "
              << medianCorrelation << "; body/onset ";
    for (const double value : bodyDb)
        std::cout << value << ' ';
    std::cout << "dB; tail/onset ";
    for (const double value : tailDb)
        std::cout << value << ' ';
    std::cout << "dB; upper-share contraction ";
    for (const double value : contractionDb)
        std::cout << value << ' ';
    std::cout << "dB\n";
}

void testExtendedRangeMutedMatrixProxy()
{
    // Freesound's CC0 "50hz-guitar" pack supplies two bridge-pickup takes of
    // matched sustained/muted 50, 75 and 100 Hz notes from a tagged seven-
    // string baritone. The uploader does not identify which hand performs the
    // mute, so these remain direction-only low-register probes rather than
    // Palm calibration rails. MIDI 31/38/43 are the nearest playable G1/D2/G2
    // notes and keep the comparison on wound strings at nearly full scale.
    constexpr double sampleRate = 48000.0;
    constexpr int onsetLength = static_cast<int>(0.050 * sampleRate);
    constexpr std::array<std::array<int, 2>, 4> windows {{
        std::array<int, 2> { static_cast<int>(0.050 * sampleRate),
                             static_cast<int>(0.150 * sampleRate) },
        std::array<int, 2> { static_cast<int>(0.150 * sampleRate),
                             static_cast<int>(0.500 * sampleRate) },
        std::array<int, 2> { static_cast<int>(0.500 * sampleRate),
                             static_cast<int>(1.000 * sampleRate) },
        std::array<int, 2> { static_cast<int>(1.000 * sampleRate),
                             static_cast<int>(2.000 * sampleRate) }
    }};

    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    for (const int note : { 31, 38, 43 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        const auto open = renderNote(engine, sampleRate, note, 0.90f,
                                     PlayStyle::Sustain, 2.0);
        const auto muted = renderNote(engine, sampleRate, note, 0.90f,
                                      PlayStyle::PalmMute, 2.0);
        expect(allFinite(open) && allFinite(muted),
               "extended-range muted-matrix proxy was not finite on note "
                   + std::to_string(note));

        const double openOnset = rmsInRange(open.left, 0, onsetLength);
        const double mutedOnset = rmsInRange(muted.left, 0, onsetLength);
        std::array<double, windows.size()> pairedDecay {};
        for (std::size_t i = 0; i < windows.size(); ++i)
        {
            const auto& window = windows[i];
            const double openDecay = decibels(
                rmsInRange(open.left, window[0], window[1])
                / std::max(openOnset, 1.0e-15));
            const double mutedDecay = decibels(
                rmsInRange(muted.left, window[0], window[1])
                / std::max(mutedOnset, 1.0e-15));
            pairedDecay[i] = mutedDecay - openDecay;
        }

        const auto upperShare = [&] (const StereoBuffer& audio,
                                     int start, int length)
        {
            const double fundamentalHz = midiHz(note);
            std::array<double, 2> power {};
            for (int partial = 1; partial <= 80; ++partial)
            {
                const double frequency = fundamentalHz * partial;
                if (frequency >= 2600.0)
                    break;
                const double magnitude = scannedPartialMagnitude(
                    audio.left, start, length, sampleRate,
                    fundamentalHz, partial);
                power[frequency < 500.0 ? 0u : 1u] += magnitude * magnitude;
            }
            return power[1] / std::max(power[0] + power[1], 1.0e-30);
        };
        constexpr int spectralOnsetLength = static_cast<int>(0.060 * sampleRate);
        constexpr int spectralBodyStart = spectralOnsetLength;
        constexpr int spectralBodyLength = static_cast<int>(0.100 * sampleRate);
        const double openContraction = decibels(std::sqrt(
            upperShare(open, spectralBodyStart, spectralBodyLength)
            / std::max(upperShare(open, 0, spectralOnsetLength), 1.0e-30)));
        const double mutedContraction = decibels(std::sqrt(
            upperShare(muted, spectralBodyStart, spectralBodyLength)
            / std::max(upperShare(muted, 0, spectralOnsetLength), 1.0e-30)));

        std::cout << "PROBE extended-range muted matrix note " << note
                  << " paired decay ";
        for (const double value : pairedDecay)
            std::cout << value << ' ';
        std::cout << "dB; paired 0-60 -> 60-160 ms upper-share contraction "
                  << mutedContraction - openContraction << " dB\n";
    }
}

void testPalmHandLossStartsEngaged()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(28, 0.90f);

    const int stringIndex = TestAccess::stringForNote(engine, 28);
    expect(stringIndex >= 0,
           "immediate palm-contact fixture did not allocate open E1");
    if (stringIndex < 0)
        return;

    const float solved = TestAccess::solvedHandLossDepth(engine, stringIndex);
    StereoBuffer firstHostSample(1);
    renderInto(engine, firstHostSample);
    const float firstTick = TestAccess::handLossDepth(engine, stringIndex);

    float greatestLater = 0.0f;
    // Cumulative checkpoints at 1, 10, 40 and 80 ms cover the old fade-in
    // interval and the beginning of the permitted energy-driven relaxation.
    for (const int samples : { 47, 432, 1440, 1920 })
    {
        StereoBuffer later(samples);
        renderInto(engine, later);
        greatestLater = std::max(
            greatestLater, TestAccess::handLossDepth(engine, stringIndex));
    }

    expect(solved > 0.0f,
           "palm style did not engage the hand-loss dip");
    expect(std::abs(firstTick - solved) < 1.0e-6f,
           "palm hand-loss dip was not fully engaged at the first control tick ("
               + std::to_string(firstTick) + " of "
               + std::to_string(solved) + ")");
    expect(greatestLater <= firstTick + 1.0e-6f,
           "palm hand-loss dip grew with note age ("
               + std::to_string(firstTick) + " at first tick, "
               + std::to_string(greatestLater) + " later)");
    std::cout << "PROBE palm hand-loss depth: " << firstTick
              << " at first tick, greatest later " << greatestLater << '\n';
    expect(TestAccess::touchDepth(engine, stringIndex) == 0.0f,
           "Palm allocated a point-touch transient instead of steady hand loss");
}

void testPalmAttackContactsCompose()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::array<float, 5> pressures {
        0.0f, 0.25f, 0.50f, 0.75f, 1.0f
    };
    constexpr std::array<float, pressures.size()> expectedPalmDepth {
        0.7975f, 0.848125f, 0.89875f, 0.949375f, 1.0f
    };

    struct AttackState
    {
        float impact;
        float pulseCoefficient;
        float noiseCoefficient;
    };
    const auto inspect = [] (PlayStyle style, float pressure)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.strumSpreadSeconds = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.setPalmMutePressure(pressure);
        engine.noteOn(styleKeyswitch(style), 1.0f);
        engine.noteOn(28, 0.90f);
        const int stringIndex = TestAccess::stringForNote(engine, 28);
        expect(stringIndex >= 0,
               "stacked attack-contact fixture did not allocate E1");
        if (stringIndex < 0)
            return AttackState {};
        return AttackState {
            TestAccess::palmImpactVelocity(engine, stringIndex),
            TestAccess::excitationPulseCoefficient(engine, stringIndex),
            TestAccess::noiseBandCoefficient(engine, stringIndex)
        };
    };

    std::array<AttackState, pressures.size()> palm {};
    for (std::size_t i = 0; i < pressures.size(); ++i)
        palm[i] = inspect(PlayStyle::PalmMute, pressures[i]);

    const float impactPerDepth = palm.front().impact / expectedPalmDepth.front();
    for (std::size_t i = 0; i < palm.size(); ++i)
    {
        expect(std::abs(palm[i].impact
                        - impactPerDepth * expectedPalmDepth[i]) < 1.0e-6f,
               "Palm style and Palm Pressure did not compose at "
                   + std::to_string(pressures[i]));
        if (i > 0)
        {
            expect(palm[i].impact > palm[i - 1].impact
                       && palm[i].pulseCoefficient > palm[i - 1].pulseCoefficient
                       && palm[i].noiseCoefficient > palm[i - 1].noiseCoefficient,
                   "Palm Pressure did not continuously tighten the Palm attack");
        }
    }

    // Dead's light fretting-hand contact is separate from the bridge hand.
    // Ten-percent bridge pressure is below the old max() threshold, so these
    // coefficients move only when the two contacts genuinely compose.
    const auto deadOpen = inspect(PlayStyle::Dead, 0.0f);
    const auto deadPressed = inspect(PlayStyle::Dead, 0.10f);
    expect(deadPressed.pulseCoefficient > deadOpen.pulseCoefficient
               && deadPressed.noiseCoefficient > deadOpen.noiseCoefficient,
           "Dead's fretting and bridge hands replaced rather than composed");
}

void testPalmImpactIsSampleRateInvariant()
{
    std::array<double, 4> decaysDb {};
    int index = 0;

    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
        engine.noteOn(28, 0.90f);

        const int stringIndex = TestAccess::stringForNote(engine, 28);
        expect(stringIndex >= 0,
               "palm-impact rate fixture did not allocate open E1");
        if (stringIndex < 0)
            continue;
        const float initial = TestAccess::palmImpactVelocity(engine, stringIndex);
        StereoBuffer firstFiveMilliseconds(
            static_cast<int>(0.005 * sampleRate));
        renderInto(engine, firstFiveMilliseconds);
        const float remaining =
            TestAccess::palmImpactVelocity(engine, stringIndex);
        expect(initial > 0.0f && remaining > 0.0f,
               "palm-impact rate fixture lost its velocity state");
        const double decayDb = decibels(
            static_cast<double>(remaining) / std::max(initial, 1.0e-15f));
        decaysDb[static_cast<std::size_t>(index++)] = decayDb;
        // 0.992 retention at the 48 kHz calibration clock is -16.744 dB in
        // five milliseconds. The 44.1 kHz frame count truncates by one tenth
        // of a sample, accounting for its small deterministic difference.
        expect(std::abs(decayDb + 16.744) < 0.10,
               "palm-impact lifetime depends on the host sample rate at "
                   + std::to_string(sampleRate) + " Hz ("
                   + std::to_string(decayDb) + " dB after 5 ms)");
    }

    const auto [minimum, maximum] = std::minmax_element(
        decaysDb.begin(), decaysDb.end());
    expect(*maximum - *minimum < 0.10,
           "palm-impact five-millisecond decay spread across host rates is "
               + std::to_string(*maximum - *minimum) + " dB");
}

void testPalmImpactWaitsForStrokeAndClears()
{
    constexpr double sampleRate = 48000.0;
    EngineParameters parameters;
    parameters.strumSpreadSeconds = 0.040f;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    // A delayed pick contact is an audio-rate event, not a control-rate one.
    // Stop one host sample short of the scheduled boundary, cross it one sample
    // at a time, and require the Palm impact to arm only after every pending
    // internal sample has actually rendered silent. This catches both a coarse
    // control-tick countdown and an off-by-one at the exact contact sample.
    for (const double timingRate : { 44100.0, 48000.0, 96000.0 })
    {
        ElectryEngine timed;
        timed.prepare(timingRate, 512);
        timed.setParameters(parameters);
        timed.reset();
        timed.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
        timed.noteOn(28, 0.90f);

        const int stringIndex = TestAccess::stringForNote(timed, 28);
        expect(stringIndex >= 0,
               "sample-accurate Palm-contact fixture did not allocate E1 at "
                   + std::to_string(timingRate) + " Hz");
        if (stringIndex < 0)
            continue;

        const int factor = TestAccess::oversamplingFactor(timed);
        const int delay = TestAccess::snapshot(timed, stringIndex).startDelaySamples;
        expect(delay > factor && delay % factor == 0,
               "sample-accurate Palm-contact fixture did not land on a host "
               "sample at " + std::to_string(timingRate) + " Hz");
        if (delay <= factor || delay % factor != 0)
            continue;

        StereoBuffer beforeBoundary(delay / factor - 1);
        renderInto(timed, beforeBoundary);
        expect(TestAccess::snapshot(timed, stringIndex).startDelaySamples == factor,
               "delayed Palm countdown advanced before real samples elapsed at "
                   + std::to_string(timingRate) + " Hz");
        expect(TestAccess::palmImpactVelocity(timed, stringIndex) == 0.0f,
               "Palm impact armed before its last pending host sample at "
                   + std::to_string(timingRate) + " Hz");
        expect(peakAbs(beforeBoundary.left) == 0.0f,
               "delayed Palm stroke sounded before its contact boundary at "
                   + std::to_string(timingRate) + " Hz");

        StereoBuffer boundary(1);
        renderInto(timed, boundary, 1);
        expect(TestAccess::snapshot(timed, stringIndex).startDelaySamples == 0,
               "delayed Palm countdown missed its exact contact boundary at "
                   + std::to_string(timingRate) + " Hz");
        expect(TestAccess::palmImpactVelocity(timed, stringIndex) > 0.0f,
               "Palm impact did not arm at its exact contact boundary at "
                   + std::to_string(timingRate) + " Hz");
        expect(peakAbs(boundary.left) == 0.0f,
               "Palm excitation leaked into the sample preceding contact at "
                   + std::to_string(timingRate) + " Hz");
    }

    // Attack-side fret/rattle opportunity starts with the attack as well. A
    // long strum pre-roll must not spend this counter against a silent string
    // and leave a delayed Palm contact artificially clean.
    {
        ElectryEngine artifactTimed;
        auto artifactParameters = parameters;
        artifactParameters.artifactAmount = 1.0f;
        artifactTimed.prepare(sampleRate, 512);
        artifactTimed.setParameters(artifactParameters);
        artifactTimed.reset();
        artifactTimed.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
        artifactTimed.noteOn(28, 0.90f);
        const int stringIndex = TestAccess::stringForNote(artifactTimed, 28);
        const auto scheduled = TestAccess::snapshot(artifactTimed, stringIndex);
        const int factor = TestAccess::oversamplingFactor(artifactTimed);
        expect(scheduled.startDelaySamples > 0
                   && scheduled.startDelaySamples % factor == 0,
               "delayed Palm-artifact fixture was not configured");
        StereoBuffer toContact(scheduled.startDelaySamples / factor);
        renderInto(artifactTimed, toContact);
        const auto atContact = TestAccess::snapshot(artifactTimed, stringIndex);
        expect(atContact.artifactCollisionLength > 0
                   && atContact.artifactCollisionRemaining
                   == atContact.artifactCollisionLength,
               "a delayed Palm stroke spent its artifact window before contact");
    }

    ElectryEngine delayed;
    auto delayedParameters = parameters;
    delayedParameters.sympatheticAmount = 0.80f;
    delayed.prepare(sampleRate, 512);
    delayed.setParameters(delayedParameters);
    delayed.reset();
    delayed.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    delayed.noteOn(28, 0.90f);
    const int delayedString = TestAccess::stringForNote(delayed, 28);
    expect(TestAccess::snapshot(delayed, delayedString).startDelaySamples > 0,
           "delayed Palm-impact fixture did not schedule a strum pre-roll");
    expect(TestAccess::palmImpactVelocity(delayed, delayedString) == 0.0f,
           "Palm impact armed before the delayed pick reached the string");

    StereoBuffer beforePick(static_cast<int>(0.010 * sampleRate));
    renderInto(delayed, beforePick);
    expect(peakAbs(beforePick.left) == 0.0f,
           "a delayed Palm stroke made sound before its pick contact");
    expect(TestAccess::sympatheticHandGainTarget(delayed) == 1.0f,
           "a delayed Palm allocation moved the shared hand before contact");
    StereoBuffer throughPick(static_cast<int>(0.020 * sampleRate));
    renderInto(delayed, throughPick);
    expect(peakAbs(throughPick.left) > 1.0e-5f,
           "the delayed Palm stroke never sounded at its pick contact");

    ElectryEngine aborted;
    aborted.prepare(sampleRate, 512);
    aborted.setParameters(parameters);
    aborted.reset();
    aborted.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    aborted.noteOn(28, 0.90f);
    const int abortedString = TestAccess::stringForNote(aborted, 28);
    aborted.noteOff(28);
    expect(abortedString >= 0
               && ! TestAccess::snapshot(aborted, abortedString).active,
           "an aborted never-contacted Palm voice remained active");
    StereoBuffer cancelled(static_cast<int>(0.060 * sampleRate));
    renderInto(aborted, cancelled);
    expect(peakAbs(cancelled.left) == 0.0f,
           "an aborted delayed Palm stroke left a pre-contact thud");

    // A silent cancelled allocation must not behave like a hand left across
    // all eight strings. Before immediate retirement it kept the full Palm
    // style in the shared scan for the 50 ms voice-retirement floor, imposing
    // about 27 dB of unintended decay on an already-ringing bank at 48 kHz.
    {
        ElectryEngine shared;
        auto sharedParameters = parameters;
        sharedParameters.sympatheticAmount = 0.80f;
        shared.prepare(sampleRate, 512);
        shared.setParameters(sharedParameters);
        shared.reset();
        shared.noteOn(45, 0.90f);
        StereoBuffer establish(static_cast<int>(0.050 * sampleRate));
        renderInto(shared, establish);
        shared.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
        shared.noteOn(28, 0.90f);
        const int cancelledString = TestAccess::stringForNote(shared, 28);
        expect(cancelledString >= 0
                   && TestAccess::snapshot(shared, cancelledString)
                          .startDelaySamples > 0,
               "shared-mute cancellation fixture did not schedule E1");
        shared.noteOff(28);
        StereoBuffer nextControlTick(32);
        renderInto(shared, nextControlTick, 1);
        expect(TestAccess::sympatheticHandGainTarget(shared) == 1.0f,
               "a never-contacted Palm voice choked the shared string bank");

        // Repeating the Note On during that same silent pre-roll still has no
        // preceding stroke to preserve. Both matching offs must retire it now,
        // rather than leaving a phantom Palm hand for the release floor.
        shared.reset();
        shared.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
        shared.noteOn(28, 0.90f);
        shared.noteOn(28, 0.90f);
        const int duplicateString = TestAccess::stringForNote(shared, 28);
        expect(duplicateString >= 0
                   && TestAccess::snapshot(shared, duplicateString)
                          .startDelaySamples > 0,
               "duplicate pre-roll cancellation fixture was not pending");
        shared.noteOff(28);
        shared.noteOff(28);
        expect(duplicateString >= 0
                   && ! TestAccess::snapshot(shared, duplicateString).active,
               "duplicate never-contacted Palm notes preserved a phantom ring");
        renderInto(shared, nextControlTick, 1);
        expect(TestAccess::sympatheticHandGainTarget(shared) == 1.0f,
               "duplicate never-contacted Palm notes choked the shared bank");
    }

    // Cancelling a delayed same-string repick must not erase the preceding
    // stroke that is still physically ringing in that voice.
    {
        ElectryEngine repick;
        repick.prepare(sampleRate, 512);
        repick.setParameters(parameters);
        repick.reset();
        repick.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
        repick.noteOn(28, 0.90f);
        StereoBuffer ringing(static_cast<int>(0.080 * sampleRate));
        renderInto(repick, ringing);
        repick.noteOn(28, 0.90f);
        const int repickedString = TestAccess::stringForNote(repick, 28);
        expect(repickedString >= 0
                   && TestAccess::snapshot(repick, repickedString)
                          .startDelaySamples > 0,
               "cancelled-repick fixture did not schedule a delayed contact");
        repick.noteOff(28);
        repick.noteOff(28);
        expect(repickedString >= 0
                   && TestAccess::snapshot(repick, repickedString).active,
               "cancelling a delayed repick erased the preceding ringing stroke");
    }

    // Continuous Palm Pressure still loads and damps a hammered string, but
    // the attack itself comes from the fretting hand. It must not fabricate
    // the short picking-hand collision reserved for a real plectrum contact.
    parameters.strumSpreadSeconds = 0.0f;
    aborted.reset();
    aborted.setParameters(parameters);
    aborted.setPalmMutePressure(1.0f);
    aborted.noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
    aborted.noteOn(45, 0.90f);
    const int hammeredString = TestAccess::stringForNote(aborted, 45);
    expect(hammeredString >= 0,
           "Palm-pressure Hammer fixture did not allocate its string");
    expect(hammeredString < 0
               || TestAccess::palmImpactVelocity(aborted, hammeredString) == 0.0f,
           "Palm Pressure fabricated a plectrum impact on a Hammer attack");

    auto tailStorage = std::make_unique<ElectryEngine>();
    auto& tail = *tailStorage;
    tail.prepare(sampleRate, 512);
    tail.setParameters(parameters);
    tail.reset();
    tail.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    tail.noteOn(28, 0.90f);
    StereoBuffer settle(static_cast<int>(0.100 * sampleRate));
    renderInto(tail, settle);
    const int tailString = TestAccess::stringForNote(tail, 28);
    expect(TestAccess::palmImpactVelocity(tail, tailString) == 0.0f
               && TestAccess::palmImpactState(tail, tailString) == 0.0f,
           "Palm impact abandoned nonzero state at its render cutoff");

    // The bridge/body thud outlives its short velocity drive. A repick adds a
    // new contact but cannot erase that already-moving filter state in zero
    // time, which used to notch rapid Palm attacks at every Note On.
    auto overlappingStorage = std::make_unique<ElectryEngine>();
    auto& overlapping = *overlappingStorage;
    overlapping.prepare(sampleRate, 512);
    overlapping.setParameters(parameters);
    overlapping.reset();
    overlapping.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    overlapping.noteOn(28, 0.90f);
    StereoBuffer fiveMilliseconds(static_cast<int>(0.005 * sampleRate));
    renderInto(overlapping, fiveMilliseconds);
    const int overlappingString = TestAccess::stringForNote(overlapping, 28);
    const float stateBeforeRepick = TestAccess::palmImpactState(
        overlapping, overlappingString);
    expect(stateBeforeRepick > 0.0f,
           "overlapping Palm-impact fixture had no live filter tail");
    overlapping.noteOn(28, 0.95f);
    expect(TestAccess::palmImpactState(overlapping, overlappingString)
               == stateBeforeRepick,
           "a rapid Palm repick erased the live impact-filter tail");
}

// ---------------------------------------------------------------------------
// Independent-player timing and strum travel
// ---------------------------------------------------------------------------

void testSeededPlayerTiming()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::uint32_t secondPlayerSeed = 0x9e3779b9u;

    const auto makeEngine = [] (std::uint32_t seed, float spreadSeconds = 0.0f)
    {
        auto engine = std::make_unique<ElectryEngine>();
        engine->setVariationSeed(seed);
        engine->prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.strumSpreadSeconds = spreadSeconds;
        engine->setParameters(parameters);
        engine->reset();
        return engine;
    };

    // The established/default player has no hidden onset offset. This is the
    // exact bypass that keeps every Mono/Stereo render on its old clock.
    auto primary = makeEngine(0u);
    primary->noteOn(28, 0.9f);
    expect(TestAccess::chordPerformanceDelaySamples(*primary) == 0
               && TestAccess::snapshot(*primary, 0).startDelaySamples == 0,
           "the default player acquired a hidden timing delay");

    const auto timingSequence = [&] (std::uint32_t seed)
    {
        auto engine = makeEngine(seed);
        std::array<int, 64> delays {};
        const int maximumDelay = static_cast<int>(std::lround(
            0.006 * TestAccess::internalSampleRate(*engine)));
        for (std::size_t attack = 0; attack < delays.size(); ++attack)
        {
            engine->noteOn(28, 0.9f);
            const int delay = TestAccess::chordPerformanceDelaySamples(*engine);
            delays[attack] = delay;
            const int stringIndex = TestAccess::stringForNote(*engine, 28);
            expect(delay >= 0 && delay <= maximumDelay,
                   "the second player's timing draw left the 0-6 ms bound ("
                       + std::to_string(delay) + " of "
                       + std::to_string(maximumDelay) + " internal samples)");
            expect(stringIndex >= 0
                       && TestAccess::snapshot(*engine, stringIndex)
                              .startDelaySamples == delay,
                   "a zero-spread pick did not receive its player's timing draw");

            // More than the maximum delay, at the host rate: the pending pick
            // lands before its matching release and the next stroke begins on
            // a later engine clock.
            StereoBuffer contact(static_cast<int>(0.008 * sampleRate));
            renderInto(*engine, contact);
            engine->noteOff(28);
            StereoBuffer gap(static_cast<int>(0.008 * sampleRate));
            renderInto(*engine, gap);
        }
        return delays;
    };

    const auto firstSequence = timingSequence(secondPlayerSeed);
    const auto repeatedSequence = timingSequence(secondPlayerSeed);
    const auto otherPlayerSequence = timingSequence(0x243f6a88u);
    expect(firstSequence == repeatedSequence,
           "the second player's timing stream was not deterministic");
    const auto [minimumDelay, maximumDelay] = std::minmax_element(
        firstSequence.begin(), firstSequence.end());
    expect(*minimumDelay != *maximumDelay,
           "the second player's per-stroke timing never moved");
    expect(firstSequence != otherPlayerSequence,
           "different players received the same timing stream");

    // One wrist stroke crosses a chord: it has one player offset, then any
    // requested strum travel. Canonical chord batching must not draw once per
    // string.
    auto chord = makeEngine(secondPlayerSeed);
    const std::array<ElectryEngine::NoteOnEvent, 3> chordEvents {
        ElectryEngine::NoteOnEvent { 28, 0.9f },
        ElectryEngine::NoteOnEvent { 40, 0.9f },
        ElectryEngine::NoteOnEvent { 45, 0.9f }
    };
    chord->noteOnChord(chordEvents);
    const int chordDelay = TestAccess::chordPerformanceDelaySamples(*chord);
    for (const int note : { 28, 40, 45 })
    {
        const int stringIndex = TestAccess::stringForNote(*chord, note);
        expect(stringIndex >= 0
                   && TestAccess::snapshot(*chord, stringIndex)
                          .startDelaySamples == chordDelay,
               "one block chord received more than one player-timing offset");
    }

    // Re-anchoring a strum against a later-discovered low edge recomputes its
    // absolute onset. It must retain, rather than drop or double, the same
    // player offset on every picked member.
    constexpr float spreadSeconds = 0.020f;
    auto strummed = makeEngine(secondPlayerSeed, spreadSeconds);
    strummed->noteOn(50, 0.9f); // provisional middle-string anchor
    const int strumPlayerDelay =
        TestAccess::chordPerformanceDelaySamples(*strummed);
    strummed->noteOn(28, 0.9f); // the real low edge, delivered at the same clock
    const int preRoll = static_cast<int>(std::lround(
        spreadSeconds * TestAccess::internalSampleRate(*strummed)));
    int firstStringDelay = std::numeric_limits<int>::max();
    for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount;
         ++stringIndex)
    {
        const auto snapshot = TestAccess::snapshot(*strummed, stringIndex);
        if (snapshot.active)
            firstStringDelay = std::min(firstStringDelay,
                                        snapshot.startDelaySamples);
    }
    expect(std::abs(firstStringDelay - preRoll - strumPlayerDelay) <= 1,
           "strum re-anchoring lost or duplicated the player timing offset ("
               + std::to_string(firstStringDelay) + " vs "
               + std::to_string(preRoll + strumPlayerDelay) + ")");

    // A key lifted before the delayed physical contact never produces a late
    // phantom pick. This is the ownership edge a processor-side MIDI delay
    // would otherwise have to duplicate.
    auto cancelled = makeEngine(secondPlayerSeed);
    cancelled->noteOn(28, 0.9f);
    expect(TestAccess::snapshot(*cancelled, 0).startDelaySamples > 0,
           "the cancellation fixture did not receive a timing delay");
    cancelled->noteOff(28);
    StereoBuffer cancelledAudio(static_cast<int>(0.020 * sampleRate));
    renderInto(*cancelled, cancelledAudio);
    expect(cancelled->getActiveVoiceCount() == 0
               && peakAbs(cancelledAudio.left) == 0.0f
               && peakAbs(cancelledAudio.right) == 0.0f,
           "a released pre-contact player delay produced a late attack");

    // Hammer/tap timing belongs to the fretting hand. A separate player's
    // plectrum pocket must not delay a fresh hammer articulation.
    auto hammer = makeEngine(secondPlayerSeed, 0.020f);
    hammer->noteOn(styleKeyswitch(PlayStyle::Hammer), 1.0f);
    hammer->noteOn(45, 0.9f);
    const int hammerString = TestAccess::stringForNote(*hammer, 45);
    const auto hammerContact = TestAccess::snapshot(*hammer, hammerString);
    expect(hammerString >= 0 && hammerContact.startDelaySamples == 0
               && TestAccess::chordPerformanceDelaySamples(*hammer) == 0,
           "the wrist scheduler delayed a fresh Hammer contact");
    expect(! hammerContact.excitationInContact
               && hammerContact.contactFeedbackGain == 1.0f,
           "a fresh Hammer entered the plectrum's contact-loss phase");
    StereoBuffer immediateHammer(static_cast<int>(0.005 * sampleRate));
    renderInto(*hammer, immediateHammer);
    expect(peakAbs(immediateHammer.left) > 1.0e-6f,
           "a fresh Hammer did not sound immediately with Strum Spread on");
}

void testStrumSpread()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::array<int, 6> chord { 28, 40, 45, 50, 55, 64 };

    const auto firstOnsetSample = [] (const std::vector<float>& data, float threshold)
    {
        for (int i = 0; i < static_cast<int>(data.size()); ++i)
            if (std::abs(data[static_cast<std::size_t>(i)]) > threshold)
                return i;
        return -1;
    };

    const auto renderChord = [&] (float spreadSeconds)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.strumSpreadSeconds = spreadSeconds;
        engine.setParameters(parameters);
        engine.reset();
        for (const int note : chord)
            engine.noteOn(note, 0.85f);

        std::array<int, ElectryEngine::stringCount> delays {};
        for (int s = 0; s < ElectryEngine::stringCount; ++s)
            delays[static_cast<std::size_t>(s)] =
                TestAccess::snapshot(engine, s).startDelaySamples;

        StereoBuffer buffer(static_cast<int>(0.6 * sampleRate));
        renderInto(engine, buffer);
        return std::make_pair(delays, std::move(buffer));
    };

    const auto block = renderChord(0.0f);
    for (const int delay : block.first)
        expect(delay == 0, "a chord at zero spread was not simultaneous");
    expect(allFinite(block.second), "block chord produced non-finite audio");

    const auto strummed = renderChord(0.020f);
    // The first string the pick meets fires after the re-anchor pre-roll every
    // voice of a strummed chord carries, not immediately: until that window
    // closes a later note-on may still turn out to be the edge the stroke
    // began at. Each further string is offset by the travel time on top of it,
    // in physical string order.
    const int preRoll = static_cast<int>(0.020 * sampleRate * 2.0); // internal 2x clock
    const int step = static_cast<int>(0.020f * sampleRate * 2.0f);
    expect(std::abs(strummed.first[0] - preRoll) <= 2,
           "the leading string of a strum did not fire one pre-roll after its "
           "note-on (" + std::to_string(strummed.first[0]) + " vs "
               + std::to_string(preRoll) + ")");
    // The pick accelerates through the strings, so the offsets no longer lie
    // on a straight line - but they still increase string by string, and the
    // knob still states the mean crossing time, so seven crossings still take
    // seven times it.
    int previous = strummed.first[0];
    for (const std::size_t stringIndex : { std::size_t { 2 }, std::size_t { 3 },
                                           std::size_t { 4 }, std::size_t { 5 },
                                           std::size_t { 7 } })
    {
        expect(strummed.first[stringIndex] > previous,
               "string " + std::to_string(stringIndex)
                   + " did not receive its strum travel offset ("
                   + std::to_string(strummed.first[stringIndex]) + " after "
                   + std::to_string(previous) + ")");
        previous = strummed.first[stringIndex];
    }
    const int travel = strummed.first[7] - strummed.first[0];
    expect(std::abs(travel - 7 * step) <= 7 * step / 20,
           "a strum across eight strings did not take seven times the spread ("
               + std::to_string(travel) + " vs " + std::to_string(7 * step) + ")");
    expect(allFinite(strummed.second) && peakAbs(strummed.second.left) < 0.80f,
           "strummed chord produced non-finite or unbounded audio");

    // The strum genuinely reaches the audio: the last string's attack arrives
    // measurably later than the chord's onset.
    const int blockOnset = firstOnsetSample(block.second.left, 0.02f);
    const int strumOnset = firstOnsetSample(strummed.second.left, 0.02f);
    expect(blockOnset >= 0 && strumOnset >= 0, "chord onset was not detectable");
    const double blockPeak = static_cast<double>(peakAbs(
        block.second.left, 0, static_cast<int>(0.03 * sampleRate)));
    const double strumPeak = static_cast<double>(peakAbs(
        strummed.second.left, 0, static_cast<int>(0.03 * sampleRate)));
    expect(strumPeak < blockPeak,
           "spreading a chord did not lower its stacked initial peak");

    // A noteOnChord group is different from the scalar fallback above: the
    // complete physical shape is known at one timestamp, so its leading edge
    // needs no 20 ms causal lookahead. Travel remains physical and reverses
    // with the stroke direction.
    const auto completeChord = [&] (PickStyle pick, int blockSize = 512)
    {
        ElectryEngine complete;
        complete.prepare(sampleRate, 512);
        EngineParameters spread;
        spread.artifactAmount = 0.0f;
        spread.sympatheticAmount = 0.0f;
        spread.strumSpreadSeconds = 0.020f;
        complete.setParameters(spread);
        complete.reset();
        complete.noteOn(pickKeyswitch(pick), 1.0f);

        std::array<ElectryEngine::NoteOnEvent, chord.size()> events {};
        for (std::size_t index = 0; index < chord.size(); ++index)
            events[index] = { chord[index], 0.85f };
        complete.noteOnChord(events);

        std::array<int, ElectryEngine::stringCount> delays {};
        for (int stringIndex = 0;
             stringIndex < ElectryEngine::stringCount; ++stringIndex)
            delays[static_cast<std::size_t>(stringIndex)] =
                TestAccess::snapshot(complete, stringIndex).startDelaySamples;
        StereoBuffer audio(static_cast<int>(0.6 * sampleRate));
        renderInto(complete, audio, blockSize);
        return std::make_pair(delays, std::move(audio));
    };

    const auto completeDown = completeChord(PickStyle::Down);
    const auto completeUp = completeChord(PickStyle::Up);
    expect(completeDown.first[0] == 0 && completeUp.first[7] == 0,
           "a complete strum batch retained the scalar path's re-anchor latency");
    const std::array<int, 6> playedStrings { 0, 2, 3, 4, 5, 7 };
    int downPrevious = -1;
    int upPrevious = std::numeric_limits<int>::max();
    for (const int stringIndex : playedStrings)
    {
        const int downDelay =
            completeDown.first[static_cast<std::size_t>(stringIndex)];
        const int upDelay =
            completeUp.first[static_cast<std::size_t>(stringIndex)];
        expect(downDelay > downPrevious,
               "a complete down-strum did not retain monotone string travel");
        expect(upDelay < upPrevious,
               "a complete up-strum did not retain reversed string travel");
        downPrevious = downDelay;
        upPrevious = upDelay;
    }
    expect(completeDown.first[7] - completeDown.first[0]
               == strummed.first[7] - strummed.first[0],
           "removing complete-batch pre-roll changed the wrist's travel time");

    // Rendering that scheduled batch in hostile client block sizes must not
    // move an onset or change a sample.
    const auto partitioned = completeChord(PickStyle::Down, 17);
    expect(partitioned.second.left == completeDown.second.left
               && partitioned.second.right == completeDown.second.right,
           "complete strum audio depended on render block partitioning");

    // A one-note complete batch has no strings to cross, so Strum Spread is an
    // exact timing and audio no-op. This is the common live-riff case that was
    // previously charged a fixed 20 ms before the instrument could respond.
    const auto completeSingle = [&] (float spreadSeconds)
    {
        ElectryEngine single;
        single.prepare(sampleRate, 512);
        EngineParameters singleParameters;
        singleParameters.artifactAmount = 0.0f;
        singleParameters.sympatheticAmount = 0.0f;
        singleParameters.strumSpreadSeconds = spreadSeconds;
        single.setParameters(singleParameters);
        single.reset();
        const std::array<ElectryEngine::NoteOnEvent, 1> event {{
            { 40, 0.85f }
        }};
        single.noteOnChord(event);
        const int delay = TestAccess::snapshot(
            single, TestAccess::stringForNote(single, 40)).startDelaySamples;
        StereoBuffer audio(4096);
        renderInto(single, audio, 31);
        return std::make_pair(delay, std::move(audio));
    };
    const auto flatSingle = completeSingle(0.0f);
    const auto spreadSingle = completeSingle(0.020f);
    expect(flatSingle.first == 0 && spreadSingle.first == 0,
           "a complete single-note batch acquired Strum Spread latency");
    expect(flatSingle.second.left == spreadSingle.second.left
               && flatSingle.second.right == spreadSingle.second.right,
           "Strum Spread changed a complete single-note attack");

    // Separate host timestamps are separate performed strokes even inside the
    // scalar chord window. Alternate must therefore advance, and the second
    // one-note batch must also begin without hidden pre-roll.
    {
        ElectryEngine timed;
        timed.prepare(sampleRate, 512);
        EngineParameters timedParameters;
        timedParameters.artifactAmount = 0.0f;
        timedParameters.sympatheticAmount = 0.0f;
        timedParameters.strumSpreadSeconds = 0.020f;
        timed.setParameters(timedParameters);
        timed.reset();
        timed.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
        const std::array<ElectryEngine::NoteOnEvent, 1> first {{ { 28, 0.85f } }};
        timed.noteOnChord(first);
        StereoBuffer tenMilliseconds(static_cast<int>(0.010 * sampleRate));
        renderInto(timed, tenMilliseconds);
        const std::array<ElectryEngine::NoteOnEvent, 1> second {{ { 40, 0.85f } }};
        timed.noteOnChord(second);
        const int secondString = TestAccess::stringForNote(timed, 40);
        const auto secondVoice = TestAccess::snapshot(timed, secondString);
        expect(secondVoice.strokeIsUp && secondVoice.startDelaySamples == 0,
               "a later complete batch merged into the preceding stroke");
    }

    // Releasing an un-crossed string cancels only that future contact. With
    // the same low-string stroke left behind, the result is sample-identical
    // to a one-string batch rather than growing a late phantom attack.
    const auto cancelledCrossing = [&] (bool scheduleHighString)
    {
        ElectryEngine cancelled;
        cancelled.prepare(sampleRate, 512);
        EngineParameters cancelParameters;
        cancelParameters.artifactAmount = 0.0f;
        cancelParameters.sympatheticAmount = 0.0f;
        cancelParameters.strumSpreadSeconds = 0.040f;
        cancelled.setParameters(cancelParameters);
        cancelled.reset();
        if (scheduleHighString)
        {
            const std::array<ElectryEngine::NoteOnEvent, 2> events {{
                { 28, 0.85f }, { 64, 0.85f }
            }};
            cancelled.noteOnChord(events);
            cancelled.noteOff(64);
            const auto high = TestAccess::snapshot(cancelled, 7);
            expect(! high.active && high.startDelaySamples == 0,
                   "an early Note Off left an un-crossed batch member pending");
        }
        else
        {
            const std::array<ElectryEngine::NoteOnEvent, 1> event {{
                { 28, 0.85f }
            }};
            cancelled.noteOnChord(event);
        }
        StereoBuffer audio(static_cast<int>(0.5 * sampleRate));
        renderInto(cancelled, audio, 67);
        return audio;
    };
    const auto lowOnly = cancelledCrossing(false);
    const auto highCancelled = cancelledCrossing(true);
    expect(lowOnly.left == highCancelled.left
               && lowOnly.right == highCancelled.right,
           "a cancelled future string changed the completed leading contact");

    // A note that arrives after the chord window starts a fresh stroke, so it
    // carries the pre-roll and nothing else - never the previous chord's
    // travel.
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.strumSpreadSeconds = 0.040f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(28, 0.9f);
    StereoBuffer gap(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, gap);
    engine.noteOn(64, 0.9f);
    expect(std::abs(TestAccess::snapshot(
                        engine, ElectryEngine::stringCount - 1).startDelaySamples
                    - preRoll) <= 2,
           "a note outside the chord window inherited a strum offset");

    // A delayed voice is never retired before its excitation fires.
    engine.reset();
    for (const int note : chord)
        engine.noteOn(note, 0.85f);
    StereoBuffer settle(static_cast<int>(0.5 * sampleRate));
    renderInto(engine, settle);
    expect(engine.getActiveVoiceCount() == static_cast<int>(chord.size()),
           "a strum-delayed string was retired before it was picked");
    expect(peakAbs(settle.left) > 1.0e-4f, "the delayed strings never sounded");

    // Strum Spread schedules the stroke rather than shaping it, so the control
    // tick copies it verbatim. A chord dispatched at offset 0 of the block that
    // carries the automation change must already use the new value; reading the
    // smoothed copy scheduled the whole chord with the previous spread. reset()
    // syncs the smoothed state from the target, so this deliberately does not
    // reset after the change.
    {
        ElectryEngine automated;
        automated.prepare (sampleRate, 512);
        EngineParameters flat;
        flat.artifactAmount = 0.0f;
        flat.sympatheticAmount = 0.0f;
        flat.strumSpreadSeconds = 0.0f;
        automated.setParameters (flat);
        automated.reset();
        StereoBuffer settled (1024);
        renderInto (automated, settled);

        EngineParameters spreadNow = flat;
        spreadNow.strumSpreadSeconds = 0.020f;
        automated.setParameters (spreadNow);

        // No render in between: the chord arrives at sample offset 0.
        for (const int note : chord)
            automated.noteOn (note, 0.85f);

        int delayedStrings = 0;
        for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount; ++stringIndex)
            if (TestAccess::snapshot (automated, stringIndex).startDelaySamples > 0)
                ++delayedStrings;
        expect (delayedStrings > 0,
                "a chord automated in the same block was scheduled with the "
                "previous strum spread");
    }

    // Lifting a key before the pick reaches that string cancels the stroke.
    // Leaving the countdown running excited the string after its release, so a
    // short strummed chord grew a late attack once the keys were already up.
    {
        ElectryEngine released;
        released.prepare(sampleRate, 512);
        EngineParameters spread;
        spread.artifactAmount = 0.0f;
        spread.sympatheticAmount = 0.0f;
        spread.strumSpreadSeconds = 0.12f;
        released.setParameters(spread);
        released.reset();
        for (const int note : chord)
            released.noteOn(note, 0.85f);

        // Every key is up long before the pick has crossed the neck.
        StereoBuffer opening(static_cast<int>(0.02 * sampleRate));
        renderInto(released, opening);
        for (const int note : chord)
            released.noteOff(note);

        for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount; ++stringIndex)
            expect(TestAccess::snapshot(released, stringIndex).startDelaySamples == 0,
                   "string " + std::to_string(stringIndex)
                       + " kept a pending strum excitation after its key was lifted");

        StereoBuffer tail(static_cast<int>(1.0 * sampleRate));
        renderInto(released, tail);
        // By this point the damped strings have decayed; any energy here is a
        // pick that landed after the key was released.
        const float late = peakAbs(tail.left, static_cast<int>(0.15 * sampleRate));
        expect(late < 0.01f,
               "a string was picked after its key was released (late peak "
                   + std::to_string(late) + ")");
    }
}

void testStrumTravelFollowsStroke()
{
    // The pick enters the neck at one edge, travels in one direction, and
    // speeds up as it goes. Three things follow, and none of them held before:
    // the edge is set by the stroke rather than by whichever note-on the host
    // sent first, the crossing intervals compress instead of lying on a
    // straight line, and no two strokes lay the same ramp down twice.
    constexpr double sampleRate = 48000.0;
    constexpr double internalRate = sampleRate * 2.0;   // the engine's own clock
    constexpr float spread = 0.012f;
    const int spreadSamples = static_cast<int>(spread * internalRate);
    // Every voice of a strummed chord is held back by this much, which is what
    // buys the re-anchor window a note-on arriving in a later block needs.
    const int preRoll = static_cast<int>(0.020 * internalRate);

    const auto strum = [&] (ElectryEngine& engine, const std::vector<int>& notes)
    {
        for (const int note : notes)
            engine.noteOn(note, 0.85f);
        std::array<int, ElectryEngine::stringCount> delays {};
        for (int s = 0; s < ElectryEngine::stringCount; ++s)
            delays[static_cast<std::size_t>(s)] =
                TestAccess::snapshot(engine, s).startDelaySamples;
        return delays;
    };

    const auto makeEngine = [&] (std::unique_ptr<ElectryEngine>& engine,
                                 float spreadSeconds, PickStyle pick)
    {
        engine = std::make_unique<ElectryEngine>();
        engine->prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.strumSpreadSeconds = spreadSeconds;
        engine->setParameters(parameters);
        engine->reset();
        engine->noteOn(pickKeyswitch(pick), 1.0f);
    };

    // 1. The stroke sets the edge. The same three notes in the same MIDI order
    //    must travel low-to-high on a downstroke and high-to-low on an
    //    upstroke; before this the two produced identical offsets.
    {
        std::unique_ptr<ElectryEngine> down, up;
        makeEngine(down, spread, PickStyle::Down);
        makeEngine(up, spread, PickStyle::Up);
        const auto downDelays = strum(*down, { 40, 45, 50 });
        const auto upDelays = strum(*up, { 40, 45, 50 });
        expect(downDelays[2] < downDelays[3] && downDelays[3] < downDelays[4],
               "a downstroke did not travel from the low string ("
                   + std::to_string(downDelays[2]) + ", "
                   + std::to_string(downDelays[3]) + ", "
                   + std::to_string(downDelays[4]) + ")");
        expect(upDelays[2] > upDelays[3] && upDelays[3] > upDelays[4],
               "an upstroke did not travel from the high string ("
                   + std::to_string(upDelays[2]) + ", "
                   + std::to_string(upDelays[3]) + ", "
                   + std::to_string(upDelays[4]) + ")");
        // 8. The whole cost of the pre-roll: the string the pick starts from
        //    sounds exactly one re-anchor window after its note-on.
        expect(std::abs(downDelays[2] - preRoll) <= 2
                   && std::abs(upDelays[4] - preRoll) <= 2,
               "the string the stroke started from did not sound one pre-roll "
               "after its note-on (" + std::to_string(downDelays[2]) + ", "
                   + std::to_string(upDelays[4]) + " vs "
                   + std::to_string(preRoll) + ")");
    }

    // 2/3. The wrist accelerates through the strings, so the crossing
    //      intervals compress - and the knob still states the mean crossing
    //      time, so eight strings still take seven times it.
    {
        std::unique_ptr<ElectryEngine> engine;
        makeEngine(engine, spread, PickStyle::Down);
        const auto delays = strum(*engine, { 28, 35, 40, 45, 50, 55, 59, 64 });
        std::array<int, ElectryEngine::stringCount - 1> gaps {};
        for (int k = 0; k < ElectryEngine::stringCount - 1; ++k)
            gaps[static_cast<std::size_t>(k)] =
                delays[static_cast<std::size_t>(k + 1)]
                - delays[static_cast<std::size_t>(k)];
        for (int k = 0; k + 1 < ElectryEngine::stringCount - 1; ++k)
            expect(gaps[static_cast<std::size_t>(k + 1)]
                       <= gaps[static_cast<std::size_t>(k)],
                   "the strum's crossing intervals did not compress at gap "
                       + std::to_string(k) + " ("
                       + std::to_string(gaps[static_cast<std::size_t>(k)]) + " then "
                       + std::to_string(gaps[static_cast<std::size_t>(k + 1)]) + ")");
        const double lastOverFirst = static_cast<double>(gaps[6])
                                   / static_cast<double>(gaps[0]);
        expect(lastOverFirst > 0.55 && lastOverFirst < 0.85,
               "the strum's last crossing was not about 0.7 of its first ("
                   + std::to_string(lastOverFirst) + ")");
        const int travel = delays[7] - delays[0];
        expect(std::abs(travel - 7 * spreadSamples) <= 7 * spreadSamples / 20,
               "eight strings did not take seven times the strum spread ("
                   + std::to_string(travel) + " vs "
                   + std::to_string(7 * spreadSamples) + ")");
    }

    // 4. A chord whose first note-on is a middle string. The pick does not
    //    travel outward in both directions at once: it re-anchors on the low
    //    edge and every offset stays non-negative and monotone in string
    //    index. Before this the same chord gave 24/12/0/12/24 ms.
    {
        std::unique_ptr<ElectryEngine> engine;
        makeEngine(engine, spread, PickStyle::Down);
        const auto delays = strum(*engine, { 50, 28, 45, 55, 59 });
        const std::array<int, 5> played { 0, 3, 4, 5, 6 };
        int previous = -1;
        for (const int s : played)
        {
            const int delay = delays[static_cast<std::size_t>(s)];
            expect(delay >= 0,
                   "string " + std::to_string(s)
                       + " of a middle-anchored chord was scheduled backwards");
            expect(delay > previous,
                   "a middle-anchored chord did not travel in string order at "
                   "string " + std::to_string(s) + " ("
                       + std::to_string(delay) + " after "
                       + std::to_string(previous) + ")");
            previous = delay;
        }
    }

    // 5. At a zero spread the chord is one block again, to the sample.
    {
        std::unique_ptr<ElectryEngine> engine;
        makeEngine(engine, 0.0f, PickStyle::Down);
        const auto delays = strum(*engine, { 28, 35, 40, 45, 50, 55, 59, 64 });
        for (const int delay : delays)
            expect(delay == 0,
                   "a chord at zero spread carried a strum offset or a "
                   "pre-roll (" + std::to_string(delay) + ")");
    }

    // 6. The wrist does not lay the same ramp down twice. Two strums of the
    //    same chord 12 s apart - long enough that the strings have decayed and
    //    nothing else is shared - must differ somewhere by at least one
    //    internal sample. The offsets were previously a pure function of the
    //    spread and the string index, so the two were identical.
    {
        std::unique_ptr<ElectryEngine> engine;
        makeEngine(engine, spread, PickStyle::Down);
        const std::vector<int> chord { 28, 35, 40, 45, 50, 55, 59, 64 };
        const auto first = strum(*engine, chord);
        for (const int note : chord)
            engine->noteOff(note);
        StereoBuffer decay(static_cast<int>(12.0 * sampleRate));
        renderInto(*engine, decay);
        const auto second = strum(*engine, chord);
        int largest = 0;
        for (int s = 0; s < ElectryEngine::stringCount; ++s)
            largest = std::max(largest,
                               std::abs(second[static_cast<std::size_t>(s)]
                                        - first[static_cast<std::size_t>(s)]));
        expect(largest >= 1,
               "two strums of the same chord laid down the same ramp ("
                   + std::to_string(largest) + " internal samples apart)");
    }

    // 7/9. The block-straddling case, pinned in absolute onsets. One chord's
    //      note-ons routinely arrive across several process() calls, so what
    //      matters is each event's arrival sample plus its voice's remaining
    //      delay, reduced to one clock. Those onsets must reverse with the
    //      stroke and must not depend on the order the host sent the notes -
    //      today they are 0/20/40 ms in arrival order whichever way the chord
    //      is sent, i.e. the chord travels in host order.
    {
        const int blockSamples = static_cast<int>(0.008 * sampleRate);
        const auto splitDelivery = [&] (PickStyle pick, bool ascending)
        {
            std::unique_ptr<ElectryEngine> engine;
            makeEngine(engine, spread, pick);
            const std::array<int, 3> ascendingNotes { 40, 45, 50 };
            const std::array<int, 3> descendingNotes { 50, 45, 40 };
            const auto& notes = ascending ? ascendingNotes : descendingNotes;
            int arrival = 0;
            for (std::size_t k = 0; k < notes.size(); ++k)
            {
                engine->noteOn(notes[k], 0.85f);
                if (k + 1 < notes.size())
                {
                    StereoBuffer block(blockSamples);
                    renderInto(*engine, block);
                    arrival += blockSamples * 2;   // host samples to the engine's clock
                }
            }
            // Read after the last note-on: the chord's span is 16 ms, inside
            // the 20 ms pre-roll, so nothing has sounded and every voice is
            // still on the same clock.
            std::array<int, 3> onsets {};
            for (int s = 2; s <= 4; ++s)
                onsets[static_cast<std::size_t>(s - 2)] =
                    arrival + TestAccess::snapshot(*engine, s).startDelaySamples;
            return onsets;
        };

        const auto downAscending = splitDelivery(PickStyle::Down, true);
        const auto downDescending = splitDelivery(PickStyle::Down, false);
        const auto upAscending = splitDelivery(PickStyle::Up, true);
        const auto upDescending = splitDelivery(PickStyle::Up, false);

        expect(downAscending[0] < downAscending[1]
                   && downAscending[1] < downAscending[2],
               "a split-block downstroke did not sound low string first");
        expect(upAscending[0] > upAscending[1] && upAscending[1] > upAscending[2],
               "a split-block upstroke did not sound high string first");
        for (std::size_t k = 0; k < 3; ++k)
        {
            expect(downAscending[k] == downDescending[k],
                   "a split-block downstroke's onsets depended on the order the "
                   "host sent the chord (string " + std::to_string(k + 2) + ": "
                       + std::to_string(downAscending[k]) + " vs "
                       + std::to_string(downDescending[k]) + ")");
            expect(upAscending[k] == upDescending[k],
                   "a split-block upstroke's onsets depended on the order the "
                   "host sent the chord (string " + std::to_string(k + 2) + ": "
                       + std::to_string(upAscending[k]) + " vs "
                       + std::to_string(upDescending[k]) + ")");
        }

        // 9. What a re-anchor costs. The first string to sound in the split
        //    delivery may be pushed back by at most the chord's own arrival
        //    span plus the control period the countdown is quantised to.
        std::unique_ptr<ElectryEngine> single;
        makeEngine(single, spread, PickStyle::Down);
        const auto blockChord = strum(*single, { 40, 45, 50 });
        const int splitFirst = std::min({ downAscending[0], downAscending[1],
                                          downAscending[2] });
        const int arrivalSpan = 2 * blockSamples * 2;   // two blocks, engine clock
        expect(splitFirst - blockChord[2] <= arrivalSpan + 16,
               "a re-anchor delayed the leading string by more than the chord's "
               "own arrival span (" + std::to_string(splitFirst) + " vs "
                   + std::to_string(blockChord[2]) + ")");
    }
}

// ---------------------------------------------------------------------------
// CC1 resonance, acoustic feedback, and fret-following pick position
// ---------------------------------------------------------------------------

void testResonanceControlRaisesSympatheticRing()
{
    // CC1 lifts the sympathetic coupling from the parameter's base amount
    // toward total, scaled by the Resonance Depth parameter, and a lowered
    // wheel is a bit-exact no-op.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.sympatheticAmount = 0.15f;
    parameters.resonanceDepth = 1.0f;

    constexpr double openHighE = 329.62756;
    const auto tailRingAt = [&] (float resonance)
    {
        engine.setParameters(parameters);
        engine.reset();
        engine.setResonance(resonance);
        engine.noteOn(45, 0.95f);
        StereoBuffer body(static_cast<int>(0.5 * sampleRate));
        renderInto(engine, body);
        engine.noteOff(45);
        StereoBuffer tail(static_cast<int>(2.1 * sampleRate));
        renderInto(engine, tail);
        expect(allFinite(tail), "the resonance control produced non-finite audio");
        const int start = static_cast<int>(0.9 * sampleRate);
        const int length = static_cast<int>(1.0 * sampleRate);
        return dftMagnitude(tail.left, start, length, sampleRate, openHighE);
    };

    const double base = tailRingAt(0.0f);
    const double lifted = tailRingAt(1.0f);
    expect(lifted > 2.5 * std::max(base, 1.0e-12),
           "a raised wheel did not deepen the sympathetic ring ("
               + std::to_string(base) + " -> " + std::to_string(lifted) + ")");

    // Depth scales what the wheel can reach.
    parameters.resonanceDepth = 0.25f;
    const double shallow = tailRingAt(1.0f);
    expect(shallow < lifted * 0.85 && shallow > base * 0.9,
           "Resonance Depth does not scale the wheel's lift ("
               + std::to_string(shallow) + " against "
               + std::to_string(lifted) + " deep, "
               + std::to_string(base) + " base)");

    // A lowered wheel is exactly the parameter alone, bit for bit - even with
    // a signal sitting in the acoustic-return ring.
    parameters.resonanceDepth = 1.0f;
    engine.setParameters(parameters);
    const auto renderWheelDown = [&] (bool feedTheRing)
    {
        // The wheel is down before the reset, so the smoothed resonance state
        // starts at zero rather than gliding down from the previous take.
        engine.setResonance(0.0f);
        engine.reset();
        if (feedTheRing)
        {
            std::vector<float> loud(2048, 0.5f);
            engine.pushAcousticReturn(loud.data(), loud.data(),
                                      static_cast<int>(loud.size()));
        }
        engine.noteOn(45, 0.8f);
        StereoBuffer take(static_cast<int>(0.7 * sampleRate));
        renderInto(engine, take);
        return take;
    };
    const auto reference = renderWheelDown(false);
    const auto withWheelDown = renderWheelDown(true);
    expect(reference.left == withWheelDown.left,
           "a lowered resonance wheel is not an exact bypass");
}

void testResonanceFeedbackSelfSustains()
{
    // The whole point of the resonance wheel: close the loop the way the
    // plug-in does - engine into the amplifier chain, the amplified output
    // pushed back at the strings - and a distorted note at full wheel keeps
    // itself alive, while the same loop with the wheel down decays, the same
    // wheel without the amplifier decays, and nothing ever leaves the bounded
    // range.
    constexpr double sampleRate = 48000.0;
    constexpr int hostBlockSize = 512;

    struct LoopResult
    {
        double earlyRms { 0.0 };
        double lateRms { 0.0 };
        float peak { 0.0f };
        bool finite { true };
    };

    const auto runClosedLoop = [&] (float resonance, float distortion,
                                    float amp, bool palmMuted = false)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, hostBlockSize);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        parameters.sympatheticAmount = 0.2f;
        parameters.resonanceDepth = 1.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.setResonance(resonance);
        if (palmMuted)
            engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);

        electry::ElectryFx fx;
        fx.prepare(sampleRate);
        electry::FxParameters fxParameters;
        fxParameters.distortion = distortion;
        fxParameters.amp = amp;
        fx.setParameters(fxParameters);
        fx.reset();
        // What the plug-in shell does: the rig's acoustic loudness in the
        // room follows the amplifier controls, because the chain itself keeps
        // its listening level roughly constant.
        engine.setAcousticReturnLevel(std::min(1.0f, amp + 0.6f * distortion));

        // The note is released early: a released string damps in tens of
        // milliseconds, so anything still loud seconds later has to be the
        // feedback loop keeping the instrument alive, exactly like a guitar
        // left facing its amplifier.
        engine.noteOn(40, 0.95f); // open E2
        const int totalSamples = static_cast<int>(5.0 * sampleRate);
        const int releaseSample = static_cast<int>(0.75 * sampleRate);
        const int earlyStart = static_cast<int>(0.25 * sampleRate);
        const int earlyEnd = static_cast<int>(0.65 * sampleRate);
        const int lateStart = static_cast<int>(4.0 * sampleRate);
        const int feedbackChunkSize = engine.getAcousticReturnDelaySamples();
        std::vector<float> left(static_cast<std::size_t>(feedbackChunkSize));
        std::vector<float> right(static_cast<std::size_t>(feedbackChunkSize));
        LoopResult result;
        double earlySum = 0.0;
        double lateSum = 0.0;
        long earlyCount = 0;
        long lateCount = 0;
        bool released = false;
        int rendered = 0;
        while (rendered < totalSamples)
        {
            // The palm-muted take keeps the note held: the muting hand is on
            // the strings only while the muted note is, and lifting it under
            // a raised wheel legitimately lets the howl return.
            if (! palmMuted && ! released && rendered == releaseSample)
            {
                engine.noteOff(40);
                released = true;
            }
            int count = std::min(feedbackChunkSize, totalSamples - rendered);
            if (! palmMuted && ! released && rendered < releaseSample)
                count = std::min(count, releaseSample - rendered);
            engine.process(left.data(), right.data(), count);
            fx.process(left.data(), right.data(), count);
            engine.pushAcousticReturn(left.data(), right.data(), count);

            for (int i = 0; i < count; ++i)
            {
                const float sample = left[static_cast<std::size_t>(i)];
                if (! std::isfinite(sample))
                    result.finite = false;
                result.peak = std::max(result.peak, std::abs(sample));
                const double energy = static_cast<double>(sample)
                                    * static_cast<double>(sample);
                const int absoluteSample = rendered + i;
                if (absoluteSample >= earlyStart && absoluteSample < earlyEnd)
                {
                    earlySum += energy;
                    ++earlyCount;
                }
                else if (absoluteSample >= lateStart)
                {
                    lateSum += energy;
                    ++lateCount;
                }
            }
            rendered += count;
        }
        result.earlyRms = std::sqrt(earlySum / std::max<long>(earlyCount, 1));
        result.lateRms = std::sqrt(lateSum / std::max<long>(lateCount, 1));
        return result;
    };

    const auto fed = runClosedLoop(1.0f, 0.75f, 0.8f);
    expect(fed.finite, "the fed-back loop produced non-finite audio");
    expect(fed.peak < 4.0f, "the fed-back loop escaped its bounded range");
    expect(fed.lateRms > 0.25 * fed.earlyRms,
           "full resonance with a distorted amplifier does not self-sustain "
           "after the key is released ("
               + std::to_string(decibels(fed.lateRms
                                         / std::max(fed.earlyRms, 1.0e-15)))
               + " dB after four seconds)");
    // Pin the direct feedback routing. A three-note shape puts A4 on the G
    // string, then leaves it held after the upper two
    // strings are released. During coefficient selection, equal or
    // half-strength direct drive let the idle high E win this closed loop; the
    // voiced quarter share leaves the performed string comfortably in charge
    // without removing bridge bloom.
    {
        constexpr double ownershipRate = 44100.0;
        constexpr int ownershipBlockSize = 256;
        ElectryEngine engine;
        engine.prepare(ownershipRate, ownershipBlockSize);
        EngineParameters parameters;
        parameters.pickupSelector = PickupSelector::Bridge;
        parameters.pickupType = 0.4f;
        parameters.toneKnob = 0.9f;
        parameters.bodyResonance = 0.45f;
        parameters.stringAge = 0.08f;
        parameters.pickPosition = 0.30f;
        parameters.resonanceDepth = 1.0f;
        parameters.bendTimeSeconds = 0.20f;
        parameters.sympatheticAmount = 0.35f;
        parameters.outputGain = 1.5f;
        parameters.outputMode = electry::OutputMode::Stereo;
        engine.setParameters(parameters);
        engine.reset();

        electry::ElectryFx fx;
        fx.prepare(ownershipRate);
        electry::FxParameters fxParameters;
        fxParameters.distortion = 0.30f;
        fxParameters.amp = 0.82f;
        fxParameters.compressor = 0.35f;
        fxParameters.delay = 0.30f;
        fxParameters.room = 0.35f;
        fx.setParameters(fxParameters);
        fx.reset();
        engine.setAcousticReturnLevel(
            std::min(1.0f, fxParameters.amp
                         + 0.6f * fxParameters.distortion));

        std::vector<float> left(ownershipBlockSize);
        std::vector<float> right(ownershipBlockSize);
        const auto render = [&] (double seconds)
        {
            int remaining = static_cast<int>(seconds * ownershipRate);
            while (remaining > 0)
            {
                const int count = std::min(ownershipBlockSize, remaining);
                engine.process(left.data(), right.data(), count);
                fx.process(left.data(), right.data(), count);
                engine.pushAcousticReturn(left.data(), right.data(), count);
                remaining -= count;
            }
        };

        render(0.25);
        engine.noteOn(pickKeyswitch(PickStyle::Alternate), 1.0f);
        engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        const std::array<ElectryEngine::NoteOnEvent, 3> chord {{
            { 69, 1.0f }, { 74, 1.0f }, { 79, 1.0f }
        }};
        engine.noteOnChord(chord);
        render(0.10);
        engine.noteOff(74);
        engine.noteOff(79);
        render(0.80);
        engine.setResonance(1.0f);
        render(2.20);

        constexpr int playedString = 5;
        constexpr int idleHighEString = 7;
        const auto played = TestAccess::snapshot(engine, playedString);
        const auto idleHighE = TestAccess::snapshot(engine, idleHighEString);
        expect(played.active && played.keyDown && played.midiNote == 69,
               "the feedback-ownership fixture did not retain A4 on string 5");
        expect(! idleHighE.active && idleHighE.sympatheticReady,
               "the feedback-ownership fixture did not leave high E idle");
        const double playedAmplitude = std::sqrt(
            std::max<double>(TestAccess::voiceOutputEnergy(
                                 engine, playedString),
                             0.0));
        const double idleHighEAmplitude = std::sqrt(
            std::max<double>(idleHighE.sympatheticEnergy, 0.0));
        std::cout << "PROBE held A4/high-E feedback energy ratio: "
                  << decibels(playedAmplitude
                              / std::max(idleHighEAmplitude, 1.0e-15))
                  << " dB\n";
        expect(playedAmplitude > 2.0 * idleHighEAmplitude,
               "the idle high E stole a held A4 feedback loop ("
                   + std::to_string(playedAmplitude) + " played, "
                   + std::to_string(idleHighEAmplitude) + " idle)");
    }

    const auto wheelDown = runClosedLoop(0.0f, 0.75f, 0.8f);
    expect(wheelDown.finite && wheelDown.peak < 4.0f,
           "the wheel-down loop was not bounded");
    expect(wheelDown.lateRms < 0.05 * wheelDown.earlyRms,
           "a released distorted note with the wheel down failed to decay ("
               + std::to_string(decibels(
                     wheelDown.lateRms
                     / std::max(wheelDown.earlyRms, 1.0e-15)))
               + " dB after four seconds)");
    expect(fed.lateRms > 4.0 * std::max(wheelDown.lateRms, 1.0e-9),
           "the wheel makes no difference to the closed distorted loop");

    const auto dry = runClosedLoop(1.0f, 0.0f, 0.0f);
    expect(dry.finite && dry.peak < 4.0f, "the dry loop was not bounded");
    expect(dry.lateRms < 0.05 * dry.earlyRms,
           "full resonance without the amplifier failed to decay ("
               + std::to_string(decibels(dry.lateRms
                                         / std::max(dry.earlyRms, 1.0e-15)))
               + " dB after four seconds)");

    // The muting hand starves the loop: the same full-wheel distorted rig
    // with the Palm Mute style latched at the default Palm Tightness must not
    // howl. A linear or even squared hand residue measurably regenerated
    // back to nearly the open level, which is what this pins against.
    const auto muted = runClosedLoop(1.0f, 0.75f, 0.8f, true);
    expect(muted.finite && muted.peak < 4.0f,
           "the palm-muted loop was not bounded");
    expect(muted.lateRms < 0.1 * fed.lateRms,
           "a palm-muted passage still howls at full resonance ("
               + std::to_string(muted.lateRms) + " against fed "
               + std::to_string(fed.lateRms) + ")");
}

void testPickGeometryFollowsFret()
{
    constexpr double sampleRate = 48000.0;

    // The picking hand stays put while the fretting hand moves, so the pluck
    // position as a fraction of the sounding length grows by 2^(fret/12).
    // Note 86 is only reachable at fret 22 of the top string, and note 64 is
    // that same string open, which pins the comparison to one physical string.
    const auto combFraction = [&] (int midiNote)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickPosition = 0.30f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(midiNote, 0.8f);
        const int stringIndex = ElectryEngine::stringCount - 1;
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        expect(snapshot.midiNote == midiNote,
               "note " + std::to_string(midiNote)
                   + " was not allocated to the top string");
        return static_cast<double>(snapshot.excitationCombDelay)
             / std::max(static_cast<double>(snapshot.lastCompensatedPeriod), 1.0e-9);
    };

    const double openComb = combFraction(64);
    const double frettedComb = combFraction(86);
    const double expectedRatio = std::pow(2.0, 22.0 / 12.0);
    const double actualRatio = frettedComb / std::max(openComb, 1.0e-9);
    expect(frettedComb > 0.5 && frettedComb < 0.98
               && std::abs(actualRatio - expectedRatio)
                      < 1.0e-4 * expectedRatio,
           "pluck position did not follow the fretted sounding length ("
               + std::to_string(actualRatio) + " vs "
               + std::to_string(expectedRatio) + ")");
}

void testPickContactGeometry()
{
    constexpr double sampleRate = 48000.0;

    // A plectrum is neither a point nor symmetric: it touches the string over a
    // patch and it slips off far faster than it loaded.
    const auto pickGeometry = [] (
        int midiNote, float hardness, float stringAge, float pitchBend,
        float pickPosition = EngineParameters {}.pickPosition,
        PickStyle pickStyle = PickStyle::Down,
        PlayStyle playStyle = PlayStyle::Sustain)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickHardness = hardness;
        parameters.stringAge = stringAge;
        parameters.pickPosition = pickPosition;
        engine.setParameters(parameters);
        engine.setPitchBend(pitchBend);
        engine.reset();
        engine.noteOn(pickKeyswitch(pickStyle), 1.0f);
        engine.noteOn(styleKeyswitch(playStyle), 1.0f);
        engine.noteOn(midiNote, 0.9f);
        const int stringIndex = TestAccess::stringForNote(engine, midiNote);
        expect(stringIndex >= 0, "note " + std::to_string(midiNote)
                                     + " was not allocated");
        const auto snapshot = TestAccess::snapshot(
            engine, std::max(stringIndex, 0));
        if (stringIndex >= 0)
        {
            // Absolute reflected-image geometry, not only invariance: the
            // centre is 2x/c = pP. A physical contact of full width w extends
            // by w/c on either side of that image because P = 2L/c.
            const float scaleLength = TestAccess::scaleLengthMetres(engine);
            const float fretStretch = std::exp2(
                static_cast<float>(snapshot.fret) / 12.0f);
            float expectedOpenFraction =
                0.025f + (0.48f - 0.025f) * parameters.pickPosition;
            if (playStyle == PlayStyle::PalmMute)
                expectedOpenFraction *= 0.8f;
            if (pickStyle == PickStyle::Up
                && playStyle != PlayStyle::Hammer)
                expectedOpenFraction -= 0.020f;
            const float expectedFraction = electry::clampf(
                (expectedOpenFraction
                    + snapshot.strokeContactOffsetMetres / scaleLength)
                    * fretStretch,
                0.02f, 0.98f);
            const float expectedCentre = expectedFraction
                                       * snapshot.lastCompensatedPeriod;
            const float soundingLength = std::max(
                scaleLength / fretStretch, 0.05f);
            const float contactMetres = 0.001f
                * (1.5f + (0.5f - 1.5f) * hardness);
            const float expectedHalfWidth = 0.5f
                * (contactMetres / soundingLength)
                * snapshot.lastCompensatedPeriod;
            expect(std::abs(snapshot.excitationCombDelay - expectedCentre)
                       < 1.0e-6f * expectedCentre
                       && std::abs(snapshot.excitationCombWidth
                                   - expectedHalfWidth)
                       < 1.0e-6f * expectedHalfWidth,
                   "pick image centre/width left the absolute metre-derived "
                   "2x/c geometry");
        }
        return snapshot;
    };

    const auto defaultAge = EngineParameters {}.stringAge;
    const auto lowDefault = pickGeometry(28, 0.6f, defaultAge, 0.0f);
    const auto middleDefault = pickGeometry(52, 0.6f, defaultAge, 0.0f);
    const auto highDefault = pickGeometry(64, 0.6f, defaultAge, 0.0f);
    const auto lowSoft = pickGeometry(28, 0.0f, defaultAge, 0.0f);
    const auto lowHard = pickGeometry(28, 1.0f, defaultAge, 0.0f);
    const auto fretTwelveNearFinger = pickGeometry(
        76, 0.6f, defaultAge, 0.0f, 1.0f);
    const auto stoppedAtFinger = pickGeometry(
        86, 0.6f, defaultAge, 0.0f, 1.0f);
    const auto bridgeDown = pickGeometry(
        28, 0.6f, defaultAge, 0.0f, 0.0f, PickStyle::Down);
    const auto bridgeUp = pickGeometry(
        28, 0.6f, defaultAge, 0.0f, 0.0f, PickStyle::Up);
    const auto neckDown = pickGeometry(
        28, 0.6f, defaultAge, 0.0f, 1.0f, PickStyle::Down);
    const auto neckUp = pickGeometry(
        28, 0.6f, defaultAge, 0.0f, 1.0f, PickStyle::Up);
    const auto palmBridgeDown = pickGeometry(
        28, 0.6f, defaultAge, 0.0f, 0.0f, PickStyle::Down,
        PlayStyle::PalmMute);
    const auto palmBridgeUp = pickGeometry(
        28, 0.6f, defaultAge, 0.0f, 0.0f, PickStyle::Up,
        PlayStyle::PalmMute);

    expect(lowDefault.excitationCombWidth > 0.0f,
           "the pick contact patch has no width");
    expect(lowDefault.excitationCombDelay
                   / lowDefault.lastCompensatedPeriod < 0.49f,
           "the below-midpoint pick fixture did not stay on its original path");
    expect(fretTwelveNearFinger.excitationCombDelay
                   / fretTwelveNearFinger.lastCompensatedPeriod > 0.90f,
           "a valid fret-12 pick near the stopping finger was collapsed to "
           "the string midpoint");
    expect(std::abs(stoppedAtFinger.excitationCombDelay
                        / stoppedAtFinger.lastCompensatedPeriod
                    - 0.98f) < 1.0e-6f,
           "a fixed pick beyond the shortened string was not stopped just "
           "bridge-side of the fretting finger");
    const auto contactFraction = [] (const auto& snapshot)
    {
        return snapshot.excitationCombDelay
             / snapshot.lastCompensatedPeriod;
    };
    expect(contactFraction(bridgeUp) < contactFraction(bridgeDown),
           "the bridge-end upstroke did not stay bridgeward of the downstroke");
    expect(std::abs((contactFraction(neckDown) - contactFraction(neckUp))
                    - 0.020f) < 1.0e-6f,
           "the neck-end upstroke did not retain its 2%-of-scale offset");
    expect(contactFraction(palmBridgeUp) <= contactFraction(palmBridgeDown)
               && std::abs(contactFraction(palmBridgeUp) - 0.02f) < 1.0e-6f,
           "the palm-muted bridge-end upstroke escaped or reversed the shared "
           "physical endpoint guard");
    // A fixed physical patch is a larger share of the low string's much longer
    // round trip, so it spans more delay-line samples there.
    expect(lowDefault.excitationCombWidth > 4.0f * highDefault.excitationCombWidth,
           "the contact patch does not scale with the string's length ("
               + std::to_string(lowDefault.excitationCombWidth) + " vs "
               + std::to_string(highDefault.excitationCombWidth) + ")");
    // A soft rounded pick touches over roughly three times the patch of a stiff
    // sharp one.
    expect(lowSoft.excitationCombWidth > 2.0f * lowHard.excitationCombWidth,
           "pick hardness does not narrow the contact patch");

    // The patch spans a fixed distance along the string and the wave speed does
    // not change when the string is fretted, so its width in delay samples is
    // the same open and at the twelfth fret. The allocator prefers the free
    // string with the lowest fret, so note 64 is the top string open and note
    // 76 is that same string at fret 12; both are unambiguous.
    const auto openTop = pickGeometry(64, 0.6f, defaultAge, 0.0f);
    const auto frettedTop = pickGeometry(76, 0.6f, defaultAge, 0.0f);
    expect(openTop.stringIndex == frettedTop.stringIndex,
           "the fretted comparison did not stay on one physical string");
    expect(std::abs(openTop.excitationCombDelay
                    - frettedTop.excitationCombDelay)
               < 2.0e-5f * openTop.excitationCombDelay,
           "the fixed pick position moved in delay samples when fretted ("
               + std::to_string(openTop.excitationCombDelay) + " vs "
               + std::to_string(frettedTop.excitationCombDelay) + ")");
    expect(std::abs(openTop.excitationCombWidth
                    - frettedTop.excitationCombWidth)
               < 2.0e-5f * openTop.excitationCombWidth,
           "the contact patch is not fret invariant in delay samples ("
               + std::to_string(openTop.excitationCombWidth) + " vs "
               + std::to_string(frettedTop.excitationCombWidth) + ")");

    // Raising tension raises transverse wave speed. A fixed hand and tip
    // therefore shrink in delay samples by the inverse frequency ratio.
    const auto bentTop = pickGeometry(64, 0.6f, defaultAge, 1.0f);
    const float expectedBendScale = std::exp2(-2.0f / 12.0f);
    expect(std::abs(bentTop.excitationCombDelay / openTop.excitationCombDelay
                        - expectedBendScale) < 2.0e-5f
               && std::abs(bentTop.excitationCombWidth
                               / openTop.excitationCombWidth
                               - expectedBendScale) < 2.0e-5f,
           "a two-semitone pre-bend did not move pick geometry with 1/c");

    // Loop damping changes filter phase and hence raw delay, not string
    // geometry or wave speed. This explicitly prevents that digital
    // coordinate from leaking back into the physical contact model.
    const auto freshLow = pickGeometry(28, 0.6f, 0.0f, 0.0f);
    const auto oldLow = pickGeometry(28, 0.6f, 1.0f, 0.0f);
    expect(std::abs(freshLow.verticalDelayTarget - oldLow.verticalDelayTarget)
               > 1.0e-3f,
           "the String Age fixture did not move loop-filter phase");
    expect(std::abs(freshLow.lastCompensatedPeriod
                    - oldLow.lastCompensatedPeriod)
                   < 1.0e-6f * freshLow.lastCompensatedPeriod
               && std::abs(freshLow.excitationCombDelay
                            - oldLow.excitationCombDelay)
                   < 2.0e-5f * freshLow.excitationCombDelay
               && std::abs(freshLow.excitationCombWidth
                            - oldLow.excitationCombWidth)
                   < 2.0e-5f * freshLow.excitationCombWidth,
           "String Age moved the physical pick point or contact width");
    std::cout << "PROBE physical pick raw/full ratios: E4 "
              << openTop.verticalDelayTarget / openTop.lastCompensatedPeriod
              << ", E3 "
              << middleDefault.verticalDelayTarget
                    / middleDefault.lastCompensatedPeriod
              << ", default E1 "
              << lowDefault.verticalDelayTarget
                    / lowDefault.lastCompensatedPeriod
              << ", fresh E1 "
              << freshLow.verticalDelayTarget / freshLow.lastCompensatedPeriod
              << ", old E1 "
              << oldLow.verticalDelayTarget / oldLow.lastCompensatedPeriod
              << '\n';

    // A stiffer pick holds the string longer and then releases it faster.
    const float softSlip = 1.0f / lowSoft.excitationLoadScale;
    const float hardSlip = 1.0f / lowHard.excitationLoadScale;
    expect(hardSlip > softSlip + 0.10f,
           "pick hardness does not move the slip point later ("
               + std::to_string(softSlip) + " to " + std::to_string(hardSlip)
               + ")");
    expect(lowHard.excitationSlipScale > lowSoft.excitationSlipScale,
           "a stiffer pick does not release the string faster");

    // The release window is a load smoothstep times a slip smoothstep. Whatever
    // the slip point, its area is exactly one half - the same area the
    // symmetric raised cosine it replaced had - so the asymmetry changes the
    // attack's spectrum without changing how hard the note lands.
    for (const auto& snapshot : { lowSoft, lowDefault, lowHard })
    {
        const auto smoothStep = [] (double value)
        {
            value = std::clamp(value, 0.0, 1.0);
            return value * value * (3.0 - 2.0 * value);
        };
        constexpr int steps = 200000;
        double area = 0.0;
        for (int step = 0; step < steps; ++step)
        {
            const double progress = (static_cast<double>(step) + 0.5)
                                  / static_cast<double>(steps);
            area += smoothStep(progress * snapshot.excitationLoadScale)
                  * smoothStep((1.0 - progress) * snapshot.excitationSlipScale);
        }
        area /= static_cast<double>(steps);
        expect(std::abs(area - 0.5) < 1.0e-3,
               "the asymmetric pick release window is not level neutral ("
                   + std::to_string(area) + ")");
    }
}

// A low note has to be carried by its own fundamental. Measured against dry
// reference recordings, the two ways this engine has failed that were a pickup
// position comb that cancelled exactly - which put a zero at DC and cost the
// fundamental most - and a bridge hand modelled as a genuinely broadband
// absorber, which damped the fundamental as hard as the top end and turned a
// palm mute into a short pick. Both are voicing, so neither is pinned to a
// number here; what is pinned is the audible consequence each one had.
// The hand's loss dip is the one filter in this model that sits inside the
// feedback loop with a shape solved per note rather than a fixed one. Its safety
// rests entirely on a peaking section with sub-unity gain having a magnitude
// bounded by one everywhere, so that bound is asserted directly on the
// coefficients the engine actually runs, across the playable range and the whole
// travel of both mute controls. If it ever exceeded one the loop would grow at
// that frequency, which is the one failure this model cannot absorb.
// The mute dip sits inside the loop, so its phase is part of the sounding
// period and the engine subtracts it from the delay-line read. Without that
// subtraction the mute drags the pitch flat - measured at up to 13 cents on
// the low string, which is what the compensation exists to remove and what
// this pins. The depth is also modulated over the note, so the correction has
// to follow the depth actually applied rather than the one the note started
// on; that part is worth about a cent in the settled window a DFT can read,
// so this test guards the invariant rather than that refinement.
//
// Measured against the same note unmuted, so estimator bias cancels; the bound
// rejects the former uncompensated hand-filter phase error of up to 13 cents.
void testPalmMuteDoesNotShiftPitch()
{
    constexpr double sampleRate = 48000.0;
    int asserted = 0;
    int variedAsserted = 0;

    for (const int midiNote : { 28, 40, 52, 64 })
    {
        const double nominal = midiHz(midiNote);
        const int start = static_cast<int>(0.080 * sampleRate);
        const int length = static_cast<int>(0.250 * sampleRate);

        const auto fundamentalOf = [&] (float pressure, float velocity,
                                        std::uint64_t precedingNotes,
                                        double& rmsOut)
        {
            EngineParameters parameters;
            parameters.palmMute = pressure;
            ElectryEngine engine;
            engine.prepare(sampleRate, 512);
            engine.setParameters(parameters);
            engine.reset();
            TestAccess::setPrecedingNoteCount(engine, precedingNotes);
            engine.noteOn(pickKeyswitch(PickStyle::Down), 1.0f);
            engine.noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
            engine.noteOn(midiNote, velocity);
            StereoBuffer buffer(static_cast<int>(0.4 * sampleRate));
            renderInto(engine, buffer);
            double sum = 0.0;
            for (int i = 0; i < length; ++i)
                sum += static_cast<double>(buffer.left[start + i])
                     * static_cast<double>(buffer.left[start + i]);
            rmsOut = std::sqrt(sum / static_cast<double>(length));
            // The fundamental alone. Scoring a partial series instead tracks
            // the mute's spectral tilt rather than its pitch, which reads as
            // several cents of drift that the fundamental does not show.
            double best = nominal;
            double bestMagnitude = -1.0;
            for (double cents = -60.0; cents <= 60.0; cents += 0.25)
            {
                const double frequency = nominal
                    * std::pow(2.0, cents / 1200.0);
                const double magnitude = dftMagnitude(
                    buffer.left, start, length, sampleRate, frequency);
                if (magnitude > bestMagnitude)
                {
                    bestMagnitude = magnitude;
                    best = frequency;
                }
            }
            return best;
        };

        double openRms = 0.0;
        const double open = fundamentalOf(0.0f, 0.95f, 0, openRms);

        for (const float pressure : { 0.30f, 0.55f, 1.00f })
        {
            double mutedRms = 0.0;
            const double muted = fundamentalOf(pressure, 0.95f, 0,
                                               mutedRms);
            // Assert only where the note still has enough sustained energy for
            // pitch to be a property of it at all. A fully muted low E is ~37
            // dB below the open note here and dropping fast; two windowings of
            // that same remnant disagree by 8 cents, so a threshold on it would
            // be measuring the estimator. 30 dB down is the cutoff, which
            // excludes exactly that one case out of the twelve.
            if (mutedRms < 1.0e-5 || mutedRms < 0.03 * openRms)
                continue;

            const double shift = centsBetween(muted, open);
            ++asserted;
            expect(std::abs(shift) < 6.0,
                   "palm mute shifts pitch by " + std::to_string(shift)
                       + " cents at note " + std::to_string(midiNote)
                       + ", pressure " + std::to_string(pressure));
        }

        // The contact-rate multiplier moves with both written velocity and
        // the deterministic stroke draw. Cover both axes on the two lowest E
        // strings without multiplying the full playable-range matrix above.
        // This matrix specifically covers compensation of the hand-loss
        // filter whose depth the contact-rate multiplier changes.
        if (midiNote <= 40)
        {
            for (const float velocity : { 0.20f, 0.95f })
            {
                for (const std::uint64_t precedingNotes : { 0u, 7u })
                {
                    double variedOpenRms = 0.0;
                    const double variedOpen = fundamentalOf(
                        0.0f, velocity, precedingNotes, variedOpenRms);
                    double variedMutedRms = 0.0;
                    const double variedMuted = fundamentalOf(
                        0.55f, velocity, precedingNotes, variedMutedRms);
                    if (variedMutedRms < 1.0e-5
                        || variedMutedRms < 0.03 * variedOpenRms)
                        continue;

                    const double shift = centsBetween(variedMuted, variedOpen);
                    ++variedAsserted;
                    expect(std::abs(shift) < 6.0,
                           "palm-mute contact shifts pitch by "
                               + std::to_string(shift) + " cents at note "
                               + std::to_string(midiNote) + ", velocity "
                               + std::to_string(velocity) + ", stroke "
                               + std::to_string(precedingNotes + 1));
                }
            }
        }
    }

    // The energy guard must not be able to quietly skip the whole test.
    expect(asserted >= 9,
           "expected at least 9 measurable palm-mute pitch cases, asserted "
               + std::to_string(asserted));
    expect(variedAsserted >= 6,
           "expected at least 6 measurable velocity/stroke pitch cases, asserted "
               + std::to_string(variedAsserted));
}

#if ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2
void testLowestStringLossCorrection()
{
    const auto responseMagnitude = [] (double b0, double b1, double b2,
                                       double a1, double a2, double omega)
    {
        const double cosine = std::cos(omega);
        const double sine = std::sin(omega);
        const double cosine2 = 2.0 * cosine * cosine - 1.0;
        const double sine2 = 2.0 * sine * cosine;
        const double nr = b0 + b1 * cosine + b2 * cosine2;
        const double ni = -(b1 * sine + b2 * sine2);
        const double dr = 1.0 + a1 * cosine + a2 * cosine2;
        const double di = -(a1 * sine + a2 * sine2);
        return std::sqrt((nr * nr + ni * ni)
                         / std::max(dr * dr + di * di, 1.0e-30));
    };

    for (const double hostRate : { 44100.0, 48000.0, 96000.0,
                                   192000.0, 384000.0 })
    {
        for (const int note : { 30, 36, 42 })
        {
            ElectryEngine engine;
            engine.prepare(hostRate, 512);
            EngineParameters parameters;
            parameters.sympatheticAmount = 0.0f;
            engine.setParameters(parameters);
            engine.reset();
            TestAccess::retriggerVoice(engine, 0, note, 0.9f);

            const auto loss = TestAccess::fittedLossState(engine, 0);
            expect(loss.active,
                   "the fitted lowest-string correction was not active at note "
                       + std::to_string(note));
            const auto horizontal = TestAccess::fittedLossState(
                engine, 0, true);
            expect(horizontal.active,
                   "the horizontal polarisation omitted the loss correction");
            expect(horizontal == loss,
                   "the two polarisations received different loss filters");

            // Both poles and both zeros remain inside the unit circle. These
            // are the real-coefficient Jury conditions for z^2+c1*z+c2.
            const double z1 = loss.b1 / loss.b0;
            const double z2 = loss.b2 / loss.b0;
            expect(std::abs(loss.a2) < 1.0
                       && 1.0 + loss.a1 + loss.a2 > 0.0
                       && 1.0 - loss.a1 + loss.a2 > 0.0,
                   "the fitted loss correction is not stable");
            expect(std::abs(z2) < 1.0 && 1.0 + z1 + z2 > 0.0
                       && 1.0 - z1 + z2 > 0.0,
                   "the fitted loss correction is not minimum phase");

            double peakMagnitude = 0.0;
            for (int bin = 0; bin <= 4096; ++bin)
            {
                const double omega = 3.14159265358979323846
                                   * static_cast<double>(bin) / 4096.0;
                peakMagnitude = std::max(
                    peakMagnitude,
                    responseMagnitude(loss.b0, loss.b1, loss.b2,
                                      loss.a1, loss.a2, omega));
            }
            expect(peakMagnitude <= 1.0 + 2.0e-10,
                   "the fitted loss correction can add loop energy (peak "
                       + std::to_string(peakMagnitude) + ")");

            const double f0 = midiHz(note);
            const double internalRate = TestAccess::internalSampleRate(engine);
            std::array<double, 7> upperLoss {};
            const double fundamentalLoss = -20.0 * std::log10(
                std::max(responseMagnitude(
                    loss.b0, loss.b1, loss.b2, loss.a1, loss.a2,
                    2.0 * 3.14159265358979323846 * f0 / internalRate),
                         1.0e-30)) * f0;
            for (int partial = 2; partial <= 8; ++partial)
            {
                const double magnitude = responseMagnitude(
                    loss.b0, loss.b1, loss.b2, loss.a1, loss.a2,
                    2.0 * 3.14159265358979323846 * f0 * partial
                        / internalRate);
                upperLoss[static_cast<std::size_t>(partial - 2)] =
                    -20.0 * std::log10(std::max(magnitude, 1.0e-30)) * f0;
            }
            std::sort(upperLoss.begin(), upperLoss.end());
            const double medianUpperLoss = upperLoss[3];
            expect(fundamentalLoss > 1.0 && fundamentalLoss < 5.6,
                   "the fitted loss correction escaped its observed H1 range");
            expect(medianUpperLoss > 14.0 && medianUpperLoss < 24.0,
                   "the fitted loss correction missed the H2-H8 direction");

            for (const bool horizontal : { false, true })
            {
                const float effective = TestAccess::effectiveLoopFrequency(
                    engine, 0, horizontal);
                expect(std::abs(centsBetween(effective, f0)) < 1.0,
                       "the loss correction phase compensation detuned note "
                           + std::to_string(note));
            }
        }
    }

    // Scope is evidence, not convenience: the fit is one physical F# string
    // over F#1--F#2. Electry's unsupported open Drop-E, the next fret above the
    // range and every other string remain exact bypasses.
    for (const auto [stringIndex, note] :
         { std::pair { 0, 28 }, std::pair { 0, 43 }, std::pair { 1, 36 } })
    {
        ElectryEngine engine;
        engine.prepare(48000.0, 512);
        engine.reset();
        TestAccess::retriggerVoice(engine, stringIndex, note, 0.9f);
        expect(! TestAccess::fittedLossState(engine, stringIndex).active,
               "the F#-string fit leaked outside its evidence domain");
    }

    // The evidence gate follows the same continuous finger coordinate as a
    // Hammer or Slide. It must be unchanged at retarget, follow the exact C1
    // one-fret taper, remain passive, and land on an exact endpoint bypass/full
    // response without a pitch or output step.
    struct BoundaryMove { int from, to; bool fromActive, toActive; };
    for (const auto style : { PlayStyle::Hammer, PlayStyle::Slide })
    for (const auto move : {
        BoundaryMove { 29, 30, false, true },
        BoundaryMove { 30, 29, true, false },
        BoundaryMove { 42, 43, true, false },
        BoundaryMove { 43, 42, false, true } })
    {
        ElectryEngine engine;
        engine.prepare(48000.0, 512);
        EngineParameters quiet;
        quiet.sympatheticAmount = 0.0f;
        quiet.artifactAmount = 0.0f;
        quiet.pickNoise = 0.0f;
        quiet.fingerNoise = 0.0f;
        quiet.releaseNoise = 0.0f;
        engine.setParameters(quiet);
        engine.reset();
        TestAccess::retriggerVoice(engine, 0, move.from, 0.85f);
        StereoBuffer before(5760);
        renderInto(engine, before);

        const auto dipState = [&] (bool horizontal = false)
        {
            return TestAccess::fittedLossState(engine, 0, horizontal);
        };

        const auto source = dipState();
        expect(source.active == move.fromActive,
               "invalid fitted-loss legato boundary fixture");
        const float frequencyBefore = TestAccess::effectiveLoopFrequency(
            engine, 0);

        TestAccess::legatoRetargetVoice(engine, 0, move.to, style);
        const auto atRetarget = dipState();
        expect(atRetarget == source,
               "legato changed fitted loss before the finger moved");
        const float frequencyAfter = TestAccess::effectiveLoopFrequency(
            engine, 0);
        expect(std::abs(centsBetween(frequencyAfter, frequencyBefore)) < 2.0,
               "fitted-loss topology changed pitch at a legato boundary");

        float previousLossRate = source.peakDb * midiHz(move.from);
        float previousSample = before.left.back();
        float maximumStep = 0.0f;
        bool transitionFinite = true;
        bool checkedFractionalPassivity = false;
        int ticks = 0;
        while (TestAccess::legatoBlend(engine, 0) < 1.0f && ticks < 512)
        {
            StereoBuffer tick(8);
            renderInto(engine, tick);
            transitionFinite = transitionFinite && allFinite(tick);
            maximumStep = std::max(
                maximumStep, std::abs(tick.left.front() - previousSample));
            for (std::size_t i = 1; i < tick.left.size(); ++i)
                maximumStep = std::max(
                    maximumStep,
                    std::abs(tick.left[i] - tick.left[i - 1]));
            previousSample = tick.left.back();

            const float liveFrequency =
                TestAccess::programmedLegatoFrequency(engine, 0);
            const float liveFret = 12.0f * std::log2(
                liveFrequency / midiHz(28));
            const auto smooth = [] (float value)
            {
                const float x = std::clamp(value, 0.0f, 1.0f);
                return x * x * (3.0f - 2.0f * x);
            };
            const float expectedPeakDb =
                smooth(liveFret - 1.0f) * smooth(15.0f - liveFret)
                * 22.9327503f / liveFrequency;
            const auto current = dipState();
            const auto horizontal = dipState(true);
            expect(std::abs(current.peakDb - expectedPeakDb) < 2.0e-5f,
                   "fitted loss did not follow the continuous fret gate");
            expect(current.active == (current.peakDb > 0.0f)
                       && horizontal == current,
                   "fitted-loss polarisations diverged during legato (peak "
                       + std::to_string(current.peakDb) + "/"
                       + std::to_string(horizontal.peakDb) + ", active "
                       + std::to_string(current.active) + "/"
                       + std::to_string(horizontal.active) + ")");
            const bool rising = move.toActive && ! move.fromActive;
            const float currentLossRate = current.peakDb * liveFrequency;
            expect(rising
                       ? currentLossRate + 1.0e-5f >= previousLossRate
                       : currentLossRate <= previousLossRate + 1.0e-5f,
                   "fitted loss rate was not monotone through its boundary "
                   "taper (" + std::to_string(previousLossRate) + " -> "
                       + std::to_string(currentLossRate) + ")");
            previousLossRate = currentLossRate;

            if (! checkedFractionalPassivity
                && TestAccess::legatoBlend(engine, 0) >= 0.45f)
            {
                double peakMagnitude = 0.0;
                for (int bin = 0; bin <= 4096; ++bin)
                    peakMagnitude = std::max(
                        peakMagnitude,
                        responseMagnitude(
                            current.b0, current.b1, current.b2,
                            current.a1, current.a2,
                            3.14159265358979323846
                                * static_cast<double>(bin) / 4096.0));
                expect(peakMagnitude <= 1.0 + 2.0e-10,
                       "fractional fitted-loss taper added loop energy");
                checkedFractionalPassivity = true;
            }
            ++ticks;
        }

        const auto destination = dipState();
        expect(ticks > 1 && ticks < 512
                   && destination.active == move.toActive,
               "fitted-loss legato taper missed its exact endpoint (ticks "
                   + std::to_string(ticks) + ", peak "
                   + std::to_string(destination.peakDb) + ", active "
                   + std::to_string(destination.active) + ")");
        const float destinationPeakDb = move.toActive
            ? 22.9327503f / midiHz(move.to) : 0.0f;
        expect(std::abs(destination.peakDb - destinationPeakDb) < 1.0e-6f,
               "fitted-loss legato taper ended at the wrong depth");
        StereoBuffer destinationSettle(2400);
        renderInto(engine, destinationSettle);
        expect(std::abs(centsBetween(
                   TestAccess::effectiveLoopFrequency(engine, 0),
                   midiHz(move.to))) < 2.0,
               "fitted-loss legato taper ended detuned");
        const float scale = peakAbs(before.left);
        expect(transitionFinite && std::isfinite(maximumStep)
                   && maximumStep < std::max(0.05f, 1.2f * scale),
               "fitted-loss legato boundary produced a hard discontinuity");
    }

    // Hold sounding pitch fixed while the finger crosses the lower boundary.
    // Only filter phase and the compensating raw-delay translation may move;
    // without that translation the fitted section alone shifts F#1 by several
    // cents.
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0,
                                     192000.0, 384000.0 })
    {
        ElectryEngine phaseCompensated;
        phaseCompensated.prepare(sampleRate, 512);
        phaseCompensated.reset();
        TestAccess::retriggerVoice(phaseCompensated, 0, 29, 0.85f);
        StereoBuffer phaseSettle(static_cast<int>(0.12 * sampleRate));
        renderInto(phaseCompensated, phaseSettle);
        TestAccess::legatoRetargetVoice(phaseCompensated, 0, 30);
        TestAccess::setLegatoWithOpposingBend(phaseCompensated, 0, 0.0f);
        const std::array phaseBefore {
            TestAccess::effectiveLoopFrequency(phaseCompensated, 0),
            TestAccess::effectiveLoopFrequency(phaseCompensated, 0, true)
        };
        TestAccess::setLegatoWithOpposingBend(phaseCompensated, 0, 0.5f);
        for (int polarisation = 0; polarisation < 2; ++polarisation)
            expect(std::abs(centsBetween(
                       TestAccess::effectiveLoopFrequency(
                           phaseCompensated, 0, polarisation != 0),
                       phaseBefore[static_cast<std::size_t>(polarisation)]))
                       < 0.25,
                   "fitted-loss taper moved stationary effective pitch at "
                       + std::to_string(sampleRate) + " Hz");
    }

    // A new target reached before the old glide finishes must start from the
    // live filter, not either stale endpoint.
    ElectryEngine chained;
    chained.prepare(48000.0, 512);
    chained.reset();
    TestAccess::retriggerVoice(chained, 0, 29, 0.85f);
    StereoBuffer chainedSettle(5760);
    renderInto(chained, chainedSettle);
    TestAccess::legatoRetargetVoice(chained, 0, 30);
    int chainedTicks = 0;
    while (TestAccess::legatoBlend(chained, 0) < 0.40f
           && chainedTicks < 512)
    {
        StereoBuffer tick(8);
        renderInto(chained, tick);
        ++chainedTicks;
    }
    const auto chainedState = TestAccess::fittedLossState(chained, 0);
    TestAccess::legatoRetargetVoice(chained, 0, 29);
    expect(chainedTicks < 512 && chainedState.active
               && TestAccess::fittedLossState(chained, 0) == chainedState,
           "chained legato restarted the fitted-loss taper");
    int reversedTicks = 0;
    while (TestAccess::legatoBlend(chained, 0) < 1.0f
           && reversedTicks < 512)
    {
        StereoBuffer tick(8);
        renderInto(chained, tick);
        ++reversedTicks;
    }
    const auto reversedEndpoint = TestAccess::fittedLossState(chained, 0);
    expect(reversedTicks < 512 && ! reversedEndpoint.active
               && reversedEndpoint.peakDb == 0.0f,
           "chained fitted-loss reversal missed its exact bypass");

    // Eligibility is fret based. Wheel pitch cannot switch an unsupported
    // note on or change the full correction of a supported note.
    struct BentNote { int note; float bend; };
    for (const auto bentNote : {
        BentNote { 29, 1.0f }, BentNote { 30, -1.0f },
        BentNote { 42, 1.0f }, BentNote { 43, -1.0f } })
    {
        ElectryEngine bent;
        bent.prepare(48000.0, 512);
        bent.reset();
        TestAccess::retriggerVoice(bent, 0, bentNote.note, 0.85f);
        const auto before = TestAccess::fittedLossState(bent, 0);
        bent.setPitchBend(bentNote.bend);
        StereoBuffer bendSettle(16800);
        renderInto(bent, bendSettle);
        expect(TestAccess::fittedLossState(bent, 0) == before,
               "pitch wheel moved the fitted-loss evidence gate");
    }

    constexpr ElectryEngine::ExpressionId expressionId = 2;
    ElectryEngine expressed;
    expressed.prepare(48000.0, 512);
    expressed.reset();
    expressed.noteOn(30, 0.85f, expressionId);
    const auto memberBefore = TestAccess::fittedLossState(expressed, 0);
    expressed.setExpressionPitchBend(expressionId, -2.0f);
    StereoBuffer memberBendSettle(16800);
    renderInto(expressed, memberBendSettle);
    expect(memberBefore.active
               && TestAccess::fittedLossState(expressed, 0) == memberBefore,
           "MPE member bend moved the fitted-loss evidence gate");

    // Idle strings are open and therefore outside this fretted-data candidate.
    // Bending unsupported open E1 across F#1 must not carry the played filter
    // into the sympathetic loop or switch topology during the wheel gesture.
    ElectryEngine sympathetic;
    sympathetic.prepare(48000.0, 512);
    EngineParameters parameters;
    parameters.sympatheticAmount = 0.8f;
    sympathetic.setParameters(parameters);
    sympathetic.reset();
    sympathetic.setPitchBend(1.0f);
    sympathetic.noteOn(45, 0.9f);
    StereoBuffer settle(24000);
    renderInto(sympathetic, settle);
    expect(TestAccess::snapshot(sympathetic, 0).sympatheticReady
               && ! TestAccess::fittedLossState(sympathetic, 0).active,
           "pitch bend switched the unsupported sympathetic E1 correction on");
}
#endif

void testHandDipNeverExpands()
{
    constexpr double sampleRate = 48000.0;
    int activeConfigurations = 0;
    float worstMagnitude = 0.0f;
    double worstOmegaFraction = 0.0;

    for (const float pressure : { 0.0f, 0.15f, 0.55f, 1.0f })
    {
        for (const auto playStyle : { PlayStyle::PalmMute, PlayStyle::Sustain })
        {
            for (const float muteDamping : { 0.0f, 0.55f, 1.0f })
            {
                EngineParameters parameters;
                parameters.palmMute = pressure;
                parameters.muteDamping = muteDamping;

                ElectryEngine engine;
                engine.prepare(sampleRate, 512);
                engine.setParameters(parameters);

                // Sweep the playable range: the dip's centre tracks the
                // fundamental, so a high note pushes it toward Nyquist where the
                // coefficients are most awkward.
                for (int note = ElectryEngine::lowestPlayableNote;
                     note <= ElectryEngine::highestPlayableNote; note += 6)
                // Three ages, chosen to span the modulation rather than to
                // sample it densely: the fully engaged startup and two points
                // after energy-driven grip relaxation may begin. Denser sweeps
                // cost minutes and found nothing more.
                for (const int ageBlocks : { 0, 4, 45 })
                {
                    engine.allNotesOff();
                    engine.noteOn(styleKeyswitch(playStyle), 1.0f);
                    engine.noteOn(note, 0.9f);

                    // Sampled at several ages, not just after the first block.
                    // The dip starts at full solved depth, then may relax as the
                    // string stops driving the hand, so the bound has to hold at
                    // every depth that modulation produces, not only at note-on.
                    float left[512] {};
                    float right[512] {};
                    engine.process(left, right, 64);
                    for (int block = 0; block < ageBlocks; ++block)
                        engine.process(left, right, 512);

                    for (int stringIndex = 0;
                         stringIndex < ElectryEngine::stringCount; ++stringIndex)
                    {
                        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
                        double a1 = 0.0, a2 = 0.0;
                        bool active = false;
                        TestAccess::handDip(engine, stringIndex,
                                            b0, b1, b2, a1, a2, active);
                        if (! active)
                            continue;
                        ++activeConfigurations;

                        for (int step = 0; step <= 600; ++step)
                        {
                            const double omega =
                                3.14159265358979323846 * step / 600.0;
                            const double cw = std::cos(omega);
                            const double sw = std::sin(omega);
                            const double c2 = std::cos(2.0 * omega);
                            const double s2 = std::sin(2.0 * omega);
                            const double nr = b0 + b1 * cw + b2 * c2;
                            const double ni = -(b1 * sw + b2 * s2);
                            const double dr = 1.0 + a1 * cw + a2 * c2;
                            const double di = -(a1 * sw + a2 * s2);
                            const double dn = dr * dr + di * di;
                            if (dn < 1.0e-20)
                                continue;
                            const auto magnitude = static_cast<float>(
                                std::sqrt((nr * nr + ni * ni) / dn));
                            if (magnitude > worstMagnitude)
                            {
                                worstMagnitude = magnitude;
                                worstOmegaFraction = omega
                                    / 3.14159265358979323846;
                            }
                            // The bound is an equality at DC and Nyquist by
                            // construction, so the slack here is for double
                            // rounding only. It was 6.6e-4 out when the section
                            // ran in float, which is what put it in double.
                            if (! (magnitude <= 1.000001f))
                            {
                                std::printf("  |H| = %.6f at note %d, "
                                            "pressure %.2f, mute %.2f, "
                                            "age %d blocks\n",
                                            magnitude, note, pressure,
                                            muteDamping, ageBlocks);
                                expect(false, "hand loss dip expands inside "
                                              "the loop");
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    // A bound that holds because nothing was ever configured proves nothing.
    if (activeConfigurations < 100)
    {
        std::printf("  only %d active configurations\n", activeConfigurations);
        expect(false, "hand loss dip never engaged, so its bound is vacuous");
        return;
    }

    std::printf("Hand dip peak magnitude %.8f at omega/pi = %.6f over %d "
                "active configurations\n",
                worstMagnitude, worstOmegaFraction, activeConfigurations);
}

void testLowRegisterFundamentalWeight()
{
    constexpr double sampleRate = 48000.0;

    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.outputMode = electry::OutputMode::Mono;
    parameters.pickHardness = 0.85f;
    parameters.pickPosition = 0.18f;
    // Silence the deterministic mechanical noise: this measures where the
    // string's own energy sits, not the plectrum's.
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.artifactAmount = 0.0f;

    // Energy in a band as a fraction of the tone's total, sampled at the
    // partials so the figure measures the spectral envelope rather than window
    // leakage between harmonic lines - the same approach spectralCentroid takes.
    const auto bandFraction = [] (const std::vector<float>& data, int start,
                                  int length, double fundamentalHz,
                                  double lowHz, double highHz)
    {
        double inBand = 0.0;
        double total = 0.0;
        for (int partial = 1; partial <= 48; ++partial)
        {
            const double frequency = fundamentalHz * partial;
            if (frequency >= 0.45 * sampleRate)
                break;
            const double magnitude = dftMagnitude(data, start, length,
                                                  sampleRate, frequency);
            const double power = magnitude * magnitude;
            total += power;
            if (frequency >= lowHz && frequency < highHz)
                inBand += power;
        }
        return total > 0.0 ? inBand / total : 0.0;
    };

    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(parameters);

    // The open low E of the Drop-E instrument, MIDI 28, and its own fundamental.
    constexpr double lowE = 41.2034;
    const int window = static_cast<int>(0.25 * sampleRate);

    const auto open = renderNote(engine, sampleRate, 28, 0.9f,
                                 PlayStyle::Sustain, 1.2);
    const auto muted = renderNote(engine, sampleRate, 28, 0.9f,
                                  PlayStyle::PalmMute, 1.2);

    const double openFundamental = bandFraction(open.left, 0, window, lowE,
                                                0.0, 1.6 * lowE);
    const double mutedFundamental = bandFraction(muted.left, 0, window, lowE,
                                                 0.0, 1.6 * lowE);

    std::printf("PROBE Drop-E E1 fundamental fractions: %.6f open, %.6f muted\n",
                openFundamental, mutedFundamental);

    // With the comb cancelling exactly this sat near a twentieth of the tone's
    // energy, under persistent low mids, which is the hollow, clavinet-like
    // register the references do not have.
    // The threshold remains more than twice the exactly-cancelling comb's
    // historical 0.0039 result without pinning a moving attack to one FFT bin.
    expect(openFundamental > 0.008,
           "the open low E does not carry its own fundamental ("
               + std::to_string(openFundamental) + " of its energy)");

    // A bridge hand loads a string; it does not filter its fundamental out. A
    // broadband absorber took this to a small fraction of the open note's,
    // which is what read as a thin, cut-off pick instead of a chug.
    // A mute removes the top end, so the fundamental should end up a *larger*
    // share of what is left than on the open note, not a smaller one. A
    // A broadband hand historically measured 0.0091 here.
    expect(mutedFundamental > 0.018,
           "a palm mute strips the fundamental rather than damping the string ("
               + std::to_string(mutedFundamental) + " of its energy)");
    expect(mutedFundamental > openFundamental,
           "a muted low E is not more fundamental-dominated than an open one ("
               + std::to_string(mutedFundamental) + " muted against "
               + std::to_string(openFundamental) + " open)");

    // The comb must still be a position comb: weighting its delayed tap below
    // one shortens the null without removing the geometry, so the bridge
    // position stays the brighter of the two.
    EngineParameters neckParameters = parameters;
    neckParameters.pickupSelector = PickupSelector::Neck;
    ElectryEngine neckEngine;
    neckEngine.prepare(sampleRate, 512);
    neckEngine.setParameters(neckParameters);
    const auto neck = renderNote(neckEngine, sampleRate, 28, 0.9f,
                                 PlayStyle::Sustain, 1.2);
    const double neckFundamental = bandFraction(neck.left, 0, window, lowE,
                                                0.0, 1.6 * lowE);
    expect(neckFundamental > openFundamental,
           "the neck pickup does not sense more fundamental than the bridge ("
               + std::to_string(neckFundamental) + " against "
               + std::to_string(openFundamental) + ")");
}

// ---------------------------------------------------------------------------
// Version 1.1: display readout and fretboard geometry
// ---------------------------------------------------------------------------

void testVisualStateAndGeometry()
{
    namespace visuals = electry::visuals;
    constexpr int lastFret = ElectryEngine::fretCount;

    expect(visuals::fretWireFraction(0, lastFret) == 0.0f,
           "the nut is not at the start of the drawn neck");
    expect(std::abs(visuals::fretWireFraction(lastFret, lastFret) - 1.0f) < 1.0e-6f,
           "the last fret is not at the end of the drawn neck");
    // The twelfth fret sits at half the scale length, which on a 22-fret neck
    // is 0.5 / (1 - 2^(-22/12)) of the drawn span.
    const float octaveExpected = 0.5f / (1.0f - std::pow(2.0f, -22.0f / 12.0f));
    expect(std::abs(visuals::fretWireFraction(12, lastFret) - octaveExpected)
               < 1.0e-4f,
           "the twelfth fret is not at half the scale length");
    for (int fret = 1; fret <= lastFret; ++fret)
    {
        expect(visuals::fretWireFraction(fret, lastFret)
                   > visuals::fretWireFraction(fret - 1, lastFret),
               "fret positions are not monotonic at fret " + std::to_string(fret));
    }
    expect(visuals::fretWireFraction(2, lastFret) - visuals::fretWireFraction(1, lastFret)
               < visuals::fretWireFraction(1, lastFret),
           "fret spacing does not narrow toward the body");
    expect(visuals::fretWireFraction(-5, lastFret) == 0.0f
               && visuals::fretWireFraction(999, lastFret) == 1.0f,
           "fret geometry did not clamp out-of-range frets");
    expect(visuals::fretCentreFraction(0, lastFret) == 0.0f,
           "an open string is not drawn at the nut");
    expect(visuals::fretCentreFraction(5, lastFret)
                   > visuals::fretWireFraction(4, lastFret)
               && visuals::fretCentreFraction(5, lastFret)
                      < visuals::fretWireFraction(5, lastFret),
           "a fingered note is not between its enclosing fret wires");

    // The span-aware overloads exist so a caller solving many wire/centre
    // positions against the same neck - the editor's paint() does - can share
    // one fretSpan() call instead of paying for it again on every fret; they
    // must therefore agree exactly with the two-argument forms that solve
    // their own span internally.
    const float span = visuals::fretSpan(lastFret);
    for (int fret = -2; fret <= lastFret + 2; ++fret)
    {
        expect(visuals::fretWireFraction(fret, lastFret, span)
                   == visuals::fretWireFraction(fret, lastFret),
               "the span-aware fretWireFraction disagrees with the two-argument "
               "form at fret " + std::to_string(fret));
        expect(visuals::fretCentreFraction(fret, lastFret, span)
                   == visuals::fretCentreFraction(fret, lastFret),
               "the span-aware fretCentreFraction disagrees with the two-argument "
               "form at fret " + std::to_string(fret));
    }
    expect(visuals::fretSpan(0) == 0.0f && visuals::fretSpan(-3) == 0.0f,
           "fretSpan did not report zero for a degenerate neck");

    for (int s = 1; s < ElectryEngine::stringCount; ++s)
    {
        expect(visuals::stringRowFraction(s, ElectryEngine::stringCount, 0.085f)
                   > visuals::stringRowFraction(s - 1, ElectryEngine::stringCount,
                                                0.085f),
               "string rows are not ordered low to high");
        expect(visuals::stringThickness(s, 0.9f, 2.6f)
                   < visuals::stringThickness(s - 1, 0.9f, 2.6f),
               "string thickness does not taper toward the top string");
    }
    expect(visuals::stringRowFraction(0, ElectryEngine::stringCount, 0.085f) >= 0.085f
               && visuals::stringRowFraction(7, ElectryEngine::stringCount, 0.085f)
                      <= 0.915f,
           "string rows escaped the drawn fingerboard");

    float level = 0.0f;
    level = visuals::meterBallistics(level, 1.0f, 0.55f, 0.18f);
    expect(level > 0.5f, "meter attack is too slow to show an attack");
    const float afterAttack = level;
    level = visuals::meterBallistics(level, 0.0f, 0.55f, 0.18f);
    expect(level < afterAttack && level > 0.5f * afterAttack,
           "meter release does not hold the reading");
    for (int i = 0; i < 200; ++i)
        level = visuals::meterBallistics(level, 0.0f, 0.55f, 0.18f);
    expect(level < 1.0e-6f, "meter did not settle back to zero");
    expect(visuals::meterBallistics(std::numeric_limits<float>::quiet_NaN(), 0.5f,
                                    0.5f, 0.5f) == 0.25f,
           "meter ballistics did not recover from a non-finite reading");

    expect(visuals::vibrationShape(0.0f, 0.0f) < 1.0e-6f
               && visuals::vibrationShape(1.0f, 0.0f) < 1.0e-6f,
           "an open string is not pinned at the nut and bridge");
    expect(std::abs(visuals::vibrationShape(0.5f, 0.0f) - 1.0f) < 1.0e-5f,
           "an open string does not peak at its midpoint");
    expect(visuals::vibrationShape(0.2f, 0.5f) == 0.0f,
           "the string moves behind the fretting finger");
    expect(std::abs(visuals::vibrationShape(0.75f, 0.5f) - 1.0f) < 1.0e-5f,
           "a fretted string does not peak at the middle of its sounding length");
    expect(visuals::levelHeat(0.0f) == 0.0f
               && std::abs(visuals::levelHeat(1.0f) - 1.0f) < 1.0e-6f
               && visuals::levelHeat(0.25f) > 0.25f,
           "the level-to-heat curve is not a monotonic knee");

    // Packing is lossless for everything the display needs.
    for (int note = -1; note <= 127; note += 13)
    {
        for (int fret = -1; fret <= ElectryEngine::fretCount; fret += 5)
        {
            electry::StringVisualState state;
            state.midiNote = note;
            state.fret = fret;
            state.sounding = (note % 2) == 0;
            state.sympathetic = (fret % 2) == 0;
            state.releasing = (note % 3) == 0;
            state.level = 0.5f;
            state.playStyle = static_cast<PlayStyle>(
                (note + 1) % ElectryEngine::playStyleKeyswitchCount);
            state.strokeUp = (note % 5) == 0;
            const auto round = visuals::unpackStringVisual(
                visuals::packStringVisual(state));
            expect(round.midiNote == state.midiNote && round.fret == state.fret
                       && round.sounding == state.sounding
                       && round.sympathetic == state.sympathetic
                       && round.releasing == state.releasing
                       && round.playStyle == state.playStyle
                       && round.strokeUp == state.strokeUp
                       && std::abs(round.level - state.level) < 0.005f,
                   "packed string state did not survive a round trip");
        }
    }

    // The engine's readout names the right physical string, fret and note.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.sympatheticAmount = 0.8f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(styleKeyswitch(PlayStyle::PalmMute), 1.0f);
    engine.noteOn(pickKeyswitch(PickStyle::Up), 1.0f);
    engine.noteOn(45, 0.95f);
    // A firm mute's decay target is short, so the readout has to be sampled
    // while the note is genuinely sounding: a quarter of a second in, the
    // string is far down and the voice may have correctly retired.
    StereoBuffer buffer(static_cast<int>(0.10 * sampleRate));
    renderInto(engine, buffer);

    std::array<electry::StringVisualState, ElectryEngine::stringCount> visual {};
    engine.getStringVisualState(visual);
    expect(visual[3].sounding && visual[3].midiNote == 45 && visual[3].fret == 0
               && visual[3].playStyle == PlayStyle::PalmMute
               && visual[3].strokeUp,
           "the display readout did not identify the played open A string");
    expect(visual[3].level > 0.0f, "a struck string reported no display level");
    expect(! visual[0].sounding, "an unplayed string reported a played note");

    int ringing = 0;
    for (const auto& state : visual)
        if (state.sympathetic)
        {
            ++ringing;
            expect(state.midiNote >= ElectryEngine::lowestPlayableNote
                       && state.fret == 0,
                   "a coupled string reported an implausible open note");
        }
    expect(ringing == engine.getSympatheticStringCount(),
           "the coupled-string count disagrees with the per-string readout");

    engine.reset();
    engine.getStringVisualState(visual);
    for (const auto& state : visual)
        expect(! state.sounding && ! state.sympathetic && state.midiNote == -1
                   && state.level == 0.0f,
               "a reset engine still reported a sounding string");
}

// The editor reads these helpers directly off the lock-free audio-to-editor
// transfer, so a non-finite or out-of-range value has to be recovered here
// rather than upstream: `testVisualStateAndGeometry` above exercises every
// helper's ordinary range and only `meterBallistics`' non-finite `current`
// guard, leaving `levelHeat`'s own guard, `meterBallistics`' non-finite
// `target` guard, `vibrationShape`'s own guard on either argument, and
// `packStringVisual`'s non-finite level and out-of-range note/fret/playStyle
// clamps unexercised by any existing test.
void testVisualStateSanitizesNonFiniteInput()
{
    namespace visuals = electry::visuals;
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();

    expect(visuals::levelHeat(nan) == 0.0f,
           "levelHeat did not recover from a non-finite level");
    expect(visuals::levelHeat(inf) == 0.0f,
           "levelHeat did not recover from a positive-infinite level");
    expect(visuals::levelHeat(-inf) == 0.0f,
           "levelHeat did not recover from a negative-infinite level");

    // A non-finite target has to sanitize to zero, exactly like a non-finite
    // current does above: with current already at rest, a NaN target must
    // leave the meter at rest rather than latching NaN into the display.
    expect(visuals::meterBallistics(0.0f, nan, 0.5f, 0.5f) == 0.0f,
           "meterBallistics did not recover from a non-finite target");
    // With current above the sanitized zero target, the non-finite target
    // takes the release branch and the reading has to fall rather than hold
    // or grow, so a poisoned target cannot freeze the display at a stale
    // level.
    const float releasing = visuals::meterBallistics(1.0f, inf, 0.4f, 0.4f);
    expect(releasing < 1.0f && releasing >= 0.0f,
           "meterBallistics did not release toward zero for a non-finite target");

    // vibrationShape() clamped both arguments but, unlike its siblings above,
    // never checked either for non-finite before clampf() ran: clampf's
    // comparisons all fail on NaN, so a NaN survived the clamp unchanged and
    // reached std::sin() below, drawing a NaN offset into the string path
    // instead of leaving it at rest.
    expect(visuals::vibrationShape(nan, 0.0f) == 0.0f,
           "vibrationShape did not recover from a non-finite position");
    expect(visuals::vibrationShape(inf, 0.0f) == 0.0f,
           "vibrationShape did not recover from an infinite position");
    expect(visuals::vibrationShape(0.5f, nan) == 0.0f,
           "vibrationShape did not recover from a non-finite stopped fraction");
    expect(visuals::vibrationShape(0.5f, inf) == 0.0f,
           "vibrationShape did not recover from an infinite stopped fraction");

    // packStringVisual's own guards: a non-finite level packs to silent
    // rather than propagating NaN through the lock-free word, and a
    // wildly out-of-range note, fret or play style clamps to the nearest
    // valid value instead of wrapping into an unrelated field via the
    // packed word's bit layout.
    electry::StringVisualState poisoned;
    poisoned.midiNote = 4000;
    poisoned.fret = -900;
    poisoned.level = nan;
    poisoned.playStyle = static_cast<PlayStyle>(999);
    const auto packed = visuals::packStringVisual(poisoned);
    const auto round = visuals::unpackStringVisual(packed);
    expect(round.midiNote == 127,
           "packStringVisual did not clamp an out-of-range high note");
    expect(round.fret == -1,
           "packStringVisual did not clamp an out-of-range low fret");
    expect(round.level == 0.0f,
           "packStringVisual did not sanitize a non-finite level");
    expect(round.playStyle == PlayStyle::Dead,
           "packStringVisual did not clamp an out-of-range play style");

    electry::StringVisualState poisonedLow;
    poisonedLow.midiNote = -900;
    poisonedLow.fret = 900;
    const auto roundLow = visuals::unpackStringVisual(
        visuals::packStringVisual(poisonedLow));
    expect(roundLow.midiNote == -1,
           "packStringVisual did not clamp an out-of-range low note");
    expect(roundLow.fret == ElectryEngine::fretCount,
           "packStringVisual did not clamp an out-of-range high fret");
}

// stringRowFraction and stringThickness each clamp their string index to the
// modelled set, and stringRowFraction separately clamps its inset and
// special-cases a degenerate one-or-fewer-string count; none of those guards
// were exercised by testVisualStateAndGeometry above, which only ever fed
// them ordinary in-range indices and a fixed, in-range inset. The
// span-aware fretWireFraction/fretCentreFraction overloads also fall back to
// zero for a non-positive span, a defensive branch fretSpan() itself never
// produces for the engine's fixed, positive fret count but that a caller
// could still reach directly.
void testVisualGeometryClampsOutOfRangeInput()
{
    namespace visuals = electry::visuals;
    constexpr int lastFret = ElectryEngine::fretCount;
    constexpr int stringCount = ElectryEngine::stringCount;

    expect(visuals::stringRowFraction(-5, stringCount, 0.085f)
               == visuals::stringRowFraction(0, stringCount, 0.085f),
           "stringRowFraction did not clamp a negative string index");
    expect(visuals::stringRowFraction(999, stringCount, 0.085f)
               == visuals::stringRowFraction(stringCount - 1, stringCount, 0.085f),
           "stringRowFraction did not clamp an out-of-range high string index");
    expect(visuals::stringRowFraction(0, 1, 0.085f) == 0.5f,
           "stringRowFraction did not centre a single-string layout");
    expect(visuals::stringRowFraction(0, 0, 0.085f) == 0.5f,
           "stringRowFraction did not centre a degenerate zero-string layout");
    expect(visuals::stringRowFraction(0, stringCount, 0.9f)
               == visuals::stringRowFraction(0, stringCount, 0.45f),
           "stringRowFraction did not clamp an inset above its 0.45 ceiling");
    expect(visuals::stringRowFraction(0, stringCount, -0.3f)
               == visuals::stringRowFraction(0, stringCount, 0.0f),
           "stringRowFraction did not clamp a negative inset");

    expect(visuals::stringThickness(-3, 0.9f, 2.6f)
               == visuals::stringThickness(0, 0.9f, 2.6f),
           "stringThickness did not clamp a negative string index");
    expect(visuals::stringThickness(999, 0.9f, 2.6f)
               == visuals::stringThickness(stringCount - 1, 0.9f, 2.6f),
           "stringThickness did not clamp an out-of-range high string index");

    expect(visuals::fretWireFraction(5, lastFret, 0.0f) == 0.0f,
           "the span-aware fretWireFraction did not fall back to zero for a "
           "zero span");
    expect(visuals::fretWireFraction(5, lastFret, -1.0f) == 0.0f,
           "the span-aware fretWireFraction did not fall back to zero for a "
           "negative span");
    expect(visuals::fretCentreFraction(5, lastFret, 0.0f) == 0.0f,
           "the span-aware fretCentreFraction did not fall back to zero for a "
           "zero span");
}

// ---------------------------------------------------------------------------
// Version 1.1: the paths the efficiency work depends on
// ---------------------------------------------------------------------------

void testPickupCullingAndChannelLinking()
{
    constexpr double sampleRate = 48000.0;

    const auto settle = [] (ElectryEngine& engine, double seconds)
    {
        StereoBuffer buffer(static_cast<int>(seconds * sampleRate));
        renderInto(engine, buffer);
        return buffer;
    };

    struct SelectorCase { PickupSelector selector; bool neck; bool bridge; };
    for (const auto& item : { SelectorCase { PickupSelector::Bridge, false, true },
                              SelectorCase { PickupSelector::Neck, true, false },
                              SelectorCase { PickupSelector::Both, true, true } })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupSelector = item.selector;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.8f);
        const auto audio = settle(engine, 0.2);
        expect(TestAccess::pickupPathActive(engine, true) == item.neck,
               "the neck pickup path was not culled to match the selector");
        expect(TestAccess::pickupPathActive(engine, false) == item.bridge,
               "the bridge pickup path was not culled to match the selector");
        expect(allFinite(audio) && peakAbs(audio.left) > 1.0e-5f,
               "a selector position rendered silence");

        constexpr int idleString = 0;
        expect(TestAccess::snapshot(engine, idleString).sympatheticReady,
               "the pickup-culling fixture did not wake idle E1");
        const std::array<bool, 3> populated { true, true, true };
        const std::array<bool, 3> clear { false, false, false };
        expect(TestAccess::pickupHistory(engine, idleString, true)
                   == (item.neck ? populated : clear),
               "the idle neck pickup history did not follow its culling state");
        expect(TestAccess::pickupHistory(engine, idleString, false)
                   == (item.bridge ? populated : clear),
               "the idle bridge pickup history did not follow its culling state");
    }

    // Bringing a culled pickup back must not click: its aperture and EMF
    // memory is cleared and the selector mix fades it in over about 4 ms.
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.artifactAmount = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(45, 0.9f);
    settle(engine, 0.30);
    parameters.pickupSelector = PickupSelector::Both;
    engine.setParameters(parameters);
    const auto crossfade = settle(engine, 0.10);
    expect(allFinite(crossfade), "switching pickups produced non-finite audio");
    float largestStep = 0.0f;
    for (std::size_t i = 1; i < crossfade.left.size(); ++i)
        largestStep = std::max(largestStep,
                               std::abs(crossfade.left[i] - crossfade.left[i - 1]));
    expect(largestStep < 0.08f,
           "restoring a culled pickup produced a discontinuity ("
               + std::to_string(largestStep) + ")");

    // Mono runs one shared output chain and is bit-identical dual mono;
    // opening the field copies its state across without a discontinuity.
    ElectryEngine field;
    field.prepare(sampleRate, 512);
    EngineParameters mono;
    mono.outputMode = electry::OutputMode::Mono;
    field.setParameters(mono);
    field.reset();
    field.noteOn(40, 0.9f);
    StereoBuffer monoAudio(static_cast<int>(0.25 * sampleRate));
    renderInto(field, monoAudio);
    expect(TestAccess::channelsLinked(field),
           "Mono did not link the shared output chain");
    expect(monoAudio.left == monoAudio.right,
           "Mono is not sample-identical dual mono");

    EngineParameters stereo = mono;
    stereo.outputMode = electry::OutputMode::Stereo;
    field.setParameters(stereo);
    StereoBuffer opening(static_cast<int>(0.20 * sampleRate));
    renderInto(field, opening);
    expect(! TestAccess::channelsLinked(field),
           "Stereo did not unlink the shared output chain");
    float largestFieldStep = 0.0f;
    for (std::size_t i = 1; i < opening.right.size(); ++i)
        largestFieldStep = std::max(
            largestFieldStep, std::abs(opening.right[i] - opening.right[i - 1]));
    expect(allFinite(opening) && largestFieldStep < 0.08f,
           "opening the stereo field produced a discontinuity ("
               + std::to_string(largestFieldStep) + ")");
}

// ---------------------------------------------------------------------------
// Rate invariance, polarisation coupling, and the coupled strings' own loss
// ---------------------------------------------------------------------------

// Energy in a band, summed over logarithmically spaced probe frequencies.
double bandEnergyDb(const std::vector<float>& data, int start, int length,
                    double sampleRate, double lowHz, double highHz)
{
    double energy = 0.0;
    for (double f = lowHz; f < highHz; f *= 1.03)
    {
        const double m = dftMagnitude(data, start, length, sampleRate, f);
        energy += m * m;
    }
    return 10.0 * std::log10(energy + 1.0e-30);
}

// A humbucker's two-point cancellation notch sits at c/2d, where d is the
// distance between its coils and c is the string's transverse wave speed.
// Modelling it as one wide rectangular window instead put the notch at c/W,
// most of an octave too high - 5507 Hz on string 2 where Lemme reports a low E
// notching at about 3000 Hz, and 7351 Hz on string 3 against about 4000 Hz.
//
// Moving the notch is the smaller half of what the two coils do. The larger
// half is that two narrow windows pass a top octave one wide window was
// throwing away, so the assertions below bound the broadband change as well
// as placing the notch. What they cannot do is bound it per octave band
// against the shipping engine: the misplaced null on the wound strings sat
// *inside* the 4-8 kHz band, so moving it out of that band necessarily raises
// it, and on the plain strings the change goes the other way. See the plan
// document for the measurement that retired that bound.
void testPickupGeometryFollowsLiveWaveSpeed()
{
    constexpr int note = 45; // open A2, clear of every pickup-delay clamp
    constexpr double sampleRate = 48000.0;
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Both;
    parameters.pickupType = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.bendTimeSeconds = 0.040f;

    const auto geometryAt = [&] (float bendSemitones)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        // The legacy wheel's public range is +/-1 for +/-2 semitones. reset()
        // snaps the preserved target so this also covers a pre-bent Note On.
        engine.setPitchBend(0.5f * bendSemitones);
        engine.reset();
        engine.noteOn(note, 0.8f);
        const int stringIndex = TestAccess::stringForNote(engine, note);
        expect(stringIndex >= 0,
               "the pickup wave-speed probe did not allocate A2");
        return TestAccess::pickupGeometry(engine, std::max(stringIndex, 0));
    };

    const auto expectScaled = [&] (const TestAccess::PickupGeometry& rest,
                                   const TestAccess::PickupGeometry& moved,
                                   float semitones,
                                   const std::string& context)
    {
        const float expectedRatio = std::exp2(semitones / 12.0f);
        const float expectedDelayScale = 1.0f / expectedRatio;
        expect(std::abs(moved.waveSpeedRatio / expectedRatio - 1.0f) < 2.0e-5f,
               context + " cached the wrong transverse wave-speed ratio ("
                   + std::to_string(moved.waveSpeedRatio) + ")");
        const std::array before { rest.neckTapDelay, rest.bridgeTapDelay,
                                  rest.apertureDelay, rest.coilDelay };
        const std::array after { moved.neckTapDelay, moved.bridgeTapDelay,
                                 moved.apertureDelay, moved.coilDelay };
        const std::array stages { "neck tap", "bridge tap", "aperture",
                                  "coil spacing" };
        for (std::size_t index = 0; index < before.size(); ++index)
        {
            const float actualScale = after[index]
                / std::max(before[index], 1.0e-6f);
            expect(std::abs(actualScale / expectedDelayScale - 1.0f) < 2.0e-4f,
                   context + " left the " + stages[index]
                       + " outside the live wave-speed coordinate ("
                       + std::to_string(actualScale) + " vs "
                       + std::to_string(expectedDelayScale) + ")");
        }
    };

    // All four spatial stages are metres divided by c. This pre-bent Note On
    // covers the legacy wheel.
    const auto rest = geometryAt(0.0f);
    expect(rest.coilDelay > 1.0f,
           "the pickup wave-speed fixture did not retain two coils");
    expectScaled(rest, geometryAt(2.0f), 2.0f, "+2-semitone pre-bend");

#if ELECTRY_ENERGY_ATTACK_PITCH
    // Legato and damping solves have their own pitch cache. Advancing it first
    // must not consume a smaller attack-tension move before pickup c sees it.
    {
        constexpr float attackFactor = 1.003f;
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(note, 0.8f);
        const int stringIndex = TestAccess::stringForNote(engine, note);
        expect(stringIndex >= 0,
               "the attack-cache pickup probe did not allocate A2");
        const int safeStringIndex = std::max(stringIndex, 0);
        const auto before =
            TestAccess::pickupGeometry(engine, safeStringIndex);
        TestAccess::setAttackPitchAfterLoopCacheAdvanced(
            engine, safeStringIndex, attackFactor);
        expectScaled(before, TestAccess::pickupGeometry(
                               engine, safeStringIndex),
                     12.0f * std::log2(attackFactor),
                     "pickup-owned attack-tension cache");
    }
#endif

    // Member expression reaches configureVoicePitch through separate state.
    {
        constexpr ElectryEngine::ExpressionId expressionId = 3;
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.setExpressionPitchBend(expressionId, 2.0f);
        engine.reset();
        engine.noteOn(note, 0.8f, expressionId);
        const int stringIndex = TestAccess::stringForNote(engine, note);
        expect(stringIndex >= 0,
               "the expression pickup probe did not allocate A2");
        expectScaled(rest, TestAccess::pickupGeometry(
                               engine, std::max(stringIndex, 0)),
                     2.0f, "member expression bend");
    }

    // Reconfiguring the live geometry must not clear the spatial FIRs or the
    // Faraday differentiator. A reset would turn every control tick into a
    // false attack even if the steady-state delay were right.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(note, 0.8f);
        StereoBuffer establish(static_cast<int>(0.08 * sampleRate));
        renderInto(engine, establish, 17);
        const int stringIndex = TestAccess::stringForNote(engine, note);
        expect(TestAccess::refreshPickupWaveSpeedPreservesHistory(
                   engine, stringIndex, std::exp2(2.0f / 12.0f)),
               "a wave-speed refresh reset populated pickup/EMF history");
    }

    // A pure slide changes speaking length and pitch reciprocally, so it must
    // not masquerade as a tension change. Absolute pickup delays stay fixed.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(note, 0.8f);
        const int stringIndex = TestAccess::stringForNote(engine, note);
        const auto before = TestAccess::pickupGeometry(engine, stringIndex);
        engine.noteOn(styleKeyswitch(PlayStyle::Slide), 1.0f);
        engine.noteOn(note + 2, 0.8f);
        StereoBuffer travel(static_cast<int>(0.015 * sampleRate));
        renderInto(engine, travel, 17);
        const auto after = TestAccess::pickupGeometry(engine, stringIndex);
        expect(TestAccess::legatoBlend(engine, stringIndex) > 0.35f
                   && TestAccess::legatoBlend(engine, stringIndex) < 0.65f,
               "the pickup slide fixture missed the travelling finger");
        expect(std::abs(after.waveSpeedRatio - 1.0f) < 1.0e-7f
                   && std::abs(after.neckTapDelay / before.neckTapDelay - 1.0f)
                          < 2.0e-5f
                   && std::abs(after.bridgeTapDelay / before.bridgeTapDelay - 1.0f)
                          < 2.0e-5f
                   && std::abs(after.apertureDelay / before.apertureDelay - 1.0f)
                          < 2.0e-5f
                   && std::abs(after.coilDelay / before.coilDelay - 1.0f)
                          < 2.0e-5f,
               "a zero-bend slide changed transverse wave speed");

        // Opposing wheel travel can hold total pitch still while tension moves.
        // Stay below the dispersion fit's 0.06-fret quantum so this specifically
        // proves that pickup geometry follows its own tension coordinate.
        TestAccess::setLegatoWithOpposingBend(engine, stringIndex, 0.0f);
        const auto opposedStart =
            TestAccess::pickupGeometry(engine, stringIndex);
        TestAccess::setLegatoWithOpposingBend(engine, stringIndex, 0.10f);
        const auto opposedMoved =
            TestAccess::pickupGeometry(engine, stringIndex);
        const float fromSemitones = 12.0f * std::log2(
            TestAccess::legatoFromFrequency(engine, stringIndex)
            / TestAccess::snapshot(engine, stringIndex).baseFrequency);
        const float bendStart = -fromSemitones;
        const float bendMoved = -fromSemitones
            * (1.0f - electry::smoothStep(0.10f));
        const float expectedRatioChange =
            std::exp2((bendMoved - bendStart) / 12.0f);
        expect(std::abs(opposedMoved.waveSpeedRatio
                            / (opposedStart.waveSpeedRatio
                               * expectedRatioChange)
                        - 1.0f) < 2.0e-5f,
               "opposing slide/bend left pickup geometry on the pitch gate");
    }

    // A live finger vibrato is the other tension path. Use the voice's actual
    // semitone excursion rather than deriving the expectation from this cache.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(note + 2, 0.8f);
        const int stringIndex = TestAccess::stringForNote(engine, note + 2);
        const auto before = TestAccess::pickupGeometry(engine, stringIndex);
        engine.setVibrato(1.0f);
        const int tickFrames = TestAccess::hostFramesPerControlPeriod(engine);
        StereoBuffer tick(tickFrames);
        auto widest = before;
        float widestSemitones = 0.0f;
        for (int frame = 0; frame < static_cast<int>(1.0 * sampleRate);
             frame += tickFrames)
        {
            renderInto(engine, tick, tickFrames);
            const auto current = TestAccess::pickupGeometry(engine, stringIndex);
            const float currentSemitones =
                TestAccess::snapshot(engine, stringIndex).vibratoSemitones;
#if ELECTRY_ENERGY_ATTACK_PITCH
            const float currentTensionSemitones = currentSemitones
                + 12.0f * std::log2(TestAccess::attackPitchState(
                                        engine, stringIndex).frequencyFactor);
#else
            const float currentTensionSemitones = currentSemitones;
#endif
            // Inspect a tick that actually crossed the engine's 0.0008-semitone
            // pitch quantum; between such ticks the instantaneous gesture may
            // move while both the loop target and pickup geometry correctly hold.
            if (current.waveSpeedRatio > widest.waveSpeedRatio + 1.0e-7f)
            {
                widestSemitones = currentTensionSemitones;
                widest = current;
            }
        }
        expect(widestSemitones > 0.08f,
               "the vibrato fixture never developed a measurable tension bend");
        expectScaled(before, widest, widestSemitones,
                     "live fretting-hand vibrato");
    }

    // An idle open string is the same vibrating steel under the same pickups.
    // It must therefore use the played path's position, aperture and coil-pair
    // geometry, including the live 1/c coordinate and exact single-coil end.
    {
        auto coupledParameters = parameters;
        coupledParameters.sympatheticAmount = 1.0f;
        coupledParameters.stringGauge = 0.0f;
        ElectryEngine coupled;
        coupled.prepare(sampleRate, 512);
        coupled.setParameters(coupledParameters);
        coupled.reset();
        coupled.noteOn(note, 0.9f);
        StereoBuffer wake(static_cast<int>(0.12 * sampleRate));
        renderInto(coupled, wake);

        constexpr int idleString = 0; // open Drop-E E1
        expect(TestAccess::snapshot(coupled, idleString).sympatheticReady,
               "the sympathetic pickup fixture did not wake idle E1");
        const auto idleRest = TestAccess::pickupGeometry(coupled, idleString);
        const auto idleDispersion = TestAccess::snapshot(
            coupled, idleString);
        const auto idleFilterState = TestAccess::loopFilterState(
            coupled, idleString);

        ElectryEngine played;
        played.prepare(sampleRate, 512);
        auto playedParameters = coupledParameters;
        playedParameters.sympatheticAmount = 0.0f;
        played.setParameters(playedParameters);
        played.reset();
        played.noteOn(ElectryEngine::lowestPlayableNote, 0.9f);
        const auto playedOpen = TestAccess::pickupGeometry(played, idleString);
        const auto playedDispersion = TestAccess::snapshot(
            played, idleString);
        expect(idleRest.neckTapDelay == playedOpen.neckTapDelay
                   && idleRest.bridgeTapDelay == playedOpen.bridgeTapDelay
                   && idleRest.apertureDelay == playedOpen.apertureDelay
                   && idleRest.coilDelay == playedOpen.coilDelay,
               "idle E1 did not reuse the played open string's pickup geometry");
        expect(idleRest.neckTapDelay > idleRest.bridgeTapDelay * 3.0f
                   && TestAccess::coilPairActive(coupled, idleString, true)
                   && TestAccess::coilPairActive(coupled, idleString, false),
               "idle E1 did not expose distinct pickup positions and two coils");
        expect(idleDispersion.inharmonicity
                       == playedDispersion.inharmonicity
                   && idleDispersion.dispersionLowCoefficient
                       == playedDispersion.dispersionLowCoefficient
                   && idleDispersion.dispersionHighCoefficient
                       == playedDispersion.dispersionHighCoefficient
                   && idleDispersion.dispersionLowPartial
                       == playedDispersion.dispersionLowPartial
                   && idleDispersion.dispersionHighPartial
                       == playedDispersion.dispersionHighPartial,
               "idle E1 did not reuse the played open string's stiffness fit");
        expect(idleDispersion.inharmonicity > 0.0f
                   && std::all_of(idleFilterState.begin() + 1,
                                  idleFilterState.end(),
                                  [] (float state)
                                  {
                                      return std::isfinite(state)
                                          && state != 0.0f;
                                  }),
               "idle E1 did not run its eight-stage dispersion cascade");
        const auto expectIdleTuning = [&] (const std::string& context)
        {
            const float effective = TestAccess::effectiveLoopFrequency(
                coupled, idleString, false, true);
            expect(std::isfinite(effective)
                       && std::abs(centsBetween(
                              effective,
                              TestAccess::sympatheticFrequency(
                                  coupled, idleString))) < 0.25,
                   context + " detuned idle E1");
        };
        expectIdleTuning("sympathetic dispersion phase compensation");

        coupledParameters.pickupType = 1.0f;
        coupled.setParameters(coupledParameters);
        // The endpoint snap waits for the ordinary 14 ms smoothing tail to
        // enter its 1e-4 deadband. Give both directions the full tail before
        // inspecting geometry or starting the independent bend gesture.
        StereoBuffer endpointSettle(static_cast<int>(0.20 * sampleRate));
        renderInto(coupled, endpointSettle, 17);
        const auto singleCoil = TestAccess::pickupGeometry(coupled, idleString);
        expect(singleCoil.coilDelay == 0.0f
                   && ! TestAccess::coilPairActive(coupled, idleString, true)
                   && ! TestAccess::coilPairActive(coupled, idleString, false)
                   && singleCoil.apertureDelay == idleRest.apertureDelay,
               "idle E1 missed the exact single-coil pickup endpoint");

        coupledParameters.pickupType = 0.0f;
        coupled.setParameters(coupledParameters);
        renderInto(coupled, endpointSettle, 17);
        const auto humbuckerRestored =
            TestAccess::pickupGeometry(coupled, idleString);
        expect(humbuckerRestored.neckTapDelay == idleRest.neckTapDelay
                   && humbuckerRestored.bridgeTapDelay
                       == idleRest.bridgeTapDelay
                   && humbuckerRestored.apertureDelay
                       == idleRest.apertureDelay
                   && humbuckerRestored.coilDelay == idleRest.coilDelay
                   && TestAccess::coilPairActive(coupled, idleString, true)
                   && TestAccess::coilPairActive(coupled, idleString, false),
               "idle E1 did not restore its humbucker pickup geometry");

        float bendHighWater = TestAccess::effectiveLoopFrequency(
            coupled, idleString);
        double bendWorstDrawdownCents = 0.0;
        coupled.setPitchBend(1.0f);
        const int bendTickFrames = TestAccess::hostFramesPerControlPeriod(
            coupled);
        StereoBuffer bendTick(bendTickFrames);
        for (int rendered = 0;
             rendered < static_cast<int>(0.20 * sampleRate);
             rendered += bendTickFrames)
        {
            renderInto(coupled, bendTick, bendTickFrames);
            const float current = TestAccess::effectiveLoopFrequency(
                coupled, idleString);
            bendHighWater = std::max(bendHighWater, current);
            bendWorstDrawdownCents = std::max(
                bendWorstDrawdownCents,
                centsBetween(bendHighWater, current));
        }
        expect(bendWorstDrawdownCents < 0.05,
               "a rising sympathetic-string bend fell "
                   + std::to_string(bendWorstDrawdownCents)
                   + " cents at a loop-filter refit");
        const auto idleBentGeometry = TestAccess::pickupGeometry(
            coupled, idleString);
        const auto idleBentDispersion = TestAccess::snapshot(
            coupled, idleString);
        const float bendSemitones = 12.0f * std::log2(
            TestAccess::sympatheticFrequency(coupled, idleString)
            / midiHz(ElectryEngine::lowestPlayableNote));
        expect(bendSemitones > 1.9f,
               "the sympathetic pickup fixture did not reach its bend");
        expectScaled(humbuckerRestored, idleBentGeometry, bendSemitones,
                     "live sympathetic-string bend");
        const float fittedBend = TestAccess::lastConfiguredSemitones(
            coupled, idleString);
        const float expectedBendRatio = std::exp2(-fittedBend / 6.0f);
        expect(fittedBend > 1.9f
                   && std::abs(idleBentDispersion.inharmonicity
                                   / idleDispersion.inharmonicity
                                   / expectedBendRatio
                               - 1.0f) < 2.0e-5f,
               "live sympathetic-string bend left stiffness outside the "
               "1/T law");
        expectIdleTuning("live sympathetic-string bend");

        // Gauge changes alter flexural rigidity, even while this unowned
        // string is already ringing. This also pins the inactive-voice cache
        // invalidation: without it B remains on the light string exactly.
        coupledParameters.stringGauge = 1.0f;
        coupled.setParameters(coupledParameters);
        renderInto(coupled, endpointSettle, 17);
        const auto rebuiltDispersion = TestAccess::snapshot(
            coupled, idleString);
        const float rebuiltFit = TestAccess::lastConfiguredSemitones(
            coupled, idleString);
        constexpr float gaugeBRatio = 121.0f / 81.0f;
        const float expectedRebuiltRatio = gaugeBRatio
            * std::exp2(-rebuiltFit / 6.0f);
        expect(std::abs(rebuiltDispersion.inharmonicity
                            / idleDispersion.inharmonicity
                            / expectedRebuiltRatio
                        - 1.0f) < 0.01f,
               "live String Gauge automation did not refit idle-string "
               "dispersion");
        expectIdleTuning("live sympathetic-string gauge rebuild");

        // Picking this already-ringing steel changes ownership, not the
        // physical pickup. Preserve its populated spatial/Faraday memories so
        // the played attack does not begin behind an artificially empty path.
        const auto neckBeforeHandoff =
            TestAccess::pickupHistory(coupled, idleString, true);
        const auto bridgeBeforeHandoff =
            TestAccess::pickupHistory(coupled, idleString, false);
        const auto filtersBeforeHandoff = TestAccess::loopFilterState(
            coupled, idleString);
        const std::array<bool, 3> populated { true, true, true };
        coupled.noteOn(ElectryEngine::lowestPlayableNote, 0.9f);
        const auto filtersAfterHandoff = TestAccess::loopFilterState(
            coupled, idleString);
        bool filtersChokedTogether = true;
        for (std::size_t i = 0; i < filtersBeforeHandoff.size(); ++i)
        {
            filtersChokedTogether = filtersChokedTogether
                && filtersAfterHandoff[i] == 0.22f * filtersBeforeHandoff[i];
        }
        expect(neckBeforeHandoff == populated
                   && bridgeBeforeHandoff == populated
                   && ! TestAccess::snapshot(coupled, idleString)
                           .sympatheticReady
                   && TestAccess::pickupHistory(coupled, idleString, true)
                       == neckBeforeHandoff
                   && TestAccess::pickupHistory(coupled, idleString, false)
                       == bridgeBeforeHandoff,
               "the sympathetic-to-played handoff cleared pickup history");
        expect(filtersChokedTogether,
               "the sympathetic-to-played handoff left loop-filter memory "
               "outside the physical choke");
    }

}

void testHumbuckerTwoCoilNotch()
{
    constexpr double sampleRate = 48000.0;

    // These golden pickup spectra predate the body-path promotion and belong to
    // this fixed string/scale/gauge fixture, not whichever Build default a test
    // selects. Pin the fixture so an alternate Build cannot masquerade as a
    // pickup regression.
    const auto pickupReference = []
    {
        EngineParameters parameters;
        parameters.bodyWood = 0.0f;
        parameters.bodySize = 0.0f;
        parameters.bodyShape = 0.0f;
        parameters.construction = 0.0f;
        parameters.scaleLength = 0.85f;
        parameters.stringGauge = 1.0f;
        return parameters;
    };

    // Rendering protocol shared by every level measurement below.
    const auto renderChord = [&] (float pickupType, float velocity,
                                  double seconds)
    {
        auto engine = std::make_unique<ElectryEngine>();
        engine->prepare(sampleRate, 512);
        auto parameters = pickupReference();
        parameters.pickupType = pickupType;
        engine->setParameters(parameters);
        engine->reset();
        engine->noteOn(pickKeyswitch(PickStyle::Down), 1.0f);
        engine->noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
            engine->noteOn(note, velocity);
        StereoBuffer buffer(static_cast<int>(seconds * sampleRate));
        renderInto(*engine, buffer);
        return buffer;
    };
    const auto renderSingle = [&] (float pickupType, int midiNote,
                                   float velocity, double seconds)
    {
        auto engine = std::make_unique<ElectryEngine>();
        engine->prepare(sampleRate, 512);
        auto parameters = pickupReference();
        parameters.pickupType = pickupType;
        engine->setParameters(parameters);
        engine->reset();
        engine->noteOn(pickKeyswitch(PickStyle::Down), 1.0f);
        engine->noteOn(styleKeyswitch(PlayStyle::Sustain), 1.0f);
        engine->noteOn(midiNote, velocity);
        StereoBuffer buffer(static_cast<int>(seconds * sampleRate));
        renderInto(*engine, buffer);
        return buffer;
    };

    // 1. Where the notch is, and how deep, read off the two stages the voice
    // is actually running rather than off a rendered spectrum.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        auto parameters = pickupReference();
        parameters.pickupType = 0.0f;
        engine.setParameters(parameters);
        // reset() snaps the parameter smoother, so the voice is configured at
        // the control value asked for rather than part-way through a glide
        // from the default.
        engine.reset();
        engine.noteOn(40, 0.8f);
        engine.noteOn(45, 0.8f);
        StereoBuffer settle(512);
        renderInto(engine, settle);

        struct NotchCase { int midiNote; double lowHz; double highHz; };
        const std::array<NotchCase, 2> cases {{
            { 40, 2800.0, 3300.0 },   // string 2, E2: c/2d = 3043 Hz
            { 45, 3800.0, 4400.0 },   // string 3, A2: c/2d = 4062 Hz
        }};
        for (const auto& notch : cases)
        {
            const int stringIndex =
                TestAccess::stringForNote(engine, notch.midiNote);
            expect(stringIndex >= 0,
                   "note " + std::to_string(notch.midiNote)
                       + " did not take a string");
            if (stringIndex < 0)
                continue;
            expect(TestAccess::coilPairActive(engine, stringIndex),
                   "the humbucker is running on one coil");

            double deepest = 1.0e30;
            double deepestHz = 0.0;
            for (double f = 2000.0; f <= 8000.0; f *= 1.0004)
            {
                const double magnitude =
                    TestAccess::apertureChainMagnitude(
                        engine, stringIndex, f, true);
                if (magnitude < deepest)
                {
                    deepest = magnitude;
                    deepestHz = f;
                }
            }
            std::cout << "PROBE humbucker notch on note " << notch.midiNote
                      << ": " << deepestHz << " Hz\n";
            expect(deepestHz > notch.lowHz && deepestHz < notch.highHz,
                   "humbucker notch on note " + std::to_string(notch.midiNote)
                       + " is at " + std::to_string(deepestHz)
                       + " Hz, outside " + std::to_string(notch.lowHz) + ".."
                       + std::to_string(notch.highHz) + " Hz");

            // The local envelope is the aperture window on its own - the
            // smooth trend the coil pair notches into.
            const double envelope =
                TestAccess::apertureChainMagnitude(
                    engine, stringIndex, deepestHz, false);
            const double depthDb = 20.0 * std::log10(
                envelope / std::max(deepest, 1.0e-12));
            std::cout << "PROBE humbucker notch depth on note "
                      << notch.midiNote << ": " << depthDb << " dB\n";
            expect(depthDb >= 10.0,
                   "humbucker notch on note " + std::to_string(notch.midiNote)
                       + " is only " + std::to_string(depthDb)
                       + " dB below its local envelope");
        }
    }

    // 2. The single coil is one coil and is structurally untouched: the coil
    // pair is not in its path at all. This rendered snapshot includes the
    // deterministic string source upstream, so an intentional excitation
    // change refreshes the numbers while the structural assertion above still
    // owns the pickup topology.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        auto parameters = pickupReference();
        parameters.pickupType = 1.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.8f);
        StereoBuffer settle(512);
        renderInto(engine, settle);
        const int stringIndex = TestAccess::stringForNote(engine, 45);
        expect(stringIndex >= 0, "note 45 did not take a string");
        if (stringIndex >= 0)
            expect(! TestAccess::coilPairActive(engine, stringIndex),
                   "the single coil grew a second coil");

        // Partial magnitudes in dB, measured on the shipping engine with this
        // protocol. Partials past the 28th sit more than 90 dB below the
        // strongest one, where a one-ulp difference in the window length is
        // worth a decibel, so the comparison stops there.
        struct Partial { int index; double decibels; };
        const std::array<Partial, 16> reference {{
#if ELECTRY_ENERGY_ATTACK_PITCH
            // Whole-path candidate snapshot. A fixed bin is no longer a
            // stationary pickup response, but freezing the deterministic
            // traversing source still catches spectral regressions instead of
            // reducing this rendered check to mere finiteness.
            { 1, 16.6982 }, { 2, 22.5579 }, { 3, 24.7175 },
            { 4, 24.7792 }, { 6, 22.0725 }, { 8, 14.0735 },
            { 10, 1.46969 }, { 12, -2.11286 }, { 14, -10.3226 },
            { 16, -20.0485 }, { 18, -41.4025 }, { 20, -52.9458 },
            { 22, -65.8754 }, { 24, -68.4314 }, { 26, -66.4581 },
            { 28, -73.0455 },
#else
            { 1, 16.6907 }, { 2, 22.5653 }, { 3, 24.7606 },
            { 4, 24.8363 }, { 6, 22.3542 }, { 8, 14.6733 },
            { 10, 2.73903 }, { 12, 0.227274 }, { 14, -5.87426 },
            { 16, -12.3793 }, { 18, -29.7126 }, { 20, -42.6239 },
            { 22, -55.6061 }, { 24, -59.3919 }, { 26, -61.3935 },
            { 28, -67.7839 },
#endif
        }};
        const auto render = renderSingle(1.0f, 45, 0.70f, 1.0);
        const int start = static_cast<int>(0.05 * sampleRate);
        const int window = static_cast<int>(0.4 * sampleRate);
        const double f0 = midiHz(45);
        double worst = 0.0;
        for (const auto& partial : reference)
        {
            const double measured = 20.0 * std::log10(
                dftMagnitude(render.left, start, window, sampleRate,
                             f0 * partial.index) + 1.0e-30);
            worst = std::max(worst, std::abs(measured - partial.decibels));
            // This whole-source alarm complements the structural pickup check
            // above. Strong shipping partials reproduce tightly; the tail is
            // 30-80 dB down, so a wider tolerance avoids promoting numerical
            // residue into a pickup claim. The moving-pitch candidate has its
            // own explicitly looser snapshot branch.
#if ELECTRY_ENERGY_ATTACK_PITCH
            const double tolerance = partial.index <= 16 ? 0.75 : 2.5;
#else
#if ELECTRY_MEASURED_BODY_RESPONSE
            // The measured body deliberately changes the complete pickup
            // voltage. Keep this as a tight pickup-shape alarm without
            // freezing the old oversized structural path into the reference.
            const double tolerance = partial.index <= 12 ? 0.5 : 1.25;
#else
            const double tolerance = partial.index <= 12 ? 0.2 : 1.0;
#endif
#endif
            expect(std::abs(measured - partial.decibels) < tolerance,
                   "single-coil partial " + std::to_string(partial.index)
                       + " moved from " + std::to_string(partial.decibels)
                       + " dB to " + std::to_string(measured) + " dB");
        }
        std::cout << "PROBE single-coil partials moved at most " << worst
                  << " dB\n";
    }

    // Live automation has to reach the same structural endpoint as reset().
    // Otherwise an asymptotic residue keeps the second coil rendering forever.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        auto parameters = pickupReference();
        parameters.pickupType = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.8f);
        const int stringIndex = TestAccess::stringForNote(engine, 45);
        expect(stringIndex >= 0, "live pickup automation did not allocate A2");
        if (stringIndex >= 0)
            expect(TestAccess::coilPairActive(engine, stringIndex),
                   "the live pickup automation fixture did not start paired");

        parameters.pickupType = 1.0f;
        engine.setParameters(parameters);
        StereoBuffer travel(static_cast<int>(0.2 * sampleRate));
        renderInto(engine, travel, 17);
        if (stringIndex >= 0)
            expect(! TestAccess::coilPairActive(engine, stringIndex),
                   "live pickup automation never reached one coil");
    }

    // 3. The low-frequency recovery the pickup comb's weight was fitted for
    // must survive. The retained floor is 30.6608 dB; the current shipping
    // source measures 31.4205 dB.
    {
        const auto lowE = renderSingle(0.0f, 28, 0.80f, 1.5);
        const double band = bandEnergyDb(lowE.left,
                                         static_cast<int>(0.02 * sampleRate),
                                         static_cast<int>(1.0 * sampleRate),
                                         sampleRate, 60.0, 85.0);
        std::cout << "PROBE open low E 60-85 Hz band: " << band << " dB\n";
#if ELECTRY_MEASURED_BODY_RESPONSE
        expect(band > 30.6608 - 1.0,
#else
        expect(band > 30.6608 - 0.5,
#endif
               "the open low E lost its 60-85 Hz band ("
                   + std::to_string(band) + " dB against 30.6608 dB)");
    }

    // 4. Broadband balance. The humbucker has to stay the dark pickup of the
    // pair: on a full eight-string chord its 2-16 kHz to sub-500 Hz ratio may
    // not move more than 3 dB against the coherent-stroke engine's -71.949 dB,
    // and in every octave band from 4 to 16 kHz on a low, a middle and a plain
    // string it must stay at least 12 dB below the single coil (the current
    // shipping snapshot's narrowest gap is 14.15 dB).
    {
        const auto chord = renderChord(0.0f, 0.80f, 1.5);
        const int start = static_cast<int>(0.02 * sampleRate);
        const int window = static_cast<int>(1.0 * sampleRate);
        const double ratio =
            bandEnergyDb(chord.left, start, window, sampleRate, 2000.0, 16000.0)
            - bandEnergyDb(chord.left, start, window, sampleRate, 60.0, 500.0);
        std::cout << "PROBE humbucker chord 2-16k/sub-500 ratio: " << ratio
                  << " dB\n";
        expect(std::abs(ratio - (-71.949)) < 3.0,
               "the humbucker's broadband balance on a chord moved from "
                   "-71.949 dB to " + std::to_string(ratio) + " dB");

        struct Band { double lowHz; double highHz; };
        const std::array<Band, 2> bands {{ { 4000.0, 8000.0 },
                                           { 8000.0, 16000.0 } }};
        // Octave-band energy on the shipping engine, humbucker then single
        // coil, for notes 28, 40 and 64.
        struct Reference { int midiNote; double humbucker[2]; double single[2]; };
        const std::array<Reference, 3> shipping {{
#if ELECTRY_ENERGY_ATTACK_PITCH
            { 28, { -77.388, -108.256 }, { -50.493, -89.667 } },
            { 40, { -88.627, -115.293 }, { -69.527, -98.756 } },
            { 64, { -71.935, -111.974 }, { -51.853, -94.022 } },
#else
            { 28, { -83.789, -108.853 }, { -56.876, -94.705 } },
            { 40, { -75.579, -112.945 }, { -55.594, -98.413 } },
            { 64, { -73.047, -116.830 }, { -53.394, -99.677 } },
#endif
        }};
        for (const auto& reference : shipping)
        {
            const auto humbucker = renderSingle(0.0f, reference.midiNote,
                                                0.80f, 1.5);
            const auto single = renderSingle(1.0f, reference.midiNote,
                                             0.80f, 1.5);
            for (std::size_t b = 0; b < bands.size(); ++b)
            {
                const double dark = bandEnergyDb(
                    humbucker.left, start, window, sampleRate,
                    bands[b].lowHz, bands[b].highHz);
                const double bright = bandEnergyDb(
                    single.left, start, window, sampleRate,
                    bands[b].lowHz, bands[b].highHz);
                std::cout << "PROBE note " << reference.midiNote << " "
                          << bands[b].lowHz << "-" << bands[b].highHz
                          << " Hz: humbucker " << dark << " dB (was "
                          << reference.humbucker[b] << "), single coil "
                          << bright << " dB (was " << reference.single[b]
                          << ")\n";
                expect(dark < bright - 12.0,
                       "the humbucker is no longer the dark pickup in the "
                           + std::to_string(static_cast<int>(bands[b].lowHz))
                           + " Hz band on note "
                           + std::to_string(reference.midiNote) + " ("
                           + std::to_string(dark) + " dB against "
                           + std::to_string(bright) + " dB)");
                // A loose whole-source drift alarm, not the pickup
                // discriminator: the direct dark-versus-bright separation
                // above and the analytic topology/notch checks own that claim.
                expect(std::abs(dark - reference.humbucker[b]) < 12.0,
                       "humbucker octave-band energy on note "
                           + std::to_string(reference.midiNote) + " moved from "
                           + std::to_string(reference.humbucker[b]) + " dB to "
                           + std::to_string(dark) + " dB");
                // Hold the current single-coil whole-source snapshot tightly,
                // but only where the measurement means something. A band
                // sitting 90-odd dB down is numerical floor rather than signal,
                // and its decibel value does not reproduce between x86_64 and
                // arm64, which contract multiply-adds differently. Tightening
                // this further would only pin one architecture's noise.
                const double singleTolerance = reference.single[b] > -80.0
                    ? 1.25 : 2.5;
                expect(std::abs(bright - reference.single[b]) < singleTolerance,
                       "single-coil octave-band energy on note "
                           + std::to_string(reference.midiNote) + " moved from "
                           + std::to_string(reference.single[b]) + " dB to "
                           + std::to_string(bright) + " dB");
            }
        }
    }

    // 5. The coil pair adds a tap inside the pickup chain, so the ultrasonic
    // floor has to stay at least 150 dB below the spectral peak above 12 kHz
    // on a full chord at velocity 1.0.
    {
        const auto chord = renderChord(0.32f, 1.0f, 1.0);
        const int start = static_cast<int>(0.2 * sampleRate);
        constexpr int window = 32768;
        double peak = 0.0;
        double top = 0.0;
        for (double f = 40.0; f < 23500.0; f *= 1.0075)
        {
            const double magnitude = dftMagnitude(chord.left, start, window,
                                                  sampleRate, f);
            peak = std::max(peak, magnitude);
            if (f > 12000.0)
                top = std::max(top, magnitude);
        }
        const double floorDb = 20.0 * std::log10(peak / std::max(top, 1.0e-30));
        std::cout << "PROBE ultrasonic floor above 12 kHz: " << floorDb
                  << " dB below the spectral peak\n";
        expect(floorDb > 150.0,
               "the ultrasonic floor rose to " + std::to_string(floorDb)
                   + " dB below the spectral peak");
    }
}

// The instrument has to sound the same at every host rate, and the decay
// envelope is where that is hardest. The polarisation seam is encountered once
// per loop, but its historical coefficient was derived from the compensated
// digital delay. That made the same note decay differently between rate
// families and produced a twofold coefficient jump just above a 96 kHz host.
void testDecayIsSampleRateInvariant()
{
    struct Window { double start, end; double toleranceDb; };
    // The late window is allowed more slack: it is 20-30 dB down, and what is
    // left there is the one-pole loop filter's own shape, which is fitted at
    // two frequencies and can only interpolate between them in normalised
    // radians.
    const std::array<Window, 3> windows {{
        { 0.10, 0.50, 1.0 }, { 0.50, 1.50, 1.0 }, { 1.50, 3.00, 2.0 } }};

    for (const int note : { 28, 45, 64, 86 })
    {
        std::array<std::array<double, 3>, 6> levels {};
        int rateIndex = 0;
        for (const double rate :
             { 44100.0, 48000.0, 88200.0, 96000.0, 96001.0, 192000.0 })
        {
            ElectryEngine engine;
            engine.prepare(rate, 512);
            EngineParameters parameters;
            parameters.sympatheticAmount = 0.0f;
            parameters.artifactAmount = 0.0f;
            parameters.pickNoise = 0.0f;
            parameters.fingerNoise = 0.0f;
            parameters.releaseNoise = 0.0f;
            engine.setParameters(parameters);

            const auto buffer = renderNote(engine, rate, note, 0.9f,
                                           PlayStyle::Sustain, 3.0);
            expect(allFinite(buffer),
                   "the rate-invariance render was not finite at "
                       + std::to_string(rate) + " Hz");
            const double attack = rmsInRange(buffer.left, 0,
                                             static_cast<int>(0.1 * rate));
            expect(attack > 1.0e-5,
                   "the rate-invariance render produced no attack at "
                       + std::to_string(rate) + " Hz");
            for (std::size_t w = 0; w < windows.size(); ++w)
            {
                const double level = rmsInRange(
                    buffer.left, static_cast<int>(windows[w].start * rate),
                    static_cast<int>(windows[w].end * rate));
                levels[static_cast<std::size_t>(rateIndex)][w] =
                    20.0 * std::log10(std::max(level, 1.0e-12) / attack);
            }
            ++rateIndex;
        }

        std::array<double, windows.size()> spreads {};
        for (std::size_t w = 0; w < windows.size(); ++w)
        {
            double lowest = 1.0e30, highest = -1.0e30;
            for (const auto& perRate : levels)
            {
                lowest = std::min(lowest, perRate[w]);
                highest = std::max(highest, perRate[w]);
            }
            spreads[w] = highest - lowest;
            expect(spreads[w] < windows[w].toleranceDb,
                   "note " + std::to_string(note) + "'s decay between "
                       + std::to_string(windows[w].start) + " s and "
                       + std::to_string(windows[w].end)
                       + " s depends on the host sample rate ("
                       + std::to_string(spreads[w]) + " dB spread)");
        }
        if (note == 86)
            std::cout << "PROBE high-E host-rate decay spreads: "
                      << spreads[0] << "/" << spreads[1] << "/"
                      << spreads[2] << " dB\n";
    }
}

// A returned wave encounters the two-polarisation seam once per loop. Its
// coefficient must therefore follow the note, not the internal clock. The
// engine changes from 2x to native immediately above a 96 kHz host, so that
// boundary is the most sensitive construction check.
void testPolarisationCouplingIsRateInvariant()
{
    constexpr std::array<double, 7> rates {
        44100.0, 48000.0, 88200.0, 96000.0, 96001.0, 192000.0, 384000.0
    };
    EngineParameters parameters;
    parameters.sympatheticAmount = 0.0f;
    parameters.artifactAmount = 0.0f;

    for (const int note : { 28, 79, 86 })
    {
        std::array<double, rates.size()> couplings {};
        for (std::size_t index = 0; index < rates.size(); ++index)
        {
            ElectryEngine engine;
            engine.prepare(rates[index], 512);
            engine.setParameters(parameters);
            engine.reset();
            engine.noteOn(note, 0.9f);
            const int stringIndex = TestAccess::stringForNote(engine, note);
            expect(stringIndex >= 0,
                   "the coupling probe note was not allocated at "
                       + std::to_string(rates[index]) + " Hz");
            if (stringIndex < 0)
                return;

            const auto snapshot = TestAccess::snapshot(engine, stringIndex);
            expect(snapshot.polarisationCoupling > 0.0f,
                   "the polarisation coupling was switched off");
            expect(snapshot.polarisationCoupling < 0.25f,
                   "the polarisation-coupling upper clamp engaged inside the "
                   "playable range");
            expect(snapshot.lastCompensatedPeriod > 0.0f,
                   "the coupling probe has no solved sounding period");
            if (! (snapshot.lastCompensatedPeriod > 0.0f))
                return;
            couplings[index] = snapshot.polarisationCoupling;

            const double liveFrequency =
                TestAccess::internalSampleRate(engine)
                / static_cast<double>(snapshot.lastCompensatedPeriod);
            const double referenceProduct =
                static_cast<double>(snapshot.polarisationCoupling)
                * 96000.0 / liveFrequency;
            expect(std::abs(referenceProduct - 0.04) < 1.0e-6,
                   "the 96 kHz-reference polarisation pitch law moved ("
                       + std::to_string(referenceProduct) + ")");
        }

        const auto [lowest, highest] =
            std::minmax_element(couplings.begin(), couplings.end());
        const double relativeSpread = (*highest - *lowest) / *highest;
        expect(relativeSpread < 1.0e-4,
               "note " + std::to_string(note)
                   + "'s polarisation coupling follows the host clock ("
                   + std::to_string(100.0 * relativeSpread) + "% spread)");

        const double boundarySpread =
            std::abs(couplings[3] - couplings[4])
            / std::max(couplings[3], couplings[4]);
        expect(boundarySpread < 1.0e-4,
               "note " + std::to_string(note)
                   + "'s polarisation coupling jumps above the 96 kHz "
                     "oversampling boundary ("
                   + std::to_string(100.0 * boundarySpread) + "%)");
    }

    // Pin live sounding pitch rather than only static MIDI assignment. A
    // future shortcut from f0 back to baseFrequency would pass every check
    // above but make the seam ignore wheel, MPE, legato and vibrato travel.
    ElectryEngine bent;
    bent.prepare(48000.0, 512);
    bent.setParameters(parameters);
    bent.setPitchBend(0.5f); // +1 semitone at the legacy +/-2-semitone range.
    bent.reset();
    constexpr int bentNote = 79;
    bent.noteOn(bentNote, 0.9f);
    const int bentString = TestAccess::stringForNote(bent, bentNote);
    expect(bentString >= 0, "the bent coupling probe note was not allocated");
    if (bentString < 0)
        return;
    const auto bentSnapshot = TestAccess::snapshot(bent, bentString);
    expect(bentSnapshot.lastCompensatedPeriod > 0.0f,
           "the bent coupling probe has no solved sounding period");
    if (! (bentSnapshot.lastCompensatedPeriod > 0.0f))
        return;
    const double bentFrequency = TestAccess::internalSampleRate(bent)
        / static_cast<double>(bentSnapshot.lastCompensatedPeriod);
    const double bentReferenceProduct =
        static_cast<double>(bentSnapshot.polarisationCoupling)
        * 96000.0 / bentFrequency;
    expect(std::abs(bentReferenceProduct - 0.04) < 1.0e-6,
           "the polarisation coupling ignored live sounding pitch ("
               + std::to_string(bentReferenceProduct) + ")");

    // The construction check is backed by the rendered outcome: the high note
    // must keep its established sustain on both sides of the rate-family seam.
    for (const double rate : { 44100.0, 48000.0, 96000.0, 96001.0, 192000.0 })
    {
        ElectryEngine rateEngine;
        rateEngine.prepare(rate, 512);
        rateEngine.setParameters(parameters);
        rateEngine.reset();
        const auto buffer = renderNote(rateEngine, rate, 86, 0.9f,
                                       PlayStyle::Sustain, 3.0);
        const double attack = rmsInRange(buffer.left, 0,
                                         static_cast<int>(0.1 * rate));
        const double sustain = rmsInRange(buffer.left,
                                          static_cast<int>(1.0 * rate),
                                          static_cast<int>(2.0 * rate));
        const double relative =
            20.0 * std::log10(std::max(sustain, 1.0e-12) / attack);
        expect(relative > -30.0,
               "the 22nd-fret high E no longer sustains at "
                   + std::to_string(static_cast<int>(rate)) + " Hz ("
                   + std::to_string(relative) + " dB under its attack at 1-2 s)");
    }
}

// A string nobody is fingering is the same piece of steel as one that is being
// played, so its loop has to lose its top end at the same rate. The fixed
// one-pole this replaced was a mild lowpass whatever the string, which left the
// wound strings' coupled ring carrying kilohertz content for seconds - a bright
// metallic reverb rather than strings.
void testCoupledStringLosesItsTopEndLikeAPlayedString()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.sympatheticAmount = 0.6f;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);

    const auto buffer = renderNote(engine, sampleRate, 45, 0.95f,
                                   PlayStyle::Sustain, 3.0, 0.5);
    expect(allFinite(buffer), "the coupled-ring render was not finite");

    const int start = static_cast<int>(1.0 * sampleRate);
    const int length = static_cast<int>(1.5 * sampleRate);
    const double low = bandEnergyDb(buffer.left, start, length, sampleRate,
                                    60.0, 700.0);
    const double high = bandEnergyDb(buffer.left, start, length, sampleRate,
                                     1500.0, 6000.0);
    // The ring itself must still be there.
    expect(rmsInRange(buffer.left, start, start + length) > 1.0e-5,
           "the coupled strings stopped ringing altogether");
    // ...and it must be a string, not a cymbal. The fixed coefficient scored
    // -37.8 dB here; a played wound string's own decay law scores -87.5.
    expect(high - low < -60.0,
           "the coupled ring still carries a played string's worth of "
           "kilohertz content (" + std::to_string(high - low) + " dB)");

    // The wound low strings must be damped harder than the plain high ones,
    // which is the whole point of solving the loss from the string rather than
    // fixing it: a plain .009 keeps far more of its top end than a wound .080.
    engine.reset();
    engine.noteOn(45, 0.9f);
    StereoBuffer settle(static_cast<int>(0.4 * sampleRate));
    renderInto(engine, settle);
    const auto lowString = TestAccess::snapshot(engine, 0);
    const auto highString = TestAccess::snapshot(engine, ElectryEngine::stringCount - 1);
    expect(lowString.sympatheticReady && highString.sympatheticReady,
           "the coupled strings were not configured");
    expect(lowString.loopDampingCoefficient
               > highString.loopDampingCoefficient + 0.2f,
           "the wound coupled low E is not damped harder than the plain high E ("
               + std::to_string(lowString.loopDampingCoefficient) + " vs "
               + std::to_string(highString.loopDampingCoefficient) + ")");
}

// The other half of solving the coupled loop from decay targets: the
// fundamental's target is the one that must never be given up.
//
// Two decay targets are two constraints on a first-order filter and a scalar,
// and they do not always both fit - the one-pole's own loss at the fundamental
// has to be bought back by the loop gain, and that gain cannot exceed one.
// Solving the tilt and then clamping the gain discards the fundamental's target
// silently: the coupled open low E realised a 0.099 s T60 against its 8.97 s
// target at String Age 1.0, and every string collapsed to between 8 and 53 ms
// under the palm mute. The whole sympathetic bank disappeared wherever either
// control was up. The engine now backs the high-frequency target off instead,
// which costs only the top of the tilt.
//
// The loop's realised decay is read back from what actually runs - the solved
// gain, one-pole coefficient and live per-sample hand gain - rather than from
// the solver's own arithmetic. Leaving the last term out hid Palm Pressure
// being applied once in the loop target and a second time by the global hand.
void testCoupledStringKeepsItsFundamentalDecayTarget()
{
    // Realised round-trip T60 of a coupled loop at its own fundamental.
    const auto realisedT60 = [] (const ElectryEngine& engine, int stringIndex)
    {
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        const double rate = TestAccess::internalSampleRate(engine);
        const double f0 = TestAccess::sympatheticFrequency(engine, stringIndex);
        const double omega = 2.0 * 3.14159265358979323846 * f0 / rate;
        const double a = snapshot.loopDampingCoefficient;
        const double magnitude = (1.0 - a)
            / std::sqrt(std::max(1.0 + a * a - 2.0 * a * std::cos(omega),
                                 1.0e-30));
        const double period = rate / f0;
        const double handGain = TestAccess::sympatheticHandGain(engine);
        const double perRoundTrip = snapshot.loopGain * magnitude
                                  * std::pow(handGain, period);
        if (perRoundTrip <= 0.0 || perRoundTrip >= 1.0)
            return 1.0e9;
        return -3.0 * period / (rate * std::log10(perRoundTrip));
    };

    const auto configured = [&] (double hostRate, float stringAge,
                                 float palmMute, ElectryEngine& engine)
    {
        engine.prepare(hostRate, 512);
        EngineParameters parameters;
        parameters.stringAge = stringAge;
        parameters.palmMute = palmMute;
        parameters.sympatheticAmount = 0.6f;
        engine.setParameters(parameters);
        engine.reset();
        // One note wakes the other seven strings as coupled loops.
        engine.noteOn(45, 0.9f);
        StereoBuffer settle(static_cast<int>(0.4 * hostRate));
        renderInto(engine, settle);
    };

    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        const std::string at = " at " + std::to_string(static_cast<int>(rate))
                             + " Hz";

        // Worn strings. The wound coupled strings' targets are still seconds
        // long here - 8.97, 8.52 and 8.07 s on the bottom three - and the
        // clamp realised 0.099, 0.79 and 1.48.
        {
            ElectryEngine engine;
            configured(rate, 1.0f, 0.0f, engine);
            for (int s = 0; s < ElectryEngine::stringCount; ++s)
            {
                if (! TestAccess::snapshot(engine, s).sympatheticReady)
                    continue;
                const double t60 = realisedT60(engine, s);
                expect(t60 > 3.0 && t60 < 12.0,
                       "coupled string " + std::to_string(s)
                           + " does not hold its multi-second decay target at "
                             "String Age 1.0" + at + " (" + std::to_string(t60)
                           + " s)");
            }
        }

        // Under the palm mute the coupled fundamental target is the engine's
        // own constant: `exp(lerp(log(t60), log(0.080), blend))` is exactly
        // 0.080 s at full pressure, whatever the string and whatever the host
        // rate. Six of the eight strings realised 8 to 53 ms instead.
        {
            ElectryEngine engine;
            configured(rate, 0.30f, 1.0f, engine);
            for (int s = 0; s < ElectryEngine::stringCount; ++s)
            {
                if (! TestAccess::snapshot(engine, s).sympatheticReady)
                    continue;
                const double t60 = realisedT60(engine, s);
                expect(std::abs(t60 - 0.080) < 0.004,
                       "coupled string " + std::to_string(s)
                           + " does not realise the 0.080 s full-mute decay "
                             "target" + at + " (" + std::to_string(t60) + " s)");
            }
        }

        // Half pressure, where the targets run from 1.11 to 1.28 s and the
        // clamp realised 16 to 99 ms.
        {
            ElectryEngine engine;
            configured(rate, 0.30f, 0.5f, engine);
            for (int s = 0; s < ElectryEngine::stringCount; ++s)
            {
                if (! TestAccess::snapshot(engine, s).sympatheticReady)
                    continue;
                const double t60 = realisedT60(engine, s);
                expect(t60 > 0.6 && t60 < 1.8,
                       "coupled string " + std::to_string(s)
                           + " does not hold its half-mute decay target" + at
                           + " (" + std::to_string(t60) + " s)");
            }
        }
    }

    // The same target on every host, not merely a plausible one on each: the
    // clamp made the coupled low E's realised decay 0.094 s at 44.1 kHz,
    // 0.099 at 48 and 0.159 at 96, in the release whose headline is rate
    // invariance.
    double lowE[3] = { 0.0, 0.0, 0.0 };
    int index = 0;
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        ElectryEngine engine;
        configured(rate, 1.0f, 0.0f, engine);
        lowE[index++] = realisedT60(engine, 0);
    }
    const double spread = (std::max({ lowE[0], lowE[1], lowE[2] })
                           - std::min({ lowE[0], lowE[1], lowE[2] }))
                        / std::max(lowE[0], 1.0e-9);
    expect(spread < 0.02,
           "the coupled low E's realised decay depends on the host rate ("
               + std::to_string(lowE[0]) + " / " + std::to_string(lowE[1])
               + " / " + std::to_string(lowE[2]) + " s)");

    // Finally, that the bank is still there and still finite at String Age 1.0,
    // where the clamp used to leave it. This last check is deliberately a weak
    // one and it is worth saying why rather than dressing it up: with the
    // driving string itself still ringing, the rendered level in any window is
    // set mostly by what the bus is feeding the coupled loops and only weakly
    // by the loops' own decay, so it separates the two revisions by a few dB in
    // the late windows and not at all in the early ones. The assertions that
    // pin this regression are the analytic ones above, read off the gain and
    // coefficient the loops actually run.
    constexpr double sampleRate = 48000.0;
    const auto ringEnergy = [&] (float sympathetic)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.stringAge = 1.0f;
        parameters.sympatheticAmount = sympathetic;
        parameters.artifactAmount = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        const auto buffer = renderNote(engine, sampleRate, 45, 0.95f,
                                       PlayStyle::Sustain, 3.0, 0.5);
        expect(allFinite(buffer), "the worn-string coupled render was not finite");
        return rmsInRange(buffer.left, static_cast<int>(1.5 * sampleRate),
                          static_cast<int>(2.5 * sampleRate));
    };
    expect(ringEnergy(0.6f) > 2.0 * ringEnergy(0.0f),
           "the coupled bank contributes nothing at String Age 1.0");
}

// ---------------------------------------------------------------------------
// The strings share a bridge: the strings that are being *played* read the
// coupling bus too, minus their own contribution to it.
// ---------------------------------------------------------------------------
//
// Until this shipped the bus was written only by played voices and read only
// by idle ones, so a voicing that fingers all eight strings had no coupling
// path at all: the same chord rendered at Resonance 0.20 and at 0 was
// bit-identical. That is the case this test leads with, and it is the one no
// implementation can pass without closing the loop.
//
// Everything here pins `pickNoise`, `fingerNoise` and `releaseNoise` at zero.
// Not for the reason the plan first gave - the per-note noise seed - but for a
// larger one measured on this engine: the per-attack stroke draw seeded from
// the note counter moves a two-note render 30 to 65 dB away from the sum of
// its two single-note renders, which swamps any coupling term. Every
// comparison below is therefore the *same* voicing at two coupling settings,
// where the seeds are identical by construction.
void testFingeredStringsShareTheBridge()
{
    constexpr double sampleRate = 48000.0;
    const std::vector<int> allEight { 28, 35, 40, 45, 50, 55, 59, 64 };

    const auto renderChord = [&] (const std::vector<int>& notes, float velocity,
                                  float sympathetic, float resonanceDepth,
                                  double seconds)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.sympatheticAmount = sympathetic;
        parameters.resonanceDepth = resonanceDepth;
        parameters.artifactAmount = 0.0f;
        parameters.strumSpreadSeconds = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        for (const int note : notes)
            engine.noteOn(note, velocity);
        StereoBuffer buffer(static_cast<int>(seconds * sampleRate));
        renderInto(engine, buffer);
        return buffer;
    };

    // Relative L2 of one render against another, in decibels, over a window.
    const auto relativeDb = [] (const std::vector<float>& a,
                                const std::vector<float>& b,
                                int start, int end)
    {
        double difference = 0.0;
        double reference = 0.0;
        for (int i = start; i < end; ++i)
        {
            const double delta = static_cast<double>(a[static_cast<std::size_t>(i)])
                               - static_cast<double>(b[static_cast<std::size_t>(i)]);
            difference += delta * delta;
            reference += static_cast<double>(b[static_cast<std::size_t>(i)])
                       * static_cast<double>(b[static_cast<std::size_t>(i)]);
        }
        if (reference <= 0.0)
            return 0.0;
        return 10.0 * std::log10(std::max(difference, 1.0e-300) / reference);
    };

    const int firstWindow = static_cast<int>(1.5 * sampleRate);

    // 1. The voicing that leaves nothing open now hears itself. Both figures
    //    are exactly zero difference - minus infinity - without the step.
    const auto chordOff = renderChord(allEight, 0.90f, 0.0f, 0.0f, 2.0);
    expect(allFinite(chordOff), "the uncoupled eight-string chord is not finite");
    const auto chordDefault = renderChord(allEight, 0.90f, 0.20f, 0.0f, 2.0);
    const auto chordFull = renderChord(allEight, 0.90f, 1.00f, 0.0f, 2.0);
    expect(allFinite(chordDefault) && allFinite(chordFull),
           "the coupled eight-string chord is not finite");

    const double defaultDb = relativeDb(chordDefault.left, chordOff.left,
                                        0, firstWindow);
    const double fullDb = relativeDb(chordFull.left, chordOff.left,
                                     0, firstWindow);
    expect(defaultDb >= -66.0,
           "an all-eight-fingered chord at Resonance 0.20 does not differ from "
           "the same chord at 0 (" + std::to_string(defaultDb) + " dB)");
    expect(fullDb >= -56.0,
           "an all-eight-fingered chord at full Resonance does not differ from "
           "the same chord at 0 (" + std::to_string(fullDb) + " dB)");

    // 2. The control scales it, rather than switching a fixed amount on.
    const auto chordLow = renderChord(allEight, 0.90f, 0.05f, 0.0f, 2.0);
    const auto chordHalf = renderChord(allEight, 0.90f, 0.50f, 0.0f, 2.0);
    const double lowDb = relativeDb(chordLow.left, chordOff.left, 0, firstWindow);
    const double halfDb = relativeDb(chordHalf.left, chordOff.left, 0, firstWindow);
    expect(lowDb < defaultDb && defaultDb < halfDb,
           "the coupling does not grow with the Resonance control ("
               + std::to_string(lowDb) + " -> " + std::to_string(defaultDb)
               + " -> " + std::to_string(halfDb) + " dB)");
    std::cout << "PROBE all-eight-fingered chord against the same chord at"
                 " Resonance 0: " << lowDb << " dB at 0.05, " << defaultDb
              << " dB at 0.20, " << halfDb << " dB at 0.50, " << fullDb
              << " dB at 1.0 (exactly zero difference before the step)\n";

    // 3. Thirty seconds of eight strings at full velocity and maximum
    //    Resonance: bounded, and falling. Strict monotonicity over successive
    //    1 s windows is *not* asserted - the uncoupled engine already breaks it
    //    three times over this render, by up to 0.43 dB, so the bar is that the
    //    coupling does not add regeneration on top of that. Polarisation and
    //    body-mode beating move energy through these coarse windows even while
    //    the render falls end to end, so two decibels separates beating from
    //    actual growth.
    const auto longChord = renderChord(allEight, 1.0f, 1.0f, 1.0f, 30.0);
    expect(allFinite(longChord), "thirty seconds of maximum coupling is not finite");
    expect(peakAbs(longChord.left) < 3.05f && peakAbs(longChord.right) < 3.05f,
           "maximum played-string coupling escaped the output guard");
    double previousWindow = 0.0;
    double worstRise = 0.0;
    for (int second = 0; second < 30; ++second)
    {
        const double windowRms = rmsInRange(
            longChord.left, static_cast<int>(second * sampleRate),
            static_cast<int>((second + 1) * sampleRate));
        if (second > 0 && windowRms > previousWindow)
            worstRise = std::max(worstRise,
                                 20.0 * std::log10(windowRms / previousWindow));
        previousWindow = windowRms;
    }
    expect(worstRise <= 2.0,
           "a 1 s window of the maximum-coupling chord grew by "
               + std::to_string(worstRise) + " dB");
    const double firstSecond = rmsInRange(longChord.left, 0,
                                          static_cast<int>(sampleRate));
    const double lastSecond = rmsInRange(longChord.left,
                                         static_cast<int>(29.0 * sampleRate),
                                         static_cast<int>(30.0 * sampleRate));
    const double thirtySecondFall =
        20.0 * std::log10(firstSecond / std::max(lastSecond, 1.0e-30));
    expect(thirtySecondFall >= 70.0,
           "the maximum-coupling chord did not decay over thirty seconds");
    std::cout << "PROBE thirty seconds of maximum coupling: worst 1 s rise "
              << worstRise << " dB, total fall " << thirtySecondFall << " dB\n";

    // 4. The spectral-radius bound, read at the seam. The coupling matrix has
    //    a zero diagonal and off-diagonal entries g/(1 - G_i), so the row-sum
    //    norm is (N - 1) g max_i 1/(1 - G_i); it is held at or below 0.25 by
    //    construction, at every setting, and is checked here against the loop
    //    gains the voices are actually running.
    double worstRowSum = 0.0;
    for (const float sympathetic : { 0.05f, 0.20f, 0.50f, 1.0f })
        for (const float resonance : { 0.0f, 1.0f })
            for (const float mute : { 0.0f, 0.5f })
            {
                ElectryEngine engine;
                engine.prepare(sampleRate, 512);
                EngineParameters parameters;
                parameters.sympatheticAmount = sympathetic;
                parameters.resonanceDepth = resonance;
                parameters.palmMute = mute;
                parameters.artifactAmount = 0.0f;
                parameters.strumSpreadSeconds = 0.0f;
                engine.setParameters(parameters);
                engine.reset();
                for (const int note : allEight)
                    engine.noteOn(note, 1.0f);
                StereoBuffer settle(static_cast<int>(0.5 * sampleRate));
                renderInto(engine, settle);

                int active = 0;
                double worstAmplification = 1.0;
                for (int stringIndex = 0;
                     stringIndex < ElectryEngine::stringCount; ++stringIndex)
                {
                    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
                    if (! snapshot.active)
                        continue;
                    ++active;
                    worstAmplification = std::max(
                        worstAmplification,
                        1.0 / std::max(1.0 - snapshot.loopGain, 1.0e-6));
                }
                const double gain = TestAccess::bridgeCouplingGain(engine);
                const double rowSum = static_cast<double>(active - 1) * gain
                                    * worstAmplification;
#if ELECTRY_ENERGY_ATTACK_PITCH
                // The engine solves and stores this contract in float. A live
                // attack-tension damping refit can land exactly on 0.25f;
                // recomputing the same float seam here in double differs by at
                // most one float ULP. Keep the engine's own strict bound below.
                const double recomputedBound = static_cast<double>(
                    std::nextafter(0.25f, 1.0f));
#else
                constexpr double recomputedBound = 0.25;
#endif
                expect(rowSum <= recomputedBound,
                       "the played-string coupling exceeded its row-sum bound ("
                           + std::to_string(rowSum) + " at Resonance "
                           + std::to_string(sympathetic) + ")");
                expect(TestAccess::bridgeCouplingRowSum(engine) <= 0.25f,
                       "the engine's own row-sum norm exceeded 0.25");
                expect(gain > 0.0 || mute >= 1.0f,
                       "the played strings read no coupling at all");
                worstRowSum = std::max(worstRowSum, rowSum);
            }
    std::cout << "PROBE worst played-string coupling row-sum norm over the "
                 "sweep: " << worstRowSum << " against a bound of 0.25\n";

    // 5. The self-term. With one voice sounding, the bus *is* that voice's own
    //    contribution, so `bus - own` is exactly zero and the injection must be
    //    exactly zero however far the control is pushed. The check is on the
    //    voice's own output-energy follower rather than on the summed pickup
    //    output, because that sum also carries seven idle strings ringing
    //    sympathetically. A voice that drove itself through the bus would
    //    change this follower - and every decay time, T60 and timbre calibration
    //    in the instrument sits downstream of that.
    const auto singleNoteLoopEnergy = [&] (float sympathetic)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.sympatheticAmount = sympathetic;
        parameters.artifactAmount = 0.0f;
        parameters.strumSpreadSeconds = 0.0f;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(40, 0.90f);
        StereoBuffer buffer(static_cast<int>(2.0 * sampleRate));
        renderInto(engine, buffer);
        const int stringIndex = TestAccess::stringForNote(engine, 40);
        expect(stringIndex >= 0, "the single note did not sound");
        expect(TestAccess::bridgeCouplingGain(engine) == 0.0f,
               "a lone voice was given a non-zero share of the bridge bus");
        return TestAccess::voiceOutputEnergy(engine, stringIndex);
    };
    const float loopEnergyOff = singleNoteLoopEnergy(0.0f);
    expect(loopEnergyOff > 0.0f, "the single note left no loop energy to read");
    expect(singleNoteLoopEnergy(0.20f) == loopEnergyOff,
           "a single note's own loop moved with the Resonance control at 0.20");
    expect(singleNoteLoopEnergy(1.0f) == loopEnergyOff,
           "a single note's own loop moved with the Resonance control at 1.0");

    // 6. Off is off, and the top end stays clean. The new path is a broadband
    //    injection into eight high-Q loops, which is exactly the shape that
    //    folds energy back above Nyquist if it is unbounded.
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.sympatheticAmount = 0.0f;
        parameters.artifactAmount = 0.0f;
        parameters.strumSpreadSeconds = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        for (const int note : allEight)
            engine.noteOn(note, 1.0f);
        StereoBuffer settle(static_cast<int>(0.5 * sampleRate));
        renderInto(engine, settle);
        expect(TestAccess::bridgeCouplingGain(engine) == 0.0f
                   && TestAccess::bridgeCouplingRowSum(engine) == 0.0f,
               "the played-string coupling is not exactly zero at Resonance 0");
    }

    const auto aliasing = renderChord(allEight, 1.0f, 1.0f, 1.0f, 2.0);
    const int aliasStart = static_cast<int>(0.2 * sampleRate);
    const int aliasLength = static_cast<int>(1.0 * sampleRate);
    double spectralPeak = 0.0;
    for (double frequency = 60.0; frequency < 8000.0; frequency *= 1.01)
        spectralPeak = std::max(spectralPeak,
                                dftMagnitude(aliasing.left, aliasStart,
                                             aliasLength, sampleRate, frequency));
    double aliasPeak = 0.0;
    for (double frequency = 12000.0; frequency < 23000.0; frequency *= 1.005)
        aliasPeak = std::max(aliasPeak,
                             dftMagnitude(aliasing.left, aliasStart,
                                          aliasLength, sampleRate, frequency));
    const double aliasFloorDb =
        20.0 * std::log10(spectralPeak / std::max(aliasPeak, 1.0e-30));
    // A moving delay creates real modulation sidebands, so this scan is no
    // longer a static alias measurement. Keep a stringent 120 dB guard on the
    // entire ultrasonic result; the existing finger vibrato is around 90 dB
    // on the same metric, while an unstable or folded network is far louder.
    expect(aliasFloorDb >= 120.0,
           "played-string coupling raised the ultrasonic floor to "
               + std::to_string(aliasFloorDb) + " dB below the peak");
    std::cout << "PROBE ultrasonic floor with played-string coupling at maximum: "
              << aliasFloorDb << " dB below the spectral peak\n";
}

void testIdleFreezeAndDenormalSafety()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.sympatheticAmount = 1.0f;
    parameters.artifactAmount = 1.0f;
    parameters.bodyResonance = 1.0f;
    parameters.stringAge = 0.0f;
    engine.setParameters(parameters);
    engine.reset();

    // A freshly prepared engine is already frozen: no body mode, coil, DC
    // blocker or decimator runs until a string is asked to vibrate.
    StereoBuffer beforeAnyNote(static_cast<int>(0.5 * sampleRate));
    renderInto(engine, beforeAnyNote);
    expect(peakAbs(beforeAnyNote.left) == 0.0f
               && peakAbs(beforeAnyNote.right) == 0.0f,
           "an untouched engine did not render exact digital silence");

    for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
        engine.noteOn(note, 1.0f);
    StereoBuffer strum(static_cast<int>(1.0 * sampleRate));
    renderInto(engine, strum);
    expect(peakAbs(strum.left) > 0.01f, "the strum did not sound");
    engine.allNotesOff();

    // Every sample of the ring-out must be finite, and none of it may be a
    // subnormal float: the hosts that matter run with flush-to-zero, but the
    // engine must not depend on that to stay cheap.
    int subnormals = 0;
    int silentBlocks = 0;
    bool sawSilence = false;
    constexpr float smallestNormal = 1.1754943508e-38f;
    for (int block = 0; block < 40 && ! sawSilence; ++block)
    {
        StereoBuffer tail(static_cast<int>(0.5 * sampleRate));
        renderInto(engine, tail);
        expect(allFinite(tail), "the ring-out produced non-finite audio");
        float peak = 0.0f;
        for (const float sample : tail.left)
        {
            const float magnitude = std::abs(sample);
            peak = std::max(peak, magnitude);
            if (magnitude > 0.0f && magnitude < smallestNormal)
                ++subnormals;
        }
        if (peak == 0.0f)
        {
            sawSilence = true;
            silentBlocks = block;
        }
    }
    expect(subnormals == 0,
           "the ring-out generated " + std::to_string(subnormals)
               + " subnormal output samples");
    expect(sawSilence,
           "the engine never reached exact silence after the last note");
    // Freezing must happen promptly enough to matter, but never so early that
    // an audible tail is truncated.
    expect(silentBlocks >= 1 && silentBlocks <= 24,
           "the idle freeze happened at an implausible time (block "
               + std::to_string(silentBlocks) + ")");

    // A frozen engine wakes cleanly.
    engine.noteOn(45, 0.9f);
    StereoBuffer wake(static_cast<int>(0.25 * sampleRate));
    renderInto(engine, wake);
    expect(allFinite(wake) && peakAbs(wake.left) > 1.0e-3f,
           "a frozen engine did not wake for a new note");
}

void testCpuGuardrail()
{
    constexpr double sampleRate = 96000.0;
    constexpr int totalSamples = static_cast<int>(2.0 * 96000.0);
    // Time a representative Palm attack/body window separately from the full
    // two-second render above.
    constexpr int palmSamples = static_cast<int>(0.110 * sampleRate);

    // All eight physical strings ringing in Drop-E tuning. With every string
    // played there is no coupled string left to render, so this isolates the
    // active-voice path from idle sympathetic-string work.
    const auto strike = [&] (ElectryEngine& engine, PickupSelector selector,
                             electry::OutputMode mode,
                             PlayStyle style = PlayStyle::Sustain)
    {
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupSelector = selector;
        parameters.outputMode = mode;
        parameters.artifactAmount = 1.0f;
        parameters.bodyResonance = 1.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(styleKeyswitch(style), 1.0f);

        for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
            engine.noteOn(note, 0.9f);
    };

    StereoBuffer buffer(totalSamples);
    StereoBuffer palmBuffer(palmSamples);
    const auto timeRender = [&] (ElectryEngine& engine, StereoBuffer& audio)
    {
        const auto begin = std::chrono::steady_clock::now();
        renderInto(engine, audio);
        const auto end = std::chrono::steady_clock::now();
        expect(allFinite(audio), "the CPU guardrail render was not finite");
        return std::chrono::duration<double>(end - begin).count()
             / (static_cast<double>(audio.size()) / sampleRate);
    };

    // The two configurations are timed alternately, each from a freshly struck
    // chord, and each keeps its fastest sample. Measuring one configuration to
    // completion and then the other lets a busy stretch on a shared runner land
    // entirely on whichever went second, which is enough to invert a real
    // difference; interleaving exposes both to the same noise.
    double worstCase = 1.0e9;
    double defaultCase = 1.0e9;
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        ElectryEngine worstEngine;
        strike (worstEngine, PickupSelector::Both, electry::OutputMode::Stereo);
        worstCase = std::min (worstCase, timeRender (worstEngine, buffer));

        ElectryEngine defaultEngine;
        strike (defaultEngine, PickupSelector::Bridge, electry::OutputMode::Mono);
        defaultCase = std::min (defaultCase,
                                timeRender (defaultEngine, buffer));
    }
    std::cout << "Eight-string render CPU ratio at 96 kHz: " << worstCase
              << "x worst case (Both + Stereo), " << defaultCase
              << "x default (Bridge + Mono)\n";

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    constexpr double ceiling = 40.0;
#else
    // Loose portable runaway guard; shared CI runners are not a stable
    // benchmark fixture.
    constexpr double ceiling = 8.0;
#endif
    expect(worstCase < ceiling,
           "eight-string render exceeded the portable CPU ceiling");
    expect(defaultCase < ceiling,
           "default-configuration render exceeded the portable CPU ceiling");

    double palmCase = 1.0e9;
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        ElectryEngine palmEngine;
        strike (palmEngine, PickupSelector::Both,
                electry::OutputMode::Stereo, PlayStyle::PalmMute);
        palmCase = std::min(
            palmCase, timeRender(palmEngine, palmBuffer));
    }
    std::cout << "Eight-string Palm CPU ratio over 0-110 ms at 96 kHz: "
              << palmCase << "x\n";
    expect(palmCase < ceiling,
           "eight-string Palm render exceeded the portable CPU ceiling");

    // The complementary production case: one played string drives all seven
    // idle open strings through the bridge. The eight-active fixture above
    // cannot measure sympathetic-string work because no idle voice remains.
    const auto strikeWithIdleStrings = [&] (
        ElectryEngine& engine, PickupSelector selector,
        electry::OutputMode mode)
    {
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupSelector = selector;
        parameters.outputMode = mode;
        parameters.sympatheticAmount = 1.0f;
        parameters.artifactAmount = 0.0f;
        parameters.bodyResonance = 0.0f;
        parameters.stringAge = 0.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.9f); // open A2; seven physical strings remain idle
    };

    double idleWorstCase = 1.0e9;
    double idleDefaultCase = 1.0e9;
    StereoBuffer idleWake(512);
    const auto wakeIdleStrings = [&] (ElectryEngine& engine,
                                      const std::string& context)
    {
        renderInto(engine, idleWake);
        expect(engine.getActiveVoiceCount() == 1
                   && engine.getSympatheticStringCount()
                          == ElectryEngine::stringCount - 1,
               context + " did not keep one driver and seven coupled strings");
    };
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        ElectryEngine worstEngine;
        strikeWithIdleStrings(
            worstEngine, PickupSelector::Both, electry::OutputMode::Stereo);
        wakeIdleStrings(worstEngine, "Both/Stereo idle CPU fixture");
        idleWorstCase = std::min(
            idleWorstCase, timeRender(worstEngine, buffer));

        ElectryEngine defaultEngine;
        strikeWithIdleStrings(
            defaultEngine, PickupSelector::Bridge, electry::OutputMode::Mono);
        wakeIdleStrings(defaultEngine, "Bridge/Mono idle CPU fixture");
        idleDefaultCase = std::min(
            idleDefaultCase, timeRender(defaultEngine, buffer));
    }
    std::cout << "One-active/seven-idle render CPU ratio at 96 kHz: "
              << idleWorstCase << "x worst case (Both + Stereo), "
              << idleDefaultCase << "x default (Bridge + Mono)\n";
    expect(idleWorstCase < ceiling && idleDefaultCase < ceiling,
           "the sympathetic-string render exceeded the portable CPU ceiling");

    // The idle strings share the played-string dispersion fitter. Exercise a
    // full wheel glide here as well as the settled render above: without the
    // six-cent fit quantum this would run seven 520-candidate searches on
    // every control tick, a cost the all-active glide below cannot expose.
    double idleGlideCase = 1.0e9;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        ElectryEngine glideEngine;
        strikeWithIdleStrings(
            glideEngine, PickupSelector::Both, electry::OutputMode::Stereo);
        wakeIdleStrings(glideEngine, "idle wheel-glide CPU fixture");
        glideEngine.setPitchBend(1.0f);
        idleGlideCase = std::min(
            idleGlideCase, timeRender(glideEngine, buffer));
    }
    std::cout << "One-active/seven-idle wheel-glide CPU ratio at 96 kHz: "
              << idleGlideCase << "x\n";
    expect(idleGlideCase < ceiling,
           "a bent sympathetic-string bank exceeded the portable CPU ceiling");
    expect(idleGlideCase < idleWorstCase * 2.0,
           "the sympathetic wheel glide costs far more than its settled "
           "render (" + std::to_string(idleGlideCase) + "x vs "
               + std::to_string(idleWorstCase) + "x)");

    // The default configuration is cheaper than the worst case, and it is
    // deliberately not asserted here. It used to be, as `defaultCase <
    // worstCase * 0.97`, and that assertion was flaky: the saving it looks for
    // is a few per cent of the render, CI has measured it as low as 1.3%, and
    // that is smaller than the run-to-run spread of a wall clock on a shared
    // runner. Interleaving the two configurations and keeping each one's
    // fastest sample -- which is what the loop above does, and is the right
    // thing to do -- narrows that spread but cannot get underneath it. No
    // threshold makes a timing comparison both sensitive to a 1% difference
    // and reliable on hardware this project does not control.
    //
    // Nothing is lost by dropping it, because the claim underneath it is
    // structural rather than statistical: the default skips the unselected
    // pickup chain and runs one shared output chain instead of two.
    // testPickupCullingAndChannelLinking asserts exactly that, directly and
    // deterministically, by reading the engine's own culling and link flags for
    // every selector position. A regression that made the default do the worst
    // case's work fails there, on the first run, on any machine.
    //
    // What is left here is what a CPU guardrail is actually for: a runaway
    // ceiling. That is a threshold a wall clock can carry, because it is orders
    // of magnitude away from the noise rather than inside it.
    std::cout << "  (the default/worst-case ratio is reported, not asserted; "
              << "the culling it reflects is asserted structurally in "
              << "testPickupCullingAndChannelLinking)\n";

    // A full-throw wheel glide on the same chord must not be a hidden second
    // worst case. Re-fitting the dispersion grid on every control tick of the
    // glide once cost several times the settled render - beyond realtime on
    // this configuration - which the settled measurements above cannot see.
    double glideCase = 1.0e9;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        ElectryEngine glideEngine;
        strike (glideEngine, PickupSelector::Both, electry::OutputMode::Stereo);
        glideEngine.setPitchBend (1.0f);
        glideCase = std::min (glideCase, timeRender (glideEngine, buffer));
    }
    std::cout << "Eight-string wheel-glide CPU ratio at 96 kHz: " << glideCase
              << "x\n";
    expect(glideCase < ceiling,
           "a bent eight-string chord exceeded the portable CPU ceiling ("
               + std::to_string(glideCase) + "x)");
    expect(glideCase < worstCase * 2.0,
           "the wheel glide costs far more than the settled worst case ("
               + std::to_string(glideCase) + "x vs "
               + std::to_string(worstCase) + "x)");
}

} // namespace

int main()
{
    testPassiveContactFoldedRingReference();
#if ELECTRY_ANALYTIC_RELEASE_IC
    testAnalyticReleasePickupSpatialHistory();
    testAnalyticReleasedStringInitialCondition();
    testAnalyticReleaseMaximumRateAttackCost();
#endif
    testModalResonatorPeakGain();
#if ELECTRY_MEASURED_BODY_RESPONSE
    testMeasuredBodyResponsePhysics();
#endif
    testInternalOversamplingPolicy();
    testStringDelaySmoothingTimeConstant();
    testPrepareClampsHostileSampleRate();
    testRenderMatrixFiniteAndBounded();
    testPitchAccuracy();
    testDropELowNoteAtMaximumRate();
    testPrepareSanitisesSampleRate();
    testProcessRejectsInvalidBuffers();
    testDeterminism();
    testKeyswitchesSelectStylesSilently();
    testOverlappingSameNoteOffKeepsLatestRepickHeld();
    testHeldStringRepickKeys();
    testHeldTremoloPickingGesture();
    testHammerLatchedRepicksUsePickingHand();
    testAlternateStrokeSequence();
    testAlternateChordSharesOneStroke();
    testChordSharesPickingHandVariation();
    testRapidSameStringRepicksAreSeparateStrokes();
    testZeroSpreadAlternateRunChangesStrings();
    testArticulationsSoundDistinct();
    testStyleAndStrokeCombinations();
    testFingeredNotesDrawNoPickingHandVariation();
    testExplicitLegacyExpressionIdIsBitExact();
    testExpressionPitchBendIsSelective();
    testDelayedRepickPreservesFrozenExpressionPitch();
    testSamePitchExpressionOwnersReleaseIndependently();
    testPitchWheelUsesUniformSemitoneInterval();
    testReleaseDampingUsesPerformedPitch();
    testHeldDampingUsesPerformedPitch();
    testStiffnessFollowsLiveTension();
    testPitchWheelGlideFollowsBendTime();
    testPitchWheelBendsSympatheticStrings();
    testHeldLegatoSourceOutranksPlainReleaseTails();
    testHammerOnLegatoContinuity();
    testPullOffLegatoDirection();
#if ELECTRY_DECOUPLED_PICK_RELEASE
    testFingerReleaseUsesContactPeriodAcrossRates();
#endif
    testHardPickingStaysInTune();
#if ELECTRY_ENERGY_ATTACK_PITCH
    testEnergyAttackPitchExperiment();
#endif
    testAttackStateTransitions();
    testPickupsToneAndBuildMorph();
    testPickupGeometryFollowsLiveWaveSpeed();
    testHumbuckerTwoCoilNotch();
    testArtifactsControl();
#if ELECTRY_POSITIONED_FRET_COLLISION
    testPositionedFretCollision();
#endif
    testAdvancedDispersionAndBodyConductance();
    testLowRegisterGuitarEnvelope();
    testOpenLowStringLevelBalance();
    testMonoStereoOutputField();
    testVelocityDynamicRange();
    testVelocityExpression();
    testPickingHandVariation();
    testMaterialAndControlAudibility();
    testGuitarBuildMacro();
    testGuitarBuildRangeIsAudible();
    testNoiseComponentsAndSilence();
    testStringAllocationAndPolyphony();
    testChordAssignmentIsPermutationInvariant();
    testVoiceStealingPriority();
    testNoteOnVelocitySanitisation();
    testSetVibratoSanitisation();
    testSetPitchBendSanitisation();
    testSetResonanceReturnLevelAndPalmMutePressureSanitisation();
    testDelayTapClampsAndInterpolates();
    testFrettingHandPosition();
    testTouchHarmonics();
    testPinchHarmonic();
    testSlideArticulation();
    testFrettingHandVibrato();
    testVibratoIsAHandNotAnLfo();
    testVibratoRequiresHeldFinger();
    testDeadNote();
    testSustainPedal();
    testVibratoOnlyMovesFingeredStrings();
    testLegatoSlideDoesNotConsumeAPickStroke();
    testLiveDampingRefitsPreservePitch();
    testSharedHandRetunesActiveStringDamping();
    testSympatheticBridgeCoupling();
    testPalmMuteContinuum();
    testPalmMuteHandContactDynamics();
    testTremoloStudyClearsPalmBeforeHighLead();
    testPalmMuteSpectralLoss();
    testRapidPalmMuteChugs();
    testScoreMatchedC2PalmProxy();
    testExtendedRangeMutedMatrixProxy();
    testPalmHandLossStartsEngaged();
    testPalmAttackContactsCompose();
    testPalmImpactIsSampleRateInvariant();
    testPalmImpactWaitsForStrokeAndClears();
    testSeededPlayerTiming();
    testStrumSpread();
    testStrumTravelFollowsStroke();
    testResonanceControlRaisesSympatheticRing();
    testResonanceFeedbackSelfSustains();
    testPickGeometryFollowsFret();
    testPickContactGeometry();
    testPalmMuteDoesNotShiftPitch();
#if ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2
    testLowestStringLossCorrection();
#endif
    testHandDipNeverExpands();
    testLowRegisterFundamentalWeight();
    testVisualStateAndGeometry();
    testVisualStateSanitizesNonFiniteInput();
    testVisualGeometryClampsOutOfRangeInput();
    testPickupCullingAndChannelLinking();
    testIdleFreezeAndDenormalSafety();
    testDecayIsSampleRateInvariant();
    testPolarisationCouplingIsRateInvariant();
    testCoupledStringLosesItsTopEndLikeAPlayedString();
    testCoupledStringKeepsItsFundamentalDecayTarget();
    testFingeredStringsShareTheBridge();
    testParameterSanitisation();
    testParameterSanitisationFallsBackToDefaults();
    testAcousticReturnUsesFixedNominalDelay();
    testPushAcousticReturnSanitisation();
    testCpuGuardrail();

    // Palm-mute bridge impact smoke check.
    {
        constexpr auto sampleRate = 48000.0;
        auto engineStorage = std::make_unique<ElectryEngine>();
        auto& engine = *engineStorage;
        engine.prepare(sampleRate, 512);

        electry::EngineParameters parameters {};
        parameters.muteDamping = 0.8f;
        engine.setParameters(parameters);

        const auto muted = renderNote(engine, sampleRate, 28, 0.95f, PlayStyle::PalmMute, 0.5, 0.1);
        expect(peakAbs(muted.left) > 1.0e-5f, "palm mute stroke was silent");
        expect(peakAbs(muted.left) < 16.0f, "palm mute stroke exceeded peak guardrail");
    }

    if (failures != 0)
    {
        std::cerr << failures << " Electry DSP check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Electry DSP checks passed.\n";
    return EXIT_SUCCESS;
}
