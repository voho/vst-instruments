#include "DrumEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace drumalor
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;
constexpr float minusSixtyDb = -6.90775527898f;
constexpr float minusOneHundredDb = -11.51292546497f;
constexpr float referenceSampleRate = 48000.0f;
constexpr float silenceThreshold = 1.0e-5f;
// Long enough that a -24 semitone kick (13 Hz at the end of its sweep) cannot
// look quiet merely because the meter spans a zero crossing.
constexpr float peakReleaseSeconds = 0.030f;
constexpr float retirementFadeSeconds = 0.003f;
constexpr float forcedFadeSeconds = 0.005f;
// The slowest oscillator is a -24 semitone Kick at 13 Hz. Requiring more
// than its 38.5 ms absolute-peak spacing prevents zero crossings from being
// mistaken for a completed tail.
constexpr float naturalQuietHoldSeconds = 0.045f;

constexpr std::array<InstrumentMetadata, instrumentCount> metadata {{
    { Instrument::Kick,      "Kick",       "kick",       36, "Punch",   "Drive",      { 0.68f, 0.42f, 0.0f, 0.55f } },
    { Instrument::Snare,     "Snare",      "snare",      38, "Wires",   "Snap",       { 0.62f, 0.64f, 0.0f, 0.48f } },
    { Instrument::Clap,      "Clap",       "clap",       39, "Spread",  "Tone",       { 0.48f, 0.62f, 0.0f, 0.45f } },
    { Instrument::ClosedHat, "Closed Hat", "closedHat", 42, "Metal",   "Tone",       { 0.58f, 0.70f, 0.0f, 0.30f } },
    { Instrument::OpenHat,   "Open Hat",   "openHat",   46, "Metal",   "Tone",       { 0.62f, 0.68f, 0.0f, 0.55f } },
    { Instrument::Ride,      "Ride",       "ride",       51, "Bell",    "Tone",       { 0.45f, 0.62f, 0.0f, 0.62f } },
    { Instrument::Crash,     "Crash",      "crash",      49, "Spread",  "Brightness", { 0.58f, 0.65f, 0.0f, 0.65f } },
    { Instrument::LowTom,    "Low Tom",    "lowTom",     45, "Punch",   "Skin",       { 0.55f, 0.40f, 0.0f, 0.60f } },
    { Instrument::MidTom,    "Mid Tom",    "midTom",     47, "Punch",   "Skin",       { 0.55f, 0.45f, 0.0f, 0.52f } },
    { Instrument::HighTom,   "High Tom",   "highTom",    50, "Punch",   "Skin",       { 0.50f, 0.50f, 0.0f, 0.45f } },
    { Instrument::Shaker,    "Shaker",     "shaker",     82, "Density", "Color",      { 0.62f, 0.62f, 0.0f, 0.45f } },
    { Instrument::Perc1,     "Perc 1",     "perc1",      56, "Ratio",   "Drive",      { 0.50f, 0.45f, 0.0f, 0.45f } },
    { Instrument::Perc2,     "Perc 2",     "perc2",      75, "Hollow",  "Click",      { 0.55f, 0.55f, 0.0f, 0.40f } },
}};

constexpr std::array<float, instrumentCount> minimumDecay {{
    0.07f, 0.06f, 0.08f, 0.018f, 0.12f, 0.25f, 0.30f,
    0.10f, 0.08f, 0.06f, 0.05f, 0.05f, 0.025f
}};

constexpr std::array<float, instrumentCount> maximumDecay {{
    2.40f, 1.80f, 1.80f, 0.32f, 3.20f, 6.00f, 7.00f,
    2.40f, 1.80f, 1.40f, 1.20f, 2.00f, 1.10f
}};

float clampUnit (float value, float fallback = 0.5f) noexcept
{
    return std::isfinite (value) ? std::clamp (value, 0.0f, 1.0f) : fallback;
}

float coefficientForTime (float seconds, float sampleRate) noexcept
{
    return std::exp (minusSixtyDb / std::max (1.0f, seconds * sampleRate));
}

float constantPowerLeft (float pan) noexcept
{
    return std::sqrt (0.5f * (1.0f - std::clamp (pan, -1.0f, 1.0f)));
}

float constantPowerRight (float pan) noexcept
{
    return std::sqrt (0.5f * (1.0f + std::clamp (pan, -1.0f, 1.0f)));
}
} // namespace

const InstrumentMetadata& getInstrumentMetadata (Instrument instrument) noexcept
{
    const auto index = static_cast<std::size_t> (instrument);
    return metadata[index < metadata.size() ? index : 0u];
}

std::string_view getInstrumentDisplayName (Instrument instrument) noexcept
{
    return getInstrumentMetadata (instrument).displayName;
}

std::string_view getInstrumentSlug (Instrument instrument) noexcept
{
    return getInstrumentMetadata (instrument).slug;
}

int getStandardMidiNote (Instrument instrument) noexcept
{
    return getInstrumentMetadata (instrument).standardMidiNote;
}

std::string_view getCharacterALabel (Instrument instrument) noexcept
{
    return getInstrumentMetadata (instrument).characterALabel;
}

std::string_view getCharacterBLabel (Instrument instrument) noexcept
{
    return getInstrumentMetadata (instrument).characterBLabel;
}

std::optional<Instrument> instrumentForMidiNote (int midiNote) noexcept
{
    switch (midiNote)
    {
        case 35: case 36: return Instrument::Kick;
        case 38: case 40: return Instrument::Snare;
        case 39: return Instrument::Clap;
        case 42: case 44: return Instrument::ClosedHat;
        case 46: return Instrument::OpenHat;
        case 51: case 53: case 59: return Instrument::Ride;
        case 49: case 57: return Instrument::Crash;
        case 41: case 43: case 45: return Instrument::LowTom;
        case 47: case 48: return Instrument::MidTom;
        case 50: return Instrument::HighTom;
        case 70: case 82: return Instrument::Shaker;
        case 56: return Instrument::Perc1;
        case 37: case 75: case 76: case 77: return Instrument::Perc2;
        default: return std::nullopt;
    }
}

float DrumEngine::Biquad::tick (float input) noexcept
{
    const float output = b0 * input + z1;
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;
    return output;
}

void DrumEngine::Biquad::clear() noexcept
{
    z1 = z2 = 0.0f;
}

float DrumEngine::Resonator::tick (float input) noexcept
{
    const float output = inputGain * input + a1 * y1 + a2 * y2;
    y2 = y1;
    y1 = output;
    return output;
}

void DrumEngine::Resonator::clear() noexcept
{
    y1 = y2 = 0.0f;
}

DrumEngine::DrumEngine() noexcept
{
    for (const auto& item : metadata)
        setInstrumentParameters (item.instrument, item.defaultParameters);
}

bool DrumEngine::validInstrument (Instrument instrument) noexcept
{
    return static_cast<std::size_t> (instrument) < instrumentCount;
}

std::size_t DrumEngine::indexFor (Instrument instrument) noexcept
{
    return static_cast<std::size_t> (instrument);
}

void DrumEngine::prepare (double sampleRate, int maxBlockSize) noexcept
{
    if (! std::isfinite (sampleRate))
        sampleRate = 48000.0;
    sampleRate_ = std::clamp (sampleRate, 8000.0, 192000.0);
    inverseSampleRate_ = static_cast<float> (1.0 / sampleRate_);
    maxBlockSize_ = std::max (1, maxBlockSize);
    const float floatSampleRate = static_cast<float> (sampleRate_);
    maximumVoiceSamples_ = std::max<std::uint64_t> (
        1u, static_cast<std::uint64_t> (maximumTailSeconds * sampleRate_));
    const auto forcedFadeSamples = std::max<std::uint64_t> (
        1u, static_cast<std::uint64_t> (std::ceil (forcedFadeSeconds * floatSampleRate)));
    forcedFadeStartSamples_ = maximumVoiceSamples_ > forcedFadeSamples
        ? maximumVoiceSamples_ - forcedFadeSamples : 0u;
    naturalQuietHoldSamples_ = std::max<std::uint32_t> (
        1u, static_cast<std::uint32_t> (std::ceil (
            naturalQuietHoldSeconds * floatSampleRate)));
    peakReleaseMultiplier_ = coefficientForTime (peakReleaseSeconds, floatSampleRate);
    retirementFadeMultiplier_ = std::exp (
        minusOneHundredDb / std::max (1.0f, retirementFadeSeconds * floatSampleRate));
    forcedFadeMultiplier_ = std::exp (
        minusOneHundredDb / static_cast<float> (forcedFadeSamples));
    modalNoiseScale_ = referenceSampleRate / floatSampleRate;
    modalNoisePhaseIncrement_ = modalNoiseScale_;
    for (int i = 0; i < sineTableSize; ++i)
        sineTable_[static_cast<std::size_t> (i)] = std::sin (
            twoPi * static_cast<float> (i) / static_cast<float> (sineTableSize));
    prepared_ = true;
    reset();
}

void DrumEngine::reset() noexcept
{
    for (auto& voice : voices_)
        voice = Voice {};
    for (auto& voice : retiringVoices_)
        voice = Voice {};
    triggerCounters_.fill (0);
    generation_ = 0;
    smoothedOutputGain_ = outputGain_.load (std::memory_order_relaxed);
    dcInputLeft_ = dcInputRight_ = 0.0f;
    dcOutputLeft_ = dcOutputRight_ = 0.0f;
    activeVoiceCount_.store (0, std::memory_order_relaxed);
}

void DrumEngine::setInstrumentParameters (Instrument instrument,
                                           const InstrumentParameters& values) noexcept
{
    if (! validInstrument (instrument))
        return;
    const auto& defaults = getInstrumentMetadata (instrument).defaultParameters;
    auto& target = parameters_[indexFor (instrument)];
    target.characterA.store (clampUnit (values.characterA, defaults.characterA),
                             std::memory_order_relaxed);
    target.characterB.store (clampUnit (values.characterB, defaults.characterB),
                             std::memory_order_relaxed);
    const float pitch = std::isfinite (values.pitch) ? values.pitch : defaults.pitch;
    target.pitch.store (std::clamp (pitch, -24.0f, 24.0f), std::memory_order_relaxed);
    target.decay.store (clampUnit (values.decay, defaults.decay),
                        std::memory_order_relaxed);
}

void DrumEngine::setOutputGain (float linearGain) noexcept
{
    outputGain_.store (std::isfinite (linearGain) ? std::clamp (linearGain, 0.0f, 2.0f) : 0.82f,
                       std::memory_order_relaxed);
}

InstrumentParameters DrumEngine::snapshotParameters (Instrument instrument) const noexcept
{
    const auto& source = parameters_[indexFor (instrument)];
    return { source.characterA.load (std::memory_order_relaxed),
             source.characterB.load (std::memory_order_relaxed),
             source.pitch.load (std::memory_order_relaxed),
             source.decay.load (std::memory_order_relaxed) };
}

float DrumEngine::decaySecondsFor (Instrument instrument, float normalizedDecay) const noexcept
{
    const auto index = indexFor (instrument);
    const float low = minimumDecay[index];
    const float high = maximumDecay[index];
    return low * std::pow (high / low, clampUnit (normalizedDecay));
}

std::uint32_t DrumEngine::hash32 (std::uint32_t value) noexcept
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value == 0u ? 1u : value;
}

float DrumEngine::nextNoise (Voice& voice) noexcept
{
    std::uint32_t x = voice.noiseState;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    voice.noiseState = x == 0u ? 1u : x;
    return static_cast<float> (voice.noiseState & 0x00ffffffu) / 8388607.5f - 1.0f;
}

float DrumEngine::nextModalNoise (Voice& voice) const noexcept
{
    if (! voice.modalNoiseReady)
    {
        voice.modalNoiseCurrent = nextNoise (voice);
        voice.modalNoiseNext = nextNoise (voice);
        voice.modalNoisePhase = 0.0f;
        voice.modalNoiseReady = true;
    }

    const float output = voice.modalNoiseCurrent
        + voice.modalNoisePhase * (voice.modalNoiseNext - voice.modalNoiseCurrent);
    voice.modalNoisePhase += modalNoisePhaseIncrement_;
    while (voice.modalNoisePhase >= 1.0f)
    {
        voice.modalNoisePhase -= 1.0f;
        voice.modalNoiseCurrent = voice.modalNoiseNext;
        voice.modalNoiseNext = nextNoise (voice);
    }
    return output;
}

float DrumEngine::softClip (float value) noexcept
{
    value = std::clamp (value, -8.0f, 8.0f);
    return value / (1.0f + std::abs (value));
}

float DrumEngine::sineLookup (float phase) const noexcept
{
    phase -= std::floor (phase);
    const float position = phase * static_cast<float> (sineTableSize);
    const int whole = static_cast<int> (position);
    const int index = whole & sineTableMask;
    const float fraction = position - static_cast<float> (whole);
    const float a = sineTable_[static_cast<std::size_t> (index)];
    const float b = sineTable_[static_cast<std::size_t> ((index + 1) & sineTableMask)];
    return a + fraction * (b - a);
}

float DrumEngine::oscillator (Voice& voice, int oscillatorIndex) const noexcept
{
    const auto index = static_cast<std::size_t> (std::clamp (oscillatorIndex, 0, oscillatorCount - 1));
    voice.phases[index] += voice.phaseIncrements[index];
    voice.phases[index] -= std::floor (voice.phases[index]);
    return sineLookup (voice.phases[index]);
}

void DrumEngine::configureLowpass (Biquad& filter, float frequency, float q) const noexcept
{
    frequency = std::clamp (frequency, 10.0f, 0.45f * static_cast<float> (sampleRate_));
    q = std::clamp (q, 0.15f, 20.0f);
    const float omega = twoPi * frequency * inverseSampleRate_;
    const float cosine = std::cos (omega);
    const float alpha = std::sin (omega) / (2.0f * q);
    const float inverseA0 = 1.0f / (1.0f + alpha);
    filter.b0 = 0.5f * (1.0f - cosine) * inverseA0;
    filter.b1 = (1.0f - cosine) * inverseA0;
    filter.b2 = filter.b0;
    filter.a1 = -2.0f * cosine * inverseA0;
    filter.a2 = (1.0f - alpha) * inverseA0;
    filter.clear();
}

void DrumEngine::configureHighpass (Biquad& filter, float frequency, float q) const noexcept
{
    frequency = std::clamp (frequency, 10.0f, 0.45f * static_cast<float> (sampleRate_));
    q = std::clamp (q, 0.15f, 20.0f);
    const float omega = twoPi * frequency * inverseSampleRate_;
    const float cosine = std::cos (omega);
    const float alpha = std::sin (omega) / (2.0f * q);
    const float inverseA0 = 1.0f / (1.0f + alpha);
    filter.b0 = 0.5f * (1.0f + cosine) * inverseA0;
    filter.b1 = -(1.0f + cosine) * inverseA0;
    filter.b2 = filter.b0;
    filter.a1 = -2.0f * cosine * inverseA0;
    filter.a2 = (1.0f - alpha) * inverseA0;
    filter.clear();
}

void DrumEngine::configureBandpass (Biquad& filter, float frequency, float q) const noexcept
{
    frequency = std::clamp (frequency, 10.0f, 0.45f * static_cast<float> (sampleRate_));
    q = std::clamp (q, 0.15f, 20.0f);
    const float omega = twoPi * frequency * inverseSampleRate_;
    const float cosine = std::cos (omega);
    const float alpha = std::sin (omega) / (2.0f * q);
    const float inverseA0 = 1.0f / (1.0f + alpha);
    filter.b0 = alpha * inverseA0;
    filter.b1 = 0.0f;
    filter.b2 = -filter.b0;
    filter.a1 = -2.0f * cosine * inverseA0;
    filter.a2 = (1.0f - alpha) * inverseA0;
    filter.clear();
}

void DrumEngine::configureResonator (Resonator& resonator, float frequency,
                                     float decaySeconds) const noexcept
{
    frequency = std::clamp (frequency, 20.0f, 0.45f * static_cast<float> (sampleRate_));
    decaySeconds = std::max (0.005f, decaySeconds);
    const float omega = twoPi * frequency * inverseSampleRate_;
    const float radius = coefficientForTime (decaySeconds, static_cast<float> (sampleRate_));
    resonator.a1 = 2.0f * radius * std::cos (omega);
    resonator.a2 = -radius * radius;

    // A two-pole resonator's impulse residue is inputGain / sin (omega).
    // Preserve the existing 48 kHz residue while keeping that ratio constant
    // at every sample rate.
    const float referenceFrequency = std::min (frequency, 0.45f * referenceSampleRate);
    const float referenceOmega = twoPi * referenceFrequency / referenceSampleRate;
    const float referenceRadius = coefficientForTime (decaySeconds, referenceSampleRate);
    const float referenceGain = 0.45f * std::sqrt (
        std::max (1.0e-8f, 1.0f - referenceRadius * referenceRadius));
    resonator.inputGain = referenceGain * std::sin (omega)
        / std::max (1.0e-4f, std::sin (referenceOmega));
    resonator.clear();
}

int DrumEngine::findVoiceSlot() const noexcept
{
    for (int index = 0; index < maxVoices; ++index)
        if (! voices_[static_cast<std::size_t> (index)].active)
            return index;

    int candidate = 0;
    float quietest = std::numeric_limits<float>::max();
    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
    for (int index = 0; index < maxVoices; ++index)
    {
        const auto& voice = voices_[static_cast<std::size_t> (index)];
        const float level = voice.recentPeak * voice.chokeGain;
        if (level < quietest || (level == quietest && voice.generation < oldest))
        {
            candidate = index;
            quietest = level;
            oldest = voice.generation;
        }
    }
    return candidate;
}

void DrumEngine::silenceVoice (Voice& voice) noexcept
{
    voice = Voice {};
}

void DrumEngine::beginChoke (Voice& voice, float seconds) noexcept
{
    if (! voice.active)
        return;
    beginFadeToSilence (
        voice, coefficientForTime (std::max (0.0005f, seconds),
                                   static_cast<float> (sampleRate_)));
}

void DrumEngine::beginFadeToSilence (Voice& voice, float multiplier) noexcept
{
    if (! voice.active)
        return;
    voice.choking = true;
    voice.chokeMultiplier = std::min (
        voice.chokeMultiplier, std::clamp (multiplier, 0.0f, 1.0f));
}

void DrumEngine::retireVoice (const Voice& source) noexcept
{
    if (! source.active)
        return;

    Voice* destination = nullptr;
    float quietest = std::numeric_limits<float>::max();
    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
    for (auto& voice : retiringVoices_)
    {
        if (! voice.active)
        {
            destination = &voice;
            break;
        }

        const float level = voice.recentPeak * voice.chokeGain;
        if (level < quietest || (level == quietest && voice.generation < oldest))
        {
            destination = &voice;
            quietest = level;
            oldest = voice.generation;
        }
    }

    *destination = source;
    beginFadeToSilence (*destination, retirementFadeMultiplier_);
}

void DrumEngine::chokeHats() noexcept
{
    for (auto& voice : voices_)
        if (voice.active && (voice.instrument == Instrument::ClosedHat
                             || voice.instrument == Instrument::OpenHat))
            beginChoke (voice, 0.003f);
}

void DrumEngine::allSoundsOff() noexcept
{
    for (auto& voice : voices_)
        if (voice.active)
            beginChoke (voice, 0.004f);
    for (auto& voice : retiringVoices_)
        if (voice.active)
            beginChoke (voice, 0.004f);
}

void DrumEngine::updateActiveVoiceCount() noexcept
{
    int count = 0;
    for (const auto& voice : voices_)
        count += voice.active ? 1 : 0;
    activeVoiceCount_.store (count, std::memory_order_relaxed);
}

int DrumEngine::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount_.load (std::memory_order_relaxed);
}

void DrumEngine::initialiseModalVoice (Voice& voice, const float* ratios, int modeCount,
                                       float baseFrequency, float decaySeconds,
                                       float spread, float brightness) noexcept
{
    modeCount = std::clamp (modeCount, 0, resonatorCount);
    float gainSum = 0.0f;
    for (int mode = 0; mode < modeCount; ++mode)
    {
        const auto hash = hash32 (voice.noiseState + static_cast<std::uint32_t> (mode * 0x9e37));
        const float random = static_cast<float> (hash & 0xffffu) / 32767.5f - 1.0f;
        const float ratio = ratios[mode] * (1.0f + 0.035f * spread * random);
        const float modeDecay = decaySeconds * (0.62f + 0.38f / (1.0f + 0.10f * static_cast<float> (mode)));
        configureResonator (voice.resonators[static_cast<std::size_t> (mode)],
                            baseFrequency * ratio, modeDecay);
        const float tilt = -1.30f + 1.05f * brightness;
        const float gain = std::pow (std::max (1.0f, ratio), tilt);
        voice.modeGains[static_cast<std::size_t> (mode)] = gain;
        gainSum += gain;
    }
    const float scale = gainSum > 0.0f ? 1.35f / gainSum : 1.0f;
    for (int mode = 0; mode < modeCount; ++mode)
        voice.modeGains[static_cast<std::size_t> (mode)] *= scale;
}

void DrumEngine::initialiseVoice (Voice& voice, Instrument instrument, float velocity,
                                  const InstrumentParameters& values, std::uint32_t seed) noexcept
{
    voice = Voice {};
    voice.active = true;
    voice.instrument = instrument;
    voice.generation = ++generation_;
    voice.noiseState = seed == 0u ? 1u : seed;
    voice.velocity = velocity * (0.68f + 0.32f * std::sqrt (velocity));
    voice.recentPeak = voice.velocity;
    voice.characterA = values.characterA;
    voice.characterB = values.characterB;
    voice.pitchRatio = std::exp2 (values.pitch / 12.0f);
    voice.decaySeconds = decaySecondsFor (instrument, values.decay);
    voice.envelopeMultiplier = coefficientForTime (voice.decaySeconds,
                                                     static_cast<float> (sampleRate_));
    voice.auxiliaryMultiplier = coefficientForTime (voice.decaySeconds * 0.85f,
                                                     static_cast<float> (sampleRate_));
    voice.transientMultiplier = coefficientForTime (0.008f, static_cast<float> (sampleRate_));
    voice.pitchEnvelopeMultiplier = coefficientForTime (0.030f, static_cast<float> (sampleRate_));
    // Quiet voices retire from their measured output level. This is the hard
    // host-facing ceiling; a forced fade begins shortly before it.
    voice.maximumSamples = maximumVoiceSamples_;

    float pan = 0.0f;
    switch (instrument)
    {
        case Instrument::ClosedHat: pan = 0.16f; break;
        case Instrument::OpenHat:   pan = 0.20f; break;
        case Instrument::Ride:      pan = 0.27f; break;
        case Instrument::Crash:     pan = -0.27f; break;
        case Instrument::LowTom:    pan = -0.20f; break;
        case Instrument::HighTom:   pan = 0.20f; break;
        case Instrument::Shaker:    pan = 0.12f; break;
        case Instrument::Perc1:     pan = -0.12f; break;
        case Instrument::Perc2:     pan = 0.12f; break;
        default: break;
    }
    voice.panLeft = constantPowerLeft (pan);
    voice.panRight = constantPowerRight (pan);

    static constexpr float hatRatios[6] { 1.0f, 1.342f, 1.778f, 2.133f, 2.697f, 3.415f };
    static constexpr float rideRatios[10] { 1.0f, 1.41f, 1.73f, 2.12f, 2.70f, 3.16f, 4.11f, 5.32f, 6.47f, 7.83f };
    static constexpr float crashRatios[12] { 1.0f, 1.34f, 1.68f, 2.05f, 2.63f, 3.11f, 3.78f, 4.41f, 5.38f, 6.27f, 7.15f, 8.40f };

    switch (instrument)
    {
        case Instrument::Kick:
            voice.baseFrequency = 52.0f * voice.pitchRatio;
            voice.sweepAmount = 0.8f + 4.4f * voice.characterA;
            voice.pitchEnvelopeMultiplier = coefficientForTime (0.012f + 0.050f * voice.characterA,
                                                                  static_cast<float> (sampleRate_));
            voice.transientMultiplier = coefficientForTime (0.0025f + 0.004f * voice.characterA,
                                                             static_cast<float> (sampleRate_));
            configureHighpass (voice.filterA, 2400.0f + 4200.0f * voice.characterA, 0.72f);
            break;

        case Instrument::Snare:
            voice.baseFrequency = 185.0f * voice.pitchRatio;
            voice.phaseIncrements[0] = std::min (0.45f, voice.baseFrequency * inverseSampleRate_);
            voice.phaseIncrements[1] = std::min (0.45f, voice.baseFrequency * 1.78f * inverseSampleRate_);
            voice.envelopeMultiplier = coefficientForTime (voice.decaySeconds * 0.72f,
                                                            static_cast<float> (sampleRate_));
            configureBandpass (voice.filterA,
                               (1250.0f + 4800.0f * voice.characterB) * std::pow (voice.pitchRatio, 0.30f),
                               0.65f + 0.45f * voice.characterB);
            configureHighpass (voice.filterB, 700.0f + 1700.0f * voice.characterB, 0.7f);
            voice.transientMultiplier = coefficientForTime (0.004f + 0.005f * voice.characterB,
                                                             static_cast<float> (sampleRate_));
            break;

        case Instrument::Clap:
        {
            const auto spacing = static_cast<std::uint64_t> ((0.007f + 0.011f * voice.characterA)
                                                              * static_cast<float> (sampleRate_));
            voice.burstStarts = { 0u, spacing, 2u * spacing + spacing / 5u,
                                  3u * spacing + spacing / 2u };
            voice.minimumSilenceSamples = voice.burstStarts.back() + static_cast<std::uint64_t> (
                std::ceil (0.010f * static_cast<float> (sampleRate_)));
            voice.transientEnvelope = 0.0f;
            voice.transientMultiplier = coefficientForTime (0.0030f + 0.0025f * voice.characterA,
                                                             static_cast<float> (sampleRate_));
            configureBandpass (voice.filterA,
                               (850.0f + 2750.0f * voice.characterB) * std::pow (voice.pitchRatio, 0.42f),
                               0.68f + 0.42f * voice.characterB);
            configureHighpass (voice.filterB, 430.0f + 1450.0f * voice.characterB, 0.72f);
            break;
        }

        case Instrument::ClosedHat:
        case Instrument::OpenHat:
        {
            const float base = 1550.0f * voice.pitchRatio;
            for (int oscillatorIndex = 0; oscillatorIndex < 6; ++oscillatorIndex)
            {
                const float alternating = (oscillatorIndex & 1) == 0 ? -1.0f : 1.0f;
                const float ratio = hatRatios[oscillatorIndex]
                    * (1.0f + alternating * 0.025f * voice.characterA);
                voice.phaseIncrements[static_cast<std::size_t> (oscillatorIndex)] =
                    std::min (0.45f, base * ratio * inverseSampleRate_);
                voice.phases[static_cast<std::size_t> (oscillatorIndex)] =
                    static_cast<float> (oscillatorIndex) * 0.117f;
            }
            configureHighpass (voice.filterA, 3400.0f + 6500.0f * voice.characterB, 0.70f);
            configureBandpass (voice.filterB, 6500.0f + 4800.0f * voice.characterB, 0.85f);
            voice.transientMultiplier = coefficientForTime (instrument == Instrument::ClosedHat ? 0.0025f : 0.006f,
                                                             static_cast<float> (sampleRate_));
            break;
        }

        case Instrument::Ride:
            initialiseModalVoice (voice, rideRatios, 10, 335.0f * voice.pitchRatio,
                                  voice.decaySeconds, 0.28f, voice.characterB);
            configureHighpass (voice.filterA, 260.0f, 0.70f);
            configureBandpass (voice.filterB, 4300.0f + 5500.0f * voice.characterB, 0.80f);
            voice.transientMultiplier = coefficientForTime (0.006f + 0.012f * voice.characterA,
                                                             static_cast<float> (sampleRate_));
            break;

        case Instrument::Crash:
            initialiseModalVoice (voice, crashRatios, 12, 255.0f * voice.pitchRatio,
                                  voice.decaySeconds, voice.characterA, voice.characterB);
            configureHighpass (voice.filterA, 300.0f + 500.0f * voice.characterB, 0.65f);
            configureBandpass (voice.filterB, 3400.0f + 6200.0f * voice.characterB, 0.72f);
            voice.transientMultiplier = coefficientForTime (0.015f + 0.025f * voice.characterA,
                                                             static_cast<float> (sampleRate_));
            break;

        case Instrument::LowTom:
        case Instrument::MidTom:
        case Instrument::HighTom:
        {
            const float root = instrument == Instrument::LowTom ? 82.0f
                             : instrument == Instrument::MidTom ? 123.0f : 174.0f;
            voice.baseFrequency = root * voice.pitchRatio;
            voice.sweepAmount = 0.12f + 1.20f * voice.characterA;
            voice.phaseIncrements[1] = std::min (
                0.45f, voice.baseFrequency * (1.48f + 0.30f * voice.characterB) * inverseSampleRate_);
            voice.pitchEnvelopeMultiplier = coefficientForTime (0.016f + 0.040f * voice.characterA,
                                                                  static_cast<float> (sampleRate_));
            voice.transientMultiplier = coefficientForTime (0.003f + 0.004f * voice.characterB,
                                                             static_cast<float> (sampleRate_));
            configureBandpass (voice.filterA, 1900.0f + 2800.0f * voice.characterB, 0.72f);
            break;
        }

        case Instrument::Shaker:
            voice.transientEnvelope = 0.0f;
            voice.transientMultiplier = coefficientForTime (0.0008f + 0.0012f * voice.characterA,
                                                             static_cast<float> (sampleRate_));
            configureBandpass (voice.filterA,
                               (4400.0f + 6600.0f * voice.characterB) * std::pow (voice.pitchRatio, 0.65f),
                               0.75f + 1.2f * voice.characterB);
            configureHighpass (voice.filterB, 1900.0f + 2600.0f * voice.characterB, 0.70f);
            break;

        case Instrument::Perc1:
        {
            voice.baseFrequency = 535.0f * voice.pitchRatio;
            const float ratio = 1.34f + 0.42f * voice.characterA;
            voice.phaseIncrements[0] = std::min (0.45f, voice.baseFrequency * inverseSampleRate_);
            voice.phaseIncrements[1] = std::min (0.45f, voice.baseFrequency * ratio * inverseSampleRate_);
            voice.phaseIncrements[2] = std::min (0.45f, voice.baseFrequency * 2.0f * inverseSampleRate_);
            voice.phaseIncrements[3] = std::min (0.45f, voice.baseFrequency * ratio * 2.0f * inverseSampleRate_);
            configureBandpass (voice.filterA, std::min (6200.0f, voice.baseFrequency * 1.55f), 1.0f);
            configureHighpass (voice.filterB, 1800.0f, 0.70f);
            voice.transientMultiplier = coefficientForTime (0.0035f, static_cast<float> (sampleRate_));
            break;
        }

        case Instrument::Perc2:
        {
            const float hollow = voice.characterA;
            const float ratios[4] { 1.0f, 1.42f + 0.35f * hollow,
                                    2.31f + 0.55f * hollow, 3.84f - 0.40f * hollow };
            initialiseModalVoice (voice, ratios, 4, 930.0f * voice.pitchRatio,
                                  voice.decaySeconds, 0.12f, 0.35f + 0.35f * hollow);
            configureBandpass (voice.filterA, 2800.0f + 5000.0f * voice.characterB, 0.85f);
            configureHighpass (voice.filterB, 350.0f + 550.0f * (1.0f - hollow), 0.70f);
            voice.transientMultiplier = coefficientForTime (0.0015f + 0.003f * voice.characterB,
                                                             static_cast<float> (sampleRate_));
            break;
        }

        case Instrument::Count:
            break;
    }
}

void DrumEngine::trigger (Instrument instrument, float velocity) noexcept
{
    if (! validInstrument (instrument) || ! std::isfinite (velocity) || velocity <= 0.0f)
        return;
    if (! prepared_)
        prepare (sampleRate_, maxBlockSize_);
    velocity = std::clamp (velocity, 0.0f, 1.0f);
    if (instrument == Instrument::ClosedHat || instrument == Instrument::OpenHat)
        chokeHats();

    const auto index = indexFor (instrument);
    const auto counter = ++triggerCounters_[index];
    const std::uint32_t seed = hash32 (0x6d2b79f5u
        ^ static_cast<std::uint32_t> ((index + 1u) * 0x9e3779b9u)
        ^ static_cast<std::uint32_t> (counter)
        ^ static_cast<std::uint32_t> (counter >> 32u));
    auto& voice = voices_[static_cast<std::size_t> (findVoiceSlot())];
    if (voice.active && voice.ageSamples != 0u)
        retireVoice (voice);
    initialiseVoice (voice, instrument, velocity, snapshotParameters (instrument), seed);
    updateActiveVoiceCount();
}

bool DrumEngine::triggerMidi (int midiNote, float velocity) noexcept
{
    const auto instrument = instrumentForMidiNote (midiNote);
    if (! instrument.has_value())
        return false;
    trigger (*instrument, velocity);
    return true;
}

float DrumEngine::renderKick (Voice& voice) noexcept
{
    const float frequency = voice.baseFrequency * (1.0f + voice.sweepAmount * voice.pitchEnvelope);
    voice.phaseIncrements[0] = std::min (0.45f, frequency * inverseSampleRate_);
    const float fundamental = oscillator (voice, 0);
    const float harmonic = sineLookup (2.0f * voice.phases[0]);
    const float click = voice.filterA.tick (nextNoise (voice)) * voice.transientEnvelope
        * (0.10f + 0.28f * voice.characterA);
    const float body = (fundamental + 0.18f * voice.characterB * harmonic) * voice.envelope;
    const float drive = 1.0f + 5.5f * voice.characterB;
    return 1.25f * softClip (drive * (body + click)) / (0.75f + 0.25f * drive);
}

float DrumEngine::renderSnare (Voice& voice) noexcept
{
    const float body = (0.72f * oscillator (voice, 0) + 0.36f * oscillator (voice, 1))
        * voice.envelope;
    const float noise = nextNoise (voice);
    const float wires = voice.filterA.tick (noise) * voice.auxiliaryEnvelope;
    const float snap = voice.filterB.tick (noise) * voice.transientEnvelope;
    const float wireMix = 0.18f + 0.82f * voice.characterA;
    return 0.72f * ((0.78f - 0.48f * voice.characterA) * body
                    + wireMix * wires + 0.35f * voice.characterB * snap);
}

float DrumEngine::renderClap (Voice& voice) noexcept
{
    for (const auto start : voice.burstStarts)
        if (voice.ageSamples == start)
            voice.transientEnvelope += 1.0f;
    const float noise = voice.filterB.tick (voice.filterA.tick (nextNoise (voice)));
    const float burst = (0.38f + 0.16f * voice.characterA) * voice.transientEnvelope;
    const float tail = (0.13f + 0.24f * voice.characterB) * voice.envelope;
    return noise * (burst + tail);
}

float DrumEngine::renderHat (Voice& voice) noexcept
{
    std::array<float, 6> oscillators {};
    for (int index = 0; index < 6; ++index)
        oscillators[static_cast<std::size_t> (index)] = oscillator (voice, index);
    const float rings = oscillators[0] * oscillators[1]
                      + oscillators[2] * oscillators[3]
                      + oscillators[4] * oscillators[5];
    const float tones = oscillators[0] + oscillators[2] + oscillators[4];
    const float noise = nextNoise (voice);
    const float metallic = (0.18f + 0.12f * voice.characterA) * rings
                         + 0.07f * voice.characterA * tones
                         + 0.20f * (1.0f - voice.characterA) * noise;
    const float high = voice.filterA.tick (metallic);
    const float focused = voice.filterB.tick (metallic);
    const float attack = 0.12f * voice.transientEnvelope * noise;
    return (0.58f * high + (0.18f + 0.20f * voice.characterB) * focused + attack)
        * voice.envelope;
}

float DrumEngine::renderRide (Voice& voice) noexcept
{
    const float noise = nextModalNoise (voice);
    const float excitation = modalNoiseScale_ * noise * voice.transientEnvelope
        * (0.30f + 0.70f * voice.characterA);
    float modes = 0.0f;
    for (std::size_t mode = 0; mode < 10; ++mode)
    {
        float gain = voice.modeGains[mode];
        if (mode < 3)
            gain *= 0.65f + 1.25f * voice.characterA;
        modes += gain * voice.resonators[mode].tick (excitation);
    }
    const float wash = voice.filterB.tick (noise) * voice.auxiliaryEnvelope
        * (0.28f * (1.0f - voice.characterA));
    return 0.82f * voice.filterA.tick (modes + wash);
}

float DrumEngine::renderCrash (Voice& voice) noexcept
{
    const float noise = nextModalNoise (voice);
    const float excitation = modalNoiseScale_ * noise
        * (0.25f * voice.transientEnvelope + 0.035f * voice.envelope);
    float modes = 0.0f;
    for (std::size_t mode = 0; mode < resonatorCount; ++mode)
        modes += voice.modeGains[mode] * voice.resonators[mode].tick (excitation);
    const float wash = voice.filterB.tick (noise) * voice.auxiliaryEnvelope
        * (0.14f + 0.22f * voice.characterA);
    return 0.88f * voice.filterA.tick (modes + wash);
}

float DrumEngine::renderTom (Voice& voice) noexcept
{
    const float frequency = voice.baseFrequency * (1.0f + voice.sweepAmount * voice.pitchEnvelope);
    voice.phaseIncrements[0] = std::min (0.45f, frequency * inverseSampleRate_);
    const float fundamental = oscillator (voice, 0);
    const float shell = oscillator (voice, 1);
    const float skin = voice.filterA.tick (nextNoise (voice)) * voice.transientEnvelope;
    return 0.98f * ((fundamental + (0.06f + 0.19f * voice.characterB) * shell)
                    * voice.envelope + 0.16f * voice.characterB * skin);
}

float DrumEngine::renderShaker (Voice& voice) noexcept
{
    const float decision = 0.5f + 0.5f * nextNoise (voice);
    const float collisionsPerSecond = 320.0f + 4800.0f * voice.characterA;
    const float probability = std::min (0.80f, collisionsPerSecond * inverseSampleRate_);
    const float grainNoise = nextNoise (voice);
    if (decision < probability)
        voice.transientEnvelope += (0.45f + 0.55f * std::abs (grainNoise));
    const float grains = voice.filterB.tick (voice.filterA.tick (
        grainNoise * voice.transientEnvelope));
    return 0.95f * grains * voice.envelope;
}

float DrumEngine::renderPerc1 (Voice& voice) noexcept
{
    const float first = oscillator (voice, 0) + 0.24f * oscillator (voice, 2);
    const float second = oscillator (voice, 1) + 0.24f * oscillator (voice, 3);
    const float metallic = 0.62f * first + 0.46f * second + 0.22f * first * second;
    const float click = voice.filterB.tick (nextNoise (voice)) * voice.transientEnvelope;
    const float drive = 1.0f + 4.5f * voice.characterB;
    const float shaped = voice.filterA.tick (metallic) * voice.envelope + 0.12f * click;
    return 1.15f * softClip (drive * shaped) / (0.78f + 0.22f * drive);
}

float DrumEngine::renderPerc2 (Voice& voice) noexcept
{
    const float noise = nextModalNoise (voice);
    const float excitation = (voice.ageSamples == 0 ? 1.0f : 0.0f)
        + modalNoiseScale_ * voice.characterB * voice.transientEnvelope * noise;
    float body = 0.0f;
    for (std::size_t mode = 0; mode < 4; ++mode)
        body += voice.modeGains[mode] * voice.resonators[mode].tick (excitation);
    const float click = voice.filterA.tick (noise) * voice.transientEnvelope;
    return 1.35f * voice.filterB.tick (body + 0.20f * voice.characterB * click);
}

float DrumEngine::renderVoice (Voice& voice) noexcept
{
    if (voice.ageSamples >= forcedFadeStartSamples_)
        beginFadeToSilence (voice, forcedFadeMultiplier_);

    float output = 0.0f;
    switch (voice.instrument)
    {
        case Instrument::Kick:      output = renderKick (voice); break;
        case Instrument::Snare:     output = renderSnare (voice); break;
        case Instrument::Clap:      output = renderClap (voice); break;
        case Instrument::ClosedHat:
        case Instrument::OpenHat:   output = renderHat (voice); break;
        case Instrument::Ride:      output = renderRide (voice); break;
        case Instrument::Crash:     output = renderCrash (voice); break;
        case Instrument::LowTom:
        case Instrument::MidTom:
        case Instrument::HighTom:   output = renderTom (voice); break;
        case Instrument::Shaker:    output = renderShaker (voice); break;
        case Instrument::Perc1:     output = renderPerc1 (voice); break;
        case Instrument::Perc2:     output = renderPerc2 (voice); break;
        case Instrument::Count:     break;
    }

    output *= voice.velocity * voice.chokeGain;
    voice.lastOutput = output;
    voice.recentPeak = std::max (
        std::abs (output), voice.recentPeak * peakReleaseMultiplier_);
    voice.envelope *= voice.envelopeMultiplier;
    voice.auxiliaryEnvelope *= voice.auxiliaryMultiplier;
    voice.transientEnvelope *= voice.transientMultiplier;
    voice.pitchEnvelope *= voice.pitchEnvelopeMultiplier;
    if (voice.choking)
        voice.chokeGain *= voice.chokeMultiplier;
    ++voice.ageSamples;

    if (! voice.choking && voice.ageSamples >= voice.minimumSilenceSamples)
    {
        if (voice.recentPeak < silenceThreshold)
            ++voice.quietSamples;
        else
            voice.quietSamples = 0u;
    }

    if ((voice.choking && voice.chokeGain <= silenceThreshold)
        || voice.quietSamples >= naturalQuietHoldSamples_
        || voice.ageSamples >= voice.maximumSamples)
        silenceVoice (voice);
    return output;
}

void DrumEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;
    if (! prepared_)
        prepare (sampleRate_, std::max (maxBlockSize_, numSamples));

    const float gainTarget = outputGain_.load (std::memory_order_relaxed);
    const float gainSmoothing = 1.0f - std::exp (-inverseSampleRate_ / 0.020f);
    const float dcCoefficient = std::exp (-twoPi * 12.0f * inverseSampleRate_);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float dryLeft = 0.0f;
        float dryRight = 0.0f;
        bool hasActiveVoices = false;
        for (auto& voice : voices_)
        {
            if (! voice.active)
                continue;
            const float panLeft = voice.panLeft;
            const float panRight = voice.panRight;
            const float value = renderVoice (voice);
            dryLeft += value * panLeft;
            dryRight += value * panRight;
            hasActiveVoices = hasActiveVoices || voice.active;
        }
        for (auto& voice : retiringVoices_)
        {
            if (! voice.active)
                continue;
            const float panLeft = voice.panLeft;
            const float panRight = voice.panRight;
            const float value = renderVoice (voice);
            dryLeft += value * panLeft;
            dryRight += value * panRight;
            hasActiveVoices = hasActiveVoices || voice.active;
        }

        smoothedOutputGain_ += gainSmoothing * (gainTarget - smoothedOutputGain_);
        const float dcLeft = dryLeft - dcInputLeft_ + dcCoefficient * dcOutputLeft_;
        const float dcRight = dryRight - dcInputRight_ + dcCoefficient * dcOutputRight_;
        dcInputLeft_ = dryLeft;
        dcInputRight_ = dryRight;
        dcOutputLeft_ = dcLeft;
        dcOutputRight_ = dcRight;

        const float outputLeft = softClip (smoothedOutputGain_ * dcLeft);
        const float outputRight = softClip (smoothedOutputGain_ * dcRight);
        if (left != nullptr && right == left)
        {
            left[sample] = 0.5f * (outputLeft + outputRight);
        }
        else
        {
            if (left != nullptr)
                left[sample] = outputLeft;
            if (right != nullptr)
                right[sample] = outputRight;
        }

        // The final voice has already reached the inaudible end of its natural
        // or forced fade. Do not let the mix DC blocker extend the host tail.
        if (! hasActiveVoices)
        {
            dcInputLeft_ = dcInputRight_ = 0.0f;
            dcOutputLeft_ = dcOutputRight_ = 0.0f;
        }
    }

    updateActiveVoiceCount();
}

} // namespace drumalor
