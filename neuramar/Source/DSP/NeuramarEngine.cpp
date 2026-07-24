#include "NeuramarEngine.h"

#include "AirFilterbank.h"
#include "SpectralEnvelope.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace neuramar
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

[[nodiscard]] float finiteOr(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

[[nodiscard]] float clampParameter(float value, float minimum,
                                   float maximum, float fallback) noexcept
{
    return std::clamp(finiteOr(value, fallback), minimum, maximum);
}

[[nodiscard]] float interpolatePhase(
    const std::array<float, NeuralModel::harmonicCount>& phases,
    float oneBasedIndex) noexcept
{
    if (!(oneBasedIndex >= 1.0f))
        return phases.front();
    if (oneBasedIndex >= static_cast<float>(phases.size()))
        return phases.back();

    const float zeroBased = oneBasedIndex - 1.0f;
    const auto lower = static_cast<std::size_t>(zeroBased);
    const float fraction = zeroBased - static_cast<float>(lower);
    if (!(fraction > 0.0f))
        return phases[lower];
    const float firstAngle = twoPi * phases[lower];
    const float secondAngle = twoPi * phases[lower + 1];
    const float real = (1.0f - fraction) * std::cos(firstAngle)
        + fraction * std::cos(secondAngle);
    const float imaginary = (1.0f - fraction) * std::sin(firstAngle)
        + fraction * std::sin(secondAngle);
    if (std::abs(real) + std::abs(imaginary) < 1.0e-8f)
        return phases[lower];
    float result = std::atan2(imaginary, real) / twoPi;
    result -= std::floor(result);
    return result;
}

[[nodiscard]] float interpolatePhaseShortest(float first, float second,
                                             float amount) noexcept
{
    float difference = second - first;
    difference -= std::floor(difference + 0.5f);
    float result = first + amount * difference;
    result -= std::floor(result);
    return result;
}

// The limit is 0.49 * sampleRate and the fade scale is 1 / (0.06 *
// sampleRate); both are precomputed in prepare() so the per-sample harmonic
// loop multiplies instead of dividing for every audible harmonic.
[[nodiscard]] float coreNyquistGain(float frequencyHz, float nyquistLimitHz,
                                    float nyquistFadeScale) noexcept
{
    return std::clamp((nyquistLimitHz - frequencyHz) * nyquistFadeScale,
                      0.0f, 1.0f);
}

[[nodiscard]] float guardFiniteOutput(float value) noexcept
{
    // Hosts process floating-point audio above 0 dBFS. Preserve that headroom
    // instead of waveshaping every overload at base rate, which would fold new
    // harmonics into high-register notes. This is only a pathological-state
    // guard; ordinary level control remains exactly linear.
    constexpr float emergencyMagnitude = 7.95f;
    return std::clamp(finiteOr(value, 0.0f),
                      -emergencyMagnitude, emergencyMagnitude);
}

[[nodiscard]] std::array<float, NeuralModel::harmonicCount>
makeSourceFilterEnvelope(
    const std::array<float, NeuralModel::harmonicCount>& amplitudes) noexcept
{
    // Parity-balanced power kernel: half of the total weight sits on even
    // offsets (the centre and +/-2 taps) and half on the odd +/-1 taps, so an
    // alternating odd/even excitation cancels exactly. Narrowing the shoulder
    // from the former 0.0625 to 0.015625 keeps that cancellation but blurs a
    // narrow formant over a much smaller span, which is what actually limits
    // held-out register accuracy. Reflecting at both ends preserves the parity
    // balance at the evidence boundaries instead of renormalising a truncated,
    // parity-biased weight sum.
    constexpr float shoulderWeight = 0.015625f;
    constexpr std::array<float, 5> weights {
        shoulderWeight, 0.25f, 0.5f - 2.0f * shoulderWeight, 0.25f,
        shoulderWeight
    };
    std::array<float, NeuralModel::harmonicCount> envelope {};
    constexpr auto lastIndex = static_cast<std::ptrdiff_t>(
        NeuralModel::harmonicCount) - 1;
    for (std::size_t harmonic = 0; harmonic < amplitudes.size(); ++harmonic)
    {
        double power = 0.0;
        for (int offset = -2; offset <= 2; ++offset)
        {
            auto neighbour = static_cast<std::ptrdiff_t>(harmonic)
                + static_cast<std::ptrdiff_t>(offset);
            if (neighbour < 0)
                neighbour = -neighbour;
            else if (neighbour > lastIndex)
                neighbour = 2 * lastIndex - neighbour;
            neighbour = std::clamp<std::ptrdiff_t>(neighbour, 0, lastIndex);

            const float weight = weights[static_cast<std::size_t>(offset + 2)];
            const float amplitude = std::max(
                amplitudes[static_cast<std::size_t>(neighbour)], 0.0f);
            power += static_cast<double>(weight) * amplitude * amplitude;
        }
        envelope[harmonic] = static_cast<float>(std::sqrt(std::max(power, 0.0)));
    }
    return envelope;
}

[[nodiscard]] float blendMagnitude(float first, float second,
                                   float amount) noexcept
{
    // Companded log interpolation is perceptually smooth, keeps both endpoints
    // exact (including silence), and avoids the level swell of a linear blend.
    constexpr float knee = 1.0e-5f;
    first = std::max(finiteOr(first, 0.0f), 0.0f);
    second = std::max(finiteOr(second, 0.0f), 0.0f);
    amount = std::clamp(finiteOr(amount, 0.0f), 0.0f, 1.0f);
    const float firstLog = std::log1p(first / knee);
    const float secondLog = std::log1p(second / knee);
    return knee * std::expm1(firstLog + amount * (secondLog - firstLog));
}

[[nodiscard]] float wrapUnit(float phase) noexcept
{
    phase -= std::floor(phase);
    return phase;
}
} // namespace

void NeuramarEngine::Bandpass::set(float centreHz, float bandwidthOctaves,
                                   float sampleRate, int rampSamples) noexcept
{
    // Control frames re-assert the same centre for as long as the pitch and
    // brightness stand still. Once the previous ramp has finished there is
    // nothing left to move, so the sinh/sin/cos/sqrt redesign is skipped. An
    // early re-assertion mid-ramp (a voice-pan refresh) still falls through
    // and re-anchors, exactly like a changed centre.
    if (centreHz == configuredCentreHz
        && bandwidthOctaves == configuredBandwidthOctaves
        && rampRemaining == 0)
        return;
    configuredCentreHz = centreHz;
    configuredBandwidthOctaves = bandwidthOctaves;

    const auto coefficients = airfilter::makeCoefficients(
        centreHz, bandwidthOctaves, sampleRate);
    if (rampSamples <= 0)
    {
        b0 = coefficients.b0;
        b2 = coefficients.b2;
        a1 = coefficients.a1;
        a2 = coefficients.a2;
        outputScale = coefficients.outputScale;
        b0Step = b2Step = a1Step = a2Step = outputScaleStep = 0.0f;
        rampRemaining = 0;
        return;
    }

    const float inverseRamp = 1.0f / static_cast<float>(rampSamples);
    b0Step = (coefficients.b0 - b0) * inverseRamp;
    b2Step = (coefficients.b2 - b2) * inverseRamp;
    a1Step = (coefficients.a1 - a1) * inverseRamp;
    a2Step = (coefficients.a2 - a2) * inverseRamp;
    outputScaleStep = (coefficients.outputScale - outputScale) * inverseRamp;
    rampRemaining = rampSamples;
}

float NeuramarEngine::Bandpass::tick(float input) noexcept
{
    if (rampRemaining > 0)
    {
        b0 += b0Step;
        b2 += b2Step;
        a1 += a1Step;
        a2 += a2Step;
        outputScale += outputScaleStep;
        if (--rampRemaining == 0)
            b0Step = b2Step = a1Step = a2Step = outputScaleStep = 0.0f;
    }
    const float output = b0 * input + z1;
    z1 = z2 - a1 * output;
    z2 = b2 * input - a2 * output;
    return output * outputScale;
}

void NeuramarEngine::Voice::clear() noexcept
{
    *this = Voice {};
}

NeuramarEngine::NeuramarEngine() noexcept
{
    for (int index = 0; index < sineTableSize; ++index)
        sineTable_[static_cast<std::size_t>(index)] = std::sin(
            twoPi * static_cast<float>(index) / static_cast<float>(sineTableSize));
    // Two wrapped guard entries: the first lets the final cell interpolate
    // across the phase wrap, and the second keeps the lookup in bounds for
    // wrapped phases within half an ulp of 1.0, whose scaled table position
    // rounds up to exactly sineTableSize.
    sineTable_[sineTableSize] = sineTable_.front();
    sineTable_.back() = sineTable_[1];
    for (std::size_t harmonic = 0; harmonic < NeuralModel::harmonicCount; ++harmonic)
        inverseHarmonicRolloff_[harmonic] = 1.0f
            / std::pow(static_cast<float>(harmonic + 1), 1.15f);
    refreshHarmonicStretch(0.0f);
}

void NeuramarEngine::refreshHarmonicStretch(float inharmonicity) noexcept
{
    inharmonicity = std::clamp(finiteOr(inharmonicity, 0.0f), 0.0f,
                               2.0f * NeuralModel::maximumInharmonicity);
    if (inharmonicity == cachedInharmonicity_)
        return;

    cachedInharmonicity_ = inharmonicity;
    for (std::size_t harmonic = 0; harmonic < renderedHarmonicCount; ++harmonic)
    {
        harmonicStretchRatio_[harmonic] = stretchedHarmonicRatio(
            static_cast<float>(harmonic + 1), inharmonicity);
    }
}

void NeuramarEngine::prepare(double sampleRate, int maxBlockSize)
{
    (void) maxBlockSize;
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 768000.0)
        sampleRate = 48000.0;

    sampleRate_ = sampleRate;
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    coreNyquistLimitHz_ = 0.49f * static_cast<float>(sampleRate_);
    coreNyquistFadeScale_ = static_cast<float>(
        1.0 / (0.06 * sampleRate_));
    controlPeriod_ = std::clamp(static_cast<int>(std::lround(sampleRate_ / 250.0)),
                                16, 4096);
    reset();
}

void NeuramarEngine::reset() noexcept
{
    for (auto& voice : voices_)
        voice.clear();
    for (auto& tail : fadeTails_)
        tail.clear();
    ageCounter_ = 0;
}

void NeuramarEngine::setModel(const NeuralModel* immutableModel) noexcept
{
    const auto* previous = model_.load(std::memory_order_acquire);
    if (previous == immutableModel)
        return;

    if (immutableModel == nullptr)
    {
        allSoundOff();
        model_.store(nullptr, std::memory_order_release);
        return;
    }

    for (std::size_t index = 0; index < voices_.size(); ++index)
    {
        if (voices_[index].active)
            beginFadeTail(index);
        voices_[index].clear();
    }
    model_.store(immutableModel, std::memory_order_release);
}

void NeuramarEngine::setParameters(const EngineParameters& parameters) noexcept
{
    parameters_.imprint.store(clampParameter(parameters.imprint, 0.0f, 1.0f, 1.0f),
                              std::memory_order_relaxed);
    parameters_.bodyLock.store(clampParameter(parameters.bodyLock, 0.0f, 1.0f, 0.65f),
                               std::memory_order_relaxed);
    parameters_.air.store(clampParameter(parameters.air, 0.0f, 1.0f, 0.35f),
                          std::memory_order_relaxed);
    parameters_.bone.store(clampParameter(parameters.bone, 0.0f, 1.0f, 0.30f),
                           std::memory_order_relaxed);
    parameters_.brightness.store(clampParameter(parameters.brightness, 0.0f, 1.0f, 0.50f),
                                 std::memory_order_relaxed);
    parameters_.evolutionRate.store(clampParameter(parameters.evolutionRate, 0.125f, 4.0f, 1.0f),
                                    std::memory_order_relaxed);
    parameters_.orbit.store(clampParameter(parameters.orbit, 0.0f, 1.0f, 0.15f),
                            std::memory_order_relaxed);
    parameters_.mutation.store(clampParameter(parameters.mutation, 0.0f, 1.0f, 0.10f),
                               std::memory_order_relaxed);
    parameters_.noise.store(clampParameter(parameters.noise, 0.0f, 1.0f, 0.0f),
                            std::memory_order_relaxed);
    parameters_.attackSeconds.store(clampParameter(parameters.attackSeconds, 0.0f, 10.0f, 0.0f),
                                    std::memory_order_relaxed);
    parameters_.releaseSeconds.store(clampParameter(parameters.releaseSeconds, 0.005f, 20.0f, 0.35f),
                                     std::memory_order_relaxed);
    parameters_.spread.store(clampParameter(parameters.spread, 0.0f, 1.0f, 0.35f),
                             std::memory_order_relaxed);
    parameters_.rootOffsetSemitones.store(
        clampParameter(parameters.rootOffsetSemitones, -12.0f, 12.0f, 0.0f),
        std::memory_order_relaxed);
    parameters_.outputGain.store(clampParameter(parameters.outputGain, 0.0f, 2.0f, 0.72f),
                                 std::memory_order_relaxed);
    parameters_.stretch.store(clampParameter(parameters.stretch, 0.0f, 2.0f, 1.0f),
                              std::memory_order_relaxed);
    parameters_.formantShiftSemitones.store(
        clampParameter(parameters.formantShiftSemitones, -24.0f, 24.0f, 0.0f),
        std::memory_order_relaxed);
    parameters_.touch.store(clampParameter(parameters.touch, 0.0f, 1.0f, 0.0f),
                            std::memory_order_relaxed);
}

EngineParameters NeuramarEngine::loadParameters() const noexcept
{
    EngineParameters result;
    result.imprint = parameters_.imprint.load(std::memory_order_relaxed);
    result.bodyLock = parameters_.bodyLock.load(std::memory_order_relaxed);
    result.air = parameters_.air.load(std::memory_order_relaxed);
    result.bone = parameters_.bone.load(std::memory_order_relaxed);
    result.brightness = parameters_.brightness.load(std::memory_order_relaxed);
    result.evolutionRate = parameters_.evolutionRate.load(std::memory_order_relaxed);
    result.orbit = parameters_.orbit.load(std::memory_order_relaxed);
    result.mutation = parameters_.mutation.load(std::memory_order_relaxed);
    result.noise = parameters_.noise.load(std::memory_order_relaxed);
    result.attackSeconds = parameters_.attackSeconds.load(std::memory_order_relaxed);
    result.releaseSeconds = parameters_.releaseSeconds.load(std::memory_order_relaxed);
    result.spread = parameters_.spread.load(std::memory_order_relaxed);
    result.rootOffsetSemitones = parameters_.rootOffsetSemitones.load(std::memory_order_relaxed);
    result.outputGain = parameters_.outputGain.load(std::memory_order_relaxed);
    result.stretch = parameters_.stretch.load(std::memory_order_relaxed);
    result.formantShiftSemitones = parameters_.formantShiftSemitones.load(
        std::memory_order_relaxed);
    result.touch = parameters_.touch.load(std::memory_order_relaxed);
    return result;
}

void NeuramarEngine::noteOn(int midiNote, float velocity) noexcept
{
    const auto* model = model_.load(std::memory_order_acquire);
    if (model == nullptr || midiNote < 0 || midiNote > 127)
        return;

    velocity = clampParameter(velocity, 0.0f, 1.0f, 0.0f);
    if (velocity <= 0.0f)
    {
        noteOff(midiNote);
        return;
    }

    Voice* selected = nullptr;
    std::size_t selectedIndex = 0;
    for (std::size_t index = 0; index < voices_.size(); ++index)
    {
        if (!voices_[index].active)
        {
            selected = &voices_[index];
            selectedIndex = index;
            break;
        }
    }

    if (selected == nullptr)
    {
        selectedIndex = 0;
        for (std::size_t index = 1; index < voices_.size(); ++index)
        {
            const auto& candidate = voices_[index];
            const auto& current = voices_[selectedIndex];
            if ((candidate.releasing && !current.releasing)
                || (candidate.releasing == current.releasing
                    && candidate.ageStamp < current.ageStamp))
                selectedIndex = index;
        }
        selected = &voices_[selectedIndex];
    }

    if (selected->active)
        beginFadeTail(selectedIndex);
    selected->clear();
    selected->active = true;
    selected->midiNote = midiNote;
    selected->velocity = velocity;
    selected->ageStamp = ++ageCounter_;
    selected->controlCountdown = 0;

    const auto mixHash = [](std::uint32_t value) noexcept
    {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        return value != 0u ? value : 1u;
    };
    const auto noteHash = mixHash(
        static_cast<std::uint32_t>(midiNote + 1) * 0x9e3779b9u);
    const auto voiceHash = mixHash(
        noteHash ^ static_cast<std::uint32_t>(selected->ageStamp));
    const float mutationAmount = parameters_.mutation.load(
        std::memory_order_relaxed);
    // With Mutation and model-space Noise both at zero, the same note restarts
    // every oscillator and the audible Air stream identically, independent of
    // prior voice allocation. Positive Mutation gives Air a new voice stream;
    // positive Noise independently selects a fresh latent trajectory below.
    const auto airNoiseHash = mutationAmount <= 0.0f ? noteHash : voiceHash;
    for (std::size_t band = 0; band < selected->airNoiseStates.size(); ++band)
    {
        selected->airNoiseStates[band] = mixHash(
            airNoiseHash
                ^ (0x9e3779b9u * static_cast<std::uint32_t>(band + 1)));
    }
    selected->mutationOffset = 2.0f
        * static_cast<float>(voiceHash & 0xffffu) / 65535.0f - 1.0f;
    selected->velocityGain = velocity * (0.72f + 0.28f * std::sqrt(velocity));
    // These sinusoidal variation shapes depend only on the voice identity, so
    // one evaluation at note-on replaces the same 72 sines every control frame
    // in the reference-target and Air paths.
    for (std::size_t harmonic = 0; harmonic < NeuralModel::harmonicCount; ++harmonic)
        selected->harmonicVariationSin[harmonic] = std::sin(
            2.173f * static_cast<float>(harmonic + 1)
            + 3.7f * selected->mutationOffset);
    for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
        selected->airVariationSin[band] = std::sin(
            1.37f * static_cast<float>(band + 1) + 5.0f * selected->mutationOffset);

    // Model-space NOISE owns a separate deterministic domain. It neither
    // consumes the Air PRNG nor reuses Mutation's static scalar.
    const auto latentPhaseAHash = mixHash(voiceHash ^ 0xa511e9b3u);
    const auto latentPhaseBHash = mixHash(voiceHash ^ 0x63d83595u);
    const auto latentRateAHash = mixHash(voiceHash ^ 0xc2b2ae35u);
    const auto latentRateBHash = mixHash(voiceHash ^ 0x27d4eb2fu);
    constexpr float inverseU32 = 1.0f / 4294967295.0f;
    selected->latentPhaseA = static_cast<float>(latentPhaseAHash) * inverseU32;
    selected->latentPhaseB = static_cast<float>(latentPhaseBHash) * inverseU32;
    selected->latentRateAHertz = 0.07f
        + 0.10f * static_cast<float>(latentRateAHash) * inverseU32;
    selected->latentRateBHertz = 0.19f
        + 0.18f * static_cast<float>(latentRateBHash) * inverseU32;
    // Pan is assigned from the currently sounding set below. A single voice is
    // always centred, while chords occupy a symmetric horizon without making
    // the result depend on how many notes were played earlier.
    selected->pan = 0.0f;

    const float phaseMutation = mutationAmount * selected->mutationOffset;
    const float correctedRootMidi = static_cast<float>(model->metadata_.rootMidiNote)
        + parameters_.rootOffsetSemitones.load(std::memory_order_relaxed);
    const float phaseRatio = std::clamp(
        std::exp2((static_cast<float>(midiNote) - correctedRootMidi) / 12.0f)
            * (1.0f + 0.0012f * phaseMutation),
        0.0078125f, 128.0f);
    const float bodyLock = parameters_.bodyLock.load(std::memory_order_relaxed);
    for (std::size_t harmonic = 0;
         harmonic < selected->harmonicPhases.size(); ++harmonic)
    {
        const float harmonicNumber = static_cast<float>(harmonic + 1);
        const bool hasPitchFollowingPhase = harmonic < NeuralModel::harmonicCount;
        const float pitchFollowingPhase = hasPitchFollowingPhase
            ? model->initialHarmonicPhases_[harmonic]
            : interpolatePhase(model->initialHarmonicPhases_,
                               harmonicNumber * phaseRatio);
        const float sourceCoordinate = harmonicNumber * phaseRatio;
        const bool hasBodyPhase = sourceCoordinate > 0.0f
            && sourceCoordinate
                <= static_cast<float>(NeuralModel::harmonicCount) + 0.999f;
        const float bodyPhase = hasBodyPhase
            ? interpolatePhase(model->initialHarmonicPhases_, sourceCoordinate)
            : pitchFollowingPhase;
        selected->harmonicPhases[harmonic] = wrapUnit(
            interpolatePhaseShortest(pitchFollowingPhase, bodyPhase, bodyLock)
            + 0.003f * phaseMutation * static_cast<float>(harmonic));
    }
    for (std::size_t mode = 0; mode < NeuralModel::boneModeCount; ++mode)
        selected->bonePhases[mode] = wrapUnit(
            model->initialBonePhases_[mode] + 0.007f * phaseMutation);
    refreshVoicePans();
}

void NeuramarEngine::noteOff(int midiNote) noexcept
{
    Voice* oldestHeld = nullptr;
    for (auto& voice : voices_)
    {
        if (!voice.active || voice.releasing || voice.midiNote != midiNote)
            continue;
        if (oldestHeld == nullptr || voice.ageStamp < oldestHeld->ageStamp)
            oldestHeld = &voice;
    }
    if (oldestHeld != nullptr)
        oldestHeld->releasing = true;
}

void NeuramarEngine::allNotesOff() noexcept
{
    for (auto& voice : voices_)
        if (voice.active)
            voice.releasing = true;
}

void NeuramarEngine::allSoundOff() noexcept
{
    for (auto& voice : voices_)
        voice.clear();
    for (auto& tail : fadeTails_)
        tail.clear();
}

void NeuramarEngine::beginFadeTail(std::size_t voiceIndex) noexcept
{
    if (voiceIndex >= voices_.size())
        return;

    const auto& voice = voices_[voiceIndex];
    auto& tail = fadeTails_[voiceIndex];
    const int fadeSamples = std::max(16, static_cast<int>(std::lround(
        0.003 * sampleRate_)));
    tail.left = voice.lastLeft;
    tail.right = voice.lastRight;
    tail.gain = 1.0f;
    tail.gainStep = 1.0f / static_cast<float>(fadeSamples);
    tail.remaining = fadeSamples;
}

void NeuramarEngine::refreshVoicePans() noexcept
{
    std::array<Voice*, maximumVoices> ordered {};
    std::size_t count = 0;
    for (auto& voice : voices_)
        if (voice.active)
            ordered[count++] = &voice;
    // voices_ has exactly maximumVoices elements, so count can never exceed
    // ordered.size(); this clamp only gives the optimizer a bound it can
    // prove, silencing a GCC -Warray-bounds false positive in std::sort's
    // unreachable large-range branch for this fixed 8-element array.
    count = std::min(count, ordered.size());

    std::sort(ordered.begin(), ordered.begin() + static_cast<std::ptrdiff_t>(count),
              [](const Voice* left, const Voice* right)
              {
                  return left->ageStamp < right->ageStamp;
              });

    for (std::size_t rank = 0; rank < count; ++rank)
    {
        ordered[rank]->pan = count == 1
            ? 0.0f
            : -1.0f + 2.0f * static_cast<float>(rank)
                / static_cast<float>(count - 1);
        // Recalculate constant-power gains on the next sample rather than
        // waiting up to one control interval after a chord changes shape.
        ordered[rank]->controlCountdown = 0;
    }
}

float NeuramarEngine::sine(float phase) const noexcept
{
    phase = wrapUnit(phase);
    const float tablePosition = phase * static_cast<float>(sineTableSize);
    const auto lower = static_cast<std::size_t>(tablePosition);
    const float fraction = tablePosition - static_cast<float>(lower);
    return sineTable_[lower]
        + fraction * (sineTable_[lower + 1] - sineTable_[lower]);
}

float NeuramarEngine::nextNoise(std::uint32_t& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state) * (2.0f / 4294967295.0f) - 1.0f;
}

void NeuramarEngine::updateVoiceControl(Voice& voice, const NeuralModel& model,
                                        const EngineParameters& parameters) noexcept
{
    const bool firstControlFrame = voice.modelTimeSeconds <= 0.0;
    const float correctedRootMidi = static_cast<float>(model.metadata_.rootMidiNote)
        + parameters.rootOffsetSemitones;
    const float noteDistance = static_cast<float>(voice.midiNote) - correctedRootMidi;
    const float basicRatio = std::exp2(noteDistance / 12.0f);
    const float mutationDetune = 1.0f + 0.0012f * parameters.mutation
        * voice.mutationOffset;
    voice.transpositionRatio = std::clamp(basicRatio * mutationDetune,
                                          0.0078125f, 128.0f);

    const float duration = std::max(model.metadata_.durationSeconds, 0.001f);
    // Except for the first sounding sample, predict one control interval
    // ahead. The per-sample ramp then arrives at that target at the time it
    // describes instead of reproducing every learned gesture 4 ms late.
    const double evaluationModelTime = voice.modelTimeSeconds
        + (firstControlFrame ? 0.0
            : static_cast<double>(parameters.evolutionRate
                * static_cast<float>(controlPeriod_ - 1)) / sampleRate_);
    const float loopStart = std::clamp(model.metadata_.loopStartSeconds,
                                       0.0f, duration);
    const float loopEnd = std::clamp(model.metadata_.loopEndSeconds,
                                     loopStart + 0.001f, duration);
    const float loopLength = std::max(loopEnd - loopStart, 0.001f);
    const double oneShotTime = std::min(evaluationModelTime,
                                        static_cast<double>(duration));
    double orbitTime = evaluationModelTime;
    if (orbitTime > loopStart)
    {
        const double cycle = std::fmod(orbitTime - loopStart,
                                       2.0 * static_cast<double>(loopLength));
        orbitTime = loopStart + (cycle <= loopLength
            ? cycle : 2.0 * static_cast<double>(loopLength) - cycle);
    }
    const float effectiveTime = static_cast<float>(std::clamp(
        oneShotTime + parameters.orbit * (orbitTime - oneShotTime)
            + parameters.mutation * voice.mutationOffset * 0.018 * duration,
        0.0, static_cast<double>(duration)));

    SynthesisFrame frame;
    const float normalisedTime = effectiveTime / duration;
    if (parameters.noise > 0.0f)
    {
        const std::uint64_t predictedSample = voice.renderedSampleCount
            + static_cast<std::uint64_t>(
                firstControlFrame ? 0 : controlPeriod_ - 1);
        const double latentTimeSeconds = static_cast<double>(predictedSample)
            / sampleRate_;
        const float phaseA = static_cast<float>(std::fmod(
            static_cast<double>(voice.latentPhaseA)
                + static_cast<double>(voice.latentRateAHertz)
                    * latentTimeSeconds,
            1.0));
        const float phaseB = static_cast<float>(std::fmod(
            static_cast<double>(voice.latentPhaseB)
                + static_cast<double>(voice.latentRateBHertz)
                    * latentTimeSeconds,
            1.0));
        const float latentCoordinate = parameters.noise * std::clamp(
            0.62f * sine(phaseA) + 0.38f * sine(phaseB),
            -1.0f, 1.0f);
        model.evaluate(normalisedTime, latentCoordinate, frame);
    }
    else
    {
        model.evaluate(normalisedTime, frame);
    }

    // Touch turns MIDI velocity into an excitation strength rather than a
    // volume knob: a harder note leans the harmonic tilt brighter and lets
    // more of the learned Air through, exactly as a stronger physical
    // excitation would. 0.72 is the reference "mezzo-forte" velocity, so the
    // learned timbre is reproduced unchanged there for any Touch depth.
    constexpr float touchReferenceVelocity = 0.72f;
    const float touchOffset = voice.velocity - touchReferenceVelocity;
    const float touchTilt = parameters.touch * 0.55f * touchOffset;
    const float touchAirGain = std::clamp(
        1.0f + parameters.touch * 1.35f * touchOffset, 0.0f, 3.0f);

    const float brightnessTilt = (parameters.brightness - 0.5f) * 1.2f
        + touchTilt;
    const float characteristic = parameters.imprint;
    // Formant moves the learned resonant body in absolute frequency without
    // moving the played pitch. Dividing the envelope lookup coordinate by the
    // shift is what reads the learned envelope one shift lower, which places
    // its resonances one shift higher in the render.
    const float formantScale = std::exp2(
        parameters.formantShiftSemitones * (1.0f / 12.0f));
    const float envelopeRatio = std::clamp(
        voice.transpositionRatio / formantScale, 0.0078125f, 512.0f);
    const bool needsBodyEnvelope = parameters.bodyLock > 0.0f
        && characteristic > 0.0f;
    spectral::ShapePreservingEnvelope<NeuralModel::harmonicCount> bodyEnvelope;
    std::array<float, NeuralModel::harmonicCount> sourceFilterEnvelope {};
    if (needsBodyEnvelope)
    {
        sourceFilterEnvelope = makeSourceFilterEnvelope(
            frame.harmonicAmplitudes);
        bodyEnvelope.prepare(sourceFilterEnvelope);
    }
    const float fundamentalReference = std::max(frame.harmonicAmplitudes.front(),
                                                1.0e-5f);

    // The reference targets always evaluate the brightness tilt at integer
    // harmonic numbers, so the power curve is rebuilt only when Brightness
    // actually moves instead of on every control frame.
    if (brightnessTilt != voice.cachedBrightnessTilt)
    {
        voice.cachedBrightnessTilt = brightnessTilt;
        for (std::size_t harmonic = 0; harmonic < renderedHarmonicCount; ++harmonic)
            voice.brightnessTiltTable[harmonic] = std::pow(
                static_cast<float>(harmonic + 1), brightnessTilt);
    }

    const float availableMappedHarmonics =
        (static_cast<float>(NeuralModel::harmonicCount) + 0.999f)
        / envelopeRatio;
    // Retire a previous contraction only after its slots are actually silent.
    // Voice-pan refreshes can request an early control update, so the presence
    // of another update alone does not prove a full ramp period elapsed.
    bool retiringHarmonicsAreSilent = true;
    for (std::size_t harmonic = voice.targetHarmonicCount;
         harmonic < voice.activeHarmonicCount; ++harmonic)
    {
        retiringHarmonicsAreSilent = retiringHarmonicsAreSilent
            && voice.amplitudes[harmonic] <= 1.0e-6f;
    }
    if (!firstControlFrame && retiringHarmonicsAreSilent
        && voice.targetHarmonicCount < voice.activeHarmonicCount)
        voice.activeHarmonicCount = voice.targetHarmonicCount;

    std::size_t desiredHarmonicCount = NeuralModel::harmonicCount;
    if (availableMappedHarmonics
        > static_cast<float>(NeuralModel::harmonicCount))
    {
        desiredHarmonicCount = std::min(
            renderedHarmonicCount,
            static_cast<std::size_t>(std::floor(availableMappedHarmonics)));
    }
    voice.targetHarmonicCount = desiredHarmonicCount;
    if (desiredHarmonicCount > voice.activeHarmonicCount)
    {
        if (!firstControlFrame)
        {
            std::fill(voice.amplitudes.begin()
                          + static_cast<std::ptrdiff_t>(voice.activeHarmonicCount),
                      voice.amplitudes.begin()
                          + static_cast<std::ptrdiff_t>(desiredHarmonicCount),
                      0.0f);
        }
        voice.activeHarmonicCount = desiredHarmonicCount;
    }

    std::array<float, renderAmplitudeCount> targets {};
    std::array<float, NeuralModel::harmonicCount> referenceTargets {};
    const bool capacityLimited = desiredHarmonicCount
            == renderedHarmonicCount
        && envelopeRatio
                * static_cast<float>(renderedHarmonicCount)
            < static_cast<float>(NeuralModel::harmonicCount - 1);
    // The Body-Locked spectral coordinate is always a fixed multiple of the
    // harmonic index, so pow(index * scale, tilt) factors into one cached
    // table lookup and one scalar. That removes a pow() and a sin() from every
    // rendered harmonic of every control frame.
    const float bodyCoordinateScale = 1.0f
        + parameters.bodyLock * (envelopeRatio - 1.0f);
    const float evidenceTiltScale = std::pow(
        std::max(bodyCoordinateScale, 1.0e-6f), brightnessTilt);
    const bool harmonicsVary = parameters.mutation > 0.0f;
    const float variationDepth = parameters.mutation * 0.045f;
    constexpr float inverseTwoPi = 1.0f / twoPi;
    const float renderedFundamental = model.metadata_.rootFrequencyHz
        * voice.transpositionRatio * frame.pitchRatio;
    double renderedPower = 0.0;
    for (std::size_t harmonic = 0;
         harmonic < desiredHarmonicCount; ++harmonic)
    {
        const float harmonicNumber = static_cast<float>(harmonic + 1);
        const bool hasPitchFollowing = harmonic < NeuralModel::harmonicCount;
        const float pitchFollowing = hasPitchFollowing
            ? frame.harmonicAmplitudes[harmonic] : 0.0f;
        const float sourceCoordinate = harmonicNumber * envelopeRatio;
        const bool hasBodyEvidence = sourceCoordinate > 0.0f
            && sourceCoordinate
                <= static_cast<float>(NeuralModel::harmonicCount) + 0.999f;
        float excitationResidual = 1.0f;
        if (hasPitchFollowing && needsBodyEnvelope)
        {
            const float envelopeAtHarmonic = sourceFilterEnvelope[harmonic];
            excitationResidual = envelopeAtHarmonic > 1.0e-7f
                ? pitchFollowing / envelopeAtHarmonic : 0.0f;
            excitationResidual = std::clamp(excitationResidual, 0.0f, 4.0f);
        }
        float bodyLocked = needsBodyEnvelope
            ? excitationResidual * bodyEnvelope.sample(sourceCoordinate)
            : pitchFollowing;
        // Keep a modest pitch anchor when a fixed-frequency lookup would
        // otherwise erase the played fundamental. Other coordinates above the
        // observed envelope fade out instead of inventing upper-band energy.
        if (harmonic == 0)
            bodyLocked = std::max(bodyLocked, 0.25f * pitchFollowing);
        // The excitation/filter factorisation keeps odd/even and reed/bow
        // character attached to harmonic index while moving only the smooth
        // resonant envelope in absolute frequency.  For virtual low-register
        // harmonics no excitation evidence exists, so use the neutral envelope
        // rather than fabricating a repeated residual pattern.
        const float learned = hasPitchFollowing
            ? blendMagnitude(pitchFollowing, bodyLocked, parameters.bodyLock)
            : parameters.bodyLock * bodyLocked;
        const float neutral = hasPitchFollowing
            ? fundamentalReference * inverseHarmonicRolloff_[harmonic]
            : 0.0f;
        const float spectralCoordinate = hasBodyEvidence
            ? harmonicNumber * bodyCoordinateScale : harmonicNumber;
        const float tilt = spectralCoordinate < 1.0f
            ? 1.0f
            : (hasBodyEvidence ? evidenceTiltScale : 1.0f)
                * voice.brightnessTiltTable[harmonic];
        const float variation = harmonicsVary
            ? 1.0f + variationDepth * sine(
                  (2.173f * spectralCoordinate
                   + 3.7f * voice.mutationOffset) * inverseTwoPi)
            : 1.0f;
        float target = std::clamp(
            (neutral + characteristic * (learned - neutral)) * tilt * variation,
            0.0f, 2.0f);
        if (capacityLimited
            && harmonicNumber
                > static_cast<float>(renderedHarmonicCount - 12))
        {
            const float position = (harmonicNumber
                - static_cast<float>(renderedHarmonicCount - 12)) / 12.0f;
            target *= 0.5f + 0.5f * std::cos(pi * position);
        }
        // Fold the anti-alias taper into the ramped amplitude. The audio loop
        // then needs no per-sample band-limiting arithmetic at all, and every
        // harmonic that the taper silences drops out of the render entirely.
        target *= coreNyquistGain(
            renderedFundamental * harmonicStretchRatio_[harmonic],
            coreNyquistLimitHz_, coreNyquistFadeScale_);
        targets[harmonic] = target;
        renderedPower += static_cast<double>(target) * target;

        if (hasPitchFollowing)
        {
            const float referenceNeutral = fundamentalReference
                * inverseHarmonicRolloff_[harmonic];
            const float referenceTilt = voice.brightnessTiltTable[harmonic];
            const float referenceVariation = 1.0f
                + parameters.mutation * 0.045f
                    * voice.harmonicVariationSin[harmonic];
            referenceTargets[harmonic] = std::clamp(
                (referenceNeutral + characteristic
                    * (pitchFollowing - referenceNeutral))
                    * referenceTilt * referenceVariation,
                0.0f, 2.0f);
        }
    }

    // Factor register-dependent harmonic shape from overall Core power. This
    // compensates both the denser fixed-envelope grid on low notes and the
    // harmonics safely removed near Nyquist on high notes. The upper bound is
    // deliberately modest: unavailable spectrum must never become a huge gain
    // boost, and all changes continue to use the normal control-rate ramp.
    double referencePower = 0.0;
    const float referenceFundamental = model.metadata_.rootFrequencyHz
        * frame.pitchRatio;
    for (std::size_t harmonic = 0;
         harmonic < referenceTargets.size(); ++harmonic)
    {
        const float antialias = coreNyquistGain(
            referenceFundamental * harmonicStretchRatio_[harmonic],
            coreNyquistLimitHz_, coreNyquistFadeScale_);
        const double amplitude = referenceTargets[harmonic] * antialias;
        referencePower += amplitude * amplitude;
    }
    float registerGain = 1.0f;
    if (referencePower > 1.0e-12 && renderedPower > 1.0e-12)
    {
        registerGain = std::clamp(static_cast<float>(std::sqrt(
            referencePower / renderedPower)), 0.50f, 1.5848932f);
    }
    for (std::size_t harmonic = 0;
         harmonic < desiredHarmonicCount; ++harmonic)
        targets[harmonic] *= registerGain;

    // Air and Bone are resonant body layers, so the Formant shift moves them
    // in absolute frequency together with the Body-Locked Core envelope.
    const float spectralScale = std::pow(
        voice.transpositionRatio * frame.pitchRatio,
        1.0f - parameters.bodyLock) * formantScale;
    const float brightnessScale = std::exp2(parameters.brightness - 0.5f);
    for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
    {
        const std::size_t output = airOutputOffset + band;
        const float variation = 1.0f + parameters.mutation * 0.12f
            * voice.airVariationSin[band];
        const float desiredFrequency = model.airCentreFrequenciesHz_[band]
            * spectralScale * brightnessScale;
        const float edgeGain = std::clamp(
            (desiredFrequency - 20.0f) / 30.0f, 0.0f, 1.0f)
            * std::clamp((0.45f * static_cast<float>(sampleRate_)
                          - desiredFrequency)
                             / (0.07f * static_cast<float>(sampleRate_)),
                         0.0f, 1.0f);
        targets[output] = std::clamp(frame.airAmplitudes[band]
            * parameters.air * variation * edgeGain * touchAirGain,
            0.0f, 2.0f);

        voice.airFilters[band].set(
            desiredFrequency,
            model.airBandwidthOctaves_[band], static_cast<float>(sampleRate_),
            firstControlFrame ? 0 : controlPeriod_);
    }

    for (std::size_t mode = 0; mode < NeuralModel::boneModeCount; ++mode)
    {
        const std::size_t output = boneOutputOffset + mode;
        const float active = model.boneModeReliabilities_[mode] > 0.0f
            ? 1.0f : 0.0f;
        const float desiredFrequency = model.metadata_.rootFrequencyHz
            * model.boneFrequencyRatios_[mode] * spectralScale;
        const float edgeGain = std::clamp(
            (desiredFrequency - 10.0f) / 20.0f, 0.0f, 1.0f)
            * std::clamp((0.49f * static_cast<float>(sampleRate_)
                          - desiredFrequency)
                             / (0.04f * static_cast<float>(sampleRate_)),
                         0.0f, 1.0f);
        targets[output] = std::clamp(frame.boneAmplitudes[mode]
            * active * parameters.bone * edgeGain, 0.0f, 2.0f);
        const float boundedFrequency = std::clamp(
            desiredFrequency,
            10.0f, 0.49f * static_cast<float>(sampleRate_));
        if (firstControlFrame)
        {
            voice.boneFrequenciesHz[mode] = boundedFrequency;
            voice.boneFrequencySteps[mode] = 0.0f;
        }
        else
        {
            voice.boneFrequencySteps[mode] =
                (boundedFrequency - voice.boneFrequenciesHz[mode])
                / static_cast<float>(controlPeriod_);
        }
    }

    // Everything above the last harmonic that is audible now, or becomes
    // audible during the coming ramp, contributes exactly zero for the whole
    // control period. Recording that boundary lets the audio loop stop there.
    std::size_t soundingHarmonics = 0;
    for (std::size_t harmonic = 0;
         harmonic < voice.activeHarmonicCount; ++harmonic)
    {
        if (targets[harmonic] > 0.0f || voice.amplitudes[harmonic] > 0.0f)
            soundingHarmonics = harmonic + 1;
    }
    voice.soundingHarmonicCount = soundingHarmonics;

    for (std::size_t output = 0; output < renderAmplitudeCount; ++output)
    {
        if (firstControlFrame)
        {
            // No previous sounding sample exists to smooth from. Installing
            // the learned onset immediately avoids adding an arbitrary
            // control-period attack in front of the model's own trajectory.
            voice.amplitudes[output] = targets[output];
            voice.amplitudeSteps[output] = 0.0f;
        }
        else
        {
            voice.amplitudeSteps[output] =
                (targets[output] - voice.amplitudes[output])
                / static_cast<float>(controlPeriod_);
        }
    }
    if (firstControlFrame)
    {
        voice.pitchRatio = frame.pitchRatio;
        voice.pitchRatioStep = 0.0f;
    }
    else
    {
        voice.pitchRatioStep = (frame.pitchRatio - voice.pitchRatio)
            / static_cast<float>(controlPeriod_);
    }

    const float pan = std::clamp(voice.pan * parameters.spread
        + 0.08f * parameters.mutation * voice.mutationOffset, -1.0f, 1.0f);
    voice.panLeft = std::sqrt(0.5f * (1.0f - pan));
    voice.panRight = std::sqrt(0.5f * (1.0f + pan));
}

void NeuramarEngine::process(float* left, float* right, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    const auto* model = model_.load(std::memory_order_acquire);
    const EngineParameters parameters = loadParameters();
    // The stiff-string ratio table is shared by every voice and only depends
    // on the model coefficient and the Stretch control, so it is rebuilt at
    // most once per block and usually never. No allocation is involved.
    refreshHarmonicStretch(model != nullptr
        ? model->inharmonicity() * parameters.stretch : 0.0f);
    const float attackCoefficient = parameters.attackSeconds <= 0.0f
        ? 1.0f
        : 1.0f - std::exp(-inverseSampleRate_ / parameters.attackSeconds);
    constexpr float retirementLevel = 1.0e-5f;
    const float releaseMultiplier = std::exp(
        std::log(retirementLevel) * inverseSampleRate_
        / std::max(parameters.releaseSeconds, 0.005f));

    // With no model, no sounding voice, and no click-fade remnant, every
    // output sample is exactly zero: skip the loop and write silence.
    bool anythingSounding = model != nullptr
        && std::any_of(voices_.begin(), voices_.end(),
                       [](const Voice& voice) noexcept { return voice.active; });
    for (const auto& tail : fadeTails_)
        anythingSounding = anythingSounding || tail.remaining > 0;
    if (!anythingSounding)
    {
        if (left != nullptr)
            std::fill(left, left + numSamples, 0.0f);
        if (right != nullptr && right != left)
            std::fill(right, right + numSamples, 0.0f);
        return;
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float outputLeft = 0.0f;
        float outputRight = 0.0f;
        bool voiceRetired = false;

        if (model != nullptr)
        {
            for (auto& voice : voices_)
            {
                if (!voice.active)
                    continue;

                if (voice.controlCountdown <= 0)
                {
                    const bool firstControlFrame = voice.modelTimeSeconds <= 0.0;
                    updateVoiceControl(voice, *model, parameters);
                    // Let the first sample install the exact onset, then begin
                    // forward interpolation on the following sample.
                    voice.controlCountdown = firstControlFrame
                        ? 1 : controlPeriod_;
                }

                const auto advanceAmplitude = [&voice](std::size_t output) noexcept
                {
                    voice.amplitudes[output] = std::max(0.0f,
                        voice.amplitudes[output] + voice.amplitudeSteps[output]);
                };
                const std::size_t soundingHarmonics
                    = voice.soundingHarmonicCount;
                for (std::size_t harmonic = 0;
                     harmonic < soundingHarmonics; ++harmonic)
                    advanceAmplitude(harmonic);
                for (std::size_t output = airOutputOffset;
                     output < renderAmplitudeCount; ++output)
                    advanceAmplitude(output);
                voice.pitchRatio = std::clamp(
                    voice.pitchRatio + voice.pitchRatioStep, 0.75f, 1.35f);
                --voice.controlCountdown;

                if (voice.releasing)
                    voice.envelope *= releaseMultiplier;
                else
                    voice.envelope += attackCoefficient * (1.0f - voice.envelope);

                const float fundamentalHz = model->metadata_.rootFrequencyHz
                    * voice.transpositionRatio * voice.pitchRatio;
                const float phaseStep = fundamentalHz * inverseSampleRate_;
                float coreSample = 0.0f;
                // The band-limiting taper already lives in the ramped
                // amplitude, and every stored phase is already reduced to
                // [0, 1), so the inner loop is one table lookup, one
                // multiply-add and one wrap per audible partial.
                for (std::size_t harmonic = 0;
                     harmonic < soundingHarmonics; ++harmonic)
                {
                    const float phase = voice.harmonicPhases[harmonic];
                    coreSample += voice.amplitudes[harmonic] * sineUnit(phase);
                    const float advanced = phase
                        + phaseStep * harmonicStretchRatio_[harmonic];
                    voice.harmonicPhases[harmonic] = advanced
                        - std::floor(advanced);
                }

                float airSample = 0.0f;
                for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
                {
                    const std::size_t output = airOutputOffset + band;
                    airSample += voice.amplitudes[output]
                        * voice.airFilters[band].tick(
                            nextNoise(voice.airNoiseStates[band]));
                }

                float boneSample = 0.0f;
                for (std::size_t mode = 0; mode < NeuralModel::boneModeCount; ++mode)
                {
                    const std::size_t output = boneOutputOffset + mode;
                    boneSample += voice.amplitudes[output]
                        * sine(voice.bonePhases[mode]);
                    voice.boneFrequenciesHz[mode] = std::clamp(
                        voice.boneFrequenciesHz[mode]
                            + voice.boneFrequencySteps[mode],
                        10.0f, 0.49f * static_cast<float>(sampleRate_));
                    voice.bonePhases[mode] = wrapUnit(voice.bonePhases[mode]
                        + voice.boneFrequenciesHz[mode] * inverseSampleRate_);
                }

                const float voiceSample = voice.envelope * voice.velocityGain
                    * (coreSample + airSample + boneSample);
                voice.lastLeft = voiceSample * voice.panLeft;
                voice.lastRight = voiceSample * voice.panRight;
                outputLeft += voice.lastLeft;
                outputRight += voice.lastRight;
                voice.modelTimeSeconds += static_cast<double>(parameters.evolutionRate)
                    / sampleRate_;
                ++voice.renderedSampleCount;

                if (voice.releasing && voice.envelope <= retirementLevel)
                {
                    voice.clear();
                    voiceRetired = true;
                }
            }
        }

        if (voiceRetired)
            refreshVoicePans();

        for (auto& tail : fadeTails_)
        {
            if (tail.remaining <= 0)
                continue;
            outputLeft += tail.left * tail.gain;
            outputRight += tail.right * tail.gain;
            tail.gain = std::max(0.0f, tail.gain - tail.gainStep);
            if (--tail.remaining <= 0)
                tail.clear();
        }

        const auto finishOutput = [gain = parameters.outputGain](float value) noexcept
        {
            return guardFiniteOutput(gain * value);
        };
        outputLeft = finishOutput(outputLeft);
        outputRight = finishOutput(outputRight);

        if (left != nullptr)
            left[sample] = outputLeft;
        if (right != nullptr && right != left)
            right[sample] = outputRight;
        else if (right != nullptr)
            right[sample] = 0.5f * (outputLeft + outputRight);
    }
}

int NeuramarEngine::getActiveVoiceCount() const noexcept
{
    int result = 0;
    for (const auto& voice : voices_)
        result += voice.active ? 1 : 0;
    return result;
}

} // namespace neuramar
