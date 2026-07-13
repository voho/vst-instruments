#include "MarsEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace mars
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;
constexpr float envelopeRange = 6.90775527898f; // -60 dB in the requested time.

float finiteOr(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float triangleAtPhase(float phase) noexcept
{
    phase -= std::floor(phase);
    return phase < 0.5f ? -1.0f + 4.0f * phase : 3.0f - 4.0f * phase;
}

float safetyLimit(float value) noexcept
{
    if (!std::isfinite(value))
        return 0.0f;

    const float magnitude = std::abs(value);
    if (magnitude <= 0.98f)
        return value;

    const float excess = magnitude - 0.98f;
    const float limited = 0.98f + excess / (1.0f + excess / 0.22f);
    return std::copysign(std::min(limited, 1.2f), value);
}
} // namespace

void MarsEngine::Envelope::reset() noexcept
{
    stage = EnvelopeStage::Idle;
    value = 0.0f;
}

void MarsEngine::Envelope::noteOn() noexcept
{
    value = 0.0f;
    stage = EnvelopeStage::Attack;
}

void MarsEngine::Envelope::noteOff() noexcept
{
    if (stage != EnvelopeStage::Idle)
        stage = EnvelopeStage::Release;
}

float MarsEngine::Envelope::tick(float attackCoefficient, float decayCoefficient,
                                 float sustain, float releaseCoefficient) noexcept
{
    sustain = std::clamp(sustain, 0.0f, 1.0f);

    switch (stage)
    {
        case EnvelopeStage::Idle:
            value = 0.0f;
            break;

        case EnvelopeStage::Attack:
            value += (1.001f - value) * attackCoefficient;
            if (value >= 0.999f)
            {
                value = 1.0f;
                stage = EnvelopeStage::Decay;
            }
            break;

        case EnvelopeStage::Decay:
            value += (sustain - value) * decayCoefficient;
            if (std::abs(value - sustain) < 0.0005f)
            {
                value = sustain;
                stage = EnvelopeStage::Sustain;
            }
            break;

        case EnvelopeStage::Sustain:
            value += (sustain - value) * decayCoefficient;
            break;

        case EnvelopeStage::Release:
            value += (0.0f - value) * releaseCoefficient;
            if (value < 0.00001f)
                reset();
            break;
    }

    return value;
}

void MarsEngine::LadderFilter::reset() noexcept
{
    state.fill(0.0f);
}

float MarsEngine::LadderFilter::process(float input, float integratorGain,
                                        float resonance, float drive) noexcept
{
    const float g = std::clamp(integratorGain, 0.00001f, 0.92f);
    const float complement = 1.0f - g;

    // Express the output of the four TPT one-poles as P*u + S. That permits
    // the feedback path to be solved at the current sample rather than delayed.
    float feedforward = 1.0f;
    float stateContribution = 0.0f;
    for (const float poleState : state)
    {
        stateContribution = g * stateContribution + complement * poleState;
        feedforward *= g;
    }

    const float feedback = 4.18f * std::clamp(resonance, 0.0f, 0.995f);
    const float driveGain = 1.0f + 4.5f * std::clamp(drive, 0.0f, 1.0f);
    const float driven = input * driveGain;

    float u = MarsEngine::softSaturate(driven - feedback * stateContribution);
    for (int iteration = 0; iteration < 2; ++iteration)
    {
        const float predictedOutput = feedforward * u + stateContribution;
        const float feedbackInput = driven - feedback * predictedOutput;
        const float saturated = MarsEngine::softSaturate(feedbackInput);
        const float derivative = MarsEngine::softSaturateDerivative(feedbackInput);
        const float function = u - saturated;
        const float slope = std::max(0.08f, 1.0f + feedback * feedforward * derivative);
        u -= function / slope;
    }

    float stageInput = u;
    for (auto& poleState : state)
    {
        const float v = (stageInput - poleState) * g;
        const float output = v + poleState;
        poleState = output + v;
        stageInput = output;
    }

    const float compensated = stageInput * (1.0f + 0.72f * resonance)
                            / (1.0f + 0.52f * drive);
    return MarsEngine::softSaturate(compensated * (1.0f + 0.55f * drive));
}

void MarsEngine::StateVariableFilter::reset() noexcept
{
    ic1eq = 0.0f;
    ic2eq = 0.0f;
}

void MarsEngine::StateVariableFilter::process(float input, float g,
                                              float resonance, float drive,
                                              float& low, float& band,
                                              float& high) noexcept
{
    g = std::clamp(g, 0.00001f, 8.0f);
    resonance = std::clamp(resonance, 0.0f, 0.995f);
    drive = std::clamp(drive, 0.0f, 1.0f);

    const float driveGain = 1.0f + 3.6f * drive;
    const float driven = MarsEngine::softSaturate(input * driveGain)
                       / (0.86f + 0.34f * drive);
    const float damping = 2.0f - 1.94f * resonance;
    const float a1 = 1.0f / (1.0f + g * (g + damping));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = driven - ic2eq;
    const float v1 = a1 * ic1eq + a2 * v3;
    const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
    ic1eq = 2.0f * v1 - ic1eq;
    ic2eq = 2.0f * v2 - ic2eq;

    band = MarsEngine::softSaturate(v1 * (1.0f + 0.35f * resonance));
    low = MarsEngine::softSaturate(v2 * (1.0f + 0.48f * resonance));
    const float highRaw = driven - damping * v1 - v2;
    high = MarsEngine::softSaturate(highRaw * (0.92f + 0.22f * resonance));
}

MarsEngine::MarsEngine() noexcept
{
    targetParameters_ = sanitise(EngineParameters {});
    smoothedParameters_ = targetParameters_;
}

EngineParameters MarsEngine::sanitise(const EngineParameters& source) noexcept
{
    EngineParameters p = source;
    if (p.voiceMode != VoiceMode::Poly && p.voiceMode != VoiceMode::Unison
        && p.voiceMode != VoiceMode::Fifth)
        p.voiceMode = VoiceMode::Poly;
    const auto validWave = [](OscillatorWave wave)
    {
        return wave == OscillatorWave::Saw || wave == OscillatorWave::Pulse || wave == OscillatorWave::Triangle;
    };
    if (!validWave(p.osc1Wave))
        p.osc1Wave = OscillatorWave::Saw;
    if (!validWave(p.osc2Wave))
        p.osc2Wave = OscillatorWave::Pulse;
    if (p.filterModel != FilterModel::Ladder && p.filterModel != FilterModel::Orbit)
        p.filterModel = FilterModel::Ladder;
    if (p.lfoWave != LfoWaveform::Triangle && p.lfoWave != LfoWaveform::Sine
        && p.lfoWave != LfoWaveform::SampleHold)
        p.lfoWave = LfoWaveform::Triangle;
    p.unisonVoices = std::clamp(p.unisonVoices, 2, 8);
    p.osc1Octave = std::clamp(p.osc1Octave, -2, 2);
    p.osc2Octave = std::clamp(p.osc2Octave, -2, 2);
    p.osc2Semitones = std::clamp(p.osc2Semitones, -12, 12);
    p.osc2FineCents = std::clamp(finiteOr(p.osc2FineCents, 0.0f), -100.0f, 100.0f);
    p.pulseWidth = std::clamp(finiteOr(p.pulseWidth, 0.48f), 0.05f, 0.95f);
    p.crossMod = std::clamp(finiteOr(p.crossMod, 0.08f), 0.0f, 1.0f);
    p.oscMix = std::clamp(finiteOr(p.oscMix, 0.42f), 0.0f, 1.0f);
    p.subLevel = std::clamp(finiteOr(p.subLevel, 0.18f), 0.0f, 1.0f);
    p.noiseLevel = std::clamp(finiteOr(p.noiseLevel, 0.04f), 0.0f, 1.0f);
    p.cutoffHz = std::clamp(finiteOr(p.cutoffHz, 4200.0f), 18.0f, 22000.0f);
    p.resonance = std::clamp(finiteOr(p.resonance, 0.28f), 0.0f, 0.995f);
    p.filterDrive = std::clamp(finiteOr(p.filterDrive, 0.24f), 0.0f, 1.0f);
    p.filterShape = std::clamp(finiteOr(p.filterShape, 0.35f), 0.0f, 1.0f);
    p.filterEnvAmount = std::clamp(finiteOr(p.filterEnvAmount, 0.42f), -1.0f, 1.0f);
    p.lfoFilterOctaves = std::clamp(finiteOr(p.lfoFilterOctaves, 0.15f), -8.0f, 8.0f);
    p.filterKeyTrack = std::clamp(finiteOr(p.filterKeyTrack, 0.45f), 0.0f, 1.0f);
    p.ampAttack = std::clamp(finiteOr(p.ampAttack, 0.008f), 0.0005f, 20.0f);
    p.ampDecay = std::clamp(finiteOr(p.ampDecay, 0.38f), 0.0005f, 20.0f);
    p.ampSustain = std::clamp(finiteOr(p.ampSustain, 0.78f), 0.0f, 1.0f);
    p.ampRelease = std::clamp(finiteOr(p.ampRelease, 0.55f), 0.0005f, 20.0f);
    p.filterAttack = std::clamp(finiteOr(p.filterAttack, 0.012f), 0.0005f, 20.0f);
    p.filterDecay = std::clamp(finiteOr(p.filterDecay, 0.45f), 0.0005f, 20.0f);
    p.filterSustain = std::clamp(finiteOr(p.filterSustain, 0.34f), 0.0f, 1.0f);
    p.filterRelease = std::clamp(finiteOr(p.filterRelease, 0.62f), 0.0005f, 20.0f);
    p.lfoRateHz = std::clamp(finiteOr(p.lfoRateHz, 4.8f), 0.02f, 30.0f);
    p.lfoPitchCents = std::clamp(finiteOr(p.lfoPitchCents, 8.0f), -200.0f, 200.0f);
    p.lfoPwm = std::clamp(finiteOr(p.lfoPwm, 0.18f), 0.0f, 1.0f);
    p.drift = std::clamp(finiteOr(p.drift, 0.28f), 0.0f, 1.0f);
    p.spread = std::clamp(finiteOr(p.spread, 0.58f), 0.0f, 1.0f);
    p.glideSeconds = std::clamp(finiteOr(p.glideSeconds, 0.0f), 0.0f, 5.0f);
    p.velocityAmount = std::clamp(finiteOr(p.velocityAmount, 0.72f), 0.0f, 1.0f);
    p.chorusMix = std::clamp(finiteOr(p.chorusMix, 0.32f), 0.0f, 1.0f);
    p.chorusRateHz = std::clamp(finiteOr(p.chorusRateHz, 0.56f), 0.03f, 8.0f);
    p.outputGain = std::clamp(finiteOr(p.outputGain, 0.72f), 0.0f, 2.0f);
    return p;
}

void MarsEngine::prepare(double sampleRate, int maxBlockSize)
{
    (void) maxBlockSize;
    sampleRate = std::isfinite(sampleRate) ? sampleRate : 48000.0;
    sampleRate_ = std::clamp(sampleRate, 8000.0, maximumSupportedSampleRate);
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    oversampling_ = sampleRate_ <= 96000.0 ? oversampleFactor : 1;
    oversampledRate_ = static_cast<float>(sampleRate_ * static_cast<double>(oversampling_));
    filterCrossfadeSamples_ = std::max(1, static_cast<int>(std::lround(
        0.003 * static_cast<double>(oversampledRate_))));
    // A stolen voice contributes its last sample to this -60 dB / 2 ms tail.
    // It is short enough to remain a transient treatment rather than another
    // voice, and fixed state keeps the note-on path allocation-free.
    stealTailCoefficient_ = std::exp(-envelopeRange
        / (0.002f * std::max(oversampledRate_, 1.0f)));
    dcCoefficient_ = std::exp(-twoPi * 8.0f * inverseSampleRate_);

    buildVoiceCards();
    prepared_ = true;
    reset();
}

void MarsEngine::reset()
{
    for (auto& voice : voices_)
        voice = Voice {};

    oversampleLeftHistory_.fill(0.0f);
    oversampleRightHistory_.fill(0.0f);
    oversampleWriteIndex_ = 0;

    chorusLeft_.fill(0.0f);
    chorusRight_.fill(0.0f);
    chorusWriteIndex_ = 0;
    chorusPhase_ = 0.173f;
    chorusLowLeft_ = 0.0f;
    chorusLowRight_ = 0.0f;
    dcInputLeft_ = dcInputRight_ = 0.0f;
    dcOutputLeft_ = dcOutputRight_ = 0.0f;
    lfoPhase_ = 0.0f;
    randomLfoValue_ = -0.27f;
    globalNoiseState_ = 0x6d2b79f5u;
    pitchBendTarget_ = 0.0f;
    pitchBend_ = 0.0f;
    modWheelTarget_ = 0.0f;
    modWheel_ = 0.0f;
    sustainPedalDown_ = false;
    smoothedParameters_ = targetParameters_;
    oscillator1MixGain_ = std::sqrt(std::max(0.0f, 1.0f - smoothedParameters_.oscMix));
    oscillator2MixGain_ = std::sqrt(std::max(0.0f, smoothedParameters_.oscMix));
    stealTailLeft_ = 0.0f;
    stealTailRight_ = 0.0f;
    generation_ = 0;
    activeVoiceCount_ = 0;
    lastPlayedMidi_ = 60.0f;
    hasLastPlayedMidi_ = false;
}

void MarsEngine::setParameters(const EngineParameters& parameters)
{
    targetParameters_ = sanitise(parameters);
}

std::uint32_t MarsEngine::hash32(std::uint32_t value) noexcept
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value == 0u ? 1u : value;
}

float MarsEngine::hashBipolar(std::uint32_t value) noexcept
{
    return static_cast<float>(hash32(value) & 0x00ffffffu) / 8388607.5f - 1.0f;
}

void MarsEngine::buildVoiceCards() noexcept
{
    for (int i = 0; i < maxVoices; ++i)
    {
        auto& card = cards_[static_cast<std::size_t>(i)];
        const std::uint32_t seed = 0x9e3779b9u * static_cast<std::uint32_t>(i + 1);
        card.osc1Cents = 3.2f * hashBipolar(seed + 1u);
        card.osc2Cents = 4.1f * hashBipolar(seed + 2u);
        card.cutoffError = hashBipolar(seed + 3u);
        card.resonanceError = hashBipolar(seed + 4u);
        card.envelopeError = hashBipolar(seed + 5u);
        card.driveError = hashBipolar(seed + 6u);
        card.panError = hashBipolar(seed + 7u);
        card.pulseSkew = hashBipolar(seed + 8u);
        card.driftRate = 0.031f + 0.091f * (0.5f + 0.5f * hashBipolar(seed + 9u));
        card.driftDepth = 0.72f + 0.56f * (0.5f + 0.5f * hashBipolar(seed + 10u));
        card.driftPhase = 0.5f + 0.5f * hashBipolar(seed + 11u);
    }
}

float MarsEngine::midiToHz(float midiNote) noexcept
{
    return 440.0f * std::exp2((midiNote - 69.0f) / 12.0f);
}

float MarsEngine::wrapPhase(float phase) noexcept
{
    return phase - std::floor(phase);
}

float MarsEngine::smoothStep(float value) noexcept
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

float MarsEngine::envelopeCoefficient(float seconds, float sampleRate) noexcept
{
    seconds = std::max(seconds, 0.0005f);
    return 1.0f - std::exp(-envelopeRange / (seconds * sampleRate));
}

float MarsEngine::polyBlep(float phase, float increment) noexcept
{
    increment = std::clamp(increment, 0.0000001f, 0.499f);
    if (phase < increment)
    {
        const float x = phase / increment;
        return x + x - x * x - 1.0f;
    }
    if (phase > 1.0f - increment)
    {
        const float x = (phase - 1.0f) / increment;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}

float MarsEngine::softSaturate(float value) noexcept
{
    value = std::clamp(value, -3.0f, 3.0f);
    const float square = value * value;
    return value * (27.0f + square) / (27.0f + 9.0f * square);
}

float MarsEngine::softSaturateDerivative(float value) noexcept
{
    if (value <= -3.0f || value >= 3.0f)
        return 0.0f;
    const float square = value * value;
    const float numerator = 27.0f * value + value * square;
    const float denominator = 27.0f + 9.0f * square;
    const float numeratorDerivative = 27.0f + 3.0f * square;
    const float denominatorDerivative = 18.0f * value;
    return (numeratorDerivative * denominator - numerator * denominatorDerivative)
         / (denominator * denominator);
}

float MarsEngine::adaaShape(float value) noexcept
{
    // A smooth diode-like transfer whose antiderivative is cheap and stable.
    // The bounded input also protects the divided difference under hostile
    // automation or corrupt host state.
    value = std::clamp(value, -12.0f, 12.0f);
    return value / std::sqrt(1.0f + value * value);
}

float MarsEngine::adaaAntiderivative(float value) noexcept
{
    value = std::clamp(value, -12.0f, 12.0f);
    return std::sqrt(1.0f + value * value);
}

float MarsEngine::processAdaaMixer(float value, Voice& voice) noexcept
{
    value = std::clamp(finiteOr(value, 0.0f), -12.0f, 12.0f);
    if (!voice.mixerInitialised)
    {
        voice.previousMixerInput = value;
        voice.mixerInitialised = true;
        return adaaShape(value);
    }

    const float previous = voice.previousMixerInput;
    const float difference = value - previous;
    voice.previousMixerInput = value;

    // First-order ADAA. Around coincident samples the quotient loses precision,
    // so evaluate the underlying nonlinearity at the midpoint instead.
    float output = std::abs(difference) < 1.0e-4f * (1.0f + std::abs(value))
        ? adaaShape(0.5f * (value + previous))
        : (adaaAntiderivative(value) - adaaAntiderivative(previous)) / difference;
    if (!std::isfinite(output))
        output = adaaShape(value);
    return output;
}

int MarsEngine::layersForMode(const EngineParameters& parameters) const noexcept
{
    if (parameters.voiceMode == VoiceMode::Unison)
        return std::clamp(parameters.unisonVoices, 2, 8);
    if (parameters.voiceMode == VoiceMode::Fifth)
        return 2;
    return 1;
}

int MarsEngine::orbitIntervalForLayer(int layer) const noexcept
{
    return layer == 0 ? 0 : 7;
}

int MarsEngine::findFreeVoice() const noexcept
{
    for (int i = 0; i < maxVoices; ++i)
        if (!voices_[static_cast<std::size_t>(i)].active)
            return i;
    return -1;
}

void MarsEngine::makeRoomFor(int required) noexcept
{
    int freeVoices = 0;
    for (const auto& voice : voices_)
        freeVoices += voice.active ? 0 : 1;

    while (freeVoices < required)
    {
        int candidate = -1;
        std::uint64_t oldestGeneration = std::numeric_limits<std::uint64_t>::max();
        for (int i = 0; i < maxVoices; ++i)
        {
            const auto& voice = voices_[static_cast<std::size_t>(i)];
            if (voice.active && voice.releasing && voice.generation < oldestGeneration)
            {
                candidate = i;
                oldestGeneration = voice.generation;
            }
        }
        if (candidate < 0)
        {
            for (int i = 0; i < maxVoices; ++i)
            {
                const auto& voice = voices_[static_cast<std::size_t>(i)];
                if (voice.active && voice.generation < oldestGeneration)
                {
                    candidate = i;
                    oldestGeneration = voice.generation;
                }
            }
        }
        if (candidate < 0)
            break;

        const auto generation = voices_[static_cast<std::size_t>(candidate)].generation;
        for (auto& voice : voices_)
        {
            if (voice.active && voice.generation == generation)
            {
                silenceVoice(voice, true);
                ++freeVoices;
            }
        }
    }
}

void MarsEngine::initialiseVoice(Voice& voice, int slot, int rootMidi,
                                 int soundingMidi, int layer, int layerCount,
                                 float velocity,
                                 const EngineParameters& parameters) noexcept
{
    voice = Voice {};
    voice.active = true;
    voice.keyDown = true;
    voice.rootMidi = rootMidi;
    voice.soundingMidi = soundingMidi;
    voice.layer = layer;
    voice.cardIndex = slot;
    voice.generation = generation_;
    voice.velocity = velocity;
    voice.targetMidi = static_cast<float>(soundingMidi);
    const float interval = static_cast<float>(soundingMidi - rootMidi);
    voice.currentMidi = parameters.glideSeconds > 0.0f && hasLastPlayedMidi_
        ? std::clamp(lastPlayedMidi_ + interval, 0.0f, 127.0f)
        : voice.targetMidi;

    const float layerPosition = layerCount > 1
        ? 2.0f * static_cast<float>(layer) / static_cast<float>(layerCount - 1) - 1.0f
        : 0.0f;
    if (parameters.voiceMode == VoiceMode::Unison)
    {
        voice.unisonCents = layerPosition * (4.0f + 20.0f * parameters.drift);
        voice.panBase = 0.86f * layerPosition;
        voice.groupGain = 0.72f / std::sqrt(static_cast<float>(layerCount));
    }
    else if (parameters.voiceMode == VoiceMode::Fifth)
    {
        voice.unisonCents = 0.35f * layerPosition;
        voice.panBase = 0.76f * layerPosition;
        voice.groupGain = 0.57f / std::sqrt(static_cast<float>(layerCount));
    }
    else
    {
        voice.groupGain = 0.82f;
        voice.panBase = 0.0f;
    }

    const std::uint32_t seed = hash32(static_cast<std::uint32_t>(generation_)
                                    ^ static_cast<std::uint32_t>(rootMidi * 977 + layer * 131));
    voice.noiseState = seed;
    voice.oscillator1.phase = wrapPhase(0.17f + 0.37f * (0.5f + 0.5f * hashBipolar(seed + 1u)));
    voice.oscillator2.phase = wrapPhase(0.53f + 0.41f * (0.5f + 0.5f * hashBipolar(seed + 2u)));
    voice.subOscillator.phase = wrapPhase(voice.oscillator1.phase * 0.5f);
    // The leaky polyBLEP triangle stores the low-pass integrator state, whose
    // steady-state amplitude is approximately one quarter of the output.
    voice.oscillator1.triangle = 0.25f * triangleAtPhase(voice.oscillator1.phase);
    voice.oscillator2.triangle = 0.25f * triangleAtPhase(voice.oscillator2.phase);
    voice.subOscillator.triangle = 0.25f * triangleAtPhase(voice.subOscillator.phase);
    voice.driftPhase = wrapPhase(cards_[static_cast<std::size_t>(slot)].driftPhase
                               + 0.173f * static_cast<float>(layer));
    voice.ampEnvelope.noteOn();
    voice.filterEnvelope.noteOn();
    voice.controlCountdown = 0;
    updateVoiceControl(voice, parameters, 0.0f);
}

void MarsEngine::noteOn(int midiNote, float velocity)
{
    if (velocity <= 0.0f || !std::isfinite(velocity))
    {
        noteOff(midiNote);
        return;
    }
    if (!prepared_)
        prepare(sampleRate_, 512);

    midiNote = std::clamp(midiNote, 0, 127);
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    const EngineParameters parameters = targetParameters_;
    const int layerCount = layersForMode(parameters);

    // Repeated MIDI notes retrigger as one physical key action.
    for (auto& voice : voices_)
        if (voice.active && voice.rootMidi == midiNote)
            silenceVoice(voice, true);

    for (;;)
    {
        int heldGroups = 0;
        std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
        for (const auto& voice : voices_)
        {
            if (voice.active && voice.keyDown && voice.layer == 0)
            {
                ++heldGroups;
                oldest = std::min(oldest, voice.generation);
            }
        }
        if (heldGroups < 16 || oldest == std::numeric_limits<std::uint64_t>::max())
            break;
        for (auto& voice : voices_)
            if (voice.active && voice.generation == oldest)
                silenceVoice(voice, true);
    }

    makeRoomFor(layerCount);
    ++generation_;
    for (int layer = 0; layer < layerCount; ++layer)
    {
        const int slot = findFreeVoice();
        if (slot < 0)
            break;
        const int interval = parameters.voiceMode == VoiceMode::Fifth
            ? orbitIntervalForLayer(layer) : 0;
        const int soundingMidi = std::clamp(midiNote + interval, 0, 127);
        initialiseVoice(voices_[static_cast<std::size_t>(slot)], slot, midiNote,
                        soundingMidi, layer, layerCount, velocity, parameters);
    }
    lastPlayedMidi_ = static_cast<float>(midiNote);
    hasLastPlayedMidi_ = true;
    updateActiveVoiceCount();
}

void MarsEngine::noteOff(int midiNote)
{
    midiNote = std::clamp(midiNote, 0, 127);
    for (auto& voice : voices_)
    {
        if (voice.active && voice.rootMidi == midiNote)
        {
            voice.keyDown = false;
            if (sustainPedalDown_)
            {
                voice.sustained = true;
            }
            else
            {
                voice.releasing = true;
                voice.ampEnvelope.noteOff();
                voice.filterEnvelope.noteOff();
            }
        }
    }
}

void MarsEngine::allNotesOff()
{
    for (auto& voice : voices_)
    {
        if (!voice.active)
            continue;

        const bool wasHeld = voice.keyDown;
        voice.keyDown = false;
        if (sustainPedalDown_ && wasHeld && !voice.releasing)
        {
            voice.sustained = true;
            continue;
        }

        if (!sustainPedalDown_)
        {
            voice.sustained = false;
            voice.releasing = true;
            voice.ampEnvelope.noteOff();
            voice.filterEnvelope.noteOff();
        }
    }
}

void MarsEngine::setPitchBend(float normalisedBipolar) noexcept
{
    pitchBendTarget_ = std::clamp(finiteOr(normalisedBipolar, 0.0f), -1.0f, 1.0f);
}

void MarsEngine::setModWheel(float amount) noexcept
{
    modWheelTarget_ = std::clamp(finiteOr(amount, 0.0f), 0.0f, 1.0f);
}

void MarsEngine::setSustainPedal(bool down) noexcept
{
    if (sustainPedalDown_ == down)
        return;

    sustainPedalDown_ = down;
    if (down)
        return;

    for (auto& voice : voices_)
    {
        if (voice.active && voice.sustained && !voice.keyDown)
        {
            voice.sustained = false;
            voice.releasing = true;
            voice.ampEnvelope.noteOff();
            voice.filterEnvelope.noteOff();
        }
    }
}

void MarsEngine::addVoiceToStealTail(const Voice& voice) noexcept
{
    if (!voice.active || !std::isfinite(voice.lastOutput))
        return;

    constexpr float maximumTail = 8.0f;
    stealTailLeft_ = std::clamp(stealTailLeft_ + voice.lastOutput * voice.panLeft,
                                -maximumTail, maximumTail);
    stealTailRight_ = std::clamp(stealTailRight_ + voice.lastOutput * voice.panRight,
                                 -maximumTail, maximumTail);
}

void MarsEngine::renderStealTail(float& left, float& right) noexcept
{
    left = stealTailLeft_;
    right = stealTailRight_;
    stealTailLeft_ *= stealTailCoefficient_;
    stealTailRight_ *= stealTailCoefficient_;
    if (std::abs(stealTailLeft_) < 1.0e-7f)
        stealTailLeft_ = 0.0f;
    if (std::abs(stealTailRight_) < 1.0e-7f)
        stealTailRight_ = 0.0f;
}

void MarsEngine::silenceVoice(Voice& voice, bool preserveTail) noexcept
{
    if (preserveTail)
        addVoiceToStealTail(voice);
    voice.active = false;
    voice.releasing = false;
    voice.keyDown = false;
    voice.sustained = false;
    voice.ampEnvelope.reset();
    voice.filterEnvelope.reset();
    voice.ladder.reset();
    voice.stateVariable.reset();
}

void MarsEngine::updateActiveVoiceCount() noexcept
{
    int count = 0;
    for (const auto& voice : voices_)
        count += voice.active ? 1 : 0;
    activeVoiceCount_ = count;
}

float MarsEngine::nextNoise(Voice& voice) noexcept
{
    std::uint32_t value = voice.noiseState;
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    voice.noiseState = value == 0u ? 1u : value;
    return static_cast<float>(voice.noiseState & 0x00ffffffu) / 8388607.5f - 1.0f;
}

float MarsEngine::nextLfoValue(const EngineParameters& parameters) noexcept
{
    lfoPhase_ += parameters.lfoRateHz * inverseSampleRate_;
    if (lfoPhase_ >= 1.0f)
    {
        lfoPhase_ -= std::floor(lfoPhase_);
        globalNoiseState_ = hash32(globalNoiseState_ + 0x9e3779b9u);
        randomLfoValue_ = hashBipolar(globalNoiseState_);
    }

    if (parameters.lfoWave == LfoWaveform::Sine)
        return std::sin(twoPi * lfoPhase_);
    if (parameters.lfoWave == LfoWaveform::SampleHold)
        return randomLfoValue_;
    return 1.0f - 4.0f * std::abs(lfoPhase_ - 0.5f);
}

void MarsEngine::updateVoiceControl(Voice& voice,
                                    const EngineParameters& parameters,
                                    float lfoValue) noexcept
{
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float ageScale = 0.25f + 0.75f * parameters.drift;
    const float voiceAge = static_cast<float>(voice.ageSamples) / oversampledRate_;
    const float lfoFade = smoothStep(voiceAge / 0.14f);
    const float driftValue = std::sin(twoPi * voice.driftPhase);
    voice.driftPhase = wrapPhase(voice.driftPhase
                               + card.driftRate * static_cast<float>(controlPeriod) / oversampledRate_);

    if (parameters.glideSeconds <= 0.0005f)
    {
        voice.currentMidi = voice.targetMidi;
    }
    else
    {
        const float glideCoefficient = 1.0f - std::exp(
            -envelopeRange * static_cast<float>(controlPeriod)
            / (parameters.glideSeconds * oversampledRate_));
        voice.currentMidi += glideCoefficient * (voice.targetMidi - voice.currentMidi);
    }

    const float wanderCents = parameters.drift * card.driftDepth * (2.6f * driftValue);
    const float lfoCents = (parameters.lfoPitchCents + 40.0f * modWheel_)
                         * lfoFade * lfoValue;
    const float oscillator1Cents = voice.unisonCents + ageScale * card.osc1Cents
                                 + wanderCents + lfoCents;
    const float oscillator2Cents = -0.37f * voice.unisonCents + parameters.osc2FineCents
                                 + ageScale * card.osc2Cents - 0.72f * wanderCents
                                 + lfoCents;
    const float bendSemitones = 2.0f * pitchBend_;
    const float oscillator1Note = voice.currentMidi + static_cast<float>(12 * parameters.osc1Octave)
                                + bendSemitones
                                + oscillator1Cents / 100.0f;
    const float oscillator2Note = voice.currentMidi
                                + static_cast<float>(12 * parameters.osc2Octave
                                                     + parameters.osc2Semitones)
                                + bendSemitones
                                + oscillator2Cents / 100.0f;
    const float oscillator1Target = std::clamp(midiToHz(oscillator1Note) / oversampledRate_,
                                               0.0000001f, 0.45f);
    const float oscillator2Target = std::clamp(midiToHz(oscillator2Note) / oversampledRate_,
                                               0.0000001f, 0.45f);
    const float subTarget = std::max(0.0000001f, 0.5f * oscillator1Target);

    const float keyOctaves = parameters.filterKeyTrack
                           * (voice.currentMidi - 60.0f) / 12.0f;
    const float envelopeOctaves = 5.4f * parameters.filterEnvAmount
                                * voice.filterEnvelope.value;
    const float lfoOctaves = (parameters.lfoFilterOctaves + 0.5f * modWheel_)
                           * lfoFade * lfoValue;
    const float componentOctaves = ageScale * (0.075f * card.cutoffError)
                                 + parameters.drift * 0.028f * driftValue;
    const float maximumCutoff = 0.45f * static_cast<float>(sampleRate_);
    const float cutoff = std::clamp(parameters.cutoffHz
                                    * std::exp2(keyOctaves + envelopeOctaves
                                                + lfoOctaves + componentOctaves),
                                    18.0f, maximumCutoff);
    const float stateTarget = std::tan(pi * cutoff / oversampledRate_);
    const float ladderTarget = stateTarget / (1.0f + stateTarget);

    const float panMotion = parameters.voiceMode == VoiceMode::Fifth
        ? 0.16f * std::sin(twoPi * (voice.driftPhase + 0.19f * static_cast<float>(voice.layer)))
        : 0.0f;
    const float pan = std::clamp(parameters.spread
                                 * (voice.panBase + panMotion + 0.12f * ageScale * card.panError),
                                 -1.0f, 1.0f);
    const float targetLeft = std::sqrt(0.5f * (1.0f - pan));
    const float targetRight = std::sqrt(0.5f * (1.0f + pan));

    if (!voice.controlInitialised)
    {
        voice.oscillator1Increment = oscillator1Target;
        voice.oscillator2Increment = oscillator2Target;
        voice.subIncrement = subTarget;
        voice.ladderGain = ladderTarget;
        voice.stateGain = stateTarget;
        voice.panLeft = targetLeft;
        voice.panRight = targetRight;
        voice.controlInitialised = true;
    }

    const float period = static_cast<float>(controlPeriod);
    voice.oscillator1Step = (oscillator1Target - voice.oscillator1Increment) / period;
    voice.oscillator2Step = (oscillator2Target - voice.oscillator2Increment) / period;
    voice.subStep = (subTarget - voice.subIncrement) / period;
    voice.ladderGainStep = (ladderTarget - voice.ladderGain) / period;
    voice.stateGainStep = (stateTarget - voice.stateGain) / period;
    voice.panLeftStep = (targetLeft - voice.panLeft) / period;
    voice.panRightStep = (targetRight - voice.panRight) / period;
    voice.resonance = std::clamp(parameters.resonance
                                 + 0.045f * ageScale * card.resonanceError, 0.0f, 0.995f);
    voice.drive = std::clamp(parameters.filterDrive
                             * (1.0f + 0.16f * ageScale * card.driveError), 0.0f, 1.0f);

    const float envelopeScale = std::clamp(1.0f + 0.14f * ageScale * card.envelopeError,
                                           0.72f, 1.28f);
    voice.ampAttackCoefficient = envelopeCoefficient(parameters.ampAttack * envelopeScale,
                                                      oversampledRate_);
    voice.ampDecayCoefficient = envelopeCoefficient(parameters.ampDecay * envelopeScale,
                                                     oversampledRate_);
    voice.ampReleaseCoefficient = envelopeCoefficient(parameters.ampRelease * envelopeScale,
                                                       oversampledRate_);
    voice.filterAttackCoefficient = envelopeCoefficient(parameters.filterAttack * envelopeScale,
                                                         oversampledRate_);
    voice.filterDecayCoefficient = envelopeCoefficient(parameters.filterDecay * envelopeScale,
                                                        oversampledRate_);
    voice.filterReleaseCoefficient = envelopeCoefficient(parameters.filterRelease * envelopeScale,
                                                          oversampledRate_);
}

float MarsEngine::renderOscillator(Oscillator& oscillator, OscillatorWave waveform,
                                   float increment, float pulseWidth,
                                   float phaseOffset, bool& wrapped) noexcept
{
    increment = std::clamp(increment, 0.0000001f, 0.45f);
    pulseWidth = std::clamp(pulseWidth, 0.05f, 0.95f);
    const float phase = wrapPhase(oscillator.phase + phaseOffset);
    float output = 0.0f;
    if (waveform == OscillatorWave::Pulse)
    {
        const float shiftedPulsePhase = wrapPhase(phase - pulseWidth);
        output = (phase < pulseWidth ? 1.0f : -1.0f)
               + polyBlep(phase, increment)
               - polyBlep(shiftedPulsePhase, increment);
        oscillator.triangle = 0.25f * triangleAtPhase(phase);
    }
    else if (waveform == OscillatorWave::Triangle)
    {
        // A one-pole leaky integrator of the BLEP-corrected square is the
        // standard stable polyBLEP triangle construction. Unlike an unbounded
        // accumulator it cannot collect floating-point DC error over long pads.
        const float triangleSquare = (phase < 0.5f ? 1.0f : -1.0f)
                                   + polyBlep(phase, increment)
                                   - polyBlep(wrapPhase(phase - 0.5f), increment);
        oscillator.triangle = increment * triangleSquare
                            + (1.0f - increment) * oscillator.triangle;
        oscillator.triangle = std::clamp(finiteOr(oscillator.triangle, 0.0f),
                                         -0.35f, 0.35f);
        output = 4.0f * oscillator.triangle;
    }
    else
    {
        output = 2.0f * phase - 1.0f - polyBlep(phase, increment);
        oscillator.triangle = 0.25f * triangleAtPhase(phase);
    }

    oscillator.phase += increment;
    wrapped = oscillator.phase >= 1.0f;
    if (wrapped)
        oscillator.phase -= std::floor(oscillator.phase);
    return output;
}

float MarsEngine::renderVoiceOversample(Voice& voice,
                                        const EngineParameters& parameters,
                                        float lfoValue) noexcept
{
    if (!voice.active)
        return 0.0f;

    if (--voice.controlCountdown <= 0)
    {
        updateVoiceControl(voice, parameters, lfoValue);
        voice.controlCountdown = controlPeriod;
    }

    voice.oscillator1Increment += voice.oscillator1Step;
    voice.oscillator2Increment += voice.oscillator2Step;
    voice.subIncrement += voice.subStep;
    voice.ladderGain += voice.ladderGainStep;
    voice.stateGain += voice.stateGainStep;
    voice.panLeft += voice.panLeftStep;
    voice.panRight += voice.panRightStep;

    const float ampEnvelope = voice.ampEnvelope.tick(voice.ampAttackCoefficient,
                                                     voice.ampDecayCoefficient,
                                                     parameters.ampSustain,
                                                     voice.ampReleaseCoefficient);
    voice.filterEnvelope.tick(voice.filterAttackCoefficient,
                              voice.filterDecayCoefficient,
                              parameters.filterSustain,
                              voice.filterReleaseCoefficient);

    const float voiceAge = static_cast<float>(voice.ageSamples) / oversampledRate_;
    const float lfoFade = smoothStep(voiceAge / 0.14f);
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float ageScale = 0.25f + 0.75f * parameters.drift;
    const float pulseWidth = std::clamp(parameters.pulseWidth
                                        + 0.42f * parameters.lfoPwm * lfoFade * lfoValue
                                        + 0.035f * ageScale * card.pulseSkew,
                                        0.05f, 0.95f);

    bool oscillator2Wrapped = false;
    const float oscillator2 = renderOscillator(voice.oscillator2, parameters.osc2Wave,
                                               voice.oscillator2Increment, pulseWidth,
                                               0.0f, oscillator2Wrapped);
    (void) oscillator2Wrapped;
    const float phaseModulation = 0.068f * parameters.crossMod * oscillator2;
    bool oscillator1Wrapped = false;
    float oscillator1 = renderOscillator(voice.oscillator1, parameters.osc1Wave,
                                         voice.oscillator1Increment, pulseWidth,
                                         phaseModulation, oscillator1Wrapped);

    (void) oscillator1Wrapped;

    bool subWrapped = false;
    const float sub = renderOscillator(voice.subOscillator, OscillatorWave::Pulse,
                                       voice.subIncrement, 0.5f, 0.0f, subWrapped);
    (void) subWrapped;

    const float shaping = 1.05f + 0.28f * parameters.drift;
    const float normalisation = std::max(0.2f, softSaturate(shaping));
    oscillator1 = softSaturate(oscillator1 * shaping) / normalisation;

    const float whiteNoise = nextNoise(voice);
    const float noiseCoefficient = std::clamp(twoPi * 5200.0f / oversampledRate_, 0.001f, 0.42f);
    voice.noiseColour += noiseCoefficient * (whiteNoise - voice.noiseColour);
    const float filteredNoise = 0.62f * whiteNoise + 0.38f * voice.noiseColour;
    const float rawMix = oscillator1MixGain_ * oscillator1
                       + oscillator2MixGain_ * oscillator2
                       + parameters.subLevel * 0.72f * sub
                       + parameters.noiseLevel * filteredNoise;
    const float mixerGain = 0.62f + 1.9f * voice.drive;
    const float mixed = processAdaaMixer(rawMix * mixerGain, voice)
                      / (0.92f + 0.46f * voice.drive);

    if (!voice.filterModelInitialised)
    {
        voice.activeFilterModel = parameters.filterModel;
        voice.filterBlend = parameters.filterModel == FilterModel::Orbit ? 1.0f : 0.0f;
        voice.filterBlendStep = 0.0f;
        voice.filterCrossfadeRemaining = 0;
        voice.filterModelInitialised = true;
    }
    else if (voice.activeFilterModel != parameters.filterModel)
    {
        // At a steady endpoint the inactive model has stale state, so restart
        // it silently under a zero-gain side of the crossfade. If automation
        // reverses an in-flight transition both models are already current and
        // the blend simply changes direction without a discontinuity.
        if (voice.filterCrossfadeRemaining == 0)
        {
            if (parameters.filterModel == FilterModel::Ladder)
                voice.ladder.reset();
            else
                voice.stateVariable.reset();
        }

        voice.activeFilterModel = parameters.filterModel;
        voice.filterCrossfadeRemaining = filterCrossfadeSamples_;
        const float targetBlend = parameters.filterModel == FilterModel::Orbit ? 1.0f : 0.0f;
        voice.filterBlendStep = (targetBlend - voice.filterBlend)
                              / static_cast<float>(filterCrossfadeSamples_);
    }

    float filtered = 0.0f;
    const auto processLadder = [&voice, mixed]() noexcept
    {
        return voice.ladder.process(mixed, voice.ladderGain,
                                    voice.resonance, voice.drive);
    };
    const auto processOrbit = [&voice, &parameters, mixed]() noexcept
    {
        float low = 0.0f;
        float band = 0.0f;
        float high = 0.0f;
        voice.stateVariable.process(mixed, voice.stateGain, voice.resonance,
                                    voice.drive, low, band, high);

        // ORBIT traverses LP -> BP -> HP in two equal-power crossfades. The
        // modest gain trims equalise perceived loudness without flattening the
        // deliberately different responses at the three anchor positions.
        const float shape = std::clamp(parameters.filterShape, 0.0f, 1.0f);
        if (shape < 0.5f)
        {
            const float blend = shape * 2.0f;
            return std::cos(0.5f * pi * blend) * low
                 + std::sin(0.5f * pi * blend) * (1.22f * band);
        }
        const float blend = (shape - 0.5f) * 2.0f;
        return std::cos(0.5f * pi * blend) * (1.22f * band)
             + std::sin(0.5f * pi * blend) * (0.92f * high);
    };

    if (voice.filterCrossfadeRemaining > 0)
    {
        const float ladderOutput = processLadder();
        const float orbitOutput = processOrbit();
        filtered = (1.0f - voice.filterBlend) * ladderOutput
                 + voice.filterBlend * orbitOutput;

        --voice.filterCrossfadeRemaining;
        if (voice.filterCrossfadeRemaining == 0)
        {
            voice.filterBlend = voice.activeFilterModel == FilterModel::Orbit ? 1.0f : 0.0f;
            voice.filterBlendStep = 0.0f;
        }
        else
        {
            voice.filterBlend = std::clamp(voice.filterBlend + voice.filterBlendStep,
                                           0.0f, 1.0f);
        }
    }
    else if (voice.activeFilterModel == FilterModel::Ladder)
        filtered = processLadder();
    else
        filtered = processOrbit();

    const float velocityCurve = std::sqrt(voice.velocity);
    const float velocityGain = 1.0f + parameters.velocityAmount * (velocityCurve - 1.0f);
    const float vcaInput = filtered * ampEnvelope * velocityGain * voice.groupGain;
    const float output = softSaturate(vcaInput * (1.0f + 0.48f * voice.drive))
                       / (1.0f + 0.18f * voice.drive);
    voice.lastOutput = output;
    ++voice.ageSamples;

    if (voice.ampEnvelope.stage == EnvelopeStage::Idle)
        silenceVoice(voice);
    return output;
}

void MarsEngine::downsampleStereo(float firstLeft, float firstRight,
                                  float secondLeft, float secondRight,
                                  float& outputLeft, float& outputRight) noexcept
{
    constexpr int historySize = 15;
    const auto push = [this](float left, float right)
    {
        oversampleLeftHistory_[static_cast<std::size_t>(oversampleWriteIndex_)] = left;
        oversampleRightHistory_[static_cast<std::size_t>(oversampleWriteIndex_)] = right;
        oversampleWriteIndex_ = (oversampleWriteIndex_ + 1) % historySize;
    };
    push(firstLeft, firstRight);
    push(secondLeft, secondRight);

    const auto filter = [this](const std::array<float, historySize>& history)
    {
        const auto delayed = [this, &history](int samples)
        {
            int index = oversampleWriteIndex_ - 1 - samples;
            if (index < 0)
                index += historySize;
            return history[static_cast<std::size_t>(index)];
        };
        return -0.000269694680235f * (delayed(0) + delayed(14))
               + 0.00939858691896f * (delayed(2) + delayed(12))
               - 0.056935395043f * (delayed(4) + delayed(10))
               + 0.29782827755f * (delayed(6) + delayed(8))
               + 0.499956450509f * delayed(7);
    };

    // 15-tap Kaiser-windowed half-band low-pass, cutoff at the eventual output
    // Nyquist. It follows the summed nonlinear voices, which both avoids 32
    // redundant FIRs and prevents their out-of-band products from folding.
    outputLeft = filter(oversampleLeftHistory_);
    outputRight = filter(oversampleRightHistory_);
}

float MarsEngine::readFractional(const std::array<float, chorusBufferSize>& buffer,
                                 float delaySamples) const noexcept
{
    float position = static_cast<float>(chorusWriteIndex_) - delaySamples;
    while (position < 0.0f)
        position += static_cast<float>(chorusBufferSize);
    const int index = static_cast<int>(position) & (chorusBufferSize - 1);
    const float fraction = position - std::floor(position);
    const float first = buffer[static_cast<std::size_t>(index)];
    const float second = buffer[static_cast<std::size_t>((index + 1) & (chorusBufferSize - 1))];
    return first + fraction * (second - first);
}

void MarsEngine::processChorus(float inputLeft, float inputRight,
                               const EngineParameters& parameters,
                               float& outputLeft, float& outputRight) noexcept
{
    chorusLeft_[static_cast<std::size_t>(chorusWriteIndex_)] = inputLeft;
    chorusRight_[static_cast<std::size_t>(chorusWriteIndex_)] = inputRight;

    const float baseDelay = 0.0095f;
    const float depth = 0.0028f;
    chorusPhase_ = wrapPhase(chorusPhase_ + parameters.chorusRateHz * inverseSampleRate_);
    const float leftMod = std::sin(twoPi * chorusPhase_);
    const float rightMod = std::sin(twoPi * wrapPhase(chorusPhase_ + 0.31f));
    float wetLeft = readFractional(chorusRight_, (baseDelay + depth * leftMod)
                                                  * static_cast<float>(sampleRate_));
    float wetRight = readFractional(chorusLeft_, (baseDelay + depth * rightMod)
                                                   * static_cast<float>(sampleRate_));
    const float bandwidthCoefficient = std::clamp(
        1.0f - std::exp(-twoPi * 7200.0f * inverseSampleRate_), 0.01f, 0.72f);
    chorusLowLeft_ += bandwidthCoefficient * (wetLeft - chorusLowLeft_);
    chorusLowRight_ += bandwidthCoefficient * (wetRight - chorusLowRight_);
    wetLeft = softSaturate(1.08f * chorusLowLeft_);
    wetRight = softSaturate(1.08f * chorusLowRight_);

    chorusWriteIndex_ = (chorusWriteIndex_ + 1) & (chorusBufferSize - 1);
    const float mix = parameters.chorusMix;
    outputLeft = (1.0f - 0.16f * mix) * inputLeft + 0.74f * mix * wetLeft;
    outputRight = (1.0f - 0.16f * mix) * inputRight + 0.74f * mix * wetRight;
}

float MarsEngine::processDcBlocker(float input, float& previousInput,
                                   float& previousOutput) const noexcept
{
    const float output = input - previousInput + dcCoefficient_ * previousOutput;
    previousInput = input;
    previousOutput = output;
    return output;
}

void MarsEngine::process(float* left, float* right, int numSamples)
{
    if (numSamples <= 0)
        return;
    if (!prepared_)
        prepare(sampleRate_, numSamples);

    const EngineParameters targets = targetParameters_;
    smoothedParameters_.voiceMode = targets.voiceMode;
    smoothedParameters_.osc1Wave = targets.osc1Wave;
    smoothedParameters_.osc2Wave = targets.osc2Wave;
    smoothedParameters_.filterModel = targets.filterModel;
    smoothedParameters_.lfoWave = targets.lfoWave;
    smoothedParameters_.unisonVoices = targets.unisonVoices;
    smoothedParameters_.osc1Octave = targets.osc1Octave;
    smoothedParameters_.osc2Octave = targets.osc2Octave;
    smoothedParameters_.osc2Semitones = targets.osc2Semitones;

    const float smoothing = 1.0f - std::exp(-inverseSampleRate_ / 0.014f);
    const float performanceSmoothing = 1.0f - std::exp(-inverseSampleRate_ / 0.005f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto smooth = [smoothing](float& current, float target)
        {
            current += smoothing * (target - current);
        };
        smooth(smoothedParameters_.osc2FineCents, targets.osc2FineCents);
        smooth(smoothedParameters_.pulseWidth, targets.pulseWidth);
        smooth(smoothedParameters_.crossMod, targets.crossMod);
        smooth(smoothedParameters_.oscMix, targets.oscMix);
        smooth(smoothedParameters_.subLevel, targets.subLevel);
        smooth(smoothedParameters_.noiseLevel, targets.noiseLevel);
        smooth(smoothedParameters_.cutoffHz, targets.cutoffHz);
        smooth(smoothedParameters_.resonance, targets.resonance);
        smooth(smoothedParameters_.filterDrive, targets.filterDrive);
        smooth(smoothedParameters_.filterShape, targets.filterShape);
        smooth(smoothedParameters_.filterEnvAmount, targets.filterEnvAmount);
        smooth(smoothedParameters_.filterKeyTrack, targets.filterKeyTrack);
        smooth(smoothedParameters_.ampAttack, targets.ampAttack);
        smooth(smoothedParameters_.ampDecay, targets.ampDecay);
        smooth(smoothedParameters_.ampSustain, targets.ampSustain);
        smooth(smoothedParameters_.ampRelease, targets.ampRelease);
        smooth(smoothedParameters_.filterAttack, targets.filterAttack);
        smooth(smoothedParameters_.filterDecay, targets.filterDecay);
        smooth(smoothedParameters_.filterSustain, targets.filterSustain);
        smooth(smoothedParameters_.filterRelease, targets.filterRelease);
        smooth(smoothedParameters_.lfoRateHz, targets.lfoRateHz);
        smooth(smoothedParameters_.lfoPitchCents, targets.lfoPitchCents);
        smooth(smoothedParameters_.lfoPwm, targets.lfoPwm);
        smooth(smoothedParameters_.lfoFilterOctaves, targets.lfoFilterOctaves);
        smooth(smoothedParameters_.drift, targets.drift);
        smooth(smoothedParameters_.spread, targets.spread);
        smooth(smoothedParameters_.glideSeconds, targets.glideSeconds);
        smooth(smoothedParameters_.velocityAmount, targets.velocityAmount);
        smooth(smoothedParameters_.chorusMix, targets.chorusMix);
        smooth(smoothedParameters_.chorusRateHz, targets.chorusRateHz);
        smooth(smoothedParameters_.outputGain, targets.outputGain);
        pitchBend_ += performanceSmoothing * (pitchBendTarget_ - pitchBend_);
        modWheel_ += performanceSmoothing * (modWheelTarget_ - modWheel_);
        // One pair of square roots per host sample provides a true equal-power
        // oscillator law without repeating transcendental work per voice.
        oscillator1MixGain_ = std::sqrt(std::max(0.0f, 1.0f - smoothedParameters_.oscMix));
        oscillator2MixGain_ = std::sqrt(std::max(0.0f, smoothedParameters_.oscMix));

        const float lfoValue = nextLfoValue(smoothedParameters_);
        float dryLeft = 0.0f;
        float dryRight = 0.0f;
        if (oversampling_ == 2)
        {
            float firstLeft = 0.0f;
            float firstRight = 0.0f;
            float secondLeft = 0.0f;
            float secondRight = 0.0f;
            for (auto& voice : voices_)
            {
                if (!voice.active)
                    continue;
                const float rendered = renderVoiceOversample(
                    voice, smoothedParameters_, lfoValue);
                firstLeft += rendered * voice.panLeft;
                firstRight += rendered * voice.panRight;
            }
            float tailLeft = 0.0f;
            float tailRight = 0.0f;
            renderStealTail(tailLeft, tailRight);
            firstLeft += tailLeft;
            firstRight += tailRight;
            for (auto& voice : voices_)
            {
                if (!voice.active)
                    continue;
                const float rendered = renderVoiceOversample(
                    voice, smoothedParameters_, lfoValue);
                secondLeft += rendered * voice.panLeft;
                secondRight += rendered * voice.panRight;
            }
            renderStealTail(tailLeft, tailRight);
            secondLeft += tailLeft;
            secondRight += tailRight;
            downsampleStereo(firstLeft, firstRight, secondLeft, secondRight,
                             dryLeft, dryRight);
        }
        else
        {
            // Above 96 kHz the host rate is already high enough for the
            // nonlinear island; running exactly one internal step preserves
            // pitch and envelope timing without needless CPU work.
            for (auto& voice : voices_)
            {
                if (!voice.active)
                    continue;

                const float rendered = renderVoiceOversample(
                    voice, smoothedParameters_, lfoValue);
                dryLeft += rendered * voice.panLeft;
                dryRight += rendered * voice.panRight;
            }
            float tailLeft = 0.0f;
            float tailRight = 0.0f;
            renderStealTail(tailLeft, tailRight);
            dryLeft += tailLeft;
            dryRight += tailRight;
        }

        float chorusLeft = 0.0f;
        float chorusRight = 0.0f;
        processChorus(dryLeft, dryRight, smoothedParameters_, chorusLeft, chorusRight);
        float outputLeft = processDcBlocker(chorusLeft * smoothedParameters_.outputGain,
                                            dcInputLeft_, dcOutputLeft_);
        float outputRight = processDcBlocker(chorusRight * smoothedParameters_.outputGain,
                                             dcInputRight_, dcOutputRight_);
        outputLeft = safetyLimit(outputLeft);
        outputRight = safetyLimit(outputRight);

        if (left != nullptr)
            left[sample] = outputLeft;
        if (right != nullptr && right != left)
            right[sample] = outputRight;
        else if (right != nullptr)
            right[sample] = 0.5f * (outputLeft + outputRight);
    }

    updateActiveVoiceCount();
}

int MarsEngine::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount_;
}

} // namespace mars
