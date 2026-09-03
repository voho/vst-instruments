// Circuit-block suite: laws the individual modelled circuits must obey, as
// opposed to what the played instrument does. Each check pins a behaviour
// the modelling contract anchors — keyboard law, pulse duties, the dual
// filter's modes, self-oscillation, the brightness pole, the explicitly
// labelled Shaper-VCA seam, and the resolved MM5837 noise circuit.

#include "DSP/GhostarEngine.h"
#include "DSP/SpiritNoise.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace ghostar
{
struct HighQProbeResult
{
    double lp;
    double bp;
    double hp;
    double bandpassCompanion;
    double lowpassCompanion;
    double chargeCompanion;
};

struct HighQProbeConfig
{
    double lowerGain;
    double lowerResistance;
    double upperGain;
    double upperResistance;
};

struct LowerMnaProbeResult
{
    double lp;
    double bp;
    double hp;
    std::array<double, 3> mixerCompanions;
    double bandpassCompanion;
    double lowpassCompanion;
    double highQCompanion;
    double highQChargeStep;
};

struct UpperCascadeProbeResult
{
    double output;
    double controlledLowpass;
    double controlledBandpass;
    double fixedLowpass;
    double fixedBandpass;
    std::array<double, 4> companions;
    double highQCompanion;
    double highQChargeStep;
};

struct UpperSlopeProjectionProbeResult
{
    double controlledLowpass;
    double fixedLowpass;
    double controlledLowpassCompanion;
    double fixedLowpassCompanion;
    double highQCompanion;
};

struct OverdriveProbeResult
{
    double output;
    double couplingCompanion;
};

struct ShaperCycleProbeResult
{
    int riseSamples;
    int fallSamples;
    int gateHighSamples;
};

struct ShaperPhaseProbeResult
{
    double level;
    bool rising;
    bool active;
    bool gate;
};

struct PassiveCompanionProbeResult
{
    double output;
    double companion;
};

struct OutputNetworkProbeResult
{
    double filterWiper;
    double shaperWiper;
    double shaperTop;
    double companion;
};

struct PitchControlLagProbeResult
{
    double pole;
    double nowWeight;
    double previousWeight;
    std::array<double, 64> step;
    bool startedUninitialised;
    bool resetClearedState;
    double resetInitialOutput;
    double resetSteadyOutput;
};

struct ModulationProbeResult
{
    double oscillatorAOctaves;
    double oscillatorBOctaves;
    double upperOctaves;
    double lowerOctaves;
    double pwmA;
    double pwmB;
    double audioGain;
    double audioAOctaves;
    double audioBOctaves;
    double audioUpperOctaves;
    double audioLowerOctaves;
    double audioDuty;
    bool audioActive;
};

struct ModSourceTickProbeResult
{
    bool audioActive { false };
    bool sourceMatches { false };
    double sharedAudioError { 0.0 };
    double sharedRedError { 0.0 };
    double basePitchA { 0.0 };
    double basePitchB { 0.0 };
    double baseDutyA { 0.0 };
    std::array<double, 4> referenceRed {};
    std::array<double, 4> pitchA {};
    std::array<double, 4> pitchB {};
    std::array<double, 4> dutyA {};
    std::array<double, 4> upperFilter {};
    std::array<double, 4> lowerFilter {};
};

struct SampleHoldRedEdgeProbeResult
{
    double availableEngineRed { 0.0 };
    double availableReferenceRed { 0.0 };
    double captured { 0.0 };
    std::array<double, 4> laterEngineRed {};
    std::array<double, 4> laterReferenceRed {};
    std::array<double, 4> held {};
};

struct EnvelopeResetProbeResult
{
    int releaseSamples;
    double before;
    double afterNotch;
    double afterRestart;
    bool stayedInRelease;
    bool restartedAttack;
};

struct LfoResetProbeResult
{
    int clampedSamples;
    double capacitorAfterClamp;
    double visibleDuringClamp;
    double capacitorAfterRelease;
    double visibleAfterRelease;
    double sampleHoldAfterFirstStep;
    bool risingAfterRelease;
};

struct LfoGateXProbeResult
{
    int envelopeResetSamples;
    bool finalSquareHigh;
    bool shaperCycleActive;
    double shaperLevel;
};

struct ExternalGateProbeResult
{
    bool internalYHigh;
    bool envelopeGate;
};

struct ExternalGateFallProbeResult
{
    bool openBeforeFall;
    bool internalYHighAfterFall;
    bool openAfterFall;
};

struct ExternalGateHardStopProbeResult
{
    bool restartedResetWhileHeldHigh;
    bool attackedWhileHeldHigh;
    bool restartedShaperWhileHeldHigh;
    int resetSamplesAfterRealEdge;
    bool attackedAfterRealEdge;
    bool shaperStartedAfterRealEdge;
};

struct ExternalAudioTimingProbeResult
{
    double oracleError;
    double impulseSum;
    double impulseCentroid;
    double symmetryError;
    double passbandMagnitude;
    double firstImageMagnitude;
    double secondImageMagnitude;
    double dcError;
    int firstDelayedFrameTick;
    double jackStateCaptureDifference;
    double jackSelectionDifference;
    double brightnessTickZeroAlignmentDifference;
    double brightnessTickZeroSelectionDifference;
    double brightnessTickOneSelectionDifference;
    double brightnessStateDifference;
    double brightnessCompanionMagnitude;
};

struct ExternalAudioMixerProbeResult
{
    double filterEnergy;
    double shaperEnergy;
    double filterSum;
    double shaperSum;
};

struct ExternalAudioProcessProbeResult
{
    double impulseSum;
    double impulseCentroid;
    double polarityError;
};

struct ExternalAudioResetProbeResult
{
    double stageBBeforeReset;
    double stageABeforeReset;
    double stageBAfterReset;
    double stageAAfterReset;
};

struct ExternalAudioRedNoiseProbeResult
{
    double cutoffSpan;
    double jackCutoffDifference;
    double jackRedNoiseDifference;
};

struct ExternalPitchProbeResult
{
    double glidedNote;
    double oscillatorAOctaves;
    double oscillatorBOctaves;
    double oscillatorBDroneHz;
    double upperCutoffHz;
    double lowerCutoffHz;
    double storedSourceVolts;
    bool jackInserted;
    bool keyboardGate;
    bool envelopeGate;
};

struct ExternalPitchGlideProbeResult
{
    double offStep;
    double onStep;
    double autoOneKeyStep;
    double autoTwoKeyStep;
    double autoAfterDroppingToOneKey;
    double internalNoteStep;
    double beforeInsertion;
    double afterInsertion;
    double afterRemoval;
    double beforeReset;
    double afterReset;
    double afterResetTick;
};

struct OscBPedalProbeResult
{
    double oscBNodeVolts;
    double oscillatorAOctaves;
    double oscillatorBOctaves;
    double oscillatorBDroneHz;
    double upperCutoffHz;
    double lowerCutoffHz;
    double oscBResistanceKOhm;
    bool oscBInserted;
};

struct FilterPedalProbeResult
{
    double nodeVolts;
    double oscillatorAOctaves;
    double oscillatorBOctaves;
    double upperCutoffHz;
    double lowerCutoffHz;
    double resistanceKOhm;
    bool inserted;
};

struct RearPedalRetentionProbeResult
{
    std::array<double, 6> oscBNodes;
    std::array<double, 6> filterNodes;
};

struct FilterPedalModeSwitchProbeResult
{
    double formantNodeDifference;
    double dynamicNodeDifference;
    double formantUpperCutoffDifference;
    double formantLowerCutoffDifference;
    double dynamicUpperCutoffDifference;
    double dynamicLowerCutoffDifference;
};

struct FilterPedalRemovalProbeResult
{
    double connectedNode;
    double connectedOpenReference;
    double removedNode;
    double removedOpenReference;
    double resetNode;
    double resetOpenReference;
    double stoppedNode;
    double stoppedOpenReference;
};

struct GhostarCircuitTestAccess
{
    static OscBPedalProbeResult oscBPedalAt(
        bool inserted, double resistanceKOhm,
        OscBRange oscillatorBRange = OscBRange::Unison) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.oscBRange = oscillatorBRange;
        parameters.interval = 0.5f;
        parameters.cutoff = 0.5f;
        parameters.lowerOnly = 0.8f;
        parameters.kbAmount = 0.0f;
        parameters.filterEnvAmount = 0.5f;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.currentNote_ = 69;
        engine.setOscBPedalInput(inserted, resistanceKOhm);
        engine.advanceControls();
        return { engine.oscBPedalNodeVolts_,
                 engine.controlOscAOctaves_,
                 engine.controlOscBOctaves_,
                 engine.controlOscBDroneHz_,
                 engine.controlUpperCutoffHz_,
                 engine.controlLowerCutoffHz_,
                 engine.oscBPedalResistanceKOhm_,
                 engine.oscBPedalJackInserted_ };
    }

    static FilterPedalProbeResult filterPedalAt(
        bool inserted, double resistanceKOhm,
        TrackingMode tracking = TrackingMode::Dynamic) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.interval = 0.5f;
        parameters.cutoff = 0.5f;
        parameters.lowerOnly = 0.8f;
        parameters.kbAmount = 0.0f;
        parameters.filterEnvAmount = 0.5f;
        parameters.tracking = tracking;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.currentNote_ = 69;
        engine.setFilterPedalInput(inserted, resistanceKOhm);
        engine.advanceControls();
        return { engine.filterPedalNodeVolts_,
                 engine.controlOscAOctaves_,
                 engine.controlOscBOctaves_,
                 engine.controlUpperCutoffHz_,
                 engine.controlLowerCutoffHz_,
                 engine.filterPedalResistanceKOhm_,
                 engine.filterPedalJackInserted_ };
    }

    static RearPedalRetentionProbeResult rearPedalRetention() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.tracking = TrackingMode::Dynamic;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;

        engine.advanceControls();
        RearPedalRetentionProbeResult result;
        result.oscBNodes[0] = engine.oscBPedalNodeVolts_;
        result.filterNodes[0] = engine.filterPedalNodeVolts_;

        engine.setOscBPedalInput(true, 100.0);
        engine.setFilterPedalInput(true, 100.0);
        engine.advanceControls();
        result.oscBNodes[1] = engine.oscBPedalNodeVolts_;
        result.filterNodes[1] = engine.filterPedalNodeVolts_;

        engine.reset();
        result.oscBNodes[2] = engine.oscBPedalNodeVolts_;
        result.filterNodes[2] = engine.filterPedalNodeVolts_;
        engine.advanceControls();
        result.oscBNodes[3] = engine.oscBPedalNodeVolts_;
        result.filterNodes[3] = engine.filterPedalNodeVolts_;

        engine.stopAllSound();
        result.oscBNodes[4] = engine.oscBPedalNodeVolts_;
        result.filterNodes[4] = engine.filterPedalNodeVolts_;

        engine.setOscBPedalInput(true, 0.0);
        engine.setFilterPedalInput(true, 0.0);
        engine.advanceControls();
        result.oscBNodes[5] = engine.oscBPedalNodeVolts_;
        result.filterNodes[5] = engine.filterPedalNodeVolts_;
        return result;
    }

    static FilterPedalModeSwitchProbeResult
    unpluggedFilterPedalModeSwitch() noexcept
    {
        const auto configure = [](GhostarEngine& engine,
                                  TrackingMode tracking) noexcept {
            EngineParameters parameters;
            parameters.cutoff = 0.5f;
            parameters.lowerOnly = 0.8f;
            parameters.kbAmount = 0.0f;
            parameters.filterEnvAmount = 0.5f;
            parameters.tracking = tracking;
            engine.parameters_ = parameters;
            engine.targetParameters_ = parameters;
        };

        GhostarEngine switched;
        switched.prepare(48000.0, 64);
        configure(switched, TrackingMode::Dynamic);
        switched.advanceControls();
        configure(switched, TrackingMode::Formant);
        switched.advanceControls();

        GhostarEngine freshFormant;
        freshFormant.prepare(48000.0, 64);
        configure(freshFormant, TrackingMode::Formant);
        freshFormant.advanceControls();
        const double formantNodeDifference =
            switched.filterPedalNodeVolts_
            - switched.filterPedalOpenNodeVolts_;
        const double formantUpperCutoffDifference =
            switched.controlUpperCutoffHz_
            - freshFormant.controlUpperCutoffHz_;
        const double formantLowerCutoffDifference =
            switched.controlLowerCutoffHz_
            - freshFormant.controlLowerCutoffHz_;

        configure(switched, TrackingMode::Dynamic);
        switched.advanceControls();
        GhostarEngine freshDynamic;
        freshDynamic.prepare(48000.0, 64);
        configure(freshDynamic, TrackingMode::Dynamic);
        freshDynamic.advanceControls();
        return {
            formantNodeDifference,
            switched.filterPedalNodeVolts_
                - switched.filterPedalOpenNodeVolts_,
            formantUpperCutoffDifference,
            formantLowerCutoffDifference,
            switched.controlUpperCutoffHz_
                - freshDynamic.controlUpperCutoffHz_,
            switched.controlLowerCutoffHz_
                - freshDynamic.controlLowerCutoffHz_,
        };
    }

    static FilterPedalRemovalProbeResult filterPedalRemoval() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.tracking = TrackingMode::Dynamic;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;

        engine.setFilterPedalInput(true, 100.0);
        engine.advanceControls();
        FilterPedalRemovalProbeResult result {
            engine.filterPedalNodeVolts_,
            engine.filterPedalOpenNodeVolts_,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
        };
        engine.setFilterPedalInput(false, 100.0);
        engine.advanceControls();
        result.removedNode = engine.filterPedalNodeVolts_;
        result.removedOpenReference = engine.filterPedalOpenNodeVolts_;

        engine.reset();
        result.resetNode = engine.filterPedalNodeVolts_;
        result.resetOpenReference = engine.filterPedalOpenNodeVolts_;
        engine.stopAllSound();
        result.stoppedNode = engine.filterPedalNodeVolts_;
        result.stoppedOpenReference = engine.filterPedalOpenNodeVolts_;
        return result;
    }

    static ExternalPitchProbeResult externalPitchAt(
        double sourceVolts, bool jackInserted, int keyboardNote = 72,
        TrackingMode tracking = TrackingMode::Dynamic,
        float keyboardAmount = 1.0f,
        OscBRange oscillatorBRange = OscBRange::Unison,
        MasterOctave octave = MasterOctave::Eight,
        float pitchBend = 0.0f) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.octave = octave;
        parameters.oscBRange = oscillatorBRange;
        parameters.interval = 0.5f;
        parameters.cutoff = 0.5f;
        parameters.kbAmount = keyboardAmount;
        parameters.filterEnvAmount = 0.5f;
        parameters.tracking = tracking;
        parameters.glideMode = GlideMode::Off;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.setPitchBend(pitchBend);
        engine.setExternalPitchInput(jackInserted, sourceVolts);
        if (keyboardNote >= 0)
            engine.noteOn(keyboardNote, 1.0f);
        engine.advanceControls();
        return { engine.glidedNote_, engine.controlOscAOctaves_,
                 engine.controlOscBOctaves_, engine.controlOscBDroneHz_,
                 engine.controlUpperCutoffHz_, engine.controlLowerCutoffHz_,
                 engine.externalPitchSourceVolts_,
                 engine.externalPitchJackInserted_, engine.keyGate_,
                 engine.envelopeGate_ };
    }

    static std::array<double, 2> externalPitchNodes(
        double sourceVolts) noexcept
    {
        const auto nodes = GhostarEngine::externalPitchNodes(sourceVolts);
        return { nodes.loadedVolts, nodes.conditionedVolts };
    }

    static ExternalPitchGlideProbeResult externalPitchGlideModes() noexcept
    {
        const auto step = [](GlideMode mode, int heldKeys) {
            GhostarEngine engine;
            engine.prepare(48000.0, 64);
            EngineParameters parameters;
            parameters.glide = 1.0f;
            parameters.glideMode = mode;
            engine.parameters_ = parameters;
            engine.targetParameters_ = parameters;
            engine.setExternalPitchInput(true, 0.0);
            engine.noteOn(60, 1.0f);
            if (heldKeys == 2)
                engine.noteOn(64, 1.0f);
            engine.advanceControls();
            engine.setExternalPitchInput(true, 1.1);
            engine.advanceControls();
            return engine.glidedNote_;
        };

        GhostarEngine autoDrop;
        autoDrop.prepare(48000.0, 64);
        EngineParameters autoParameters;
        autoParameters.glide = 1.0f;
        autoParameters.glideMode = GlideMode::Auto;
        autoDrop.parameters_ = autoParameters;
        autoDrop.targetParameters_ = autoParameters;
        autoDrop.setExternalPitchInput(true, 0.0);
        autoDrop.noteOn(60, 1.0f);
        autoDrop.noteOn(64, 1.0f);
        autoDrop.advanceControls();
        autoDrop.setExternalPitchInput(true, 1.1);
        autoDrop.advanceControls();
        autoDrop.noteOff(64);
        autoDrop.setExternalPitchInput(true, 2.2);
        autoDrop.advanceControls();

        GhostarEngine internalStep;
        internalStep.prepare(48000.0, 64);
        internalStep.noteOn(60, 1.0f);
        internalStep.advanceControls();
        internalStep.noteOn(72, 1.0f);
        internalStep.advanceControls();

        GhostarEngine switched;
        switched.prepare(48000.0, 64);
        EngineParameters switchedParameters;
        switchedParameters.glide = 1.0f;
        switchedParameters.glideMode = GlideMode::On;
        switched.parameters_ = switchedParameters;
        switched.targetParameters_ = switchedParameters;
        switched.noteOn(48, 1.0f);
        switched.advanceControls();
        const double beforeInsertion = switched.glidedNote_;
        switched.setExternalPitchInput(true, 0.0);
        switched.advanceControls();
        const double afterInsertion = switched.glidedNote_;
        switched.setExternalPitchInput(false, 0.0);
        switched.advanceControls();

        GhostarEngine resetState;
        resetState.prepare(48000.0, 64);
        resetState.parameters_ = switchedParameters;
        resetState.targetParameters_ = switchedParameters;
        resetState.setExternalPitchInput(true, 0.0);
        resetState.advanceControls();
        resetState.setExternalPitchInput(true, 1.1);
        resetState.advanceControls();
        const double beforeReset = resetState.glidedNote_;
        resetState.reset();
        const double afterReset = resetState.glidedNote_;
        resetState.advanceControls();

        return { step(GlideMode::Off, 1), step(GlideMode::On, 1),
                 step(GlideMode::Auto, 1), step(GlideMode::Auto, 2),
                 autoDrop.glidedNote_, internalStep.glidedNote_,
                 beforeInsertion, afterInsertion, switched.glidedNote_,
                 beforeReset, afterReset, resetState.glidedNote_ };
    }

    static std::array<double, 2> externalPitchArpeggiatorTargets() noexcept
    {
        const auto target = [](bool inserted) {
            GhostarEngine engine;
            engine.prepare(48000.0, 64);
            EngineParameters parameters;
            parameters.arpeggiator = ArpeggiatorMode::Ripple;
            engine.parameters_ = parameters;
            engine.targetParameters_ = parameters;
            engine.currentNote_ = 60;
            engine.arpSoundingNote_ = 84;
            // Keep this probe between clocks: it is testing the switched KCV
            // target, not asking handleArpClock() to scan an empty key stack.
            engine.lfoCapLevel_ = 0.0;
            engine.lfoRising_ = true;
            engine.lfoSquareHigh_ = true;
            engine.previousLfoSquareHigh_ = true;
            engine.setExternalPitchInput(inserted, 0.0);
            engine.advanceControls();
            return engine.glidedNote_;
        };
        return { target(false), target(true) };
    }

    static bool externalPitchSetterPreservesKeyboardState() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        engine.noteOn(67, 1.0f);
        const auto keyStack = engine.keyStack_;
        const int keyStackSize = engine.keyStackSize_;
        const bool keyGate = engine.keyGate_;
        const int currentNote = engine.currentNote_;
        const bool trigger = engine.pendingTrigger_;
        const bool lfoReset = engine.pendingLfoReset_;
        const bool shaperTrigger = engine.pendingShaperTrigger_;
        engine.setExternalPitchInput(true,
            std::numeric_limits<double>::quiet_NaN());
        return engine.externalPitchJackInserted_
            && engine.externalPitchSourceVolts_ == 0.0
            && engine.keyStack_ == keyStack
            && engine.keyStackSize_ == keyStackSize
            && engine.keyGate_ == keyGate
            && engine.currentNote_ == currentNote
            && engine.pendingTrigger_ == trigger
            && engine.pendingLfoReset_ == lfoReset
            && engine.pendingShaperTrigger_ == shaperTrigger;
    }

    static double selectedWaveVolts(Waveform waveform,
                                    double bipolarSample) noexcept
    {
        return GhostarEngine::p1014SelectedWaveVolts(waveform,
                                                     bipolarSample);
    }

    static PitchControlLagProbeResult pitchControlLagAt(
        double hostRate) noexcept
    {
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        const bool startedUninitialised = !engine.pitchLagA_.initialised;
        engine.runPitchControlLag(engine.pitchLagA_, 0.0);

        std::array<double, 64> step {};
        for (double& output : step)
            output = engine.runPitchControlLag(engine.pitchLagA_, 1.0);

        engine.reset();
        const bool resetClearedState = !engine.pitchLagA_.initialised
            && !engine.pitchLagB_.initialised;
        const double resetInitialOutput =
            engine.runPitchControlLag(engine.pitchLagA_, 0.375);
        const double resetSteadyOutput =
            engine.runPitchControlLag(engine.pitchLagA_, 0.375);
        return { engine.pitchLagPole_, engine.pitchLagNow_,
                 engine.pitchLagPrevious_, step, startedUninitialised,
                 resetClearedState, resetInitialOutput, resetSteadyOutput };
    }

    static std::array<double, 4> pitchControlBaseStep(
        bool sync) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.sync = sync;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.phaseA_ = 0.0;
        engine.phaseB_ = 0.0;
        engine.controlOscAOctaves_ = 1.0;
        engine.controlOscBOctaves_ = 1.0;
        engine.controlOscBDrone_ = false;
        engine.controlAudioRateMod_ = GhostarEngine::AudioRateMod {};
        engine.pitchLagA_ = { 0.0, 0.0, true };
        engine.pitchLagB_ = { 0.0, 0.0, true };
        engine.renderVoiceSample();
        return { engine.phaseA_, engine.phaseB_, engine.pitchLagA_.output,
                 engine.pitchLagB_.output };
    }

    static std::array<double, 2> cyclicPitchControlHeldStep() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.sync = true;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.phaseA_ = 0.0;
        engine.phaseB_ = 0.0;
        engine.controlOscAOctaves_ = 0.0;
        engine.controlOscBOctaves_ = 0.0;
        engine.controlOscBDrone_ = false;
        engine.controlAudioRateMod_ = GhostarEngine::AudioRateMod {};
        engine.controlAudioRateMod_.active = true;
        engine.controlAudioRateMod_.gain = 1.0;
        engine.controlAudioRateMod_.aOctaves = 1.0;
        engine.controlAudioRateMod_.bOctaves = 1.0;
        engine.lastOscBWave_ = 1.0;
        engine.pitchLagA_ = { 0.0, 1.0, true };
        engine.pitchLagB_ = { 0.0, 1.0, true };
        engine.renderVoiceSample();
        return { engine.phaseA_, engine.phaseB_ };
    }

    static std::array<double, 5> oscillatorModTapAtSawWrap(
        double previousConditionedB, bool sync) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.oscAWaveform = Waveform::Triangle;
        parameters.oscBWaveform = Waveform::Sawtooth;
        parameters.sync = sync;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.phaseA_ = 0.0;
        constexpr double internalRate = 4.0 * 48000.0;
        const double bStep = 440.0 * std::exp2(previousConditionedB)
                           / internalRate;
        engine.phaseB_ = 1.0 - 0.4 * bStep;
        engine.heldWaveB_ = 2.0 * engine.phaseB_ - 1.0;
        engine.heldWaveformA_ = Waveform::Triangle;
        engine.heldWaveformB_ = Waveform::Sawtooth;
        engine.lastOscBWave_ = previousConditionedB;
        engine.controlOscAOctaves_ = 0.0;
        engine.controlOscBOctaves_ = 0.0;
        engine.controlOscBDrone_ = false;
        engine.controlAudioRateMod_ = GhostarEngine::AudioRateMod {};
        engine.controlAudioRateMod_.active = true;
        engine.controlAudioRateMod_.gain = 1.0;
        engine.controlAudioRateMod_.aOctaves = 1.0;
        engine.controlAudioRateMod_.bOctaves = 1.0;
        // Prime both physical multiplier capacitors at the preceding B
        // voltage. Leaving them in their reset state would exercise only the
        // intentional no-swoop initialization and conceal the RC transition.
        engine.runPitchControlLag(engine.pitchLagA_, previousConditionedB);
        engine.runPitchControlLag(engine.pitchLagB_, previousConditionedB);
        engine.renderVoiceSample();
        return { engine.phaseA_, engine.phaseB_, engine.lastOscBWave_,
                 engine.pitchLagA_.output, engine.pitchLagB_.output };
    }

    static double oscillatorWaveSwitchHeldVoltage() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.oscBWaveform = Waveform::Sawtooth;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.phaseB_ = 0.25;
        // This deferred sample was created while TRIANGLE was selected. Its
        // bipolar value happens to be zero, which new SAW scaling would turn
        // into a visibly different voltage.
        engine.heldWaveB_ = 0.0;
        engine.heldWaveformB_ = Waveform::Triangle;
        engine.controlOscBOctaves_ = 0.0;
        engine.controlOscBDrone_ = false;
        engine.controlAudioRateMod_ = GhostarEngine::AudioRateMod {};
        engine.renderVoiceSample();
        return engine.lastOscBWave_;
    }

    static double oscillatorWaveSwitchAtWrapVoltage() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.oscBWaveform = Waveform::RectThin;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        constexpr double internalRate = 4.0 * 48000.0;
        const double bStep = 440.0 / internalRate;
        engine.phaseB_ = 1.0 - 0.1 * bStep;
        engine.heldWaveB_ = 2.0 * engine.phaseB_ - 1.0;
        engine.heldWaveformB_ = Waveform::Sawtooth;
        engine.heldDutyB_ = 0.5;
        engine.oscBDuty_ = 0.5;
        engine.controlOscBOctaves_ = 0.0;
        engine.controlOscBDrone_ = false;
        engine.controlAudioRateMod_ = GhostarEngine::AudioRateMod {};
        engine.renderVoiceSample();
        return engine.lastOscBWave_;
    }

    static double oscillatorSwitchToPulseHeldVoltage() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.oscBWaveform = Waveform::RectThin;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.phaseB_ = 0.25;
        engine.heldWaveB_ = 0.0;
        engine.heldWaveformB_ = Waveform::Triangle;
        engine.heldDutyB_ = 0.5;
        engine.oscBDuty_ = 0.03;
        engine.controlOscBOctaves_ = 0.0;
        engine.controlOscBDrone_ = false;
        engine.controlAudioRateMod_ = GhostarEngine::AudioRateMod {};
        engine.renderVoiceSample();
        return engine.lastOscBWave_;
    }

    static std::array<double, 2> pulseDutyEndpointRange(
        double duty) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.oscBWaveform = Waveform::RectThin;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.phaseB_ = 0.123;
        engine.heldWaveB_ = duty == 0.0 ? -1.0 : 1.0;
        engine.heldWaveformB_ = Waveform::RectThin;
        engine.heldDutyB_ = duty;
        engine.oscBDuty_ = duty;
        engine.controlPwmB_ = 0.0;
        engine.controlOscBOctaves_ = 0.0;
        engine.controlOscBDrone_ = false;
        engine.controlAudioRateMod_ = GhostarEngine::AudioRateMod {};

        double minimum = 1.0e9;
        double maximum = -1.0e9;
        for (int sample = 0; sample < 2048; ++sample)
        {
            engine.renderVoiceSample();
            minimum = std::min(minimum, engine.lastOscBWave_);
            maximum = std::max(maximum, engine.lastOscBWave_);
        }
        return { minimum, maximum };
    }

    static double bPhaseAcrossASawWrap(Waveform selectedAWaveform,
                                       double aDuty,
                                       bool sync) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.oscAWaveform = selectedAWaveform;
        parameters.oscBWaveform = Waveform::Triangle;
        parameters.sync = sync;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;

        constexpr double internalRate = 4.0 * 48000.0;
        constexpr double step = 440.0 / internalRate;
        engine.phaseA_ = 1.0 - 0.4 * step;
        engine.phaseB_ = 0.37;
        engine.heldWaveformA_ = selectedAWaveform;
        engine.heldWaveformB_ = Waveform::Triangle;
        engine.heldDutyA_ = aDuty;
        engine.oscADuty_ = aDuty;
        engine.controlPwmA_ = 0.0;
        engine.controlOscAOctaves_ = 0.0;
        engine.controlOscBOctaves_ = 0.0;
        engine.controlOscBDrone_ = false;
        engine.controlAudioRateMod_ = GhostarEngine::AudioRateMod {};
        engine.renderVoiceSample();
        return engine.phaseB_;
    }

    static HighQProbeConfig highQConfig() noexcept
    {
        GhostarEngine engine;
        return { engine.lowerHighQ_.amplifierGain,
                 engine.lowerHighQ_.sourceResistanceOhms,
                 engine.upperHighQ_.amplifierGain,
                 engine.upperHighQ_.sourceResistanceOhms };
    }

    static HighQProbeResult highQStep(double bandpassCompanion,
                                      double lowpassCompanion,
                                      double chargeCompanion,
                                      double input, double g, double k,
                                      double gain, double resistance,
                                      double chargeStep) noexcept
    {
        GhostarEngine::SvfSection section {
            bandpassCompanion, lowpassCompanion
        };
        GhostarEngine::HighQBranch highQ {
            chargeCompanion, gain, resistance
        };
        const auto output = GhostarEngine::runSection(
            section, input, g, k, &highQ, chargeStep);
        return { output.lp, output.bp, output.hp,
                 section.ic1, section.ic2, highQ.chargeCompanion };
    }

    static LowerMnaProbeResult lowerMnaStep(
        const std::array<double, 3>& sourceTops,
        const std::array<double, 3>& travels,
        const std::array<double, 3>& mixerCompanions,
        double bandpassCompanion, double lowpassCompanion,
        double highQCompanion, double dryInput, double g, double k,
        double hostRate) noexcept
    {
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        engine.lowerSection_ = { bandpassCompanion, lowpassCompanion };
        engine.lowerMixerCompanions_ = mixerCompanions;
        engine.lowerHighQ_.chargeCompanion = highQCompanion;
        const auto output = engine.runLowerSection(
            sourceTops, travels, dryInput, g, k);
        return { output.lp, output.bp, output.hp,
                 engine.lowerMixerCompanions_, engine.lowerSection_.ic1,
                 engine.lowerSection_.ic2,
                 engine.lowerHighQ_.chargeCompanion,
                 engine.highQChargeStep_ };
    }

    static UpperCascadeProbeResult upperCascadeStep(
        UpperSlope slope, const std::array<double, 4>& companions,
        const std::array<double, 2>& lowpassEndpoints,
        double highQCompanion, double input, double g,
        double controlledK, double controlledInputGain,
        double hostRate) noexcept
    {
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        engine.upperControlled_ = { companions[0], companions[1] };
        engine.upperFixed_ = { companions[2], companions[3] };
        engine.upperControlledLp_ = lowpassEndpoints[0];
        engine.upperFixedLp_ = lowpassEndpoints[1];
        engine.upperSlopeState_ = slope;
        engine.upperHighQ_.chargeCompanion = highQCompanion;
        const double output = engine.runUpperCascade(
            input, g, controlledK, controlledInputGain, slope);
        return {
            output,
            engine.upperControlledLp_,
            0.5 * (companions[0] + engine.upperControlled_.ic1),
            engine.upperFixedLp_,
            0.5 * (companions[2] + engine.upperFixed_.ic1),
            { engine.upperControlled_.ic1, engine.upperControlled_.ic2,
              engine.upperFixed_.ic1, engine.upperFixed_.ic2 },
            engine.upperHighQ_.chargeCompanion,
            engine.highQChargeStep_
        };
    }

    static UpperSlopeProjectionProbeResult upperSlopeProjection(
        UpperSlope oldSlope, UpperSlope newSlope,
        double controlledLowpass, double fixedLowpass,
        double controlledCompanion, double fixedCompanion,
        double highQCompanion) noexcept
    {
        GhostarEngine engine;
        engine.upperSlopeState_ = oldSlope;
        engine.upperControlledLp_ = controlledLowpass;
        engine.upperFixedLp_ = fixedLowpass;
        engine.upperControlled_.ic2 = controlledCompanion;
        engine.upperFixed_.ic2 = fixedCompanion;
        engine.upperHighQ_.chargeCompanion = highQCompanion;
        engine.selectUpperSlope(newSlope);
        return { engine.upperControlledLp_, engine.upperFixedLp_,
                 engine.upperControlled_.ic2, engine.upperFixed_.ic2,
                 engine.upperHighQ_.chargeCompanion };
    }

    static std::array<double, 2> upperControl(
        UpperResonanceMode mode, float resonance) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.upperResonance = mode;
        parameters.resonance = resonance;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.advanceControls();
        return { engine.controlUpperK_, engine.controlUpperInputGain_ };
    }

    static OverdriveProbeResult overdriveStep(double lowerLowpass,
                                               double couplingCompanion,
                                               double hostRate) noexcept
    {
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        engine.overdriveCouplingCompanion_ = couplingCompanion;
        const double output = engine.processOverdrive(lowerLowpass);
        return { output, engine.overdriveCouplingCompanion_ };
    }

    static ShaperCycleProbeResult shaperFreeCycle(float shape) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);

        EngineParameters parameters;
        parameters.shaperMode = ShaperMode::Free;
        parameters.shaperShape = shape;
        parameters.shaperRate = 0.5f;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.shaperLevel_ = -1.0;
        engine.shaperRising_ = true;
        engine.shaperGate_ = true;

        int riseSamples = 0;
        int gateHighSamples = 0;
        while (engine.shaperRising_ && riseSamples < 100000)
        {
            gateHighSamples += engine.shaperGate_ ? 1 : 0;
            engine.advanceControls();
            ++riseSamples;
        }

        int fallSamples = 0;
        while (!engine.shaperRising_ && fallSamples < 100000)
        {
            gateHighSamples += engine.shaperGate_ ? 1 : 0;
            engine.advanceControls();
            ++fallSamples;
        }
        return { riseSamples, fallSamples, gateHighSamples };
    }

    static ShaperPhaseProbeResult shaperEnvelopePhaseStep(
        ShaperMode mode, double level, bool rising, bool active,
        bool keyboardGate) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);

        EngineParameters parameters;
        parameters.shaperMode = mode;
        parameters.shaperRate = 0.0f;
        parameters.gateKbd = true;
        parameters.gateX = false;
        parameters.gateYExt = false;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.shaperLevel_ = level;
        engine.shaperRising_ = rising;
        engine.shaperCycleActive_ = active;
        engine.keyGate_ = keyboardGate;
        engine.previousGateForShaper_ = keyboardGate;

        engine.advanceControls();
        return { engine.shaperLevel_, engine.shaperRising_,
                 engine.shaperCycleActive_, engine.shaperGate_ };
    }

    static PassiveCompanionProbeResult brightnessStep(
        double input, double companion, double rheostatOhms,
        double hostRate) noexcept
    {
        const auto result = outputNetworkStep(
            0.0, input, companion, rheostatOhms, 1.0, true, hostRate);
        return { result.shaperWiper, result.companion };
    }

    static OutputNetworkProbeResult outputNetworkStep(
        double filterInput, double shaperInput, double companion,
        double rheostatOhms, double masterTravel, bool split,
        double hostRate) noexcept
    {
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        engine.brightnessCompanion_ = companion;
        engine.controlBrightnessResistanceOhms_ = rheostatOhms;
        const auto output = engine.processOutputNetwork(
            filterInput, shaperInput, masterTravel, split);
        return { output.filter, output.shaper, output.shaperTop,
                 engine.brightnessCompanion_ };
    }

    static PassiveCompanionProbeResult filterCouplingStep(
        double input, double companion, double hostRate) noexcept
    {
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        engine.filterCouplingCompanion_ = companion;
        const double output = engine.processFilterCoupling(input);
        return { output, engine.filterCouplingCompanion_ };
    }

    static PassiveCompanionProbeResult ringStep(
        double triangleA, double triangleB, double companion,
        double hostRate) noexcept
    {
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        engine.ringCouplingCompanion_ = companion;
        const double output =
            engine.processRingModulator(triangleA, triangleB);
        return { output, engine.ringCouplingCompanion_ };
    }

    static double brightnessResistance(float travel) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.brightness = travel;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.advanceControls();
        return engine.controlBrightnessResistanceOhms_;
    }

    static double upperCutoffForNote(int note, float kbAmount) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.cutoff = 0.5f;
        parameters.kbAmount = kbAmount;
        parameters.filterEnvAmount = 0.5f;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.noteOn(note, 1.0f);
        engine.advanceControls();
        return engine.controlUpperCutoffHz_;
    }

    static double oscillatorATuneOctaves(float travel) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.tune = travel;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.currentNote_ = 69;
        engine.glidedNote_ = 69.0;
        engine.glideInitialised_ = true;
        engine.advanceControls();
        return engine.controlOscAOctaves_;
    }

    static std::array<double, 2> filterCutoffsForPanel(
        float master, float lowerOnly, TrackingMode tracking) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.cutoff = master;
        parameters.lowerOnly = lowerOnly;
        parameters.kbAmount = 0.0f;
        parameters.filterEnvAmount = 0.5f;
        parameters.tracking = tracking;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.advanceControls();
        return { engine.controlUpperCutoffHz_,
                 engine.controlLowerCutoffHz_ };
    }

    static std::array<double, 2> filterCutoffsForFilterEnvelope(
        double envelopeLevel, float amount,
        TrackingMode tracking = TrackingMode::Dynamic) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.cutoff = 0.5f;
        parameters.kbAmount = 0.0f;
        parameters.filterEnvAmount = amount;
        parameters.tracking = tracking;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.filterEnvelope_.level = envelopeLevel;
        engine.advanceControls();
        return { engine.controlUpperCutoffHz_,
                 engine.controlLowerCutoffHz_ };
    }

    static double upperLowDamping() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.upperResonance = UpperResonanceMode::Low;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.advanceControls();
        return engine.controlUpperK_;
    }

    static double fullGlideAfterOneSample() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.glide = 1.0f;
        parameters.glideMode = GlideMode::On;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.glidedNote_ = 48.0;
        engine.glideInitialised_ = true;
        engine.currentNote_ = 60;
        engine.keyStack_[0] = 60;
        engine.keyStackSize_ = 1;
        engine.advanceControls();
        return engine.glidedNote_;
    }

    static double lfoHzForTravel(float travel) noexcept
    {
        constexpr double hostRate = 48000.0;
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        EngineParameters parameters;
        parameters.lfoRate = travel;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.lfoCapLevel_ = -1.0;
        engine.lfoRising_ = true;
        engine.advanceControls();
        return 0.25 * (engine.lfoCapLevel_ + 1.0) * hostRate;
    }

    static LfoResetProbeResult keyboardLfoResetFrom(
        double capacitorLevel, bool rising,
        double hostRate = 40000.0) noexcept
    {
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        EngineParameters parameters;
        parameters.trigger = TriggerMode::Single;
        parameters.gateKbd = false;
        parameters.gateX = false;
        parameters.gateYExt = false;
        parameters.arpeggiator = ArpeggiatorMode::Off;
        parameters.modSource = ModSource::SampleHoldY;
        parameters.lfoRate = 1.0f;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.lfoCapLevel_ = capacitorLevel;
        engine.lfoRising_ = rising;
        engine.lfoSquareHigh_ = rising;
        engine.previousLfoSquareHigh_ = rising;
        engine.sampleHoldValue_ = 0.11;
        engine.shaperLevel_ = 0.73;

        engine.noteOn(60, 1.0f);
        int clampedSamples = 0;
        double sampleHoldAfterFirstStep = 0.0;
        do
        {
            engine.advanceControls();
            ++clampedSamples;
            if (clampedSamples == 1)
                sampleHoldAfterFirstStep = engine.sampleHoldValue_;
        }
        while (engine.lfoKtSecondsRemaining_ > 0.0);

        const double capacitorAfterClamp = engine.lfoCapLevel_;
        const double visibleDuringClamp = engine.lastLfoTriangle_;
        engine.advanceControls();
        return {
            clampedSamples,
            capacitorAfterClamp,
            visibleDuringClamp,
            engine.lfoCapLevel_,
            engine.lastLfoTriangle_,
            sampleHoldAfterFirstStep,
            engine.lfoRising_,
        };
    }

    static LfoResetProbeResult keyboardLfoResetWithArpeggiator() noexcept
    {
        constexpr double hostRate = 40000.0;
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        EngineParameters parameters;
        parameters.trigger = TriggerMode::Single;
        parameters.gateKbd = false;
        parameters.gateX = false;
        parameters.gateYExt = false;
        parameters.arpeggiator = ArpeggiatorMode::Ripple;
        parameters.modSource = ModSource::SampleHoldY;
        parameters.lfoRate = 1.0f;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.lfoCapLevel_ = 0.2;
        engine.lfoRising_ = false;
        engine.lfoSquareHigh_ = false;
        engine.previousLfoSquareHigh_ = false;
        engine.sampleHoldValue_ = 0.11;
        engine.shaperLevel_ = 0.73;

        engine.noteOn(60, 1.0f);
        engine.advanceControls();
        return {
            engine.lastLfoTriangle_ == -1.0 ? 1 : 0,
            engine.lfoCapLevel_,
            engine.lastLfoTriangle_,
            engine.lfoCapLevel_,
            engine.lastLfoTriangle_,
            engine.sampleHoldValue_,
            engine.lfoRising_,
        };
    }

    static double integratedLfoKtDuration(double sampleRate) noexcept
    {
        double ktSecondsRemaining = 25.0e-6;
        double integrated = 0.0;
        const double interval = 1.0 / sampleRate;
        while (ktSecondsRemaining > 0.0)
            integrated += GhostarEngine::consumeLfoKtDuration(
                ktSecondsRemaining, interval);
        return integrated;
    }

    static LfoGateXProbeResult subSampleLfoResetThroughGateX(
        double hostRate,
        ShaperMode shaperMode = ShaperMode::Reset) noexcept
    {
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        EngineParameters parameters;
        parameters.gateKbd = false;
        parameters.gateX = true;
        parameters.gateYExt = false;
        parameters.trigger = TriggerMode::Single;
        parameters.arpeggiator = ArpeggiatorMode::Off;
        parameters.shaperMode = shaperMode;
        parameters.shaperRate = 1.0f;
        parameters.shaperShape = 0.0f;
        parameters.lfoRate = 1.0f;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;

        // KT first forces the falling LG leg high, but the retained C13
        // charge crosses +5 V and returns LG low before this host interval
        // ends. The edge must still reach both physical Gate-X consumers.
        engine.lfoCapLevel_ = 0.997;
        engine.lfoRising_ = false;
        engine.lfoSquareHigh_ = false;
        engine.previousLfoSquareHigh_ = false;
        engine.previousGateForShaper_ = false;
        engine.previousEnvelopeXGate_ = false;
        engine.loudnessEnvelope_.stage = GhostarEngine::Adsr::Stage::Decay;
        engine.loudnessEnvelope_.level = 0.8;
        engine.pendingLfoReset_ = true;

        engine.advanceControls();
        return {
            1 + static_cast<int>(engine.envelopeResetSamplesRemaining_),
            engine.lfoSquareHigh_,
            engine.shaperCycleActive_,
            engine.shaperLevel_,
        };
    }

    static ModulationProbeResult modulationAt(
        ModXDestination xDestination, ShaperYDestination yDestination,
        float xTravel, float yTravel, bool audioRateX = false,
        bool dynamicTracking = true) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.modSource = audioRateX
            ? ModSource::OscB : ModSource::LfoSquare;
        parameters.modXTo = xDestination;
        parameters.shaperYTo = yDestination;
        parameters.shaperMode = ShaperMode::KbdHold;
        parameters.cutoff = 0.5f;
        parameters.lowerOnly = 0.8f;
        parameters.kbAmount = 0.0f;
        parameters.filterEnvAmount = 0.5f;
        parameters.tracking = dynamicTracking
            ? TrackingMode::Dynamic : TrackingMode::Formant;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.currentNote_ = 69;
        engine.glidedNote_ = 69.0;
        engine.glideInitialised_ = true;
        engine.keyGate_ = true;
        engine.previousGateForShaper_ = true;
        engine.shaperLevel_ = 1.0;
        engine.shaperRising_ = false;
        engine.modWheel_ = 0.0;
        engine.targetModWheel_ = 0.0;
        engine.shaperWheel_ = 0.0;
        engine.targetShaperWheel_ = 0.0;
        engine.lfoCapLevel_ = -1.0;
        engine.lfoRising_ = true;

        engine.advanceControls();
        const double baseUpperCutoff = engine.controlUpperCutoffHz_;
        const double baseLowerCutoff = engine.controlLowerCutoffHz_;
        engine.modWheel_ = xTravel;
        engine.targetModWheel_ = xTravel;
        engine.shaperWheel_ = yTravel;
        engine.targetShaperWheel_ = yTravel;
        engine.lfoCapLevel_ = -1.0;
        engine.lfoRising_ = true;
        engine.advanceControls();
        const auto& audio = engine.controlAudioRateMod_;
        return {
            engine.controlOscAOctaves_,
            engine.controlOscBOctaves_,
            std::log2(engine.controlUpperCutoffHz_ / baseUpperCutoff),
            std::log2(engine.controlLowerCutoffHz_ / baseLowerCutoff),
            engine.controlPwmA_,
            engine.controlPwmB_,
            audio.gain,
            audio.aOctaves,
            audio.bOctaves,
            audio.upperOctaves,
            audio.lowerOctaves,
            audio.duty,
            audio.active,
        };
    }

    static ModSourceTickProbeResult modSourceTicks(
        ModSource source, ModXDestination destination) noexcept
    {
        constexpr double hostRate = 48000.0;
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        EngineParameters parameters;
        parameters.modSource = source;
        parameters.modXTo = destination;
        parameters.shaperYTo = ShaperYDestination::Off;
        parameters.filterEnvAmount = 0.5f;
        parameters.kbAmount = 0.0f;
        parameters.tracking = TrackingMode::Dynamic;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.currentNote_ = 69;
        engine.glidedNote_ = 69.0;
        engine.glideInitialised_ = true;
        engine.modWheel_ = 1.0f;
        engine.targetModWheel_ = 1.0f;
        engine.shaperLevel_ = 1.0;
        engine.lfoCapLevel_ = 0.0;
        engine.lfoRising_ = true;
        engine.lfoSquareHigh_ = true;
        engine.previousLfoSquareHigh_ = true;
        engine.sampleHoldValue_ = 0.375;

        // Keep an independent copy of the one physical MM5837 circuit. If the
        // audio and RED taps ever advance separate generators (or one advances
        // twice per internal tick), these comparisons diverge immediately.
        SpiritNoise reference;
        reference.prepare(4.0 * hostRate);
        for (int tick = 0; tick < 4096; ++tick)
        {
            (void) engine.noise_.process();
            (void) reference.process();
        }

        engine.advanceControls();
        ModSourceTickProbeResult result;
        result.audioActive = engine.controlAudioRateMod_.active;
        result.sourceMatches = engine.controlAudioRateMod_.source == source;
        result.basePitchA = engine.controlOscAOctaves_;
        result.basePitchB = engine.controlOscBOctaves_;
        result.baseDutyA = engine.oscADuty_ + engine.controlPwmA_;
        for (std::size_t tick = 0; tick < result.referenceRed.size(); ++tick)
        {
            const double expectedAudio = reference.process();
            result.referenceRed[tick] = reference.red();
            engine.renderVoiceSample();
            const std::size_t frameIndex =
                (static_cast<std::size_t>(engine.preMixerDelayIndex_)
                 + engine.preMixerDelay_.size() - 1u)
                % engine.preMixerDelay_.size();
            const auto& frame = engine.preMixerDelay_[frameIndex];
            result.sharedAudioError = std::max(
                result.sharedAudioError,
                std::abs(frame.pinkNoise - expectedAudio));
            result.sharedRedError = std::max(
                result.sharedRedError,
                std::abs(engine.noise_.red() - result.referenceRed[tick]));
            result.pitchA[tick] = engine.pitchLagA_.previousInput;
            result.pitchB[tick] = engine.pitchLagB_.previousInput;
            result.dutyA[tick] = engine.heldDutyA_;
            result.upperFilter[tick] = frame.audioModUpper;
            result.lowerFilter[tick] = frame.audioModLower;
        }
        return result;
    }

    static SampleHoldRedEdgeProbeResult sampleHoldRedClockEdge() noexcept
    {
        constexpr double hostRate = 48000.0;
        GhostarEngine engine;
        engine.prepare(hostRate, 64);
        EngineParameters parameters;
        parameters.modSource = ModSource::SampleHoldRandom;
        parameters.modXTo = ModXDestination::OscAB;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.modWheel_ = engine.targetModWheel_ = 1.0f;

        SpiritNoise reference;
        reference.prepare(4.0 * hostRate);
        for (int tick = 0; tick < 4096; ++tick)
        {
            (void) engine.noise_.process();
            (void) reference.process();
        }

        SampleHoldRedEdgeProbeResult result;
        result.availableEngineRed = engine.noise_.red();
        result.availableReferenceRed = reference.red();
        engine.sampleHoldValue_ = -0.875;
        engine.lfoCapLevel_ = 0.0;
        engine.lfoRising_ = true;
        engine.lfoSquareHigh_ = false;
        engine.previousLfoSquareHigh_ = false;
        engine.advanceControls();
        result.captured = engine.sampleHoldValue_;

        for (std::size_t tick = 0; tick < result.held.size(); ++tick)
        {
            (void) reference.process();
            engine.renderVoiceSample();
            result.laterEngineRed[tick] = engine.noise_.red();
            result.laterReferenceRed[tick] = reference.red();
            result.held[tick] = engine.sampleHoldValue_;
        }
        return result;
    }

    static double loudnessGainForEnvelope(double level,
                                          bool bypass = false) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.vcaBypass = bypass;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.loudnessEnvelope_.level = level;
        engine.advanceControls();
        return engine.controlLoudnessGain_;
    }

    static double envelopeDecayTarget(double sustain) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        GhostarEngine::Adsr envelope;
        envelope.stage = GhostarEngine::Adsr::Stage::Decay;
        envelope.level = 1.0;
        engine.advanceEnvelope(envelope, true, false,
                               0.0, 1.0, 1.0, 2.0e6 + 100.0, sustain);
        return envelope.level;
    }

    static double envelopeReleaseStep(double level,
                                      double resistanceOhms,
                                      double sampleRate) noexcept
    {
        GhostarEngine engine;
        engine.prepare(sampleRate, 64);
        GhostarEngine::Adsr envelope;
        envelope.stage = GhostarEngine::Adsr::Stage::Release;
        envelope.level = level;
        engine.advanceEnvelope(envelope, false, false,
                               0.0, 1.0, 0.0, resistanceOhms, 0.0);
        return envelope.level;
    }

    static double envelopeReleaseSecondsTo(double targetLevel,
                                           double resistanceOhms,
                                           double sampleRate) noexcept
    {
        GhostarEngine engine;
        engine.prepare(sampleRate, 64);
        GhostarEngine::Adsr envelope;
        envelope.stage = GhostarEngine::Adsr::Stage::Release;
        envelope.level = 1.0;
        std::uint64_t samples = 0;
        const auto limit = static_cast<std::uint64_t>(40.0 * sampleRate);
        while (envelope.level > targetLevel && samples < limit)
        {
            engine.advanceEnvelope(envelope, false, false,
                                   0.0, 1.0, 0.0, resistanceOhms, 0.0);
            ++samples;
        }
        return static_cast<double>(samples) / sampleRate;
    }

    static EnvelopeResetProbeResult envelopeMultipleResetNotch() noexcept
    {
        constexpr double sampleRate = 8000.0;
        GhostarEngine engine;
        engine.prepare(sampleRate, 64);
        EngineParameters parameters;
        parameters.gateKbd = true;
        parameters.gateX = false;
        parameters.gateYExt = false;
        parameters.trigger = TriggerMode::Multiple;
        parameters.loudnessAttack = 0.5f;
        parameters.loudnessRelease = 0.5f;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;

        engine.keyGate_ = true;
        engine.envelopeGate_ = true;
        engine.previousEnvelopeGs_ = true;
        engine.pendingTrigger_ = true;
        engine.loudnessEnvelope_.stage = GhostarEngine::Adsr::Stage::Decay;
        engine.loudnessEnvelope_.level = 0.8;

        const double before = engine.loudnessEnvelope_.level;
        int releaseSamples = 0;
        do
        {
            engine.advanceControls();
            if (engine.loudnessEnvelope_.stage
                == GhostarEngine::Adsr::Stage::Release)
                ++releaseSamples;
        }
        while (engine.envelopeResetSamplesRemaining_ != 0);

        const double afterNotch = engine.loudnessEnvelope_.level;
        const bool stayedInRelease = engine.loudnessEnvelope_.stage
            == GhostarEngine::Adsr::Stage::Release;
        engine.advanceControls();
        return {
            releaseSamples,
            before,
            afterNotch,
            engine.loudnessEnvelope_.level,
            stayedInRelease,
            engine.loudnessEnvelope_.stage
                == GhostarEngine::Adsr::Stage::Attack,
        };
    }

    static int xEdgeResetSamplesUnderHeldKeyboardGate() noexcept
    {
        GhostarEngine engine;
        engine.prepare(8000.0, 64);
        EngineParameters parameters;
        parameters.gateKbd = true;
        parameters.gateX = true;
        parameters.gateYExt = false;
        parameters.trigger = TriggerMode::Single;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;

        engine.keyGate_ = true;
        engine.envelopeGate_ = true;
        engine.previousEnvelopeGs_ = true;
        engine.previousEnvelopeXGate_ = false;
        engine.lfoCapLevel_ = -1.0;
        engine.lfoRising_ = true;
        engine.previousLfoSquareHigh_ = false;
        engine.loudnessEnvelope_.stage = GhostarEngine::Adsr::Stage::Decay;
        engine.loudnessEnvelope_.level = 0.8;

        int releaseSamples = 0;
        do
        {
            engine.advanceControls();
            if (engine.loudnessEnvelope_.stage
                == GhostarEngine::Adsr::Stage::Release)
                ++releaseSamples;
        }
        while (engine.envelopeResetSamplesRemaining_ != 0);
        return releaseSamples;
    }

    static ExternalGateProbeResult externalGateWithInternalY(
        bool jackInserted, double volts) noexcept
    {
        GhostarEngine engine;
        engine.prepare(8000.0, 64);
        EngineParameters parameters;
        parameters.gateKbd = false;
        parameters.gateX = false;
        parameters.gateYExt = true;
        parameters.shaperMode = ShaperMode::Free;
        parameters.shaperRate = 0.0f;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;

        engine.shaperLevel_ = 0.0;
        engine.shaperRising_ = true;
        engine.shaperGate_ = true;
        engine.setExternalGateInput(jackInserted, volts);
        engine.advanceControls();
        return { engine.shaperGate_, engine.envelopeGate_ };
    }

    static ExternalGateFallProbeResult externalGateFallOverInternalY() noexcept
    {
        GhostarEngine engine;
        engine.prepare(8000.0, 64);
        EngineParameters parameters;
        parameters.gateKbd = false;
        parameters.gateX = false;
        parameters.gateYExt = true;
        parameters.shaperMode = ShaperMode::Free;
        parameters.shaperRate = 0.0f;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;

        engine.shaperLevel_ = 0.0;
        engine.shaperRising_ = true;
        engine.shaperGate_ = true;
        engine.previousEnvelopeYGate_ = true;
        engine.setExternalGateInput(true, 10.0);
        engine.advanceControls();
        const bool openBeforeFall = engine.envelopeGate_;

        engine.setExternalGateInput(true, 0.0);
        engine.advanceControls();
        return { openBeforeFall, engine.shaperGate_, engine.envelopeGate_ };
    }

    static int externalGateEdgeResetSamplesUnderHeldKeyboard() noexcept
    {
        GhostarEngine engine;
        engine.prepare(8000.0, 64);
        EngineParameters parameters;
        parameters.gateKbd = true;
        parameters.gateX = false;
        parameters.gateYExt = true;
        parameters.trigger = TriggerMode::Single;
        parameters.shaperMode = ShaperMode::Free;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;

        engine.keyGate_ = true;
        engine.envelopeGate_ = true;
        engine.previousEnvelopeGs_ = true;
        engine.previousEnvelopeYGate_ = false;
        engine.loudnessEnvelope_.stage = GhostarEngine::Adsr::Stage::Decay;
        engine.loudnessEnvelope_.level = 0.8;
        engine.setExternalGateInput(true, 0.0);
        engine.setExternalGateInput(true, 10.0);

        int releaseSamples = 0;
        do
        {
            engine.advanceControls();
            if (engine.loudnessEnvelope_.stage
                == GhostarEngine::Adsr::Stage::Release)
                ++releaseSamples;
        }
        while (engine.envelopeResetSamplesRemaining_ != 0);
        return releaseSamples;
    }

    static ExternalGateHardStopProbeResult externalGateAcrossHardStop(
        bool allSoundOff) noexcept
    {
        GhostarEngine engine;
        engine.prepare(8000.0, 64);
        EngineParameters parameters;
        parameters.gateKbd = false;
        parameters.gateX = false;
        parameters.gateYExt = true;
        parameters.trigger = TriggerMode::Single;
        parameters.shaperMode = ShaperMode::Run;
        parameters.shaperRate = 1.0f;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;

        // Establish HIGH as an already-observed physical level before the
        // hard stop, including completion of its legitimate reset notch.
        engine.setExternalGateInput(true, 10.0);
        engine.advanceControls();
        while (engine.envelopeResetSamplesRemaining_ != 0)
            engine.advanceControls();
        engine.advanceControls();

        if (allSoundOff)
            engine.stopAllSound();
        else
            engine.reset();

        bool restartedResetWhileHeldHigh = false;
        bool attackedWhileHeldHigh = false;
        bool restartedShaperWhileHeldHigh = false;
        for (int sample = 0; sample < 64; ++sample)
        {
            engine.advanceControls();
            restartedResetWhileHeldHigh |=
                engine.envelopeResetSamplesRemaining_ != 0;
            attackedWhileHeldHigh |= engine.loudnessEnvelope_.stage
                != GhostarEngine::Adsr::Stage::Idle;
            restartedShaperWhileHeldHigh |= engine.shaperCycleActive_;
        }

        engine.setExternalGateInput(true, 0.0);
        engine.advanceControls();
        engine.setExternalGateInput(true, 10.0);

        int resetSamplesAfterRealEdge = 0;
        do
        {
            engine.advanceControls();
            ++resetSamplesAfterRealEdge;
        }
        while (engine.envelopeResetSamplesRemaining_ != 0
               && resetSamplesAfterRealEdge < 100);
        engine.advanceControls();

        return {
            restartedResetWhileHeldHigh,
            attackedWhileHeldHigh,
            restartedShaperWhileHeldHigh,
            resetSamplesAfterRealEdge,
            engine.loudnessEnvelope_.stage
                == GhostarEngine::Adsr::Stage::Attack,
            engine.shaperCycleActive_,
        };
    }

    static ExternalAudioTimingProbeResult externalAudioTiming() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);

        constexpr std::size_t responseSamples =
            GhostarEngine::stageATaps
            + 2 * (GhostarEngine::stageBTaps - 1);
        std::array<double, responseSamples> actual {};
        for (std::size_t tick = 0; tick < actual.size(); ++tick)
        {
            const int internalStep = static_cast<int>(tick % 4);
            const double hostImpulse = tick / 4 == 0 ? 1.0 : 0.0;
            actual[tick] = engine.reconstructExternalAudio(
                hostImpulse, internalStep);
        }

        // Independent dense oracle: reverse B -> A interpolation is
        // 4 * (hA convolved with hB expanded by one zero between taps).
        // Rebuilding the dense kernels from the sparse storage keeps this
        // check independent of both production ring schedules.
        std::array<double, GhostarEngine::stageATaps> stageA {};
        std::array<double, GhostarEngine::stageBTaps> stageB {};
        for (int stored = 0; stored < engine.stageAKernel_.count; ++stored)
        {
            const int tap = GhostarEngine::stageATaps - 1
                - engine.stageAKernel_.offsets[
                    static_cast<std::size_t>(stored)];
            stageA[static_cast<std::size_t>(tap)] =
                engine.stageAKernel_.values[static_cast<std::size_t>(stored)];
        }
        for (int stored = 0; stored < engine.stageBKernel_.count; ++stored)
        {
            const int tap = GhostarEngine::stageBTaps - 1
                - engine.stageBKernel_.offsets[
                    static_cast<std::size_t>(stored)];
            stageB[static_cast<std::size_t>(tap)] =
                engine.stageBKernel_.values[static_cast<std::size_t>(stored)];
        }

        std::array<double, responseSamples> expected {};
        for (std::size_t a = 0; a < stageA.size(); ++a)
            for (std::size_t b = 0; b < stageB.size(); ++b)
                expected[a + 2 * b] += 4.0 * stageA[a] * stageB[b];

        double oracleError = 0.0;
        double sum = 0.0;
        double firstMoment = 0.0;
        double symmetryError = 0.0;
        for (std::size_t tick = 0; tick < actual.size(); ++tick)
        {
            oracleError = std::max(
                oracleError, std::abs(actual[tick] - expected[tick]));
            sum += actual[tick];
            firstMoment += static_cast<double>(tick) * actual[tick];
            symmetryError = std::max(
                symmetryError,
                std::abs(actual[tick] - actual[actual.size() - 1 - tick]));
        }

        // Independent fixed-frequency contract. The 4x interpolation impulse
        // sums to four, so divide its DFT by four for unity signal gain. The
        // 0.45/0.55 host-rate pair brackets stage B's sharp transition; the
        // halfband-complementary 1.55 point exercises stage A's second-image
        // stopband without rebuilding either production kernel.
        const auto responseMagnitude = [&actual](double hostCycles) {
            std::complex<double> response {};
            constexpr double twoPi = 6.28318530717958647692;
            for (std::size_t tick = 0; tick < actual.size(); ++tick)
            {
                const double angle = -twoPi * hostCycles
                                   * static_cast<double>(tick) / 4.0;
                response += actual[tick]
                          * std::complex<double> { std::cos(angle),
                                                   std::sin(angle) };
            }
            return std::abs(response) / 4.0;
        };
        const double passbandMagnitude = responseMagnitude(0.45);
        const double firstImageMagnitude = responseMagnitude(0.55);
        const double secondImageMagnitude = responseMagnitude(1.55);

        GhostarEngine dcEngine;
        dcEngine.prepare(48000.0, 64);
        std::array<double, 4> expectedDc {};
        for (std::size_t tick = 0; tick < expected.size(); ++tick)
            expectedDc[tick % expectedDc.size()] += expected[tick];
        double dcError = 0.0;
        for (int hostSample = 0; hostSample < 256; ++hostSample)
            for (int step = 0; step < 4; ++step)
            {
                const double value =
                    dcEngine.reconstructExternalAudio(1.0, step);
                if (hostSample >= 192)
                    dcError = std::max(
                        dcError,
                        std::abs(value
                                 - expectedDc[static_cast<std::size_t>(step)]));
            }

        // Put a one-tick signal/control marker into the real frame FIFO and
        // observe the first Shaper output it can affect. This is deliberately
        // separate from the reconstructor oracle above: both lanes must land
        // on the same circuit tick.
        GhostarEngine delayed;
        delayed.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.masterVolume = 1.0f;
        parameters.splitPaths = true;
        parameters.filterPathA = 0.0f;
        parameters.filterPathB = 0.0f;
        parameters.filterPathNoise = 0.0f;
        parameters.shaperPathA = 0.0f;
        parameters.shaperPathB = 0.0f;
        parameters.shaperPathRing = 0.0f;
        parameters.shaperPathNoise = 0.0f;
        delayed.parameters_ = parameters;
        delayed.targetParameters_ = parameters;
        delayed.cem3360OutputNoiseScale_ = 0.0;
        delayed.controlShaperMixNoise_ = 1.0;
        delayed.controlShaperVcaGain_ = 1.0;
        delayed.controlBrightnessResistanceOhms_ = 0.0;
        delayed.externalAudioJackInserted_ = true;

        int firstDelayedFrameTick = -1;
        for (int tick = 0;
             tick < GhostarEngine::externalInputLatencyInternalSamples() + 8;
             ++tick)
        {
            delayed.renderVoiceSample(1.0);
            if (firstDelayedFrameTick < 0
                && std::abs(delayed.lastShaperPathSample_) > 1.0e-12)
                firstDelayedFrameTick = tick;
            if (tick == 0)
            {
                delayed.controlShaperMixNoise_ = 0.0;
                delayed.externalAudioJackInserted_ = false;
            }
        }

        const auto prepareMarker = [](GhostarEngine& marker,
                                      double brightnessOhms,
                                      bool jackInserted) {
            marker.prepare(48000.0, 64);
            EngineParameters markerParameters;
            markerParameters.masterVolume = 1.0f;
            markerParameters.splitPaths = true;
            markerParameters.filterPathA = 0.0f;
            markerParameters.filterPathB = 0.0f;
            markerParameters.filterPathNoise = 0.0f;
            markerParameters.shaperPathA = 0.0f;
            markerParameters.shaperPathB = 0.0f;
            markerParameters.shaperPathRing = 0.0f;
            markerParameters.shaperPathNoise = 0.0f;
            marker.parameters_ = markerParameters;
            marker.targetParameters_ = markerParameters;
            marker.cem3360OutputNoiseScale_ = 0.0;
            marker.controlShaperMixNoise_ = 1.0;
            marker.controlShaperVcaGain_ = 1.0;
            marker.controlBrightnessResistanceOhms_ = brightnessOhms;
            marker.externalAudioJackInserted_ = jackInserted;
        };

        // Cable presence belongs to the delayed frame too. Change only the
        // live state after tick zero: the consumed tick-zero frame must match
        // an engine that stayed plugged, while a genuinely unplugged
        // tick-zero counterfactual proves external and pink are distinguishable.
        GhostarEngine switchedJack;
        GhostarEngine heldJack;
        GhostarEngine normalledJack;
        prepareMarker(switchedJack, 0.0, true);
        prepareMarker(heldJack, 0.0, true);
        prepareMarker(normalledJack, 0.0, false);
        double switchedJackSample = 0.0;
        double heldJackSample = 0.0;
        double normalledJackSample = 0.0;
        for (int tick = 0;
             tick <= GhostarEngine::externalInputLatencyInternalSamples();
             ++tick)
        {
            switchedJack.renderVoiceSample(1.0);
            heldJack.renderVoiceSample(1.0);
            normalledJack.renderVoiceSample(1.0);
            if (tick == 0)
            {
                switchedJack.externalAudioJackInserted_ = false;
                normalledJack.externalAudioJackInserted_ = true;
            }
            if (tick == GhostarEngine::externalInputLatencyInternalSamples())
            {
                switchedJackSample = switchedJack.lastShaperPathSample_;
                heldJackSample = heldJack.lastShaperPathSample_;
                normalledJackSample = normalledJack.lastShaperPathSample_;
            }
        }

        // BRIGHTNESS must come from the matching delayed frame, not merely
        // from some delayed frame. Schedule 50k at tick zero and 0R from tick
        // one: the first response must equal a held-50k reference, while the
        // following response must depart from it. A held-0R counterfactual
        // proves the tick-zero comparison is able to discriminate the two.
        // Separately poison only the live resistance at consumption to catch
        // either output or companion-state use of the undelayed control.
        constexpr double capturedBrightnessOhms = 50000.0;
        constexpr double followingBrightnessOhms = 0.0;
        constexpr double livePoisonBrightnessOhms = 100000.0;
        GhostarEngine scheduledBrightness;
        GhostarEngine livePerturbedBrightness;
        GhostarEngine heldCapturedBrightness;
        GhostarEngine heldFollowingBrightness;
        prepareMarker(scheduledBrightness, capturedBrightnessOhms, true);
        prepareMarker(livePerturbedBrightness, capturedBrightnessOhms, true);
        prepareMarker(heldCapturedBrightness, capturedBrightnessOhms, true);
        prepareMarker(heldFollowingBrightness, followingBrightnessOhms, true);
        double brightnessTickZeroAlignmentDifference = 0.0;
        double brightnessTickZeroSelectionDifference = 0.0;
        double brightnessTickOneSelectionDifference = 0.0;
        double brightnessStateDifference = 0.0;
        double brightnessCompanionMagnitude = 0.0;
        for (int tick = 0;
             tick <= GhostarEngine::externalInputLatencyInternalSamples() + 1;
             ++tick)
        {
            if (tick == GhostarEngine::externalInputLatencyInternalSamples())
                livePerturbedBrightness.controlBrightnessResistanceOhms_ =
                    livePoisonBrightnessOhms;
            scheduledBrightness.renderVoiceSample(1.0);
            livePerturbedBrightness.renderVoiceSample(1.0);
            heldCapturedBrightness.renderVoiceSample(1.0);
            heldFollowingBrightness.renderVoiceSample(1.0);
            if (tick == 0)
            {
                scheduledBrightness.controlBrightnessResistanceOhms_ =
                    followingBrightnessOhms;
                livePerturbedBrightness.controlBrightnessResistanceOhms_ =
                    followingBrightnessOhms;
                scheduledBrightness.controlShaperMixNoise_ = 0.0;
                livePerturbedBrightness.controlShaperMixNoise_ = 0.0;
                heldCapturedBrightness.controlShaperMixNoise_ = 0.0;
                heldFollowingBrightness.controlShaperMixNoise_ = 0.0;
            }
            if (tick == GhostarEngine::externalInputLatencyInternalSamples())
            {
                const auto stateDifference = [](const GhostarEngine& a,
                                                const GhostarEngine& b) {
                    return std::max(
                        std::abs(a.lastShaperPathSample_
                                 - b.lastShaperPathSample_),
                        std::abs(a.brightnessCompanion_
                                 - b.brightnessCompanion_));
                };
                brightnessTickZeroAlignmentDifference = stateDifference(
                    scheduledBrightness, heldCapturedBrightness);
                brightnessTickZeroSelectionDifference = stateDifference(
                    heldFollowingBrightness, heldCapturedBrightness);
                brightnessStateDifference = stateDifference(
                    livePerturbedBrightness, scheduledBrightness);
                brightnessCompanionMagnitude =
                    std::abs(heldCapturedBrightness.brightnessCompanion_);
                livePerturbedBrightness.controlBrightnessResistanceOhms_ =
                    followingBrightnessOhms;
            }
            if (tick
                == GhostarEngine::externalInputLatencyInternalSamples() + 1)
            {
                brightnessTickOneSelectionDifference = std::max(
                    std::abs(scheduledBrightness.lastShaperPathSample_
                             - heldCapturedBrightness.lastShaperPathSample_),
                    std::abs(scheduledBrightness.brightnessCompanion_
                             - heldCapturedBrightness.brightnessCompanion_));
                brightnessStateDifference = std::max(
                    brightnessStateDifference,
                    std::max(
                        std::abs(livePerturbedBrightness.lastShaperPathSample_
                                 - scheduledBrightness.lastShaperPathSample_),
                        std::abs(livePerturbedBrightness.brightnessCompanion_
                                 - scheduledBrightness.brightnessCompanion_)));
            }
        }

        return { oracleError, sum, firstMoment / sum, symmetryError,
                 passbandMagnitude, firstImageMagnitude,
                 secondImageMagnitude, dcError, firstDelayedFrameTick,
                 std::abs(switchedJackSample - heldJackSample),
                 std::abs(normalledJackSample - heldJackSample),
                 brightnessTickZeroAlignmentDifference,
                 brightnessTickZeroSelectionDifference,
                 brightnessTickOneSelectionDifference,
                 brightnessStateDifference,
                 brightnessCompanionMagnitude };
    }

    static ExternalAudioMixerProbeResult externalAudioMixerProbe(
        bool jackInserted, double externalAudio, double normalledPink) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        engine.cem3360OutputNoiseScale_ = 0.0;

        GhostarEngine::PreMixerFrame frame;
        frame.pinkNoise = normalledPink;
        frame.upperCutoffHz = 4000.0;
        frame.lowerCutoffHz = 4000.0;
        frame.upperK = 1.0;
        frame.upperInputGain = 2.0;
        frame.lowerK = 1.0;
        frame.loudnessGain = 1.0;
        frame.shaperVcaGain = 1.0;
        frame.brightnessResistanceOhms = 0.0;
        frame.filterMixNoise = 1.0;
        frame.shaperMixNoise = 1.0;
        frame.masterVolume = 1.0f;
        frame.lowerMode = LowerFilterMode::Out;
        frame.slope = UpperSlope::TwelveDb;
        frame.splitPaths = true;
        frame.externalAudioJackInserted = jackInserted;
        engine.preMixerDelay_[0] = frame;

        double filterEnergy = 0.0;
        double shaperEnergy = 0.0;
        double filterSum = 0.0;
        double shaperSum = 0.0;
        for (int tick = 0; tick < 64; ++tick)
        {
            engine.renderVoiceSample(externalAudio);
            filterEnergy += engine.lastFilterPathSample_
                          * engine.lastFilterPathSample_;
            shaperEnergy += engine.lastShaperPathSample_
                          * engine.lastShaperPathSample_;
            filterSum += engine.lastFilterPathSample_;
            shaperSum += engine.lastShaperPathSample_;
        }
        return { filterEnergy, shaperEnergy, filterSum, shaperSum };
    }

    static ExternalAudioProcessProbeResult
    externalAudioProcessImpulse() noexcept
    {
        constexpr int warmupSamples = 256;
        constexpr int responseSamples = 512;
        using Response = std::array<float, responseSamples>;

        const auto render = [](float polarity) {
            GhostarEngine engine;
            engine.prepare(48000.0, responseSamples);
            EngineParameters parameters;
            parameters.masterVolume = 1.0f;
            parameters.brightness = 1.0f;
            parameters.splitPaths = true;
            parameters.filterPathA = 0.0f;
            parameters.filterPathB = 0.0f;
            parameters.filterPathNoise = 0.0f;
            parameters.shaperPathA = 0.0f;
            parameters.shaperPathB = 0.0f;
            parameters.shaperPathRing = 0.0f;
            parameters.shaperPathNoise = 1.0f;
            parameters.shaperMode = ShaperMode::KbdHold;
            parameters.gateKbd = true;
            engine.parameters_ = parameters;
            engine.targetParameters_ = parameters;
            engine.keyGate_ = true;
            engine.shaperLevel_ = 1.0;
            engine.shaperRising_ = false;
            engine.externalAudioJackInserted_ = true;
            engine.cem3360OutputNoiseScale_ = 0.0;
            // Open C18 for this timing probe so the Shaper lane between the
            // two halfband cascades is a memoryless, positive scalar.
            engine.brightnessG_ = 0.0;

            std::array<float, warmupSamples> silence {};
            std::array<float, warmupSamples> warmLeft {};
            std::array<float, warmupSamples> warmRight {};
            engine.process(silence.data(), warmLeft.data(), warmRight.data(),
                           warmupSamples);

            Response input {};
            Response left {};
            Response right {};
            input[0] = polarity;
            engine.process(input.data(), left.data(), right.data(),
                           responseSamples);
            return right;
        };

        const auto positive = render(1.0f);
        const auto negative = render(-1.0f);
        double sum = 0.0;
        double moment = 0.0;
        double polarityError = 0.0;
        for (std::size_t sample = 0; sample < positive.size(); ++sample)
        {
            const double value = static_cast<double>(positive[sample]);
            sum += value;
            moment += static_cast<double>(sample) * value;
            polarityError = std::max(
                polarityError,
                std::abs(value + static_cast<double>(negative[sample])));
        }
        return { sum, moment / sum, polarityError };
    }

    static double externalAudioHistoryMagnitude(float input) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 1);
        float left = 0.0f;
        float right = 0.0f;
        engine.process(&input, &left, &right, 1);

        double magnitude = 0.0;
        for (const double sample : engine.externalStageBRing_)
            magnitude = std::max(magnitude, std::abs(sample));
        for (const double sample : engine.externalStageARing_)
            magnitude = std::max(magnitude, std::abs(sample));
        return magnitude;
    }

    static ExternalAudioResetProbeResult externalAudioResetHistory() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 1);
        float input = 1.0f;
        float left = 0.0f;
        float right = 0.0f;
        engine.process(&input, &left, &right, 1);

        const auto magnitude = [](const auto& ring) {
            double result = 0.0;
            for (const double sample : ring)
                result = std::max(result, std::abs(sample));
            return result;
        };
        const double stageBBefore = magnitude(engine.externalStageBRing_);
        const double stageABefore = magnitude(engine.externalStageARing_);
        engine.reset();
        return { stageBBefore, stageABefore,
                 magnitude(engine.externalStageBRing_),
                 magnitude(engine.externalStageARing_) };
    }

    static ExternalAudioRedNoiseProbeResult
    externalAudioKeepsRedNoiseUpstream() noexcept
    {
        GhostarEngine normalled;
        GhostarEngine inserted;
        normalled.prepare(48000.0, 64);
        inserted.prepare(48000.0, 64);

        EngineParameters parameters;
        parameters.masterVolume = 0.0f;
        parameters.filterPathA = 0.0f;
        parameters.filterPathB = 0.0f;
        parameters.filterPathNoise = 0.0f;
        parameters.modSource = ModSource::RedNoise;
        parameters.modXTo = ModXDestination::FilterUL;
        normalled.parameters_ = parameters;
        normalled.targetParameters_ = parameters;
        inserted.parameters_ = parameters;
        inserted.targetParameters_ = parameters;
        normalled.modWheel_ = normalled.targetModWheel_ = 1.0f;
        inserted.modWheel_ = inserted.targetModWheel_ = 1.0f;
        normalled.setExternalAudioInput(false);
        inserted.setExternalAudioInput(true);

        double minimumCutoff = std::numeric_limits<double>::infinity();
        double maximumCutoff = 0.0;
        double jackCutoffDifference = 0.0;
        double jackRedNoiseDifference = 0.0;
        for (int tick = 0; tick < 4096; ++tick)
        {
            normalled.advanceControls();
            inserted.advanceControls();
            normalled.renderVoiceSample(0.0);
            inserted.renderVoiceSample(0.0);
            const auto normalledFrameIndex =
                (static_cast<std::size_t>(normalled.preMixerDelayIndex_)
                 + normalled.preMixerDelay_.size() - 1u)
                % normalled.preMixerDelay_.size();
            const auto insertedFrameIndex =
                (static_cast<std::size_t>(inserted.preMixerDelayIndex_)
                 + inserted.preMixerDelay_.size() - 1u)
                % inserted.preMixerDelay_.size();
            const auto& normalledFrame =
                normalled.preMixerDelay_[normalledFrameIndex];
            const auto& insertedFrame =
                inserted.preMixerDelay_[insertedFrameIndex];
            const double normalledCutoff = normalledFrame.upperCutoffHz
                * std::exp2(normalledFrame.audioModUpper);
            const double insertedCutoff = insertedFrame.upperCutoffHz
                * std::exp2(insertedFrame.audioModUpper);
            minimumCutoff = std::min(minimumCutoff,
                                     insertedCutoff);
            maximumCutoff = std::max(maximumCutoff, insertedCutoff);
            jackCutoffDifference = std::max(
                jackCutoffDifference,
                std::abs(normalledCutoff - insertedCutoff));
            jackRedNoiseDifference = std::max(
                jackRedNoiseDifference,
                std::abs(normalled.noise_.red() - inserted.noise_.red()));
        }
        return { maximumCutoff - minimumCutoff, jackCutoffDifference,
                 jackRedNoiseDifference };
    }

    static int multipleKeyResetSamplesWithKbdDeselected() noexcept
    {
        GhostarEngine engine;
        engine.prepare(8000.0, 64);
        EngineParameters parameters;
        parameters.gateKbd = false;
        parameters.gateX = true;
        parameters.gateYExt = false;
        parameters.trigger = TriggerMode::Multiple;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;

        engine.keyGate_ = true;
        engine.envelopeGate_ = true;
        engine.previousEnvelopeGs_ = true;
        engine.previousEnvelopeXGate_ = true;
        engine.lfoCapLevel_ = -0.6;
        engine.lfoRising_ = true;
        engine.lfoSquareHigh_ = true;
        engine.previousLfoSquareHigh_ = true;
        engine.pendingTrigger_ = true;
        engine.loudnessEnvelope_.stage = GhostarEngine::Adsr::Stage::Decay;
        engine.loudnessEnvelope_.level = 0.8;

        int releaseSamples = 0;
        do
        {
            engine.advanceControls();
            if (engine.loudnessEnvelope_.stage
                == GhostarEngine::Adsr::Stage::Release)
                ++releaseSamples;
        }
        while (engine.envelopeResetSamplesRemaining_ != 0);
        return releaseSamples;
    }

    static int arpeggiatorResetSamples(bool simultaneousX) noexcept
    {
        GhostarEngine engine;
        engine.prepare(8000.0, 64);
        EngineParameters parameters;
        parameters.gateKbd = true;
        parameters.gateX = simultaneousX;
        parameters.gateYExt = false;
        parameters.trigger = TriggerMode::Single;
        parameters.arpeggiator = ArpeggiatorMode::Ripple;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.noteOn(60, 1.0f);

        engine.previousEnvelopeGs_ = true;
        engine.previousEnvelopeXGate_ = false;
        engine.lfoCapLevel_ = -1.0;
        engine.lfoRising_ = true;
        engine.lfoSquareHigh_ = false;
        engine.previousLfoSquareHigh_ = false;
        engine.loudnessEnvelope_.stage = GhostarEngine::Adsr::Stage::Decay;
        engine.loudnessEnvelope_.level = 0.8;

        int releaseSamples = 0;
        do
        {
            engine.advanceControls();
            if (engine.loudnessEnvelope_.stage
                == GhostarEngine::Adsr::Stage::Release)
                ++releaseSamples;
        }
        while (engine.envelopeResetSamplesRemaining_ != 0);
        return releaseSamples;
    }

    static double minimumAttackCapPeak() noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.gateKbd = true;
        parameters.gateX = false;
        parameters.gateYExt = false;
        parameters.trigger = TriggerMode::Single;
        parameters.loudnessAttack = 0.0f;
        engine.parameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.keyGate_ = true;

        for (int sample = 0; sample < 2000; ++sample)
        {
            engine.advanceControls();
            if (engine.loudnessEnvelope_.stage
                == GhostarEngine::Adsr::Stage::Decay)
                return engine.loudnessEnvelope_.level;
        }
        return -1.0;
    }
};
} // namespace ghostar

namespace
{
using ghostar::EngineParameters;
using ghostar::GhostarEngine;
using ghostar::SpiritNoise;

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << "\n";
        ++failures;
    }
}

// Independent P1014 oracle: triangle is direct, saw has its 10k/10k divider
// plus the CEM3340's typical 100-ohm output impedance, and the open-emitter
// pulse output drives 10k+6k8.  All selected taps then enter the common
// 24k/91k/10k IC10 conditioner.
void testP1014SelectedWaveConditioner()
{
    constexpr double gain = 1.0 + 10.0 / 24.0 + 10.0 / 91.0;
    constexpr double triangleHigh = 12.0 / 3.0;
    constexpr double sawHigh = (2.0 * 12.0 / 3.0) * 10.0e3
                             / (100.0 + 10.0e3 + 10.0e3);
    constexpr double pulseLoad = 10.0e3 + 6.8e3;
    constexpr double pulsePinHigh = (12.0 - 0.3)
        / (1.0 + 1.3e3 / pulseLoad);
    constexpr double pulseHigh = pulsePinHigh * 6.8e3 / pulseLoad;
    const auto expected = [](double tapVolts) {
        return std::clamp(gain * tapVolts - 5.0, -12.0, 12.0);
    };
    const auto actual = [](ghostar::Waveform waveform, double sample) {
        return ghostar::GhostarCircuitTestAccess::selectedWaveVolts(
            waveform, sample);
    };

    check(std::abs(actual(ghostar::Waveform::Triangle, -1.0)
                   - expected(0.0)) < 1.0e-14
              && std::abs(actual(ghostar::Waveform::Triangle, 1.0)
                          - expected(triangleHigh)) < 1.0e-14,
          "P1014 sends the direct 0..4 V triangle into IC10");
    check(std::abs(actual(ghostar::Waveform::Sawtooth, -1.0)
                   - expected(0.0)) < 1.0e-14
              && std::abs(actual(ghostar::Waveform::Sawtooth, 1.0)
                          - expected(sawHigh)) < 1.0e-14,
          "P1014 includes the saw output impedance and 10k/10k divider");
    check(std::abs(actual(ghostar::Waveform::RectThin, -1.0)
                   - expected(0.0)) < 1.0e-14
              && std::abs(actual(ghostar::Waveform::RectThin, 1.0)
                          - expected(pulseHigh)) < 1.0e-14,
          "P1014 solves the loaded open-emitter pulse into 10k/6k8");
    check(std::abs(sawHigh / triangleHigh - 0.9950248756218906)
                  < 1.0e-15
              && std::abs(pulseHigh / triangleHigh - 1.0988950276243095)
                  < 1.0e-15,
          "the selector deliberately equalises saw and pulse near triangle");
    check(actual(ghostar::Waveform::RectThin, 1.0) < 12.0,
          "the loaded nominal pulse remains inside the 1458 supply rails");
    check(actual(ghostar::Waveform::RectThin, 10.0) == 12.0,
          "P1014 never exceeds its proven positive supply rail");

    // Put B's saw wrap 40% into this internal sample. Its emitted value has a
    // non-zero BLEP residual; using bare heldWaveB_ would fail this oracle.
    constexpr double previousConditionedB = 0.5;
    const auto phases =
        ghostar::GhostarCircuitTestAccess::oscillatorModTapAtSawWrap(
            previousConditionedB, false);
    constexpr double internalRate = 4.0 * 48000.0;
    const double bStep = 440.0 * std::exp2(previousConditionedB)
                       / internalRate;
    const double startPhase = 1.0 - 0.4 * bStep;
    const double heldRawB = 2.0 * startPhase - 1.0;
    constexpr double sawWrapHeldCorrection = -0.6 * 0.6;
    const double currentConditionedB = expected(
        0.5 * sawHigh
            * (heldRawB + sawWrapHeldCorrection + 1.0)) / 5.0;
    constexpr double pitchTau = 1.82e-6;
    const double pitchRatio = 1.0 / (internalRate * pitchTau);
    const double pitchAverage = -std::expm1(-pitchRatio) / pitchRatio;
    const double pitchNow = 1.0 - pitchAverage;
    const double filteredPitch = previousConditionedB
        + pitchNow * (currentConditionedB - previousConditionedB);
    check(std::abs(phases[0]
                   - 440.0 * std::exp2(filteredPitch) / internalRate)
              < 1.0e-14,
          "Osc A pitch filters B's fresh BLEP-corrected emitted wave");
    check(std::abs(phases[1]
                   - 0.6 * bStep)
              < 1.0e-14,
          "Osc B self-modulation retains the causal prior sample");
    check(std::abs(phases[2] - currentConditionedB) < 1.0e-14
              && std::abs(currentConditionedB
                          - expected(0.5 * sawHigh
                                     * (heldRawB + 1.0)) / 5.0)
                     > 1.0e-3,
          "the fresh modulation tap includes the saw-wrap BLEP residual");
    check(std::abs(phases[3] - filteredPitch) < 1.0e-14,
          "fresh B advances Osc A's pitch capacitor before its step");
    check(std::abs(phases[4] - filteredPitch) < 1.0e-14,
          "Osc B commits its capacitor against the fresh emitted wave");

    const auto synced =
        ghostar::GhostarCircuitTestAccess::oscillatorModTapAtSawWrap(
            previousConditionedB, true);
    check(std::abs(synced[0] - bStep) < 1.0e-14
              && std::abs(synced[3] - filteredPitch) < 1.0e-14,
          "SYNC predicts from prior B then commits A against fresh B");

    const double switched = ghostar::GhostarCircuitTestAccess::
        oscillatorWaveSwitchHeldVoltage();
    check(std::abs(switched - actual(ghostar::Waveform::Triangle, 0.0) / 5.0)
              < 1.0e-14,
          "a live selector change preserves the held sample's old voltage scale");
    const double oldVsNewScale = std::abs(
        expected(0.5 * triangleHigh) - expected(0.5 * sawHigh)) / 5.0;
    check(std::abs(std::abs(switched
                           - actual(ghostar::Waveform::Sawtooth, 0.0) / 5.0)
                   - oldVsNewScale) < 1.0e-14
              && oldVsNewScale > 1.0e-4,
          "the held triangle is not reinterpreted through the near-equal saw tap");

    const double switchedAtWrap = ghostar::GhostarCircuitTestAccess::
        oscillatorWaveSwitchAtWrapVoltage();
    constexpr double switchWrapU = 0.1;
    const double heldSaw =
        2.0 * (1.0 - switchWrapU * 440.0 / internalRate) - 1.0;
    const double pulseWrapHeldCorrection =
        2.0 * (1.0 - switchWrapU) * (1.0 - switchWrapU) * 0.5;
    const double expectedSwitchedAtWrap =
        (expected(0.5 * sawHigh * (heldSaw + 1.0))
         + 0.5 * pulseHigh * gain * pulseWrapHeldCorrection) / 5.0;
    check(std::abs(switchedAtWrap - expectedSwitchedAtWrap) < 1.0e-13
              && switchedAtWrap < 12.0 / 5.0,
          "a selector-change wrap adds the new tap's physical BLEP residual");

    const double switchedToPulse = ghostar::GhostarCircuitTestAccess::
        oscillatorSwitchToPulseHeldVoltage();
    check(std::abs(switchedToPulse
                   - actual(ghostar::Waveform::Triangle, 0.0) / 5.0)
              < 1.0e-14,
          "switching to pulse never invents a stale PWM-boundary event");
}

// Curtis specifies the CEM3340 multiplier-output bypass corner. P1014 fits
// Rs=1.82k to ground with C=1n in parallel on both oscillators; normalizing
// that current-to-voltage network gives H(s)=1/(1+s*Rs*C), so the pitch sum
// must retain 1.82 us without a rate-specific current/prior-sample choice.
void testCem3340PitchMultiplierBypass()
{
    constexpr double resistance = 1.82e3;
    constexpr double capacitance = 1.0e-9;
    constexpr double tau = resistance * capacitance;
    constexpr std::array<double, 6> hostRates {
        8000.0, 44100.0, 48000.0, 96000.0, 192000.0, 768000.0
    };

    for (const double hostRate : hostRates)
    {
        const auto probe =
            ghostar::GhostarCircuitTestAccess::pitchControlLagAt(hostRate);
        const double internalRate = 4.0 * hostRate;
        const double ratio = 1.0 / (internalRate * tau);
        const double expectedPole = std::exp(-ratio);
        const double average = -std::expm1(-ratio) / ratio;
        const double expectedNow = 1.0 - average;
        const double expectedPrevious = average - expectedPole;

        check(std::abs(probe.pole - expectedPole) < 1.0e-14
                  && std::abs(probe.nowWeight - expectedNow) < 1.0e-14
                  && std::abs(probe.previousWeight - expectedPrevious)
                         < 1.0e-14,
              "the CEM3340 pitch pole uses the 1.82k/1nF linear-input law");
        check(probe.pole >= 0.0 && probe.nowWeight >= 0.0
                  && probe.previousWeight >= 0.0
                  && std::abs(probe.pole + probe.nowWeight
                              + probe.previousWeight - 1.0) < 1.0e-14,
              "the pitch-pole weights are nonnegative and preserve DC");

        const double dcDelay = (probe.pole + probe.previousWeight)
            / (1.0 - probe.pole) / internalRate;
        check(std::abs(dcDelay - tau) < 5.0e-15,
              "the discrete pitch pole preserves the analog 1.82 us delay");

        double previous = 0.0;
        bool monotone = true;
        for (const double output : probe.step)
        {
            monotone = monotone && output >= previous - 1.0e-15
                && output <= 1.0 + 1.0e-15;
            previous = output;
        }
        check(std::abs(probe.step.front() - probe.nowWeight) < 1.0e-14
                  && monotone && probe.step.back() > 0.9999,
              "the pitch capacitor step is monotone at every supported rate");
        check(probe.startedUninitialised && probe.resetClearedState
                  && probe.resetInitialOutput == 0.375
                  && std::abs(probe.resetSteadyOutput - 0.375) < 1.0e-14,
              "reset clears pitch memory without inventing a power-up swoop");
    }

    constexpr double hostRate = 44100.0;
    constexpr double probeHz = 10000.0;
    const auto probe =
        ghostar::GhostarCircuitTestAccess::pitchControlLagAt(hostRate);
    const double omega = 2.0 * std::acos(-1.0) * probeHz
                       / (4.0 * hostRate);
    const std::complex<double> zInverse = std::polar(1.0, -omega);
    const std::complex<double> response =
        (probe.nowWeight + probe.previousWeight * zInverse)
        / (1.0 - probe.pole * zInverse);
    const double analogPhase = -std::atan(
        2.0 * std::acos(-1.0) * probeHz * tau);
    const double analogGainDb = -10.0 * std::log10(
        1.0 + std::pow(2.0 * std::acos(-1.0) * probeHz * tau, 2.0));
    const double discreteGainDb = 20.0 * std::log10(std::abs(response));
    check(std::abs(std::arg(response) - analogPhase)
                  < 0.05 * std::acos(-1.0) / 180.0,
          "the pitch pole retains the analog 10 kHz phase signature");
    check(std::abs(discreteGainDb - analogGainDb) < 0.15,
          "the pitch pole keeps 10 kHz gain close on the 4x grid");

    constexpr double internalRate = 4.0 * 48000.0;
    const double ratio = 1.0 / (internalRate * tau);
    const double average = -std::expm1(-ratio) / ratio;
    const double nowWeight = 1.0 - average;
    const double expectedBaseStep =
        440.0 * std::exp2(nowWeight) / internalRate;
    for (const bool sync : { false, true })
    {
        const auto scheduled =
            ghostar::GhostarCircuitTestAccess::pitchControlBaseStep(sync);
        check(std::abs(scheduled[0] - expectedBaseStep) < 1.0e-14
                  && std::abs(scheduled[1] - expectedBaseStep) < 1.0e-14
                  && std::abs(scheduled[2] - nowWeight) < 1.0e-14
                  && std::abs(scheduled[3] - nowWeight) < 1.0e-14,
              "both oscillators consume a new base CV without a grid delay");
    }

    const auto cyclic =
        ghostar::GhostarCircuitTestAccess::cyclicPitchControlHeldStep();
    const double pole = std::exp(-ratio);
    const double expectedHeldStep =
        440.0 * std::exp2(1.0 - pole) / internalRate;
    check(std::abs(cyclic[0] - expectedHeldStep) < 1.0e-14
              && std::abs(cyclic[1] - expectedHeldStep) < 1.0e-14,
          "self-FM and SYNC predict the capacitor under causal held B");
}

// The CEM3340 itself supports the complete 0..100% PWM range.  At either
// endpoint the coincident pulse events cancel, leaving a true DC plateau at
// the corresponding P1014-conditioned level rather than a 3/97% guard band.
void testPulseWidthReachesTheCemEndpoints()
{
    constexpr double gain = 1.0 + 10.0 / 24.0 + 10.0 / 91.0;
    constexpr double pulseLoad = 10.0e3 + 6.8e3;
    constexpr double pulseHigh = (12.0 - 0.3)
        / (1.0 + 1.3e3 / pulseLoad) * 6.8e3 / pulseLoad;
    constexpr double expectedLow = -5.0 / 5.0;
    constexpr double expectedHigh = (gain * pulseHigh - 5.0) / 5.0;

    const auto zero =
        ghostar::GhostarCircuitTestAccess::pulseDutyEndpointRange(0.0);
    const auto one =
        ghostar::GhostarCircuitTestAccess::pulseDutyEndpointRange(1.0);
    check(std::abs(zero[0] - expectedLow) < 1.0e-13
              && std::abs(zero[1] - expectedLow) < 1.0e-13,
          "zero-percent PWM is the constant conditioned pulse low");
    check(std::abs(one[0] - expectedHigh) < 1.0e-13
              && std::abs(one[1] - expectedHigh) < 1.0e-13,
          "hundred-percent PWM is the constant conditioned pulse high");
}

// One internal sample of the generic C37 companion integration must satisfy
// its TPT seam and the BA130 pair's KVL independently of played-instrument
// tests. The Lower branch constants are also pinned here; its distinct
// moving-input solve has a separate component-level oracle below.
void testHighQCompanionsSatisfyTheirIntegratedEquations()
{
    constexpr double nodeVoltsPerUnit = 5.0;
    constexpr double diodeVolts = 0.043;
    constexpr double pairSaturationAmps = 4.6e-9;
    constexpr double capacitanceRatio = 22.0;
    constexpr double chargeStep =
        1.0 / (2.0 * 192000.0 * 22.0e-9 * nodeVoltsPerUnit);

    const auto config = ghostar::GhostarCircuitTestAccess::highQConfig();
    check(std::abs(config.lowerGain
                   - (1.0 + 33000.0 / 220.0)
                       * 2200.0 / 24200.0) < 1.0e-14,
          "Lower high-Q branch uses the P1013 gain network");
    check(std::abs(config.lowerResistance - 2000.0) < 1.0e-12,
          "Lower high-Q branch uses R171 parallel R172");
    check(std::abs(config.upperGain - 16.0) < 1.0e-14,
          "Upper high-Q branch uses the P1013 gain network");
    check(config.upperResistance == 0.0,
          "Upper high-Q branch keeps the ideal TL082 source resistance");

    const auto verify = [&](double oldBp, double oldLp, double oldCharge,
                            double input, double g, double k,
                            double gain, double resistance) {
        const auto result = ghostar::GhostarCircuitTestAccess::highQStep(
            oldBp, oldLp, oldCharge, input, g, k, gain, resistance,
            chargeStep);
        const double current =
            (result.chargeCompanion - oldCharge) / (2.0 * chargeStep);
        const double a = 1.0 / (1.0 + g * (g + k));
        const double baselineBp = a * (oldBp + g * (input - oldLp));
        const double bpPerAmp = -a * g * chargeStep;
        const double baselineLp = oldLp + g * baselineBp;
        const double drive = nodeVoltsPerUnit
            * (gain * baselineBp - baselineLp
               - capacitanceRatio * oldCharge);
        const double series = resistance + nodeVoltsPerUnit
            * (g * bpPerAmp + (1.0 + capacitanceRatio) * chargeStep
               - gain * bpPerAmp);
        const double diodeResidual = std::fma(series, current, -drive)
            + diodeVolts * std::asinh(current / pairSaturationAmps);

        check(std::abs(result.bp - (baselineBp + bpPerAmp * current))
                  < 1.0e-12,
              "high-Q BP endpoint satisfies the coupled TPT equation");
        check(std::abs(result.lp
                       - (baselineLp
                          + (g * bpPerAmp + chargeStep) * current))
                  < 1.0e-12,
              "high-Q LP endpoint conserves coupling-cap charge");
        check(std::abs(result.hp - (input - k * result.bp - result.lp))
                  < 1.0e-12,
              "the generic TPT seam satisfies its HP endpoint identity");
        check(std::abs(diodeResidual) < 1.0e-8 * (1.0 + std::abs(drive)),
              "high-Q diode current satisfies KVL");
        return result;
    };

    const auto lower = verify(0.13, -0.04, 0.005, 0.27, 0.2, 0.08,
                              config.lowerGain, config.lowerResistance);
    const auto lowerNegative = verify(-0.13, 0.04, -0.005, -0.27, 0.2,
                                      0.08, config.lowerGain,
                                      config.lowerResistance);
    check(std::abs(lower.bp + lowerNegative.bp) < 1.0e-12
              && std::abs(lower.lp + lowerNegative.lp) < 1.0e-12
              && std::abs(lower.chargeCompanion
                          + lowerNegative.chargeCompanion) < 1.0e-12,
          "the Lower local C33 reduction is odd-symmetric");

    verify(-0.09, 0.03, -0.002, -0.18, 0.35, -0.005,
           config.upperGain, config.upperResistance);
}

// P1013 makes the Upper filter one switched four-state network, not two
// independent SVFs. Each CEM half has VIF and VIV tied, SW4 moves C40 between
// their VLP caps, R194 couples those nodes only in 12 dB, and the linked
// IC14B pole changes gain. Check the component equations directly (OQ-09).
void testUpperCascadeMatchesP1013()
{
    constexpr double hostRate = 48000.0;
    constexpr double internalRate = 4.0 * hostRate;
    constexpr double timingCapacitance = 22.0e-9;
    constexpr double memoryCapacitance = 1.0e-9;
    constexpr double couplingOhms = 1.0e6;
    constexpr double nodeVoltsPerUnit = 5.0;
    constexpr double diodeVolts = 0.043;
    constexpr double pairSaturationAmps = 4.6e-9;
    constexpr double highQCapacitanceRatio = 22.0;
    constexpr double highQGain = 16.0;
    constexpr double outputGain12 = 201.0;
    constexpr double outputGain24 = 101.0;

    const auto low = ghostar::GhostarCircuitTestAccess::upperControl(
        ghostar::UpperResonanceMode::Low, 0.91f);
    check(low[0] == 2.0 && low[1] == 3.0,
          "Upper LOW ties VIF+VIV for drive 3 while retaining Q=0.5");

    // Independent Q-pin KCL: input drive uses 1/Qcommanded, while only the
    // loop damping receives the declared 1/50 external-enhancement offset.
    constexpr float resonanceTravel = 0.73f;
    const double t = static_cast<double>(resonanceTravel);
    const double wiperOhms = 18.2e3 + 100.0e3 * t * (1.0 - t);
    const double qPinConductance = 1.0 / wiperOhms + 1.0 / 91.0e3
                                 + 1.0 / 221.0;
    const double qPinCurrent = -12.0 * t / wiperOhms + 12.0 / 91.0e3;
    const double qPinVolts = qPinCurrent / qPinConductance;
    const double lowPinVolts = 12.0 * 221.0 / (91.0e3 + 221.0);
    const double commandedQ = 0.5 * std::pow(
        10.0, -(qPinVolts - lowPinVolts) / 0.065);
    const double commandedDamping = 1.0 / commandedQ;
    const auto variable = ghostar::GhostarCircuitTestAccess::upperControl(
        ghostar::UpperResonanceMode::Variable, resonanceTravel);
    check(std::abs(variable[0] - (commandedDamping - 1.0 / 50.0))
                  < 1.0e-13
              && std::abs(variable[1] - (1.0 + commandedDamping))
                  < 1.0e-13,
          "Upper tied-input drive uses commanded Q before enhancement");

    // Charge sharing on both switch directions: C40 retains the old selected
    // endpoint, the new 22 nF node receives 1/23 of the voltage difference,
    // and neither the abandoned node nor C37 moves in the ideal event.
    constexpr double controlledLp = 0.31;
    constexpr double fixedLp = -0.17;
    constexpr double controlledCompanion = 0.29;
    constexpr double fixedCompanion = -0.15;
    constexpr double highQCompanion = 0.004;
    const auto to24 = ghostar::GhostarCircuitTestAccess::upperSlopeProjection(
        ghostar::UpperSlope::TwelveDb,
        ghostar::UpperSlope::TwentyFourDb,
        controlledLp, fixedLp, controlledCompanion, fixedCompanion,
        highQCompanion);
    const double sharedFixed =
        (22.0 * fixedLp + controlledLp) / 23.0;
    check(to24.controlledLowpass == controlledLp
              && to24.controlledLowpassCompanion == controlledCompanion
              && std::abs(to24.fixedLowpass - sharedFixed) < 1.0e-15
              && std::abs(to24.fixedLowpassCompanion
                          - (fixedCompanion + sharedFixed - fixedLp))
                  < 1.0e-15
              && to24.highQCompanion == highQCompanion,
          "12-to-24 dB transfers C40 charge only into the fixed VLP node");

    const auto to12 = ghostar::GhostarCircuitTestAccess::upperSlopeProjection(
        ghostar::UpperSlope::TwentyFourDb,
        ghostar::UpperSlope::TwelveDb,
        to24.controlledLowpass, to24.fixedLowpass,
        to24.controlledLowpassCompanion, to24.fixedLowpassCompanion,
        to24.highQCompanion);
    const double sharedControlled =
        (22.0 * controlledLp + sharedFixed) / 23.0;
    check(std::abs(to12.controlledLowpass - sharedControlled) < 1.0e-15
              && std::abs(to12.controlledLowpassCompanion
                          - (controlledCompanion
                             + sharedControlled - controlledLp))
                  < 1.0e-15
              && to12.fixedLowpass == sharedFixed
              && to12.fixedLowpassCompanion
                     == to24.fixedLowpassCompanion
              && to12.highQCompanion == highQCompanion,
          "24-to-12 dB transfers C40 charge only into controlled VLP");

    const auto verifyStep = [&](ghostar::UpperSlope slope,
                                const std::array<double, 4>& old,
                                const std::array<double, 2>& oldEndpoints,
                                double oldHighQ, double input, double g,
                                double controlledK,
                                double controlledInputGain) {
        const auto result =
            ghostar::GhostarCircuitTestAccess::upperCascadeStep(
                slope, old, oldEndpoints, oldHighQ, input, g,
                controlledK, controlledInputGain, hostRate);
        const double current =
            (result.highQCompanion - oldHighQ)
            / (2.0 * result.highQChargeStep);
        const double controlledCap = timingCapacitance
            + (slope == ghostar::UpperSlope::TwelveDb
                   ? memoryCapacitance : 0.0);
        const double fixedCap = timingCapacitance
            + (slope == ghostar::UpperSlope::TwentyFourDb
                   ? memoryCapacitance : 0.0);
        const double controlledG = g * timingCapacitance / controlledCap;
        const double fixedG = g * timingCapacitance / fixedCap;
        const double controlledCoupling =
            slope == ghostar::UpperSlope::TwelveDb
                ? 1.0 / (2.0 * internalRate * couplingOhms * controlledCap)
                : 0.0;
        const double fixedCoupling =
            slope == ghostar::UpperSlope::TwelveDb
                ? 1.0 / (2.0 * internalRate * couplingOhms * fixedCap)
                : 0.0;
        const double currentStep =
            1.0 / (2.0 * internalRate * controlledCap
                   * nodeVoltsPerUnit);

        check(std::abs(result.controlledBandpass
                       - (old[0] + g * (controlledInputGain * input
                                       - result.controlledLowpass
                                       - controlledK
                                           * result.controlledBandpass)))
                  < 1.0e-12,
              "controlled Upper BP satisfies tied-input CEM KCL");
        check(std::abs(result.controlledLowpass
                       - (old[1]
                          + controlledG * result.controlledBandpass
                          + controlledCoupling
                              * (result.fixedLowpass
                                 - result.controlledLowpass)
                          + currentStep * current))
                  < 1.0e-12,
              "controlled Upper VLP satisfies selected-cap and R194 KCL");
        check(std::abs(result.fixedBandpass
                       - (old[2] + g * (3.0 * result.controlledLowpass
                                       - result.fixedLowpass
                                       - 2.0 * result.fixedBandpass)))
                  < 1.0e-12,
              "fixed Upper BP receives both tied inputs at Q=0.5");
        check(std::abs(result.fixedLowpass
                       - (old[3] + fixedG * result.fixedBandpass
                          + fixedCoupling
                              * (result.controlledLowpass
                                 - result.fixedLowpass)))
                  < 1.0e-12,
              "fixed Upper VLP satisfies selected-cap and R194 KCL");

        check(std::abs(result.controlledLowpass
                       - 0.5 * (old[1] + result.companions[1]))
                  < 1.0e-12
              && std::abs(result.fixedLowpass
                          - 0.5 * (old[3] + result.companions[3]))
                  < 1.0e-12,
              "Upper physical VLP endpoints match their TPT companions");

        const double charge =
            0.5 * (oldHighQ + result.highQCompanion);
        const double diodeDrive = nodeVoltsPerUnit
            * (highQGain * result.controlledBandpass
               - result.controlledLowpass
               - highQCapacitanceRatio * charge);
        const double diodeResidual = -diodeDrive
            + diodeVolts * std::asinh(current / pairSaturationAmps);
        check(std::abs(diodeResidual)
                  < 1.0e-8 * (1.0 + std::abs(diodeDrive)),
              "Upper C37/BA130 endpoint satisfies physical KVL");

        const double expectedOutput =
            slope == ghostar::UpperSlope::TwelveDb
                ? outputGain12 * result.controlledLowpass
                : outputGain24 * result.fixedLowpass;
        check(std::abs(result.output - expectedOutput) < 1.0e-14,
              "SW4 selects the traced VLP tap and linked IC14B gain");
        return result;
    };

    verifyStep(ghostar::UpperSlope::TwelveDb,
               { 0.011, -0.017, -0.013, 0.029 }, { -0.02, 0.03 },
               0.003, 0.025, 0.08, 0.41, 1.63);
    verifyStep(ghostar::UpperSlope::TwentyFourDb,
               { -0.016, 0.021, 0.009, -0.027 }, { 0.02, -0.03 },
               -0.002, -0.031, 0.11, 0.28, 1.42);

    // Equal, static VLP nodes isolate the linked output pole and its absolute
    // 201/101 gains.
    constexpr double staticLp = 0.2;
    constexpr double balancingCharge = -staticLp / 22.0;
    const std::array<double, 4> staticStates { 0.0, staticLp,
                                               0.0, staticLp };
    const std::array<double, 2> staticEndpoints { staticLp, staticLp };
    const auto output12 =
        ghostar::GhostarCircuitTestAccess::upperCascadeStep(
            ghostar::UpperSlope::TwelveDb, staticStates, staticEndpoints,
            balancingCharge, 0.0, 0.0, 2.0, 3.0, hostRate);
    const auto output24 =
        ghostar::GhostarCircuitTestAccess::upperCascadeStep(
            ghostar::UpperSlope::TwentyFourDb, staticStates, staticEndpoints,
            balancingCharge, 0.0, 0.0, 2.0, 3.0, hostRate);
    check(std::abs(output12.output - outputGain12 * staticLp) < 1.0e-14
              && std::abs(output24.output - outputGain24 * staticLp)
                  < 1.0e-14
              && std::abs(output24.output / output12.output
                          - outputGain24 / outputGain12) < 1.0e-15,
          "IC14B applies its absolute 201/101 gains with SLOPE");
}

// P1013 does not sum the three Lower sources at virtual earth. Every 100k
// wiper sees VLP through 220k and VBP through 68p, so even a slider at zero
// continues loading both moving nodes. This oracle shares no matrix assembly
// with the engine: it checks each branch KCL and both integrated state laws at
// the endpoint the production 2x2 solve returned.
void testLowerMixerMnaSatisfiesP1013()
{
    constexpr double hostRate = 48000.0;
    constexpr double internalRate = 4.0 * hostRate;
    constexpr double timingCapacitance = 22.0e-9;
    constexpr double wiperCapacitance = 68.0e-12;
    constexpr double wiperResistance = 220.0e3;
    constexpr double potResistance = 100.0e3;
    constexpr double nodeVoltsPerUnit = 5.0;
    constexpr double highQCapRatio = 22.0;
    constexpr double wiperCapRatio =
        wiperCapacitance / timingCapacitance;
    constexpr double wiperConductance =
        2.0 * internalRate * wiperCapacitance;
    constexpr double resistanceStep =
        1.0 / (2.0 * internalRate * timingCapacitance
               * wiperResistance);
    constexpr double diodeVolts = 0.043;
    constexpr double pairSaturationAmps = 4.6e-9;

    const std::array<double, 3> sources { 0.61, -0.27, 0.19 };
    const std::array<double, 3> travels { 0.0, 0.37, 1.0 };
    const std::array<double, 3> oldMixer { 0.08, -0.03, 0.04 };
    constexpr double oldBp = 0.11;
    constexpr double oldLp = -0.06;
    constexpr double oldHighQ = 0.003;
    constexpr double dry = 0.23;
    constexpr double g = 0.21;
    constexpr double k = 0.34;

    const auto result = ghostar::GhostarCircuitTestAccess::lowerMnaStep(
        sources, travels, oldMixer, oldBp, oldLp, oldHighQ,
        dry, g, k, hostRate);
    constexpr double expectedHighQChargeStep =
        1.0 / (2.0 * internalRate * timingCapacitance * nodeVoltsPerUnit);
    check(std::abs(result.highQChargeStep - expectedHighQChargeStep)
              < 1.0e-15,
          "the C33/C37 charge step uses the 22 nF CEM state and 5 V unit");
    const auto config = ghostar::GhostarCircuitTestAccess::highQConfig();
    const double highQCurrent =
        (result.highQCompanion - oldHighQ)
        / (2.0 * result.highQChargeStep);

    std::array<double, 3> wipers {};
    double lowpassInjection = 0.0;
    double bandpassInjection = 0.0;
    double endpointBpTransfer = 0.0;
    for (std::size_t index = 0; index < sources.size(); ++index)
    {
        const double travel = travels[index];
        const double source = travel * sources[index];
        const double thevenin = potResistance * travel * (1.0 - travel);
        if (thevenin == 0.0)
        {
            wipers[index] = source;
            check(std::abs(wipers[index] - source) < 1.0e-15,
                  "a Lower slider endpoint clamps its physical wiper");
        }
        else
        {
            const double denominator = 1.0 / thevenin
                                     + 1.0 / wiperResistance
                                     + wiperConductance;
            wipers[index] = (source / thevenin
                + result.lp / wiperResistance
                + wiperConductance * (result.bp + oldMixer[index]))
                / denominator;
            const double kcl = (wipers[index] - source) / thevenin
                + (wipers[index] - result.lp) / wiperResistance
                + wiperConductance
                    * (wipers[index] - result.bp - oldMixer[index]);
            check(std::abs(kcl) < 1.0e-14,
                  "each finite Lower wiper satisfies its three-arm KCL");
        }

        lowpassInjection += wipers[index] - result.lp;
        bandpassInjection += wipers[index] - result.bp - oldMixer[index];
        const double voltage = wipers[index] - result.bp;
        const double ordinaryNext = 2.0 * voltage - oldMixer[index];
        if (travel == 0.0 || travel == 1.0)
        {
            check(std::abs(result.mixerCompanions[index] - voltage)
                      < 1.0e-12,
                  "an endpoint 68p companion has no hidden alternating mode");
            endpointBpTransfer += wiperCapRatio
                                * (voltage - ordinaryNext);
        }
        else
        {
            check(std::abs(result.mixerCompanions[index] - ordinaryNext)
                      < 1.0e-12,
                  "a finite wiper advances its 68p trapezoidal companion");
        }
    }

    check(std::abs(result.lp
                   - (oldLp + g * result.bp
                      + resistanceStep * lowpassInjection
                      + result.highQChargeStep * highQCurrent)) < 1.0e-12,
          "Lower VLP satisfies the CEM, 220k and C33 integrated equation");
    check(std::abs(result.bp
                   - (oldBp - g * (result.lp + k * result.bp)
                      + wiperCapRatio * bandpassInjection)) < 1.0e-12,
          "Lower VBP satisfies the CEM and three 68p integrated equations");
    check(std::abs(result.lowpassCompanion - (2.0 * result.lp - oldLp))
              < 1.0e-12,
          "Lower VLP advances its trapezoidal companion");
    check(std::abs(result.bandpassCompanion
                   - (2.0 * result.bp - oldBp + endpointBpTransfer))
              < 1.0e-12,
          "endpoint projection transfers equal discrete charge into VBP");
    check(std::abs(result.hp - (dry - k * result.bp - result.lp))
              < 1.0e-12,
          "the Lower solver exposes its canonical diagnostic HP identity");

    const double capacitorCharge = oldHighQ
                                 + result.highQChargeStep * highQCurrent;
    const double diodeDrive = nodeVoltsPerUnit
        * (config.lowerGain * result.bp - result.lp
           - highQCapRatio * capacitorCharge);
    const double diodeResidual = std::fma(config.lowerResistance,
                                          highQCurrent, -diodeDrive)
        + diodeVolts * std::asinh(highQCurrent / pairSaturationAmps);
    check(std::abs(diodeResidual) < 1.0e-8 * (1.0 + std::abs(diodeDrive)),
          "Lower C33's BA130 current satisfies its physical KVL");

    check(std::abs((wipers[0] - result.lp) / wiperResistance)
                  + std::abs(wiperConductance
                      * (wipers[0] - result.bp - oldMixer[0])) > 1.0e-9,
          "a zeroed Lower slider still loads both moving state nodes");
}

// Independent scalar and one-pole reference for the hypothesised A3+B7+C10
// network. Bisection deliberately shares no Newton
// implementation with the engine, so resistor/sign errors cannot agree by
// construction. C10 makes R167 a clean-VLP feed, not a ground shunt.
void testHypotheticalA3B7C10NetworkMatchesReference()
{
    constexpr double nodeVoltsPerUnit = 5.0;
    constexpr double diodeVolts = 0.043;
    constexpr double pairSaturationAmps = 4.6e-9;
    constexpr double feedback = 330.0e3;
    constexpr double clamp = 2.2e3;
    constexpr double inverting = 2.2e3;
    constexpr double diodeReturn = 470.0;
    constexpr double linearGain =
        1.0 + feedback / inverting + feedback / diodeReturn;
    constexpr double driveGain = 0.5 * linearGain - 1.0;
    constexpr double diodeSeries = 0.5 * (feedback + clamp);
    constexpr double pickup = 47.0e3;
    constexpr double shunt = 33.0e3;
    constexpr double load = 220.0;
    constexpr double coupling = 220.0e-9;

    const auto reference = [&](double lowerLowpass, double oldState,
                               double hostRate) {
        const double inputVolts = nodeVoltsPerUnit * lowerLowpass;
        const double drive = driveGain * inputVolts;
        const double magnitude = std::abs(drive);
        double lo = 0.0;
        double hi = magnitude / diodeSeries;
        for (int step = 0; step < 80; ++step)
        {
            const double mid = 0.5 * (lo + hi);
            const double residual = diodeSeries * mid
                + diodeVolts * std::asinh(mid / pairSaturationAmps)
                - magnitude;
            if (residual > 0.0)
                hi = mid;
            else
                lo = mid;
        }
        const double current = std::copysign(0.5 * (lo + hi), drive);
        const double opAmpOutput =
            (linearGain * inputVolts - feedback * current)
            / nodeVoltsPerUnit;
        const double internalRate = 4.0 * hostRate;
        const double conductance = 2.0 * internalRate * coupling;
        const double sourceVoltage =
            (opAmpOutput * shunt + lowerLowpass * pickup)
            / (pickup + shunt);
        const double sourceOhms = pickup * shunt / (pickup + shunt);
        const double seriesOhms = sourceOhms + load;
        const double capacitorCurrent = conductance
            * (sourceVoltage - oldState)
            / (1.0 + conductance * seriesOhms);
        const double capacitorVoltage = sourceVoltage
                                       - seriesOhms * capacitorCurrent;
        return ghostar::OverdriveProbeResult {
            load * capacitorCurrent,
            2.0 * capacitorVoltage - oldState
        };
    };

    constexpr double hostRate = 48000.0;
    // Span the BA130's almost-open small-signal region, its knee and a hard
    // transient. This guards the production solver's residual, not an
    // iteration count: the independent 80-step bisection remains the oracle.
    for (const double drive : { 1.0e-12, 1.0e-7, 1.0e-3, 0.12, 3.0 })
    {
        const auto actual = ghostar::GhostarCircuitTestAccess::overdriveStep(
            drive, -0.015, hostRate);
        const auto expected = reference(drive, -0.015, hostRate);
        check(std::abs(actual.output - expected.output) < 1.0e-11,
              "A3+B7+C10 implicit diode solve matches bisection across drive");
        check(std::abs(actual.couplingCompanion
                       - expected.couplingCompanion) < 1.0e-11,
              "A3+B7+C10 C34 state matches bisection across drive");
    }

    const auto actual = ghostar::GhostarCircuitTestAccess::overdriveStep(
        0.12, -0.015, hostRate);

    const auto negative = ghostar::GhostarCircuitTestAccess::overdriveStep(
        -0.12, 0.015, hostRate);
    check(std::abs(actual.output + negative.output) < 1.0e-12
              && std::abs(actual.couplingCompanion
                          + negative.couplingCompanion) < 1.0e-12,
          "A3+B7+C10 hypothesis is odd-symmetric");
}

// P1013 puts C18/P3 across the Shaper VCA's 20k output load and C30 in
// series with the Filter path before its VCA. These one-step checks solve
// the two capacitor companions independently from the production code.
void testOutputCapacitorCompanionsMatchP1013()
{
    constexpr double brightnessLoad = 20000.0;
    constexpr double brightnessCap = 27.0e-9;
    constexpr double filterLoad = 24000.0 * 100000.0 / 124000.0;
    constexpr double filterCap = 470.0e-9;

    check(std::abs(ghostar::GhostarCircuitTestAccess::brightnessResistance(
                       0.0f)) < 1.0e-12,
          "BRIGHTNESS zero reaches a zero-ohm rheostat");
    check(std::abs(ghostar::GhostarCircuitTestAccess::brightnessResistance(
                       1.0f) - 100000.0) < 1.0e-9,
          "BRIGHTNESS ten reaches the full 100k rheostat");

    for (const double hostRate : { 44100.0, 48000.0, 96000.0 })
    {
        const double internalRate = 4.0 * hostRate;
        const double brightnessG = 2.0 * internalRate * brightnessCap;
        for (const double rheostat : { 0.0, 12345.0, 100000.0 })
        {
            constexpr double input = -0.37;
            constexpr double oldCompanion = 0.08;
            const auto result =
                ghostar::GhostarCircuitTestAccess::brightnessStep(
                    input, oldCompanion, rheostat, hostRate);
            const double capacitorVoltage =
                0.5 * (result.companion + oldCompanion);
            const double current =
                (input - result.output) / brightnessLoad;
            check(std::abs(result.output - capacitorVoltage
                           - rheostat * current) < 1.0e-12,
                  "C18/P3 satisfies its series-branch KVL");
            check(std::abs(current - brightnessG
                           * (capacitorVoltage - oldCompanion)) < 1.0e-12,
                  "C18 conserves charge in its trapezoidal companion");
        }

        constexpr double input = 0.29;
        constexpr double oldCompanion = -0.04;
        const auto result =
            ghostar::GhostarCircuitTestAccess::filterCouplingStep(
                input, oldCompanion, hostRate);
        const double capacitorVoltage =
            0.5 * (result.companion + oldCompanion);
        const double current = result.output / filterLoad;
        const double filterG = 2.0 * internalRate * filterCap;
        check(std::abs(input - result.output - capacitorVoltage) < 1.0e-12,
              "C30 satisfies its series-capacitor KVL");
        check(std::abs(current - filterG
                       * (capacitorVoltage - oldCompanion)) < 1.0e-12,
              "C30 conserves charge in its trapezoidal companion");
    }
}

// The ring path AC-couples A through C15 into both IC7 and IC6; P2 nulls
// the dry-A carrier. At that null the printed B divider yields -15/13 times
// the two normalized triangles, with no symmetric carrier term.
void testRingModulatorMatchesP1013()
{
    constexpr double load = 39000.0 * 100000.0 / 139000.0;
    constexpr double capacitance = 1.0e-6;
    constexpr double triangleA = 0.31;
    constexpr double triangleB = -0.4;
    constexpr double oldCompanion = 0.07;

    for (const double hostRate : { 44100.0, 48000.0, 96000.0 })
    {
        const auto result = ghostar::GhostarCircuitTestAccess::ringStep(
            triangleA, triangleB, oldCompanion, hostRate);
        const double coupledA = result.output
            / (-(15.0 / 13.0) * triangleB);
        const double capacitorVoltage =
            0.5 * (result.companion + oldCompanion);
        const double current = coupledA / load;
        const double g = 2.0 * 4.0 * hostRate * capacitance;
        check(std::abs(triangleA - coupledA - capacitorVoltage) < 1.0e-12,
              "ring C15 satisfies its series-capacitor KVL");
        check(std::abs(current - g
                       * (capacitorVoltage - oldCompanion)) < 1.0e-12,
              "ring C15 conserves charge in its trapezoidal companion");
    }

    check(ghostar::GhostarCircuitTestAccess::ringStep(
              triangleA, 0.0, 0.0, 48000.0).output == 0.0,
          "the nulled ring has no invented A-carrier bleed");
    check(ghostar::GhostarCircuitTestAccess::ringStep(
              0.0, triangleB, 0.0, 48000.0).output == 0.0,
          "the ideal ring has no invented B-carrier bleed");
}

std::vector<float> renderMono(GhostarEngine& engine, double seconds,
                              double sampleRate, int discardBlocks = 0)
{
    constexpr int blockSize = 256;
    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    std::vector<float> samples;
    auto blocks = static_cast<int>(
        std::lround(seconds * sampleRate / blockSize));
    for (int block = 0; block < blocks; ++block)
    {
        engine.process(left.data(), right.data(), blockSize);
        if (block >= discardBlocks)
            samples.insert(samples.end(), left.begin(), left.end());
    }
    return samples;
}

double meanAbs(const std::vector<float>& samples)
{
    double sum = 0.0;
    for (const float value : samples)
        sum += std::abs(static_cast<double>(value));
    return samples.empty() ? 0.0 : sum / static_cast<double>(samples.size());
}

double mean(const std::vector<float>& samples)
{
    double sum = 0.0;
    for (const float value : samples)
        sum += static_cast<double>(value);
    return samples.empty() ? 0.0 : sum / static_cast<double>(samples.size());
}

double peak(const std::vector<float>& samples)
{
    double result = 0.0;
    for (const float value : samples)
        result = std::max(result, std::abs(static_cast<double>(value)));
    return result;
}

// Signal energy at one frequency, for fundamental checks that zero
// crossings cannot decide (synced and multi-peak waveforms).
double goertzelMagnitude(const std::vector<float>& samples, double hz,
                         double sampleRate)
{
    const double w = 2.0 * 3.14159265358979323846 * hz / sampleRate;
    const double coefficient = 2.0 * std::cos(w);
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;
    for (const float value : samples)
    {
        s0 = static_cast<double>(value) + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power =
        s1 * s1 + s2 * s2 - coefficient * s1 * s2;
    return std::sqrt(std::max(power, 0.0))
         / std::max(1.0, static_cast<double>(samples.size()));
}

EngineParameters brightPanel()
{
    EngineParameters parameters;
    parameters.filterPathA = 0.8f;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.kbAmount = 0.0f;
    parameters.filterEnvAmount = 0.5f;
    parameters.loudnessAttack = 0.0f;
    parameters.loudnessSustain = 1.0f;
    return parameters;
}

double dominantFrequency(int midiNote)
{
    GhostarEngine engine;
    engine.prepare(48000.0, 256);
    engine.setParameters(brightPanel());
    engine.noteOn(midiNote, 1.0f);
    const auto samples = renderMono(engine, 0.6, 48000.0, 20);

    int crossings = 0;
    for (std::size_t index = 1; index < samples.size(); ++index)
        if (samples[index - 1] <= 0.0f && samples[index] > 0.0f)
            ++crossings;
    const double seconds = static_cast<double>(samples.size()) / 48000.0;
    return static_cast<double>(crossings) / seconds;
}

// The keyboard law: MIDI 69 sounds 440 Hz at 8', one octave doubles.
void testKeyboardLaw()
{
    const double lower = dominantFrequency(57);
    const double upper = dominantFrequency(69);
    check(std::abs(lower - 220.0) < 6.0, "A3 sounds near 220 Hz");
    check(std::abs(upper - 440.0) < 9.0, "A4 sounds near 440 Hz");
    check(std::abs(upper / std::max(lower, 1.0) - 2.0) < 0.05,
          "one octave doubles the sounding frequency");
}

// P1013's 100k linear TUNE pot sees R19=1.8M into IC6's virtual earth.
// Physical noon remains the model's zero, so loading bends the travel and
// creates a small endpoint skew without changing the six-semitone total span.
void testMasterTuneFollowsItsLoadedPot()
{
    using ghostar::GhostarCircuitTestAccess;

    const auto expectedOctaves = [](double travel) {
        const double loaded = travel * 1800.0
            / (1800.0 + 100.0 * travel * (1.0 - travel));
        return (loaded - 36.0 / 73.0) * (6.0 / 12.0);
    };
    for (const double travel : { 0.0, 0.25, 0.5, 0.75, 1.0 })
    {
        const double actual =
            GhostarCircuitTestAccess::oscillatorATuneOctaves(
                static_cast<float>(travel));
        check(std::abs(actual - expectedOctaves(travel)) < 1.0e-12,
              "MASTER TUNE follows P1's loaded linear-pot law");
    }

    const double low =
        GhostarCircuitTestAccess::oscillatorATuneOctaves(0.0f);
    const double centre =
        GhostarCircuitTestAccess::oscillatorATuneOctaves(0.5f);
    const double high =
        GhostarCircuitTestAccess::oscillatorATuneOctaves(1.0f);
    check(std::abs(centre) < 1.0e-15,
          "MASTER TUNE preserves zero at physical noon");
    check(std::abs((high - low) * 12.0 - 6.0) < 1.0e-12,
          "MASTER TUNE retains its nominal six-semitone end-to-end span");
    check(std::abs(low * 12.0 + 216.0 / 73.0) < 1.0e-12
              && std::abs(high * 12.0 - 222.0 / 73.0) < 1.0e-12,
          "MASTER TUNE exposes the loaded pot's nominal endpoint skew");
}

// The 108.3% amount is component-derived. P1016 also closes the absolute
// pivot: six key bits feed DAC0800 B1..B6 while B7/B8 are grounded, so each
// semitone advances four DAC counts. The pin-4 sink cancels +12A/R39 at
// q=64*R31/R39 semitones above the lowest C (MIDI 48). Because both currents
// share +12A, the rail and IC16A feedback resistor cancel out of this oracle.
void testKeyboardTrackingAmount()
{
    const double lower =
        ghostar::GhostarCircuitTestAccess::upperCutoffForNote(60, 1.0f);
    const double upper =
        ghostar::GhostarCircuitTestAccess::upperCutoffForNote(72, 1.0f);
    check(std::abs(upper / lower - std::exp2(1.083)) < 1.0e-12,
          "one keyboard octave raises cutoff by the derived 108.3 percent");

    constexpr double pivotMidi = 48.0 + 64.0 * 4.99 / 26.6;
    const double untracked =
        ghostar::GhostarCircuitTestAccess::upperCutoffForNote(60, 0.0f);
    const double secondC =
        ghostar::GhostarCircuitTestAccess::upperCutoffForNote(60, 1.0f);
    const double expectedRatio =
        std::exp2(1.083 * (60.0 - pivotMidi) / 12.0);
    check(std::abs(secondC / untracked - expectedRatio) < 1.0e-12,
          "tracking zero follows P1016's signed DAC/reference cancellation");
    check(secondC < untracked,
          "the second C sits just below the nominal cancellation pitch");
}

// P1017 switches source-side D before P1016's loaded N input and IC16B's
// inverted P output. The jack replaces pitch only: keyboard gate/trigger and
// arpeggiator state remain on their own paths.
void testExternalPitchSwitchAndCircuitTransfer()
{
    using ghostar::GhostarCircuitTestAccess;

    constexpr double sourceOhms = 15.0e3;
    constexpr double inputOhms = 95.3e3;
    constexpr double feedbackOhms = 100.0e3;
    for (const double source : { -1.1, 0.0, 1.1 })
    {
        const auto nodes = GhostarCircuitTestAccess::externalPitchNodes(source);
        const double expectedN = source * inputOhms
                               / (sourceOhms + inputOhms);
        const double expectedP = -expectedN * feedbackOhms / inputOhms;
        check(std::abs(nodes[0] - expectedN) < 1.0e-15
                  && std::abs(nodes[1] - expectedP) < 1.0e-15,
              "EXTERNAL PITCH independently follows the D/N/P resistor transfer");
    }

    constexpr double pivot = 48.0 + 64.0 * 4.99 / 26.6;
    const auto unplugged = GhostarCircuitTestAccess::externalPitchAt(
        1.1, false, 72);
    const auto zero = GhostarCircuitTestAccess::externalPitchAt(0.0, true, 72);
    const auto positive = GhostarCircuitTestAccess::externalPitchAt(
        1.1, true, 72);
    const auto negative = GhostarCircuitTestAccess::externalPitchAt(
        -1.1, true, 72);
    const auto noKey = GhostarCircuitTestAccess::externalPitchAt(
        0.0, true, -1);

    check(unplugged.glidedNote == 72.0,
          "an unplugged EXTERNAL PITCH jack keeps the keyboard target");
    check(std::abs(zero.glidedNote - pivot) < 1.0e-12
              && std::abs(positive.glidedNote - (pivot + 12.0)) < 1.0e-12
              && std::abs(negative.glidedNote - (pivot - 12.0)) < 1.0e-12,
          "inserted zero and +/-1.1 V map about P1016's cancellation pitch");
    check(zero.jackInserted && zero.storedSourceVolts == 0.0
              && zero.glidedNote != unplugged.glidedNote,
          "a plugged zero-volt cable is distinct from the normalled jack");
    check(unplugged.keyboardGate && zero.keyboardGate
              && unplugged.envelopeGate && zero.envelopeGate
              && !noKey.keyboardGate && !noKey.envelopeGate
              && std::abs(noKey.glidedNote - pivot) < 1.0e-12,
          "EXTERNAL PITCH replaces pitch without supplying a keyboard gate");
    check(GhostarCircuitTestAccess::externalPitchSetterPreservesKeyboardState(),
          "EXTERNAL PITCH changes neither key memory nor pending trigger paths");

    const auto arp =
        GhostarCircuitTestAccess::externalPitchArpeggiatorTargets();
    check(arp[0] == 84.0 && std::abs(arp[1] - pivot) < 1.0e-12,
          "an inserted EXTERNAL PITCH jack replaces the arpeggiator KCV target");
}

// P1013 distributes the post-Glide KCV to both ordinary oscillator ranges and
// KB AMOUNT. BASS/WIDE disconnect only Osc B, while FORMANT disconnects only
// the Lower filter.
void testExternalPitchReachesOnlyItsPhysicalKcvDestinations()
{
    using ghostar::GhostarCircuitTestAccess;
    using ghostar::OscBRange;
    using ghostar::TrackingMode;

    for (const auto range : { OscBRange::MinusOne, OscBRange::Unison,
                              OscBRange::PlusOne, OscBRange::PlusTwo })
    {
        const auto zero = GhostarCircuitTestAccess::externalPitchAt(
            0.0, true, 72, TrackingMode::Dynamic, 1.0f, range);
        const auto octave = GhostarCircuitTestAccess::externalPitchAt(
            1.1, true, 72, TrackingMode::Dynamic, 1.0f, range);
        check(std::abs(octave.oscillatorAOctaves
                       - zero.oscillatorAOctaves - 1.0) < 1.0e-12
                  && std::abs(octave.oscillatorBOctaves
                              - zero.oscillatorBOctaves - 1.0) < 1.0e-12,
              "EXTERNAL PITCH raises both oscillators in an octave range");
    }

    const auto downstreamZero = GhostarCircuitTestAccess::externalPitchAt(
        0.0, true, 72, TrackingMode::Dynamic, 1.0f,
        OscBRange::Unison, ghostar::MasterOctave::Eight, 0.0f);
    const auto downstreamShift = GhostarCircuitTestAccess::externalPitchAt(
        0.0, true, 72, TrackingMode::Dynamic, 1.0f,
        OscBRange::Unison, ghostar::MasterOctave::Four, 1.0f);
    constexpr double downstreamOctaves = 1.0 + 8.0 / 12.0;
    check(std::abs(downstreamShift.oscillatorAOctaves
                   - downstreamZero.oscillatorAOctaves
                   - downstreamOctaves) < 1.0e-12
              && std::abs(downstreamShift.oscillatorBOctaves
                          - downstreamZero.oscillatorBOctaves
                          - downstreamOctaves) < 1.0e-12
              && downstreamShift.upperCutoffHz
                     == downstreamZero.upperCutoffHz
              && downstreamShift.lowerCutoffHz
                     == downstreamZero.lowerCutoffHz,
          "OCTAVE and BEND stay downstream of KCV and filter tracking");

    const auto dynamicMinus = GhostarCircuitTestAccess::externalPitchAt(
        -1.1, true, 72, TrackingMode::Dynamic);
    const auto dynamicZero = GhostarCircuitTestAccess::externalPitchAt(
        0.0, true, 72, TrackingMode::Dynamic);
    const auto dynamicPlus = GhostarCircuitTestAccess::externalPitchAt(
        1.1, true, 72, TrackingMode::Dynamic);
    const double trackingOctave = std::exp2(1.083);
    check(std::abs(dynamicPlus.upperCutoffHz / dynamicZero.upperCutoffHz
                   - trackingOctave) < 1.0e-12
              && std::abs(dynamicZero.upperCutoffHz / dynamicMinus.upperCutoffHz
                          - trackingOctave) < 1.0e-12,
          "EXTERNAL PITCH drives Upper-filter keyboard tracking in both directions");
    check(std::abs(dynamicPlus.lowerCutoffHz / dynamicZero.lowerCutoffHz
                   - trackingOctave) < 1.0e-12
              && std::abs(dynamicZero.lowerCutoffHz / dynamicMinus.lowerCutoffHz
                          - trackingOctave) < 1.0e-12,
          "EXTERNAL PITCH drives Lower-filter KCV only in DYNAMIC");

    const auto formantZero = GhostarCircuitTestAccess::externalPitchAt(
        0.0, true, 72, TrackingMode::Formant);
    const auto formantPlus = GhostarCircuitTestAccess::externalPitchAt(
        1.1, true, 72, TrackingMode::Formant);
    check(std::abs(formantPlus.upperCutoffHz / formantZero.upperCutoffHz
                   - trackingOctave) < 1.0e-12
              && formantPlus.lowerCutoffHz == formantZero.lowerCutoffHz,
          "FORMANT freezes Lower KCV while Upper tracking stays connected");

    for (const auto range : { OscBRange::Bass, OscBRange::Wide })
    {
        const auto zero = GhostarCircuitTestAccess::externalPitchAt(
            0.0, true, 72, TrackingMode::Dynamic, 1.0f, range);
        const auto octave = GhostarCircuitTestAccess::externalPitchAt(
            1.1, true, 72, TrackingMode::Dynamic, 1.0f, range);
        check(std::abs(octave.oscillatorAOctaves
                       - zero.oscillatorAOctaves - 1.0) < 1.0e-12
                  && octave.oscillatorBDroneHz == zero.oscillatorBDroneHz,
              "BASS/WIDE disconnect Osc B but not Osc A from EXTERNAL PITCH");
    }
}

// P1017 C1 follows the selected source through R41||R42 (keyboard) or
// R1||R42 (external). P1/C6 then follows that voltage. AUTO's separate key
// detector enables only the Glide RC; key, jack and reset events discharge
// neither capacitor.
void testExternalPitchUsesThePhysicalGlideModes()
{
    const auto result =
        ghostar::GhostarCircuitTestAccess::externalPitchGlideModes();
    constexpr double pivot = 48.0 + 64.0 * 4.99 / 26.6;
    constexpr double sourceOhms = 15.0e3;
    constexpr double keyboardSourceOhms = 2.2e3;
    constexpr double inputOhms = 95.3e3;
    constexpr double inputFarads = 100.0e-9;
    constexpr double inputTau = sourceOhms * inputOhms
        / (sourceOhms + inputOhms) * inputFarads;
    const double inputCoefficient =
        1.0 - std::exp(-1.0 / (inputTau * 48000.0));
    constexpr double keyboardInputTau = keyboardSourceOhms * inputOhms
        / (keyboardSourceOhms + inputOhms) * inputFarads;
    const double keyboardInputCoefficient =
        1.0 - std::exp(-1.0 / (keyboardInputTau * 48000.0));
    const double glideCoefficient =
        1.0 - std::exp(-1.0 / (0.94 * 48000.0));
    const double inputFilteredOctave = pivot + inputCoefficient * 12.0;
    const double twiceFilteredOctave =
        pivot + glideCoefficient * inputCoefficient * 12.0;

    check(std::abs(result.offStep - inputFilteredOctave) < 1.0e-12,
          "OFF bypasses Glide but retains the 1.296 ms EXTERNAL PITCH input pole");
    check(std::abs(result.onStep - twiceFilteredOctave) < 1.0e-12,
          "ON cascades the input pole with the 2M-by-470n Glide RC");
    const double nodeAfterTwoSteps = inputCoefficient
        + inputCoefficient * (2.0 - inputCoefficient);
    check(std::abs(result.autoOneKeyStep - inputFilteredOctave) < 1.0e-12
              && std::abs(result.autoTwoKeyStep - twiceFilteredOctave) < 1.0e-12
              && std::abs(result.autoAfterDroppingToOneKey
                          - (pivot + 12.0 * nodeAfterTwoSteps)) < 1.0e-12,
          "AUTO Glide follows only the independent held-key detector");
    check(std::abs(result.internalNoteStep
                   - (60.0 + 12.0 * keyboardInputCoefficient)) < 1.0e-12,
          "a live normalled keyboard step retains C1 through R41||R42");

    const double insertedNode =
        48.0 + inputCoefficient * (pivot - 48.0);
    const double inserted =
        48.0 + glideCoefficient * (insertedNode - 48.0);
    const double removedNode = insertedNode
        + keyboardInputCoefficient * (48.0 - insertedNode);
    const double removed = inserted
        + glideCoefficient * (removedNode - inserted);
    check(result.beforeInsertion == 48.0
              && std::abs(result.afterInsertion - inserted) < 1.0e-12
              && std::abs(result.afterRemoval - removed) < 1.0e-12,
          "jack changes preserve C1 and Glide continuity");
    const double nodeAfterReset = inputCoefficient
        + inputCoefficient * (1.0 - inputCoefficient);
    const double expectedBeforeReset =
        pivot + 12.0 * glideCoefficient * inputCoefficient;
    const double expectedAfterResetTick = expectedBeforeReset
        + glideCoefficient
            * (pivot + 12.0 * nodeAfterReset - expectedBeforeReset);
    check(std::abs(result.beforeReset - expectedBeforeReset) < 1.0e-12
              && result.afterReset == result.beforeReset
              && std::abs(result.afterResetTick - expectedAfterResetTick)
                     < 1.0e-12,
          "reset preserves both P1017 C1 and the audible C6 Glide charge");
}

// OSC B PEDAL is a 0..100k shunt on P1013's +12 V/R192/C47 node. Rebuild
// the conductance equation here instead of sharing an engine helper: the
// nominal 25k trim midpoint is the only assumption in the 383k pitch arm.
void testOscBPedalMatchesItsDcAndRcNetwork()
{
    using ghostar::GhostarCircuitTestAccess;

    constexpr double pullupVolts = 12.0;
    constexpr double pullupOhms = 33.0e3;
    constexpr double pitchArmOhms = 383.0e3 + 0.5 * 25.0e3;
    constexpr double capacitance = 100.0e-9;
    const auto equilibrium = [=](double resistanceKOhm) {
        if (resistanceKOhm == 0.0)
            return 0.0;
        const double conductance = 1.0 / pullupOhms
            + 1.0 / pitchArmOhms
            + 1.0 / (resistanceKOhm * 1000.0);
        return (pullupVolts / pullupOhms) / conductance;
    };
    const double openConductance = 1.0 / pullupOhms
                                 + 1.0 / pitchArmOhms;
    const double openVolts = (pullupVolts / pullupOhms) / openConductance;

    for (const double resistance : { 0.0, 50.0, 100.0 })
    {
        const auto result = GhostarCircuitTestAccess::oscBPedalAt(
            true, resistance);
        check(std::abs(result.oscBNodeVolts - equilibrium(resistance))
                  < 1.0e-12,
              "OSC B PEDAL DC follows R192, C47 and the P1014 pitch arm");
    }

    const auto openAtZero = GhostarCircuitTestAccess::oscBPedalAt(
        false, 0.0);
    const auto openAtFull = GhostarCircuitTestAccess::oscBPedalAt(
        false, 100.0);
    check(openAtZero.oscBNodeVolts == openVolts
              && openAtFull.oscBNodeVolts == openVolts
              && openAtZero.oscillatorBOctaves
                     == openAtFull.oscillatorBOctaves,
          "an absent OSC B pedal is infinite resistance at every stored value");
    check(openAtFull.oscillatorAOctaves == 0.0
              && openAtFull.oscillatorBOctaves == 0.0,
          "the unplugged OSC B pedal equilibrium is tuning-neutral");

    const auto nan = GhostarCircuitTestAccess::oscBPedalAt(
        true, std::numeric_limits<double>::quiet_NaN());
    const auto infinity = GhostarCircuitTestAccess::oscBPedalAt(
        true, std::numeric_limits<double>::infinity());
    const auto below = GhostarCircuitTestAccess::oscBPedalAt(true, -1.0);
    const auto above = GhostarCircuitTestAccess::oscBPedalAt(true, 101.0);
    check(nan.oscBResistanceKOhm == 100.0
              && infinity.oscBResistanceKOhm == 100.0
              && below.oscBResistanceKOhm == 0.0
              && above.oscBResistanceKOhm == 100.0,
          "OSC B PEDAL sanitises non-finite resistance and clamps its travel");

    const auto retained = GhostarCircuitTestAccess::rearPedalRetention();
    const double closedConductance = openConductance + 1.0 / 100.0e3;
    const double closedVolts = (pullupVolts / pullupOhms)
                             / closedConductance;
    const double coefficient = -std::expm1(
        -closedConductance / (48000.0 * capacitance));
    const double first = openVolts
                       + coefficient * (closedVolts - openVolts);
    const double second = first
                        + coefficient * (closedVolts - first);
    check(std::abs(retained.oscBNodes[0] - openVolts) < 1.0e-12
              && std::abs(retained.oscBNodes[1] - first) < 1.0e-12
              && retained.oscBNodes[2] == retained.oscBNodes[1]
              && std::abs(retained.oscBNodes[3] - second) < 1.0e-12
              && retained.oscBNodes[4] == retained.oscBNodes[3],
          "C47 follows its exact RC and retains charge through reset and stop");
    check(retained.oscBNodes[5] == 0.0,
          "a zero-ohm OSC B pedal grounds C47 in one exact limiting step");
}

// The pedal arm terminates only at Osc B's CEM3340 pitch sum. It therefore
// leaves Osc A and both filters alone, but remains active when BASS/WIDE
// disconnect B from the keyboard bus.
void testOscBPedalReachesOnlyOscBInEveryRange()
{
    using ghostar::GhostarCircuitTestAccess;
    using ghostar::OscBRange;

    constexpr double pullupOhms = 33.0e3;
    constexpr double pitchArmOhms = 383.0e3 + 0.5 * 25.0e3;
    constexpr double sourceCurrent = 12.0 / pullupOhms;
    const auto node = [=](bool connected) {
        const double conductance = 1.0 / pullupOhms
            + 1.0 / pitchArmOhms
            + (connected ? 1.0 / 100.0e3 : 0.0);
        return sourceCurrent / conductance;
    };
    const double expectedOctaves = (node(true) - node(false))
                                 * 100.0e3 / pitchArmOhms;

    const auto open = GhostarCircuitTestAccess::oscBPedalAt(false, 100.0);
    const auto pressed = GhostarCircuitTestAccess::oscBPedalAt(true, 100.0);
    check(std::abs(pressed.oscillatorBOctaves
                   - open.oscillatorBOctaves - expectedOctaves) < 1.0e-12,
          "OSC B PEDAL adds its resistor-derived delta to Osc B pitch");
    check(pressed.oscillatorAOctaves == open.oscillatorAOctaves
              && pressed.upperCutoffHz == open.upperCutoffHz
              && pressed.lowerCutoffHz == open.lowerCutoffHz,
          "OSC B PEDAL reaches neither Osc A nor either filter");

    for (const OscBRange range : { OscBRange::Bass, OscBRange::Wide })
    {
        const auto droneOpen = GhostarCircuitTestAccess::oscBPedalAt(
            false, 100.0, range);
        const auto dronePressed = GhostarCircuitTestAccess::oscBPedalAt(
            true, 100.0, range);
        check(std::abs(std::log2(dronePressed.oscillatorBDroneHz
                                 / droneOpen.oscillatorBDroneHz)
                       - expectedOctaves) < 1.0e-12,
              "OSC B PEDAL remains connected in BASS and WIDE");
    }
}

// The service scan's J8/3/J8/4 crossing is ambiguous, so this pins the
// functional reduction chosen from the official passive-pedal contract:
// +12 V/R191/C48, P1017's 10k shunt, and one or two 100k virtual-earth arms.
void testFilterPedalMatchesItsDcAndRcNetwork()
{
    using ghostar::GhostarCircuitTestAccess;
    using ghostar::TrackingMode;

    constexpr double pullupVolts = 12.0;
    constexpr double pullupOhms = 33.0e3;
    constexpr double shuntOhms = 10.0e3;
    constexpr double inputOhms = 100.0e3;
    constexpr double capacitance = 100.0e-9;
    const auto equilibrium = [=](double resistanceKOhm, double arms) {
        if (resistanceKOhm == 0.0)
            return 0.0;
        const double conductance = 1.0 / pullupOhms + 1.0 / shuntOhms
            + arms / inputOhms + 1.0 / (resistanceKOhm * 1000.0);
        return (pullupVolts / pullupOhms) / conductance;
    };
    const auto openEquilibrium = [=](double arms) {
        const double conductance = 1.0 / pullupOhms + 1.0 / shuntOhms
                                 + arms / inputOhms;
        return (pullupVolts / pullupOhms) / conductance;
    };

    for (const auto [tracking, arms] : {
             std::pair { TrackingMode::Formant, 1.0 },
             std::pair { TrackingMode::Dynamic, 2.0 } })
    {
        for (const double resistance : { 0.0, 50.0, 100.0 })
        {
            const auto result = GhostarCircuitTestAccess::filterPedalAt(
                true, resistance, tracking);
            check(std::abs(result.nodeVolts
                           - equilibrium(resistance, arms)) < 1.0e-12,
                  "FILTER PEDAL DC follows its mode-selected conductance");
        }
    }

    const auto openAtZero = GhostarCircuitTestAccess::filterPedalAt(
        false, 0.0);
    const auto openAtFull = GhostarCircuitTestAccess::filterPedalAt(
        false, 100.0);
    check(openAtZero.nodeVolts == openEquilibrium(2.0)
              && openAtFull.nodeVolts == openEquilibrium(2.0)
              && openAtZero.upperCutoffHz == openAtFull.upperCutoffHz
              && openAtZero.lowerCutoffHz == openAtFull.lowerCutoffHz,
          "an absent FILTER pedal is infinite resistance and cutoff-neutral");

    const auto nan = GhostarCircuitTestAccess::filterPedalAt(
        true, std::numeric_limits<double>::quiet_NaN());
    const auto infinity = GhostarCircuitTestAccess::filterPedalAt(
        true, std::numeric_limits<double>::infinity());
    const auto below = GhostarCircuitTestAccess::filterPedalAt(true, -1.0);
    const auto above = GhostarCircuitTestAccess::filterPedalAt(true, 101.0);
    check(nan.resistanceKOhm == 100.0
              && infinity.resistanceKOhm == 100.0
              && below.resistanceKOhm == 0.0
              && above.resistanceKOhm == 100.0,
          "FILTER PEDAL sanitises non-finite resistance and clamps its travel");

    const auto retained = GhostarCircuitTestAccess::rearPedalRetention();
    const double openConductance = 1.0 / pullupOhms + 1.0 / shuntOhms
                                 + 2.0 / inputOhms;
    const double closedConductance = openConductance + 1.0 / 100.0e3;
    const double openVolts = (pullupVolts / pullupOhms) / openConductance;
    const double closedVolts = (pullupVolts / pullupOhms)
                             / closedConductance;
    const double coefficient = -std::expm1(
        -closedConductance / (48000.0 * capacitance));
    const double first = openVolts
                       + coefficient * (closedVolts - openVolts);
    const double second = first
                        + coefficient * (closedVolts - first);
    check(std::abs(retained.filterNodes[0] - openVolts) < 1.0e-12
              && std::abs(retained.filterNodes[1] - first) < 1.0e-12
              && retained.filterNodes[2] == retained.filterNodes[1]
              && std::abs(retained.filterNodes[3] - second) < 1.0e-12
              && retained.filterNodes[4] == retained.filterNodes[3],
          "C48 follows its exact RC and retains charge through reset and stop");
    check(retained.filterNodes[5] == 0.0,
          "a zero-ohm FILTER pedal grounds C48 in one exact limiting step");
}

// SW5 sends FILTER PEDAL to Upper in both positions and adds Lower only in
// DYNAMIC. The octave scale is the filter/oscillator CV-sensitivity ratio.
void testFilterPedalFollowsTheTrackingSwitch()
{
    using ghostar::GhostarCircuitTestAccess;
    using ghostar::TrackingMode;

    constexpr double pullupOhms = 33.0e3;
    constexpr double shuntOhms = 10.0e3;
    constexpr double inputOhms = 100.0e3;
    constexpr double sourceCurrent = 12.0 / pullupOhms;
    for (const auto [tracking, arms] : {
             std::pair { TrackingMode::Formant, 1.0 },
             std::pair { TrackingMode::Dynamic, 2.0 } })
    {
        const auto node = [=](bool connected) {
            const double conductance = 1.0 / pullupOhms
                + 1.0 / shuntOhms + arms / inputOhms
                + (connected ? 1.0 / 100.0e3 : 0.0);
            return sourceCurrent / conductance;
        };
        const double expectedOctaves = (node(true) - node(false))
                                     * (21.2 / 19.6);
        const auto open = GhostarCircuitTestAccess::filterPedalAt(
            false, 100.0, tracking);
        const auto pressed = GhostarCircuitTestAccess::filterPedalAt(
            true, 100.0, tracking);

        check(std::abs(std::log2(pressed.upperCutoffHz / open.upperCutoffHz)
                       - expectedOctaves) < 1.0e-12,
              "FILTER PEDAL reaches Upper in FORMANT and DYNAMIC");
        if (tracking == TrackingMode::Dynamic)
            check(std::abs(std::log2(pressed.lowerCutoffHz
                                     / open.lowerCutoffHz)
                           - expectedOctaves) < 1.0e-12,
                  "FILTER PEDAL reaches Lower in DYNAMIC");
        else
            check(pressed.lowerCutoffHz == open.lowerCutoffHz,
                  "FORMANT disconnects Lower from FILTER PEDAL");
        check(pressed.oscillatorAOctaves == open.oscillatorAOctaves
                  && pressed.oscillatorBOctaves == open.oscillatorBOctaves,
              "FILTER PEDAL reaches neither oscillator");
    }
}

// C48 itself must retain charge when SW5 changes the number of Filter-pedal
// arms. With no cable, that physical motion is also the open-jack baseline:
// comparing against an instantaneous new equilibrium used to invent a
// 0.1865-octave sweep. A real cable removal must still expose stored charge.
void testFilterPedalOpenReferenceTracksModeWithoutErasingCharge()
{
    using ghostar::GhostarCircuitTestAccess;

    constexpr double pullupVolts = 12.0;
    constexpr double pullupOhms = 33.0e3;
    constexpr double shuntOhms = 10.0e3;
    constexpr double inputOhms = 100.0e3;
    constexpr double capacitance = 100.0e-9;
    const auto openVolts = [=](double arms) {
        const double conductance = 1.0 / pullupOhms + 1.0 / shuntOhms
                                 + arms / inputOhms;
        return (pullupVolts / pullupOhms) / conductance;
    };
    const double priorImmediateArtifact =
        (openVolts(2.0) - openVolts(1.0)) * (21.2 / 19.6);

    const auto switched =
        GhostarCircuitTestAccess::unpluggedFilterPedalModeSwitch();
    check(std::abs(priorImmediateArtifact + 0.18651437443924834) < 1.0e-15,
          "the regression oracle reconstructs the former Filter-pedal sweep");
    check(switched.formantNodeDifference == 0.0
              && switched.dynamicNodeDifference == 0.0
              && switched.formantUpperCutoffDifference == 0.0
              && switched.formantLowerCutoffDifference == 0.0
              && switched.dynamicUpperCutoffDifference == 0.0
              && switched.dynamicLowerCutoffDifference == 0.0,
          "unplugged TRACKING changes remain bit-identical pedal-neutral");

    const auto removal = GhostarCircuitTestAccess::filterPedalRemoval();
    const double openConductance = 1.0 / pullupOhms + 1.0 / shuntOhms
                                 + 2.0 / inputOhms;
    const double closedConductance = openConductance + 1.0 / 100.0e3;
    const double expectedOpen = (pullupVolts / pullupOhms)
                              / openConductance;
    const double expectedClosed = (pullupVolts / pullupOhms)
                                / closedConductance;
    const double openCoefficient = -std::expm1(
        -openConductance / (48000.0 * capacitance));
    const double expectedRemoved = expectedClosed
        + openCoefficient * (expectedOpen - expectedClosed);
    check(std::abs(removal.connectedNode - expectedClosed) < 1.0e-12
              && removal.connectedOpenReference == expectedOpen
              && std::abs(removal.removedNode - expectedRemoved) < 1.0e-12
              && removal.removedOpenReference == expectedOpen
              && removal.removedNode != removal.removedOpenReference,
          "cable removal preserves C48's independently derived RC transient");
    check(removal.resetNode == removal.removedNode
              && removal.resetOpenReference
                     == removal.removedOpenReference
              && removal.stoppedNode == removal.removedNode
              && removal.stoppedOpenReference
                     == removal.removedOpenReference,
          "reset and stop retain real and counterfactual C48 charge");
}

// P1013 loads both linear cutoff pots at their wipers. This independently
// rebuilds the two divider/summer/node equations from the labelled parts so
// a convenient linear-in-octaves surrogate cannot pass the circuit suite.
void testFilterCutoffsFollowTheLoadedP1013Pots()
{
    using ghostar::GhostarCircuitTestAccess;
    using ghostar::TrackingMode;

    constexpr double centreHz = 565.685424949238;
    constexpr double voltsPerOctave = 0.0196;
    constexpr double masterMixerGain = 100.0 / 221.0;
    constexpr double lowerMixerGain = 100.0 / 150.0;
    constexpr double dynamicNodeGain =
        (1.0 / 12.1)
        / (2.0 / 12.1 + 1.0 / 16.0 + 1.0 / 0.274 + 1.0 / 68.0);
    constexpr double formantNodeGain =
        (1.0 / 12.1)
        / (1.0 / 12.1 + 1.0 / 34.1 + 1.0 / 16.0
           + 1.0 / 0.274 + 1.0 / 68.0);
    const auto masterVolts = [](double x) {
        return (24.0 * x - 12.0) * 110.5
             / (110.5 + 100.0 * x * (1.0 - x));
    };
    const auto lowerVolts = [](double x) {
        return -12.0 * (1.0 - x) * 150.0
             / (150.0 + 100.0 * x * (1.0 - x));
    };
    const double coincidenceVolts = lowerVolts(0.8);

    for (const double master : { 0.0, 0.2, 0.5, 0.8, 1.0 })
    {
        for (const TrackingMode mode : { TrackingMode::Dynamic,
                                         TrackingMode::Formant })
        {
            const double nodeGain = mode == TrackingMode::Dynamic
                ? dynamicNodeGain : formantNodeGain;
            for (const double lower : { 0.0, 0.4, 0.8, 1.0 })
            {
                const auto actual =
                    GhostarCircuitTestAccess::filterCutoffsForPanel(
                        static_cast<float>(master),
                        static_cast<float>(lower), mode);
                const double masterOctaves = dynamicNodeGain
                    * masterMixerGain * masterVolts(master)
                    / voltsPerOctave;
                const double expectedUpper =
                    centreHz * std::exp2(masterOctaves);
                const double lowerOffset = nodeGain * lowerMixerGain
                    * (lowerVolts(lower) - coincidenceVolts)
                    / voltsPerOctave;
                const double formantDrift = (nodeGain - dynamicNodeGain)
                    * masterMixerGain * masterVolts(master)
                    / voltsPerOctave;
                const double expectedLower = expectedUpper
                    * std::exp2(lowerOffset + formantDrift);
                check(std::abs(actual[0] / expectedUpper - 1.0) < 1.0e-6
                          && std::abs(actual[1] / expectedLower - 1.0)
                                 < 1.0e-6,
                      "MASTER and LOWER ONLY follow the loaded P1013 pots");
            }
        }
    }

    const auto dynamicLow = GhostarCircuitTestAccess::filterCutoffsForPanel(
        0.5f, 0.0f, TrackingMode::Dynamic);
    const auto dynamicHigh = GhostarCircuitTestAccess::filterCutoffsForPanel(
        0.5f, 1.0f, TrackingMode::Dynamic);
    check(std::abs(std::log2(dynamicLow[1] / dynamicLow[0]) + 7.1007)
              < 5.0e-4
              && std::abs(std::log2(dynamicHigh[1] / dynamicHigh[0])
                              - 1.5661)
                     < 5.0e-4,
          "LOWER ONLY preserves its asymmetric nominal endpoint span");

    const auto formantAtLowMaster =
        GhostarCircuitTestAccess::filterCutoffsForPanel(
            0.0f, 0.8f, TrackingMode::Formant);
    const auto formantAtHighMaster =
        GhostarCircuitTestAccess::filterCutoffsForPanel(
            1.0f, 0.8f, TrackingMode::Formant);
    check(std::log2(formantAtLowMaster[1] / formantAtLowMaster[0]) < 0.0
              && std::log2(formantAtHighMaster[1]
                           / formantAtHighMaster[0]) > 0.0,
          "FORMANT exposes its small MASTER-dependent coincidence drift");
}

// The owner's manual defines P1's full NORMAL/INVERT motion as a mirrored
// five-octave sweep about CUTOFF. The schematic proves the unusual four-lug
// topology, but does not publish its fixed-tap resistance or SW5's residual
// FORMANT loading, so this oracle pins the sourced transfer and no more.
void testFilterEnvelopeFollowsThePublishedFiveOctaveMirror()
{
    using ghostar::GhostarCircuitTestAccess;
    using ghostar::TrackingMode;

    const auto octaves = [](double envelope, float amount,
                            TrackingMode tracking, bool lower = false) {
        const auto base = GhostarCircuitTestAccess::
            filterCutoffsForFilterEnvelope(envelope, 0.5f, tracking);
        const auto moved = GhostarCircuitTestAccess::
            filterCutoffsForFilterEnvelope(envelope, amount, tracking);
        const std::size_t index = lower ? 1 : 0;
        return std::log2(moved[index] / base[index]);
    };

    check(std::abs(octaves(0.0, 1.0f, TrackingMode::Dynamic) + 2.5)
                  < 1.0e-12
              && std::abs(octaves(0.5, 1.0f, TrackingMode::Dynamic))
                     < 1.0e-12
              && std::abs(octaves(1.0, 1.0f, TrackingMode::Dynamic) - 2.5)
                     < 1.0e-12,
          "full NORMAL sweeps from 2.5 octaves below to 2.5 above CUTOFF");
    check(std::abs(octaves(0.0, 0.0f, TrackingMode::Dynamic) - 2.5)
                  < 1.0e-12
              && std::abs(octaves(0.5, 0.0f, TrackingMode::Dynamic))
                     < 1.0e-12
              && std::abs(octaves(1.0, 0.0f, TrackingMode::Dynamic) + 2.5)
                     < 1.0e-12,
          "full INVERT mirrors the published NORMAL sweep");
    check(std::abs(octaves(0.25, 0.75f, TrackingMode::Dynamic) + 0.625)
                  < 1.0e-12
              && std::abs(octaves(0.75, 0.75f, TrackingMode::Dynamic) - 0.625)
                     < 1.0e-12,
          "intermediate AMOUNT scales the mirrored envelope linearly");
    check(std::abs(octaves(0.25, 0.75f, TrackingMode::Dynamic)
                       - octaves(0.25, 0.75f, TrackingMode::Formant))
              < 1.0e-12,
          "no unsupported FORMANT loading curve is invented");
    check(std::abs(octaves(0.0, 1.0f, TrackingMode::Dynamic, true) + 2.5)
                  < 1.0e-12
              && std::abs(octaves(1.0, 1.0f,
                                  TrackingMode::Dynamic, true) - 2.5)
                     < 1.0e-12,
          "DYNAMIC sends the published envelope sweep to Lower too");
    check(std::abs(octaves(0.0, 1.0f, TrackingMode::Formant, true))
                  < 1.0e-12
              && std::abs(octaves(1.0, 1.0f,
                                  TrackingMode::Formant, true))
                     < 1.0e-12,
          "FORMANT disconnects the filter envelope from Lower");

    for (const auto tracking : { TrackingMode::Dynamic,
                                 TrackingMode::Formant })
        for (const double envelope : { 0.0, 0.5, 1.0 })
            check(std::abs(octaves(envelope, 0.5f, tracking)) < 1.0e-14,
                  "AMOUNT centre is zero throughout the envelope in both modes");
}

// R135/R136/R137 offset the Loudness CEM3360's linear-control pin so the
// first 0.5 V of the 7.5 V envelope produces no nominal gain. The factory
// 10k/4k7/240k network then reaches the production sheet's nominal 1.0 cell
// gain before the envelope peak under its 52%/V linear scale.
void testLoudnessVcaUsesItsControlOffset()
{
    const auto gain = [](double envelope) {
        return ghostar::GhostarCircuitTestAccess::
            loudnessGainForEnvelope(envelope);
    };
    constexpr double inputOhms = 10.0e3;
    constexpr double groundOhms = 4.7e3;
    constexpr double negativeOhms = 240.0e3;
    constexpr double referenceVolts = 7.5;
    constexpr double linearGainPerVolt = 0.52;
    constexpr double conductance = 1.0 / inputOhms + 1.0 / groundOhms
                                 + 1.0 / negativeOhms;
    const auto componentGain = [](double envelope) {
        const double controlVolts =
            (referenceVolts * envelope / inputOhms
             - 12.0 / negativeOhms) / conductance;
        return std::clamp(linearGainPerVolt * controlVolts, 0.0, 1.0);
    };
    constexpr double saturationEnvelope =
        (conductance / linearGainPerVolt + 12.0 / negativeOhms)
        * inputOhms / referenceVolts;

    check(gain(0.0) == 0.0,
          "the Loudness VCA clamps below its control offset");
    check(std::abs(gain(1.0 / 15.0)) < 1.0e-15,
          "the Loudness VCA opens at LC=0.5 V");
    check(std::abs(gain(0.5) - componentGain(0.5)) < 1.0e-15
              && std::abs(gain(0.5) - 0.5332363636363636) < 1.0e-15,
          "mid-envelope gain follows the independent 4k7 KCL and 52%/V law");
    check(std::abs(saturationEnvelope - 0.8793144208037824) < 1.0e-15
              && gain(saturationEnvelope - 1.0e-6) < 1.0
              && gain(saturationEnvelope + 1.0e-6) == 1.0,
          "the nominal CEM3360 reaches maximum gain at the derived envelope level");
    check(std::abs(gain(1.0) - 1.0) < 1.0e-15,
          "full envelope remains clamped to nominal maximum cell gain");
    check(ghostar::GhostarCircuitTestAccess::
              loudnessGainForEnvelope(0.0, true) == 1.0,
          "VCA BYPASS ignores the envelope control offset");
    const double discreteTail = ghostar::GhostarCircuitTestAccess::
        envelopeReleaseSecondsTo(1.0 / 15.0, 2.0e6 + 100.0, 8000.0);
    const double advertisedTail = GhostarEngine::longestReleaseTailSeconds();
    check(advertisedTail >= discreteTail
              && advertisedTail - discreteTail < 1.0e-3,
          "the advertised tail covers the 8 kHz D11 knee to VCA silence");
}

// SL3 and SL7 load one common D15-biased bottom rail. The provisional
// matched-silicon curve is calibrated so their combined 100k currents put
// that rail at the Loudness VCA's independently derived 0.5 V zero. D11/D14
// then make release a nonlinear series-diode discharge rather than e^-t/tau.
void testEnvelopeDiodeFloorAndReleaseKnee()
{
    constexpr double floorLevel = 1.0 / 15.0;
    constexpr double slopeVolts = 0.043;
    constexpr double saturationAmps = 1.2479467973540046e-9;
    constexpr double referenceVolts = 7.5;
    constexpr double sustainTrackOhms = 100.0e3;

    const double floorVolts = referenceVolts
        * ghostar::GhostarCircuitTestAccess::envelopeDecayTarget(0.0);
    const double trackCurrent =
        2.0 * (referenceVolts - floorVolts) / sustainTrackOhms;
    const double diodeCurrent = saturationAmps
        * std::expm1(floorVolts / slopeVolts);
    check(std::abs(floorVolts - 0.5) < 1.0e-14
              && std::abs(trackCurrent - diodeCurrent) < 1.0e-15,
          "the two sustain tracks satisfy their shared D15 floor KCL");
    check(std::abs(ghostar::GhostarCircuitTestAccess::
                       envelopeDecayTarget(0.25)
                   - (floorLevel + 0.25 * (1.0 - floorLevel)))
              < 1.0e-14,
          "each sustain target is affine above the common diode floor");

    constexpr double sampleRate = 48000.0;
    constexpr double capacitance = 4.7e-6;
    constexpr double resistance = 2.0e6 + 100.0;
    const double oldLevel = floorLevel;
    const double newLevel = ghostar::GhostarCircuitTestAccess::
        envelopeReleaseStep(oldLevel, resistance, sampleRate);
    const double h = 1.0 / sampleRate;
    const double current = capacitance * referenceVolts
        * (oldLevel - newLevel) / h;
    const double newVolts = referenceVolts * newLevel;
    const double kvl = resistance * current
        + slopeVolts * std::log1p(current / saturationAmps);
    const double pureRcDrop = oldLevel
        * (1.0 - std::exp(-h / (resistance * capacitance)));
    check(std::abs(newVolts - kvl) < 1.0e-10,
          "the backward-Euler release step satisfies D11's diode KVL");
    check(newLevel < oldLevel
              && oldLevel - newLevel < 0.7 * pureRcDrop,
          "the release slows into the original diode knee");

    constexpr double lowRate = 8000.0;
    constexpr double fastResistance = 1.0e3 + 100.0;
    const double fastOldLevel = 1.0;
    const double fastNewLevel = ghostar::GhostarCircuitTestAccess::
        envelopeReleaseStep(fastOldLevel, fastResistance, lowRate);
    const double fastH = 1.0 / lowRate;
    const double fastCurrent = capacitance * referenceVolts
        * (fastOldLevel - fastNewLevel) / fastH;
    const double fastKvl = fastResistance * fastCurrent
        + slopeVolts * std::log1p(fastCurrent / saturationAmps);
    check(fastNewLevel < fastOldLevel
              && std::abs(referenceVolts * fastNewLevel - fastKvl)
                     < 1.0e-10,
          "the diode solve remains bounded at 8 kHz and minimum release R");

    // Stress the implicit diode current from the nearly-open knee through a
    // hard, low-rate discharge. Every point independently closes the
    // backward-Euler capacitor KCL and the diode/resistor KVL.
    for (const auto [level, releaseOhms, rate] :
         { std::array { 1.0e-4, 2.0e6 + 100.0, 768000.0 },
           std::array { floorLevel, 1.0e5, 192000.0 },
           std::array { 0.5, 1.0e4, 44100.0 },
           std::array { 1.0, 1.0e3 + 100.0, 8000.0 } })
    {
        const double next = ghostar::GhostarCircuitTestAccess::
            envelopeReleaseStep(level, releaseOhms, rate);
        const double stepCurrent = capacitance * referenceVolts
            * (level - next) * rate;
        const double stepKvl = releaseOhms * stepCurrent
            + slopeVolts * std::log1p(stepCurrent / saturationAmps);
        check(next >= 0.0 && next <= level
                  && std::abs(referenceVolts * next - stepKvl) < 1.0e-10,
              "bracketed envelope-diode solve closes KVL across its domain");
    }
}

// P1015's selected X/Y edges use their 10 nF / 470 kOhm, ~5 ms reset lane.
// MULTIPLE KT and arpeggiator AA instead meet at the distinct R10=1 MOhm,
// C7=10 nF node annotated 10 ms. Both 4.7 uF caps follow their ordinary
// diode-release paths during either notch, then the final GS rise starts
// Attack from the retained voltage. KT is tapped before the KBD gate-select
// deck, while X/Y edge branches remain effective under an already-high bus.
// R23/R24=100 ohms also put the fast Attack cap peak below its threshold node.
void testEnvelopeRetriggerUsesThePhysicalResetNotch()
{
    const auto reset = ghostar::GhostarCircuitTestAccess::
        envelopeMultipleResetNotch();
    check(reset.releaseSamples == 80 && reset.stayedInRelease,
          "an 8 kHz MULTIPLE KT retrigger spends 80 samples in its nominal "
          "10 ms GS-low release notch");
    check(reset.afterNotch < reset.before && reset.afterNotch > 0.0,
          "the reset notch releases the cap without dumping its state");
    check(reset.restartedAttack && reset.afterRestart > reset.afterNotch,
          "the final GS rise attacks from the retained post-notch voltage");
    check(ghostar::GhostarCircuitTestAccess::
              xEdgeResetSamplesUnderHeldKeyboardGate() == 40,
          "an X rise beneath a held KBD gate uses the separate 5 ms lane");
    check(ghostar::GhostarCircuitTestAccess::
              multipleKeyResetSamplesWithKbdDeselected() == 80,
          "raw MULTIPLE KT reaches the 10 ms lane with KBD gate deselected");
    check(ghostar::GhostarCircuitTestAccess::
              arpeggiatorResetSamples(false) == 80,
          "each active arpeggiator AA step drives the 10 ms reset lane");
    check(ghostar::GhostarCircuitTestAccess::
              arpeggiatorResetSamples(true) == 80,
          "a coincident X edge cannot shorten AA's 10 ms reset notch");

    constexpr double expectedFastPeak =
        1.0 - 100.0 / 1000.0 * (1.3 - 1.0);
    check(std::abs(ghostar::GhostarCircuitTestAccess::minimumAttackCapPeak()
                   - expectedFastPeak) < 1.0e-12,
          "R23/R24 KVL puts the fast Attack cap peak below its 7.5 V "
          "threshold node");
}

// J2/7 is a switched jack, not a logical OR: an empty socket normals the
// Shaper's SG output to Y/EXT, while any inserted cable opens that contact.
// The following 4075 accepts only a voltage strictly above the manual's 6 V
// threshold, and its own edge lane remains visible under an already-high KBD.
void testExternalGateReplacesNormalledYAtTheStrictThreshold()
{
    using ghostar::GhostarCircuitTestAccess;

    const auto unplugged =
        GhostarCircuitTestAccess::externalGateWithInternalY(false, 0.0);
    check(unplugged.internalYHigh && unplugged.envelopeGate,
          "an unplugged EXTERNAL GATE jack normals high SG into Y/EXT");

    const auto insertedLow =
        GhostarCircuitTestAccess::externalGateWithInternalY(true, 0.0);
    check(insertedLow.internalYHigh && !insertedLow.envelopeGate,
          "an inserted low cable disconnects normalled SG");

    const auto atThreshold =
        GhostarCircuitTestAccess::externalGateWithInternalY(true, 6.0);
    check(atThreshold.internalYHigh && !atThreshold.envelopeGate,
          "exactly 6 V remains below the EXTERNAL GATE comparator");

    const auto aboveThreshold =
        GhostarCircuitTestAccess::externalGateWithInternalY(true, 6.000001);
    check(aboveThreshold.internalYHigh && aboveThreshold.envelopeGate,
          "a voltage above 6 V opens the selected Y/EXT gate");

    const auto falling =
        GhostarCircuitTestAccess::externalGateFallOverInternalY();
    check(falling.openBeforeFall && falling.internalYHighAfterFall
              && !falling.openAfterFall,
          "a falling external gate wins over concurrently high internal SG");

    check(GhostarCircuitTestAccess::
              externalGateEdgeResetSamplesUnderHeldKeyboard() == 40,
          "an external rise under held KBD uses the independent 5 ms lane");
}

// Reset and MIDI All Sound Off kill the voice, not the live rear-panel
// voltage. Holding an already-observed HIGH across either hard stop must not
// turn that unchanged level into another edge; LOW must re-arm it first.
void testExternalGateHardStopsRequireANewPhysicalEdge()
{
    using ghostar::GhostarCircuitTestAccess;

    for (const bool allSoundOff : { false, true })
    {
        const auto result =
            GhostarCircuitTestAccess::externalGateAcrossHardStop(allSoundOff);
        check(!result.restartedResetWhileHeldHigh,
              "a hard stop did not synthesize another Y/EXT reset edge");
        check(!result.attackedWhileHeldHigh,
              "a held external HIGH did not attack again after a hard stop");
        check(!result.restartedShaperWhileHeldHigh,
              "a hard stop did not retrigger RUN from an unchanged HIGH");
        check(result.resetSamplesAfterRealEdge == 40,
              "LOW-to-HIGH re-arms the external gate's 5 ms reset lane");
        check(result.attackedAfterRealEdge,
              "the external gate attacks after the real edge's reset notch");
        check(result.shaperStartedAfterRealEdge,
              "LOW-to-HIGH re-arms RUN's selected-gate edge detector");
    }
}

// The host jack is reconstructed onto the 4x circuit clock by the reverse
// B->A halfband cascade. Its exact impulse must be 4*hA*up2(hB): this pins
// stage order, interpolation gain, phase, group delay and steady DC without
// using either of the production convolution loops as the oracle.
void testExternalAudioReconstructionAndFrameTiming()
{
    const auto result =
        ghostar::GhostarCircuitTestAccess::externalAudioTiming();
    check(result.oracleError < 2.0e-15,
          "the external-audio reconstructor matches 4*hA*up2(hB)");
    check(std::abs(result.impulseSum - 4.0) < 2.0e-13,
          "the external-audio impulse has the four-phase unity DC gain");
    check(std::abs(result.impulseCentroid - 141.0) < 1.0e-11,
          "the external-audio impulse is centred on circuit tick 141");
    check(result.symmetryError < 2.0e-15,
          "the external-audio impulse remains linear phase");
    check(std::abs(result.passbandMagnitude - 1.0) < 2.0e-5,
          "the external-audio reconstructor lost unity at its 0.45 host-rate "
          "passband edge");
    check(result.firstImageMagnitude < std::pow(10.0, -98.0 / 20.0),
          "stage B rejects less than 98 dB at the first image edge");
    check(result.secondImageMagnitude < std::pow(10.0, -126.0 / 20.0),
          "stage A rejects less than 126 dB at the second image edge");
    check(result.dcError < 2.0e-15,
          "all four reconstructed DC phases match the cascade oracle");
    check(result.firstDelayedFrameTick == 141,
          "the complete internal-source frame meets external audio at tick "
          "141");
    check(ghostar::GhostarEngine::externalInputLatencyInternalSamples()
              == result.firstDelayedFrameTick,
          "the published input delay follows the real frame FIFO");
    check(result.jackStateCaptureDifference == 0.0,
          "jack presence was read live instead of from the delayed frame");
    check(result.jackSelectionDifference > 1.0e-6,
          "the delayed jack-state check cannot distinguish pink from external");
    check(result.brightnessTickZeroAlignmentDifference == 0.0,
          "BRIGHTNESS did not arrive with its matching tick-zero frame");
    check(result.brightnessTickZeroSelectionDifference > 1.0e-8,
          "the tick-zero BRIGHTNESS frame check cannot distinguish its two "
          "resistances");
    check(result.brightnessTickOneSelectionDifference > 1.0e-8,
          "the following BRIGHTNESS resistance did not arrive one tick "
          "after the tick-zero frame");
    check(result.brightnessCompanionMagnitude > 1.0e-8,
          "the delayed BRIGHTNESS companion check did not excite its branch");
    check(result.brightnessStateDifference == 0.0,
          "the BRIGHTNESS capacitor mixed delayed and live resistance");
}

// P1017 is a switching jack, not an extra mixer input. Empty, its normal
// contact selects IC4A pink regardless of voltage presented by the host;
// inserted, the tip replaces pink on both physical NOISE sliders, including
// the electrically important silent-cable case.
void testExternalAudioJackReplacesBothNoiseSliderSources()
{
    using ghostar::GhostarCircuitTestAccess;
    constexpr double pink = 0.375;

    const auto normalled = GhostarCircuitTestAccess::externalAudioMixerProbe(
        false, 0.0, pink);
    const auto ignoredHost =
        GhostarCircuitTestAccess::externalAudioMixerProbe(
            false, -0.8125, pink);
    const auto insertedSame =
        GhostarCircuitTestAccess::externalAudioMixerProbe(
            true, pink, -0.9);
    const auto insertedSilent =
        GhostarCircuitTestAccess::externalAudioMixerProbe(
            true, 0.0, pink);

    check(normalled.filterEnergy > 1.0e-12
              && normalled.shaperEnergy > 1.0e-12,
          "the normal contact feeds IC4A pink to both NOISE sliders");
    check(ignoredHost.filterEnergy == normalled.filterEnergy
              && ignoredHost.shaperEnergy == normalled.shaperEnergy,
          "an unplugged jack ignores the unrouted host signal");
    check(insertedSame.filterEnergy == normalled.filterEnergy
              && insertedSame.shaperEnergy == normalled.shaperEnergy,
          "the inserted tip replaces pink at both slider inputs");
    check(insertedSame.filterSum == normalled.filterSum
              && insertedSame.shaperSum == normalled.shaperSum,
          "the inserted tip changed the normal contact's signal polarity");
    check(insertedSilent.filterEnergy == 0.0
              && insertedSilent.shaperEnergy == 0.0,
          "an inserted silent cable disconnects rather than normalising");
}

// Exercise the public block API, not only its two internal helpers: a signed
// impulse must actually traverse reconstruction, the 4x mixer and output
// decimation with their measured aggregate latency and unchanged polarity.
void testExternalAudioProcessPathIsSignedAndAligned()
{
    const auto result =
        ghostar::GhostarCircuitTestAccess::externalAudioProcessImpulse();
    check(std::abs(result.impulseSum - 0.45) < 1.0e-6,
          "the public External Audio path changed gain or polarity");
    check(std::abs(result.impulseCentroid - 69.75) < 2.0e-5,
          "the public External Audio path bypassed or shifted a halfband");
    check(result.polarityError == 0.0,
          "positive and negative External Audio impulses lost symmetry");
}

// The audible IC4A branch is downstream of the R6/C8 red-noise tap. Changing
// the switched audio contact must leave both that state and the modulation it
// produces bit-identical, while the modulation itself remains demonstrably
// alive.
void testExternalAudioJackLeavesRedNoiseUpstream()
{
    const auto result = ghostar::GhostarCircuitTestAccess::
        externalAudioKeepsRedNoiseUpstream();
    check(result.cutoffSpan > 1.0,
          "RED NOISE still modulates the filter with external audio inserted");
    check(result.jackCutoffDifference == 0.0,
          "the audio jack does not alter upstream RED NOISE modulation");
    check(result.jackRedNoiseDifference == 0.0,
          "the audio jack does not alter the shared MM5837 red branch");
}

// Host buffers are outside the analogue model's trust boundary. Every
// non-finite or subnormal sample is electrically zero before either FIR sees
// it, nullptr means the same silent cable, and reset removes every remembered
// external sample from both reconstruction and downstream circuit state.
void testExternalAudioInputSanitisesAndResets()
{
    constexpr int samples = 512;
    using Mono = std::array<float, samples>;
    using Stereo = std::array<Mono, 2>;

    ghostar::EngineParameters parameters;
    parameters.masterVolume = 1.0f;
    parameters.brightness = 1.0f;
    parameters.splitPaths = true;
    parameters.filterPathA = 0.0f;
    parameters.filterPathB = 0.0f;
    parameters.filterPathNoise = 1.0f;
    parameters.shaperPathA = 0.0f;
    parameters.shaperPathB = 0.0f;
    parameters.shaperPathRing = 0.0f;
    parameters.shaperPathNoise = 1.0f;
    parameters.vcaBypass = true;

    const auto prepare = [&parameters](ghostar::GhostarEngine& engine) {
        engine.prepare(48000.0, samples);
        engine.setParameters(parameters);
        engine.setExternalAudioInput(true);
        engine.reset();
    };
    const auto render = [&prepare](const float* input) {
        ghostar::GhostarEngine engine;
        prepare(engine);
        Stereo output {};
        engine.process(input, output[0].data(), output[1].data(), samples);
        return output;
    };
    const auto maximumDifference = [](const Stereo& a, const Stereo& b) {
        double difference = 0.0;
        for (std::size_t channel = 0; channel < a.size(); ++channel)
            for (std::size_t sample = 0; sample < a[channel].size(); ++sample)
                difference = std::max(
                    difference,
                    std::abs(static_cast<double>(a[channel][sample])
                             - static_cast<double>(b[channel][sample])));
        return difference;
    };
    const auto finite = [](const Stereo& output) {
        for (const auto& channel : output)
            for (const float sample : channel)
                if (!std::isfinite(sample))
                    return false;
        return true;
    };

    Mono zero {};
    const auto reference = render(zero.data());
    check(maximumDifference(reference, render(nullptr)) == 0.0,
          "a connected jack with no host bus is the same as zero volts");

    const std::array<float, 4> hostile {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::denorm_min(),
    };
    for (const float value : hostile)
    {
        Mono input {};
        input[17] = value;
        const auto output = render(input.data());
        check(finite(output),
              "a hostile external-audio sample produced non-finite output");
        check(maximumDifference(reference, output) == 0.0,
              "a hostile external-audio sample entered or persisted in an "
              "FIR");
    }
    check(ghostar::GhostarCircuitTestAccess::externalAudioHistoryMagnitude(
              std::numeric_limits<float>::denorm_min()) == 0.0,
          "a float subnormal remained hidden in external-audio FIR state");
    check(ghostar::GhostarCircuitTestAccess::externalAudioHistoryMagnitude(
              std::numeric_limits<float>::min()) > 0.0,
          "sanitising subnormals also discarded the smallest normal float");

    const auto resetHistory =
        ghostar::GhostarCircuitTestAccess::externalAudioResetHistory();
    check(resetHistory.stageBBeforeReset > 0.0
              && resetHistory.stageABeforeReset > 0.0,
          "external reconstruction histories were not live before reset");
    check(resetHistory.stageBAfterReset == 0.0
              && resetHistory.stageAAfterReset == 0.0,
          "reset did not clear both external reconstruction histories");

    ghostar::GhostarEngine used;
    prepare(used);
    Mono impulse {};
    impulse[0] = 1.0f;
    Stereo discarded {};
    used.process(impulse.data(), discarded[0].data(), discarded[1].data(),
                 samples);
    used.reset();
    Stereo afterReset {};
    used.process(zero.data(), afterReset[0].data(), afterReset[1].data(),
                 samples);

    ghostar::GhostarEngine fresh;
    prepare(fresh);
    Stereo freshOutput {};
    fresh.process(zero.data(), freshOutput[0].data(), freshOutput[1].data(),
                  samples);
    check(maximumDifference(afterReset, freshOutput) == 0.0,
          "reset clears external reconstruction, frame and output memory");
}

// MOD RATE's 100k linear P2 is loaded by R33=200k before the exponential
// CEM3360 converter. P1015 gives it 132 mV of travel; the original production
// sheet specifies 3.0 mV/dB typical, hence 44 dB. Electrical half travel is
// 4/9 of that source-derived span rather than 1/2.
void testLfoRateIncludesItsLoadedPot()
{
    const double slow =
        ghostar::GhostarCircuitTestAccess::lfoHzForTravel(0.0f);
    const double middle =
        ghostar::GhostarCircuitTestAccess::lfoHzForTravel(0.5f);
    const double fast =
        ghostar::GhostarCircuitTestAccess::lfoHzForTravel(1.0f);
    constexpr double controlTravelMillivolts = 132.0;
    constexpr double typicalMillivoltsPerDb = 3.0;
    constexpr double fastHz = 50.0;
    const double spanDb =
        controlTravelMillivolts / typicalMillivoltsPerDb;
    const double expectedSlow = fastHz * std::pow(10.0, -spanDb / 20.0);
    const double expectedMiddle = expectedSlow
        * std::pow(fastHz / expectedSlow, 4.0 / 9.0);

    check(std::abs(slow - expectedSlow) < 1.0e-12,
          "MOD RATE derives its slow endpoint from the CEM3360's 3 mV/dB "
          "scale and P1015's 132 mV span");
    check(std::abs(fast - fastHz) < 1.0e-12,
          "MOD RATE retains the manual's 50 Hz endpoint");
    check(std::abs(middle - expectedMiddle) < 1.0e-12,
          "MOD RATE includes P2's R33-loaded linear travel");
}

// P1016 qualifies each raw KT pulse with barred AA, then Q2 clamps the
// visible TL068 triangle output for KT's annotated 25 us. C13 itself is not
// discharged: it keeps charging upward while IC10B is forced to LG high.
// A reset entered from the falling leg therefore clocks S&H once, and an
// above-threshold hidden charge is recovered after release without wrapping.
void testKeyboardLfoResetRetainsC13BehindItsOutputClamp()
{
    using ghostar::GhostarCircuitTestAccess;
    using ghostar::ShaperMode;

    const auto falling =
        GhostarCircuitTestAccess::keyboardLfoResetFrom(0.999, false);
    check(falling.clampedSamples == 1
              && falling.visibleDuringClamp == -1.0,
          "annotated KT clamps the visible LFO bus for 25 us at 40 kHz");
    check(std::abs(falling.capacitorAfterClamp - 1.004) < 1.0e-12,
          "C13 retains and accumulates charge behind the Q2 clamp");
    check(std::abs(falling.sampleHoldAfterFirstStep - 0.73) < 1.0e-12,
          "forcing LG high from the falling leg clocks the sample-and-hold");
    check(!falling.risingAfterRelease
              && std::abs(falling.capacitorAfterRelease - 0.999) < 1.0e-12
              && falling.visibleAfterRelease < 1.0,
          "reset release recovers C13 overshoot without phase reflection");

    const auto rising =
        GhostarCircuitTestAccess::keyboardLfoResetFrom(-0.2, true);
    check(rising.clampedSamples == 1
              && std::abs(rising.capacitorAfterClamp + 0.195) < 1.0e-12
              && std::abs(rising.capacitorAfterRelease + 0.19) < 1.0e-12,
          "reset preserves an already-rising C13 trajectory");
    check(std::abs(rising.sampleHoldAfterFirstStep - 0.11) < 1.0e-12,
          "an already-high LG state does not invent another clock edge");

    const auto arpeggiating =
        GhostarCircuitTestAccess::keyboardLfoResetWithArpeggiator();
    check(arpeggiating.clampedSamples == 1
              && std::abs(arpeggiating.capacitorAfterClamp - 0.205)
                     < 1.0e-12
              && arpeggiating.visibleDuringClamp == -1.0,
          "an arpeggiator selector alone does not suppress raw KT");
    check(std::abs(arpeggiating.sampleHoldAfterFirstStep - 0.73) < 1.0e-12,
          "a reset-forced LG rise still clocks S&H with arpeggiation selected");

    const auto partial =
        GhostarCircuitTestAccess::keyboardLfoResetFrom(0.2, false, 8000.0);
    check(partial.clampedSamples == 1
              && std::abs(partial.capacitorAfterClamp - 0.225) < 1.0e-12
              && std::abs(partial.visibleDuringClamp + 0.028) < 1.0e-12,
          "a sub-sample KT keeps exact C13 charge and visible clamp average");
    const auto corner =
        GhostarCircuitTestAccess::keyboardLfoResetFrom(0.99, false, 8000.0);
    check(std::abs(corner.capacitorAfterClamp - 0.985) < 1.0e-12
              && std::abs(corner.visibleDuringClamp - 0.595) < 1.0e-12
              && !corner.risingAfterRelease,
          "the clamp average integrates a post-release reversal in two legs");

    for (const double sampleRate : { 8000.0, 44100.0, 48000.0,
                                     96000.0, 768000.0 })
        check(std::abs(GhostarCircuitTestAccess::
                           integratedLfoKtDuration(sampleRate)
                       - 25.0e-6) < 1.0e-15,
              "fractional KT integration preserves 25 us at every host rate");

    for (const auto [sampleRate, expectedResetSamples] : {
             std::pair { 8000.0, 40 }, std::pair { 32000.0, 160 } })
    {
        const auto gateX = GhostarCircuitTestAccess::
            subSampleLfoResetThroughGateX(sampleRate);
        check(!gateX.finalSquareHigh,
              "the Gate-X regression setup ends below the host sampling grid");
        check(gateX.envelopeResetSamples == expectedResetSamples,
              "a sub-sample LG rise reaches Gate X's 5 ms envelope-reset lane");
        check(gateX.shaperCycleActive,
              "a sub-sample LG rise reaches the Shaper's selected Gate-X input");

        const auto hold = GhostarCircuitTestAccess::
            subSampleLfoResetThroughGateX(sampleRate, ShaperMode::KbdHold);
        check(!hold.finalSquareHigh && hold.shaperLevel > 0.015
                  && hold.shaperLevel < 0.03,
              "KBD HOLD integrates the sub-sample Gate-X high time");
    }
}

// P1013 makes X a current-driven 100k rheostat, whereas Y is a voltage-fed
// 100k divider behind R60=15k. RS1/RS2 then change the wiper load: one
// oscillator is 22k||100k, two are 22k||100k||100k, one filter is 100k and
// two filters are 50k. These equations independently pin both the strikingly
// different wheel travels and the weaker shared-destination settings.
void testModulationWheelsIncludeDestinationLoading()
{
    const auto parallel = [](double a, double b) {
        return a * b / (a + b);
    };
    constexpr double wheel = 100.0;
    constexpr double sourceY = 15.0;
    constexpr double dutyDepth = 0.42;
    const double oscillatorSingle = parallel(22.0, 100.0);
    const double oscillatorPair = parallel(oscillatorSingle, 100.0);
    const double rwmLoad = parallel(200.0, 620.0);
    const double xReference = parallel(wheel, oscillatorSingle);
    const double yReference = xReference / (sourceY + xReference);
    constexpr double filterSensitivity = 21.2 / 19.6;
    const auto expectedX = [&](double travel, double load) {
        return parallel(wheel * travel, load) / xReference;
    };
    const auto expectedY = [&](double travel, double load) {
        const double lower = parallel(wheel * travel, load);
        return lower / (sourceY + wheel * (1.0 - travel) + lower)
             / yReference;
    };
    const auto expectedRwmX = [&](double travel) {
        return parallel(wheel * travel, rwmLoad)
             / parallel(wheel, rwmLoad);
    };
    const auto expectedRwmY = [&](double travel) {
        const double lower = parallel(wheel * travel, rwmLoad);
        const double fullLower = parallel(wheel, rwmLoad);
        return lower / (sourceY + wheel * (1.0 - travel) + lower)
             / (fullLower / (sourceY + fullLower));
    };
    const auto close = [](double measured, double expected) {
        return std::abs(measured - expected) < 1.0e-12;
    };

    using ghostar::GhostarCircuitTestAccess;
    using ghostar::ModXDestination;
    using ghostar::ShaperYDestination;

    const auto xAHalf = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::OscA, ShaperYDestination::Off, 0.5f, 0.0f);
    const auto xAFull = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::OscA, ShaperYDestination::Off, 1.0f, 0.0f);
    check(close(xAHalf.oscillatorAOctaves,
                expectedX(0.5, oscillatorSingle))
              && close(xAFull.oscillatorAOctaves, 1.0),
          "MOD X's current-driven wheel has its loaded rheostat travel");

    const auto xAB = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::OscAB, ShaperYDestination::Off, 1.0f, 0.0f);
    const auto xABHalf = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::OscAB, ShaperYDestination::Off, 0.5f, 0.0f);
    const double expectedXPair = expectedX(1.0, oscillatorPair);
    check(close(xAB.oscillatorAOctaves, expectedXPair)
              && close(xAB.oscillatorBOctaves, expectedXPair)
              && close(xABHalf.oscillatorAOctaves,
                       expectedX(0.5, oscillatorPair)),
          "X to A+B includes both oscillator input loads");

    const auto xU = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::FilterU, ShaperYDestination::Off, 1.0f, 0.0f);
    const auto xUHalf = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::FilterU, ShaperYDestination::Off, 0.5f, 0.0f);
    const auto xUL = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::FilterUL, ShaperYDestination::Off, 1.0f, 0.0f);
    const auto xULHalf = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::FilterUL, ShaperYDestination::Off, 0.5f, 0.0f);
    check(close(xU.upperOctaves,
                expectedX(1.0, 100.0) * filterSensitivity)
              && close(xUHalf.upperOctaves,
                       expectedX(0.5, 100.0) * filterSensitivity)
              && close(xU.lowerOctaves, 0.0),
          "X to U includes the single filter load and CV sensitivity");
    const double expectedXFilters =
        expectedX(1.0, 50.0) * filterSensitivity;
    check(close(xUL.upperOctaves, expectedXFilters)
              && close(xUL.lowerOctaves, expectedXFilters)
              && close(xULHalf.upperOctaves,
                       expectedX(0.5, 50.0) * filterSensitivity),
          "X to U+L includes both parallel filter input loads");

    const auto yBHalf = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::Off, ShaperYDestination::OscB, 0.0f, 0.5f);
    const auto yBFull = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::Off, ShaperYDestination::OscB, 0.0f, 1.0f);
    check(close(yBHalf.oscillatorBOctaves,
                expectedY(0.5, oscillatorSingle))
              && close(yBFull.oscillatorBOctaves, 1.0),
          "SHAPER Y's voltage-fed wheel has its loaded divider travel");

    const auto yAB = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::Off, ShaperYDestination::OscAB, 0.0f, 1.0f);
    const auto yABHalf = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::Off, ShaperYDestination::OscAB, 0.0f, 0.5f);
    const double expectedYPair = expectedY(1.0, oscillatorPair);
    check(close(yAB.oscillatorAOctaves, expectedYPair)
              && close(yAB.oscillatorBOctaves, expectedYPair)
              && close(yABHalf.oscillatorAOctaves,
                       expectedY(0.5, oscillatorPair)),
          "Y to A+B includes both oscillator input loads");

    const auto yL = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::Off, ShaperYDestination::FilterL, 0.0f, 1.0f);
    const auto yLHalf = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::Off, ShaperYDestination::FilterL, 0.0f, 0.5f);
    check(close(yL.upperOctaves, 0.0)
              && close(yL.lowerOctaves,
                       expectedY(1.0, 100.0) * filterSensitivity)
              && close(yLHalf.lowerOctaves,
                       expectedY(0.5, 100.0) * filterSensitivity),
              "Y to L includes the filter load and CV sensitivity");

    const auto xRwmHalf = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::OscARwm, ShaperYDestination::Off, 0.5f, 0.0f);
    const auto xRwmFull = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::OscARwm, ShaperYDestination::Off, 1.0f, 0.0f);
    check(close(xRwmHalf.pwmA, dutyDepth * expectedRwmX(0.5))
              && close(xRwmFull.pwmA, dutyDepth),
          "X RWM includes its 200k||620k load without moving full depth");

    const auto yRwmHalf = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::Off, ShaperYDestination::OscBRwm, 0.0f, 0.5f);
    const auto yRwmFull = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::Off, ShaperYDestination::OscBRwm, 0.0f, 1.0f);
    check(close(yRwmHalf.pwmB, dutyDepth * expectedRwmY(0.5))
              && close(yRwmFull.pwmB, dutyDepth),
          "Y RWM includes its 200k||620k load without moving full depth");

    const auto audioAB = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::OscAB, ShaperYDestination::Off,
        0.5f, 0.0f, true);
    const double expectedAudioPair = expectedX(0.5, oscillatorPair);
    check(audioAB.audioActive
              && close(audioAB.audioGain * audioAB.audioAOctaves,
                       expectedAudioPair)
              && close(audioAB.audioGain * audioAB.audioBOctaves,
                       expectedAudioPair),
              "audio-rate OSC B modulation uses the same loaded X network");

    const auto audioRwm = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::OscARwm, ShaperYDestination::Off,
        0.5f, 0.0f, true);
    check(audioRwm.audioActive
              && close(audioRwm.audioGain * audioRwm.audioDuty,
                       dutyDepth * expectedRwmX(0.5)),
          "audio-rate X RWM uses the same loaded rheostat travel");

    const auto audioUL = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::FilterUL, ShaperYDestination::Off,
        0.5f, 0.0f, true);
    const double expectedAudioFilters =
        expectedX(0.5, 50.0) * filterSensitivity;
    check(audioUL.audioActive
              && close(audioUL.audioGain * audioUL.audioUpperOctaves,
                       expectedAudioFilters)
              && close(audioUL.audioGain * audioUL.audioLowerOctaves,
                       expectedAudioFilters),
          "audio-rate filter modulation uses the same loaded X network");

    const auto formantUL = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::FilterUL, ShaperYDestination::Off,
        1.0f, 0.0f, false, false);
    const auto audioFormantUL = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::FilterUL, ShaperYDestination::Off,
        1.0f, 0.0f, true, false);
    check(close(formantUL.upperOctaves, expectedXFilters)
              && close(formantUL.lowerOctaves, 0.0)
              && audioFormantUL.audioActive
              && close(audioFormantUL.audioUpperOctaves, expectedXFilters)
              && close(audioFormantUL.audioLowerOctaves, 0.0),
          "FORMANT removes the Lower from both modulation-rate paths");

    const auto audioZero = GhostarCircuitTestAccess::modulationAt(
        ModXDestination::OscAB, ShaperYDestination::Off,
        0.0f, 0.0f, true);
    check(!audioZero.audioActive,
          "a fully backed-off X wheel disables audio-rate modulation");
}

// P1013 takes RED NOISE continuously from the MM5837 R6/C8 junction through
// IC4B. Unlike either S+H detent, it has no clocked hold, and its resolved
// transfer still has an 8.16 kHz pole. It must therefore reach every MOD X
// destination on the 4x circuit grid, from the same source tick as IC4A audio.
void testRedNoiseModulationRunsOnTheCircuitGrid()
{
    using ghostar::GhostarCircuitTestAccess;
    using ghostar::ModSource;
    using ghostar::ModXDestination;

    const auto span = [](const auto& values) {
        const auto [minimum, maximum] = std::minmax_element(
            values.begin(), values.end());
        return *maximum - *minimum;
    };
    const auto live = [&](const auto& values) {
        return span(values) > 1.0e-10;
    };
    const auto parallel = [](double a, double b) {
        return a * b / (a + b);
    };
    constexpr double wheel = 100.0;
    const double oscillatorSingle = parallel(22.0, 100.0);
    const double oscillatorPair = parallel(oscillatorSingle, 100.0);
    const double xReference = parallel(wheel, oscillatorSingle);
    const auto xGain = [&](double load) {
        return parallel(wheel, load) / xReference;
    };
    constexpr double filterSensitivity = 21.2 / 19.6;
    const auto matchesRoute = [](const auto& probe, const auto& actual,
                                 double base, double gain) {
        for (std::size_t tick = 0; tick < actual.size(); ++tick)
        {
            const double red = std::clamp(
                probe.referenceRed[tick] * 0.26, -1.0, 1.0);
            if (std::abs(actual[tick] - (base + red * gain)) > 1.0e-14)
                return false;
        }
        return true;
    };

    const auto checkSharedSource = [&](const auto& probe) {
        check(probe.audioActive && probe.sourceMatches,
              "continuous RED NOISE publishes an audio-rate MOD X route");
        check(probe.sharedAudioError < 1.0e-15
                  && probe.sharedRedError < 1.0e-15,
              "RED modulation and PINK audio share one MM5837 circuit tick");
        check(live(probe.referenceRed),
              "the RED branch changes inside one four-tick host sample");
    };

    const auto oscillators = GhostarCircuitTestAccess::modSourceTicks(
        ModSource::RedNoise, ModXDestination::OscAB);
    checkSharedSource(oscillators);
    const double oscillatorPairGain = xGain(oscillatorPair);
    check(matchesRoute(oscillators, oscillators.pitchA,
                       oscillators.basePitchA, oscillatorPairGain)
              && matchesRoute(oscillators, oscillators.pitchB,
                              oscillators.basePitchB, oscillatorPairGain),
          "each RED tick reaches both oscillator pitches with signed X gain");

    const auto oscillatorA = GhostarCircuitTestAccess::modSourceTicks(
        ModSource::RedNoise, ModXDestination::OscA);
    checkSharedSource(oscillatorA);
    check(matchesRoute(oscillatorA, oscillatorA.pitchA,
                       oscillatorA.basePitchA, xGain(oscillatorSingle))
              && matchesRoute(oscillatorA, oscillatorA.pitchB,
                              oscillatorA.basePitchB, 0.0),
          "each RED tick reaches only Osc A with signed X gain");

    const auto pulseWidth = GhostarCircuitTestAccess::modSourceTicks(
        ModSource::RedNoise, ModXDestination::OscARwm);
    checkSharedSource(pulseWidth);
    check(matchesRoute(pulseWidth, pulseWidth.dutyA,
                       pulseWidth.baseDutyA, 0.42),
          "each RED tick reaches Osc A width with signed wheel gain");

    const auto filters = GhostarCircuitTestAccess::modSourceTicks(
        ModSource::RedNoise, ModXDestination::FilterUL);
    checkSharedSource(filters);
    const double filterPairGain = xGain(50.0) * filterSensitivity;
    check(matchesRoute(filters, filters.upperFilter, 0.0, filterPairGain)
              && matchesRoute(filters, filters.lowerFilter,
                              0.0, filterPairGain),
          "each RED tick reaches both filters with signed X gain");

    const auto upperFilter = GhostarCircuitTestAccess::modSourceTicks(
        ModSource::RedNoise, ModXDestination::FilterU);
    checkSharedSource(upperFilter);
    const double filterSingleGain = xGain(100.0) * filterSensitivity;
    check(matchesRoute(upperFilter, upperFilter.upperFilter,
                       0.0, filterSingleGain)
              && matchesRoute(upperFilter, upperFilter.lowerFilter,
                              0.0, 0.0),
          "each RED tick reaches only Upper with signed X gain");

    const auto held = GhostarCircuitTestAccess::sampleHoldRedClockEdge();
    const double expectedHeld = std::clamp(
        held.availableReferenceRed * 0.26, -1.0, 1.0);
    check(std::abs(held.availableEngineRed - held.availableReferenceRed)
                  < 1.0e-15
              && std::abs(held.captured - expectedHeld) < 1.0e-15,
          "the S+H clock captures the RED value available at its edge");
    bool laterSourceMatches = true;
    bool laterSourceMoves = false;
    bool sampleStaysHeld = true;
    for (std::size_t tick = 0; tick < held.held.size(); ++tick)
    {
        laterSourceMatches = laterSourceMatches
            && std::abs(held.laterEngineRed[tick]
                        - held.laterReferenceRed[tick]) < 1.0e-15;
        laterSourceMoves = laterSourceMoves
            || std::abs(held.laterReferenceRed[tick]
                        - held.availableReferenceRed) > 1.0e-10;
        sampleStaysHeld = sampleStaysHeld
            && held.held[tick] == held.captured;
    }
    check(laterSourceMatches && laterSourceMoves && sampleStaysHeld,
          "later 4x RED ticks advance while S+H keeps its edge value");
}

// Lossless DWG 1 resolves C6=470n and P1=2M, hence tau=0.94 s at full
// resistance. The unmarked travel taper remains the separate OQ-08 voicing.
void testFullGlideUsesTheResolvedRcEndpoint()
{
    const double coefficient = 1.0 - std::exp(-1.0 / (0.94 * 48000.0));
    const double expected = 48.0 + coefficient * 12.0;
    check(std::abs(
              ghostar::GhostarCircuitTestAccess::fullGlideAfterOneSample()
              - expected) < 1.0e-12,
          "full GLIDE uses the 2M times 470n time constant");
}

// MASTER OCTAVE transposes in exact octaves.
void testMasterOctave()
{
    GhostarEngine engine;
    engine.prepare(48000.0, 256);
    auto parameters = brightPanel();
    parameters.octave = ghostar::MasterOctave::Sixteen;
    engine.setParameters(parameters);
    engine.noteOn(69, 1.0f);
    const auto samples = renderMono(engine, 0.6, 48000.0, 20);
    int crossings = 0;
    for (std::size_t index = 1; index < samples.size(); ++index)
        if (samples[index - 1] <= 0.0f && samples[index] > 0.0f)
            ++crossings;
    const double hz = static_cast<double>(crossings)
                    / (static_cast<double>(samples.size()) / 48000.0);
    check(std::abs(hz - 220.0) < 6.0, "16' sounds one octave below 8'");
}

// The panel duty sets: Osc A's thinnest rectangle is 6 %, Osc B's is 3 %.
void testPulseDuties()
{
    const auto measureDuty = [](ghostar::Waveform waveform, bool oscA) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        if (oscA)
        {
            parameters.oscAWaveform = waveform;
        }
        else
        {
            parameters.filterPathA = 0.0f;
            parameters.filterPathB = 0.8f;
            parameters.oscBWaveform = waveform;
        }
        engine.setParameters(parameters);
        engine.noteOn(45, 1.0f);
        const auto samples = renderMono(engine, 0.8, 48000.0, 30);
        int high = 0;
        for (const float value : samples)
            if (value > 0.0f)
                ++high;
        return static_cast<double>(high)
             / static_cast<double>(samples.size());
    };

    check(std::abs(measureDuty(ghostar::Waveform::RectWide, true) - 0.50) < 0.04,
          "Osc A's wide rectangle is a 50 % square");
    check(std::abs(measureDuty(ghostar::Waveform::RectThin, true) - 0.06) < 0.03,
          "Osc A's thinnest rectangle sits near 6 %");
    check(std::abs(measureDuty(ghostar::Waveform::RectThin, false) - 0.03) < 0.025,
          "Osc B's thinnest rectangle sits near 3 %");
}

// P1014 takes sync from A's raw saw fall before RS5, through C24/BC308/R107 into
// B pins 9/10.  It therefore resets once at the same A wrap regardless of
// A's selected waveform or PWM, and B's output carries A's fundamental.
void testHardSync()
{
    constexpr double step = 440.0 / (4.0 * 48000.0);
    constexpr double expectedSyncedPhase = 0.6 * step;
    const double triangle =
        ghostar::GhostarCircuitTestAccess::bPhaseAcrossASawWrap(
            ghostar::Waveform::Triangle, 0.5, true);
    const double saw = ghostar::GhostarCircuitTestAccess::bPhaseAcrossASawWrap(
        ghostar::Waveform::Sawtooth, 0.5, true);
    const double pulse =
        ghostar::GhostarCircuitTestAccess::bPhaseAcrossASawWrap(
            ghostar::Waveform::RectThin, 0.08, true);
    const double unsynchronised =
        ghostar::GhostarCircuitTestAccess::bPhaseAcrossASawWrap(
            ghostar::Waveform::RectThin, 0.91, false);
    check(std::abs(triangle - expectedSyncedPhase) < 1.0e-15
              && std::abs(saw - expectedSyncedPhase) < 1.0e-15
              && std::abs(pulse - expectedSyncedPhase) < 1.0e-15,
          "sync follows the raw A saw wrap, not selector or PWM edges");
    check(std::abs(unsynchronised - (0.37 + step)) < 1.0e-15,
          "opening SW2 leaves Osc B phase continuous");

    const auto fundamentalEnergy = [](bool sync) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.filterPathA = 0.0f;
        parameters.filterPathB = 0.8f;
        parameters.sync = sync;
        parameters.interval = 0.93f;   // most of a fifth up: non-harmonic
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);       // A = 220 Hz
        const auto samples = renderMono(engine, 0.8, 48000.0, 30);
        return goertzelMagnitude(samples, 220.0, 48000.0)
             / std::max(1.0e-9, meanAbs(samples));
    };

    const double synced = fundamentalEnergy(true);
    const double unsynced = fundamentalEnergy(false);
    check(synced > 2.0 * unsynced,
          "sync locks Osc B's fundamental to Osc A");
}

// A lowpass must pass less of the same source as its cutoff falls.
void testLowpassAttenuationIsMonotonic()
{
    const auto steadyLevel = [](float cutoff) {
        GhostarEngine engine;
        engine.prepare(44100.0, 256);
        auto parameters = brightPanel();
        parameters.cutoff = cutoff;
        engine.setParameters(parameters);
        engine.noteOn(69, 1.0f);
        return meanAbs(renderMono(engine, 0.6, 44100.0, 40));
    };

    const double open = steadyLevel(0.9f);
    const double mid = steadyLevel(0.5f);
    const double closed = steadyLevel(0.2f);
    check(open > mid, "half-closing the filter reduces the steady level");
    check(mid > closed, "closing the filter further keeps reducing the level");
    check(closed > 0.0, "a nearly closed filter still leaks a fundamental");
}

// 24 dB attenuates far-above-cutoff content more than 12 dB.
void testSlopeSwitch()
{
    const auto brightnessOfSlope = [](ghostar::UpperSlope slope) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.cutoff = 0.35f;
        parameters.slope = slope;
        engine.setParameters(parameters);
        engine.noteOn(69, 1.0f);
        const auto samples = renderMono(engine, 0.6, 48000.0, 30);
        // Compare a high harmonic against the fundamental.
        return goertzelMagnitude(samples, 440.0 * 8.0, 48000.0)
             / std::max(1.0e-12,
                        goertzelMagnitude(samples, 440.0, 48000.0));
    };

    check(brightnessOfSlope(ghostar::UpperSlope::TwelveDb)
              > 2.0 * brightnessOfSlope(ghostar::UpperSlope::TwentyFourDb),
          "the 24 dB slope darkens the eighth harmonic more than 12 dB");
}

// The lower filter's BANDPASS is a parametric boost: it must not attenuate
// far below its peak, and it must lift its peak as resonance rises. The
// probe note sits on the lower peak at a moderate Q, wide enough that a few
// hertz of alignment error stays inside the resonance bandwidth.
void testLowerBandPassIsParametricBoost()
{
    const auto response = [](ghostar::LowerFilterMode mode, float resonance,
                             int note, float inputTravel) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.oscAWaveform = ghostar::Waveform::Triangle;
        parameters.lowerMode = mode;
        parameters.resonance = resonance;
        parameters.filterPathA = inputTravel;
        parameters.lowerOnly = 0.663f;
        parameters.cutoff = 0.55f;
        engine.setParameters(parameters);
        engine.noteOn(note, 1.0f);
        const auto samples = renderMono(engine, 0.6, 48000.0, 30);
        return goertzelMagnitude(samples, 440.0 * std::exp2((note - 69) / 12.0),
                                 48000.0);
    };

    // A fundamental far below the peak passes without attenuation.
    const double lowOut = response(
        ghostar::LowerFilterMode::Out, 0.55f, 45, 0.8f);
    const double lowBoost =
        response(ghostar::LowerFilterMode::BandPass, 0.55f, 45, 0.8f);
    check(lowBoost > 0.6 * lowOut,
          "BANDPASS does not attenuate far below its peak");

    // The owner's manual defines this detent by the audible peak, even while
    // the exact RS7 output net remains open. The behavioral seam must not
    // collapse into OUT merely because the switch gain awaits continuity.
    const double gentle = response(
        ghostar::LowerFilterMode::BandPass, 0.15f, 69, 0.8f);
    const double sharp = response(
        ghostar::LowerFilterMode::BandPass, 0.55f, 69, 0.8f);
    check(sharp > 1.2 * gentle,
          "resonance raises the BANDPASS parametric peak");
}

// The owner's manual describes HIGHPASS and the following Upper low-pass as
// two independently movable resonant edges. The exact RS7 throw remains open,
// but its behavioral seam must reject the bass, pass the interval between the
// two cutoffs, then let the Upper section close the top again.
void testLowerHighPassMakesTheDocumentedDoublePeak()
{
    const auto response = [](ghostar::LowerFilterMode mode, int note) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.oscAWaveform = ghostar::Waveform::Triangle;
        parameters.lowerMode = mode;
        parameters.resonance = 0.55f;
        parameters.filterPathA = 0.8f;
        parameters.lowerOnly = 0.663f;
        parameters.cutoff = 0.55f;
        engine.setParameters(parameters);
        engine.noteOn(note, 1.0f);
        const auto samples = renderMono(engine, 0.9, 48000.0, 60);
        const double hz = 440.0 * std::exp2((note - 69) / 12.0);
        return goertzelMagnitude(samples, hz, 48000.0);
    };

    const double lowOut = response(ghostar::LowerFilterMode::Out, 45);
    const double middleOut = response(ghostar::LowerFilterMode::Out, 81);
    const double lowHighPass =
        response(ghostar::LowerFilterMode::HighPass, 45);
    const double middleHighPass =
        response(ghostar::LowerFilterMode::HighPass, 81);
    const double highHighPass =
        response(ghostar::LowerFilterMode::HighPass, 93);

    check(lowHighPass < 0.25 * lowOut,
          "HIGHPASS rejects content below the Lower cutoff");
    check(middleHighPass > 0.7 * middleOut,
          "HIGHPASS passes the interval between the two cutoffs");
    check(middleHighPass > 1.25 * lowHighPass
              && middleHighPass > 1.5 * highHighPass,
          "Lower high-pass and Upper low-pass form the documented two edges");
}

// OVERDRIVE is a saturator: doubling its input must yield clearly less than
// double its output, where the clean boost scales linearly. (A single-
// harmonic check is deliberately avoided — waveshaping a triangle nulls
// individual harmonics at particular drives.)
void testOverdriveCompresses()
{
    const auto level = [](ghostar::LowerFilterMode mode, float slider) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.filterPathA = slider;
        parameters.lowerMode = mode;
        parameters.resonance = 0.3f;
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);
        return meanAbs(renderMono(engine, 0.5, 48000.0, 30));
    };

    const double clean = level(ghostar::LowerFilterMode::BandPass, 0.8f);
    const double driven = level(ghostar::LowerFilterMode::Overdrive, 0.8f);
    const double out = level(ghostar::LowerFilterMode::Out, 0.8f);
    const double cleanRatio = clean
        / std::max(1.0e-12,
                   level(ghostar::LowerFilterMode::BandPass, 0.2f));
    const double drivenRatio = driven
        / std::max(1.0e-12,
                   level(ghostar::LowerFilterMode::Overdrive, 0.2f));
    check(cleanRatio > 3.2, "the clean boost scales linearly with its input");
    check(drivenRatio < 0.8 * cleanRatio,
          "the overdrive stage compresses instead of scaling linearly");
    check(driven > 0.1 * out,
          "OVERDRIVE retains an audible level through C34 and IC14B");
}

// The envelope segments are RC charges on the 4.7 uF cap through the 2 MOhm
// log sliders, so the panel's labelled time is the time *constant*: at full
// travel the envelope must fall to 1/e of its span in ~9.40047 s, not reach its
// target in that time. D15 puts zero sustain at the VCA's 1/15 dead zone;
// after one time constant the remaining envelope span is 1/e, then the
// factory 4k7 network and CEM3360's 52%/V scale set the audible level (OQ-04).
void testDecayIsTheLabelledTimeConstant()
{
    GhostarEngine engine;
    engine.prepare(48000.0, 256);
    auto parameters = brightPanel();
    parameters.loudnessAttack = 0.0f;
    parameters.loudnessDecay = 1.0f; // 2 MOhm + 100 Ohm: tau = 9.40047 s
    parameters.loudnessSustain = 0.0f;
    parameters.filterEnvAmount = 0.5f;
    engine.setParameters(parameters);
    engine.noteOn(57, 1.0f);

    // Peak just after the attack, then the level one time constant later.
    renderMono(engine, 0.05, 48000.0);
    const double atPeak = peak(renderMono(engine, 0.05, 48000.0));
    renderMono(engine, 9.40047 - 0.1, 48000.0);
    const double atTau = peak(renderMono(engine, 0.05, 48000.0));

    const double ratio = atTau / std::max(1.0e-12, atPeak);
    check(ratio > 0.39 && ratio < 0.43,
          "one envelope time constant reaches the component VCA gain oracle");
}

// Ghostar's nominal attack charges toward ~1.3x the peak, so it reaches the peak in
// ln(1.3/0.3) = 1.47 time constants — flatter-topped than a segment aiming
// at 1.5 (ln 3 = 1.10) and far from a linear ramp (SM DWG 3, OQ-04).
void testAttackAimsPastItsPeak()
{
    const auto levelAfter = [](double seconds) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.loudnessAttack = 0.75f;   // tau ~ 0.6 s
        parameters.loudnessDecay = 1.0f;
        parameters.loudnessSustain = 1.0f;
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);
        renderMono(engine, seconds, 48000.0);
        return peak(renderMono(engine, 0.02, 48000.0));
    };

    // At travel 0.75 the slider stands at 1 kOhm * 2000^0.75 = 299 kOhm,
    // R23 adds 100 Ohm, so tau = 4.7 uF * 299.17 kOhm = 1.406 s.
    constexpr double tau = 4.7e-6 * 299170.0;
    // One time constant into an aim of 1.3 puts the envelope at
    // 1.3*(1-1/e) = 0.822. The corrected Loudness VCA maps that to nominal
    // gain 1.230545*0.822-0.082036 = 0.929 before its maximum-gain clamp.
    const double atTau = levelAfter(tau);
    const double atPeak = levelAfter(4.0 * tau);
    const double fraction = atTau / std::max(1.0e-12, atPeak);
    check(fraction > 0.90 && fraction < 0.96,
          "the one-tau attack level follows the RC aim through the CEM3360 law");
}

// The travel-to-Q law is derived from the CEM3350's −65 mV/decade Q scale
// and the Spirit's own pot network, anchored by the panel's LOW = Q 0.5.
// Its signature is that resonance stays gentle through mid-travel and then
// climbs steeply: Q ≈ 1.48 at half travel against ≈ 10.9 at nine tenths.
// Tied VIF+VIV drive falls as Q rises and the external C37 loop loads the
// played response, but the near-peak harmonic must still grow strongly.
void testResonanceFollowsTheDerivedQLaw()
{
    check(ghostar::GhostarCircuitTestAccess::upperLowDamping() == 2.0,
          "Upper LOW preserves its anchored Q=0.5 instead of subtracting "
          "the VARIABLE enhancement ceiling");

    // Probe on the resonant peak itself: the filter is fed a note whose
    // eighth harmonic sits at the cutoff, and that harmonic is measured.
    const auto peakGain = [](float resonance) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.oscAWaveform = ghostar::Waveform::Sawtooth;
        parameters.upperResonance = ghostar::UpperResonanceMode::Variable;
        parameters.slope = ghostar::UpperSlope::TwelveDb;
        parameters.resonance = resonance;
        parameters.cutoff = 0.5f;   // 20 Hz * 800^0.5 = 566 Hz
        // Keep this component-law probe below C37's overload knee. Hot-signal
        // compression is tested separately from the small-signal Q ratio.
        // Stay well below C37's overload knee while clearing the final
        // CEM3360's now-modelled -116 dBFS output-current noise floor.
        parameters.filterPathA = 0.001f;
        engine.setParameters(parameters);
        engine.noteOn(46, 1.0f);    // ~93 Hz: sixth harmonic near 560 Hz
        const auto samples = renderMono(engine, 0.9, 48000.0, 60);
        // C40 makes the selected 23 nF LP integrator 22/23 as fast; the
        // resulting resonant centre is ~555 Hz and matches note 46's sixth.
        return goertzelMagnitude(samples, 555.0, 48000.0);
    };

    const double atHalf = peakGain(0.5f);
    const double atNineTenths = peakGain(0.9f);
    const double ratio = atNineTenths / std::max(1.0e-12, atHalf);
    check(ratio > 2.5 && ratio < 6.0,
          "the loaded resonant peak grows strongly from half to "
          "nine-tenths travel");

    // …and the law is gentle where the old voiced one was not: at half
    // travel the section must still be close to critically damped, not
    // ringing. Q = 1.48 puts the peak barely above 3 dB.
    const auto flatness = [](float resonance) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.upperResonance = ghostar::UpperResonanceMode::Variable;
        parameters.slope = ghostar::UpperSlope::TwelveDb;
        parameters.resonance = resonance;
        parameters.cutoff = 0.5f;
        parameters.filterPathA = 0.0f;
        parameters.filterPathNoise = 0.8f;
        engine.setParameters(parameters);
        engine.noteOn(60, 1.0f);
        const auto samples = renderMono(engine, 0.9, 48000.0, 60);
        return goertzelMagnitude(samples, 555.0, 48000.0)
             / std::max(1.0e-12,
                        goertzelMagnitude(samples, 100.0, 48000.0));
    };
    check(flatness(0.5f) < 3.0,
          "half travel is barely resonant, as Q = 1.5 requires");
}

// The traced BA130/IC12A scalar used by the interim OVERDRIVE hypothesis keeps
// climbing past its knee rather than becoming a hard tanh ceiling. OQ-10
// still owns the actual switch assignment and absolute state-node scale.
void testOverdriveCeilingKeepsClimbing()
{
    const auto level = [](float slider) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.filterPathA = slider;
        parameters.lowerMode = ghostar::LowerFilterMode::Overdrive;
        parameters.resonance = 0.3f;
        engine.setParameters(parameters);
        engine.noteOn(45, 1.0f);
        return meanAbs(renderMono(engine, 0.5, 48000.0, 30));
    };

    // Both settings are well past the knee, so a hard ceiling would put
    // them within a percent of each other.
    // The current Filter-mixer approximation puts 0.39 travel at about 0.35
    // of full drive. OQ-20 owns the moving-node replacement.
    const double driven = level(0.39f);
    const double harder = level(1.0f);
    const double growth = harder / std::max(1.0e-12, driven);
    check(growth > 1.02,
          "past the knee the OVERDRIVE circuit still grows with drive");
    check(growth < 1.6,
          "past the knee the OVERDRIVE circuit grows only slowly");
}

// At full resonance the filter self-oscillates: kicked once, it keeps
// singing after the kick is gone.
void testSelfOscillation()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    auto parameters = brightPanel();
    parameters.filterPathA = 0.0f;
    parameters.filterPathNoise = 0.6f;
    parameters.resonance = 1.0f;
    parameters.upperResonance = ghostar::UpperResonanceMode::Variable;
    parameters.cutoff = 0.6f;
    parameters.vcaBypass = true;
    engine.setParameters(parameters);
    renderMono(engine, 0.2, 44100.0);

    parameters.filterPathNoise = 0.0f;
    engine.setParameters(parameters);
    renderMono(engine, 1.0, 44100.0);
    const auto ringing = renderMono(engine, 0.5, 44100.0);
    check(peak(ringing) > 1.0e-4,
          "the filter keeps singing after its excitation is removed");
    check(peak(ringing) < 4.0, "self-oscillation stays bounded");
}

// BRIGHTNESS is the Shaper VCA output's passive low-shelf branch. Measure the
// oscillator line rather than mean absolute level: P1014's genuine waveform
// DC offset is supposed to pass through this separately exposed DC path.
void testBrightnessDarkensShaperPath()
{
    const auto shaperLevel = [](float brightness) {
        GhostarEngine engine;
        engine.prepare(44100.0, 256);
        EngineParameters parameters;
        parameters.filterPathA = 0.0f;   // isolate the Shaper path
        parameters.shaperPathA = 0.8f;
        parameters.brightness = brightness;
        parameters.shaperMode = ghostar::ShaperMode::KbdHold;
        parameters.shaperRate = 1.0f;
        engine.setParameters(parameters);
        engine.noteOn(93, 1.0f);
        const auto samples = renderMono(engine, 0.6, 44100.0, 40);
        return goertzelMagnitude(samples, 1760.0, 44100.0);
    };

    const double bright = shaperLevel(1.0f);
    const double dark = shaperLevel(0.15f);
    check(bright > 1.0e-4, "the Shaper path sounds with brightness open");
    check(dark < 0.8 * bright, "closing BRIGHTNESS darkens the Shaper path");
}

// P1017's normal contact cross-loads both 20k Master gangs through the two
// 10k output arms. C18/P3 makes the result frequency-dependent. Verify the
// endpoint-safe nodal equations independently, then pin the dark high-frequency
// quirk that a simple half-sum misses by 6 dB.
void testOutputNetworkMatchesP1013AndP1017()
{
    constexpr double masterTrack = 20000.0;
    constexpr double outputArm = 10000.0;
    constexpr double capacitance = 27.0e-9;
    constexpr double hostRate = 48000.0;
    constexpr double filterInput = 0.31;
    constexpr double shaperInput = -0.23;
    constexpr double oldCompanion = 0.07;

    for (const bool split : { false, true })
        for (const double master : { 0.0, 0.27, 0.73, 1.0 })
            for (const double rheostat : { 0.0, 23000.0, 100000.0 })
            {
                const auto result =
                    ghostar::GhostarCircuitTestAccess::outputNetworkStep(
                        filterInput, shaperInput, oldCompanion, rheostat,
                        master, split, hostRate);
                const double g = 2.0 * (4.0 * hostRate) * capacitance;
                const double gb = g / (1.0 + g * rheostat);
                const double jf = filterInput / masterTrack;
                const double js = shaperInput / masterTrack;
                const double h = split ? 0.0 : 1.0 / (2.0 * outputArm);
                const double lower = master * masterTrack;
                const double upper = (1.0 - master) * masterTrack;
                const double topDenominator = 1.0 + gb * upper;
                const double ge = gb / topDenominator;
                const double je =
                    (js + gb * oldCompanion) / topDenominator;
                const double coupling = h * lower;
                const double a = 1.0 + coupling;
                const double d = 1.0 + coupling + ge * lower;

                check(std::abs(a * result.filterWiper
                               - coupling * result.shaperWiper
                               - jf * lower) < 1.0e-12,
                      "Filter Master wiper satisfies P1017 KCL");
                check(std::abs(-coupling * result.filterWiper
                               + d * result.shaperWiper
                               - je * lower) < 1.0e-12,
                      "Shaper Master wiper satisfies P1017 KCL");
                check(std::abs(topDenominator * result.shaperTop
                               - result.shaperWiper
                               - upper * (js + gb * oldCompanion))
                          < 1.0e-12,
                      "Shaper top node satisfies P1013 KCL");

                const double capacitorVoltage =
                    0.5 * (result.companion + oldCompanion);
                const double branchCurrent =
                    g * (capacitorVoltage - oldCompanion);
                check(std::abs(result.shaperTop - capacitorVoltage
                               - rheostat * branchCurrent) < 1.0e-12,
                      "coupled C18/P3 satisfies KVL");
                check(std::abs(branchCurrent
                               - gb * (result.shaperTop - oldCompanion))
                          < 1.0e-12,
                      "coupled C18 conserves trapezoidal charge");
            }

    // At DC the C18 branch is open: the equal source impedances make the
    // normalled main exactly half the isolated Filter level.
    const auto dc = ghostar::GhostarCircuitTestAccess::outputNetworkStep(
        1.0, 0.0, 1.0 / 3.0, 0.0, 1.0, false, hostRate);
    check(std::abs(0.5 * (dc.filterWiper + dc.shaperWiper) - 0.5)
              < 1.0e-12,
          "normalled output is the equal-source half-sum at DC");

    // At the dark setting and a high-frequency step, C18 grounds the idle
    // Shaper top. The Filter wiper is halved, then the jack averages again.
    const auto dark = ghostar::GhostarCircuitTestAccess::outputNetworkStep(
        1.0, 0.0, 0.0, 0.0, 1.0, false, 96000.0);
    const auto isolated =
        ghostar::GhostarCircuitTestAccess::outputNetworkStep(
            1.0, 0.0, 0.0, 0.0, 1.0, true, 96000.0);
    const double darkRatio =
        0.5 * (dark.filterWiper + dark.shaperWiper)
        / isolated.filterWiper;
    check(std::abs(darkRatio - 0.25) < 0.002,
          "dark BRIGHTNESS cross-loading quarters the normalled Filter step");
}

// P1017 draws no global series capacitor. C30 removes Filter-path DC before
// its VCA, while the separately exposed Shaper path remains DC-coupled.
void testOutputCouplingMatchesP1013AndP1017()
{
    const auto dcMean = [](bool shaperPath) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        EngineParameters parameters;
        // RectMid's nominal 30% duty is almost DC-balanced after IC10's
        // asymmetric P1014 conditioner; RectWide exposes the DC coupling.
        parameters.oscAWaveform = ghostar::Waveform::RectWide;
        parameters.masterVolume = 1.0f;
        parameters.brightness = 1.0f;
        parameters.filterPathA = shaperPath ? 0.0f : 1.0f;
        parameters.shaperPathA = shaperPath ? 1.0f : 0.0f;
        parameters.shaperMode = ghostar::ShaperMode::KbdHold;
        parameters.vcaBypass = !shaperPath;
        engine.setParameters(parameters);
        engine.noteOn(48, 1.0f);
        renderMono(engine, 1.0, 48000.0);
        return mean(renderMono(engine, 2.0, 48000.0));
    };

    const double filterDc = dcMean(false);
    const double shaperDc = dcMean(true);
    check(std::abs(shaperDc) > 1.0e-2,
          "the Shaper jack has no invented output high-pass");
    check(std::abs(filterDc) < 0.03 * std::abs(shaperDc),
          "C30 rejects at least 97% of duty-cycle DC on the Filter path");
}

// Pin the current behavioral Shaper-VCA seam until P1013's tied-base parallel
// BC173/CEM3360 control network is calibrated (OQ-26): FREE has loud and quiet
// stretches, but this is not presented as a component-level oracle.
void testShaperFreeModePulsesItsPath()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    EngineParameters parameters;
    parameters.filterPathA = 0.0f;   // isolate the Shaper path
    parameters.shaperPathA = 0.8f;
    parameters.shaperMode = ghostar::ShaperMode::Free;
    parameters.shaperRate = 0.85f;
    engine.setParameters(parameters);
    engine.noteOn(57, 1.0f);
    renderMono(engine, 0.5, 44100.0);
    const auto samples = renderMono(engine, 2.0, 44100.0);

    // Split into short windows; some must be silent, some loud.
    const std::size_t window = 2048;
    double quietest = 1.0e9;
    double loudest = 0.0;
    for (std::size_t start = 0; start + window <= samples.size();
         start += window)
    {
        double sum = 0.0;
        for (std::size_t index = start; index < start + window; ++index)
            sum += std::abs(static_cast<double>(samples[index]));
        const double level = sum / static_cast<double>(window);
        quietest = std::min(quietest, level);
        loudest = std::max(loudest, level);
    }
    check(loudest > 1.0e-3, "the free-running Shaper opens its VCA");
    check(quietest < 0.05 * loudest,
          "the voiced free-running Shaper seam closes in the negative half");
}

// The production CEM3360 sheet gives each cell 0.4 nA RMS typical output
// noise in 16 Hz--16 kHz. With SPLIT open, full Master and no signal, IC7's
// Loudness cell is isolated on the left: 0.4 nA through its fixed 20k load,
// divided by the engine's 5 V/unit boundary, is 1.6e-6 RMS. The real
// decimators' finite transition make that 1.58668e-6 at this 32 kHz host
// rate. IC5 appears independently on the right, after C18/P3 colours it.
void testOutputVcasCarryIndependentDatasheetNoise()
{
    constexpr double sampleRate = 32000.0;
    struct Statistics
    {
        double leftRms;
        double rightRms;
        double correlation;
    };
    const auto measure = [](float brightness) {
        constexpr int blockSize = 256;
        GhostarEngine engine;
        engine.prepare(sampleRate, blockSize);
        EngineParameters parameters;
        parameters.masterVolume = 1.0f;
        parameters.brightness = brightness;
        parameters.splitPaths = true;
        parameters.filterPathA = 0.0f;
        parameters.filterPathB = 0.0f;
        parameters.filterPathNoise = 0.0f;
        parameters.shaperPathA = 0.0f;
        parameters.shaperPathB = 0.0f;
        parameters.shaperPathRing = 0.0f;
        parameters.shaperPathNoise = 0.0f;
        engine.setParameters(parameters);

        std::array<float, blockSize> left {};
        std::array<float, blockSize> right {};
        for (int block = 0; block < 32; ++block)
            engine.process(left.data(), right.data(), blockSize);

        double leftSquared = 0.0;
        double rightSquared = 0.0;
        double cross = 0.0;
        constexpr int measuredBlocks = 500;
        for (int block = 0; block < measuredBlocks; ++block)
        {
            engine.process(left.data(), right.data(), blockSize);
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const double l = left[static_cast<std::size_t>(sample)];
                const double r = right[static_cast<std::size_t>(sample)];
                leftSquared += l * l;
                rightSquared += r * r;
                cross += l * r;
            }
        }
        constexpr double count = measuredBlocks * blockSize;
        return Statistics {
            std::sqrt(leftSquared / count),
            std::sqrt(rightSquared / count),
            cross / std::sqrt(leftSquared * rightSquared)
        };
    };

    const auto bright = measure(1.0f);
    constexpr double expectedFilter = 1.586682e-6;
    check(std::abs(bright.leftRms / expectedFilter - 1.0) < 0.08,
          "the final CEM3360 VCA carries its 0.4 nA RMS output noise");
    check(bright.rightRms > 0.75 * bright.leftRms
              && bright.rightRms < 0.95 * bright.leftRms,
          "the Shaper CEM3360 carries noise through C18/P3");
    check(std::abs(bright.correlation) < 0.03,
          "the two physical CEM3360 cells use independent noise streams");

    const auto dark = measure(0.0f);
    check(dark.rightRms < 0.25 * bright.rightRms,
          "BRIGHTNESS colours the Shaper cell's output noise");
}

double noiseCircuitMagnitude(double sampleRate, double hz)
{
    SpiritNoise noise;
    noise.prepare(sampleRate);
    const double circuitRate = noise.circuitSampleRate();

    // The 0.595 Hz coupling pole is the slowest state. Two seconds of warmup
    // puts its transient more than seven time constants behind the probe.
    const auto warmup = static_cast<int>(std::lround(2.0 * circuitRate));
    const auto measured = static_cast<int>(std::lround(2.0 * circuitRate));
    const double angular = 2.0 * 3.14159265358979323846 * hz / circuitRate;
    for (int index = 0; index < warmup; ++index)
        (void) noise.processCircuit(std::sin(angular * index));

    double inPhase = 0.0;
    double quadrature = 0.0;
    for (int index = warmup; index < warmup + measured; ++index)
    {
        const double output = noise.processCircuit(std::sin(angular * index));
        inPhase += output * std::sin(angular * index);
        quadrature += output * std::cos(angular * index);
    }
    const double scale = 2.0 / static_cast<double>(measured);
    return scale * std::hypot(inPhase, quadrature);
}

double redNoiseCircuitMagnitude(double sampleRate, double hz)
{
    SpiritNoise noise;
    noise.prepare(sampleRate);
    const double circuitRate = noise.circuitSampleRate();
    const auto warmup = static_cast<int>(std::lround(2.0 * circuitRate));
    const auto measured = static_cast<int>(std::lround(2.0 * circuitRate));
    const double angular = 2.0 * 3.14159265358979323846 * hz / circuitRate;
    for (int index = 0; index < warmup; ++index)
        (void) noise.processCircuit(std::sin(angular * index));

    double inPhase = 0.0;
    double quadrature = 0.0;
    for (int index = warmup; index < warmup + measured; ++index)
    {
        (void) noise.processCircuit(std::sin(angular * index));
        const double output = noise.redCircuitOutput();
        inPhase += output * std::sin(angular * index);
        quadrature += output * std::cos(angular * index);
    }
    const double scale = 2.0 / static_cast<double>(measured);
    return scale * std::hypot(inPhase, quadrature);
}

// The grayscale service scan resolves the full P1013 transfer. These three
// magnitudes probe its bass shelf, midband and high-frequency tail; checking
// three common production grids catches both a copied component error and a
// recurrence whose poles move with the host rate.
void testNoiseCircuitMatchesTheSchematic()
{
    struct Probe
    {
        double hz;
        double magnitude;
    };
    constexpr std::array<Probe, 3> probes {{
        { 20.0, 11.8522631589 },
        { 1000.0, 0.9458922715 },
        { 10000.0, 0.5392584057 },
    }};

    for (const double hostRate : { 44100.0, 48000.0, 96000.0 })
    {
        const double internalRate = 4.0 * hostRate;
        for (const auto probe : probes)
        {
            const double measured =
                noiseCircuitMagnitude(internalRate, probe.hz);
            const double errorDb =
                20.0 * std::log10(measured / probe.magnitude);
            check(std::abs(errorDb) < 0.1,
                  "the noise circuit matches its schematic transfer");
        }
    }
}

// P1013 supplies MOD SOURCE from the R6/C8 junction through IC4B, not from
// an arbitrary very-slow RNG. These points fingerprint its broad red-noise
// shelf independently of the IC4A audio branch.
void testRedNoiseCircuitMatchesTheSchematic()
{
    struct Probe
    {
        double hz;
        double magnitude;
    };
    constexpr std::array<Probe, 3> probes {{
        { 1.0, 29.8308136769 },
        { 20.0, 29.0211762124 },
        { 100.0, 9.1483382147 },
    }};

    for (const double hostRate : { 44100.0, 48000.0, 96000.0 })
    {
        const double internalRate = 4.0 * hostRate;
        for (const auto probe : probes)
        {
            const double measured =
                redNoiseCircuitMagnitude(internalRate, probe.hz);
            const double errorDb =
                20.0 * std::log10(measured / probe.magnitude);
            check(std::abs(errorDb) < 0.1,
                  "the red-noise branch matches its schematic transfer");
        }
    }
}

// Taps 17/14 must traverse every non-zero 17-bit state before repeating.
// Running the source at its own nominal clock makes one process call one bit.
void testMm5837SequenceLength()
{
    SpiritNoise noise;
    noise.prepare(SpiritNoise::nominalClockHz);
    constexpr char expectedPrefix[] =
        "0000000000000011100000000000111111000000001110001110000011111111";
    std::uint32_t ones = 0;
    for (std::uint32_t bit = 0; bit < SpiritNoise::sequenceLength; ++bit)
    {
        (void) noise.process();
        const bool high = noise.heldBit() > 0.0;
        ones += high ? 1u : 0u;
        if (bit < sizeof(expectedPrefix) - 1u)
            check(high == (expectedPrefix[bit] == '1'),
                  "the MM5837 uses the documented taps-17/14 prefix");
    }
    check(ones == 65536u,
          "the MM5837 maximal cycle contains 65536 high bits");

    // One complete cycle returns to the fixed non-zero seed, so the known
    // prefix must start again. Combined with the maximal-cycle bit balance,
    // this rules out the constant/short generators a self-comparison admits.
    for (const char expected : expectedPrefix)
    {
        if (expected == '\0')
            break;
        (void) noise.process();
        check((noise.heldBit() > 0.0) == (expected == '1'),
              "the MM5837 PRBS repeats after exactly 131071 bits");
    }
}

// A physical self-clocked source and physical RC network do not acquire less
// low-frequency energy when a host doubles its sample rate. The previous
// host-clocked RNG did lose about 3 dB of depth per rate octave.
void testMm5837RedNoiseIsRateInvariant()
{
    struct Levels
    {
        double audio;
        double red;
    };
    const auto levelsAt = [](double hostRate) {
        const double internalRate = 4.0 * hostRate;
        SpiritNoise noise;
        noise.prepare(internalRate);
        const auto warmup = static_cast<std::uint64_t>(std::lround(
            2.0 * SpiritNoise::sequenceLength / SpiritNoise::nominalClockHz
            * internalRate));
        const auto measured = static_cast<std::uint64_t>(std::lround(
            4.0 * SpiritNoise::sequenceLength / SpiritNoise::nominalClockHz
            * internalRate));
        for (std::uint64_t sample = 0; sample < warmup; ++sample)
            (void) noise.process();
        double audioSum = 0.0;
        double redSum = 0.0;
        for (std::uint64_t sample = 0; sample < measured; ++sample)
        {
            const double audio = noise.process();
            audioSum += audio * audio;
            redSum += noise.red() * noise.red();
        }
        const double divisor = static_cast<double>(measured);
        return Levels { std::sqrt(audioSum / divisor),
                        std::sqrt(redSum / divisor) };
    };

    const auto at8 = levelsAt(8000.0);
    const auto at44 = levelsAt(44100.0);
    const auto at96 = levelsAt(96000.0);
    const auto spreadDb = [](double a, double b, double c) {
        const double lowest = std::min({ a, b, c });
        const double highest = std::max({ a, b, c });
        return 20.0 * std::log10(highest / lowest);
    };
    check(spreadDb(at8.audio, at44.audio, at96.audio) < 0.6,
          "MM5837 audio-noise level agrees across host rates within 0.6 dB");
    check(spreadDb(at8.red, at44.red, at96.red) < 0.6,
          "MM5837 red-noise depth agrees across host rates within 0.6 dB");
}

// DWG 2 marks both 20k Master Volume gangs LIN. Pin that law on an isolated
// split output; the normalled jack's cross-loading is intentionally nonlinear
// and is checked against its MNA above.
void testMasterVolumeIsLinear()
{
    const auto levelAt = [](float volume) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.masterVolume = volume;
        parameters.splitPaths = true;
        engine.setParameters(parameters);
        engine.noteOn(48, 1.0f);
        renderMono(engine, 0.5, 48000.0);
        return meanAbs(renderMono(engine, 0.5, 48000.0));
    };

    const double full = levelAt(1.0f);
    const double half = levelAt(0.5f);
    check(full > 1.0e-4, "the master-volume probe is audible");
    check(std::abs(half / full - 0.5) < 0.01,
          "half Master Volume travel gives half the output");
}

// The Shaper audio sliders are 100k LIN pots whose finite series arms end in
// a virtual-earth summer. Their Thevenin resistance bends the law: A/B/Noise
// use R42/R43/R44=47k, while the assembly-corrected Ring position SL4 uses
// errata R45=6k8. The Filter's moving-node buses are separate (OQ-20).
void testShaperMixerSliderIncludesItsLoading()
{
    const auto shaperOscillatorLevelAt = [](float travel) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        EngineParameters parameters;
        parameters.filterPathA = 0.0f;
        parameters.shaperPathA = travel;
        parameters.shaperMode = ghostar::ShaperMode::KbdHold;
        parameters.brightness = 1.0f;
        parameters.masterVolume = 1.0f;
        engine.setParameters(parameters);
        engine.noteOn(48, 1.0f);
        renderMono(engine, 0.5, 48000.0);
        return meanAbs(renderMono(engine, 0.5, 48000.0));
    };

    const double oscillatorFull = shaperOscillatorLevelAt(1.0f);
    const double oscillatorHalf = shaperOscillatorLevelAt(0.5f);
    constexpr double expectedOscillator = 0.5 * 47.0 / (47.0 + 25.0);
    check(oscillatorFull > 1.0e-4, "the mixer-loading probe is audible");
    check(std::abs(oscillatorHalf / oscillatorFull - expectedOscillator)
              < 0.01,
          "the Shaper slider includes its loaded-linear law");

    // PINK NOISE IN reaches SL3/R44=47k. Long, identically seeded renders
    // make the source colour and voiced scale cancel from the ratio.
    const auto shaperNoiseLevelAt = [](float travel) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        EngineParameters parameters;
        parameters.filterPathA = 0.0f;
        parameters.shaperPathA = 0.0f;
        parameters.shaperPathNoise = travel;
        parameters.shaperMode = ghostar::ShaperMode::KbdHold;
        parameters.brightness = 1.0f;
        parameters.masterVolume = 1.0f;
        engine.setParameters(parameters);
        engine.noteOn(48, 1.0f);
        renderMono(engine, 1.0, 48000.0);
        return meanAbs(renderMono(engine, 1.0, 48000.0));
    };
    const double noiseFull = shaperNoiseLevelAt(1.0f);
    const double noiseHalf = shaperNoiseLevelAt(0.5f);
    constexpr double expectedNoise = 0.5 * 47.0 / (47.0 + 25.0);
    check(noiseFull > 1.0e-4, "the Shaper-noise loading probe is audible");
    check(std::abs(noiseHalf / noiseFull - expectedNoise) < 0.01,
          "the Shaper Noise slider uses SL3/R44's 47k loaded law");

    const auto shaperRingLevelAt = [](float travel) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        EngineParameters parameters;
        parameters.filterPathA = 0.0f;
        parameters.shaperPathA = 0.0f;
        parameters.shaperPathRing = travel;
        parameters.oscAWaveform = ghostar::Waveform::Triangle;
        parameters.oscBWaveform = ghostar::Waveform::Triangle;
        parameters.shaperMode = ghostar::ShaperMode::KbdHold;
        parameters.brightness = 1.0f;
        parameters.masterVolume = 1.0f;
        engine.setParameters(parameters);
        engine.noteOn(48, 1.0f);
        renderMono(engine, 0.5, 48000.0);
        return meanAbs(renderMono(engine, 0.5, 48000.0));
    };
    const double ringFull = shaperRingLevelAt(1.0f);
    const double ringHalf = shaperRingLevelAt(0.5f);
    constexpr double expectedRing = 0.5 * 6.8 / (6.8 + 25.0);
    check(ringFull > 1.0e-4, "the Shaper-ring loading probe is audible");
    check(std::abs(ringHalf / ringFull - expectedRing) < 0.01,
          "the Shaper Ring slider uses SL4/R45's 6.8k loaded law");

}

// P1015's D19/D20 steer opposite ends of 1M LIN P4 through 27k R62.
// Half-cycle time is proportional to the selected resistance, so the
// component oracle is (27k + t*1M)/(1M + 2*27k), with a constant total.
void testShaperShapeFollowsItsSteeredPot()
{
    constexpr std::array<float, 5> travels { 0.0f, 0.25f, 0.5f, 0.75f,
                                              1.0f };
    int referencePeriod = 0;
    for (const float travel : travels)
    {
        const auto cycle =
            ghostar::GhostarCircuitTestAccess::shaperFreeCycle(travel);
        const int period = cycle.riseSamples + cycle.fallSamples;
        const double measured = static_cast<double>(cycle.riseSamples)
                              / static_cast<double>(period);
        const double expected =
            (27000.0 + 1000000.0 * static_cast<double>(travel))
            / (1000000.0 + 2.0 * 27000.0);

        check(std::abs(measured - expected) < 5.0e-5,
              "the Shaper split follows P4, R62 and D19/D20");
        check(std::abs(static_cast<double>(cycle.gateHighSamples) / period
                       - expected) < 5.0e-5,
              "FREE SG stays high for the complete rising leg");
        if (referencePeriod == 0)
            referencePeriod = period;
        else
            check(std::abs(period - referencePeriod) <= 2,
                  "SHAPE leaves the Shaper period constant");
    }
}

// IC6's SG output is the Shaper phase comparator: high on the rising leg,
// low after the positive reversal and throughout the envelope-mode idle.
// RS3 gives KBD HOLD a distinct path, so a gate during release reverses the
// ramp upward from its present level instead of imposing a level threshold.
void testShaperEnvelopeModesExposeTheirRisingPhaseAsSg()
{
    using ghostar::GhostarCircuitTestAccess;
    using ghostar::ShaperMode;

    const auto holdIdle = GhostarCircuitTestAccess::shaperEnvelopePhaseStep(
        ShaperMode::KbdHold, 0.0, true, false, false);
    check(!holdIdle.gate && !holdIdle.rising,
          "KBD HOLD SG is low at zero-level idle");

    const auto holdRise = GhostarCircuitTestAccess::shaperEnvelopePhaseStep(
        ShaperMode::KbdHold, 0.25, false, false, true);
    check(holdRise.gate && holdRise.rising && holdRise.level > 0.25,
          "KBD HOLD SG is high while the gate raises the Shaper");

    const auto holdTop = GhostarCircuitTestAccess::shaperEnvelopePhaseStep(
        ShaperMode::KbdHold, 1.0, true, false, true);
    check(!holdTop.gate && !holdTop.rising && holdTop.level == 1.0,
          "KBD HOLD SG flips low at the held maximum");

    const auto holdRelease =
        GhostarCircuitTestAccess::shaperEnvelopePhaseStep(
            ShaperMode::KbdHold, 0.75, true, false, false);
    check(!holdRelease.gate && !holdRelease.rising
              && holdRelease.level < 0.75,
          "KBD HOLD SG stays low during release");

    const auto holdRegate =
        GhostarCircuitTestAccess::shaperEnvelopePhaseStep(
            ShaperMode::KbdHold, 0.5, false, false, true);
    check(holdRegate.gate && holdRegate.rising && holdRegate.level > 0.5,
          "KBD HOLD re-gate reverses release into a high-SG rising leg");

    for (const ShaperMode mode : { ShaperMode::Reset, ShaperMode::Run })
    {
        const auto idle = GhostarCircuitTestAccess::shaperEnvelopePhaseStep(
            mode, 0.0, true, false, false);
        check(!idle.gate,
              "RESET/RUN SG is low while the single cycle is idle");

        const auto rise = GhostarCircuitTestAccess::shaperEnvelopePhaseStep(
            mode, 0.25, true, true, false);
        check(rise.gate && rise.rising && rise.active,
              "RESET/RUN SG is high during an active rising leg");

        const auto apex = GhostarCircuitTestAccess::shaperEnvelopePhaseStep(
            mode, 1.0, true, true, false);
        check(!apex.gate && !apex.rising && apex.active,
              "RESET/RUN SG flips low at the positive reversal");

        const auto fall = GhostarCircuitTestAccess::shaperEnvelopePhaseStep(
            mode, 0.75, false, true, false);
        check(!fall.gate && !fall.rising && fall.active,
              "RESET/RUN SG is low during an active falling leg");

        const auto ended = GhostarCircuitTestAccess::shaperEnvelopePhaseStep(
            mode, 1.0e-9, false, true, false);
        check(!ended.gate && !ended.active && ended.level == 0.0,
              "RESET/RUN SG remains low as the cycle enters idle");
    }
}
} // namespace

int main()
{
    testP1014SelectedWaveConditioner();
    testCem3340PitchMultiplierBypass();
    testPulseWidthReachesTheCemEndpoints();
    testHighQCompanionsSatisfyTheirIntegratedEquations();
    testUpperCascadeMatchesP1013();
    testLowerMixerMnaSatisfiesP1013();
    testHypotheticalA3B7C10NetworkMatchesReference();
    testOutputCapacitorCompanionsMatchP1013();
    testRingModulatorMatchesP1013();
    testKeyboardLaw();
    testExternalPitchSwitchAndCircuitTransfer();
    testMasterTuneFollowsItsLoadedPot();
    testKeyboardTrackingAmount();
    testExternalPitchReachesOnlyItsPhysicalKcvDestinations();
    testOscBPedalMatchesItsDcAndRcNetwork();
    testOscBPedalReachesOnlyOscBInEveryRange();
    testFilterPedalMatchesItsDcAndRcNetwork();
    testFilterPedalFollowsTheTrackingSwitch();
    testFilterPedalOpenReferenceTracksModeWithoutErasingCharge();
    testFilterCutoffsFollowTheLoadedP1013Pots();
    testFilterEnvelopeFollowsThePublishedFiveOctaveMirror();
    testFullGlideUsesTheResolvedRcEndpoint();
    testExternalPitchUsesThePhysicalGlideModes();
    testLoudnessVcaUsesItsControlOffset();
    testEnvelopeDiodeFloorAndReleaseKnee();
    testEnvelopeRetriggerUsesThePhysicalResetNotch();
    testExternalGateReplacesNormalledYAtTheStrictThreshold();
    testExternalGateHardStopsRequireANewPhysicalEdge();
    testExternalAudioReconstructionAndFrameTiming();
    testExternalAudioJackReplacesBothNoiseSliderSources();
    testExternalAudioProcessPathIsSignedAndAligned();
    testExternalAudioJackLeavesRedNoiseUpstream();
    testExternalAudioInputSanitisesAndResets();
    testLfoRateIncludesItsLoadedPot();
    testKeyboardLfoResetRetainsC13BehindItsOutputClamp();
    testModulationWheelsIncludeDestinationLoading();
    testRedNoiseModulationRunsOnTheCircuitGrid();
    testMasterOctave();
    testPulseDuties();
    testHardSync();
    testLowpassAttenuationIsMonotonic();
    testSlopeSwitch();
    testLowerBandPassIsParametricBoost();
    testLowerHighPassMakesTheDocumentedDoublePeak();
    testOverdriveCompresses();
    testDecayIsTheLabelledTimeConstant();
    testAttackAimsPastItsPeak();
    testResonanceFollowsTheDerivedQLaw();
    testOverdriveCeilingKeepsClimbing();
    testSelfOscillation();
    testBrightnessDarkensShaperPath();
    testOutputNetworkMatchesP1013AndP1017();
    testOutputCouplingMatchesP1013AndP1017();
    testShaperFreeModePulsesItsPath();
    testOutputVcasCarryIndependentDatasheetNoise();
    testNoiseCircuitMatchesTheSchematic();
    testRedNoiseCircuitMatchesTheSchematic();
    testMm5837SequenceLength();
    testMm5837RedNoiseIsRateInvariant();
    testMasterVolumeIsLinear();
    testShaperMixerSliderIncludesItsLoading();
    testShaperShapeFollowsItsSteeredPot();
    testShaperEnvelopeModesExposeTheirRisingPhaseAsSg();

    if (failures != 0)
    {
        std::cerr << failures << " Ghostar circuit check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Ghostar circuit checks passed.\n";
    return EXIT_SUCCESS;
}
