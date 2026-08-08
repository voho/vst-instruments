// Engine behaviour suite: what the instrument does when it is played, as
// opposed to what its individual circuit blocks compute. The circuit suite
// checks the laws; this one checks the machine.

#include "DSP/YouKnow106Chorus.h"
#include "DSP/YouKnow106Engine.h"
#include "DSP/YouKnow106Panel.h"
#include "DSP/YouKnow106Presets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <iostream>
#include <string>
#include <vector>

namespace youknow106
{
// Narrow friend probe for the engine-state regressions below. The circuit
// suite has its own executable-local definition for circuit internals.
struct YouKnow106TestAccess
{
    static int lastVoiceMidi(const YouKnow106Engine& engine, int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].lastVoiceMidi;
    }

    static bool dcoResetPending(const YouKnow106Engine& engine, int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].dcoResetPending;
    }

    static void updateVoiceScan(YouKnow106Engine& engine, int slot,
                                const EngineParameters& parameters) noexcept
    {
        engine.updateVoiceScan(engine.voices_[static_cast<std::size_t>(slot)],
                               parameters, 0.0f);
    }

    static float filterG(const YouKnow106Engine& engine, int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].filterG;
    }

    static float stageGScale(const YouKnow106Engine& engine, int slot,
                             int stage) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)]
            .filter.gScale[static_cast<std::size_t>(stage)];
    }

    static float stageOffset(const YouKnow106Engine& engine, int slot,
                             int stage) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)]
            .filter.offsetVoltage[static_cast<std::size_t>(stage)];
    }

    static float cardStageOffset(const YouKnow106Engine& engine, int slot,
                                 int stage) noexcept
    {
        return engine.cards_[static_cast<std::size_t>(slot)]
            .vcfStageOffsets[static_cast<std::size_t>(stage)];
    }

    static float pwmTarget(const YouKnow106Engine& engine) noexcept
    {
        return engine.pwmVoltsTarget_;
    }

    static float lfoValue(const YouKnow106Engine& engine) noexcept
    {
        return engine.lfoValue_;
    }

    // The single delay attenuator, read before it is multiplied into anything.
    static float lfoDelayLevel(const YouKnow106Engine& engine) noexcept
    {
        return engine.lfoDelayLevel_;
    }

    static float pwmHeld(const YouKnow106Engine& engine) noexcept
    {
        return engine.pwmVolts_;
    }

    static float subTarget(const YouKnow106Engine& engine) noexcept
    {
        return engine.subCvTarget_;
    }

    static float noiseTarget(const YouKnow106Engine& engine) noexcept
    {
        return engine.noiseCvTarget_;
    }

    static double dcoPhase(const YouKnow106Engine& engine, int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].dco.phase;
    }

    static void setDcoPhase(YouKnow106Engine& engine, int slot,
                            double phase) noexcept
    {
        engine.voices_[static_cast<std::size_t>(slot)].dco.phase = phase;
    }

    static double dcoPeriodSamples(const YouKnow106Engine& engine,
                                   int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].dco.periodSamples;
    }

    static float dcoResetFraction(const YouKnow106Engine& engine,
                                  int slot) noexcept
    {
        return engine.resetFraction(
            engine.voices_[static_cast<std::size_t>(slot)].dco.periodSamples
            * engine.inverseOversampledRate_);
    }

    static double chorusPhase(const YouKnow106Engine& engine) noexcept
    {
        return engine.chorus_.getLfoPhase();
    }

    static float pulseDuty(const YouKnow106Engine& engine, int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].pulseDuty;
    }

    static float pulseLogicState(const YouKnow106Engine& engine,
                                 int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].dco.pulseState;
    }

    static bool pulseDutyHistoryIsPrimed(const YouKnow106Engine& engine,
                                         int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].pulseDutyPrimed;
    }

    static void primePulseDutyHistory(YouKnow106Engine& engine, int slot,
                                      float duty) noexcept
    {
        auto& voice = engine.voices_[static_cast<std::size_t>(slot)];
        voice.previousPulseDuty = duty;
        voice.pulseDutyPrimed = true;
    }

    static bool pulseMixEnabled(const YouKnow106Engine& engine, int slot,
                                bool requested) noexcept
    {
        return engine.pulseMixEnabled(
            requested,
            engine.voices_[static_cast<std::size_t>(slot)].pulseDuty);
    }

    static float currentMidi(const YouKnow106Engine& engine, int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].currentMidi;
    }

    static float vcaControl(const YouKnow106Engine& engine, int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].vcaControl;
    }

    static float sharedVca(const YouKnow106Engine& engine) noexcept
    {
        return engine.sharedVca_;
    }

    static void setSharedVca(YouKnow106Engine& engine, float state) noexcept
    {
        engine.sharedVca_ = state;
    }

    static void setPwmHeld(YouKnow106Engine& engine, float state) noexcept
    {
        engine.pwmVoltsFirstPole_ = state;
        engine.pwmVolts_ = state;
    }

    static constexpr float pwmFirstPoleSeconds() noexcept
    {
        return YouKnow106Engine::pwmHoldFirstPoleSeconds;
    }

    static constexpr float pwmSecondPoleSeconds() noexcept
    {
        return YouKnow106Engine::pwmHoldSecondPoleSeconds;
    }

    static constexpr float subSlewSeconds() noexcept
    {
        return YouKnow106Engine::subHoldSlewSeconds;
    }

    static float subHeld(const YouKnow106Engine& engine) noexcept
    {
        return engine.subCv_;
    }

    static void setSubHeld(YouKnow106Engine& engine, float state) noexcept
    {
        engine.subCv_ = state;
    }

    static bool assignmentPending(const YouKnow106Engine& engine) noexcept
    {
        return engine.assignmentRescanPending_;
    }

    static std::array<float, 4> filterState(const YouKnow106Engine& engine,
                                            int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].filter.state;
    }

    // The fourth transconductor's capacitor voltage: the filter's output, in
    // the volts the service procedure measures at TP19.
    static float filterOutputVolts(const YouKnow106Engine& engine,
                                   int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].filter.voltage[3];
    }

    // Pin 9 VCA IN: the filter output after C59, which is the value the voice
    // amplifier multiplies. Reading it here rather than in the mix is the only
    // way to see this node's DC at all -- three further couplings downstream
    // of the multiply remove any that survives it.
    static float vcaInputVolts(const YouKnow106Engine& engine,
                               int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].vcaInputVolts;
    }

    static bool voiceActive(const YouKnow106Engine& engine, int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].active;
    }

    static void setFilterState(YouKnow106Engine& engine, int slot,
                               const std::array<float, 4>& state) noexcept
    {
        auto& filter =
            engine.voices_[static_cast<std::size_t>(slot)].filter;
        filter.state = state;
        filter.voltage = state;
    }

    static std::uint32_t microscopicNoiseState(
        const YouKnow106Engine& engine, int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].noiseState;
    }

    static void setMicroscopicNoiseState(YouKnow106Engine& engine, int slot,
                                         std::uint32_t state) noexcept
    {
        engine.voices_[static_cast<std::size_t>(slot)].noiseState = state;
    }

    static void initialiseVoice(YouKnow106Engine& engine, int slot,
                                int midiNote) noexcept
    {
        engine.initialiseVoice(
            engine.voices_[static_cast<std::size_t>(slot)], slot, midiNote,
            1.0f);
    }

    static void silenceVoice(YouKnow106Engine& engine, int slot) noexcept
    {
        engine.silenceVoice(
            engine.voices_[static_cast<std::size_t>(slot)]);
    }

    static float renderSilentVoiceAtControlOffset(
        YouKnow106Engine& engine, float controlOffset) noexcept
    {
        constexpr int slot = 0;
        auto& voice = engine.voices_[slot];
        auto& card = engine.cards_[slot];
        engine.initialiseVoice(voice, slot, 60, 1.0f);
        voice.filter.reset();
        voice.filterG = 0.0f;
        voice.feedback = 0.0f;
        voice.vcaControl = 1.0f;
        voice.vcaControlTarget = 1.0f;
        voice.vca = 1.0f;
        card.vcaControlOffset = controlOffset;

        EngineParameters parameters;
        parameters.sawEnabled = false;
        parameters.pulseEnabled = false;
        parameters.subLevel = 0.0f;
        parameters.noiseLevel = 0.0f;
        parameters.cutoff = 0.0f;
        parameters.resonance = 0.0f;
        parameters.calibration = 1.0f;
        parameters.enableVcfStageOffsets = false;
        parameters.enableOpAmpSlewLimiting = false;
        parameters.enableVcfEarlyEffect = false;
        parameters.enableSpatialThermalGradient = false;
        return engine.renderVoice(voice, parameters, 0.0f);
    }

    static double controlScanPhase(const YouKnow106Engine& engine) noexcept
    {
        return engine.controlScanPhase_;
    }

    static float glidedVolume(const YouKnow106Engine& engine) noexcept
    {
        return engine.glidedVolume_;
    }

    static float rateTransitionGain(const YouKnow106Engine& engine) noexcept
    {
        return engine.rateTransitionGain_;
    }

    static double outputCouplingState(const YouKnow106Engine& engine) noexcept
    {
        return engine.outputCouplingLeft_.state;
    }

    struct BandlimitedTrackState
    {
        std::array<float, YouKnow106Engine::correctionRing> ring {};
        std::array<float, YouKnow106Engine::correctionHalfWidth> delay {};
        int base { 0 };
        bool primed { false };
    };

    static BandlimitedTrackState pulseTrackState(
        const YouKnow106Engine& engine, int slot) noexcept
    {
        const auto& track =
            engine.voices_[static_cast<std::size_t>(slot)].dco.pulse;
        return { track.ring, track.delay, track.base, track.primed };
    }

    static BandlimitedTrackState sawTrackState(
        const YouKnow106Engine& engine, int slot) noexcept
    {
        const auto& track =
            engine.voices_[static_cast<std::size_t>(slot)].dco.saw;
        return { track.ring, track.delay, track.base, track.primed };
    }

    static BandlimitedTrackState subTrackState(
        const YouKnow106Engine& engine, int slot) noexcept
    {
        const auto& track =
            engine.voices_[static_cast<std::size_t>(slot)].dco.sub;
        return { track.ring, track.delay, track.base, track.primed };
    }

    static void primeDcoRestartFixture(YouKnow106Engine& engine, int slot,
                                       double phase) noexcept
    {
        auto& dco = engine.voices_[static_cast<std::size_t>(slot)].dco;
        const double reset = YouKnow106Engine::resetFraction(
            dco.periodSamples * engine.inverseOversampledRate_);
        const double rise = std::max(1.0 - reset, 1.0e-4);
        const float saw = phase < rise
            ? 2.0f * std::clamp(
                  static_cast<float>(phase / rise), 0.0f, 1.0f) - 1.0f
            : 1.0f - 2.0f * static_cast<float>((phase - rise) / reset);
        dco.phase = phase;
        dco.pulseState = 1.0f;
        dco.subState = -1.0f;
        dco.saw.reset();
        dco.pulse.reset();
        dco.sub.reset();
        dco.saw.prime(saw);
        dco.pulse.prime(dco.pulseState);
        dco.sub.prime(dco.subState);
    }

    static void performPitchWrite(YouKnow106Engine& engine, int slot,
                                  const EngineParameters& parameters) noexcept
    {
        engine.performConverterWrite(
            { YouKnow106Engine::ConverterDestination::Pitch, slot },
            parameters, 0.0f);
    }

    static std::array<float, 10> pulseTrackAfterRestart(
        const YouKnow106Engine& engine, int slot) noexcept
    {
        auto track = engine.voices_[static_cast<std::size_t>(slot)].dco.pulse;
        std::array<float, 10> output {};
        for (auto& sample : output)
            sample = track.advance(-1.0f);
        return output;
    }

    static float dcoRenderScale(const YouKnow106Engine& engine,
                                int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].dco.renderScale;
    }

    static double sawRestartEventSideError(
        const YouKnow106Engine& engine, int slot,
        double previousPeriodSamples, double previousPhase,
        float previousScale) noexcept
    {
        const auto& dco =
            engine.voices_[static_cast<std::size_t>(slot)].dco;
        auto actual = dco.saw;
        auto reference = actual;
        reference.ring.fill(0.0f);

        const auto geometry = [&engine](double periodSamples) {
            const double safePeriod = std::max(periodSamples, 1.0e-9);
            const double reset = YouKnow106Engine::resetFraction(
                safePeriod * engine.inverseOversampledRate_);
            const double rise = std::max(1.0 - reset, 1.0e-4);
            return std::array { safePeriod, reset, rise };
        };
        const auto oldGeometry = geometry(previousPeriodSamples);
        const auto newGeometry = geometry(dco.periodSamples);
        // Both timelines in the scaled naive domain the track carries: the
        // abandoned ramp under its own frozen ratio, the restarted ramp under
        // the ratio the restart froze (readable from the post-restart state).
        const float newScale = dco.renderScale;
        const float oldSawUnit = previousPhase < oldGeometry[2]
            ? 2.0f * std::clamp(
                  static_cast<float>(previousPhase / oldGeometry[2]),
                  0.0f, 1.0f) - 1.0f
            : 1.0f - 2.0f * static_cast<float>(
                  (previousPhase - oldGeometry[2]) / oldGeometry[1]);
        const float oldSaw =
            oldSawUnit * previousScale + (previousScale - 1.0f);
        constexpr float newSaw = -1.0f;
        const float oldSlope = (previousPhase < oldGeometry[2]
            ? 2.0f / static_cast<float>(oldGeometry[2] * oldGeometry[0])
            : -2.0f / static_cast<float>(oldGeometry[1] * oldGeometry[0]))
            * previousScale;
        const float newSlope =
            2.0f / static_cast<float>(newGeometry[2] * newGeometry[0])
            * newScale;

        // Build an independent residual from the values renderVoice actually
        // submits one interval after the converter write. Comparing advanced
        // tracks, rather than mere ring energy, catches a phase-zero step that
        // forgets the two ramps' different one-sample slope advances.
        engine.addStep(reference,
                       (newSaw + newSlope) - (oldSaw + oldSlope), 0.0f);
        engine.addSlope(reference, newSlope - oldSlope, 0.0f);

        double phase = 0.0;
        double maximumError = 0.0;
        for (int sample = 0; sample < 10; ++sample)
        {
            phase += 1.0 / newGeometry[0];
            phase -= std::floor(phase);
            const float naive = phase < newGeometry[2]
                ? 2.0f * std::clamp(
                      static_cast<float>(phase / newGeometry[2]),
                      0.0f, 1.0f) - 1.0f
                : 1.0f - 2.0f * static_cast<float>(
                      (phase - newGeometry[2]) / newGeometry[1]);
            maximumError = std::max(
                maximumError,
                std::abs(static_cast<double>(actual.advance(naive))
                         - reference.advance(naive)));
        }
        return maximumError;
    }

    static float dcoCv(const YouKnow106Engine& engine, int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].dcoCv;
    }

    // The chassis warm-up clock and the exponential the voices read off it.
    static double thermalWarmupSeconds(const YouKnow106Engine& engine) noexcept
    {
        return engine.thermalWarmupSeconds_;
    }

    static float thermalWarmupFraction(const YouKnow106Engine& engine) noexcept
    {
        return engine.thermalWarmupFraction_;
    }

    static double internalRate(const YouKnow106Engine& engine) noexcept
    {
        return engine.oversampledRate_;
    }

    // Advances the warm-up clock by calling the one function the render loop
    // calls, once per internal sample. It reads `inverseOversampledRate_`, so
    // the increment is whatever rate `prepare()` selected -- a fixture driving
    // this is exercising the rate dependence, not assuming it away. It skips
    // the surrounding render work and nothing else, which is what makes a
    // fifteen-minute warm-up affordable in a unit test.
    static void advanceThermalWarmup(YouKnow106Engine& engine,
                                     long long internalSamples) noexcept
    {
        for (long long sample = 0; sample < internalSamples; ++sample)
            engine.advanceThermalWarmup();
    }

    // The OTA headroom `renderVoice` solves the cascade with, read from the
    // engine's own law rather than restated here.
    static float otaHeadroomVolts(const YouKnow106Engine& engine,
                                  const EngineParameters& parameters,
                                  int cardIndex) noexcept
    {
        return engine.dynamicOtaHeadroomVolts(parameters, cardIndex);
    }

    static float dcoCvTarget(const YouKnow106Engine& engine,
                             int slot) noexcept
    {
        return engine.voices_[static_cast<std::size_t>(slot)].dcoCvTarget;
    }

    static std::array<float, 2> dcoLogicStates(
        const YouKnow106Engine& engine, int slot) noexcept
    {
        const auto& dco =
            engine.voices_[static_cast<std::size_t>(slot)].dco;
        return { dco.pulseState, dco.subState };
    }
};
} // namespace youknow106

namespace
{
using namespace youknow106;

int failures = 0;

// The build system defines this whenever it adds -fsanitize, because no macro
// covers every case: GCC with only -fsanitize=undefined defines neither
// __has_feature nor __SANITIZE_UNDEFINED__, and the realtime assertion below
// would then hold instrumented code to an uninstrumented budget. The compiler
// macros stay as a fallback for a sanitizer build configured some other way.
#if defined(YOUKNOW106_SANITIZER_BUILD)
constexpr bool sanitizerBuild = YOUKNOW106_SANITIZER_BUILD != 0;
#elif defined(__has_feature)
constexpr bool sanitizerBuild = __has_feature(address_sanitizer)
                             || __has_feature(undefined_behavior_sanitizer);
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__)
constexpr bool sanitizerBuild = true;
#else
constexpr bool sanitizerBuild = false;
#endif

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
constexpr int blockSize = 256;
// Mirrors the engine's own private constant so the test states the bound it
// checks rather than importing it.
constexpr float vcfStageCapacitorToleranceForTest = 0.02f;

struct Render
{
    std::vector<float> left;
    std::vector<float> right;
};

Render render(YouKnow106Engine& engine, int samples)
{
    Render result;
    const int blocks = (samples + blockSize - 1) / blockSize;
    result.left.assign(static_cast<std::size_t>(blocks * blockSize), 0.0f);
    result.right.assign(static_cast<std::size_t>(blocks * blockSize), 0.0f);
    for (int block = 0; block < blocks; ++block)
        engine.process(result.left.data() + block * blockSize,
                       result.right.data() + block * blockSize, blockSize);
    return result;
}

// Unlike render(), this does not round the request up to a host block. Tests
// that exercise converter-boundary ordering use it to stop on an exact scan
// phase.
Render renderExact(YouKnow106Engine& engine, int samples)
{
    Render result;
    result.left.assign(static_cast<std::size_t>(samples), 0.0f);
    result.right.assign(static_cast<std::size_t>(samples), 0.0f);
    for (int offset = 0; offset < samples; offset += blockSize)
    {
        const int count = std::min(blockSize, samples - offset);
        engine.process(result.left.data() + offset,
                       result.right.data() + offset, count);
    }
    return result;
}

double maximumDifference(const std::vector<float>& first,
                         const std::vector<float>& second)
{
    const std::size_t count = std::min(first.size(), second.size());
    double difference = 0.0;
    for (std::size_t index = 0; index < count; ++index)
        difference = std::max(
            difference,
            static_cast<double>(std::abs(first[index] - second[index])));
    return difference;
}

EngineParameters plainPatch()
{
    EngineParameters parameters;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.envDepth = 0.0f;
    parameters.keyFollow = 0.0f;
    parameters.attack = 0.0f;
    parameters.decay = 1.0f;
    parameters.sustain = 1.0f;
    parameters.release = 0.0f;
    // The stored LEVEL slider is a shared post-sum trim. Byte 99 is the nearest
    // stored setting to 0 dB in the nominal jack-board/NEC solve (+0.073 dB);
    // full travel adds 4.71 dB and changes the downstream chorus drive.
    parameters.vcaLevel = 99.0f / 127.0f;
    parameters.volume = 1.0f;
    parameters.chorus = ChorusMode::Off;
    // The plain reference patch carries no character at all: no component
    // spread, no drift, and none of the inherent circuit non-linearities --
    // all of it answers to this one control.
    parameters.calibration = 0.0f;
    return parameters;
}

EngineParameters parametersFor(const sysex::Patch& patch)
{
    EngineParameters parameters;
    parameters.lfoRate = patch.lfoRate;
    parameters.lfoDelay = patch.lfoDelay;
    parameters.dcoLfoDepth = patch.dcoLfo;
    parameters.pwmDepth = patch.pwm;
    parameters.noiseLevel = patch.noise;
    parameters.cutoff = patch.cutoff;
    parameters.resonance = patch.resonance;
    parameters.envDepth = patch.vcfEnv;
    parameters.vcfLfoDepth = patch.vcfLfo;
    parameters.keyFollow = patch.keyFollow;
    parameters.vcaLevel = patch.vcaLevel;
    parameters.attack = patch.attack;
    parameters.decay = patch.decay;
    parameters.sustain = patch.sustain;
    parameters.release = patch.release;
    parameters.subLevel = patch.sub;
    parameters.range = patch.range;
    parameters.sawEnabled = patch.saw;
    parameters.pulseEnabled = patch.pulse;
    parameters.pwmSource = patch.pwmSource;
    parameters.vcaMode = patch.vcaMode;
    parameters.envPolarity = patch.envPolarity;
    parameters.highPass = patch.highPass;
    parameters.chorus = patch.chorus;
    parameters.calibration = 0.0f;
    parameters.chorusNoise = 0.0f;
    return parameters;
}

double peakOf(const std::vector<float>& signal, std::size_t from)
{
    double peak = 0.0;
    for (std::size_t index = from; index < signal.size(); ++index)
        peak = std::max(peak, static_cast<double>(std::abs(signal[index])));
    return peak;
}

struct OutputMetrics
{
    double rms {};
    double samplePeak {};
    double truePeak4x {};
    std::uint64_t sampleOverloads {};
    std::uint64_t reconstructedOverloads {};
};

// Four equally spaced evaluations per PCM interval. Each fractional sample is
// reconstructed with a 24-tap, DC-normalised sinc (12 source samples either
// side) multiplied by a symmetric Blackman window. Integer phases reproduce
// the stored sample; out-of-range taps are zero. The corpus leaves guard audio
// on both sides of its measured window, so those zeros never enter a result.
double reconstructedSample4x(const std::vector<float>& signal,
                             double position)
{
    constexpr int radius = 12;
    const int centre = static_cast<int>(std::floor(position));
    double sum = 0.0;
    double weight = 0.0;

    for (int source = centre - radius + 1; source <= centre + radius; ++source)
    {
        const double distance = position - source;
        const double absoluteDistance = std::abs(distance);
        if (absoluteDistance >= radius)
            continue;

        const double sinc = absoluteDistance < 1.0e-12
            ? 1.0 : std::sin(pi * distance) / (pi * distance);
        const double window = 0.42
                            + 0.50 * std::cos(pi * distance / radius)
                            + 0.08 * std::cos(2.0 * pi * distance / radius);
        const double coefficient = sinc * window;
        weight += coefficient;
        if (source >= 0 && source < static_cast<int>(signal.size()))
            sum += coefficient * signal[static_cast<std::size_t>(source)];
    }
    return weight != 0.0 ? sum / weight : 0.0;
}

OutputMetrics outputMetrics(const Render& rendered, std::size_t start,
                            std::size_t count)
{
    expect(start >= 12 && start + count + 12 <= rendered.left.size(),
           "the output corpus lacks true-peak reconstruction guards");

    OutputMetrics metrics;
    double energy = 0.0;
    const auto inspectPcm = [&](const std::vector<float>& channel) {
        for (std::size_t index = start; index < start + count; ++index)
        {
            const double sample = channel[index];
            energy += sample * sample;
            metrics.samplePeak = std::max(metrics.samplePeak, std::abs(sample));
            if (std::abs(sample) > 1.0)
                ++metrics.sampleOverloads;
        }
    };
    inspectPcm(rendered.left);
    inspectPcm(rendered.right);
    metrics.rms = std::sqrt(energy / static_cast<double>(count * 2));

    const auto inspectReconstruction = [&](const std::vector<float>& channel) {
        for (std::size_t index = start; index < start + count; ++index)
            for (int phase = 0; phase < 4; ++phase)
            {
                const double reconstructed = reconstructedSample4x(
                    channel, static_cast<double>(index) + phase * 0.25);
                metrics.truePeak4x = std::max(metrics.truePeak4x,
                                               std::abs(reconstructed));
                if (std::abs(reconstructed) > 1.0)
                    ++metrics.reconstructedOverloads;
            }
    };
    inspectReconstruction(rendered.left);
    inspectReconstruction(rendered.right);
    return metrics;
}

double magnitudeAt(const std::vector<float>& signal, std::size_t start, int length,
                   double frequency, double sampleRate)
{
    std::complex<double> accumulator {};
    for (int index = 0; index < length; ++index)
    {
        const double window = 0.5 - 0.5 * std::cos(2.0 * pi * index / (length - 1));
        accumulator += signal[start + static_cast<std::size_t>(index)] * window
                     * std::exp(std::complex<double>(0.0, -2.0 * pi * frequency
                                                              * index / sampleRate));
    }
    return std::abs(accumulator) / (length * 0.5);
}

// Frequency measured between the first and last rising zero crossing, which
// resolves far better than counting crossings over a fixed window.
double measuredFrequency(const std::vector<float>& signal, std::size_t from,
                         double sampleRate)
{
    std::size_t first = 0;
    std::size_t last = 0;
    int intervals = -1;
    for (std::size_t index = from + 1; index < signal.size(); ++index)
        if (signal[index - 1] <= 0.0f && signal[index] > 0.0f)
        {
            if (intervals < 0)
                first = index;
            last = index;
            ++intervals;
        }
    return intervals > 0 ? intervals * sampleRate / static_cast<double>(last - first)
                         : 0.0;
}

// --------------------------------------------------------------------------

void testRangeTransposesByOctaves()
{
    constexpr double sampleRate = 96000.0;
    const std::array<DcoRange, 3> ranges { DcoRange::Sixteen, DcoRange::Eight,
                                           DcoRange::Four };
    std::array<double, 3> measured {};

    for (std::size_t index = 0; index < ranges.size(); ++index)
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.range = ranges[index];
        engine.setParameters(parameters);
        engine.noteOn(69, 1.0f);
        const auto rendered = render(engine, static_cast<int>(sampleRate));
        measured[index] = measuredFrequency(rendered.left, rendered.left.size() / 2,
                                            sampleRate);
    }

    expectNear(measured[1] / measured[0], 2.0, 0.01,
               "8' does not sound an octave above 16'");
    expectNear(measured[2] / measured[1], 2.0, 0.01,
               "4' does not sound an octave above 8'");
    expectNear(measured[1], 440.0, 1.0, "8' does not sound at concert pitch");
}

void testSubIsOneOctaveDown()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);

    auto parameters = plainPatch();
    parameters.sawEnabled = false;
    parameters.subLevel = 1.0f;
    engine.setParameters(parameters);
    engine.noteOn(69, 1.0f);

    const auto rendered = render(engine, static_cast<int>(sampleRate));
    const auto frequency = measuredFrequency(rendered.left, rendered.left.size() / 2,
                                             sampleRate);
    expectNear(frequency, 220.0, 1.0, "the sub is not one octave below the note");
}

void testSelfOscillationLandsOnTheServiceAnchor()
{
    // The service procedure's own check: converter code 6272 -- panel byte
    // 49 -- must self-oscillate at 248 Hz. The loop's own divider-limited
    // transconductor is what bounds the limit cycle, and the per-voice
    // trimmer absorbs the forward stages' residual compression, so the
    // rendered oscillation must land on the law within a few cents, as a
    // calibrated instrument's does.
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);

    auto parameters = plainPatch();
    parameters.sawEnabled = false;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.resonance = 1.0f;
    parameters.envDepth = 0.0f;
    parameters.keyFollow = 0.0f;
    parameters.attack = 0.0f;
    parameters.sustain = 1.0f;
    parameters.cutoff = 49.0f / 127.0f;
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);

    const auto rendered = render(engine, static_cast<int>(sampleRate * 3.0));
    const auto frequency = measuredFrequency(
        rendered.left, rendered.left.size() * 2 / 3, sampleRate);
    const double law = YouKnow106Engine::vcfCutoffHz(
        YouKnow106Engine::vcfPanelCounts(parameters.cutoff));
    expectNear(law, 248.1, 0.5, "panel byte 49 does not read the service code");
    expectNear(1200.0 * std::log2(frequency / law), 0.0, 5.0,
               "the self-oscillation does not land on the service anchor");
}

void testFilterPolesAreStaggeredOnlyByUnitCharacter()
{
    // The four IR3109 sections integrate into their own 240 pF capacitors, so
    // their poles do not coincide the way one shared `g` makes them. That
    // spread is a card dispersion like the stage offsets: signed, unbiased,
    // and absent at the calibrated reference rather than merely small.
    const auto scales = [](float calibration) {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, true);
        auto parameters = plainPatch();
        parameters.calibration = calibration;
        engine.setParameters(parameters);
        engine.noteOn(60, 1.0f);
        std::array<float, 4> result {};
        for (int stage = 0; stage < 4; ++stage)
            result[static_cast<std::size_t>(stage)] =
                YouKnow106TestAccess::stageGScale(engine, 0, stage);
        return result;
    };

    for (const float scale : scales(0.0f))
        expect(scale == 1.0f,
               "the filter poles are staggered in the calibrated nominal model");

    const auto spread = scales(1.0f);
    bool anyDifferent = false;
    for (int stage = 0; stage < 4; ++stage)
    {
        const float scale = spread[static_cast<std::size_t>(stage)];
        anyDifferent = anyDifferent || scale != 1.0f;
        // A capacitor tolerance, not a filter control: the pole must stay
        // where a 240 pF part with a few percent spread puts it.
        expect(std::abs(scale - 1.0f) <= 1.05f * vcfStageCapacitorToleranceForTest,
               "a filter pole moved further than the capacitor class allows");
        expect(scale > 0.0f, "a filter pole scale went non-positive");
    }
    expect(anyDifferent,
           "the filter poles all coincide at full Unit Character");
}

void testMixerLevelIsContinuousInSubAndNoise()
{
    // SUB LEVEL and NOISE LEVEL are sliders that vary an amplitude, not
    // switches that connect a leg. Their 100 kOhm resistors are wired whatever
    // the slider reads, so leaving the stop must not change how hard the other
    // legs load the summing node. Counting them only above zero stepped the
    // whole voice by 2.95 dB at the first non-zero byte.
    const auto levelAt = [](float subLevel, float noiseLevel) {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, true);
        auto parameters = plainPatch();
        parameters.subLevel = subLevel;
        parameters.noiseLevel = noiseLevel;
        engine.setParameters(parameters);
        engine.noteOn(48, 1.0f);
        const auto rendered = render(engine, 24000);
        double sumOfSquares = 0.0;
        const std::size_t start = 12000;
        for (std::size_t index = start; index < rendered.left.size(); ++index)
            sumOfSquares += static_cast<double>(rendered.left[index])
                          * static_cast<double>(rendered.left[index]);
        return std::sqrt(sumOfSquares
                         / static_cast<double>(rendered.left.size() - start));
    };

    // One converter step is 1/127 of travel; the sub it adds at that code is
    // far below the level change a disconnected leg would have caused.
    const double atRest = levelAt(0.0f, 0.0f);
    const double justOff = levelAt(1.0f / 127.0f, 0.0f);
    expect(atRest > 0.0, "the reference patch produced no output");
    const double stepDb = std::abs(20.0 * std::log10(justOff / atRest));
    expect(stepDb < 0.5,
           "leaving the SUB stop moved the voice by " + std::to_string(stepDb)
               + " dB; that leg is wired whether or not it is turned up");

    const double noiseJustOff = levelAt(0.0f, 1.0f / 127.0f);
    const double noiseStepDb = std::abs(20.0 * std::log10(noiseJustOff / atRest));
    expect(noiseStepDb < 0.5,
           "leaving the NOISE stop moved the voice by "
               + std::to_string(noiseStepDb) + " dB");
}

void testUnisonDoesNotBeat()
{
    // Six voices on one key must not beat against each other at *any* Unit
    // Character setting. Every note timer divides the same crystal-derived
    // clock by the same integer, so the six cards are exactly in tune by
    // construction; no component tolerance, temperature or supply term can
    // reach that division. This is the property the whole DCO architecture
    // exists to provide, and it is what separates the instrument from its
    // VCO contemporaries.
    //
    // Beating is measured rather than inferred: the six cards start their
    // ramps at their own converter slots, so they hold fixed phase offsets
    // and the summed level is steady. Detune them and those offsets slide,
    // which shows up here as the window-to-window level moving.
    for (float calibration : { 0.0f, 1.0f })
    {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, true);
        auto parameters = plainPatch();
        parameters.calibration = calibration;
        parameters.keyMode = KeyMode::Unison;
        // An open, unresonant filter and a flat envelope leave the summed
        // oscillators as the only thing that can move the level.
        parameters.cutoff = 1.0f;
        parameters.resonance = 0.0f;
        parameters.envDepth = 0.0f;
        parameters.sustain = 1.0f;
        engine.setParameters(parameters);
        engine.noteOn(48, 1.0f);

        const auto rendered = render(engine, 144000); // three seconds

        // Skip the attack and the hold capacitors' settling, then compare
        // 50 ms windows across the remaining sustain.
        const std::size_t start = 24000;
        const int window = 2400;
        double loudest = 0.0;
        double quietest = std::numeric_limits<double>::infinity();
        for (std::size_t offset = start; offset + window <= rendered.left.size();
             offset += window)
        {
            double sumOfSquares = 0.0;
            for (int index = 0; index < window; ++index)
            {
                const double value = rendered.left[offset + static_cast<std::size_t>(index)];
                sumOfSquares += value * value;
            }
            const double rms = std::sqrt(sumOfSquares / window);
            loudest = std::max(loudest, rms);
            quietest = std::min(quietest, rms);
        }

        expect(quietest > 0.0, "the unison stack produced no output");
        // A 13-cent spread across six cards beats at about 1 Hz on this note
        // and swings the window level by tens of decibels. Anything the shared
        // clock can legitimately produce is flat to well under a decibel.
        const double swingDb = 20.0 * std::log10(loudest / quietest);
        expect(swingDb < 1.0,
               "unison voices beat by " + std::to_string(swingDb)
                   + " dB at Unit Character " + std::to_string(calibration)
                   + "; the six cards share one crystal and cannot detune");
    }
}

void testAliasFloor()
{
    // The oscillator's discontinuities are repaired with residuals built by
    // integration; a broken table shows up here as a raised noise floor rather
    // than as anything obviously wrong in the waveform.
    for (double sampleRate : { 44100.0, 48000.0 })
    {
        for (int note : { 60, 84 })
        {
            YouKnow106Engine engine;
            engine.prepare(sampleRate, blockSize, true);
            auto parameters = plainPatch();
            engine.setParameters(parameters);
            engine.noteOn(note, 1.0f);

            const auto rendered = render(engine, static_cast<int>(sampleRate));
            const double wanted = 440.0 * std::pow(2.0, (note - 69) / 12.0);
            const double f0 = YouKnow106Engine::dcoQuantisedFrequency(
                YouKnow106Engine::dcoDivider(wanted), DcoRange::Eight);

            const std::size_t start = static_cast<std::size_t>(sampleRate * 0.5);
            const int length = 16384;
            double harmonic = 0.0;
            for (int index = 1; index * f0 < 20000.0; ++index)
                harmonic = std::max(harmonic, magnitudeAt(rendered.left, start, length,
                                                          index * f0, sampleRate));

            double worst = 0.0;
            for (double frequency = 100.0; frequency < 20000.0; frequency += 41.0)
            {
                bool nearHarmonic = false;
                for (int index = 1; index * f0 < 24000.0; ++index)
                    if (std::abs(frequency - index * f0) < 60.0)
                    {
                        nearHarmonic = true;
                        break;
                    }
                if (nearHarmonic)
                    continue;
                worst = std::max(worst, magnitudeAt(rendered.left, start, length,
                                                    frequency, sampleRate));
            }

                const double decibels = 20.0 * std::log10((worst + 1.0e-15) / harmonic);
            if (std::getenv("YOUKNOW106_AUDIT_ALIAS") != nullptr)
                std::cout << "ALIAS note " << note << " at "
                          << static_cast<int>(sampleRate) << " Hz: "
                          << decibels << " dB\n";
            // Tightened from -55 dB once the residual tables were read with
            // interpolation. The worst case measures about -71.6 dB, so this
            // is a real bound rather than a slack one; the remaining floor is
            // not set by the residual tables, since neither interpolation nor
            // a wider kernel moves it.
            expect(decibels < -70.0,
                   "alias floor for note " + std::to_string(note) + " at "
                       + std::to_string(static_cast<int>(sampleRate))
                       + " Hz is only " + std::to_string(decibels) + " dB down");
        }
    }
}

void testRampHasARampSpectrum()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    engine.setParameters(plainPatch());
    engine.noteOn(45, 1.0f);

    const auto rendered = render(engine, static_cast<int>(sampleRate));
    const double f0 = YouKnow106Engine::dcoQuantisedFrequency(
        YouKnow106Engine::dcoDivider(110.0), DcoRange::Eight);
    const std::size_t start = static_cast<std::size_t>(sampleRate * 0.5);

    const double fundamental = magnitudeAt(rendered.left, start, 16384, f0, sampleRate);
    for (int harmonic : { 2, 3, 5, 9 })
    {
        const double measured = magnitudeAt(rendered.left, start, 16384,
                                            harmonic * f0, sampleRate);
        const double expected = fundamental / harmonic;
        expectNear(20.0 * std::log10(measured / expected), 0.0, 1.5,
                   "harmonic " + std::to_string(harmonic)
                       + " does not follow a ramp's 1/n envelope");
    }
}

void testKeyAssignerDropsRatherThanSteals()
{
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.sustain = 1.0f;
    engine.setParameters(parameters);

    for (int note = 60; note < 68; ++note)
        engine.noteOn(note, 1.0f);
    render(engine, 4096);
    expect(engine.getActiveVoiceCount() == 6,
           "the key assigner does not stop at six voices");

    // The two extra keys must have been dropped, not swapped in: releasing the
    // first six has to silence the instrument completely.
    for (int note = 60; note < 68; ++note)
        engine.noteOff(note);
    render(engine, static_cast<int>(sampleRate));
    expect(engine.getActiveVoiceCount() == 0,
           "a dropped note left a voice sounding");
}

void testPolyModesDifferInAllocation()
{
    constexpr double sampleRate = 48000.0;

    const auto allocationOrder = [&](KeyMode mode) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, false);
        auto parameters = plainPatch();
        parameters.keyMode = mode;
        parameters.release = 0.6f;
        engine.setParameters(parameters);

        std::vector<int> masks;
        for (int note = 0; note < 4; ++note)
        {
            engine.noteOn(60 + note, 1.0f);
            render(engine, 512);
            masks.push_back(engine.getDisplayVoiceMask());
            engine.noteOff(60 + note);
            render(engine, 512);
        }
        return masks;
    };

    const auto rotating = allocationOrder(KeyMode::Poly1);
    const auto fixed = allocationOrder(KeyMode::Poly2);

    // Rotation must visit more than one voice across four separate notes;
    // fixed priority must keep coming back to the first.
    int distinct = 0;
    int seen = 0;
    for (const auto mask : rotating)
        if ((seen & mask) != mask)
        {
            seen |= mask;
            ++distinct;
        }
    expect(distinct >= 3, "rotating assignment is reusing the same voice");
    expect(fixed.front() == fixed.back(),
           "fixed-priority assignment is not returning to the first voice");
}

void testHeldKeyRescanRunsHighToLow()
{
    constexpr double sampleRate = 48000.0;

    for (const auto mode : { KeyMode::Poly1, KeyMode::Poly2 })
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, false);
        auto parameters = plainPatch();
        parameters.keyMode = mode;
        parameters.polyphony = 2;
        parameters.release = 0.0f;
        engine.setParameters(parameters);
        // Poly 2 differs from the power-on mode and first clears an empty
        // table. Let that pass finish before constructing the actual fixture.
        renderExact(engine, static_cast<int>(sampleRate * 0.01));

        // The third key is physically present in the matrix even though the
        // full two-voice allocator has to drop it on this first pass.
        engine.noteOn(60, 1.0f);
        engine.noteOn(65, 1.0f);
        engine.noteOn(71, 1.0f);
        render(engine, 4096);

        // Re-pressing the current POLY button clears the allocation table. The
        // ROM scans high addresses first, so the two surviving assignments
        // must now be 71 and 65; note 60 is the one that is dropped.
        engine.reassertKeyMode();
        const auto rescanned = render(engine, static_cast<int>(sampleRate * 0.5));
        const std::size_t start = rescanned.left.size() / 2;
        const double high = magnitudeAt(rescanned.left, start, 8192,
                                        493.883, sampleRate);
        const double low = magnitudeAt(rescanned.left, start, 8192,
                                       261.626, sampleRate);
        expect(high > 4.0 * low,
               std::string(mode == KeyMode::Poly1 ? "Poly 1" : "Poly 2")
                   + " held-key rescan did not run high to low");
    }
}

void testRescanPreservesVoiceCpuPitchHistory()
{
    // Three otherwise identical voices begin on the same voice-board pitch.
    // Clearing allocator RAM, and changing the physical key while transpose
    // keeps the transmitted pitch equal, must both leave the free-running DCO
    // at the same phase as the untouched reference.
    constexpr double sampleRate = 48000.0;
    constexpr int scanPeriod = static_cast<int>(sampleRate * 0.0042);
    YouKnow106Engine reference;
    YouKnow106Engine cleared;
    YouKnow106Engine transposedEquivalent;
    auto parameters = plainPatch();
    parameters.polyphony = 1;
    parameters.keyTranspose = 12;
    parameters.release = 0.0f;

    for (auto* engine : { &reference, &cleared, &transposedEquivalent })
    {
        engine->prepare(sampleRate, blockSize, false);
        engine->setParameters(parameters);
        engine->noteOn(48, 1.0f); // transmitted note 60
        renderExact(*engine, 8192);
        engine->noteOff(48);
        renderExact(*engine, 4096);
        expect(engine->getActiveVoiceCount() == 0,
               "the pitch-history fixture did not retire its first note");
    }

    cleared.reassertKeyMode();
    transposedEquivalent.reassertKeyMode();
    // Keep all three converter clocks aligned while the two empty rescans
    // complete.
    for (auto* engine : { &reference, &cleared, &transposedEquivalent })
        renderExact(*engine, scanPeriod * 2);

    auto equivalentParameters = parameters;
    equivalentParameters.keyTranspose = 0;
    transposedEquivalent.setParameters(equivalentParameters);

    reference.noteOn(48, 1.0f);             // 48 + 12 = 60
    cleared.noteOn(48, 1.0f);               // same after allocator clear
    transposedEquivalent.noteOn(60, 1.0f);  // 60 + 0 = 60
    const auto untouched = renderExact(reference, 4096);
    const auto afterClear = renderExact(cleared, 4096);
    const auto equivalent = renderExact(transposedEquivalent, 4096);

    expect(maximumDifference(untouched.left, afterClear.left) < 1.0e-5,
           "a POLY table clear erased the voice CPU's DCO/pitch history");
    // The different physical key changes only the inaudible filter-noise seed;
    // a root-key-domain DCO comparison would instead restart the whole ramp.
    expect(maximumDifference(untouched.left, equivalent.left) < 5.0e-4,
           "the voice CPU compared oscillator-reset pitch before transpose");
}

void testHeldTransposeUpdatesVoiceCpuPitchHistory()
{
    constexpr double sampleRate = 48000.0;
    constexpr int scanPeriod = static_cast<int>(sampleRate * 0.0042);
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, false);
    auto parameters = plainPatch();
    parameters.polyphony = 1;
    parameters.release = 0.0f;
    parameters.keyTranspose = 0;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    renderExact(engine, scanPeriod * 2);

    // The same physical key now sends pitch byte 72 to the voice CPU. That
    // byte must replace 60 in CPU history even though no allocator event ran.
    parameters.keyTranspose = 12;
    engine.setParameters(parameters);
    renderExact(engine, scanPeriod * 2);
    expect(YouKnow106TestAccess::lastVoiceMidi(engine, 0) == 72,
           "held-note transpose did not update voice-CPU pitch history");

    engine.noteOff(60);
    renderExact(engine, static_cast<int>(sampleRate));
    expect(engine.getActiveVoiceCount() == 0,
           "held-transpose history fixture did not retire its voice");

    // Physical key 72 at zero transpose is the same voice-board pitch. A stale
    // pre-transpose history byte would incorrectly request a DCO restart here.
    parameters.keyTranspose = 0;
    engine.setParameters(parameters);
    engine.noteOn(72, 1.0f);
    expect(!YouKnow106TestAccess::dcoResetPending(engine, 0),
           "equivalent pitch after held transpose falsely scheduled DCO reset");

    // The opposite run-bit state matters: on an ordinary release, neither key
    // nor sustain keeps the timer in legato mode, so a changed transpose byte
    // must request the voice CPU's normal different-pitch restart.
    YouKnow106Engine releasing;
    releasing.prepare(sampleRate, blockSize, false);
    auto releaseParameters = plainPatch();
    releaseParameters.polyphony = 1;
    releaseParameters.release = 1.0f;
    releasing.setParameters(releaseParameters);
    releasing.noteOn(60, 1.0f);
    renderExact(releasing, scanPeriod * 2);
    releasing.noteOff(60);
    releaseParameters.keyTranspose = 12;
    YouKnow106TestAccess::updateVoiceScan(releasing, 0, releaseParameters);
    expect(YouKnow106TestAccess::dcoResetPending(releasing, 0),
           "transpose change during release did not schedule DCO reset");
}

void testPhysicalPitchWriteRestartIsBandlimited()
{
    constexpr double sampleRate = 192000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, false);
    auto parameters = plainPatch();
    parameters.polyphony = 1;
    parameters.release = 1.0f;
    parameters.chorusNoise = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(69, 1.0f);
    renderExact(engine, 2048);
    engine.noteOff(69);
    // Retarget widely into the top register so forgetting the ramps' unequal
    // one-sample advances leaves a clearly measurable hard saw step.
    engine.noteOn(120, 1.0f);
    expect(YouKnow106TestAccess::dcoResetPending(engine, 0),
           "different pitch on a releasing card did not request a timer restart");

    YouKnow106TestAccess::primeDcoRestartFixture(engine, 0, 0.75);
    const auto pulseBefore = YouKnow106TestAccess::pulseTrackState(engine, 0);
    const auto filterBefore = YouKnow106TestAccess::filterState(engine, 0);
    const auto noiseBefore =
        YouKnow106TestAccess::microscopicNoiseState(engine, 0);
    const double periodBefore =
        YouKnow106TestAccess::dcoPeriodSamples(engine, 0);
    const float cvBefore = YouKnow106TestAccess::dcoCv(engine, 0);
    const float cvTargetBefore = YouKnow106TestAccess::dcoCvTarget(engine, 0);
    const float scaleBefore = YouKnow106TestAccess::dcoRenderScale(engine, 0);

    YouKnow106TestAccess::performPitchWrite(engine, 0, parameters);
    expect(!YouKnow106TestAccess::dcoResetPending(engine, 0),
           "the converter did not consume the pending timer restart");
    expect(YouKnow106TestAccess::dcoPhase(engine, 0) == 0.0,
           "the physical timer restart did not land at phase zero");
    const auto logic = YouKnow106TestAccess::dcoLogicStates(engine, 0);
    expect(logic[0] == -1.0f && logic[1] == 1.0f,
           "the physical timer restart did not reset comparator/divider state");
    expect(YouKnow106TestAccess::dcoPeriodSamples(engine, 0) != periodBefore,
           "the pitch write restarted without programming the new period");
    expect(YouKnow106TestAccess::dcoCv(engine, 0) == cvBefore,
           "the timer restart bypassed the physical compensation hold");
    expect(YouKnow106TestAccess::dcoCvTarget(engine, 0) != cvTargetBefore,
           "the pitch write did not update the compensation target");
    expect(YouKnow106TestAccess::filterState(engine, 0) == filterBefore,
           "a timer restart reset the continuously powered filter");
    expect(YouKnow106TestAccess::microscopicNoiseState(engine, 0) == noiseBefore,
           "a timer restart reseeded the physical card");

    const auto sawAfter = YouKnow106TestAccess::sawTrackState(engine, 0);
    const auto pulseAfter = YouKnow106TestAccess::pulseTrackState(engine, 0);
    const auto subAfter = YouKnow106TestAccess::subTrackState(engine, 0);
    expect(sawAfter.primed && pulseAfter.primed && subAfter.primed,
           "a physical restart cleared the bandlimited waveform histories");
    expect(pulseAfter.base == pulseBefore.base
               && pulseAfter.delay == pulseBefore.delay,
           "a physical restart discarded the delayed pre-event pulse samples");
    const auto ringEnergy = [](const auto& ring) {
        double energy = 0.0;
        for (const float value : ring)
            energy += static_cast<double>(value) * value;
        return energy;
    };
    expect(ringEnergy(sawAfter.ring) > 1.0e-8,
           "the off-phase saw restart received no BLEP/BLAMP residual");
    expect(ringEnergy(pulseAfter.ring) > 1.0e-8,
           "the comparator restart received no BLEP residual");
    expect(ringEnergy(subAfter.ring) > 1.0e-8,
           "the divider restart received no BLEP residual");
    expect(YouKnow106TestAccess::sawRestartEventSideError(
               engine, 0, periodBefore, 0.75, scaleBefore) < 1.0e-6,
           "the saw restart residual ignored its rendered event-side slopes");

    // A copied pulse track exposes the transition without the rest of the
    // synth. A hard reset jumps from +1 to -1 immediately; the symmetric BLEP
    // retains the pre-event side first, crosses in bounded increments and
    // settles on the new side after its finite residual support.
    const auto transition =
        YouKnow106TestAccess::pulseTrackAfterRestart(engine, 0);
    expectNear(transition.front(), 1.0, 0.02,
               "bandlimited restart changed a pre-event pulse sample");
    expectNear(transition.back(), -1.0, 0.02,
               "bandlimited restart did not settle on its new pulse state");
    double maximumStep = std::abs(static_cast<double>(transition.front()) - 1.0);
    for (std::size_t sample = 1; sample < transition.size(); ++sample)
        maximumStep = std::max(
            maximumStep,
            std::abs(static_cast<double>(transition[sample])
                     - transition[sample - 1]));
    expect(maximumStep / 2.0 < 0.65,
           "physical restart retained a hard full-band pulse discontinuity");

    engine.reset();
    expect(!YouKnow106TestAccess::pulseTrackState(engine, 0).primed,
           "hard engine reset no longer clears oscillator residual history");
}

void testRescanGateOffReachesTheVoiceCpu()
{
    constexpr double sampleRate = 48000.0;
    constexpr int scanPeriod = static_cast<int>(sampleRate * 0.0042);
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, false);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    parameters.polyphony = 1;
    parameters.attack = 0.0f;
    parameters.decay = 0.0f;
    parameters.sustain = 0.2f;
    parameters.release = 0.0f;
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);
    engine.noteOn(64, 1.0f);

    renderExact(engine, scanPeriod * 80);
    const float heldLevel = engine.getDisplayEnvelope();
    expect(heldLevel > 0.15f && heldLevel < 0.25f,
           "the gate-off fixture did not settle at sustain");

    // Any real Unison key-up gates the stack and rescans the still-held bits.
    // Voice zero must see the off state at phase zero before the replacement
    // Note On is installed after that boundary.
    engine.noteOff(60);
    // The fractional scheduler alternates 201/202-sample spacings here. Walk
    // only to the actual wholly subsequent completed pass instead of assuming
    // a truncated period or completing a partial pass that missed voice zero.
    for (int elapsed = 0;
         elapsed < 2 * scanPeriod + 3
             && YouKnow106TestAccess::assignmentPending(engine);
         ++elapsed)
        renderExact(engine, 1);
    expect(!YouKnow106TestAccess::assignmentPending(engine),
           "the unison keyboard rescan missed the next converter pass");
    expect(engine.getDisplayEnvelope() < heldLevel * 0.25f,
           "unison reassignment hid the firmware's gate-off interval");
    expect(engine.getActiveVoiceCount() == 1,
           "the held unison key was not reassigned after gate-off");

    renderExact(engine, scanPeriod + 2);
    expect(engine.getDisplayEnvelope() > 0.9f,
           "the held unison key did not begin its fresh attack");
}

void testDuplicateAndUnmatchedKeyEdgesAreIgnored()
{
    constexpr double sampleRate = 48000.0;
    constexpr int scanPeriod = static_cast<int>(sampleRate * 0.0042);
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, false);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    parameters.polyphony = 1;
    parameters.attack = 0.0f;
    parameters.decay = 0.0f;
    parameters.sustain = 0.2f;
    parameters.release = 0.0f;
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);

    renderExact(engine, scanPeriod * 80 + scanPeriod - 1);
    const float sustain = engine.getDisplayEnvelope();
    engine.noteOn(60, 0.3f);
    renderExact(engine, 2);
    expectNear(engine.getDisplayEnvelope(), sustain, 0.01,
               "a duplicate Note On retriggered an already-high key bit");

    // Return to the sample before phase zero, then send an off for a key whose
    // bit was never set. In particular this must not enter the Solo handler's
    // gate/clear/rescan path.
    renderExact(engine, scanPeriod - 2);
    const float beforeUnmatched = engine.getDisplayEnvelope();
    engine.noteOff(61);
    renderExact(engine, 2);
    expectNear(engine.getDisplayEnvelope(), beforeUnmatched, 0.01,
               "an unmatched Note Off changed a sounding assignment");
}

void testIdleSnapshotPrimesEverySharedHold()
{
    // Loading a host snapshot immediately after prepare must be equivalent to
    // having that snapshot in place when prepare resets the engine. Otherwise
    // the first attack slews from stale resonance, PWM, sub, noise or LEVEL.
    constexpr double sampleRate = 48000.0;
    auto parameters = plainPatch();
    parameters.vcaLevel = 0.05f;
    parameters.resonance = 0.43f;
    parameters.pulseEnabled = true;
    parameters.pwmSource = PwmSource::Manual;
    parameters.pwmDepth = 0.81f;
    parameters.subLevel = 0.63f;
    parameters.noiseLevel = 0.27f;

    YouKnow106Engine hostOrder;
    hostOrder.prepare(sampleRate, blockSize, false);
    hostOrder.setParameters(parameters);

    YouKnow106Engine preloaded;
    preloaded.setParameters(parameters);
    preloaded.prepare(sampleRate, blockSize, false);

    expectNear(YouKnow106TestAccess::pwmTarget(hostOrder),
               YouKnow106Engine::pwmControlVolts(103.0f / 127.0f), 1.0e-6,
               "idle snapshot did not prime the one shared PWM hold");
    expectNear(YouKnow106TestAccess::subTarget(hostOrder), 80.0 / 127.0, 1.0e-6,
               "idle snapshot did not prime the one shared sub hold");
    expectNear(YouKnow106TestAccess::noiseTarget(hostOrder), 34.0 / 127.0, 1.0e-6,
               "idle snapshot did not prime the one shared noise hold");

    hostOrder.noteOn(60, 1.0f);
    preloaded.noteOn(60, 1.0f);
    const auto afterHostSnapshot = renderExact(hostOrder, 4096);
    const auto preparedWithSnapshot = renderExact(preloaded, 4096);
    expect(maximumDifference(afterHostSnapshot.left,
                             preparedWithSnapshot.left) < 1.0e-7,
           "the first attack used a stale shared hold after prepare");
}

void testPhysicalFiltersKeepRunningBehindClosedVcas()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, false);
    auto parameters = plainPatch();
    parameters.chorusNoise = 0.0f;
    engine.setParameters(parameters);

    // With no assigned note the voice bus must stay silent, but each of the
    // six real cards still receives its own microscopic input-referred
    // excitation and advances its analogue filter state behind the closed VCA.
    const auto silence = renderExact(engine, 4096);
    expect(peakOf(silence.left, 0) == 0.0,
           "powered idle voice cards leaked through their closed VCAs");

    for (int slot = 0; slot < YouKnow106Engine::hardwareVoices; ++slot)
    {
        const auto state = YouKnow106TestAccess::filterState(engine, slot);
        double magnitude = 0.0;
        for (const float value : state)
            magnitude += std::abs(static_cast<double>(value));
        expect(magnitude > 1.0e-12,
               "an idle physical filter stopped evolving behind its VCA");
    }

    // Product-extension voices do not stand for powered hardware cards. Their
    // dormant filters should therefore remain untouched while the six real
    // card states above continue to move.
    const auto extension = YouKnow106TestAccess::filterState(
        engine, YouKnow106Engine::hardwareVoices);
    expect(std::all_of(extension.begin(), extension.end(), [](float value) {
               return value == 0.0f;
           }),
           "a dormant extension voice was treated as a powered physical card");
}

void testPhysicalCardStateSurvivesVoiceAssignments()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, false);
    engine.setParameters(plainPatch());

    std::array<std::uint32_t, YouKnow106Engine::hardwareVoices> seeds {};
    for (int slot = 0; slot < YouKnow106Engine::hardwareVoices; ++slot)
        seeds[static_cast<std::size_t>(slot)] =
            YouKnow106TestAccess::microscopicNoiseState(engine, slot);

    auto sortedSeeds = seeds;
    std::sort(sortedSeeds.begin(), sortedSeeds.end());
    expect(std::adjacent_find(sortedSeeds.begin(), sortedSeeds.end())
               == sortedSeeds.end(),
           "two physical cards received the same microscopic-excitation seed");

    // A full engine reset deterministically reconstructs the instrument, but
    // assigning or retiring a MIDI note must not reconstruct its filter card.
    engine.reset();
    for (int slot = 0; slot < YouKnow106Engine::hardwareVoices; ++slot)
        expect(YouKnow106TestAccess::microscopicNoiseState(engine, slot)
                   == seeds[static_cast<std::size_t>(slot)],
               "a card's microscopic-excitation seed was not deterministic");

    constexpr int slot = 2;
    const std::array<float, 4> filterMarker {
        0.125f, -0.25f, 0.375f, -0.5f
    };
    constexpr std::uint32_t noiseMarker = 0x12345679u;
    YouKnow106TestAccess::setFilterState(engine, slot, filterMarker);
    YouKnow106TestAccess::setMicroscopicNoiseState(engine, slot, noiseMarker);

    YouKnow106TestAccess::initialiseVoice(engine, slot, 60);
    expect(YouKnow106TestAccess::filterState(engine, slot) == filterMarker,
           "note activation reset the continuously powered filter state");
    expect(YouKnow106TestAccess::microscopicNoiseState(engine, slot)
               == noiseMarker,
           "note activation reseeded the card's microscopic excitation");

    YouKnow106TestAccess::silenceVoice(engine, slot);
    expect(YouKnow106TestAccess::filterState(engine, slot) == filterMarker,
           "voice retirement reset the continuously powered filter state");
    expect(YouKnow106TestAccess::microscopicNoiseState(engine, slot)
               == noiseMarker,
           "voice retirement reseeded the card's microscopic excitation");

    YouKnow106TestAccess::initialiseVoice(engine, slot, 67);
    expect(YouKnow106TestAccess::filterState(engine, slot) == filterMarker,
           "reassigning a retired card discarded its previous filter state");
    expect(YouKnow106TestAccess::microscopicNoiseState(engine, slot)
               == noiseMarker,
           "reassigning a card used a per-note microscopic-excitation seed");

    // An extension slot is deliberately unlike those six powered cards. Once
    // idle it stops rendering, so its previous analogue/sample-grid state must
    // be cleared rather than frozen and resurrected on a later assignment.
    constexpr int extensionSlot = YouKnow106Engine::hardwareVoices;
    const auto extensionSeed = YouKnow106TestAccess::microscopicNoiseState(
        engine, extensionSlot);
    YouKnow106TestAccess::setFilterState(
        engine, extensionSlot, filterMarker);
    YouKnow106TestAccess::setDcoPhase(engine, extensionSlot, 0.375);
    YouKnow106TestAccess::setMicroscopicNoiseState(
        engine, extensionSlot, noiseMarker);
    YouKnow106TestAccess::primePulseDutyHistory(engine, extensionSlot, 0.9f);
    YouKnow106TestAccess::initialiseVoice(engine, extensionSlot, 72);
    YouKnow106TestAccess::silenceVoice(engine, extensionSlot);
    const auto clearedExtension = YouKnow106TestAccess::filterState(
        engine, extensionSlot);
    expect(std::all_of(clearedExtension.begin(), clearedExtension.end(),
                       [](float value) { return value == 0.0f; }),
           "an idle extension slot retained a frozen resonant-filter state");
    expect(YouKnow106TestAccess::dcoPhase(engine, extensionSlot) == 0.0,
           "an idle extension slot retained a frozen oscillator phase");
    expect(!YouKnow106TestAccess::pulseDutyHistoryIsPrimed(
               engine, extensionSlot),
           "an idle extension slot retained a stale PWM comparator timeline");
    expect(YouKnow106TestAccess::microscopicNoiseState(engine, extensionSlot)
               == extensionSeed,
           "an idle extension slot retained a frozen noise timeline");
    expect(YouKnow106TestAccess::currentMidi(engine, extensionSlot) == 72.0f,
           "clearing virtual analogue state discarded extension portamento memory");
}

void testConverterSchedulerPreservesFractionalScanPeriod()
{
    constexpr double sampleRate = 48000.0;
    constexpr double expectedPeriodSamples = sampleRate * 0.0042;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, false);
    engine.setParameters(plainPatch());

    std::vector<int> boundaries;
    double previousPhase = YouKnow106TestAccess::controlScanPhase(engine);
    for (int sample = 0; sample < 24000 && boundaries.size() < 101; ++sample)
    {
        float left = 0.0f;
        float right = 0.0f;
        engine.process(&left, &right, 1);
        const double phase = YouKnow106TestAccess::controlScanPhase(engine);
        if (phase < previousPhase)
            boundaries.push_back(sample);
        previousPhase = phase;
    }

    expect(boundaries.size() == 101,
           "the converter scheduler did not produce the expected pass count");
    if (boundaries.size() > 1)
    {
        int shortest = std::numeric_limits<int>::max();
        int longest = 0;
        for (std::size_t index = 1; index < boundaries.size(); ++index)
        {
            const int interval = boundaries[index] - boundaries[index - 1];
            shortest = std::min(shortest, interval);
            longest = std::max(longest, interval);
        }
        const double average = static_cast<double>(boundaries.back()
                                                   - boundaries.front())
                             / static_cast<double>(boundaries.size() - 1);
        expect(shortest == 201 && longest == 202,
               "the 48 kHz converter cadence did not alternate 201/202 samples");
        expectNear(average, expectedPeriodSamples, 0.02,
                   "the converter cadence truncated the nominal 4.2 ms period");
    }
}

void testPulseOffPinsComparatorWithoutResettingTheDco()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, false);
    auto parameters = plainPatch();
    parameters.sawEnabled = false;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.pwmDepth = 1.0f;
    parameters.calibration = 1.0f;
    parameters.chorus = ChorusMode::Off;
    // This fixture pins the pulse leg's gate. With every source off, the only
    // other signal at full Character is the stage offsets' now-correctly-sized
    // DC operating point stepping through the output coupling as the VCA
    // opens -- a real, separate mechanism that would trip the blunt silence
    // bound below without a pulse leg leaking anything. Isolate the gate.
    parameters.enableVcfStageOffsets = false;
    engine.setParameters(parameters);
    expectNear(YouKnow106TestAccess::pwmTarget(engine), -0.8, 1.0e-6,
               "Pulse Off did not write the documented -0.8 V shared control");

    engine.noteOn(60, 1.0f);
    const auto offAudio = renderExact(engine, 4800);
    expectNear(YouKnow106TestAccess::pulseDuty(engine, 0), 1.0, 1.0e-6,
               "Pulse Off did not hold the running comparator high");
    expect(peakOf(offAudio.left, offAudio.left.size() / 2) < 0.001,
           "the provisional pulse-off audio gate leaked the pinned leg");
    const double before = YouKnow106TestAccess::dcoPhase(engine, 0);
    renderExact(engine, 1);
    const double after = YouKnow106TestAccess::dcoPhase(engine, 0);
    expect(after != before, "Pulse Off stopped the free-running DCO");

    parameters.pulseEnabled = true;
    engine.setParameters(parameters);
    expect(!YouKnow106TestAccess::dcoResetPending(engine, 0),
           "re-enabling pulse scheduled an oscillator reset");

    // Card 6 has a deterministic negative comparator trim. During re-enable,
    // the shared base CV therefore crosses zero before that card stops being
    // pinned high. Its audible gate must follow the card comparator, not the
    // already-positive shared base value. The shared CV climbs out of -0.8 V
    // through the R117/C62 and R116/C63 smoothing poles, so the crossing sits
    // several milliseconds after the write; search a window that covers it.
    bool sawPerCardOffsetWindow = false;
    for (int sample = 0; sample < 2048; ++sample)
    {
        renderExact(engine, 1);
        if (YouKnow106TestAccess::pwmHeld(engine) > 0.0f
            && YouKnow106TestAccess::pulseDuty(engine, 5) >= 1.0f)
        {
            sawPerCardOffsetWindow = true;
            expect(!YouKnow106TestAccess::pulseMixEnabled(engine, 5, true),
                   "pulse re-enable admitted a card whose comparator was still pinned");
            break;
        }
    }
    expect(sawPerCardOffsetWindow,
           "pulse re-enable fixture missed the per-card comparator-offset window");
}

void testMovingPwmComparatorDoesNotMissThresholdCrossings()
{
    // The MC5534A comparator is memoryless: at every instant its output is
    // determined by the free-running ramp and the held PWM threshold.  A
    // moving threshold can cross the ramp between two oscillator edges, so an
    // event scheduler which only looks for a fixed edge position can leave the
    // logic high or low for an extra cycle.  That presents as an occasional
    // full-scale blip unique to the pulse waveform.
    constexpr double sampleRate = 48000.0;
    for (const bool highQuality : { false, true })
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, 1, highQuality);
        auto parameters = plainPatch();
        parameters.sawEnabled = false;
        parameters.pulseEnabled = true;
        parameters.pwmSource = PwmSource::Lfo;
        parameters.pwmDepth = 1.0f;
        parameters.lfoRate = 1.0f;
        parameters.vcaMode = VcaMode::Gate;
        engine.setParameters(parameters);
        engine.noteOn(60, 1.0f);

        int mismatches = 0;
        int samplesSinceEdge = 0;
        int maximumEdgeGap = 0;
        int edgeCount = 0;
        float previousState = YouKnow106TestAccess::pulseLogicState(engine, 0);
        for (int sample = 0; sample < static_cast<int>(sampleRate * 3.0); ++sample)
        {
            float left = 0.0f;
            float right = 0.0f;
            engine.process(&left, &right, 1);

            const float duty = YouKnow106TestAccess::pulseDuty(engine, 0);
            const double phase = YouKnow106TestAccess::dcoPhase(engine, 0);
            const float reset =
                YouKnow106TestAccess::dcoResetFraction(engine, 0);
            const float rise = YouKnow106Engine::pulseRisePhase(duty, reset);
            const float fall = YouKnow106Engine::pulseFallPhase(duty, reset);
            const float state = YouKnow106TestAccess::pulseLogicState(engine, 0);
            const float expected = duty >= 1.0f
                ? 1.0f : (phase >= rise && phase < fall ? 1.0f : -1.0f);
            if (state != expected)
                ++mismatches;

            ++samplesSinceEdge;
            if (state != previousState)
            {
                maximumEdgeGap = std::max(maximumEdgeGap, samplesSinceEdge);
                samplesSinceEdge = 0;
                previousState = state;
                ++edgeCount;
            }
        }

        const std::string quality = highQuality ? "HQ" : "normal";
        expect(mismatches == 0,
               quality + " moving PWM left the comparator in the wrong state for "
                   + std::to_string(mismatches) + " host samples");
        expect(edgeCount > 100,
               quality + " moving-PWM fixture did not exercise comparator edges");
        const double hostPeriod = YouKnow106TestAccess::dcoPeriodSamples(engine, 0)
                                / (highQuality ? 4.0 : 1.0);
        expect(maximumEdgeGap <= static_cast<int>(std::ceil(hostPeriod)) + 2,
               quality + " moving PWM skipped a comparator edge for a full cycle");
    }
}

void testModeChangesRebuildHeldKeys()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Poly1;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    engine.noteOn(64, 1.0f);
    render(engine, 4096);
    expect(engine.getActiveVoiceCount() == 2,
           "two held keys did not occupy two Poly 1 voices");

    // The POLY-button handler gates the old assignments, clears its tables and
    // scans every key still physically down. Solo Unison consumes the first
    // set bit of the high-to-low scan once, assigning all six cards to it.
    parameters.keyMode = KeyMode::Unison;
    engine.setParameters(parameters);
    const auto unison = render(engine, static_cast<int>(sampleRate * 0.5));
    expect(engine.getActiveVoiceCount() == 6,
           "switching modes with held keys did not rebuild a full unison stack");
    const std::size_t start = unison.left.size() / 2;
    expect(magnitudeAt(unison.left, start, 8192, 329.628, sampleRate)
               > 4.0 * magnitudeAt(unison.left, start, 8192, 261.626, sampleRate),
           "the held-key rescan did not give the highest key unison priority");

    parameters.keyMode = KeyMode::Poly2;
    engine.setParameters(parameters);
    render(engine, 4096);
    expect(engine.getActiveVoiceCount() == 2,
           "switching back to a poly mode did not rebuild the two held keys");

    engine.noteOff(60);
    engine.noteOff(64);
    render(engine, static_cast<int>(sampleRate));
    expect(engine.getActiveVoiceCount() == 0,
           "mode-change rebuild left a held assignment behind");
}

void testSustainHeldVoicesRemainAssignable()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Poly2;
    parameters.release = 0.35f;
    engine.setParameters(parameters);
    engine.setSustainPedal(true);

    for (int note = 60; note < 66; ++note)
        engine.noteOn(note, 1.0f);
    render(engine, 2048);
    for (int note = 60; note < 66; ++note)
        engine.noteOff(note);
    expect(engine.getActiveVoiceCount() == 6,
           "the sustain fixture did not leave six voice tails running");

    // The assigner frees a slot at physical key-up even though the voice CPU
    // continues its tail under sustain. A seventh key therefore reuses a card
    // instead of being dropped.
    engine.noteOn(72, 1.0f);
    const auto rendered = render(engine, static_cast<int>(sampleRate * 0.4));
    const std::size_t start = rendered.left.size() / 2;
    expect(magnitudeAt(rendered.left, start, 8192, 523.251, sampleRate) > 0.002,
           "six sustain-held tails made the assigner drop the seventh key");

    engine.noteOff(72);
    engine.setSustainPedal(false);
    render(engine, static_cast<int>(sampleRate * 4.0));
    expect(engine.getActiveVoiceCount() == 0,
           "reused sustain-held voices did not eventually retire");
}

void testPoly1AffinityUsesThePhysicalKey()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Poly1;
    parameters.keyTranspose = 12;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    render(engine, 2048);
    const int firstMask = engine.getDisplayVoiceMask();
    engine.noteOff(60);
    render(engine, 48000);

    // Transpose is added to the message sent to a voice board; it is not part
    // of the key number kept by the assigner. Returning to the same physical
    // key after changing transpose must therefore recover the same card.
    parameters.keyTranspose = 0;
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);
    render(engine, 2048);
    expect(engine.getDisplayVoiceMask() == firstMask,
           "Poly 1 note affinity compared transposed pitch instead of key identity");
}

void testRepressingPolyModeRebuildsHeldAssignments()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Poly1;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    // Walk rotation away from the first card, then leave a fourth note held.
    for (int note = 60; note < 63; ++note)
    {
        engine.noteOn(note, 1.0f);
        render(engine, 1024);
        engine.noteOff(note);
        render(engine, 8192);
    }
    engine.noteOn(65, 1.0f);
    render(engine, 2048);
    expect(engine.getDisplayVoiceMask() != 1,
           "the POLY reassert fixture did not rotate away from voice one");

    // The lamp remains Poly 1, but the momentary contact still enters the ROM
    // handler: gate all assignments, clear its history and rescan held keys.
    engine.reassertKeyMode();
    render(engine, 2048);
    expect(engine.getDisplayVoiceMask() == 1,
           "re-pressing the selected POLY mode did not rebuild held keys");
}

void testInactiveVoiceKeepsAdvancingPortamento()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Poly2;
    parameters.portamento = 0.40f;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    render(engine, static_cast<int>(sampleRate * 0.3));
    engine.noteOff(60);
    render(engine, static_cast<int>(sampleRate * 0.2));

    engine.noteOn(72, 1.0f);
    render(engine, static_cast<int>(sampleRate * 0.04));
    engine.noteOff(72);
    // The envelope is idle for most of this interval, but the voice firmware's
    // pitch integrator and free-running DCO continue towards the destination.
    render(engine, static_cast<int>(sampleRate * 1.2));

    engine.noteOn(72, 1.0f);
    const auto replay = render(engine, static_cast<int>(sampleRate * 0.3));
    const std::size_t start = replay.left.size() / 2;
    const double destination = magnitudeAt(replay.left, start, 8192, 523.251, sampleRate);
    const double octaveBelow = magnitudeAt(replay.left, start, 8192, 261.626, sampleRate);
    expect(destination > 4.0 * octaveBelow,
           "portamento stopped advancing when the voice became inactive");
}

void testPortamentoStateUsesEightEightGrid()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, false);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Poly2;
    parameters.polyphony = 1;
    parameters.portamento = 64.0f / 255.0f;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    renderExact(engine, 256);
    engine.noteOff(60);
    engine.noteOn(72, 1.0f);
    YouKnow106TestAccess::updateVoiceScan(engine, 0, parameters);

    const float state = YouKnow106TestAccess::currentMidi(engine, 0);
    expect(state > 60.0f && state < 72.0f,
           "portamento fixture did not advance partway toward its target");
    expectNear(state * 256.0f, std::round(state * 256.0f), 1.0e-6,
               "portamento state left the 8.8-semitone grid");
}

void testUnisonUsesEveryVoiceWithoutDetuning()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    // Zero tolerance: with the analogue spread switched out, six voices sharing
    // one reference and one count must be exactly coincident.
    parameters.calibration = 0.0f;
    engine.setParameters(parameters);
    engine.noteOn(57, 1.0f);

    const auto rendered = render(engine, static_cast<int>(sampleRate));
    expect(engine.getActiveVoiceCount() == 6, "unison does not use every voice");

    // Equal-frequency DCOs do not beat, but their ordered writes must not be
    // collapsed onto one sample: that would force a false coherent phase lock.
    const double firstPhase = YouKnow106TestAccess::dcoPhase(engine, 0);
    const double sixthPhase = YouKnow106TestAccess::dcoPhase(engine, 5);
    const double phaseDistance = std::min(std::abs(firstPhase - sixthPhase),
                                          1.0 - std::abs(firstPhase - sixthPhase));
    expect(phaseDistance > 1.0e-4,
           "normalized converter timing phase-locked the unison DCOs");
    const std::size_t quarter = rendered.left.size() / 4;
    const double early = peakOf({ rendered.left.begin() + static_cast<long>(quarter),
                                  rendered.left.begin() + static_cast<long>(quarter * 2) },
                                0);
    const double late = peakOf({ rendered.left.begin() + static_cast<long>(quarter * 3),
                                 rendered.left.end() }, 0);
    expectNear(20.0 * std::log10((late + 1e-12) / (early + 1e-12)), 0.0, 0.5,
               "unison voices are beating against one another");
}

void testUnisonReturnsToAHeldKey()
{
    // Unison is monophonic. Holding one key, pressing a second and releasing
    // the second must hand the stack back to the first, not silence it.
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    render(engine, 4096);
    engine.noteOn(64, 1.0f);
    render(engine, 4096);
    engine.noteOff(64);
    const auto rendered = render(engine, static_cast<int>(48000.0 * 0.5));

    expect(engine.getActiveVoiceCount() == 6,
           "releasing the newer unison key silenced the key still held");
    expect(peakOf(rendered.left, rendered.left.size() / 2) > 0.01,
           "the unison stack fell silent with a key still down");

    // Six voices whose scan rewrites land at different phases of the pass do
    // not stay phase-locked through a retarget -- exactly as the hardware's
    // free-running ramps do not -- so the pitch is read spectrally rather
    // than from zero crossings of the sum.
    const std::size_t half = rendered.left.size() / 2;
    const auto atHeld = magnitudeAt(rendered.left, half, 8192, 261.6, 48000.0);
    const auto atReleased = magnitudeAt(rendered.left, half, 8192, 329.6, 48000.0);
    expect(atHeld > 4.0 * atReleased,
           "the unison stack did not return to the key still held");

    // Every physical key-up clears and rescans the unison assignment. The
    // currently highest held key remains the result here, even though its
    // envelope is retriggered by that rebuild.
    engine.noteOn(67, 1.0f);
    render(engine, 4096);
    engine.noteOff(60);
    const auto after = render(engine, static_cast<int>(48000.0 * 0.5));
    const std::size_t afterHalf = after.left.size() / 2;
    const auto atSounding = magnitudeAt(after.left, afterHalf, 8192, 392.0, 48000.0);
    const auto atOld = magnitudeAt(after.left, afterHalf, 8192, 261.6, 48000.0);
    expect(atSounding > 4.0 * atOld,
           "the unison rescan did not retain the highest held key");

    engine.noteOff(67);
    render(engine, static_cast<int>(48000.0));
    expect(engine.getActiveVoiceCount() == 0,
           "the unison stack did not release when the last key was let go");
}

void testAllNotesOffReleasesRatherThanCutting()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.release = 0.75f;  // long enough that a cut would be obvious
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    render(engine, 8192);
    engine.releaseAllNotes();
    const auto tail = render(engine, 8192);
    expect(peakOf(tail.left, 0) > 0.005,
           "releasing all notes cut the sound instead of letting it ring");
    expect(engine.getActiveVoiceCount() == 1,
           "releasing all notes retired the voice immediately");

    // The hard stop still is a hard stop. What remains afterwards is filter and
    // resampler state settling, not a voice, so the claim is that it is far
    // below the note rather than bit-exact zero on the first sample.
    const double sounding = peakOf(tail.left, 0);
    engine.allNotesOff();
    const auto silence = render(engine, 4096);
    expect(engine.getActiveVoiceCount() == 0, "the hard stop left a voice active");
    expect(peakOf(silence.left, silence.left.size() / 2) < sounding * 0.01,
           "the hard stop left something sounding");
}

void testLoweringTheVoiceCountLetsNotesFinish()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    // A third of the travel falls to a tenth in about half a second, and the
    // truncated tail is gone within two -- comfortably inside the wait below.
    parameters.release = 0.35f;
    engine.setParameters(parameters);

    for (int note = 60; note < 66; ++note)
        engine.noteOn(note, 1.0f);
    render(engine, 4096);
    expect(engine.getActiveVoiceCount() == 6, "six keys did not take six voices");

    // Reducing the count must stop new allocations, not freeze the voices that
    // are already sounding -- a frozen voice never retires and would come back
    // audibly if the count were raised again.
    parameters.polyphony = 2;
    engine.setParameters(parameters);
    for (int note = 60; note < 66; ++note)
        engine.noteOff(note);
    render(engine, static_cast<int>(48000.0 * 4.0));
    expect(engine.getActiveVoiceCount() == 0,
           "voices outside a reduced voice count never finished");

    parameters.polyphony = 6;
    engine.setParameters(parameters);
    const auto after = render(engine, 8192);
    expect(peakOf(after.left, 0) < 1.0e-4,
           "raising the voice count resumed a stale note");
}

void testTransposeReachesSoundingVoices()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    engine.setParameters(parameters);
    engine.noteOn(69, 1.0f);
    render(engine, static_cast<int>(sampleRate * 0.3));

    // Moving the transpose control has to take a note that is already sounding
    // with it, not wait for the next keypress.
    parameters.keyTranspose = 12;
    engine.setParameters(parameters);
    const auto rendered = render(engine, static_cast<int>(sampleRate * 0.5));
    expectNear(measuredFrequency(rendered.left, rendered.left.size() / 2, sampleRate),
               880.0, 3.0,
               "transposing while a note was held left it at its old pitch");
}

void testFirstGlidedNoteStartsAtItsOwnPitch()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.portamento = 1.0f;  // the slowest glide there is
    engine.setParameters(parameters);

    // With nothing to glide from, the first note must simply be at its pitch.
    // Starting it wherever the voice happened to be constructed would make it
    // crawl there over several seconds.
    engine.noteOn(72, 1.0f);
    const auto rendered = render(engine, static_cast<int>(sampleRate * 0.4));
    expectNear(measuredFrequency(rendered.left, rendered.left.size() / 2, sampleRate),
               523.25, 4.0,
               "the first note under portamento glided from somewhere else");
}

void testVoicesRetireWithComponentToleranceApplied()
{
    // A voice card's own amplifier offset can sit above any small raw control
    // threshold while still being inside the converter's deadband. Such a voice
    // is silent, and it has to retire, or it would be processed forever and
    // would block a deferred quality change.
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.calibration = 1.0f;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    for (int note = 60; note < 66; ++note)
        engine.noteOn(note, 1.0f);
    render(engine, 8192);
    expect(engine.getActiveVoiceCount() == 6, "six keys did not take six voices");

    for (int note = 60; note < 66; ++note)
        engine.noteOff(note);
    render(engine, static_cast<int>(48000.0 * 2.0));
    expect(engine.getActiveVoiceCount() == 0,
           "voices with a component tolerance offset never retired");

    // And with nothing left running, a deferred quality change can complete
    // through its short click-prevention fade.
    expect(!engine.setOversamplingEnabled(false),
           "an idle quality change skipped its safety fade");
    renderExact(engine, static_cast<int>(48000.0 * 0.01));
    expect(engine.getOversamplingFactor() == 1,
           "a quality change stayed deferred with no voice sounding");
}

void testVoiceCountAboveTheHardwareSixWorks()
{
    // The voice count is one of the controls the hardware does not have, and it
    // genuinely reaches past six -- this asserts the extra voices sound rather
    // than being an inert range on the parameter.
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.polyphony = 12;
    engine.setParameters(parameters);

    for (int note = 48; note < 60; ++note)
        engine.noteOn(note, 1.0f);
    const auto rendered = render(engine, 8192);
    expect(engine.getActiveVoiceCount() == 12,
           "a voice count above six did not allocate the extra voices");
    expect(peakOf(rendered.left, 0) > 0.0, "the extra voices produced no sound");

    // And the thirteenth key is still dropped, because the count is the limit.
    engine.noteOn(60, 1.0f);
    render(engine, 1024);
    expect(engine.getActiveVoiceCount() == 12,
           "the key assigner exceeded the voice count it was given");
}

void testUnisonSurvivesAReducedVoiceCount()
{
    // Lowering the count leaves earlier voices sounding, and a unison retarget
    // has to reach them too or they stay keyed to a note nobody is holding.
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    render(engine, 4096);
    expect(engine.getActiveVoiceCount() == 6, "unison did not take every voice");

    parameters.polyphony = 1;
    engine.setParameters(parameters);
    engine.noteOn(64, 1.0f);
    render(engine, 4096);
    engine.noteOff(60);
    render(engine, 4096);
    engine.noteOff(64);
    render(engine, static_cast<int>(48000.0 * 2.0));

    expect(engine.getActiveVoiceCount() == 0,
           "reducing the voice count left unison voices stuck on a released key");
}

void testUnisonNoteOnCollapsesAWiderStack()
{
    // Unison is one note on every voice it is given. A count lowered while a
    // wider stack is sounding must not leave the old note held against the new
    // one, which would turn the mode into a two-note chord.
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    render(engine, 8192);
    expect(engine.getActiveVoiceCount() == 6, "unison did not take every voice");

    parameters.polyphony = 1;
    engine.setParameters(parameters);
    engine.noteOn(64, 1.0f);
    // Long enough for the five dropped voices to finish their release.
    const auto rendered = render(engine, static_cast<int>(sampleRate * 0.5));

    const std::size_t window = rendered.left.size() / 2;
    const double atOldNote =
        magnitudeAt(rendered.left, rendered.left.size() - window,
                    static_cast<int>(window), 261.626, sampleRate);
    const double atNewNote =
        magnitudeAt(rendered.left, rendered.left.size() - window,
                    static_cast<int>(window), 329.628, sampleRate);
    expect(atNewNote > 0.01, "the new unison note is not sounding");
    expect(atOldNote < atNewNote * 0.05,
           "a unison note-on left the previous note sounding on the dropped voices");
    expect(engine.getActiveVoiceCount() == 1,
           "the dropped unison voices never retired");
}

void testUnisonDoesNotResurrectPolyphonicTails()
{
    // Release tails from before the mode changed are not part of the unison
    // stack. Sweeping them into a retarget would key them to a note they never
    // played and pull their envelopes back out of release, so they would ring
    // on for good and push the instrument past the voice count it was given.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    // Fixed priority from the first voice down, so the four notes land in the
    // four lowest slots and the two the unison stack will take are known.
    parameters.keyMode = KeyMode::Poly2;
    parameters.polyphony = 6;
    // Long enough that the tails are unmistakably still ringing when the mode
    // changes underneath them, short enough to be gone well inside the final
    // render if nothing revives them.
    parameters.release = 0.4f;
    engine.setParameters(parameters);

    for (int note = 48; note < 52; ++note)
        engine.noteOn(note, 1.0f);
    render(engine, 8192);
    for (int note = 48; note < 52; ++note)
        engine.noteOff(note);
    render(engine, 2048);
    expect(engine.getActiveVoiceCount() == 4,
           "the polyphonic tails are not ringing");

    // The stack is narrower than the tails, so slots 2 and 3 stay outside it.
    parameters.keyMode = KeyMode::Unison;
    parameters.polyphony = 2;
    engine.setParameters(parameters);
    engine.noteOn(64, 1.0f);
    render(engine, 2048);
    engine.noteOn(65, 1.0f);
    render(engine, 2048);
    engine.noteOff(65);
    render(engine, static_cast<int>(sampleRate
        * (YouKnow106Engine::envelopeReleaseSeconds(parameters.release) + 0.25f)));

    expect(engine.getActiveVoiceCount() == 2,
           "a unison retarget resurrected a polyphonic release tail");
}

void testSubFlipsOnTheFirstWrap()
{
    // The divider's first output edge belongs one oscillator period after the
    // retrigger, not two. Getting that wrong makes the opening half-cycle twice
    // as long as every one after it -- a low transient the hardware does not
    // have. A low note is used so the anomaly is far clear of the amplifier's
    // own opening time.
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.sawEnabled = false;
    parameters.pulseEnabled = false;
    parameters.subLevel = 1.0f;
    engine.setParameters(parameters);

    constexpr int midiNote = 24;
    const double fundamental = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
    const double periodSamples = sampleRate / fundamental;

    engine.noteOn(midiNote, 1.0f);
    const auto rendered = render(engine, static_cast<int>(periodSamples * 3.0));

    // The first falling edge after the amplifier has opened.
    const auto opened = static_cast<std::size_t>(sampleRate * 0.008);
    std::size_t firstFall = 0;
    for (std::size_t index = opened + 1; index < rendered.left.size(); ++index)
        if (rendered.left[index - 1] > 0.0f && rendered.left[index] <= 0.0f)
        {
            firstFall = index;
            break;
        }

    expect(firstFall > 0, "the sub never changed sign");
    const double periods = static_cast<double>(firstFall) / periodSamples;
    expect(periods < 1.5,
           "the sub's first half-cycle lasted two oscillator periods instead of one");
}

void testControllersReturnToNeutralOnReset()
{
    // A run that stopped with the bender pushed over must not start the next
    // one there. Hosts are not obliged to resend a neutral controller when the
    // transport restarts, and nothing else would bring the pitch back.
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.benderDcoDepth = 1.0f;
    engine.setParameters(parameters);

    engine.setPitchBend(1.0f);
    engine.noteOn(69, 1.0f);
    const auto bent = render(engine, static_cast<int>(sampleRate));
    const double bentPitch =
        measuredFrequency(bent.left, bent.left.size() / 2, sampleRate);
    expect(bentPitch > 500.0, "the bender did not raise the pitch");

    engine.reset();
    engine.noteOn(69, 1.0f);
    const auto afterReset = render(engine, static_cast<int>(sampleRate));
    const double resetPitch = measuredFrequency(afterReset.left,
                                                afterReset.left.size() / 2, sampleRate);
    expectNear(resetPitch, 440.0, 1.0,
               "a reset left the previous run's pitch bend applied");
}

void testContinuousControlsDoNotStepAtBlockBoundaries()
{
    // Main VOLUME is the one continuous audio-path pot after the converter
    // chain. A host automating it across a held note would hear a block-rate
    // step as a click unless the plug-in glides it.
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    engine.setParameters(parameters);
    engine.noteOn(45, 1.0f);
    render(engine, 8192);

    std::vector<float> left(blockSize, 0.0f);
    std::vector<float> right(blockSize, 0.0f);
    engine.process(left.data(), right.data(), blockSize);

    // The largest step the signal takes on its own, as the reference for what
    // counts as a discontinuity.
    double withinBlock = 0.0;
    for (int index = 1; index < blockSize; ++index)
        withinBlock = std::max(withinBlock,
                               std::abs(static_cast<double>(left[index])
                                        - left[index - 1]));
    const float lastOfBlock = left[blockSize - 1];

    // The whole range, in one automation move at a block boundary.
    parameters.volume = 0.0f;
    engine.setParameters(parameters);
    engine.process(left.data(), right.data(), blockSize);

    const double acrossBoundary =
        std::abs(static_cast<double>(left[0]) - lastOfBlock);
    expect(acrossBoundary <= withinBlock * 1.5 + 1.0e-4,
           "a block-boundary volume change stepped the output");

    // And it still gets there: a glide that never arrives is not a fix. Four
    // time constants is long enough to be unambiguous and short enough that a
    // glide slow enough to be audible as a fade would fail.
    const auto settled = render(engine, static_cast<int>(sampleRate * 0.02));
    expect(peakOf(settled.left, settled.left.size() / 2) < 0.02,
           "the glided volume never reached its new setting");
}

void testMainVolumeLoadedLinearPotLaw()
{
    // With the first block primed at its current panel position there is no
    // glide. The 10KB resistance law is nominally linear, but its fixed
    // selector/headphone load makes the in-circuit wiper transfer slightly
    // nonlinear and also moves the output capacitor's sub-audio corner.
    const auto atVolume = [](float volume) {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, true);
        auto parameters = plainPatch();
        parameters.volume = volume;
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);
        renderExact(engine, 48000);
        return renderExact(engine, 8192);
    };

    const auto full = atVolume(1.0f);
    const auto half = atVolume(0.5f);
    const auto quarter = atVolume(0.25f);
    const auto fittedRatio = [&](const Render& reduced) {
        double cross = 0.0;
        double referenceEnergy = 0.0;
        for (std::size_t index = 0; index < full.left.size(); ++index)
        {
            cross += static_cast<double>(full.left[index]) * reduced.left[index]
                   + static_cast<double>(full.right[index]) * reduced.right[index];
            referenceEnergy += static_cast<double>(full.left[index]) * full.left[index]
                             + static_cast<double>(full.right[index]) * full.right[index];
        }
        return cross / referenceEnergy;
    };
    const double halfRatio = fittedRatio(half);
    const double quarterRatio = fittedRatio(quarter);
    const double expectedHalf =
        YouKnow106Engine::outputCouplingHighGain(0.5f)
        / YouKnow106Engine::outputCouplingHighGain(1.0f);
    const double expectedQuarter =
        YouKnow106Engine::outputCouplingHighGain(0.25f)
        / YouKnow106Engine::outputCouplingHighGain(1.0f);
    expect(peakOf(full.left, 1024) > 0.01,
           "the static VOLUME-law fixture produced no signal");
    expectNear(halfRatio, expectedHalf, 2.0e-4,
               "VOLUME 0.5 misses the loaded nominal 10KB law");
    expectNear(quarterRatio, expectedQuarter, 2.0e-4,
               "VOLUME 0.25 misses the loaded nominal 10KB law");

    // The 5 ms value is the one-pole time constant: a 0 -> 1 move reaches
    // 1-e^-1 after 5 ms. Check the threshold crossing to within one host
    // sample, then compare the exact 10 ms (two-time-constant) state at rates
    // that exercise all of the engine's oversampling-factor branches.
    constexpr double glideSeconds = 0.005;
    const double oneTau = 1.0 - std::exp(-1.0);
    const double twoTau = 1.0 - std::exp(-2.0);
    std::array<double, 4> stateAtTenMilliseconds {};
    const std::array<double, 4> rates { 44100.0, 48000.0, 96000.0, 192000.0 };
    for (std::size_t rateIndex = 0; rateIndex < rates.size(); ++rateIndex)
    {
        const double sampleRate = rates[rateIndex];
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.volume = 0.0f;
        engine.setParameters(parameters);
        float left = 0.0f;
        float right = 0.0f;
        engine.process(&left, &right, 1);
        expect(YouKnow106TestAccess::glidedVolume(engine) == 0.0f,
               "the initial VOLUME position was not primed without a glide");

        parameters.volume = 1.0f;
        engine.setParameters(parameters);
        int samples = 0;
        while (YouKnow106TestAccess::glidedVolume(engine) < oneTau
               && samples < static_cast<int>(sampleRate * 0.02))
        {
            engine.process(&left, &right, 1);
            ++samples;
        }
        const double crossingSeconds = samples / sampleRate;
        expect(std::abs(crossingSeconds - glideSeconds)
                   <= 1.0 / sampleRate + 1.0e-12,
               "the 5 ms VOLUME glide time moved at "
                   + std::to_string(static_cast<int>(sampleRate)) + " Hz");

        const int tenMillisecondSamples =
            static_cast<int>(std::llround(sampleRate * 0.010));
        while (samples < tenMillisecondSamples)
        {
            engine.process(&left, &right, 1);
            ++samples;
        }
        stateAtTenMilliseconds[rateIndex] =
            YouKnow106TestAccess::glidedVolume(engine);
        expectNear(stateAtTenMilliseconds[rateIndex], twoTau, 3.0e-5,
                   "VOLUME glide misses the declared 5 ms one-pole law at "
                       + std::to_string(static_cast<int>(sampleRate)) + " Hz");
    }
    for (std::size_t index = 1; index < stateAtTenMilliseconds.size(); ++index)
        expectNear(stateAtTenMilliseconds[index], stateAtTenMilliseconds[0],
                   3.0e-5,
                   "VOLUME smoothing is not sample-rate invariant");
}

void testFixedOutputBoundaryCorpus()
{
    // Fixed OQ-06 product corpus: 48 kHz, normal high-quality processing,
    // velocity 1, six hardware voices, VOLUME 1, Unit Character 0 and the
    // stated chorus mode. RMS is unweighted across both channels. Sample peak
    // and overloads inspect both PCM channels; 4x true peak and reconstructed
    // overloads use reconstructedSample4x() above. All metrics use the 4096
    // samples after the fixture's warm-up, with 16 guard samples on each side.
    enum class Playing { OneNote, SixNoteChord, SoloUnison, SelfOscillation };
    struct Fixture
    {
        const char* name;
        Playing playing;
        ChorusMode chorus;
    };
    constexpr std::array fixtures {
        Fixture { "one-note/off", Playing::OneNote, ChorusMode::Off },
        Fixture { "six-note-chord/off", Playing::SixNoteChord, ChorusMode::Off },
        Fixture { "solo-unison/off", Playing::SoloUnison, ChorusMode::Off },
        Fixture { "filter-self-oscillation/off", Playing::SelfOscillation,
                  ChorusMode::Off },
        Fixture { "one-note/chorus-I", Playing::OneNote, ChorusMode::One },
        Fixture { "one-note/chorus-II", Playing::OneNote, ChorusMode::Two },
    };
    struct Baseline
    {
        double rms;
        double samplePeak;
        double truePeak4x;
        std::uint64_t sampleOverloads;
        std::uint64_t reconstructedOverloads;
    };
    // Filled from the deterministic implementation below. Floating metrics
    // use broad relative guards and counts allow a small threshold-crossing
    // margin, so ordinary compiler noise cannot turn this into a waveform
    // checksum while meaningful level/headroom changes remain visible.
    constexpr std::array baselines {
        // Re-pinned three times on 2026-08-07: the IC6 wet/dry legs were
        // corrected to the p. 15 read (dry 100/47, wet 100/39 -- chorus-off
        // fixtures moved down 1.62 dB, wet gained the same 3.24 dB the dry
        // lost), the sweep endpoints moved to the 106's own measured
        // 1.4-6.4 ms (the fixed window catches a different phase of the
        // deeper sweep), and the mixer node stopped counting switchable
        // legs -- sources mute, legs never switch -- which returned the
        // phantom 1.76 dB the old Thevenin model took from every fixture
        // with both waveforms on. The waveform-free self-oscillation probe
        // is the control: only the first correction reaches it.
        Baseline { 0.42033, 0.862052, 0.87213, 0, 0 },
        Baseline { 1.07583, 3.04992, 3.04992, 3188, 12730 },
        Baseline { 2.31987, 4.26482, 4.2802, 6402, 25626 },
        // Raised when the resonance profile was re-solved against Roland's own
        // 4.8 Vp-p self-oscillation trim; see
        // testSelfOscillationMatchesTheServiceTrim.
        Baseline { 0.19852, 0.28071, 0.28071, 0, 0 },
        Baseline { 0.749961, 1.5589, 1.59184, 1545, 6213 },
        Baseline { 0.488373, 1.52319, 1.52458, 46, 185 },
    };

    constexpr double sampleRate = 48000.0;
    constexpr int analysisSamples = 4096;
    constexpr int guardSamples = 16;
    const bool printAudit = std::getenv("YOUKNOW106_AUDIT_OUTPUT") != nullptr;
    std::array<OutputMetrics, fixtures.size()> measured {};
    Render boundaryProbe;
    bool passedPositiveOne = false;
    bool passedNegativeOne = false;

    for (std::size_t fixtureIndex = 0; fixtureIndex < fixtures.size(); ++fixtureIndex)
    {
        const auto& fixture = fixtures[fixtureIndex];
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.pulseEnabled = true;
        parameters.pwmSource = PwmSource::Manual;
        parameters.pwmDepth = 0.37f;
        parameters.subLevel = 0.50f;
        parameters.vcaLevel = 1.0f;
        parameters.volume = 1.0f;
        parameters.chorus = fixture.chorus;
        parameters.chorusNoise = 0.0f;

        if (fixture.playing == Playing::SoloUnison)
        {
            parameters.keyMode = KeyMode::Unison;
            // This low-register, unnormalised six-card stack is the deliberate
            // headroom probe: it must cross full scale without a hidden rail.
        }
        else if (fixture.playing == Playing::SelfOscillation)
        {
            parameters.sawEnabled = false;
            parameters.pulseEnabled = false;
            parameters.subLevel = 0.0f;
            parameters.resonance = 1.0f;
            parameters.cutoff = 49.0f / 127.0f;
        }
        engine.setParameters(parameters);

        if (fixture.playing == Playing::SixNoteChord)
            for (const int note : { 36, 43, 48, 52, 55, 60 })
                engine.noteOn(note, 1.0f);
        else
            engine.noteOn(fixture.playing == Playing::SoloUnison ? 36 : 60, 1.0f);

        const int warmupSamples = fixture.playing == Playing::SelfOscillation
                                ? 96000 : 24000;
        const auto rendered = renderExact(
            engine, warmupSamples + analysisSamples + guardSamples);
        measured[fixtureIndex] = outputMetrics(
            rendered, static_cast<std::size_t>(warmupSamples), analysisSamples);
        if (fixtureIndex == 0)
            boundaryProbe = rendered;

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto& signal = channel == 0 ? rendered.left : rendered.right;
            for (int index = warmupSamples;
                 index < warmupSamples + analysisSamples; ++index)
            {
                passedPositiveOne = passedPositiveOne || signal[index] > 1.0f;
                passedNegativeOne = passedNegativeOne || signal[index] < -1.0f;
            }
        }

        if (printAudit)
            std::cout << "OQ06 " << fixture.name << " rms "
                      << measured[fixtureIndex].rms << " samplePeak "
                      << measured[fixtureIndex].samplePeak << " truePeak4x "
                      << measured[fixtureIndex].truePeak4x << " sampleOverloads "
                      << measured[fixtureIndex].sampleOverloads
                      << " reconstructedOverloads "
                      << measured[fixtureIndex].reconstructedOverloads << '\n';

        const auto& expected = baselines[fixtureIndex];
        if (expected.rms > 0.0)
        {
            const auto relativeGuard = [&](double actual, double reference,
                                           const char* metric) {
                expect(std::abs(actual - reference)
                           <= std::max(1.0e-7, reference * 0.04),
                       std::string(fixture.name) + ' ' + metric
                           + " escaped its deterministic corpus guard");
            };
            relativeGuard(measured[fixtureIndex].rms, expected.rms, "RMS");
            relativeGuard(measured[fixtureIndex].samplePeak, expected.samplePeak,
                          "sample peak");
            relativeGuard(measured[fixtureIndex].truePeak4x, expected.truePeak4x,
                          "4x true peak");
            const auto countGuard = [&](std::uint64_t actual,
                                        std::uint64_t reference,
                                        const char* metric) {
                const auto tolerance = std::max<std::uint64_t>(
                    8, static_cast<std::uint64_t>(reference * 0.04));
                const auto difference = actual > reference ? actual - reference
                                                           : reference - actual;
                expect(difference <= tolerance,
                       std::string(fixture.name) + ' ' + metric
                           + " escaped its deterministic corpus guard");
            };
            countGuard(measured[fixtureIndex].sampleOverloads,
                       expected.sampleOverloads, "sample-overload count");
            countGuard(measured[fixtureIndex].reconstructedOverloads,
                       expected.reconstructedOverloads,
                       "reconstructed-overload count");
        }
    }

    expect(passedPositiveOne && passedNegativeOne,
           "the hot corpus was limited to +/-1 instead of passing floating overloads");
    expect(std::any_of(measured.begin(), measured.end(), [](const auto& metrics) {
               return metrics.samplePeak > 1.01 && metrics.sampleOverloads > 0;
           }),
           "the output corpus no longer exercises unclipped headroom above full scale");

    // Vref is deliberately not an engine/control parameter. Its pure gain is
    // applied to the already-rendered post-volume pair. Recovering the source
    // from two different references must therefore return the same pre-boundary
    // samples, while all linear metrics change only by their scalar ratio.
    const float lowerReference =
        YouKnow106Engine::compatibilityOutputReferenceRmsVolts * 0.5f;
    const float higherReference =
        YouKnow106Engine::compatibilityOutputReferenceRmsVolts * 2.0f;
    const double lowerGain = YouKnow106Engine::outputReferenceGain(lowerReference);
    const double higherGain = YouKnow106Engine::outputReferenceGain(higherReference);
    expectNear(lowerGain / higherGain, 4.0, 1.0e-6,
               "Vref does not act as a pure inverse final-boundary scale");
    double recoveredDifference = 0.0;
    for (std::size_t index = 0; index < boundaryProbe.left.size(); ++index)
        for (const double sample : { static_cast<double>(boundaryProbe.left[index]),
                                     static_cast<double>(boundaryProbe.right[index]) })
        {
            const double lowerOutput = sample * lowerGain;
            const double higherOutput = sample * higherGain;
            recoveredDifference = std::max(
                recoveredDifference,
                std::abs(lowerOutput / lowerGain - higherOutput / higherGain));
        }
    expect(recoveredDifference < 1.0e-12,
           "changing Vref changed pre-output-boundary behavior");
}

void testNotesWaitForTheSharedConverterScan()
{
    // One converter serves every voice, so a key struck between passes cannot
    // be heard until the next pass reaches its ordered DCO write. Two keys
    // struck after their current-pass slots therefore both wait for the next
    // shared schedule; neither starts a private note-on-relative scan.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    // No oversampling, so the internal rate is the host rate and the pass
    // interval can be counted in output samples directly.
    engine.prepare(sampleRate, blockSize, false);
    auto parameters = plainPatch();
    engine.setParameters(parameters);

    // The documented converter pass, taken from the service note rather than
    // from the engine, so this measures the model against the figure it claims.
    const int scanPeriod = static_cast<int>(sampleRate * 0.0042);
    expect(scanPeriod > 150, "the scan interval is too short for this fixture");

    std::vector<float> left(blockSize, 0.0f);
    std::vector<float> right(blockSize, 0.0f);
    const auto run = [&](int count) {
        engine.process(left.data(), right.data(), count);
        return peakOf(std::vector<float>(left.begin(), left.begin() + count), 0);
    };

    // The first pass falls on the first rendered sample, so this lands the
    // playhead midway between passes with the next one still ahead.
    run(scanPeriod / 2);

    engine.noteOn(60, 1.0f);
    const double duringGap = run(scanPeriod / 4);
    engine.noteOn(67, 1.0f);
    const double stillWaiting = run(scanPeriod / 8);

    expectNear(duringGap, 0.0, 1.0e-9,
               "a note sounded before the converter scan reached it");
    expectNear(stillWaiting, 0.0, 1.0e-9,
               "the second note did not wait for the same pass as the first");

    // And once the next pass has traversed both DCO slots, both are speaking.
    const double afterScan = run(scanPeriod);
    expect(afterScan > 0.01, "the notes never sounded after the scan pass");
}

void testRetriggerDoesNotTouchVcaHoldBeforeConverterScan()
{
    // A host Note On changes digital key/assignment state. The analogue VCA
    // hold can change only when process() reaches that card's ordered converter
    // write and then advances the hold network. With no rendered sample between
    // these two events, any change is necessarily an invented MIDI-time path.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, false);
    auto parameters = plainPatch();
    parameters.polyphony = 1;
    parameters.calibration = 1.0f;
    parameters.attack = 0.0f;
    parameters.decay = 0.0f;
    parameters.sustain = 64.0f / 127.0f;
    parameters.release = 1.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    renderExact(engine, 4096);
    expect(engine.getDisplayEnvelope() > 0.45f
               && engine.getDisplayEnvelope() < 0.55f,
           "the retrigger fixture did not settle near half sustain");

    engine.noteOff(60);
    const float heldBefore = YouKnow106TestAccess::vcaControl(engine, 0);
    const double phaseBefore = YouKnow106TestAccess::controlScanPhase(engine);
    expect(std::isfinite(heldBefore) && heldBefore > 0.0f,
           "the retrigger fixture has no live analogue VCA hold");

    engine.noteOn(60, 1.0f);
    expect(YouKnow106TestAccess::vcaControl(engine, 0) == heldBefore,
           "retrigger changed the analogue VCA hold before its converter write");
    expect(YouKnow106TestAccess::controlScanPhase(engine) == phaseBefore,
           "a note event advanced the converter scheduler");
}

void testSilentVoiceDoesNotInventUnmeasuredVcaFeedthrough()
{
    // VR30/R112 correct the BA662 signal-input offset; they are not the Tr20
    // control-current path represented by vcaControlOffset. With the filter
    // output pinned to zero, changing that control offset must therefore not
    // create audio. The removed implementation reused it as a millivolt input
    // offset and multiplied by control and gain, so all three cases failed.
    for (const float controlOffset : { -1.0f, 0.0f, 1.0f })
    {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, true);
        const float output = YouKnow106TestAccess::renderSilentVoiceAtControlOffset(
            engine, controlOffset);
        expect(output == 0.0f,
               "silent voice invented VCA input-offset feedthrough from its control trim");
    }
}

void testCommonVcaHoldUsesJackBoardC7TimeConstant()
{
    // A one-time-constant voltage step must reach 1-exp(-1), independently of
    // the engine's internal oversampling rate. The converter may rewrite the
    // same target during the render; it must not bypass C7's continuous state.
    constexpr double sampleRate = 48000.0;
    const double oneTau = 1.0 - std::exp(-1.0);
    const int hostSamples = static_cast<int>(std::lround(
        YouKnow106Engine::commonVcaHoldTimeConstantSeconds() * sampleRate));

    for (const bool oversampling : { false, true })
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, oversampling);
        auto parameters = plainPatch();
        parameters.vcaLevel = 1.0f;
        engine.setParameters(parameters);
        YouKnow106TestAccess::setSharedVca(engine, 0.0f);

        renderExact(engine, hostSamples);
        const double elapsed = hostSamples / sampleRate;
        const double expected = 1.0 - std::exp(
            -elapsed / YouKnow106Engine::commonVcaHoldTimeConstantSeconds());
        expectNear(YouKnow106TestAccess::sharedVca(engine), expected, 2.0e-5,
                   oversampling
                       ? "common-VCA C7 slew changed at 4x processing"
                       : "common-VCA C7 slew misses one time constant at 1x");
        expectNear(expected, oneTau, 1.0e-4,
                   "common-VCA one-tau fixture is not one time constant");
    }
}

void testPwmHoldCrossesItsTwoSmoothingPoles()
{
    // The PWM hold reaches the comparators through R117/C62 and then R116/C63
    // around IC17a. A step from a discharged network must follow the two-pole
    // cascade those components fix -- independently of the engine's internal
    // oversampling rate -- rather than the retired single compatibility pole.
    constexpr double sampleRate = 48000.0;
    const double tauFirst = YouKnow106TestAccess::pwmFirstPoleSeconds();
    const double tauSecond = YouKnow106TestAccess::pwmSecondPoleSeconds();
    const int hostSamples =
        static_cast<int>(std::lround(tauFirst * sampleRate));

    for (const bool oversampling : { false, true })
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, oversampling);
        auto parameters = plainPatch();
        parameters.pulseEnabled = true;
        parameters.pwmSource = PwmSource::Manual;
        parameters.pwmDepth = 0.5f;
        engine.setParameters(parameters);
        const double target = YouKnow106TestAccess::pwmTarget(engine);
        expect(target > 0.0, "manual PWM fixture has no positive target");
        YouKnow106TestAccess::setPwmHeld(engine, 0.0f);

        renderExact(engine, hostSamples);
        const double elapsed = hostSamples / sampleRate;
        const double expected = target
            * (1.0
               - (tauFirst * std::exp(-elapsed / tauFirst)
                  - tauSecond * std::exp(-elapsed / tauSecond))
                     / (tauFirst - tauSecond));
        expectNear(YouKnow106TestAccess::pwmHeld(engine), expected,
                   5.0e-3 * target,
                   oversampling
                       ? "PWM two-pole smoothing changed at 4x processing"
                       : "PWM smoothing does not follow the R117/C62 and "
                         "R116/C63 cascade");
    }
}

void testSubHoldUsesItsR11C1TimeConstant()
{
    // The stored SUB level reaches its mixer OTA through R11 1 kOhm into C1
    // 10 uF: one 10 ms pole. A one-time-constant step must reach 1-exp(-1)
    // regardless of the internal processing rate.
    constexpr double sampleRate = 48000.0;
    const double oneTau = 1.0 - std::exp(-1.0);
    const double tau = YouKnow106TestAccess::subSlewSeconds();
    const int hostSamples = static_cast<int>(std::lround(tau * sampleRate));

    for (const bool oversampling : { false, true })
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, oversampling);
        auto parameters = plainPatch();
        parameters.subLevel = 1.0f;
        engine.setParameters(parameters);
        const double target = YouKnow106TestAccess::subTarget(engine);
        expect(target > 0.0, "sub fixture has no positive target");
        YouKnow106TestAccess::setSubHeld(engine, 0.0f);

        renderExact(engine, hostSamples);
        const double elapsed = hostSamples / sampleRate;
        const double expected = target * (1.0 - std::exp(-elapsed / tau));
        expectNear(YouKnow106TestAccess::subHeld(engine), expected, 2.0e-5,
                   oversampling ? "SUB R11/C1 slew changed at 4x processing"
                                : "SUB slew misses its R11/C1 time constant");
        expectNear(expected / target, oneTau, 1.0e-4,
                   "SUB one-tau fixture is not one time constant");
    }
}

void testPortamentoRateFollowsItsControl()
{
    // The glide rate is set by a control in the pitch integrator's path, not
    // latched when the key went down. Turning it down mid-glide has to shorten
    // the slide, and turning it off has to land the note at the next scan.
    constexpr double sampleRate = 96000.0;
    const auto glideTo = [&](bool switchOffMidway) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        // Unison keeps it monophonic, and one voice keeps the zero-crossing
        // measurement clean: a wider free-running stack retains several phase
        // histories and is not a suitable single-frequency meter fixture.
        parameters.keyMode = KeyMode::Unison;
        parameters.polyphony = 1;
        parameters.portamento = 0.8f;
        engine.setParameters(parameters);

        engine.noteOn(48, 1.0f);
        render(engine, static_cast<int>(sampleRate * 0.3));
        engine.noteOn(72, 1.0f);
        const auto midway = render(engine, static_cast<int>(sampleRate * 0.1));
        const double partway = measuredFrequency(midway.left, midway.left.size() / 2,
                                                 sampleRate);

        if (switchOffMidway)
        {
            parameters.portamento = 0.0f;
            engine.setParameters(parameters);
        }
        const auto after = render(engine, static_cast<int>(sampleRate * 0.05));
        return std::pair<double, double> {
            partway, measuredFrequency(after.left, after.left.size() / 2, sampleRate) };
    };

    const double destination = 440.0 * std::pow(2.0, (72 - 69) / 12.0);
    const auto leftRunning = glideTo(false);
    const auto switchedOff = glideTo(true);

    // The fixture is only meaningful if the note really is still gliding when
    // the control is moved.
    expect(leftRunning.first < destination * 0.9,
           "the glide had already finished before the control was moved");
    expect(leftRunning.second < destination * 0.95,
           "the glide finished on its own, so switching it off proves nothing");
    expectNear(switchedOff.second, destination, destination * 0.01,
               "turning portamento off mid-glide did not land the note");
}

void testOscillatorSurvivesMoreThanOneCyclePerSample()
{
    // The note timer's divider bottoms out at eight, so the top range reaches
    // half a megahertz -- far above any rate the engine runs at. Nothing about
    // a waveform that fast is representable, and this does not pretend to
    // check its pitch. What it does check is that the oscillator walks every
    // crossed cycle instead of collapsing them: the state machine stays
    // consistent, the output stays finite and bounded, and the per-sample loop
    // cannot run away.
    for (double sampleRate : { 8000.0, 48000.0 })
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, false);
        auto parameters = plainPatch();
        parameters.range = DcoRange::Four;
        parameters.keyTranspose = 12;
        parameters.pulseEnabled = true;
        parameters.subLevel = 1.0f;
        engine.setParameters(parameters);

        for (int note = 120; note <= 127; ++note)
            engine.noteOn(note, 1.0f);
        const auto rendered = render(engine, static_cast<int>(sampleRate * 0.25));

        for (std::size_t index = 0; index < rendered.left.size(); ++index)
            if (!std::isfinite(rendered.left[index])
                || !std::isfinite(rendered.right[index]))
            {
                expect(false, "the oscillator produced a non-finite sample above "
                              "one cycle per sample");
                break;
            }
        expect(peakOf(rendered.left, 0) < 4.0,
               "the oscillator ran away above one cycle per sample");
    }
}

void testModulationDelayRearmsForANewPhrase()
{
    // The delay is per phrase, and a phrase begins when nothing is sounding and
    // no key is down. The last voice can retire part-way through a render call,
    // so waiting for an idle scan pass to notice would let the next note start
    // with the modulation already faded all the way in.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.lfoDelay = 0.7f;
    parameters.dcoLfoDepth = 1.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    // Long enough for the delay to finish and the modulation to reach full.
    render(engine, static_cast<int>(sampleRate * 4.0));
    const double atFullDepth = std::abs(engine.getDisplayLfo());
    expect(atFullDepth > 0.05, "the modulation never faded in");

    engine.noteOff(60);
    // Exactly one block past the retirement, so the idle scan branch has had no
    // block of its own in which to notice.
    for (int block = 0; block < 200 && engine.getActiveVoiceCount() > 0; ++block)
        render(engine, blockSize);
    expect(engine.getActiveVoiceCount() == 0, "the voice never retired");

    engine.noteOn(67, 1.0f);
    render(engine, blockSize);
    expect(std::abs(engine.getDisplayLfo()) < atFullDepth * 0.25,
           "a new phrase started with the previous phrase's modulation depth");
}

void testModulationDelayGatesPulseWidthToo()
{
    // There is one LFO and one delay attenuator, and the CPU scales the value
    // once before it distributes it. PWM therefore has to arrive gated exactly
    // as the pitch and cutoff writes do, and as the panel LFO display already
    // shows. Both distribution paths are checked: the scanned converter write
    // that a held note exercises, and the idle-priming path setParameters runs
    // when the output is empty.
    constexpr double sampleRate = 48000.0;
    // LFO RATE 0.75 is 7.4405 Hz, a 134.4 ms period, so a window shorter than
    // one full cycle reads an alignment-dependent span rather than the depth:
    // at t = 6.00 s, full release either way, an 83 ms probe reads 0.4129 and
    // this 200 ms one reads 0.4166.
    constexpr double windowSeconds = 0.200;
    constexpr int windowSamples = static_cast<int>(windowSeconds * sampleRate);

    auto parameters = plainPatch();
    parameters.sawEnabled = false;
    parameters.pulseEnabled = true;
    parameters.pwmSource = PwmSource::Lfo;
    parameters.pwmDepth = 1.0f;
    parameters.lfoRate = 0.75f;
    parameters.lfoDelay = 1.0f;

    std::vector<float> left(blockSize, 0.0f);
    std::vector<float> right(blockSize, 0.0f);
    const auto advance = [&](YouKnow106Engine& engine, double seconds) {
        const int samples = static_cast<int>(seconds * sampleRate);
        for (int done = 0; done < samples; done += blockSize)
            engine.process(left.data(), right.data(),
                           std::min(blockSize, samples - done));
    };
    const auto dutySpanOverWindow = [&](YouKnow106Engine& engine) {
        float lowest = 1.0f;
        float highest = 0.0f;
        for (int sample = 0; sample < windowSamples; ++sample)
        {
            engine.process(left.data(), right.data(), 1);
            const float duty = YouKnow106TestAccess::pulseDuty(engine, 0);
            lowest = std::min(lowest, duty);
            highest = std::max(highest, duty);
        }
        return static_cast<double>(highest - lowest);
    };

    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, false);
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);

    // DELAY 1.0 is a 4.36 s hold, so nothing at all has been released here.
    advance(engine, 0.05);
    expect(YouKnow106TestAccess::lfoDelayLevel(engine) == 0.0f,
           "the delay envelope had already released at t = 0.05 s");
    const double heldSpan = dutySpanOverWindow(engine);
    expect(heldSpan < 0.005,
           "the pulse width swept " + std::to_string(heldSpan)
               + " while the delay envelope was still shut");

    // Well past the hold and the stepped fade, where the gated and the raw
    // value are the same float and the routing can make no difference.
    advance(engine, 6.0 - 0.05 - windowSeconds);
    expect(YouKnow106TestAccess::lfoDelayLevel(engine) == 1.0f,
           "the delay envelope had not fully released at t = 6.00 s");
    const double releasedSpan = dutySpanOverWindow(engine);
    expectNear(releasedSpan, 0.4166, 0.4166 * 0.01,
               "gating the converter write changed full-depth pulse-width "
               "modulation");

    // The same distribution runs on the host thread when a snapshot arrives
    // with the output path empty. A patch change during a silence must prime
    // the one shared PWM hold from the gated value as well, or the next attack
    // starts at a duty the delay envelope has not authorised.
    YouKnow106Engine idle;
    idle.prepare(sampleRate, blockSize, false);
    idle.setParameters(parameters);
    // 30 ms of idle running puts the LFO on its positive peak with the delay
    // envelope still shut, which is where the two values are furthest apart.
    advance(idle, 0.030);
    expect(YouKnow106TestAccess::lfoValue(idle) == 1.0f,
           "the idle LFO did not reach its positive peak in 30 ms");
    expect(YouKnow106TestAccess::lfoDelayLevel(idle) == 0.0f,
           "the idle delay envelope had already released after 30 ms");
    idle.setParameters(parameters);
    // pwmControlVolts(0.5) is +3.3 V, the middle of the comparator's travel;
    // the ungated peak would prime +0.6 V, the far end of it.
    expectNear(YouKnow106TestAccess::pwmHeld(idle),
               YouKnow106Engine::pwmControlVolts(0.5f), 1.0e-6,
               "the idle snapshot primed the PWM hold from the ungated LFO");
}

void testScanTimingSurvivesAProcessingRateChange()
{
    // Converter progress is stored in normalized passes, not internal samples.
    // A rate change must preserve that progress while still keeping the next
    // write within the same physical 4.2 ms pass.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    engine.setParameters(parameters);
    expect(engine.getOversamplingFactor() > 1, "the fixture needs the deep setting");

    // Idle, so only the fixed safety fade defers the change; it is stopped
    // part-way between converter passes.
    render(engine, 64);
    expect(!engine.setOversamplingEnabled(false),
           "the idle quality change skipped its safety fade");
    renderExact(engine, static_cast<int>(sampleRate * 0.006));
    expect(engine.getOversamplingFactor() == 1, "the rate did not drop");

    engine.noteOn(60, 1.0f);
    // One documented scan interval, plus the amplifier's own slew.
    const auto onset = render(engine, static_cast<int>(sampleRate * 0.007));
    expect(peakOf(onset.left, 0) > 0.001,
           "a note after a rate change waited a stale scan interval to speak");
}

void testUnisonStackGlidesFromOneOrigin()
{
    // The stack is one note, so widening it mid-performance must not leave the
    // slots that had been idle starting where the others are still heading.
    // They all glide from the same place or the mode is not unison.
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    parameters.polyphony = 1;
    parameters.portamento = 0.8f;
    engine.setParameters(parameters);

    engine.noteOn(48, 1.0f);
    render(engine, static_cast<int>(sampleRate * 0.3));

    // Widen the stack, then move: five slots wake up for this note.
    parameters.polyphony = 6;
    engine.setParameters(parameters);
    engine.noteOn(64, 1.0f);
    const auto gliding = render(engine, static_cast<int>(sampleRate * 0.1));
    expect(engine.getActiveVoiceCount() == 6, "the stack did not widen");

    const std::size_t window = gliding.left.size() / 2;
    const double destination = 440.0 * std::pow(2.0, (64 - 69) / 12.0);
    const double atDestination =
        magnitudeAt(gliding.left, gliding.left.size() - window,
                    static_cast<int>(window), destination, sampleRate);
    const double sounding = peakOf(gliding.left, gliding.left.size() - window);
    expect(sounding > 0.01, "the widened stack is not sounding at all");
    expect(atDestination < sounding * 0.1,
           "part of the unison stack jumped to the new note instead of gliding");
}

void testQualityChangeWaitsForTheOutputPathToEmpty()
{
    // The last voice retiring is not the instrument going quiet: the delay
    // lines still hold several milliseconds of it. Switching the processing
    // rate empties them, so the switch has to wait for them to run dry.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.chorus = ChorusMode::Two;
    engine.setParameters(parameters);
    const int deepFactor = engine.getOversamplingFactor();
    expect(deepFactor > 1, "the fixture needs the deeper quality setting running");

    engine.noteOn(60, 1.0f);
    render(engine, 8192);
    engine.noteOff(60);
    // Just past the point where the voices retire, with the lines still full.
    for (int block = 0; block < 40 && engine.getActiveVoiceCount() > 0; ++block)
        render(engine, blockSize);
    expect(engine.getActiveVoiceCount() == 0, "the voice never retired");

    expect(!engine.setOversamplingEnabled(false),
           "the quality change applied while the delay lines were still full");
    render(engine, blockSize);
    expect(engine.getOversamplingFactor() == deepFactor,
           "the quality change took effect before the output path had emptied");

    // Once they have run dry it goes through by itself.
    render(engine, static_cast<int>(sampleRate * 0.1));
    expect(engine.getOversamplingFactor() == 1,
           "the quality change never applied after the output path emptied");
}

void testQualityChangeFadesRateDependentOutputPath()
{
    // A chorus with its analogue noise enabled is never literally silent.
    // Rebuilding its rate-dependent lines at unity gain exposes the reset as
    // a click even after every voice and musical tail is gone.
    constexpr double sampleRate = 48000.0;
    auto parameters = plainPatch();
    parameters.sawEnabled = false;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.chorus = ChorusMode::Two;
    parameters.chorusNoise = 1.0f;
    const auto transitionAtBlockSize = [&](int processingBlock) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        engine.setParameters(parameters);
        renderExact(engine, static_cast<int>(sampleRate * 0.1));
        const int deepFactor = engine.getOversamplingFactor();
        expect(deepFactor > 1, "the chorus-noise fixture needs HQ processing");
        expect(!engine.setOversamplingEnabled(false),
               "an audible rate-dependent path changed without a fade");
        expect(engine.getOversamplingFactor() == deepFactor,
               "the processing rate changed before its fade reached zero");

        Render rendered;
        rendered.left.assign(2048, 0.0f);
        rendered.right.assign(2048, 0.0f);
        for (int offset = 0; offset < 2048; offset += processingBlock)
        {
            const int count = std::min(processingBlock, 2048 - offset);
            engine.process(rendered.left.data() + offset,
                           rendered.right.data() + offset, count);
        }
        expect(engine.getOversamplingFactor() == 1,
               "the rate change never completed at block size "
                   + std::to_string(processingBlock));
        return rendered;
    };

    const auto reference = transitionAtBlockSize(1);
    int zeroBoundary = -1;
    int longestZeroRun = 0;
    int zeroRun = 0;
    for (std::size_t index = 0; index < reference.left.size(); ++index)
    {
        const bool zero = reference.left[index] == 0.0f
                       && reference.right[index] == 0.0f;
        zeroRun = zero ? zeroRun + 1 : 0;
        longestZeroRun = std::max(longestZeroRun, zeroRun);
        if (zero && zeroBoundary < 0
            && index >= static_cast<std::size_t>(sampleRate * 0.004)
            && index <= static_cast<std::size_t>(sampleRate * 0.007))
            zeroBoundary = static_cast<int>(index);
    }
    expect(zeroBoundary >= 0,
           "the rate-dependent reset did not occur at the zero-gain boundary");
    expect(longestZeroRun <= 2,
           "the rate transition inserted a mute longer than its boundary sample");
    if (zeroBoundary > 0)
    {
        const auto index = static_cast<std::size_t>(zeroBoundary);
        expect(std::abs(reference.left[index] - reference.left[index - 1]) < 1.0e-4f
                   && std::abs(reference.right[index] - reference.right[index - 1])
                          < 1.0e-4f,
               "the rate-dependent reset was audible at the zero-gain boundary");
    }
    expect(peakOf(reference.left, reference.left.size() * 3 / 4) > 1.0e-5,
           "the output remained faded after the rate change");

    // The split at zero must occur at the same sample even when a host sends a
    // block much longer than the fade. The former block-entry-only switch
    // produced up to 38 ms of extra silence at a 2048-sample block.
    for (const int processingBlock : { 64, 256, 2048 })
    {
        const auto blocked = transitionAtBlockSize(processingBlock);
        expect(maximumDifference(blocked.left, reference.left) < 1.0e-6
                   && maximumDifference(blocked.right, reference.right) < 1.0e-6,
               "the quality transition depends on host block size "
                   + std::to_string(processingBlock));
    }

    // Once the fade has begun, changing the control which made the old path
    // visibly audible must not bypass it halfway down.
    YouKnow106Engine controlChanged;
    controlChanged.prepare(sampleRate, blockSize, true);
    controlChanged.setParameters(parameters);
    renderExact(controlChanged, static_cast<int>(sampleRate * 0.1));
    const int controlChangedDeepFactor =
        controlChanged.getOversamplingFactor();
    expect(!controlChanged.setOversamplingEnabled(false),
           "the control-change fixture skipped its safety fade");
    renderExact(controlChanged, static_cast<int>(sampleRate * 0.002));
    const float gainBeforeControlChange =
        YouKnow106TestAccess::rateTransitionGain(controlChanged);
    parameters.chorus = ChorusMode::Off;
    parameters.chorusNoise = 0.0f;
    controlChanged.setParameters(parameters);
    renderExact(controlChanged, 1);
    expect(controlChanged.getOversamplingFactor() == controlChangedDeepFactor,
           "turning Chorus off bypassed an unfinished quality fade");
    expect(YouKnow106TestAccess::rateTransitionGain(controlChanged)
               < gainBeforeControlChange,
           "turning Chorus off reversed an unfinished quality fade");
    renderExact(controlChanged, static_cast<int>(sampleRate * 0.01));
    expect(controlChanged.getOversamplingFactor() == 1,
           "the quality change did not finish after Chorus was turned off");

    // If performance resumes before zero, preserving the note is more
    // important than completing a UI quality request. The pending rate stays
    // deferred and the output reverses back to unity.
    YouKnow106Engine interrupted;
    interrupted.prepare(sampleRate, blockSize, true);
    parameters.chorus = ChorusMode::Two;
    parameters.chorusNoise = 1.0f;
    parameters.sawEnabled = true;
    interrupted.setParameters(parameters);
    renderExact(interrupted, static_cast<int>(sampleRate * 0.1));
    const int interruptedDeepFactor = interrupted.getOversamplingFactor();
    expect(!interrupted.setOversamplingEnabled(false),
           "the interrupted fixture did not begin a chorus-noise fade");
    renderExact(interrupted, static_cast<int>(sampleRate * 0.002));
    interrupted.noteOn(60, 1.0f);
    const auto note = renderExact(
        interrupted, static_cast<int>(sampleRate * 0.02));
    expect(interrupted.getOversamplingFactor() == interruptedDeepFactor,
           "a quality change completed after a new voice started");
    expect(peakOf(note.left, note.left.size() / 2) > 0.01,
           "a pending quality fade muted a newly started voice");
}

void testQualityChangePreservesOutputCouplingTail()
{
    // A 95%-duty pulse charges the final capacitor. Once its Gate VCA closes,
    // the host-rate 115 ms tail remains long after the internal chorus and
    // decimator wait. Rebuilding HQ must not clear that rate-independent
    // state; an earlier implementation made a roughly -19 dBFS step here.
    //
    // Six cards at full VCA LEVEL, not one at 99. This fixture has now lost
    // two DC paths to two modelled capacitors, and both losses were the point
    // of modelling them. It first leaned on the mixer's raw PWM DC reaching
    // C17/C20 intact, which C56/C50 stopped; it then leaned on each card's own
    // 0.48 Hz module coupling passing the note-on duty step as a slow
    // transient, six of them summing to 0.026, which C59 stops as well -- no
    // per-voice offset of any kind now survives to the voice VCA. What is left
    // charging the final capacitor is the gate closure itself: the Gate VCA
    // shuts on a waveform that is not at zero, and the six cards' steps sum to
    // 0.0037 at 100-120 ms, seven times the bound below. That is the smallest
    // this fixture can legitimately get, because it is no longer reading any
    // DC the model manufactures -- only the step a real gate leaves.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine switched;
    YouKnow106Engine reference;
    switched.prepare(sampleRate, blockSize, true);
    reference.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.sawEnabled = false;
    parameters.pulseEnabled = true;
    parameters.pwmSource = PwmSource::Manual;
    parameters.pwmDepth = 1.0f;
    parameters.vcaMode = VcaMode::Gate;
    parameters.chorus = ChorusMode::Off;
    parameters.chorusNoise = 0.0f;
    parameters.vcaLevel = 1.0f;
    switched.setParameters(parameters);
    reference.setParameters(parameters);
    constexpr std::array chordNotes { 36, 40, 43, 47, 50, 53 };
    for (const int note : chordNotes)
    {
        switched.noteOn(note, 1.0f);
        reference.noteOn(note, 1.0f);
    }
    renderExact(switched, static_cast<int>(sampleRate));
    renderExact(reference, static_cast<int>(sampleRate));
    for (const int note : chordNotes)
    {
        switched.noteOff(note);
        reference.noteOff(note);
    }
    expect(!switched.setOversamplingEnabled(false),
           "the quality change ignored the still-sounding gate release");

    const auto changed = renderExact(
        switched, static_cast<int>(sampleRate * 0.12));
    const auto unchanged = renderExact(
        reference, static_cast<int>(sampleRate * 0.12));
    expect(switched.getOversamplingFactor() == 1,
           "the coupling-tail quality change never completed");
    const std::size_t compareFrom = changed.left.size()
                                  - static_cast<std::size_t>(sampleRate * 0.02);
    expect(peakOf(unchanged.left, compareFrom) > 0.002,
           "the asymmetric-PWM fixture produced no coupling tail to preserve");
    double difference = 0.0;
    for (std::size_t index = compareFrom; index < changed.left.size(); ++index)
    {
        difference = std::max(
            difference,
            static_cast<double>(std::abs(changed.left[index]
                                       - unchanged.left[index])));
        difference = std::max(
            difference,
            static_cast<double>(std::abs(changed.right[index]
                                       - unchanged.right[index])));
    }
    // C14/C12 and the switched HPF continue at the new internal rate, so the
    // complete tail need not be sample-identical to the unchanged reference.
    // Resetting C17/C20 would move it by the reference tail itself (>0.002);
    // this narrow bound still distinguishes preservation from a reset.
    expect(difference < 5.0e-4,
           "an HQ rebuild displaced the preserved analogue/output-coupling tail by "
               + std::to_string(difference));
}

void testQualityChangePreservesFreeRunningClocks()
{
    // HQ is a numerical option, not a power cycle. The converter scan, DCOs
    // and chorus oscillator all free-run in physical time while an idle rate
    // transition rebuilds sample-domain histories behind its safety fade.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine switched;
    YouKnow106Engine reference;
    switched.prepare(sampleRate, blockSize, true);
    reference.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.chorus = ChorusMode::Off;
    parameters.chorusNoise = 0.0f;
    switched.setParameters(parameters);
    reference.setParameters(parameters);

    renderExact(switched, static_cast<int>(sampleRate * 0.1));
    renderExact(reference, static_cast<int>(sampleRate * 0.1));
    const double deepPeriod =
        YouKnow106TestAccess::dcoPeriodSamples(switched, 0);
    expect(!switched.setOversamplingEnabled(false),
           "the free-running-clock fixture skipped the quality fade");

    renderExact(switched, static_cast<int>(sampleRate * 0.02));
    renderExact(reference, static_cast<int>(sampleRate * 0.02));
    expect(switched.getOversamplingFactor() == 1,
           "the free-running-clock quality change never completed");

    const auto circularError = [](double first, double second) {
        const double distance = std::abs(first - second);
        return std::min(distance, 1.0 - distance);
    };
    expect(circularError(YouKnow106TestAccess::dcoPhase(switched, 0),
                         YouKnow106TestAccess::dcoPhase(reference, 0))
               < 2.0e-8,
           "an HQ rebuild restarted or displaced a free-running DCO");
    expect(circularError(YouKnow106TestAccess::chorusPhase(switched),
                         YouKnow106TestAccess::chorusPhase(reference))
               < 2.0e-8,
           "an HQ rebuild restarted the free-running chorus oscillator");
    expect(circularError(YouKnow106TestAccess::controlScanPhase(switched),
                         YouKnow106TestAccess::controlScanPhase(reference))
               < 2.0e-8,
           "an HQ rebuild restarted the firmware converter scan");
    expectNear(YouKnow106TestAccess::dcoPeriodSamples(switched, 0),
               deepPeriod / 4.0, 1.0e-9,
               "a quality rebuild left the DCO period in old-rate samples");
}

void testModuleInputCouplingKeepsMixerDcOutOfTheVoiceVca()
{
    // Module board p. 13: the summed WAVE node reaches pin 1 VCF IN only
    // through C56/C50 10 uF NP, so no mixer DC reaches the filter core or the
    // voice VCA behind it. The pulse carries the most of it -- the
    // comparator's mean walks with duty -- and without the capacitor that DC
    // multiplied by the envelope in the VCA, leaving a note-on thump that grew
    // *louder* with PWM depth. The capacitance is a designator-level read; the
    // resistance it works against is not, so the corner is voiced (OQ-15).
    constexpr double sampleRate = 48000.0;
    expectNear(YouKnow106Engine::moduleCouplingCornerHz(), 0.482288, 1.0e-5,
               "the module-input coupling corner left its 10 uF / 33 kOhm pair");

    // The thump must fall as PWM deepens, not rise. Measured as the peak of a
    // 50 ms window after note-on, against the settled sustain level.
    double previousRatio = 1.0e9;
    for (const float depth : { 0.0f, 0.3f, 0.6f, 1.0f })
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.sawEnabled = false;
        parameters.pulseEnabled = true;
        parameters.pwmSource = PwmSource::Manual;
        parameters.pwmDepth = depth;
        parameters.vcaMode = VcaMode::Envelope;
        parameters.attack = 0.05f;
        parameters.decay = 0.45f;
        parameters.sustain = 0.7f;
        parameters.release = 0.3f;
        parameters.chorus = ChorusMode::Off;
        parameters.chorusNoise = 0.0f;
        engine.setParameters(parameters);
        // Let the coupling capacitors settle first. The DCOs free-run behind
        // the closed VCAs, so a cold engine spends about 3 x 330 ms charging
        // C56/C50 to the standing duty offset -- a power-on transient the real
        // instrument also has, and not what this fixture is about. Measuring
        // without it would read that charging curve and see the thump grow
        // with depth exactly as the uncoupled model did.
        renderExact(engine, static_cast<int>(sampleRate));
        engine.noteOn(48, 1.0f);

        const auto rendered = renderExact(
            engine, static_cast<int>(sampleRate * 1.5));
        // A 50 ms boxcar isolates the sub-audio thump from the pulse itself.
        const auto window = static_cast<std::size_t>(sampleRate * 0.05);
        double running = 0.0;
        for (std::size_t index = 0; index < window; ++index)
            running += rendered.left[index];
        double thump = std::abs(running) / static_cast<double>(window);
        for (std::size_t index = window; index < window * 4; ++index)
        {
            running += rendered.left[index] - rendered.left[index - window];
            thump = std::max(thump,
                             std::abs(running) / static_cast<double>(window));
        }
        const double sustained = peakOf(
            rendered.left,
            rendered.left.size() - static_cast<std::size_t>(sampleRate * 0.5));
        expect(sustained > 0.01,
               "the module-coupling fixture produced no sustained tone");
        const double ratio = thump / sustained;
        expect(ratio < previousRatio,
               "deepening PWM raised the note-on thump, so mixer DC is "
               "reaching the voice VCA again at depth "
                   + std::to_string(depth));
        previousRatio = ratio;
    }
}

void testFilterToVcaCouplingRemovesTheDutyDependentThump()
{
    // Module board pp. 18-19: pin 3 VCF OUT reaches pin 9 VCA IN only through
    // C59 1 uF/50 V NP and the VR27/R108 network, and the service procedure
    // trims VR30/25/20/15/10/5 through R112 2.2 MOhm at that same node for
    // minimum thump. The filter core makes DC of its own -- the stage offsets
    // sit inside the loop and an enabled pulse arrives duty asymmetric -- so
    // without the capacitor the envelope multiplies a standing offset and
    // leaves a sub-audio bump whose size walks with PWM duty.
    //
    // The capacitor is anchored; the load is not (R108 and VR27's setting are
    // not in tree), so what is fenced here is the declared bracket rather than
    // a value: 100 kOhm gives 1.59 Hz and 33 kOhm gives 4.82 Hz, both far
    // enough below the lowest note that the audible consequence is the DC
    // block itself and not the corner.
    const double corner = YouKnow106Engine::vcaInputCouplingCornerHz();
    expect(corner >= 1.59 && corner <= 4.83,
           "the filter-to-VCA coupling corner left its declared 33-100 kOhm "
           "bracket (got " + std::to_string(corner) + " Hz)");

    constexpr double sampleRate = 48000.0;
    constexpr int probeBlock = 64;
    constexpr double renderSeconds = 8.0;
    constexpr double releaseSeconds = 4.0;
    // A cascade of four one-pole low-pass sections. The order is part of the
    // measurand and not an implementation detail: on the same render at the
    // same duty this engine reads -9.68 dB through one pole and -42.49 dB
    // through four, so a "20 Hz low-pass" that does not say how steep says
    // nothing.
    struct SubAudioLowPass
    {
        std::array<double, 4> state {};
        double process(double input, double g) noexcept
        {
            double signal = input;
            for (auto& section : state)
            {
                const double v = (signal - section) * g / (1.0 + g);
                const double low = v + section;
                section = low + v;
                signal = low;
            }
            return signal;
        }
    };

    struct CouplingRun
    {
        float duty { 0.0f };
        double vcaInputDcVolts { 0.0 };
        double subAudioDb { 0.0 };
    };

    const auto measure = [&](float pwmPanel)
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, probeBlock, true);

        EngineParameters parameters;
        parameters.sawEnabled = false;
        parameters.pulseEnabled = true;
        parameters.subLevel = 0.0f;
        parameters.noiseLevel = 0.0f;
        parameters.pwmSource = PwmSource::Manual;
        parameters.pwmDepth = pwmPanel;
        parameters.highPass = HighPassMode::One;
        parameters.cutoff = 0.30f;
        parameters.resonance = 0.75f;
        parameters.envDepth = 0.35f;
        parameters.keyFollow = 0.50f;
        parameters.vcaMode = VcaMode::Envelope;
        parameters.vcaLevel = 0.80f;
        parameters.attack = 0.45f;
        parameters.decay = 1.0f;
        parameters.sustain = 1.0f;
        parameters.release = 0.30f;
        parameters.volume = 0.80f;
        parameters.chorus = ChorusMode::Off;
        // Unit Character 1.0 deliberately: the point of the fixture is that
        // this DC is a property of the nominal calibrated model, and the card
        // spread is left on so the assertion holds with it too.
        parameters.calibration = 1.0f;
        engine.setParameters(parameters);
        engine.reset();
        engine.setParameters(parameters);
        engine.noteOn(48, 1.0f);

        const int total = static_cast<int>(sampleRate * renderSeconds);
        const int releaseAt = static_cast<int>(sampleRate * releaseSeconds);
        std::array<float, probeBlock> left {};
        std::array<float, probeBlock> right {};

        SubAudioLowPass lowPass;
        const double g = std::tan(pi * 20.0 / sampleRate);
        double sumOfSquares = 0.0;
        double subAudioPeak = 0.0;
        // A rectangular mean of the pin 9 node over the sustain window leaves
        // up to 2.7 mV of the 130 Hz note behind, because the window is not an
        // integer number of its periods -- more than the 1 mV the DC assertion
        // is worth stating, and enough to fail it after a correct fix. A Hann
        // weight over the same window reads 0.17 mV there, and leaves a real
        // offset alone.
        double weightedSum = 0.0;
        double weight = 0.0;
        bool sustained = true;
        bool released = false;
        for (int offset = 0; offset < total; offset += probeBlock)
        {
            if (!released && offset >= releaseAt)
            {
                engine.noteOff(48);
                released = true;
            }
            engine.process(left.data(), right.data(), probeBlock);
            for (int index = 0; index < probeBlock; ++index)
            {
                const double sample = left[static_cast<std::size_t>(index)];
                sumOfSquares += sample * sample;
                subAudioPeak = std::max(
                    subAudioPeak, std::abs(lowPass.process(sample, g)));
            }
            const double seconds = static_cast<double>(offset) / sampleRate;
            if (seconds >= 1.5 && seconds <= 3.0)
            {
                const double phase = (seconds - 1.5) / 1.5;
                const double hann = 0.5 - 0.5 * std::cos(2.0 * pi * phase);
                weightedSum +=
                    hann * YouKnow106TestAccess::vcaInputVolts(engine, 0);
                weight += hann;
                sustained = sustained
                         && YouKnow106TestAccess::voiceActive(engine, 0);
            }
        }
        expect(sustained,
               "the coupling fixture's voice stopped sounding inside the "
               "1.5-3.0 s window, so the DC below is not a sustained node");
        const double rms = std::sqrt(sumOfSquares / static_cast<double>(total));
        expect(rms > 0.005,
               "the filter-to-VCA coupling fixture produced no tone at panel "
                   + std::to_string(pwmPanel));

        CouplingRun run;
        run.duty = YouKnow106TestAccess::pulseDuty(engine, 0);
        run.vcaInputDcVolts = weightedSum / weight;
        run.subAudioDb = 20.0 * std::log10(subAudioPeak / rms);
        return run;
    };

    // (a) The node the amplifier multiplies carries no standing offset. This
    // is the assertion that isolates the capacitor: every downstream coupling
    // is on the far side of the multiply, so it cannot flatter this reading.
    // Without C59 the same window reads +0.0428 V at panel 0.00, +0.0244 V at
    // panel 0.50 and +0.0298 V at panel 1.00 -- 24 to 43 times the bound.
    const CouplingRun open = measure(0.00f);
    const CouplingRun middle = measure(0.50f);
    const CouplingRun narrow = measure(1.00f);
    expectNear(open.duty, 0.5043, 1.0e-3,
               "the coupling fixture's PWM panel 0.00 left its 0.5043 duty");
    expectNear(narrow.duty, 0.9436, 1.0e-3,
               "the coupling fixture's PWM panel 1.00 left its 0.9436 duty");
    for (const auto& run : { open, middle, narrow })
    {
        expect(std::abs(run.vcaInputDcVolts) < 1.0e-3,
               "the voice amplifier is multiplying filter DC again at duty "
                   + std::to_string(run.duty) + " (pin 9 mean "
                   + std::to_string(run.vcaInputDcVolts) + " V)");
    }

    // (b) The audible consequence, at the duty that carries the most of it.
    // The bump is an envelope-shaped excursion, so it is read as the peak of
    // the whole render -- note-on and note-off transients included -- against
    // broadband RMS. Without C59 this is -17.55 dB; with it, -30.25 dB. The
    // remainder is not DC: it is the attack's own amplitude ramp, which is
    // slower than the coupling's 4.82 Hz corner and is a real property of a
    // note that starts, so the fence sits at 25 dB rather than at the 35 dB
    // the plan first asked for, which no correct implementation reaches.
    expect(narrow.subAudioDb <= -25.0,
           "the sub-20 Hz thump at duty 0.9436 is back above 25 dB under RMS "
           "(got " + std::to_string(narrow.subAudioDb) + " dB)");
    // One-sided, and it does not bite today: blocking DC also removes the
    // sub-5 Hz part of the note gate, so this figure must fall. It guards
    // only against a coupling that trades the narrow duty for the wide one.
    expect(open.subAudioDb <= -39.5,
           "the sub-20 Hz thump at duty 0.5043 rose above its uncoupled "
           "-42.49 dB (got " + std::to_string(open.subAudioDb) + " dB)");
}

void testFinalOutputCouplingRemovesManualPwmDc()
{
    // Manual PWM can make the common dry waveform strongly asymmetric. The
    // original unit removes that offset only after IC6 has recombined dry and
    // wet, through C17/C20 immediately before the stereo VOLUME control.
    constexpr double sampleRate = 48000.0;
    for (const auto chorus : { ChorusMode::Off, ChorusMode::One,
                              ChorusMode::Two })
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.sawEnabled = false;
        parameters.pulseEnabled = true;
        parameters.pwmSource = PwmSource::Manual;
        parameters.pwmDepth = 1.0f;
        parameters.subLevel = 0.0f;
        parameters.noiseLevel = 0.0f;
        parameters.vcaMode = VcaMode::Gate;
        parameters.chorus = chorus;
        parameters.chorusNoise = 0.0f;
        engine.setParameters(parameters);
        engine.noteOn(36, 1.0f);

        const auto rendered = renderExact(
            engine, static_cast<int>(sampleRate * 1.5));
        const std::size_t start = rendered.left.size()
                                - static_cast<std::size_t>(sampleRate * 0.5);
        const auto meanFrom = [start](const std::vector<float>& signal) {
            double sum = 0.0;
            for (std::size_t index = start; index < signal.size(); ++index)
                sum += signal[index];
            return sum / static_cast<double>(signal.size() - start);
        };
        expect(std::abs(meanFrom(rendered.left)) < 0.002,
               "manual PWM left residual DC on the final left output");
        expect(std::abs(meanFrom(rendered.right)) < 0.002,
               "manual PWM left residual DC on the final right output");
    }

    // The capacitors precede the pot and remain driven while its wipers are at
    // zero. After raising VOLUME, a preconditioned silent engine must converge
    // onto an otherwise identical audible engine without a fresh 115 ms
    // charging transient.
    YouKnow106Engine silent;
    YouKnow106Engine audible;
    silent.prepare(sampleRate, blockSize, true);
    audible.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.sawEnabled = false;
    parameters.pulseEnabled = true;
    parameters.pwmSource = PwmSource::Manual;
    parameters.pwmDepth = 1.0f;
    parameters.vcaMode = VcaMode::Gate;
    parameters.volume = 0.0f;
    silent.setParameters(parameters);
    parameters.volume = 1.0f;
    audible.setParameters(parameters);
    silent.noteOn(36, 1.0f);
    audible.noteOn(36, 1.0f);
    // Observe the initial charge before C14 and C12 have themselves removed
    // the deliberately asymmetric pulse train's DC. Waiting the full two
    // seconds first would correctly leave every coupling state close to zero.
    renderExact(silent, static_cast<int>(sampleRate * 0.05));
    renderExact(audible, static_cast<int>(sampleRate * 0.05));
    expect(std::abs(YouKnow106TestAccess::outputCouplingState(silent)) > 1.0e-3,
           "VOLUME zero prevented the upstream C17 state from charging");
    renderExact(silent, static_cast<int>(sampleRate * 1.95));
    renderExact(audible, static_cast<int>(sampleRate * 1.95));

    const double chargedAtZero =
        YouKnow106TestAccess::outputCouplingState(silent);
    parameters.volume = 1.0f;
    silent.setParameters(parameters);
    expect(YouKnow106TestAccess::outputCouplingState(silent) == chargedAtZero,
           "moving VOLUME reset the upstream C17 capacitor state");
    const auto raised = renderExact(
        silent, static_cast<int>(sampleRate * 0.3));
    const auto reference = renderExact(
        audible, static_cast<int>(sampleRate * 0.3));
    const std::size_t settled = static_cast<std::size_t>(sampleRate * 0.1);
    double difference = 0.0;
    for (std::size_t index = settled; index < raised.left.size(); ++index)
    {
        difference = std::max(
            difference,
            static_cast<double>(std::abs(raised.left[index]
                                       - reference.left[index])));
        difference = std::max(
            difference,
            static_cast<double>(std::abs(raised.right[index]
                                       - reference.right[index])));
    }
    // The zero and full positions legitimately present different resistances
    // to C17/C20, so their preconditioned states are not sample-identical to an
    // always-full reference. They must nevertheless converge without the much
    // larger transient a cleared capacitor would create.
    expect(difference < 5.0e-4,
           "the loaded VOLUME network failed to preserve/converge its coupling "
           "state (difference " + std::to_string(difference) + ")");
}

void testHardStopSilencesTheWholeOutputPath()
{
    // All Sound Off and Panic mean silent now. Cutting the voices alone leaves
    // the delay lines playing back the last few milliseconds of the chord.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.chorus = ChorusMode::Two;
    // The modelled line hiss would mask the thing being measured, and it is
    // not what the hard stop is being asked to remove.
    parameters.chorusNoise = 0.0f;
    engine.setParameters(parameters);

    engine.noteOn(60, 1.0f);
    const auto sounding = render(engine, 8192);
    expect(peakOf(sounding.left, sounding.left.size() / 2) > 0.01,
           "the fixture never made a sound to stop");

    engine.allNotesOff();
    const auto afterStop = render(engine, blockSize * 4);
    expect(peakOf(afterStop.left, 0) < 1.0e-4,
           "a hard stop left the delay lines playing the note back");
    expect(peakOf(afterStop.right, 0) < 1.0e-4,
           "a hard stop left the right channel ringing");
}

void testComponentDriftRateIsIndependentOfOversampling()
{
    // The modelled component wander has to advance in seconds, not in internal
    // samples, or the same patch would drift four times faster with the quality
    // setting on.
    // The self-oscillating filter is the cleanest probe there is: its pitch
    // tracks the drifting control voltage directly, with no harmonics or
    // beats to confuse the measurement at either rate.
    const auto wander = [](bool oversampled) {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, oversampled);
        auto parameters = plainPatch();
        parameters.calibration = 1.0f;
        parameters.sawEnabled = false;
        parameters.cutoff = 0.45f;
        parameters.resonance = 1.0f;
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);
        const auto rendered = render(engine, static_cast<int>(48000.0 * 3.0));

        // Pitch per quarter-second window; the wander is the total movement.
        double previous = 0.0;
        double movement = 0.0;
        for (std::size_t index = rendered.left.size() / 3;
             index + 12000 <= rendered.left.size(); index += 12000)
        {
            std::vector<float> window(rendered.left.begin() + static_cast<long>(index),
                                      rendered.left.begin() + static_cast<long>(index + 12000));
            const double pitch = measuredFrequency(window, 0, 48000.0);
            if (previous > 0.0 && pitch > 0.0)
                movement += std::abs(pitch - previous);
            if (pitch > 0.0)
                previous = pitch;
        }
        return movement;
    };

    const double deep = wander(true);
    const double shallow = wander(false);
    expect(deep > 0.0 && shallow > 0.0, "no component wander was measurable");
    const double ratio = deep / shallow;
    expect(ratio > 0.5 && ratio < 2.0,
           "component wander advances at a different rate with oversampling on ("
               + std::to_string(ratio) + "x)");
}

void testRailDroopTracksLoadAtOneWallClockRate()
{
    // The regulator loading follows how hard the cards are working, and how
    // fast it follows is a property of the supply, not of a quality setting.
    // A fixed per-internal-sample coefficient would make this converge four
    // times faster with oversampling on, which is what this catches.
    //
    // The three circuit-shape toggles below default true but are deliberately
    // held off here: driving exponential reset, the VCF Early effect and the
    // spatial thermal gradient simultaneously changes the harmonic content
    // enough that the crude 63.2%-threshold settling-time estimate itself
    // starts to differ between oversampling factors (observed ratio 0.5x) --
    // a measurement artifact of this test's detector, not evidence that the
    // droop follower's own wall-clock rate has changed. Worth a closer look
    // (Newton-solver convergence under combined nonlinearity, per the
    // engine's open follow-up list) but out of scope for what this test
    // isolates.
    const auto settlingSeconds = [](bool oversampled) {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, oversampled);
        auto parameters = plainPatch();
        parameters.calibration = 1.0f;
        parameters.vcaLevel = 1.0f;
        parameters.enableVcfEarlyEffect = false;
        parameters.enableSpatialThermalGradient = false;
        engine.setParameters(parameters);
        for (const int note : { 36, 43, 48, 52, 55, 60 })
            engine.noteOn(note, 1.0f);

        // Step the load on and watch the droop rise, one block at a time.
        double final = 0.0;
        for (int block = 0; block < 400; ++block)
        {
            render(engine, blockSize);
            final = engine.getDisplayRailDroopVolts();
        }
        expect(final > 0.0, "no rail droop was measurable");

        YouKnow106Engine again;
        again.prepare(48000.0, blockSize, oversampled);
        again.setParameters(parameters);
        for (const int note : { 36, 43, 48, 52, 55, 60 })
            again.noteOn(note, 1.0f);
        // The droop is the followed voice energy, and the follower's
        // time constant is milliseconds -- the whole rise spans only a few
        // of this test's blocks, so a block-granular detector quantises the
        // crossing onto two or three steps and unrelated onset-shape changes
        // read as large rate ratios. Sample the rise in 16-sample steps
        // instead: the crossing lands roughly a millisecond-per-step scale
        // out (about 15-20 steps), the 4x-fast bug this test exists to catch
        // would quarter that count, and a one-step onset wobble moves the
        // ratio by only a few percent.
        constexpr int chunkSamples = 16;
        constexpr int chunkLimit = 3000;
        int chunks = 0;
        for (; chunks < chunkLimit; ++chunks)
        {
            if (again.getDisplayRailDroopVolts() >= 0.632 * final)
                break;
            renderExact(again, chunkSamples);
        }
        expect(chunks > 4 && chunks < chunkLimit,
               "the droop crossing left the detector's usable window");
        return (chunks + 1) * chunkSamples / 48000.0;
    };

    const double deep = settlingSeconds(true);
    const double shallow = settlingSeconds(false);
    expect(deep > 0.0 && shallow > 0.0, "the rail droop never settled");
    const double ratio = deep / shallow;
    expect(ratio > 0.6 && ratio < 1.7,
           "rail droop tracks its load at a different wall-clock rate with "
           "oversampling on (" + std::to_string(ratio) + "x)");
}

void testThermalWarmupClockRunsToCompletionAtEveryRate()
{
    // The chassis warm-up is wall-clock physics: T(t) = 25 + 15(1 - e^-t/900),
    // with the 15 C rise scaled by Unit Character. Nothing about it belongs to
    // the numerical grid, and the comment on `voiceEnergyFollowerSeconds`
    // in `YouKnow106Engine.h` says so in words for the supply follower --
    // a quality setting is not allowed to change what the supply does.
    //
    // What this fences is the accumulator, not the law. The timer is advanced
    // once per *internal* sample, so its increment is 5.208e-6 s at a 192 kHz
    // internal rate. Held in a float, the running total's ULP overtakes that
    // increment on a power-of-two boundary and every further addition rounds
    // away: the clock stopped dead at 128.0 s, and which boundary caught it
    // depended on the internal rate `prepare()` had selected -- 128.0 s with
    // HQ on, 512.0 s with it off. The modelled chassis therefore froze at
    // 26.99 C or at 31.51 C according to a quality switch, and never reached
    // the 34.48 C its own law asks for.
    struct Configuration
    {
        double hostRate;
        bool highQuality;
        const char* name;
    };
    // 48 kHz HQ on, 96 kHz HQ on and 192 kHz HQ on all run the engine at a
    // 192000 Hz internal rate -- HQ targets a rate rather than multiplying the
    // host's -- so the axis this comparison actually spans is the quality
    // switch and the internal rate it selects. 48 kHz HQ off is the one that
    // lands on the other boundary, and the three HQ-on entries additionally
    // prove the reading does not move with the host rate.
    const Configuration configurations[] = {
        { 48000.0, true, "48 kHz HQ on" },
        { 48000.0, false, "48 kHz HQ off" },
        { 96000.0, true, "96 kHz HQ on" },
        { 192000.0, true, "192 kHz HQ on" },
    };

    struct Mark
    {
        double seconds;
        double celsius;
    };
    // The law's own values. 128 s and 300 s are on the rise; 900 s is the time
    // constant, where the rise has run 1 - 1/e of its course.
    const Mark marks[] = {
        { 128.0, 26.9886 },
        { 300.0, 29.2520 },
        { 900.0, 34.4818 },
    };
    constexpr double celsiusTolerance = 0.05;
    // Unit Character 1.0: getDisplayTemperatureC() scales the rise by
    // `calibration`, so the targets above are that setting's law and no
    // other's. The spatial gradient is off because the headroom target below
    // is the chassis mean's -- with the gradient on, card 0 sits about 4 C
    // hotter and reads 6.6542 V.
    const auto fixtureParameters = [] {
        auto parameters = plainPatch();
        parameters.calibration = 1.0f;
        parameters.enableSpatialThermalGradient = false;
        return parameters;
    };

    constexpr std::size_t markCount = std::size(marks);
    constexpr std::size_t configurationCount = std::size(configurations);
    std::array<std::array<double, markCount>, configurationCount> readings {};
    std::array<double, configurationCount> headroomAt900 {};

    for (std::size_t index = 0; index < configurationCount; ++index)
    {
        const auto& configuration = configurations[index];
        YouKnow106Engine engine;
        engine.prepare(configuration.hostRate, blockSize,
                       configuration.highQuality);
        const auto parameters = fixtureParameters();
        engine.setParameters(parameters);

        const double internalRate =
            YouKnow106TestAccess::internalRate(engine);
        long long advanced = 0;
        for (std::size_t mark = 0; mark < markCount; ++mark)
        {
            const long long target =
                std::llround(marks[mark].seconds * internalRate);
            YouKnow106TestAccess::advanceThermalWarmup(engine,
                                                       target - advanced);
            advanced = target;
            readings[index][mark] = engine.getDisplayTemperatureC();
            expectNear(readings[index][mark], marks[mark].celsius,
                       celsiusTolerance,
                       std::string("the warm-up clock does not reach the "
                                   "modelled temperature at t = ")
                           + std::to_string(static_cast<int>(
                                 marks[mark].seconds))
                           + " s at " + configuration.name);
        }

        headroomAt900[index] =
            YouKnow106TestAccess::otaHeadroomVolts(engine, parameters, 0);
        // 2 Vt(T) / stageAttenuation at 34.4818 C, in module-node volts. It is
        // the number the cascade is actually solved with, so the 2.5% error a
        // frozen clock left in it was a real error on hot patches.
        expectNear(headroomAt900[index], 6.5687, 0.001,
                   std::string("the modelled OTA headroom at 900 s is not the "
                               "warm chassis value at ")
                       + configuration.name);
    }

    for (std::size_t mark = 0; mark < markCount; ++mark)
    {
        double lowest = readings[0][mark];
        double highest = readings[0][mark];
        for (std::size_t index = 1; index < configurationCount; ++index)
        {
            lowest = std::min(lowest, readings[index][mark]);
            highest = std::max(highest, readings[index][mark]);
        }
        expect(highest - lowest <= 0.01,
               std::string("the quality setting moves the modelled chassis "
                           "temperature at t = ")
                   + std::to_string(static_cast<int>(marks[mark].seconds))
                   + " s (spread " + std::to_string(highest - lowest)
                   + " C across 48 kHz HQ on/off, 96 kHz HQ on and 192 kHz "
                     "HQ on)");
    }

    // The clock the fixture drove has to be the clock a render advances, or
    // everything above is a statement about a test friend. Two seconds of
    // silence at 48 kHz HQ on against the same count of internal samples
    // driven, compared bit for bit -- both the timer and the exponential.
    {
        constexpr int hostSamples = 96000;
        YouKnow106Engine rendered;
        rendered.prepare(48000.0, blockSize, true);
        rendered.setParameters(fixtureParameters());
        renderExact(rendered, hostSamples);

        YouKnow106Engine driven;
        driven.prepare(48000.0, blockSize, true);
        driven.setParameters(fixtureParameters());
        const long long internalSamples = static_cast<long long>(
            std::llround(hostSamples
                         * (YouKnow106TestAccess::internalRate(driven)
                            / 48000.0)));
        YouKnow106TestAccess::advanceThermalWarmup(driven, internalSamples);

        expect(YouKnow106TestAccess::thermalWarmupSeconds(rendered)
                   == YouKnow106TestAccess::thermalWarmupSeconds(driven),
               "the driven warm-up clock is not the one the render loop "
               "advances (rendered "
                   + std::to_string(
                         YouKnow106TestAccess::thermalWarmupSeconds(rendered))
                   + " s, driven "
                   + std::to_string(
                         YouKnow106TestAccess::thermalWarmupSeconds(driven))
                   + " s)");
        expect(YouKnow106TestAccess::thermalWarmupFraction(rendered)
                   == YouKnow106TestAccess::thermalWarmupFraction(driven),
               "the driven warm-up fraction is not the one the render loop "
               "computes");
    }
}

void testEnvelopeAndGateModes()
{
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);

    auto parameters = plainPatch();
    parameters.attack = 0.0f;
    parameters.decay = 0.3f;
    parameters.sustain = 0.0f;
    parameters.release = 0.0f;
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);
    const auto decaying = render(engine, static_cast<int>(sampleRate * 2));

    // The generator's falling segment is exponential and the amplifier
    // quasi-linear, so equal slices of time must fall by roughly equal
    // numbers of decibels while the segment runs through the amplifier's
    // linear region.
    const auto levelAt = [&](double seconds) {
        const auto start = static_cast<std::size_t>(seconds * sampleRate);
        return 20.0 * std::log10(peakOf({ decaying.left.begin() + static_cast<long>(start),
                                          decaying.left.begin()
                                              + static_cast<long>(start + 1200) }, 0)
                                 + 1.0e-12);
    };
    const double first = levelAt(0.05) - levelAt(0.15);
    const double second = levelAt(0.15) - levelAt(0.25);
    expect(first > 3.0, "the decay segment is not falling");
    expectNear(second / first, 1.0, 0.45,
               "amplitude decay is not close to constant decibels per second");

    // Gate mode ignores the generator's shape entirely.
    YouKnow106Engine gated;
    gated.prepare(sampleRate, blockSize, true);
    parameters.vcaMode = VcaMode::Gate;
    gated.setParameters(parameters);
    gated.noteOn(60, 1.0f);
    const auto held = render(gated, static_cast<int>(sampleRate));
    expect(peakOf(held.left, held.left.size() / 2) > 0.05,
           "gate mode falls silent while the key is held");
}

void testChorusWidthAndSilence()
{
    constexpr double sampleRate = 48000.0;

    const auto sideEnergy = [&](ChorusMode mode) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.chorus = mode;
        parameters.chorusNoise = 0.0f;
        engine.setParameters(parameters);
        engine.noteOn(52, 1.0f);
        const auto rendered = render(engine, static_cast<int>(sampleRate * 2));

        double side = 0.0;
        double mid = 0.0;
        for (std::size_t index = rendered.left.size() / 2; index < rendered.left.size();
             ++index)
        {
            const double difference = rendered.left[index] - rendered.right[index];
            const double sum = rendered.left[index] + rendered.right[index];
            side += difference * difference;
            mid += sum * sum;
        }
        return std::sqrt(side / std::max(mid, 1.0e-12));
    };

    expect(sideEnergy(ChorusMode::Off) < 1.0e-6,
           "the output is not mono with the effect switched out");
    expect(sideEnergy(ChorusMode::One) > 0.05, "the first mode produces no width");
    expect(sideEnergy(ChorusMode::Two) > 0.05, "the second mode produces no width");
}

void testGlideKeepsTheRampContinuous()
{
    // A glide steps the timer count once per converter pass while the
    // compensation CV slews behind it. The ramp renders that error as the
    // slope of each cycle, frozen at the wrap where both cycles share the
    // bottom rail -- so gliding must not add value discontinuities beyond
    // the authentic pitch staircase. The outer multiply this replaces
    // stepped the finished waveform mid-cycle at the scan cadence and
    // measured about four times the static second-difference floor.
    constexpr double sampleRate = 48000.0;
    const auto maxSecondDifference = [](const std::vector<float>& x) {
        double worst = 0.0;
        for (std::size_t n = 2; n < x.size(); ++n)
            worst = std::max(worst,
                             std::abs(static_cast<double>(x[n])
                                      - 2.0 * static_cast<double>(x[n - 1])
                                      + static_cast<double>(x[n - 2])));
        return worst;
    };
    const auto rampFloor = [&](bool glide) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.keyMode = KeyMode::Unison;
        parameters.portamento = 0.30f;
        engine.setParameters(parameters);
        engine.noteOn(48, 1.0f);
        renderExact(engine, 4800);
        if (glide)
            engine.noteOn(60, 1.0f);
        const auto rendered = renderExact(engine, 9600);
        return maxSecondDifference(rendered.left);
    };
    const double still = rampFloor(false);
    const double gliding = rampFloor(true);
    expect(still > 0.0 && gliding < 2.0 * still,
           "gliding steps the rendered ramp beyond the pitch staircase");
}

void testChorusSweepTrajectoryDefault()
{
    // The linear-in-delay trajectory ships: the one delay-trajectory
    // measurement on record (KR-106's click-timing series, 16 us RMS residual
    // against a straight line) reads the 106's sweep as linear in time, so
    // the frequency-linear hypothesis waits behind the switch instead of
    // shipping as the default.
    EngineParameters defaults;
    expect(!defaults.enableChorusHyperbolicSweep,
           "the frequency-linear sweep hypothesis became the default again");

    // The retained hypothesis has to stay alive behind the switch, and its
    // blend has to answer to Unit Character: at full character the two laws
    // render different mid-flank trajectories, and at zero character the
    // switch must do nothing at all.
    constexpr float sampleRate = 48000.0f;
    const auto render = [&](bool hyperbolic, float calibration) {
        Chorus chorus;
        chorus.prepare(sampleRate);
        double sum = 0.0;
        for (int index = 0; index < 48000; ++index)
        {
            const float input = std::sin(
                2.0f * 3.14159265f * 1000.0f * static_cast<float>(index)
                / sampleRate);
            float left = 0.0f;
            float right = 0.0f;
            chorus.process(input, ChorusMode::One, 0.0f, left, right,
                           false, hyperbolic, calibration);
            sum += std::abs(static_cast<double>(left - right));
        }
        return sum;
    };
    const double linear = render(false, 1.0f);
    const double bent = render(true, 1.0f);
    expect(std::abs(linear - bent) > 1.0e-3,
           "the retained hyperbolic path no longer changes the trajectory");
    const double bentAtZero = render(true, 0.0f);
    expectNear(bentAtZero, render(false, 0.0f), 1.0e-9,
               "the hyperbolic switch acted at zero Unit Character");
}

void testChorusRateNoiseReproducesTheMeasuredModeDelta()
{
    // The chorus noise floor of a real 106 sits 3.95 dB higher in mode II than
    // in mode I, measured on Panasonic parts and again on Xvive parts
    // (OQ-03). The settled topology gives both modes the same sweep depth and
    // the same clock range, so the only thing the mode line changes is the
    // modulation rate -- and this instrument's own timing network puts that
    // ratio at 1.6234799, which is 4.21 dB. The candidate mechanism is
    // therefore noise proportional to that rate.
    //
    // It ships off, because a prediction that lands 0.26 dB from a measurement
    // is a lead and not evidence. What this fence holds is that the switch
    // does what it claims in both positions: silent when off, and exactly the
    // predicted delta when on.
    constexpr double sampleRate = 48000.0;

    // Each line writes one noise sample per bucket edge, so its instantaneous
    // floor rides the swept clock and the measurement has to cover a whole
    // number of modulation cycles or the two modes are compared over different
    // parts of their own sweeps. Measured over a fixed window instead, mode II
    // reads 0.69 dB hot before anything is switched on at all.
    const auto idleFloorDb = [&](ChorusMode mode, bool rateNoise) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.chorus = mode;
        parameters.enableChorusRateNoise = rateNoise;
        engine.setParameters(parameters);
        // Past the wet-mute glide and the support filters' own settling.
        render(engine, static_cast<int>(sampleRate * 0.5));

        constexpr int cycles = 6;
        const double period = 1.0 / Chorus::settingsFor(mode).rateHz;
        const auto rendered = render(
            engine, static_cast<int>(sampleRate * period * cycles + 0.5));
        double energy = 0.0;
        for (float sample : rendered.left)
            energy += static_cast<double>(sample) * sample;
        return 10.0 * std::log10(
            energy / static_cast<double>(rendered.left.size()) + 1.0e-30);
    };

    const double offOne = idleFloorDb(ChorusMode::One, false);
    const double offTwo = idleFloorDb(ChorusMode::Two, false);
    const double onOne = idleFloorDb(ChorusMode::One, true);
    const double onTwo = idleFloorDb(ChorusMode::Two, true);

    expect(std::abs(offTwo - offOne) < 0.10,
           "the modes already differ by " + std::to_string(offTwo - offOne)
               + " dB with the rate-noise candidate switched out");
    // Mode I is the reference leg, so engaging the candidate must leave it
    // exactly where the shipped default has it.
    expect(std::abs(onOne - offOne) < 1.0e-6,
           "engaging the rate-noise candidate moved mode I's own floor by "
               + std::to_string(onOne - offOne) + " dB");

    const double predicted =
        20.0 * std::log10(Chorus::modeRateRatio());
    expect(std::abs(predicted - 4.21) < 0.01,
           "the mode-rate ratio no longer predicts the recorded 4.21 dB");
    expect(std::abs((onTwo - onOne) - predicted) < 0.10,
           "the rate-noise candidate raises mode II by "
               + std::to_string(onTwo - onOne) + " dB, not the predicted "
               + std::to_string(predicted));
}

void testChorusNoiseIsPresentAndDefeatable()
{
    constexpr double sampleRate = 48000.0;

    const auto idleNoise = [&](float scale) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.chorus = ChorusMode::Two;
        parameters.chorusNoise = scale;
        engine.setParameters(parameters);
        const auto rendered = render(engine, static_cast<int>(sampleRate));
        double energy = 0.0;
        for (std::size_t index = rendered.left.size() / 2; index < rendered.left.size();
             ++index)
            energy += static_cast<double>(rendered.left[index]) * rendered.left[index];
        return 10.0 * std::log10(energy / (rendered.left.size() / 2) + 1.0e-30);
    };

    const double modelled = idleNoise(1.0f);
    expect(modelled > -85.0 && modelled < -55.0,
           "the voiced delay-line noise floor escaped its regression guard band");
    expect(idleNoise(0.0f) < -120.0,
           "the delay lines still hiss with their noise switched out");
}

void testIdleOutputFloorCarriesTheMn3009NoiseRow()
{
    // What the line-noise constant is worth at the jacks. The circuit suite
    // fences the recovered wet line against the MN3009's 0.2 mVrms row; this
    // one fences the number a listener meets, and ties the two together so
    // neither can be satisfied on its own.
    //
    // The fixture is pinned rather than derived from plainPatch(), because the
    // floor scales with two panel controls and with nothing else: VOLUME 0.80,
    // VCA LEVEL 0.80, CHORUS NOISE 1.0, Unit Character 1.0, no note ever
    // played, HQ on at a 48 kHz host rate so the chorus runs at its 192 kHz
    // internal rate. RMS of both channels over 4 s after a 2 s settle.
    constexpr double sampleRate = 48000.0;

    const auto idleFloorDbfs = [&](ChorusMode mode) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        EngineParameters parameters;
        parameters.volume = 0.80f;
        parameters.vcaLevel = 0.80f;
        parameters.chorusNoise = 1.0f;
        parameters.calibration = 1.0f;
        parameters.chorus = mode;
        engine.setParameters(parameters);
        render(engine, static_cast<int>(sampleRate * 2.0));
        const auto rendered = render(engine, static_cast<int>(sampleRate * 4.0));
        double energy = 0.0;
        for (std::size_t index = 0; index < rendered.left.size(); ++index)
            energy += static_cast<double>(rendered.left[index]) * rendered.left[index]
                    + static_cast<double>(rendered.right[index]) * rendered.right[index];
        const double meanSquare =
            energy / static_cast<double>(rendered.left.size() * 2);
        return 10.0 * std::log10(meanSquare + 1.0e-40);
    };

    // With the chorus switched out this fixture renders bit-exact zero, so the
    // BBD line noise is the only thing in the floor below and the figure is
    // fully determined by `independentLineRandomAmplitude`.
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        EngineParameters parameters;
        parameters.volume = 0.80f;
        parameters.vcaLevel = 0.80f;
        parameters.chorusNoise = 1.0f;
        parameters.calibration = 1.0f;
        parameters.chorus = ChorusMode::Off;
        engine.setParameters(parameters);
        const auto rendered = render(engine, static_cast<int>(sampleRate * 2.0));
        double peak = 0.0;
        for (std::size_t index = 0; index < rendered.left.size(); ++index)
            peak = std::max({ peak,
                              static_cast<double>(std::abs(rendered.left[index])),
                              static_cast<double>(std::abs(rendered.right[index])) });
        expect(peak == 0.0,
               "the idle fixture is no longer silent with the chorus switched "
               "out, so its floor is not the BBD line noise alone (peak "
                   + std::to_string(peak) + ")");
    }

    const double floorOne = idleFloorDbfs(ChorusMode::One);
    const double floorTwo = idleFloorDbfs(ChorusMode::Two);
    expectNear(floorOne, -77.85, 0.5,
               "the idle output floor left the MN3009 noise row");

    // One target, not two. `settingsFor` gives modes I and II the same wet gain
    // and the same sweep -- only the rate differs, and the rate-proportional
    // noise candidate is off by default -- so a floor that differed by mode
    // would mean something other than the line noise had moved.
    expectNear(floorTwo, floorOne, 0.1,
               "chorus I and II no longer render the same idle floor");

    // The two measurements have to move together. Both are compared against
    // what this engine rendered before the constant was derived, which is a
    // recorded number and not a reference render: once the constant moves
    // there is no pre-change engine left to render against. If a gain were
    // moved somewhere in the output path instead of the line noise, the floor
    // would land and the wet line would not.
    constexpr double floorBeforeDbfs = -63.4409;
    constexpr double wetLineBeforeVrms = 1.0611e-3;

    constexpr double lineRate = 192000.0;
    Chorus chorus;
    chorus.prepare(lineRate);
    const double recover = static_cast<double>(Chorus::nodeVoltsPerUnit)
        / (static_cast<double>(Chorus::dryMixGain)
           * static_cast<double>(Chorus::wetToDryGain));
    for (int index = 0; index < static_cast<int>(lineRate * 0.5); ++index)
    {
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(0.0f, ChorusMode::One, 1.0f, left, right);
    }
    double lineEnergy = 0.0;
    const int lineWindow = static_cast<int>(lineRate * 2.0);
    for (int index = 0; index < lineWindow; ++index)
    {
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(0.0f, ChorusMode::One, 1.0f, left, right);
        const double a = static_cast<double>(left) * recover;
        const double b = static_cast<double>(right) * recover;
        lineEnergy += a * a + b * b;
    }
    // Broadband rather than A-weighted: the circuit suite owns the weighted
    // figure, and the whole path from the injection point is linear, so the
    // two read the same delta to the digit.
    const double wetLineNow =
        std::sqrt(lineEnergy / static_cast<double>(lineWindow * 2));

    const double floorDelta = floorOne - floorBeforeDbfs;
    const double lineDelta = 20.0 * std::log10(wetLineNow / wetLineBeforeVrms);
    expectNear(floorDelta, lineDelta, 0.5,
               "the idle floor moved " + std::to_string(floorDelta)
                   + " dB while the wet line moved " + std::to_string(lineDelta)
                   + " dB, so something other than the BBD line noise moved");
}

void testMainNoiseDensityIsProcessingRateInvariant()
{
    // The hardware noise source has a continuous-time spectral density. A
    // discrete white source must therefore grow by sqrt(internal sample rate),
    // or HQ mode's decimators make the same patch several dB quieter simply by
    // running the generator more often. Keep the filter well below every host
    // Nyquist limit so this measures generator density rather than legitimately
    // different captured analogue bandwidths.
    const auto levelAt = [](double sampleRate, bool oversampled) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, oversampled);
        auto parameters = plainPatch();
        parameters.sawEnabled = false;
        parameters.pulseEnabled = false;
        parameters.subLevel = 0.0f;
        parameters.noiseLevel = 1.0f;
        parameters.cutoff = YouKnow106Engine::panelPositionForCutoff(2000.0f);
        parameters.vcaMode = VcaMode::Gate;
        engine.setParameters(parameters);
        engine.noteOn(60, 1.0f);

        const auto rendered = renderExact(
            engine, static_cast<int>(sampleRate * 0.5));
        const std::size_t from = rendered.left.size() / 2;
        double energy = 0.0;
        for (std::size_t index = from; index < rendered.left.size(); ++index)
        {
            const double left = rendered.left[index];
            const double right = rendered.right[index];
            energy += left * left + right * right;
        }
        return std::sqrt(
            energy / static_cast<double>(2 * (rendered.left.size() - from)));
    };

    const double reference = levelAt(48000.0, true);
    expect(std::isfinite(reference) && reference > 1.0e-5,
           "the main-noise rate fixture produced no measurable noise");
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (const bool oversampled : { false, true })
        {
            const double measured = levelAt(sampleRate, oversampled);
            const double relativeDb =
                20.0 * std::log10((measured + 1.0e-30) / reference);
            expectNear(relativeDb, 0.0, 0.5,
                       "main-noise RMS moves with sample rate/HQ at "
                           + std::to_string(static_cast<int>(sampleRate))
                           + " Hz, HQ " + (oversampled ? "on" : "off"));
        }
}

void testSampleRateAndOversamplingConsistency()
{
    const auto levelAt = [](double sampleRate, bool oversampled) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, oversampled);
        auto parameters = plainPatch();
        parameters.cutoff = 0.7f;
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);
        const auto rendered = render(engine, static_cast<int>(sampleRate));
        double energy = 0.0;
        const auto from = rendered.left.size() / 2;
        for (std::size_t index = from; index < rendered.left.size(); ++index)
            energy += static_cast<double>(rendered.left[index]) * rendered.left[index];
        return 10.0 * std::log10(energy / (rendered.left.size() - from) + 1.0e-30);
    };

    const double reference = levelAt(48000.0, true);
    for (double sampleRate : { 44100.0, 88200.0, 96000.0, 192000.0 })
        expectNear(levelAt(sampleRate, true), reference, 1.0,
                   "output level moves with the host sample rate");
    expectNear(levelAt(48000.0, false), reference, 1.5,
               "output level moves when oversampling is switched off");

    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    expect(engine.getOversamplingFactor() == 4, "48 kHz does not run oversampled");
    const int reported = engine.getProcessingLatencySamples();
    expect(reported > 0, "an oversampled engine reports no latency");

    // The reported figure has to stay put across every configuration: the
    // quality setting can move while the host is playing, and a plug-in that
    // renegotiated its latency mid-transport would make the host re-align
    // everything around it.
    engine.prepare(192000.0, blockSize, true);
    expect(engine.getOversamplingFactor() == 1,
           "a high-rate host is oversampled unnecessarily");
    expect(engine.getProcessingLatencySamples() == reported,
           "the reported latency moved with the oversampling factor");
    engine.prepare(48000.0, blockSize, false);
    expect(engine.getProcessingLatencySamples() == reported,
           "the reported latency moved when oversampling was switched off");

    // And the shallower configurations must actually be padded out to it,
    // otherwise the constant figure would be a lie the host acts on.
    const auto onsetOffset = [](bool oversampled) {
        YouKnow106Engine local;
        local.prepare(48000.0, blockSize, oversampled);
        auto parameters = plainPatch();
        parameters.attack = 0.0f;
        local.setParameters(parameters);
        local.noteOn(60, 1.0f);
        const auto rendered = render(local, 4096);
        for (std::size_t index = 0; index < rendered.left.size(); ++index)
            if (std::abs(rendered.left[index]) > 1.0e-4f)
                return static_cast<int>(index);
        return -1;
    };
    const int deep = onsetOffset(true);
    const int shallow = onsetOffset(false);
    expect(deep >= 0 && shallow >= 0, "a configuration produced no onset at all");
    // The host-compensated numerical paths agree within four samples. The
    // remaining threshold crossing includes the nonlinear analogue filter's
    // own signal-dependent onset and is not reported as plug-in latency.
    expect(std::abs(deep - shallow) <= 4,
           "the two configurations do not share an onset (deep "
               + std::to_string(deep) + ", shallow "
               + std::to_string(shallow) + "), so the padding is wrong");
}

void testResonanceDoesNotMoveTheRenderedCorner()
{
    // The circuit suite checks the control law; this checks the coefficient
    // the render actually consumes, so a correction fixed in the pure
    // function and never wired into audio still fails. `voice.filterG` is the
    // integrator coefficient the cascade runs on, memoised behind an exact
    // equality on its own inputs, and the corner it stands for is
    // atan(g) * internal rate / pi.
    //
    // Every source is off, the envelope is out of the cutoff path and Unit
    // Character is zero, so nothing but the frequency correction couples
    // RESONANCE to that coefficient at a fixed CUTOFF. Below the oscillation
    // threshold the cascade carries no limit cycle, so there is no droop to
    // correct and the five settings must coincide. They spread +0.00 / +8.70
    // / +32.91 / +80.42 / +117.53 cents at converter code 6272 before this
    // was derived rather than fitted.
    constexpr double sampleRate = 48000.0;
    const auto corner = [&](float cutoffPanel, float resonance) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.sawEnabled = false;
        parameters.pulseEnabled = false;
        parameters.subLevel = 0.0f;
        parameters.noiseLevel = 0.0f;
        parameters.envDepth = 0.0f;
        parameters.keyFollow = 0.0f;
        parameters.calibration = 0.0f;
        parameters.cutoff = cutoffPanel;
        parameters.resonance = resonance;
        engine.setParameters(parameters);
        engine.noteOn(60, 1.0f);
        render(engine, static_cast<int>(sampleRate * 0.25));
        const double internalRate =
            sampleRate * static_cast<double>(engine.getOversamplingFactor());
        return std::atan(static_cast<double>(
                   YouKnow106TestAccess::filterG(engine, 0)))
             * internalRate / pi;
    };

    struct Code { const char* name; float panel; double hertz; };
    // Panel byte times 128 is the converter code, so these are bytes 30, 49
    // and 90 -- the service code among them.
    for (const auto& code : { Code { "3840", 30.0f / 127.0f, 56.76 },
                              Code { "6272", 49.0f / 127.0f, 248.05 },
                              Code { "11520", 90.0f / 127.0f, 5918.5 } })
    {
        const double reference = corner(code.panel, 0.0f);
        expectNear(reference, code.hertz, code.hertz * 0.002,
                   std::string("the rendered corner at converter code ")
                       + code.name + " is not the one the control law asks for");
        for (const float resonance : { 0.00f, 0.30f, 0.50f, 0.70f, 0.80f })
            expectNear(1200.0 * std::log2(corner(code.panel, resonance)
                                          / reference),
                       0.0, 10.0,
                       std::string("resonance ") + std::to_string(resonance)
                           + " moves the rendered corner at converter code "
                           + code.name);
    }
}

void testVelocityScalesTheEnvelopeIntoTheFilter()
{
    // Velocity is an extension -- the modelled hardware has no velocity input
    // and `velocityDepth` is 0 by default -- but when a player turns it up it
    // has to do what a dynamics control does. It scales the ENV amount into
    // the VCF, the only path this instrument has from the envelope to cutoff,
    // by the same gain the amplifier already applies, so a quieter note is a
    // note whose filter envelope opened less far.
    //
    // The envelope is pinned along with the panel, because the probe is at
    // t = 0.3 s: a decaying envelope would put the three velocities at three
    // points of one decay rather than at three depths. CUTOFF 0.30 with ENV
    // 0.30 keeps the realised corner inside the audio band at every velocity;
    // at the 0.50/0.40 pair this fixture was first written for, velocity 1.0
    // lands the corner at 32.8 kHz and the audible spectrum saturates.
    constexpr double sampleRate = 48000.0;
    constexpr int probeAt = 14400;   // t = 0.3 s
    constexpr int fftSize = 32768;

    struct Take
    {
        double highBandDb { 0.0 };
        double cornerHz { 0.0 };
    };

    const auto fixture = [] {
        auto parameters = plainPatch();
        parameters.cutoff = 0.30f;
        parameters.resonance = 0.30f;
        parameters.envDepth = 0.30f;
        parameters.attack = 0.0f;
        parameters.decay = 1.0f;
        parameters.sustain = 1.0f;
        parameters.release = 0.0f;
        parameters.vcaMode = VcaMode::Envelope;
        parameters.calibration = 0.0f;
        return parameters;
    };

    const auto measure = [&](float velocity, float velocityDepth) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = fixture();
        parameters.velocityDepth = velocityDepth;
        engine.setParameters(parameters);
        engine.reset();
        engine.setParameters(parameters);
        engine.noteOn(48, velocity);

        std::vector<float> left(probeAt + fftSize, 0.0f);
        std::vector<float> right(probeAt + fftSize, 0.0f);
        Take take;
        bool probed = false;
        for (int offset = 0; offset < probeAt + fftSize; offset += blockSize)
        {
            const int count =
                std::min(blockSize, probeAt + fftSize - offset);
            engine.process(left.data() + offset, right.data() + offset, count);
            if (!probed && offset + count >= probeAt)
            {
                // The coefficient the render actually consumes, not a
                // restatement of the control law: a routing fixed in the
                // target computation and never reaching the cascade fails
                // here.
                take.cornerHz =
                    std::atan(static_cast<double>(
                        YouKnow106TestAccess::filterG(engine, 0)))
                    * sampleRate
                    * static_cast<double>(engine.getOversamplingFactor()) / pi;
                probed = true;
            }
        }

        // A 32768-point Blackman-Harris window from t = 0.3 s, evaluated on a
        // 10 Hz grid from 20 Hz to 8 kHz, and the fraction of that energy at
        // or above 1 kHz. Stating the estimator matters: the same render
        // reads a different number on a different band or grid.
        std::vector<double> windowed(fftSize);
        for (int n = 0; n < fftSize; ++n)
        {
            const double phase = 2.0 * pi * static_cast<double>(n)
                               / static_cast<double>(fftSize - 1);
            windowed[static_cast<std::size_t>(n)] =
                (0.35875 - 0.48829 * std::cos(phase)
                 + 0.14128 * std::cos(2.0 * phase)
                 - 0.01168 * std::cos(3.0 * phase))
                * static_cast<double>(left[static_cast<std::size_t>(probeAt + n)]);
        }
        double totalEnergy = 0.0;
        double highEnergy = 0.0;
        for (double hertz = 20.0; hertz <= 8000.5; hertz += 10.0)
        {
            const double omega = 2.0 * pi * hertz / sampleRate;
            const std::complex<double> step { std::cos(-omega),
                                              std::sin(-omega) };
            std::complex<double> turn { 1.0, 0.0 };
            std::complex<double> sum { 0.0, 0.0 };
            for (int n = 0; n < fftSize; ++n)
            {
                sum += windowed[static_cast<std::size_t>(n)] * turn;
                turn *= step;
            }
            const double energy = std::norm(sum);
            totalEnergy += energy;
            if (hertz >= 1000.0)
                highEnergy += energy;
        }
        expect(totalEnergy > 0.0,
               "the velocity fixture rendered no energy at all at velocity "
                   + std::to_string(velocity));
        take.highBandDb =
            10.0 * std::log10(highEnergy / totalEnergy + 1.0e-30);
        return take;
    };

    // (1) The audible one. Before the routing the three are identical to
    // 0.0001 dB -- a span of 0.00 dB, the whole of gap 5 -- because velocity
    // reached the amplifier and nothing else. With it they read
    // -83.62 / -54.31 / -16.33 dB.
    // (2) The seam one. Before, all three sit at 1985.03 Hz; after,
    // 189.97 / 458.19 / 1985.03 Hz, a span of 4062 cents.
    const Take soft = measure(0.2f, 1.0f);
    const Take middle = measure(0.5f, 1.0f);
    const Take hard = measure(1.0f, 1.0f);

    expect(soft.highBandDb < middle.highBandDb
               && middle.highBandDb < hard.highBandDb,
           "the high-band energy fraction is not monotone in velocity ("
               + std::to_string(soft.highBandDb) + " / "
               + std::to_string(middle.highBandDb) + " / "
               + std::to_string(hard.highBandDb) + " dB)");
    expect(hard.highBandDb - soft.highBandDb >= 30.0,
           "velocity moves the spectrum by only "
               + std::to_string(hard.highBandDb - soft.highBandDb)
               + " dB of high-band energy fraction, so the dynamics control "
                 "is still very nearly a pure gain");

    expect(soft.cornerHz < middle.cornerHz && middle.cornerHz < hard.cornerHz,
           "the rendered corner is not monotone in velocity ("
               + std::to_string(soft.cornerHz) + " / "
               + std::to_string(middle.cornerHz) + " / "
               + std::to_string(hard.cornerHz) + " Hz)");
    const double cents = 1200.0 * std::log2(hard.cornerHz / soft.cornerHz);
    expect(cents >= 3000.0,
           "velocity moves the corner the cascade runs on by only "
               + std::to_string(cents) + " cents");

    // (3) The faithfulness one, and the reason this can be an extension at
    // all. There is no pre-change render lock in these suites to compare
    // against, and a hard-coded hash of a float render would be a constant
    // about this machine's libm rather than about the engine. The property
    // that reference would have proved is proved directly instead, from the
    // two exact identities `velocityGain` has: it is exactly 1.0f when
    // `velocityDepth` is 0, whatever the velocity, and exactly 1.0f at
    // velocity 1.0, whatever the depth. So the faithful default cannot hear
    // velocity, and the loudest note cannot hear the extension -- both bit
    // for bit, on a patch that runs every source through the filter, the
    // amplifier and the chorus.
    const auto renderFor = [&](float velocity, float velocityDepth) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = fixture();
        parameters.pulseEnabled = true;
        parameters.subLevel = 0.3f;
        parameters.noiseLevel = 0.2f;
        parameters.keyFollow = 0.5f;
        parameters.chorus = ChorusMode::One;
        parameters.calibration = 1.0f;
        parameters.velocityDepth = velocityDepth;
        engine.setParameters(parameters);
        engine.reset();
        engine.setParameters(parameters);
        engine.noteOn(48, velocity);
        return renderExact(engine, static_cast<int>(sampleRate * 0.5));
    };

    const Render faithful = renderFor(1.0f, 0.0f);
    expect(peakOf(faithful.left, 0) > 0.0,
           "the velocity faithfulness patch rendered silence");
    for (const float velocity : { 0.2f, 0.5f })
    {
        const Render quiet = renderFor(velocity, 0.0f);
        expect(maximumDifference(faithful.left, quiet.left) == 0.0
                   && maximumDifference(faithful.right, quiet.right) == 0.0,
               "velocityDepth 0.0 is no longer the faithful default: velocity "
                   + std::to_string(velocity)
                   + " changed the render the hardware would have made");
    }
    const Render extended = renderFor(1.0f, 1.0f);
    expect(maximumDifference(faithful.left, extended.left) == 0.0
               && maximumDifference(faithful.right, extended.right) == 0.0,
           "the velocity extension moved a full-velocity note, where its own "
           "gain is exactly one");
}

void testSelfOscillationMatchesTheServiceTrim()
{
    // Two Roland ADJUSTMENT steps, both taken on the same card in the same
    // state, and until now only one of them was ever checked.
    //
    //   VCF FREQUENCY   BANK 3, hold C4   248 Hz (B3)
    //   VCF RESONANCE   TP19...TP14, BANK 3, hold C4   4.8 Vp-p sine
    //   VCF WIDTH       BANK 3, hold C6   992 Hz (B5)
    //
    // The suite checked the frequency and never the amplitude, and the model
    // was 4.1 dB under it -- 2.99 Vp-p where the procedure trims to 4.8. The
    // two are coupled: the limit cycle grows with loop gain, and the stage
    // tanh's compression at that larger amplitude pulls the oscillation flat,
    // so nothing but satisfying both at once fixes either.
    constexpr double sampleRate = 48000.0;
    struct Take { double peakToPeak; double hertz; };
    const auto oscillate = [&](int note, float keyFollow) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        // A *self*-oscillation trim: nothing may reach the filter but the
        // card's own excitation.
        parameters.sawEnabled = false;
        parameters.pulseEnabled = false;
        parameters.subLevel = 0.0f;
        parameters.noiseLevel = 0.0f;
        parameters.resonance = 1.0f;
        parameters.keyFollow = keyFollow;
        parameters.cutoff = 49.0f / 127.0f; // converter code 6272
        engine.setParameters(parameters);
        engine.noteOn(note, 1.0f);
        // Kick the cascade rather than waiting seconds for card noise to build
        // the limit cycle; where it settles is what is being measured.
        YouKnow106TestAccess::setFilterState(engine, 0, { 1.0f, 1.0f, 1.0f, 1.0f });
        render(engine, static_cast<int>(sampleRate * 1.5));

        float minimum = 1.0e30f;
        float maximum = -1.0e30f;
        std::vector<float> trace;
        float left = 0.0f;
        float right = 0.0f;
        const int window = static_cast<int>(sampleRate / 4);
        trace.reserve(static_cast<std::size_t>(window));
        for (int index = 0; index < window; ++index)
        {
            engine.process(&left, &right, 1);
            const float volts = YouKnow106TestAccess::filterOutputVolts(engine, 0);
            minimum = std::min(minimum, volts);
            maximum = std::max(maximum, volts);
            trace.push_back(volts);
        }

        double mean = 0.0;
        for (const float volts : trace)
            mean += volts;
        mean /= static_cast<double>(trace.size());
        std::size_t first = 0;
        std::size_t last = 0;
        int crossings = 0;
        for (std::size_t index = 1; index < trace.size(); ++index)
            if (trace[index - 1] <= mean && trace[index] > mean)
            {
                if (crossings++ == 0)
                    first = index;
                last = index;
            }
        const double hertz = crossings > 1
            ? (crossings - 1) * sampleRate / static_cast<double>(last - first)
            : 0.0;
        return Take { static_cast<double>(maximum - minimum), hertz };
    };

    const auto atC4 = oscillate(60, 0.0f);
    // Ten per cent: the procedure publishes no tolerance, and the trimmer it
    // describes is set by ear against a scope, so this is a fidelity bound
    // rather than a claim that a card lands on 4.8 to the millivolt.
    expect(std::abs(atC4.peakToPeak - 4.8) < 0.48,
           "self-oscillation is " + std::to_string(atC4.peakToPeak)
               + " Vp-p, not the service procedure's 4.8");
    expect(std::abs(atC4.hertz - 248.0) < 4.0,
           "self-oscillation is at " + std::to_string(atC4.hertz)
               + " Hz, not the service procedure's 248");

    // VCF WIDTH: two octaves of keyboard is two octaves of cutoff, so C6 must
    // oscillate at exactly four times C4. This is what makes full key tracking
    // 1.00 rather than approximately so, and it is a second, independent point
    // on the counts-per-octave slope.
    const auto tracked4 = oscillate(60, 1.0f);
    const auto tracked6 = oscillate(84, 1.0f);
    const double octaves = std::log2(tracked6.hertz
                                     / std::max(tracked4.hertz, 1.0));
    expect(std::abs(octaves - 2.0) < 0.02,
           "full key tracking moves the filter " + std::to_string(octaves)
               + " octaves across two octaves of keyboard, not 2.00");
}

void testVcfStageOffsetsBelongToUnitCharacter()
{
    // A resonant sweep is where four asymmetric stages show up most, so drive
    // the mechanism rather than merely holding a note through it. The other
    // three circuit-shape toggles default true but are held off here so this
    // test isolates stage offsets alone -- combined with the Early effect's
    // own voltage-dependent transconductance, the spread-vs-Unit-Character
    // curve stops being monotonic enough for the loose bound below.
    const auto run = [](float calibration, bool enabled) {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, true);
        auto parameters = plainPatch();
        parameters.calibration = calibration;
        parameters.enableVcfStageOffsets = enabled;
        parameters.enableVcfEarlyEffect = false;
        parameters.enableSpatialThermalGradient = false;
        parameters.resonance = 0.85f;
        parameters.cutoff = 0.45f;
        parameters.envDepth = 0.70f;
        engine.setParameters(parameters);
        for (const int note : { 40, 47, 52, 55, 59, 64 })
            engine.noteOn(note, 1.0f);
        return render(engine, 24000);
    };

    const auto largestDifference = [](const Render& a, const Render& b) {
        double worst = 0.0;
        for (std::size_t n = 0; n < a.left.size(); ++n)
            worst = std::max(worst, std::abs(static_cast<double>(a.left[n])
                                             - static_cast<double>(b.left[n])));
        return worst;
    };

    // The contract: Unit Character zero is the calibrated nominal model, whose
    // inter-voice spread is zero. An optional card-spread mechanism has to be
    // absent there, not merely small -- so this is an equality, not a bound.
    const auto nominalOn = run(0.0f, true);
    const auto nominalOff = run(0.0f, false);
    expect(nominalOn.left == nominalOff.left && nominalOn.right == nominalOff.right,
           "IR3109 stage offsets still colour the calibrated nominal model at "
           "Unit Character zero");

    // And it must still be a live mechanism above zero, or the test above
    // would pass just as happily on a mechanism that had been deleted.
    const auto fullOn = run(1.0f, true);
    const auto fullOff = run(1.0f, false);
    expect(!(fullOn.left == fullOff.left),
           "IR3109 stage offsets do nothing at full Unit Character");

    // Half the character, somewhere between none and all of the asymmetry.
    // The cascade is nonlinear, so the bounds are deliberately loose.
    //
    // Each comparison is on-against-off at *one* Unit Character setting, which
    // is what isolates this mechanism. Measuring each setting against the
    // nominal render instead conflates every other control the same knob
    // scales -- the trimmer residuals, the drift, the converter's carry error
    // -- and on a resonant patch two of those renders decorrelate long before
    // the mechanism under test has said anything, so the ratio saturates at
    // one and the bound below stops meaning what it says.
    const auto full = largestDifference(fullOn, fullOff);
    const auto half = largestDifference(run(0.5f, true), run(0.5f, false));
    expect(full > 0.0 && half > 0.2 * full && half < 0.8 * full,
           "stage-offset spread does not scale with Unit Character");
}

void testVcfStageOffsetsAreLiveBeforeTheFirstSample()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.calibration = 0.8f;
    engine.setParameters(parameters);

    // Hoisting the write out of the audio path is only correct if every entry
    // point leaves the offsets already in place. Check before rendering
    // anything, and check every slot -- including the extension slots above
    // the hardware's six, which never take a converter turn of their own.
    const auto expectSeeded = [&](const char* when) {
        for (int slot = 0; slot < YouKnow106Engine::maxVoices; ++slot)
        {
            for (int stage = 0; stage < 4; ++stage)
            {
                // The card carries the draw in volts at the pair; the filter
                // consumes node-coordinate volts, so the seeded value is the
                // draw referred through the anchored 560/68560 divider.
                // The tolerance is float-epsilon-sized for the referred
                // magnitude (~0.1), five orders under the 122x coordinate
                // error this fixture exists to catch.
                expectNear(YouKnow106TestAccess::stageOffset(engine, slot, stage),
                           YouKnow106TestAccess::cardStageOffset(engine, slot, stage)
                               / (560.0f / (68000.0f + 560.0f)) * 0.8,
                           1.0e-6,
                           std::string("stage offset for slot ")
                               + std::to_string(slot) + " is not seeded " + when);
            }
        }
    };

    expectSeeded("after setParameters");
    engine.reset();
    engine.setParameters(parameters);
    expectSeeded("after reset");
}

void testVcfEarlyEffectBelongsToUnitCharacter()
{
    // A resonant sweep with envelope depth is where a stage's own transconductance
    // modulation under its own instantaneous voltage shows up most.
    const auto run = [](float calibration, bool enabled) {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, true);
        auto parameters = plainPatch();
        parameters.calibration = calibration;
        parameters.enableVcfEarlyEffect = enabled;
        parameters.cutoff = 0.40f;
        parameters.resonance = 0.70f;
        parameters.envDepth = 0.50f;
        parameters.decay = 0.50f;
        engine.setParameters(parameters);
        engine.noteOn(48, 1.0f);
        return render(engine, 24000);
    };

    const auto nominalOn = run(0.0f, true);
    const auto nominalOff = run(0.0f, false);
    expect(nominalOn.left == nominalOff.left && nominalOn.right == nominalOff.right,
           "the IR3109 Early effect still colours the nominal model at Unit "
           "Character zero");

    const auto fullOn = run(1.0f, true);
    const auto fullOff = run(1.0f, false);
    expect(!(fullOn.left == fullOff.left),
           "the IR3109 Early effect does nothing at full Unit Character");
}

void testSpatialThermalGradientBelongsToUnitCharacter()
{
    // A held polyphonic chord spread across every card is where a systematic,
    // rank-ordered per-card cutoff/pitch gradient shows up, rather than a
    // single voice's own tolerance.
    const auto run = [](float calibration, bool enabled) {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, true);
        auto parameters = plainPatch();
        parameters.calibration = calibration;
        parameters.enableSpatialThermalGradient = enabled;
        parameters.cutoff = 0.50f;
        parameters.resonance = 0.60f;
        engine.setParameters(parameters);
        for (const int note : { 48, 55, 60, 64, 67, 72 })
            engine.noteOn(note, 1.0f);
        return render(engine, 24000);
    };

    const auto nominalOn = run(0.0f, true);
    const auto nominalOff = run(0.0f, false);
    expect(nominalOn.left == nominalOff.left && nominalOn.right == nominalOff.right,
           "the spatial chassis thermal gradient still colours the nominal "
           "model at Unit Character zero");

    const auto fullOn = run(1.0f, true);
    const auto fullOff = run(1.0f, false);
    expect(!(fullOn.left == fullOff.left),
           "the spatial chassis thermal gradient does nothing at full Unit "
           "Character");
}

void testDeterminismAndSilence()
{
    const auto run = []() {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, true);
        auto parameters = plainPatch();
        parameters.calibration = 1.0f;
        parameters.noiseLevel = 0.4f;
        parameters.chorus = ChorusMode::Two;
        engine.setParameters(parameters);
        engine.noteOn(64, 1.0f);
        return render(engine, 24000);
    };

    const auto first = run();
    const auto second = run();
    expect(first.left == second.left && first.right == second.right,
           "the engine is not deterministic");

    // With nothing playing and the effect switched out, the output has to be
    // exactly zero rather than merely small.
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    engine.setParameters(plainPatch());
    const auto silent = render(engine, 24000);
    expect(peakOf(silent.left, 0) == 0.0 && peakOf(silent.right, 0) == 0.0,
           "an idle engine is not exactly silent");
}

void testExtremeAutomationStaysFinite()
{
    YouKnow106Engine engine;
    engine.prepare(44100.0, blockSize, true);

    std::uint32_t state = 12345u;
    const auto next = [&state]() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state & 0xffffffu) / 16777215.0f;
    };

    bool finite = true;
    double peak = 0.0;
    for (int block = 0; block < 400; ++block)
    {
        EngineParameters parameters;
        parameters.lfoRate = next();
        parameters.lfoDelay = next();
        parameters.dcoLfoDepth = next();
        parameters.pwmDepth = next();
        parameters.pwmSource = next() < 0.5f ? PwmSource::Lfo : PwmSource::Manual;
        parameters.range = static_cast<DcoRange>(static_cast<int>(next() * 2.99f));
        parameters.sawEnabled = next() < 0.7f;
        parameters.pulseEnabled = next() < 0.7f;
        parameters.subLevel = next();
        parameters.noiseLevel = next();
        parameters.highPass = static_cast<HighPassMode>(static_cast<int>(next() * 3.99f));
        parameters.cutoff = next();
        parameters.resonance = next();
        parameters.envDepth = next();
        parameters.vcfLfoDepth = next();
        parameters.keyFollow = next();
        parameters.attack = next();
        parameters.decay = next();
        parameters.sustain = next();
        parameters.release = next();
        parameters.chorus = static_cast<ChorusMode>(static_cast<int>(next() * 2.99f));
        parameters.portamento = next();
        parameters.calibration = next();
        parameters.volume = 1.0f;
        parameters.vcaLevel = 1.0f;
        engine.setParameters(parameters);
        engine.setPitchBend(next() * 2.0f - 1.0f);
        engine.setModWheel(next());

        if (block % 5 == 0)
            engine.noteOn(36 + static_cast<int>(next() * 60.0f), next());
        if (block % 7 == 0)
            engine.noteOff(36 + static_cast<int>(next() * 60.0f));

        const auto rendered = render(engine, blockSize);
        for (const auto sample : rendered.left)
        {
            finite = finite && std::isfinite(sample);
            peak = std::max(peak, static_cast<double>(std::abs(sample)));
        }
    }

    expect(finite, "extreme automation produced a non-finite sample");
    expect(peak > 1.0e-6,
           "the extreme-automation stress test never exercised the signal path");
    // This deliberately loose ceiling is only a numerical runaway guard. It
    // does not claim to model an analogue rail or an output-to-dBFS reference.
    expect(peak < 1.0e4,
           "extreme automation produced a pathological finite magnitude");
}

void testParameterSanitisation()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);

    EngineParameters parameters = plainPatch();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    parameters.cutoff = nan;
    parameters.resonance = std::numeric_limits<float>::infinity();
    parameters.attack = -5.0f;
    parameters.sustain = 12.0f;
    parameters.masterTuneCents = 900.0f;
    parameters.polyphony = 500;
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);

    const auto rendered = render(engine, 8192);
    bool finite = true;
    for (const auto sample : rendered.left)
        finite = finite && std::isfinite(sample);
    expect(finite, "hostile parameter values reached the audio path");
    expect(engine.getActiveVoiceCount() >= 1,
           "the engine stopped sounding after hostile parameters");
}

void testSustainPedalHoldsAndReleases()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.setSustainPedal(true);
    engine.noteOn(60, 1.0f);
    render(engine, 4096);
    engine.noteOff(60);
    render(engine, 8192);
    expect(engine.getActiveVoiceCount() == 1,
           "the pedal did not hold the note");

    engine.setSustainPedal(false);
    render(engine, 24000);
    expect(engine.getActiveVoiceCount() == 0,
           "the note did not release when the pedal was lifted");
}

void testFactoryPresetCorpusStaysNumericallySafe()
{
    // The authentic bytes are immutable evidence, including intentionally
    // extreme tones and several VCA LEVEL zeroes. They must not be silently
    // rebalanced to satisfy a product loudness target. This engine-side sweep
    // therefore checks numerical safety only; byte identity lives in the
    // SysEx suite.
    constexpr double sampleRate = 48000.0;
    constexpr int renderSamples = static_cast<int>(sampleRate * 0.25);
    int audibleInShortWindow = 0;

    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);

    for (const auto& preset : presets::factoryBank())
    {
        engine.reset();
        engine.setParameters(parametersFor(preset.patch));
        engine.noteOn(60, 1.0f);
        const auto rendered = render(engine, renderSamples);

        double peak = 0.0;
        bool finite = true;
        for (std::size_t index = 0; index < rendered.left.size(); ++index)
        {
            finite = finite && std::isfinite(rendered.left[index])
                            && std::isfinite(rendered.right[index]);
            peak = std::max({ peak, std::abs(static_cast<double>(rendered.left[index])),
                              std::abs(static_cast<double>(rendered.right[index])) });
        }
        const std::string where =
            std::string(preset.number) + " " + preset.name;
        expect(finite, where + " produced a non-finite sample");
        expect(peak < 1.0e4, where + " produced a numerical runaway");
        audibleInShortWindow += peak > 1.0e-8 ? 1 : 0;
    }

    expect(audibleInShortWindow >= presets::presetCount / 2,
           "too much of the complete original factory corpus was silent in the "
               "short integration sweep: " + std::to_string(audibleInShortWindow)
               + "/" + std::to_string(presets::presetCount));
}

// The firmware may latch both POLY lamps for Solo Unison; the physical contacts
// themselves are momentary. The CHORUS buttons are electrically interlocked,
// so a legacy state with both bits set is resolved to the stronger mode II
// rather than inventing a fourth clock programme.
void testPairedSwitchModes()
{
    expect(keyModeFor(true, false) == KeyMode::Poly1, "POLY 1 alone is not Poly 1");
    expect(keyModeFor(false, true) == KeyMode::Poly2, "POLY 2 alone is not Poly 2");
    expect(keyModeFor(true, true) == KeyMode::Unison,
           "both POLY buttons down is not unison");
    // Not reachable as a stable hardware latch; accepted only so an obsolete
    // or partially automated host state has the power-on Poly 1 meaning.
    expect(keyModeFor(false, false) == KeyMode::Poly1,
           "neither POLY button leaves the assigner undefined");

    expect(chorusModeFor(false, false) == ChorusMode::Off,
           "neither chorus button is not off");
    expect(chorusModeFor(true, false) == ChorusMode::One, "I alone is not mode I");
    expect(chorusModeFor(false, true) == ChorusMode::Two, "II alone is not mode II");
    expect(chorusModeFor(true, true) == ChorusMode::Two,
           "a legacy both-chorus state is not canonical mode II");

    // Round-trip: the engaged-state helpers the editor and the SysEx writer use
    // have to agree with the mode they came from.
    for (const auto mode : { ChorusMode::Off, ChorusMode::One, ChorusMode::Two })
        expect(chorusModeFor(chorusOneEngaged(mode), chorusTwoEngaged(mode)) == mode,
               "a hardware chorus mode did not round trip through its buttons");
    for (int one = 0; one <= 1; ++one)
        for (int two = 0; two <= 1; ++two)
        {
            // Poly1-off/Poly2-off collapses onto Poly 1, so only the three
            // reachable-on-hardware combinations round-trip.
            if (!one && !two)
                continue;
            const auto mode = keyModeFor(one != 0, two != 0);
            expect(poly1Engaged(mode) == (one != 0), "poly 1 round trip");
            expect(poly2Engaged(mode) == (two != 0), "poly 2 round trip");
        }
}

// Every legend on the panel has to be drawn in full. This is the check that
// caught "VOLUME" being ellipsized into a slider cut-out narrower than the
// word, and the stacked buttons whose legends were set at a size that did not
// fit their width.
void testNoLabelIsTruncated()
{
    const auto* overflow = panel::firstOverflowingLabel();
    expect(overflow == nullptr,
           std::string("a panel legend does not fit its control: ")
               + (overflow != nullptr ? overflow : ""));

    // Guard the guard: a legend far too long for its slot must be rejected, or
    // the check above would pass by being blind rather than by the panel
    // fitting. "PORTAMENTO" is what this section would print if it had room.
    const auto& controls = panel::controls();
    const auto& narrow = controls[1];
    expect(panel::textWidth("PORTAMENTO", panel::labelPointSize, true)
               > narrow.labelWidth,
           "the width model thinks a ten-character legend fits a one-slot label");
    expect(panel::buttonPointSizeFor("PORTAMENTO", narrow.width, 40.0f)
               < panel::buttonPointSizeMin,
           "the width model thinks a ten-character legend fits a narrow button");
}

void testPanelLayout()
{
    expect(panel::layoutIsConsistent(),
           "the panel description overlaps, escapes its section, or has a broken group");
    expect(panel::panelWidth() > 0.0f, "the panel has no width");

    const auto& controls = panel::controls();
    expect(controls.size() == panel::controlCount, "panel control count");

    // Every front-panel control must name a parameter, and every parameter the
    // panel names must be reachable from exactly one slider or one radio group.
    for (const auto& control : controls)
    {
        expect(control.parameterId != nullptr && std::strlen(control.parameterId) > 0,
               "a panel control names no parameter");
        expect(control.label != nullptr && std::strlen(control.label) > 0,
               "a panel control carries no legend");
        expect(control.tooltip != nullptr && std::strlen(control.tooltip) >= 24,
               std::string("a panel control has no useful tooltip: ")
                   + (control.label != nullptr ? control.label : ""));
    }

    const std::array<const char*, 11> mustAppear {
        parameters::cutoff, parameters::resonance, parameters::attack,
        parameters::release, parameters::chorusI, parameters::chorusII,
        parameters::range, parameters::highPass, parameters::vcaLevel,
        parameters::poly1, parameters::poly2
    };
    for (const auto* wanted : mustAppear)
    {
        int found = 0;
        for (const auto& control : controls)
            if (std::strcmp(control.parameterId, wanted) == 0)
                ++found;
        expect(found >= 1, std::string("panel does not expose ") + wanted);
    }

    // Accent colour now carries signal meaning instead of merely alternating:
    // green is modulation/performance, blue is the audio path. Keeping that
    // contract explicit prevents a visual polish pass from turning the traces
    // back into decoration.
    const auto& sections = panel::sections();
    for (const auto& section : sections)
    {
        const bool isModulation = std::strcmp(section.name, "BENDER") == 0
                               || std::strcmp(section.name, "MODE") == 0
                               || std::strcmp(section.name, "LFO") == 0
                               || std::strcmp(section.name, "ENV") == 0;
        expect(section.accent == (isModulation ? panel::Accent::Magenta
                                               : panel::Accent::Cyan),
               std::string("section has the wrong signal-flow accent: ")
                   + section.name);
    }
    expect(std::abs(panel::panelWidth() - panel::editorWidth) < 1.0e-5f,
           "the folded panel width disagrees with the editor contract");
}

// The panel help is part of the instrument contract, not decoration. The
// engine routes one delay-gated LFO value to pitch, PWM and cutoff; stale help
// used to describe the pre-fix PWM exception even after that exception had
// left the DSP, directly contradicting the behavior the modulation suite
// fences.
void testPanelHelpMatchesTheModulationRouting()
{
    const panel::Control* delay = nullptr;
    const panel::Control* pwmLfo = nullptr;
    for (const auto& control : panel::controls())
    {
        if (control.parameterId != nullptr
            && std::strcmp(control.parameterId, parameters::lfoDelay) == 0)
            delay = &control;
        if (control.parameterId != nullptr && control.label != nullptr
            && std::strcmp(control.parameterId, parameters::pwmMode) == 0
            && std::strcmp(control.label, "LFO") == 0)
            pwmLfo = &control;
    }

    expect(delay != nullptr, "the panel has no LFO DELAY help to verify");
    expect(pwmLfo != nullptr, "the panel has no PWM LFO help to verify");
    if (delay == nullptr || pwmLfo == nullptr)
        return;

    const std::string delayHelp(delay->tooltip);
    const std::string pwmHelp(pwmLfo->tooltip);
    expect(delayHelp.find("DCO, PWM and VCF") != std::string::npos,
           "LFO DELAY help does not name all three gated destinations");
    expect(pwmHelp.find("delay-gated LFO") != std::string::npos,
           "PWM LFO help does not describe the routed delay envelope");
    expect(delayHelp.find("does not delay PWM") == std::string::npos
               && pwmHelp.find("LFO DELAY does not apply") == std::string::npos,
           "the panel still describes the removed raw-LFO PWM path");
}

// Cost of rendering `seconds` of a patch, as a multiple of realtime, taking
// the fastest of three passes so one descheduled run cannot decide a fence.
double realtimeCost(const EngineParameters& parameters, int notes,
                    double sampleRate, double seconds)
{
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    engine.setParameters(parameters);
    for (int note = 0; note < notes; ++note)
        engine.noteOn(48 + note * 4, 1.0f);

    std::vector<float> left(static_cast<std::size_t>(blockSize));
    std::vector<float> right(static_cast<std::size_t>(blockSize));
    // Past the attack and the modulation delay, so every pass measures the
    // same steady state.
    for (int block = 0; block < 40; ++block)
        engine.process(left.data(), right.data(), blockSize);

    const int total = static_cast<int>(sampleRate * seconds);
    double best = std::numeric_limits<double>::max();
    for (int pass = 0; pass < 3; ++pass)
    {
        const auto started = std::chrono::steady_clock::now();
        int done = 0;
        while (done < total)
        {
            const int count = std::min(blockSize, total - done);
            engine.process(left.data(), right.data(), count);
            done += count;
        }
        const std::chrono::duration<double> elapsed =
            std::chrono::steady_clock::now() - started;
        best = std::min(best, elapsed.count()
                                  / (static_cast<double>(total) / sampleRate));
    }
    return best;
}

void testQualityChangeRefreshesTheFilterCoefficient()
{
    // updateVoiceAudio memoises the counts-to-coefficient chain -- an exp2,
    // two double pow and a tan per card per internal sample -- on exact
    // equality of the counts and loop gain it consumes. Those two survive a
    // quality change untouched while the coefficient they produce is measured
    // in internal samples and does not, so the memo has to be retired when
    // the grid moves. Without that, a settled card would keep integrating on
    // the old rate's pole and the whole instrument would shift in cutoff
    // whenever HQ was switched.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.cutoff = 0.55f;
    parameters.chorus = ChorusMode::Off;
    engine.setParameters(parameters);

    // No key is pressed anywhere in this fixture. The six cards run behind
    // their closed VCAs whatever the keyboard is doing, their holds settle to
    // an exact constant at Unit Character zero, and the memo is therefore
    // being hit on every sample -- which is precisely the state a quality
    // change has to be able to interrupt. Playing a note afterwards would
    // hide the fault: a new note moves the counts and forces a solve.
    render(engine, static_cast<int>(sampleRate * 0.5));
    const float hqCoefficient = YouKnow106TestAccess::filterG(engine, 0);
    const int hqFactor = engine.getOversamplingFactor();
    expect(hqCoefficient > 0.0f, "the settled card has no filter coefficient");

    engine.setOversamplingEnabled(false);
    render(engine, static_cast<int>(sampleRate * 0.5));
    expect(engine.getOversamplingFactor() != hqFactor,
           "the quality change never took effect");
    const float plainCoefficient = YouKnow106TestAccess::filterG(engine, 0);

    // g = tan(pi f / rate), and for this cutoff the argument is small enough
    // that the coefficient scales with the rate ratio to well inside 1%.
    const double expected = static_cast<double>(hqCoefficient) * hqFactor;
    expect(plainCoefficient != hqCoefficient,
           "the filter coefficient did not move when the internal rate did: "
           "the cutoff memo survived a quality change");
    expect(std::abs(plainCoefficient / expected - 1.0) < 0.01,
           "the rebuilt filter coefficient is "
               + std::to_string(plainCoefficient) + ", not the "
               + std::to_string(expected) + " the new grid calls for");
}

void testResonanceDoesNotMultiplyTheSolveCost()
{
    // A ratio, not a time, because a wall-clock ceiling only says how fast the
    // machine running the suite is. What this fences is a property of the
    // solver: the implicit cascade must not cost several times more to run at
    // high resonance than at low, which is what happens when its convergence
    // test cannot be satisfied and every hot sample runs the iteration cap.
    //
    // Measured on one 2.8 GHz core at 48 kHz/HQ before the step test was
    // scaled to the volts it measures: 2.21, 2.24, 2.23. After: 1.31, 1.30,
    // 1.32. Both patches drive the same six cards through the same path and
    // differ only in RESONANCE, so the ratio is the solver's own.
    constexpr double sampleRate = 48000.0;
    auto parameters = plainPatch();
    parameters.chorus = ChorusMode::Off;
    // Not the reference patch's fully-open filter: this has to be a setting
    // where the cascade is actually integrating a musical pole.
    parameters.cutoff = 0.62f;

    parameters.resonance = 0.10f;
    const double plain = realtimeCost(parameters, 6, sampleRate, 1.0);
    parameters.resonance = 0.95f;
    const double resonant = realtimeCost(parameters, 6, sampleRate, 1.0);

    expect(plain > 0.0, "the plain benchmark patch did not run");
    const double ratio = resonant / plain;
    expect(ratio < 1.7,
           "resonance 0.95 costs " + std::to_string(ratio)
               + "x what resonance 0.10 costs; the cascade solve is running "
                 "its iteration cap");
}

void testCpuBudget()
{
    // Not a benchmark of the host, just a guard against a change that makes the
    // engine an order of magnitude more expensive without anyone noticing.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.pulseEnabled = true;
    parameters.subLevel = 0.5f;
    parameters.noiseLevel = 0.1f;
    parameters.resonance = 0.7f;
    parameters.chorus = ChorusMode::Two;
    engine.setParameters(parameters);
    for (int note = 0; note < 6; ++note)
        engine.noteOn(48 + note * 4, 1.0f);

    const auto started = std::chrono::steady_clock::now();
    const auto rendered = render(engine, static_cast<int>(sampleRate * 2));
    const std::chrono::duration<double> elapsed =
        std::chrono::steady_clock::now() - started;
    const double realtimeRatio = elapsed.count() / 2.0;

    expect(peakOf(rendered.left, 0) > 0.0, "the benchmark patch produced silence");
    // Instrumentation costs roughly an order of magnitude, so an instrumented
    // build cannot meet a realtime target. The CMake timeout already allows for
    // that; a looser ceiling keeps the runaway guard without making sanitizer
    // runs impossible.
    const double ceiling = sanitizerBuild ? 12.0 : 5.0;
    expect(realtimeRatio < ceiling,
           "six voices with the effect engaged cost "
               + std::to_string(realtimeRatio) + "x realtime");
}
} // namespace

int main()
{
    testRangeTransposesByOctaves();
    testSubIsOneOctaveDown();
    testSelfOscillationLandsOnTheServiceAnchor();
    testFilterPolesAreStaggeredOnlyByUnitCharacter();
    testMixerLevelIsContinuousInSubAndNoise();
    testUnisonDoesNotBeat();
    testAliasFloor();
    testRampHasARampSpectrum();
    testKeyAssignerDropsRatherThanSteals();
    testPolyModesDifferInAllocation();
    testHeldKeyRescanRunsHighToLow();
    testRescanPreservesVoiceCpuPitchHistory();
    testHeldTransposeUpdatesVoiceCpuPitchHistory();
    testPhysicalPitchWriteRestartIsBandlimited();
    testRescanGateOffReachesTheVoiceCpu();
    testDuplicateAndUnmatchedKeyEdgesAreIgnored();
    testIdleSnapshotPrimesEverySharedHold();
    testPhysicalFiltersKeepRunningBehindClosedVcas();
    testPhysicalCardStateSurvivesVoiceAssignments();
    testConverterSchedulerPreservesFractionalScanPeriod();
    testPulseOffPinsComparatorWithoutResettingTheDco();
    testMovingPwmComparatorDoesNotMissThresholdCrossings();
    testModeChangesRebuildHeldKeys();
    testSustainHeldVoicesRemainAssignable();
    testPoly1AffinityUsesThePhysicalKey();
    testRepressingPolyModeRebuildsHeldAssignments();
    testInactiveVoiceKeepsAdvancingPortamento();
    testPortamentoStateUsesEightEightGrid();
    testUnisonUsesEveryVoiceWithoutDetuning();
    testVoiceCountAboveTheHardwareSixWorks();
    testUnisonSurvivesAReducedVoiceCount();
    testUnisonNoteOnCollapsesAWiderStack();
    testUnisonDoesNotResurrectPolyphonicTails();
    testSubFlipsOnTheFirstWrap();
    testControllersReturnToNeutralOnReset();
    testContinuousControlsDoNotStepAtBlockBoundaries();
    testMainVolumeLoadedLinearPotLaw();
    testFixedOutputBoundaryCorpus();
    testNotesWaitForTheSharedConverterScan();
    testRetriggerDoesNotTouchVcaHoldBeforeConverterScan();
    testSilentVoiceDoesNotInventUnmeasuredVcaFeedthrough();
    testCommonVcaHoldUsesJackBoardC7TimeConstant();
    testPwmHoldCrossesItsTwoSmoothingPoles();
    testSubHoldUsesItsR11C1TimeConstant();
    testPortamentoRateFollowsItsControl();
    testOscillatorSurvivesMoreThanOneCyclePerSample();
    testModulationDelayRearmsForANewPhrase();
    testModulationDelayGatesPulseWidthToo();
    testScanTimingSurvivesAProcessingRateChange();
    testUnisonStackGlidesFromOneOrigin();
    testQualityChangeWaitsForTheOutputPathToEmpty();
    testQualityChangeFadesRateDependentOutputPath();
    testQualityChangePreservesOutputCouplingTail();
    testQualityChangePreservesFreeRunningClocks();
    testModuleInputCouplingKeepsMixerDcOutOfTheVoiceVca();
    testFilterToVcaCouplingRemovesTheDutyDependentThump();
    testFinalOutputCouplingRemovesManualPwmDc();
    testHardStopSilencesTheWholeOutputPath();
    testComponentDriftRateIsIndependentOfOversampling();
    testRailDroopTracksLoadAtOneWallClockRate();
    testThermalWarmupClockRunsToCompletionAtEveryRate();
    testTransposeReachesSoundingVoices();
    testFirstGlidedNoteStartsAtItsOwnPitch();
    testVoicesRetireWithComponentToleranceApplied();
    testUnisonReturnsToAHeldKey();
    testAllNotesOffReleasesRatherThanCutting();
    testLoweringTheVoiceCountLetsNotesFinish();
    testEnvelopeAndGateModes();
    testChorusWidthAndSilence();
    testGlideKeepsTheRampContinuous();
    testChorusSweepTrajectoryDefault();
    testChorusNoiseIsPresentAndDefeatable();
    testIdleOutputFloorCarriesTheMn3009NoiseRow();
    testChorusRateNoiseReproducesTheMeasuredModeDelta();
    testMainNoiseDensityIsProcessingRateInvariant();
    testSampleRateAndOversamplingConsistency();
    testResonanceDoesNotMoveTheRenderedCorner();
    testVelocityScalesTheEnvelopeIntoTheFilter();
    testSelfOscillationMatchesTheServiceTrim();
    testVcfStageOffsetsBelongToUnitCharacter();
    testVcfStageOffsetsAreLiveBeforeTheFirstSample();
    testVcfEarlyEffectBelongsToUnitCharacter();
    testSpatialThermalGradientBelongsToUnitCharacter();
    testDeterminismAndSilence();
    testExtremeAutomationStaysFinite();
    testParameterSanitisation();
    testSustainPedalHoldsAndReleases();
    testFactoryPresetCorpusStaysNumericallySafe();
    testPairedSwitchModes();
    testNoLabelIsTruncated();
    testPanelLayout();
    testPanelHelpMatchesTheModulationRouting();
    testQualityChangeRefreshesTheFilterCoefficient();
    testResonanceDoesNotMultiplyTheSolveCost();
    testCpuBudget();

    if (failures != 0)
    {
        std::cerr << failures << " YouKnow106 engine check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All YouKnow106 engine checks passed.\n";
    return EXIT_SUCCESS;
}
