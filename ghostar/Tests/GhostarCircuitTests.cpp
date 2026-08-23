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
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

struct EnvelopeResetProbeResult
{
    int releaseSamples;
    double before;
    double afterNotch;
    double afterRestart;
    bool stayedInRelease;
    bool restartedAttack;
};

struct GhostarCircuitTestAccess
{
    static double selectedWaveVolts(Waveform waveform,
                                    double bipolarSample) noexcept
    {
        return GhostarEngine::p1014SelectedWaveVolts(waveform,
                                                     bipolarSample);
    }

    static std::array<double, 3> oscillatorModTapAtSawWrap(
        double previousConditionedB) noexcept
    {
        GhostarEngine engine;
        engine.prepare(48000.0, 64);
        EngineParameters parameters;
        parameters.oscAWaveform = Waveform::Triangle;
        parameters.oscBWaveform = Waveform::Sawtooth;
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
        engine.renderVoiceSample();
        return { engine.phaseA_, engine.phaseB_, engine.lastOscBWave_ };
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
        engine.lfoPhase_ = 0.0;
        engine.advanceControls();
        return engine.lfoPhase_ * hostRate;
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
        engine.modWheel_ = xTravel;
        engine.targetModWheel_ = xTravel;
        engine.shaperWheel_ = yTravel;
        engine.targetShaperWheel_ = yTravel;
        engine.lfoPhase_ = 0.0;

        engine.advanceControls();
        const double baseCutoff = std::sqrt(20.0 * 16000.0);
        const double lowerBaseOctaves =
            (static_cast<double>(parameters.lowerOnly) - 0.8) * 6.25;
        const auto& audio = engine.controlAudioRateMod_;
        return {
            engine.controlOscAOctaves_,
            engine.controlOscBOctaves_,
            std::log2(engine.controlUpperCutoffHz_ / baseCutoff),
            std::log2(engine.controlLowerCutoffHz_ / baseCutoff)
                - lowerBaseOctaves,
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

    static bool xEdgeResetsUnderHeldKeyboardGate() noexcept
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
        engine.lfoPhase_ = 0.0;
        engine.previousLfoSquareHigh_ = false;
        engine.loudnessEnvelope_.stage = GhostarEngine::Adsr::Stage::Decay;
        engine.loudnessEnvelope_.level = 0.8;
        engine.advanceControls();
        return engine.envelopeResetSamplesRemaining_ != 0
            && engine.loudnessEnvelope_.stage
                == GhostarEngine::Adsr::Stage::Release;
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
            previousConditionedB);
    constexpr double internalRate = 4.0 * 48000.0;
    const double bStep = 440.0 * std::exp2(previousConditionedB)
                       / internalRate;
    const double startPhase = 1.0 - 0.4 * bStep;
    const double heldRawB = 2.0 * startPhase - 1.0;
    constexpr double sawWrapHeldCorrection = -0.6 * 0.6;
    const double currentConditionedB = expected(
        0.5 * sawHigh
            * (heldRawB + sawWrapHeldCorrection + 1.0)) / 5.0;
    check(std::abs(phases[0]
                   - 440.0 * std::exp2(currentConditionedB) / internalRate)
              < 1.0e-14,
          "Osc A modulation uses B's fresh BLEP-corrected emitted wave");
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
          "the open RS7 high-pass seam remains explicit and deterministic");

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

// Independent scalar and one-pole reference for the functionally identified
// A3+B7+C10 OVERDRIVE combination. Bisection deliberately shares no Newton
// implementation with the engine, so resistor/sign errors cannot agree by
// construction. C10 makes R167 a clean-VLP feed, not a ground shunt.
void testOverdriveMatchesTheTracedCircuit()
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
    const auto actual = ghostar::GhostarCircuitTestAccess::overdriveStep(
        0.12, -0.015, hostRate);
    const auto expected = reference(0.12, -0.015, hostRate);
    check(std::abs(actual.output - expected.output) < 1.0e-11,
          "OVERDRIVE output matches the traced IC12A/C34 circuit");
    check(std::abs(actual.couplingCompanion - expected.couplingCompanion)
              < 1.0e-11,
          "OVERDRIVE C34 companion matches the traced output high-pass");

    const auto negative = ghostar::GhostarCircuitTestAccess::overdriveStep(
        -0.12, 0.015, hostRate);
    check(std::abs(actual.output + negative.output) < 1.0e-12
              && std::abs(actual.couplingCompanion
                          + negative.couplingCompanion) < 1.0e-12,
          "OVERDRIVE circuit is odd-symmetric");
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

// R135/R136/R137 offset the Loudness CEM3360's linear-control pin so the
// first 0.5 V of the 7.5 V envelope produces no nominal gain. Absolute cell
// gain is normalised separately; the affine law itself is component-derived.
void testLoudnessVcaUsesItsControlOffset()
{
    const auto gain = [](double envelope) {
        return ghostar::GhostarCircuitTestAccess::
            loudnessGainForEnvelope(envelope);
    };

    check(gain(0.0) == 0.0,
          "the Loudness VCA clamps below its control offset");
    check(std::abs(gain(1.0 / 15.0)) < 1.0e-15,
          "the Loudness VCA opens at LC=0.5 V");
    check(std::abs(gain(0.5) - 13.0 / 28.0) < 1.0e-15,
          "the Loudness VCA follows the R135/R136/R137 affine law");
    check(std::abs(gain(1.0) - 1.0) < 1.0e-15,
          "full envelope is the normalised Loudness gain endpoint");
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
}

// Every accepted MULTIPLE/X/Y edge first pulls the shared GS line low for
// the factory-annotated ~5 ms. Both 4.7 uF caps follow their ordinary diode
// release paths during that notch, then the final GS rise starts Attack from
// the retained voltage. X/Y edge branches sit ahead of the gate OR, so they
// remain effective under an already-high keyboard gate. R23/R24=100 ohms
// also sit between the threshold node and each cap, giving a 0.97 cap-side
// peak at the nominal 1 kOhm fast Attack endpoint.
void testEnvelopeRetriggerUsesThePhysicalResetNotch()
{
    const auto reset = ghostar::GhostarCircuitTestAccess::
        envelopeMultipleResetNotch();
    check(reset.releaseSamples == 40 && reset.stayedInRelease,
          "an 8 kHz MULTIPLE retrigger spends 40 samples in the nominal "
          "5 ms GS-low release notch");
    check(reset.afterNotch < reset.before && reset.afterNotch > 0.0,
          "the reset notch releases the cap without dumping its state");
    check(reset.restartedAttack && reset.afterRestart > reset.afterNotch,
          "the final GS rise attacks from the retained post-notch voltage");
    check(ghostar::GhostarCircuitTestAccess::
              xEdgeResetsUnderHeldKeyboardGate(),
          "a selected X rise retriggers beneath an already-high KBD gate");

    constexpr double expectedFastPeak =
        1.0 - 100.0 / 1000.0 * (1.3 - 1.0);
    check(std::abs(ghostar::GhostarCircuitTestAccess::minimumAttackCapPeak()
                   - expectedFastPeak) < 1.0e-12,
          "R23/R24 KVL puts the fast Attack cap peak below its 7.5 V "
          "threshold node");
}

// MOD RATE's 100k linear P2 is loaded by R33=200k before the exponential
// CEM3360 converter. The endpoint ratio remains nominal, but electrical half
// travel is 4/9 of the converter span rather than 1/2.
void testLfoRateIncludesItsLoadedPot()
{
    const double slow =
        ghostar::GhostarCircuitTestAccess::lfoHzForTravel(0.0f);
    const double middle =
        ghostar::GhostarCircuitTestAccess::lfoHzForTravel(0.5f);
    const double fast =
        ghostar::GhostarCircuitTestAccess::lfoHzForTravel(1.0f);
    const double expectedMiddle = 0.3 * std::pow(50.0 / 0.3, 4.0 / 9.0);

    check(std::abs(slow - 0.3) < 1.0e-12,
          "MOD RATE retains its nominal sub-1 Hz endpoint");
    check(std::abs(fast - 50.0) < 1.0e-12,
          "MOD RATE retains the manual's 50 Hz endpoint");
    check(std::abs(middle - expectedMiddle) < 1.0e-12,
          "MOD RATE includes P2's R33-loaded linear travel");
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
    const double oscillatorSingle = parallel(22.0, 100.0);
    const double oscillatorPair = parallel(oscillatorSingle, 100.0);
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

    // The exact RS7 output net for this named detent still needs a hardware
    // continuity table. Do not pin a made-up peak gain to the dry+BP
    // behavioral seam; the live Lower state and limiter have independent
    // component-level tests above.
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

    const double cleanRatio =
        level(ghostar::LowerFilterMode::BandPass, 0.8f)
        / std::max(1.0e-12, level(ghostar::LowerFilterMode::BandPass, 0.2f));
    const double drivenRatio =
        level(ghostar::LowerFilterMode::Overdrive, 0.8f)
        / std::max(1.0e-12, level(ghostar::LowerFilterMode::Overdrive, 0.2f));
    check(cleanRatio > 3.2, "the clean boost scales linearly with its input");
    check(drivenRatio < 0.8 * cleanRatio,
          "the overdrive stage compresses instead of scaling linearly");
}

// The envelope segments are RC charges on the 4.7 uF cap through the 2 MOhm
// log sliders, so the panel's labelled time is the time *constant*: at full
// travel the envelope must fall to 1/e of its span in ~9.40047 s, not reach its
// target in that time. D15 puts zero sustain at the VCA's 1/15 dead zone,
// so the two affine laws cancel and audible gain is exactly 1/e after one
// time constant (OQ-04).
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
    check(ratio > 0.35 && ratio < 0.39,
          "one envelope time constant reaches the affine VCA gain oracle");
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
    // One time constant into an aim of 1.3 reaches 1.3*(1-1/e) = 0.822.
    const double atTau = levelAfter(tau);
    const double atPeak = levelAfter(4.0 * tau);
    const double fraction = atTau / std::max(1.0e-12, atPeak);
    check(fraction > 0.76 && fraction < 0.88,
          "the attack reaches ~82 % of its peak in one time constant, as an "
          "nominal RC model aiming 1.3x past it does");
}

// The travel-to-Q law is derived from the CEM3350's −65 mV/decade Q scale
// and the Spirit's own pot network, anchored by the panel's LOW = Q 0.5.
// Its signature is that resonance stays gentle through mid-travel and then
// climbs steeply: Q ≈ 1.48 at half travel against ≈ 10.9 at nine tenths, a
// ratio near 7.4. A resonant section's peak gain tracks its Q, so the
// measured peak ratio is the law's fingerprint (OQ-12).
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
        parameters.filterPathA = 0.00001f;
        engine.setParameters(parameters);
        engine.noteOn(46, 1.0f);    // ~93 Hz: sixth harmonic near 560 Hz
        const auto samples = renderMono(engine, 0.9, 48000.0, 60);
        return goertzelMagnitude(samples, 566.0, 48000.0);
    };

    const double atHalf = peakGain(0.5f);
    const double atNineTenths = peakGain(0.9f);
    const double ratio = atNineTenths / std::max(1.0e-12, atHalf);
    check(ratio > 4.5 && ratio < 11.0,
          "the resonant peak grows by the derived Q ratio between half and "
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
        return goertzelMagnitude(samples, 566.0, 48000.0)
             / std::max(1.0e-12,
                        goertzelMagnitude(samples, 100.0, 48000.0));
    };
    check(flatness(0.5f) < 3.0,
          "half travel is barely resonant, as Q = 1.5 requires");
}

// The traced BA130/IC12A OVERDRIVE keeps climbing past its knee rather than
// becoming a hard tanh ceiling. OQ-10 still owns absolute state-node scale.
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
        renderMono(engine, 0.75, 48000.0);
        return mean(renderMono(engine, 0.75, 48000.0));
    };

    check(std::abs(dcMean(false)) < 1.0e-3,
          "C30 rejects duty-cycle DC on the Filter path");
    check(std::abs(dcMean(true)) > 1.0e-2,
          "the Shaper jack has no invented output high-pass");
}

// Pin the current behavioral Shaper-VCA seam until P1013's always-biased
// TR2/CEM3360 control node is calibrated (OQ-26): FREE has loud and quiet
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
// a virtual-earth summer. Their Thevenin resistance bends the 47k-arm law:
// half travel is 0.5*47/(47+25)=0.3264 of full, not 0.5. The Filter's two
// moving-node buses are deliberately not asserted here (OQ-20).
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

    // R45 is the distinct errata-corrected 6.8k law. Long, identically seeded
    // renders make the noise comparison deterministic, so the source colour
    // and the voiced full-travel scale cancel from the ratio.
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
    constexpr double expectedNoise = 0.5 * 6.8 / (6.8 + 25.0);
    check(noiseFull > 1.0e-4, "the Shaper-noise loading probe is audible");
    check(std::abs(noiseHalf / noiseFull - expectedNoise) < 0.01,
          "the Shaper Noise slider includes its 6.8k loaded law");

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
    testPulseWidthReachesTheCemEndpoints();
    testHighQCompanionsSatisfyTheirIntegratedEquations();
    testLowerMixerMnaSatisfiesP1013();
    testOverdriveMatchesTheTracedCircuit();
    testOutputCapacitorCompanionsMatchP1013();
    testRingModulatorMatchesP1013();
    testKeyboardLaw();
    testKeyboardTrackingAmount();
    testFullGlideUsesTheResolvedRcEndpoint();
    testLoudnessVcaUsesItsControlOffset();
    testEnvelopeDiodeFloorAndReleaseKnee();
    testEnvelopeRetriggerUsesThePhysicalResetNotch();
    testLfoRateIncludesItsLoadedPot();
    testModulationWheelsIncludeDestinationLoading();
    testMasterOctave();
    testPulseDuties();
    testHardSync();
    testLowpassAttenuationIsMonotonic();
    testSlopeSwitch();
    testLowerBandPassIsParametricBoost();
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
