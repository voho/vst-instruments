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
constexpr float supplySagAttackSeconds = 0.004f;
constexpr float supplySagReleaseSeconds = 0.085f;

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

double besselI0 (double value) noexcept
{
    // Stable power series for the Kaiser-window range used by the metallic
    // decimator. This runs only in prepare/reset, never in the audio loop.
    const double squaredQuarter = 0.25 * value * value;
    double sum = 1.0;
    double term = 1.0;
    for (int order = 1; order <= 24; ++order)
    {
        term *= squaredQuarter
            / static_cast<double> (order * order);
        sum += term;
        if (term < 1.0e-15 * sum)
            break;
    }
    return sum;
}

float polyBlep (float phase, float phaseIncrement) noexcept
{
    phaseIncrement = std::clamp (phaseIncrement, 1.0e-6f, 0.49f);
    if (phase < phaseIncrement)
    {
        const float normalized = phase / phaseIncrement;
        return normalized + normalized - normalized * normalized - 1.0f;
    }
    if (phase > 1.0f - phaseIncrement)
    {
        const float normalized = (phase - 1.0f) / phaseIncrement;
        return normalized * normalized + normalized + normalized + 1.0f;
    }
    return 0.0f;
}

float constantPowerLeft (float pan) noexcept
{
    return std::sqrt (0.5f * (1.0f - std::clamp (pan, -1.0f, 1.0f)));
}

float constantPowerRight (float pan) noexcept
{
    return std::sqrt (0.5f * (1.0f + std::clamp (pan, -1.0f, 1.0f)));
}

float rationalShaper (float value, float positiveCurvature,
                      float negativeCurvature) noexcept
{
    value = std::clamp (std::isfinite (value) ? value : 0.0f, -64.0f, 64.0f);
    const float curvature = value >= 0.0f ? positiveCurvature : negativeCurvature;
    return value / (1.0f + curvature * std::abs (value));
}

float rationalShaperPrimitive (float value, float positiveCurvature,
                               float negativeCurvature) noexcept
{
    value = std::clamp (std::isfinite (value) ? value : 0.0f, -64.0f, 64.0f);
    const float curvature = value >= 0.0f ? positiveCurvature : negativeCurvature;
    const float inverseCurvature = 1.0f / curvature;
    if (value >= 0.0f)
        return value * inverseCurvature
            - std::log1p (curvature * value) * inverseCurvature * inverseCurvature;
    return (-curvature * value - std::log1p (-curvature * value))
        * inverseCurvature * inverseCurvature;
}

float antialiasedRationalShaper (float input, float& previousInput,
                                 float positiveCurvature,
                                 float negativeCurvature) noexcept
{
    input = std::clamp (std::isfinite (input) ? input : 0.0f, -64.0f, 64.0f);
    const float previous = std::clamp (
        std::isfinite (previousInput) ? previousInput : input, -64.0f, 64.0f);
    const float difference = input - previous;
    const float threshold = 1.0e-4f * (1.0f + std::abs (input) + std::abs (previous));
    float output = 0.0f;
    if (std::abs (difference) <= threshold)
    {
        // The midpoint is the limiting derivative of the divided difference
        // and avoids cancellation when two consecutive inputs nearly match.
        output = rationalShaper (0.5f * (input + previous),
                                 positiveCurvature, negativeCurvature);
    }
    else
    {
        output = (rationalShaperPrimitive (input, positiveCurvature, negativeCurvature)
                  - rationalShaperPrimitive (previous, positiveCurvature, negativeCurvature))
            / difference;
    }
    // ADAA's divided difference averages even a perfectly linear transfer
    // over the current and previous sample. Apply it only to the nonlinear
    // residual f(x) - x, leaving the non-aliasing linear path undelayed. This
    // preserves low-level brightness and cross-sample-rate gain.
    output += input - 0.5f * (input + previous);
    previousInput = input;
    return std::isfinite (output) ? output : 0.0f;
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
    sagAttackCoefficient_ = 1.0f - std::exp (
        -1.0f / std::max (1.0f, supplySagAttackSeconds * floatSampleRate));
    sagReleaseCoefficient_ = 1.0f - std::exp (
        -1.0f / std::max (1.0f, supplySagReleaseSeconds * floatSampleRate));
    modalNoiseScale_ = referenceSampleRate / floatSampleRate;
    modalNoisePhaseIncrement_ = modalNoiseScale_;
    // The discontinuous metallic source islands run at a high internal rate,
    // while resonators, envelopes and the per-voice circuit stages remain at
    // the host rate. This targets oversampling where Schmitt edges and ring
    // products actually create out-of-band energy instead of multiplying the
    // cost of the entire 128-voice path.
    metallicOversampleFactor_ = floatSampleRate < 44100.0f ? 8
                               : floatSampleRate <= 48000.0f ? 4
                               : floatSampleRate <= 96000.0f ? 2 : 1;
    metallicInternalSampleRate_ = floatSampleRate
        * static_cast<float> (metallicOversampleFactor_);
    metallicInverseSampleRate_ = 1.0f / metallicInternalSampleRate_;
    configureMetallicDecimator();
    metallicIncrementSmoothing_ = 1.0f - std::exp (
        -1.0f / std::max (1.0f, 0.0015f * metallicInternalSampleRate_));
    cymbalClockIncrement_ = std::min (1.0f, 30000.0f * inverseSampleRate_);
    const float reconstructionCutoff = std::min (13500.0f, 0.42f * floatSampleRate);
    cymbalReconstructionCoefficient_ = 1.0f - std::exp (
        -twoPi * reconstructionCutoff / floatSampleRate);
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
    componentDrift_.fill (0.0f);
    resetMetallicOscillatorBanks();
    anyVoiceActive_ = false;
    metallicFrozenSamples_ = 0;
    generation_ = 0;
    smoothedOutputGain_ = outputGain_.load (std::memory_order_relaxed);
    dcInputLeft_ = dcInputRight_ = 0.0f;
    dcOutputLeft_ = dcOutputRight_ = 0.0f;
    masterAdaaPreviousLeft_ = masterAdaaPreviousRight_ = 0.0f;
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

float DrumEngine::signedUnitFromHash (std::uint32_t value) noexcept
{
    const auto hashed = hash32 (value);
    return static_cast<float> (hashed & 0x00ffffffu) / 8388607.5f - 1.0f;
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

float DrumEngine::applyAnalogOutputStage (Voice& voice, float input) const noexcept
{
    // A compact circuit-inspired output stage: a rectifier-like envelope pulls
    // down a virtual supply rail on loud transients, while unequal positive and
    // negative transfer curves mimic a slightly mismatched transistor pair.
    // Every state variable belongs to its voice, so polyphonic summing remains
    // deterministic and no cross-voice synchronization is needed.
    const float rectified = std::min (2.0f, std::abs (input));
    const float sagCoefficient = rectified > voice.supplySag
        ? sagAttackCoefficient_ : sagReleaseCoefficient_;
    voice.supplySag += sagCoefficient * (rectified - voice.supplySag);
    const float rail = 1.0f - 0.042f * std::min (1.5f, voice.supplySag);
    const float drive = voice.circuitDrive / std::max (0.90f, rail);

    // First-order analytic antiderivative antialiasing evaluates the average
    // transfer over the interval between consecutive inputs. It suppresses
    // the dominant discontinuity in the derivative without oversampling. The
    // operating-point form keeps drive and transistor-pair bias independent.
    const bool isKick = voice.instrument == Instrument::Kick;
    const float positiveCurvature = isKick
        ? 0.18f + 0.30f * voice.characterB : 0.205f;
    const float negativeCurvature = isKick
        ? 0.18f - 0.08f * voice.characterB : 0.165f;
    const float shaperInput = drive * input + voice.circuitBias;
    const float shaped = antialiasedRationalShaper (
        shaperInput, voice.analogPreviousInput,
        positiveCurvature, negativeCurvature);
    const float zero = rationalShaper (
        voice.circuitBias, positiveCurvature, negativeCurvature);
    // The kick output stage is allowed modest drive-dependent makeup:
    // saturation should add density without making the low end retreat.
    const float makeup = isKick ? 1.0f + 0.32f * voice.characterB : 1.0f;
    const float output = makeup * rail * (shaped - zero) / std::max (1.0f, drive);
    return std::isfinite (output) ? std::clamp (output, -4.0f, 4.0f) : 0.0f;
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
    // The tonal core represents a lightly asymmetric analogue resonator, not
    // an immutable table sine. A small supply-dependent pitch term couples it
    // to the voice rail, while explicitly band-limited second/third harmonics
    // reproduce component/core asymmetry without passing a discontinuity into
    // a memoryless waveshaper.
    const float railPitch = 1.0f - 0.0018f * std::min (1.5f, voice.supplySag);
    const float increment = std::clamp (
        voice.phaseIncrements[index] * railPitch, 0.0f, 0.45f);
    voice.phases[index] += increment;
    voice.phases[index] -= std::floor (voice.phases[index]);
    const float phase = voice.phases[index];
    const float asymmetry = voice.oscillatorAsymmetries[index];
    const float secondGain = 0.022f * asymmetry
        * std::clamp ((0.48f - 2.0f * increment) / 0.08f, 0.0f, 1.0f);
    const float thirdGain = (0.004f + 0.005f * std::abs (asymmetry))
        * std::clamp ((0.48f - 3.0f * increment) / 0.08f, 0.0f, 1.0f);
    const float output = sineLookup (phase)
        + secondGain * sineLookup (2.0f * phase)
        + thirdGain * sineLookup (3.0f * phase);
    return output / (1.0f + std::abs (secondGain) + thirdGain);
}

int DrumEngine::metallicBankIndexFor (Instrument instrument) noexcept
{
    switch (instrument)
    {
        case Instrument::ClosedHat: return 0;
        case Instrument::OpenHat:   return 1;
        case Instrument::Ride:      return 2;
        case Instrument::Crash:     return 3;
        case Instrument::Perc1:     return 4;
        default:                    return -1;
    }
}

void DrumEngine::configureMetallicDecimator() noexcept
{
    // The discontinuous source island needs substantially more rejection than
    // a light audio low-pass can provide before an N:1 sample-rate change.
    // These odd-length Kaiser-windowed sinc kernels target an approximately
    // 80 dB stopband between 0.40 and 0.50 of the host sample rate. The tap
    // counts scale with the internal rate so the transition stays comparable
    // for every adaptive oversampling factor.
    metallicDecimatorCoefficients_.fill (0.0f);
    metallicDecimatorTapCount_ = metallicOversampleFactor_ <= 1 ? 1
                                 : metallicOversampleFactor_ == 2 ? 129
                                 : metallicOversampleFactor_ == 4 ? 257 : 401;
    if (metallicDecimatorTapCount_ == 1)
    {
        metallicDecimatorCoefficients_[0] = 1.0f;
        return;
    }

    constexpr double beta = 7.85726; // Kaiser beta for roughly 80 dB rejection.
    const int halfLength = metallicDecimatorTapCount_ / 2;
    const double cutoff = 0.45 / static_cast<double> (metallicOversampleFactor_);
    const double inverseWindowDenominator = 1.0 / besselI0 (beta);
    double coefficientSum = 0.0;
    for (int tap = 0; tap < metallicDecimatorTapCount_; ++tap)
    {
        const int offset = tap - halfLength;
        const double sinc = offset == 0
            ? 2.0 * cutoff
            : std::sin (2.0 * static_cast<double> (pi) * cutoff
                        * static_cast<double> (offset))
                / (static_cast<double> (pi) * static_cast<double> (offset));
        const double ratio = static_cast<double> (offset)
            / static_cast<double> (halfLength);
        const double window = besselI0 (
            beta * std::sqrt (std::max (0.0, 1.0 - ratio * ratio)))
            * inverseWindowDenominator;
        const float coefficient = static_cast<float> (sinc * window);
        metallicDecimatorCoefficients_[static_cast<std::size_t> (tap)] = coefficient;
        coefficientSum += coefficient;
    }

    const float inverseSum = static_cast<float> (1.0 / coefficientSum);
    for (int tap = 0; tap < metallicDecimatorTapCount_; ++tap)
        metallicDecimatorCoefficients_[static_cast<std::size_t> (tap)] *= inverseSum;
}

void DrumEngine::resetMetallicOscillatorBanks() noexcept
{
    static constexpr std::array<Instrument, metallicBankCount> instruments {
        Instrument::ClosedHat, Instrument::OpenHat, Instrument::Ride,
        Instrument::Crash, Instrument::Perc1
    };

    for (std::size_t bankIndex = 0; bankIndex < metallicBanks_.size(); ++bankIndex)
    {
        auto& bank = metallicBanks_[bankIndex];
        bank = RelaxationOscillatorBank {};
        bank.instrument = instruments[bankIndex];

        for (std::size_t oscillatorIndex = 0;
             oscillatorIndex < bank.phases.size(); ++oscillatorIndex)
        {
            const auto seed = static_cast<std::uint32_t> (
                0x9e3779b9u * (bankIndex + 1u)
                + 0x85ebca6bu * (oscillatorIndex + 1u));
            bank.phases[oscillatorIndex] = 0.5f
                + 0.5f * signedUnitFromHash (seed ^ 0x243f6a88u);
            bank.fixedTolerances[oscillatorIndex] = signedUnitFromHash (
                seed ^ 0xb7e15162u);
            bank.dutyCycles[oscillatorIndex] = std::clamp (
                0.4798f + 0.010f * signedUnitFromHash (seed ^ 0x13198a2eu),
                0.445f, 0.515f);
            bank.thresholds[oscillatorIndex] = std::clamp (
                0.50f + 0.035f * signedUnitFromHash (seed ^ 0x03707344u),
                0.40f, 0.60f);
        }

        const auto values = snapshotParameters (bank.instrument);
        bank.lastParameterPitch = values.pitch;
        bank.lastParameterCharacterA = values.characterA;
        configureMetallicOscillatorBank (
            bank.instrument, std::exp2 (values.pitch / 12.0f),
            values.characterA, true);

        // Fill the complete reconstruction history from the running circuit
        // rather than exposing a zero-state filter transient on the first hit.
        for (int substep = 0; substep < metallicDecimatorTapCount_; ++substep)
        {
            bank.decimatorHistory[static_cast<std::size_t> (
                bank.decimatorWriteIndex)] = renderMetallicBankSubstep (bank);
            if (++bank.decimatorWriteIndex >= maximumMetallicDecimatorTaps)
                bank.decimatorWriteIndex = 0;
        }
    }
}

void DrumEngine::wakeMetallicOscillatorBanks() noexcept
{
    // The engine renders exact digital silence while no voice exists, so the
    // free-running metallic circuits are frozen instead of advanced sample by
    // sample. This restores the state they would have reached: short gaps are
    // replayed substep-exactly, longer gaps advance every phase analytically,
    // snap the capacitors onto their settled periodic orbit, and re-render one
    // full reconstruction history. The gap length is an absolute sample count,
    // so the result is independent of host block partitioning.
    if (metallicFrozenSamples_ == 0u)
        return;

    const auto substepsToCover = metallicFrozenSamples_
        * static_cast<std::uint64_t> (metallicOversampleFactor_);
    const auto warmupSubsteps = static_cast<std::uint64_t> (metallicDecimatorTapCount_);
    // Short gaps are replayed substep-exactly, so a briefly idle engine stays
    // sample-identical to one that kept rendering (a tested superposition
    // contract). Beyond this bound the smoothed increments and capacitors have
    // long settled onto their periodic orbit, which the analytic jump restores
    // directly; the bound also caps the wake cost at well under a millisecond.
    constexpr std::uint64_t exactReplaySubsteps = 2048;
    metallicFrozenSamples_ = 0;

    for (auto& bank : metallicBanks_)
    {
        auto replaySubsteps = substepsToCover;
        if (substepsToCover > exactReplaySubsteps)
        {
            const auto advance = substepsToCover - warmupSubsteps;
            for (int oscillatorIndex = 0; oscillatorIndex < bank.activeOscillators;
                 ++oscillatorIndex)
            {
                const auto index = static_cast<std::size_t> (oscillatorIndex);
                const double travelled = static_cast<double> (bank.targetIncrements[index])
                    * static_cast<double> (advance);
                bank.phases[index] = std::clamp (
                    static_cast<float> (bank.phases[index]
                        + travelled - std::floor (bank.phases[index] + travelled)),
                    0.0f, 1.0f);
            }
            // Snap increments and capacitor states to the settled values the
            // smoothed circuit converges to within a couple of milliseconds.
            configureMetallicOscillatorBank (
                bank.instrument, std::exp2 (bank.lastParameterPitch / 12.0f),
                bank.lastParameterCharacterA, true);
            replaySubsteps = warmupSubsteps;
        }

        for (std::uint64_t substep = 0; substep < replaySubsteps; ++substep)
        {
            bank.decimatorHistory[static_cast<std::size_t> (
                bank.decimatorWriteIndex)] = renderMetallicBankSubstep (bank);
            if (++bank.decimatorWriteIndex >= maximumMetallicDecimatorTaps)
                bank.decimatorWriteIndex = 0;
        }
    }
}

void DrumEngine::updateMetallicBankParameterTargets() noexcept
{
    for (auto& bank : metallicBanks_)
    {
        const auto values = snapshotParameters (bank.instrument);
        if (values.pitch == bank.lastParameterPitch
            && values.characterA == bank.lastParameterCharacterA)
            continue;

        configureMetallicOscillatorBank (
            bank.instrument, std::exp2 (values.pitch / 12.0f),
            values.characterA, false);
        bank.lastParameterPitch = values.pitch;
        bank.lastParameterCharacterA = values.characterA;
    }
}

void DrumEngine::configureMetallicOscillatorBank (Instrument instrument,
                                                   float pitchRatio,
                                                   float characterA,
                                                   bool snap) noexcept
{
    const int bankIndex = metallicBankIndexFor (instrument);
    if (bankIndex < 0)
        return;

    auto& bank = metallicBanks_[static_cast<std::size_t> (bankIndex)];
    bank.characterA = std::clamp (characterA, 0.0f, 1.0f);
    pitchRatio = std::clamp (pitchRatio, 0.20f, 4.20f);

    static constexpr std::array<float, metallicOscillatorCount> hatRatios {
        1.0f, 1.342f, 1.778f, 2.133f, 2.697f, 3.415f
    };
    // Measured nominal HD14584 oscillator frequencies from the classic
    // six-inverter cymbal source. The 800/540 Hz pair also anchors Perc 1.
    static constexpr std::array<float, metallicOscillatorCount> cymbalFrequencies {
        205.3f, 369.6f, 304.4f, 522.7f, 800.0f, 540.0f
    };

    bank.activeOscillators = instrument == Instrument::Perc1
        ? 2 : metallicOscillatorCount;
    for (int oscillatorIndex = 0; oscillatorIndex < bank.activeOscillators;
         ++oscillatorIndex)
    {
        const auto index = static_cast<std::size_t> (oscillatorIndex);
        float frequency = 0.0f;
        float toleranceDepth = 0.004f;
        if (instrument == Instrument::ClosedHat || instrument == Instrument::OpenHat)
        {
            const float alternating = (oscillatorIndex & 1) == 0 ? -1.0f : 1.0f;
            frequency = 1550.0f * hatRatios[index]
                * (1.0f + alternating * 0.025f * bank.characterA);
            toleranceDepth = 0.006f;
        }
        else if (instrument == Instrument::Ride || instrument == Instrument::Crash)
        {
            frequency = cymbalFrequencies[index]
                * (instrument == Instrument::Crash ? 0.94f : 1.0f);
            toleranceDepth = oscillatorIndex < 4
                ? 0.004f + (instrument == Instrument::Crash
                                ? 0.004f * bank.characterA : 0.0f)
                : 0.018f + (instrument == Instrument::Crash
                                ? 0.012f * bank.characterA : 0.002f);
        }
        else
        {
            frequency = oscillatorIndex == 0
                ? 535.0f
                : 535.0f * (1.34f + 0.42f * bank.characterA);
            toleranceDepth = 0.008f;
        }

        frequency *= pitchRatio
            * (1.0f + toleranceDepth * bank.fixedTolerances[index]);
        const float increment = std::clamp (
            frequency * metallicInverseSampleRate_, 1.0e-7f, 0.45f);
        bank.targetIncrements[index] = increment;
        if (snap || bank.currentIncrements[index] <= 0.0f)
            bank.currentIncrements[index] = increment;

        const float threshold = bank.thresholds[index];
        const float logarithmicSwing = std::log (
            (1.0f + threshold) / std::max (0.05f, 1.0f - threshold));
        const float duty = bank.dutyCycles[index];
        bank.riseCoefficients[index] = 1.0f - std::exp (
            -logarithmicSwing * increment / std::max (0.10f, duty));
        bank.fallCoefficients[index] = 1.0f - std::exp (
            -logarithmicSwing * increment / std::max (0.10f, 1.0f - duty));

        if (snap)
        {
            const float phase = bank.phases[index];
            if (phase < duty)
            {
                const float normalized = phase / std::max (0.10f, duty);
                bank.capacitorStates[index] = 1.0f
                    - (1.0f + threshold)
                        * std::exp (-logarithmicSwing * normalized);
            }
            else
            {
                const float normalized = (phase - duty)
                    / std::max (0.10f, 1.0f - duty);
                bank.capacitorStates[index] = -1.0f
                    + (1.0f + threshold)
                        * std::exp (-logarithmicSwing * normalized);
            }
        }
    }
}

float DrumEngine::renderMetallicBankSubstep (
    RelaxationOscillatorBank& bank) noexcept
{
    std::array<float, metallicOscillatorCount> pulses {};
    std::array<float, metallicOscillatorCount> capacitors {};
    for (int oscillatorIndex = 0; oscillatorIndex < bank.activeOscillators;
         ++oscillatorIndex)
    {
        const auto index = static_cast<std::size_t> (oscillatorIndex);
        bank.currentIncrements[index] += metallicIncrementSmoothing_
            * (bank.targetIncrements[index] - bank.currentIncrements[index]);
        const float increment = std::clamp (
            bank.currentIncrements[index], 1.0e-7f, 0.45f);
        float phase = bank.phases[index] + increment;
        phase -= std::floor (phase);
        bank.phases[index] = phase;

        const float duty = bank.dutyCycles[index];
        float pulse = phase < duty ? 1.0f : -1.0f;
        pulse += polyBlep (phase, increment);
        float fallingPhase = phase - duty;
        if (fallingPhase < 0.0f)
            fallingPhase += 1.0f;
        pulse -= polyBlep (fallingPhase, increment);
        // Remove the exact duty-cycle DC term before the downstream channel
        // filters. Component mismatch still changes harmonic balance without
        // making overlapping cymbals pull on the master DC blocker.
        pulses[index] = pulse - (2.0f * duty - 1.0f);

        const float target = phase < duty ? 1.0f : -1.0f;
        const float coefficient = phase < duty
            ? bank.riseCoefficients[index] : bank.fallCoefficients[index];
        bank.capacitorStates[index] += coefficient
            * (target - bank.capacitorStates[index]);
        capacitors[index] = bank.capacitorStates[index]
            / std::max (0.25f, bank.thresholds[index]);
    }

    if (bank.instrument == Instrument::ClosedHat
        || bank.instrument == Instrument::OpenHat)
    {
        const float pulseRings = pulses[0] * pulses[1]
                               + pulses[2] * pulses[3]
                               + pulses[4] * pulses[5];
        const float capacitorRings = capacitors[0] * capacitors[1]
                                   + capacitors[2] * capacitors[3]
                                   + capacitors[4] * capacitors[5];
        const float tones = pulses[0] + pulses[2] + pulses[4];
        return (0.18f + 0.12f * bank.characterA)
                   * (0.82f * pulseRings + 0.18f * capacitorRings)
             + 0.07f * bank.characterA * tones;
    }

    if (bank.instrument == Instrument::Perc1)
    {
        const float first = pulses[0] + 0.16f * capacitors[0];
        const float second = pulses[1] + 0.16f * capacitors[1];
        return 0.42f * first + 0.34f * second + 0.14f * first * second;
    }

    float sum = 0.0f;
    for (int oscillatorIndex = 0; oscillatorIndex < bank.activeOscillators;
         ++oscillatorIndex)
        sum += pulses[static_cast<std::size_t> (oscillatorIndex)];
    return sum / static_cast<float> (bank.activeOscillators);
}

float DrumEngine::decimateMetallicBank (
    const RelaxationOscillatorBank& bank) const noexcept
{
    float reconstructed = 0.0f;
    int recentIndex = bank.decimatorWriteIndex - 1;
    if (recentIndex < 0)
        recentIndex += maximumMetallicDecimatorTaps;
    int oldestIndex = bank.decimatorWriteIndex - metallicDecimatorTapCount_;
    if (oldestIndex < 0)
        oldestIndex += maximumMetallicDecimatorTaps;

    // The linear-phase Kaiser kernel is symmetric. Pair equidistant history
    // samples before multiplying to halve the active-bank reconstruction cost
    // without changing its response or persistent state.
    const int centreTap = metallicDecimatorTapCount_ / 2;
    for (int tap = 0; tap < centreTap; ++tap)
    {
        reconstructed += metallicDecimatorCoefficients_[static_cast<std::size_t> (tap)]
            * (bank.decimatorHistory[static_cast<std::size_t> (recentIndex)]
               + bank.decimatorHistory[static_cast<std::size_t> (oldestIndex)]);
        if (--recentIndex < 0)
            recentIndex += maximumMetallicDecimatorTaps;
        if (++oldestIndex >= maximumMetallicDecimatorTaps)
            oldestIndex = 0;
    }
    reconstructed += metallicDecimatorCoefficients_[static_cast<std::size_t> (centreTap)]
        * bank.decimatorHistory[static_cast<std::size_t> (recentIndex)];
    return reconstructed;
}

void DrumEngine::renderMetallicOscillatorBanks (
    std::uint32_t activeBankMask) noexcept
{
    for (std::size_t bankIndex = 0; bankIndex < metallicBanks_.size(); ++bankIndex)
    {
        auto& bank = metallicBanks_[bankIndex];
        float latestSource = 0.0f;
        for (int substep = 0; substep < metallicOversampleFactor_; ++substep)
        {
            latestSource = renderMetallicBankSubstep (bank);
            bank.decimatorHistory[static_cast<std::size_t> (
                bank.decimatorWriteIndex)] = latestSource;
            if (++bank.decimatorWriteIndex >= maximumMetallicDecimatorTaps)
                bank.decimatorWriteIndex = 0;
        }

        // All oscillator/capacitor/filter state keeps advancing through a
        // closed VCA. The expensive convolution is only needed while a voice
        // can actually observe this bank.
        const bool isActive = (activeBankMask
            & (std::uint32_t { 1 } << static_cast<unsigned> (bankIndex))) != 0u;
        bank.output = isActive
            ? (metallicOversampleFactor_ > 1
                   ? decimateMetallicBank (bank) : latestSource)
            : 0.0f;
    }
}

float DrumEngine::metallicSourceFor (Instrument instrument) const noexcept
{
    const int bankIndex = metallicBankIndexFor (instrument);
    return bankIndex >= 0
        ? metallicBanks_[static_cast<std::size_t> (bankIndex)].output : 0.0f;
}

float DrumEngine::nextCymbalPcm (Voice& voice, float source) const noexcept
{
    // The TR-909 cymbals replayed compressed PCM at roughly 30 kHz. Drumalor
    // remains fully synthesized, but this voice-local clock and 63-level
    // quantizer contribute the same held-DAC grain to a generated oscillator/
    // noise composite. No sample or copyrighted ROM data is embedded.
    voice.cymbalClockPhase += cymbalClockIncrement_;
    if (voice.cymbalClockPhase >= 1.0f)
    {
        voice.cymbalClockPhase -= std::floor (voice.cymbalClockPhase);
        const float decorrelation = 0.18f * nextNoise (voice);
        const float composite = std::clamp (source + decorrelation, -1.0f, 1.0f);
        voice.cymbalPcmValue = std::round (31.0f * composite) * (1.0f / 31.0f);
    }
    // A real reconstruction network does not expose the held DAC steps
    // directly. This exact one-pole update removes their broadband digital
    // edge while retaining the audible 30 kHz clock grain at ordinary rates.
    voice.cymbalPcmReconstructed += cymbalReconstructionCoefficient_
        * (voice.cymbalPcmValue - voice.cymbalPcmReconstructed);
    return voice.cymbalPcmReconstructed;
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
    for (const auto& voice : retiringVoices_)
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
                                  const InstrumentParameters& values, std::uint32_t seed,
                                  const HitVariation& variation) noexcept
{
    voice = Voice {};
    voice.active = true;
    voice.instrument = instrument;
    voice.generation = ++generation_;
    voice.noiseState = seed == 0u ? 1u : seed;
    voice.velocity = velocity * (0.68f + 0.32f * std::sqrt (velocity));
    voice.recentPeak = voice.velocity;
    voice.characterA = std::clamp (values.characterA + variation.characterAOffset,
                                   0.0f, 1.0f);
    voice.characterB = std::clamp (values.characterB + variation.characterBOffset,
                                   0.0f, 1.0f);
    voice.pitchRatio = std::exp2 ((values.pitch + 0.01f * variation.pitchCents) / 12.0f);
    voice.decaySeconds = decaySecondsFor (instrument, values.decay)
        * variation.decayScale;
    voice.transientScale = variation.transientScale;
    // Treat MIDI velocity as trigger/accent voltage as well as final VCA
    // loudness. The deliberately narrow range preserves the established gain
    // curve while making hard hits inject more energy into the physical core.
    voice.excitationScale = 0.74f + 0.26f * std::sqrt (velocity);
    voice.circuitDrive = std::clamp (
        1.10f + 0.48f * voice.characterB + variation.circuitDriveOffset,
        1.02f, 1.72f);
    voice.circuitBias = variation.circuitBias;
    // ADAA starts from the quiescent circuit operating point. This avoids a
    // fictitious interval from zero to the bias voltage on the first sample.
    voice.analogPreviousInput = voice.circuitBias;
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

    // A triggered analogue oscillator rarely begins at precisely the same
    // capacitor voltage. Keep the displacement small for punch consistency,
    // but seed it per hit so even tonal voices do not become static samples.
    for (std::size_t oscillatorIndex = 0; oscillatorIndex < voice.phases.size(); ++oscillatorIndex)
    {
        const float oscillatorOffset = signedUnitFromHash (
            seed ^ static_cast<std::uint32_t> (0x4f1bbcdcu + oscillatorIndex * 0x9e3779b9u));
        voice.phases[oscillatorIndex] = variation.phaseOffset
            * (0.72f + 0.28f * oscillatorOffset);
        const auto fixedSeed = static_cast<std::uint32_t> (
            (indexFor (instrument) + 1u) * 0x9e3779b9u
            + (oscillatorIndex + 1u) * 0x85ebca6bu);
        voice.oscillatorAsymmetries[oscillatorIndex] = std::clamp (
            0.82f * signedUnitFromHash (fixedSeed ^ 0x3c6ef372u)
                + 0.18f * oscillatorOffset,
            -1.0f, 1.0f);
    }

    // Short, increasingly dense acoustic modes give the synthetic 909 layer
    // body without allowing a handful of low partials to cling for the tail.
    static constexpr float rideRatios[12] {
        1.0f, 1.431f, 2.097f, 3.042f, 4.181f, 5.528f,
        6.958f, 8.694f, 10.736f, 12.944f, 15.347f, 17.778f
    };
    static constexpr float crashRatios[12] {
        1.0f, 1.468f, 2.129f, 3.032f, 4.161f, 5.548f,
        7.177f, 9.032f, 11.129f, 13.387f, 15.968f, 18.871f
    };

    switch (instrument)
    {
        case Instrument::Kick:
            // A charged energy reservoir excites a stable rotating two-state
            // resonator below. Its settled default is 48 Hz, leaving genuine
            // sub-100 Hz weight while retaining the full pitch range.
            voice.baseFrequency = 48.0f * voice.pitchRatio;
            voice.sweepAmount = 0.70f + 4.0f * voice.characterA;
            voice.pitchEnvelopeMultiplier = coefficientForTime (0.016f + 0.050f * voice.characterA,
                                                                  static_cast<float> (sampleRate_));
            voice.transientMultiplier = coefficientForTime (0.0020f + 0.0045f * voice.characterA,
                                                             static_cast<float> (sampleRate_));
            voice.kickCharge = (0.92f + 0.16f * voice.characterA)
                * voice.transientScale * voice.excitationScale;
            voice.kickChargeMultiplier = coefficientForTime (
                0.00045f + 0.00085f * voice.characterA,
                static_cast<float> (sampleRate_));
            voice.kickBaseRadius = coefficientForTime (
                voice.decaySeconds * 1.08f, static_cast<float> (sampleRate_));
            voice.circuitDrive = std::clamp (
                1.25f + 3.2f * voice.characterB + variation.circuitDriveOffset,
                1.15f, 4.65f);
            // Drive also moves the nonlinear operating point and the mismatch
            // between the virtual diode/transistor branches. This creates the
            // musically useful even harmonics of a biased analogue stage.
            voice.circuitBias += 0.12f * voice.characterB;
            voice.analogPreviousInput = voice.circuitBias;
            configureBandpass (voice.filterA,
                               1900.0f + 5400.0f * voice.characterA, 0.72f);
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
            configureMetallicOscillatorBank (
                instrument, voice.pitchRatio, voice.characterA, false);
            configureHighpass (voice.filterA, 3400.0f + 6500.0f * voice.characterB, 0.70f);
            configureBandpass (voice.filterB, 6500.0f + 4800.0f * voice.characterB, 0.85f);
            voice.transientMultiplier = coefficientForTime (instrument == Instrument::ClosedHat ? 0.0025f : 0.006f,
                                                             static_cast<float> (sampleRate_));
            break;
        }

        case Instrument::Ride:
        {
            configureMetallicOscillatorBank (
                instrument, voice.pitchRatio, voice.characterA, false);

            const float modalPitch = std::pow (voice.pitchRatio, 0.74f);
            const float modalDecay = 0.16f + voice.decaySeconds
                * (0.13f + 0.08f * voice.characterA);
            initialiseModalVoice (voice, rideRatios, 12, 720.0f * modalPitch,
                                  modalDecay, 0.09f, 0.56f + 0.28f * voice.characterB);
            const float filterPitch = std::pow (voice.pitchRatio, 0.46f);
            configureBandpass (voice.filterA, 3440.0f * filterPitch, 0.68f);
            configureBandpass (voice.filterB, 7100.0f * filterPitch, 0.76f);
            configureBandpass (voice.filterC, 10500.0f * filterPitch, 0.90f);
            voice.envelopeMultiplier = coefficientForTime (
                voice.decaySeconds * 0.88f, static_cast<float> (sampleRate_));
            voice.auxiliaryMultiplier = coefficientForTime (
                voice.decaySeconds * 1.06f, static_cast<float> (sampleRate_));
            voice.transientMultiplier = coefficientForTime (
                0.0022f + 0.0038f * voice.characterA,
                static_cast<float> (sampleRate_));
            break;
        }

        case Instrument::Crash:
        {
            configureMetallicOscillatorBank (
                instrument, voice.pitchRatio, voice.characterA, false);

            const float modalPitch = std::pow (voice.pitchRatio, 0.72f);
            const float modalDecay = 0.12f + voice.decaySeconds
                * (0.10f + 0.08f * voice.characterA);
            initialiseModalVoice (voice, crashRatios, 12, 620.0f * modalPitch,
                                  modalDecay, 0.12f + 0.36f * voice.characterA,
                                  0.58f + 0.30f * voice.characterB);
            const float filterPitch = std::pow (voice.pitchRatio, 0.44f);
            configureBandpass (voice.filterA, 3440.0f * filterPitch, 0.62f);
            configureBandpass (voice.filterB, 7100.0f * filterPitch, 0.72f);
            configureBandpass (voice.filterC, 10500.0f * filterPitch, 0.84f);
            voice.envelopeMultiplier = coefficientForTime (
                voice.decaySeconds * 0.80f, static_cast<float> (sampleRate_));
            voice.auxiliaryMultiplier = coefficientForTime (
                voice.decaySeconds * 1.12f, static_cast<float> (sampleRate_));
            voice.transientMultiplier = coefficientForTime (
                0.0035f + 0.0065f * voice.characterA,
                static_cast<float> (sampleRate_));
            voice.pitchEnvelopeMultiplier = coefficientForTime (
                0.010f + 0.020f * voice.characterA,
                static_cast<float> (sampleRate_));
            break;
        }

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
            configureMetallicOscillatorBank (
                instrument, voice.pitchRatio, voice.characterA, false);
            configureBandpass (voice.filterA, std::min (6200.0f, voice.baseFrequency * 1.55f), 1.0f);
            configureHighpass (voice.filterB, 1800.0f, 0.70f);
            voice.transientMultiplier = coefficientForTime (0.0035f, static_cast<float> (sampleRate_));
            voice.circuitDrive = std::clamp (
                1.15f + 2.4f * voice.characterB + variation.circuitDriveOffset,
                1.05f, 3.75f);
            break;
        }

        case Instrument::Perc2:
        {
            const float hollow = voice.characterA;
            const float ratios[4] { 1.0f, 1.42f + 0.35f * hollow,
                                    2.31f + 0.55f * hollow, 3.84f - 0.40f * hollow };
            initialiseModalVoice (voice, ratios, 4, 930.0f * voice.pitchRatio,
                                  voice.decaySeconds, 0.055f, 0.35f + 0.35f * hollow);
            configureBandpass (voice.filterA, 2800.0f + 5000.0f * voice.characterB, 0.85f);
            configureHighpass (voice.filterB, 350.0f + 550.0f * (1.0f - hollow), 0.70f);
            voice.transientMultiplier = coefficientForTime (0.0015f + 0.003f * voice.characterB,
                                                             static_cast<float> (sampleRate_));
            break;
        }

        case Instrument::Count:
            break;
    }

    const int metallicBankIndex = metallicBankIndexFor (instrument);
    if (metallicBankIndex >= 0)
    {
        auto& bank = metallicBanks_[static_cast<std::size_t> (metallicBankIndex)];
        // The target contains this hit's tiny analogue tolerance, while these
        // values remember the nominal control position. Silent automation can
        // then retune the free-running source without erasing per-hit drift.
        bank.lastParameterPitch = values.pitch;
        bank.lastParameterCharacterA = values.characterA;
    }
}

void DrumEngine::trigger (Instrument instrument, float velocity) noexcept
{
    if (! validInstrument (instrument) || ! std::isfinite (velocity) || velocity <= 0.0f)
        return;
    if (! prepared_)
        prepare (sampleRate_, maxBlockSize_);
    // Restore the skipped interval under the targets that were active during
    // that interval, then publish any control change that arrived with this
    // trigger before the first audible sample is rendered.
    wakeMetallicOscillatorBanks();
    updateMetallicBankParameterTargets();
    velocity = std::clamp (velocity, 0.0f, 1.0f);
    if (instrument == Instrument::ClosedHat || instrument == Instrument::OpenHat)
        chokeHats();

    const auto index = indexFor (instrument);
    const auto counter = ++triggerCounters_[index];
    const std::uint32_t seed = hash32 (0x6d2b79f5u
        ^ static_cast<std::uint32_t> ((index + 1u) * 0x9e3779b9u)
        ^ static_cast<std::uint32_t> (counter)
        ^ static_cast<std::uint32_t> (counter >> 32u));

    // Component values in an analogue voice do not jump independently on
    // every strike: temperature, supply and capacitor history move slowly.
    // Model that as bounded, correlated trigger-domain drift, then layer very
    // small per-hit tolerances on top. Hash-derived values keep the sequence
    // reproducible after reset and independent of process block boundaries.
    auto& drift = componentDrift_[index];
    drift = std::clamp (0.86f * drift
                           + 0.14f * signedUnitFromHash (seed ^ 0xa341316cu),
                        -1.0f, 1.0f);
    HitVariation variation;
    variation.pitchCents = 3.2f * drift
        + 2.4f * signedUnitFromHash (seed ^ 0xc8013ea4u);
    variation.decayScale = std::clamp (
        1.0f + 0.025f * drift
            + 0.016f * signedUnitFromHash (seed ^ 0xad90777du),
        0.955f, 1.045f);
    variation.characterAOffset = 0.018f * drift
        + 0.014f * signedUnitFromHash (seed ^ 0x7e95761eu);
    variation.characterBOffset = -0.014f * drift
        + 0.016f * signedUnitFromHash (seed ^ 0x3c6ef372u);
    variation.transientScale = std::clamp (
        1.0f - 0.035f * drift
            + 0.035f * signedUnitFromHash (seed ^ 0xbb67ae85u),
        0.925f, 1.075f);
    variation.circuitDriveOffset = 0.035f * drift
        + 0.045f * signedUnitFromHash (seed ^ 0x1b873593u);
    variation.circuitBias = 0.010f * drift
        + 0.007f * signedUnitFromHash (seed ^ 0x85ebca6bu);
    variation.phaseOffset = 0.010f * drift
        + 0.012f * signedUnitFromHash (seed ^ 0xc2b2ae35u);
    auto& voice = voices_[static_cast<std::size_t> (findVoiceSlot())];
    if (voice.active && voice.ageSamples != 0u)
        retireVoice (voice);
    initialiseVoice (voice, instrument, velocity, snapshotParameters (instrument), seed,
                     variation);
    anyVoiceActive_ = true;
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
    // Stable time-varying energy-state resonator. Unlike directly changing a
    // biquad's coefficients, each update is a rotation followed by an explicit
    // contraction, so rapid pitch sweeps cannot inject unbounded state energy.
    // Euclidean state energy is invariant under the quadrature rotation.
    // The former L1 estimate changed with phase and unintentionally modulated
    // pitch and damping at twice the kick frequency. The scale preserves its
    // former average operating range while removing that digital fingerprint.
    const float stateMagnitude = std::min (
        1.5f, 1.27323954f * std::sqrt (
            voice.kickStateX * voice.kickStateX
            + voice.kickStateY * voice.kickStateY));
    const float triggerSweep = 0.84f + 0.16f * voice.velocity;
    const float amplitudePitch = 1.0f
        + 0.016f * voice.characterB * stateMagnitude;
    const float frequency = std::clamp (
        voice.baseFrequency
            * (1.0f + voice.sweepAmount * triggerSweep * voice.pitchEnvelope)
            * amplitudePitch,
        4.0f, 0.18f * static_cast<float> (sampleRate_));

    const float angle = twoPi * frequency * inverseSampleRate_;
    const float angleSquared = angle * angle;
    const float angleFourth = angleSquared * angleSquared;
    const float angleSixth = angleFourth * angleSquared;
    float sine = angle * (1.0f - angleSquared / 6.0f
                          + angleFourth / 120.0f - angleSixth / 5040.0f);
    float cosine = 1.0f - angleSquared / 2.0f + angleFourth / 24.0f
        - angleSixth / 720.0f + angleFourth * angleFourth / 40320.0f;
    // One Newton normalization keeps the polynomial rotation on the unit
    // circle, preventing approximation error from becoming hidden damping.
    const float normCorrection = 1.5f
        - 0.5f * (sine * sine + cosine * cosine);
    sine *= normCorrection;
    cosine *= normCorrection;

    // Diode/conductor losses rise with stored energy. Applying the loss as a
    // positive contraction keeps it stable at every supported sample rate.
    const float nonlinearLossPerSecond = (0.45f + 3.5f * voice.characterB)
        * stateMagnitude;
    const float dynamicLoss = std::clamp (
        1.0f - nonlinearLossPerSecond * inverseSampleRate_, 0.98f, 1.0f);
    const float radius = std::clamp (
        voice.kickBaseRadius * dynamicLoss, 0.0f, 0.9999995f);

    // The trigger charges a virtual capacitor; its normalized discharge sums
    // to the stored charge independently of sample rate.
    const float discharge = voice.kickCharge
        * (1.0f - voice.kickChargeMultiplier);
    voice.kickCharge *= voice.kickChargeMultiplier;
    const float stateX = voice.kickStateX + discharge;
    const float stateY = voice.kickStateY;
    voice.kickStateX = radius * (cosine * stateX - sine * stateY);
    voice.kickStateY = radius * (sine * stateX + cosine * stateY);

    const float click = voice.filterA.tick (nextNoise (voice)) * voice.transientEnvelope
        * voice.transientScale * (0.08f + 0.25f * voice.characterA);
    const float body = (1.30f + 0.16f * voice.characterB) * voice.kickStateY;
    // Harmonics and level-dependent coloration are intentionally delegated to
    // the shared antialiased circuit stage, avoiding a second aliasing clipper.
    return body + click;
}

float DrumEngine::renderSnare (Voice& voice) noexcept
{
    const float body = (0.72f * oscillator (voice, 0) + 0.36f * oscillator (voice, 1))
        * voice.envelope;
    const float noise = nextNoise (voice);
    const float wires = voice.filterA.tick (noise) * voice.auxiliaryEnvelope;
    const float snap = voice.filterB.tick (noise) * voice.transientEnvelope
        * voice.transientScale;
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
    const float burst = (0.38f + 0.16f * voice.characterA)
        * voice.transientEnvelope * voice.transientScale;
    const float tail = (0.13f + 0.24f * voice.characterB) * voice.envelope;
    return noise * (burst + tail);
}

float DrumEngine::renderHat (Voice& voice) noexcept
{
    const float noise = nextNoise (voice);
    // The persistent Schmitt/RC bank is evaluated once per engine sample, so
    // overlapping hits hear the same free-running hardware source instead of
    // restarting six ideal sines with newly randomized components.
    const float metallic = metallicSourceFor (voice.instrument)
                         + 0.20f * (1.0f - voice.characterA) * noise;
    const float high = voice.filterA.tick (metallic);
    const float focused = voice.filterB.tick (metallic);
    const float attack = 0.12f * voice.transientEnvelope * noise * voice.transientScale;
    return (0.58f * high + (0.18f + 0.20f * voice.characterB) * focused + attack)
        * voice.envelope;
}

float DrumEngine::renderRide (Voice& voice) noexcept
{
    const float oscillatorBank = metallicSourceFor (voice.instrument);
    const float noise = nextModalNoise (voice);
    const float pcm = nextCymbalPcm (
        voice, 0.68f * oscillatorBank + 0.24f * noise);

    // Three circuit bands mirror the useful structure of the 808 cymbal,
    // while the quantized generated layer fills the continuous spectrum that
    // made the sample-based 909 ride sit easily in a mix.
    const float bodyBand = voice.filterA.tick (
        0.78f * oscillatorBank + 0.22f * pcm);
    const float shimmerBand = voice.filterB.tick (
        0.58f * oscillatorBank + 0.42f * pcm);
    const float airBand = voice.filterC.tick (
        0.34f * oscillatorBank + 0.66f * pcm);

    const float contact = voice.ageSamples == 0u
        ? 2.15f * voice.transientScale * voice.excitationScale
        : 0.055f * modalNoiseScale_ * noise * voice.transientEnvelope
            * voice.transientScale * voice.excitationScale;
    float modes = 0.0f;
    for (std::size_t mode = 0; mode < resonatorCount; ++mode)
    {
        float gain = voice.modeGains[mode];
        gain *= mode < 4
            ? 0.50f + 1.40f * voice.characterA
            : 0.90f - 0.25f * voice.characterA;
        modes += gain * voice.resonators[mode].tick (contact);
    }

    const float bell = voice.characterA;
    const float tone = voice.characterB;
    const float body = (0.46f + 0.50f * bell - 0.10f * tone)
        * bodyBand * voice.envelope;
    const float shimmer = (0.18f + 0.34f * tone)
        * shimmerBand * voice.auxiliaryEnvelope;
    const float air = (0.065f + 0.255f * tone)
        * airBand * voice.auxiliaryEnvelope;
    const float bellModes = (0.12f + 0.42f * bell) * modes;
    return 1.12f * (body + shimmer + air + bellModes);
}

float DrumEngine::renderCrash (Voice& voice) noexcept
{
    const float oscillatorBank = metallicSourceFor (voice.instrument);
    const float noise = nextModalNoise (voice);
    const float pcm = nextCymbalPcm (
        voice, 0.52f * oscillatorBank + 0.34f * noise);
    const float spread = voice.characterA;
    const float coherent = 0.68f - 0.28f * spread;
    const float quantized = 0.32f + 0.18f * spread;
    const float diffuse = 0.10f * spread;
    const float source = coherent * oscillatorBank + quantized * pcm
                       + diffuse * noise;

    const float bodyBand = voice.filterA.tick (source);
    const float shimmerBand = voice.filterB.tick (
        (0.86f - 0.18f * spread) * source + 0.18f * spread * noise);
    const float airBand = voice.filterC.tick (
        (0.72f - 0.20f * spread) * source + 0.28f * spread * noise);

    // The modal layer is struck briefly and then left alone. The long tail is
    // carried by diffuse oscillator/PCM bands, avoiding the old continuously
    // driven resonators that exposed a handful of clinging pitches.
    const float excitation = voice.ageSamples == 0u
        ? 1.75f * voice.transientScale * voice.excitationScale
        : 0.075f * modalNoiseScale_ * noise * voice.transientEnvelope
            * voice.transientScale * voice.excitationScale;
    float modes = 0.0f;
    for (std::size_t mode = 0; mode < resonatorCount; ++mode)
        modes += voice.modeGains[mode] * voice.resonators[mode].tick (excitation);

    const float brightness = voice.characterB;
    const float bloom = 0.34f + 0.66f * (1.0f - voice.pitchEnvelope);
    const float body = (0.43f - 0.11f * brightness)
        * bodyBand * voice.envelope;
    const float shimmer = (0.17f + 0.38f * brightness)
        * shimmerBand * voice.auxiliaryEnvelope * bloom;
    const float air = (0.07f + 0.32f * brightness)
        * airBand * voice.auxiliaryEnvelope * bloom;
    const float struckMetal = (0.20f - 0.07f * spread) * modes;
    return 1.18f * (body + shimmer + air + struckMetal);
}

float DrumEngine::renderTom (Voice& voice) noexcept
{
    const float membraneStiffness = 1.0f + 0.004f * voice.characterB
        * voice.excitationScale * voice.envelope;
    const float frequency = voice.baseFrequency
        * (1.0f + voice.sweepAmount * voice.pitchEnvelope)
        * membraneStiffness;
    voice.phaseIncrements[0] = std::min (0.45f, frequency * inverseSampleRate_);
    const float fundamental = oscillator (voice, 0);
    const float shell = oscillator (voice, 1);
    const float skin = voice.filterA.tick (nextNoise (voice)) * voice.transientEnvelope
        * voice.transientScale;
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
        voice.transientEnvelope += voice.transientScale * voice.excitationScale
            * (0.45f + 0.55f * std::abs (grainNoise));
    const float grains = voice.filterB.tick (voice.filterA.tick (
        grainNoise * voice.transientEnvelope));
    return 0.95f * grains * voice.envelope;
}

float DrumEngine::renderPerc1 (Voice& voice) noexcept
{
    const float metallic = metallicSourceFor (voice.instrument);
    const float click = voice.filterB.tick (nextNoise (voice)) * voice.transientEnvelope
        * voice.transientScale;
    const float shaped = voice.filterA.tick (metallic) * voice.envelope + 0.12f * click;
    // Drive is handled by the shared antialiased stage. Keeping a single
    // nonlinear memory here avoids cascading a memoryless alias source.
    return 1.05f * shaped;
}

float DrumEngine::renderPerc2 (Voice& voice) noexcept
{
    const float noise = nextModalNoise (voice);
    const float excitation = (voice.ageSamples == 0 ? voice.transientScale : 0.0f)
        + 0.10f * modalNoiseScale_ * voice.characterB * voice.transientEnvelope
            * voice.transientScale * noise;
    float body = 0.0f;
    for (std::size_t mode = 0; mode < 4; ++mode)
        body += voice.modeGains[mode] * voice.resonators[mode].tick (
            excitation * voice.excitationScale);
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
    output = applyAnalogOutputStage (voice, output);
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

    updateMetallicBankParameterTargets();

    // Voice activity can only decrease inside one engine chunk; triggers split
    // processing at their exact event offsets. Discover observable banks and
    // collect the audible voices once per chunk instead of rescanning both
    // 64-voice pools for every sample.
    std::uint32_t activeMetallicBankMask = 0u;
    std::array<Voice*, maxVoices + retiringVoiceCount> chunkVoices {};
    int chunkVoiceCount = 0;
    const auto observeVoice = [&activeMetallicBankMask, &chunkVoices,
                               &chunkVoiceCount] (Voice& voice)
    {
        if (! voice.active)
            return;
        chunkVoices[static_cast<std::size_t> (chunkVoiceCount++)] = &voice;
        const int bankIndex = metallicBankIndexFor (voice.instrument);
        if (bankIndex >= 0)
            activeMetallicBankMask |= std::uint32_t { 1 }
                << static_cast<unsigned> (bankIndex);
    };
    for (auto& voice : voices_)
        observeVoice (voice);
    for (auto& voice : retiringVoices_)
        observeVoice (voice);

    const float gainTarget = outputGain_.load (std::memory_order_relaxed);
    const float gainSmoothing = 1.0f - std::exp (-inverseSampleRate_ / 0.020f);
    const float dcCoefficient = std::exp (-twoPi * 12.0f * inverseSampleRate_);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // While no voice exists in either pool, every downstream stage is at
        // its zero rest state and the block is exact digital silence, so only
        // the output-gain smoother needs to advance. The frozen sample count
        // lets the next trigger restore the free-running metallic circuits to
        // the state they would have reached (see wakeMetallicOscillatorBanks).
        if (! anyVoiceActive_)
        {
            ++metallicFrozenSamples_;
            smoothedOutputGain_ += gainSmoothing * (gainTarget - smoothedOutputGain_);
            if (left != nullptr)
                left[sample] = 0.0f;
            if (right != nullptr && right != left)
                right[sample] = 0.0f;
            continue;
        }

        // Free-running component oscillators advance even when their VCAs are
        // closed. A later trigger therefore samples the circuit's actual phase
        // rather than restarting a synthetic waveform at note-on.
        renderMetallicOscillatorBanks (activeMetallicBankMask);
        float dryLeft = 0.0f;
        float dryRight = 0.0f;
        bool hasActiveVoices = false;
        for (int voiceIndex = 0; voiceIndex < chunkVoiceCount; ++voiceIndex)
        {
            auto& voice = *chunkVoices[static_cast<std::size_t> (voiceIndex)];
            if (! voice.active)
                continue;
            // Pan is captured first because a voice completing its tail this
            // sample is reset to defaults inside renderVoice.
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

        const float outputLeft = std::clamp (antialiasedRationalShaper (
            smoothedOutputGain_ * dcLeft, masterAdaaPreviousLeft_, 1.0f, 1.0f),
            -1.0f, 1.0f);
        const float outputRight = std::clamp (antialiasedRationalShaper (
            smoothedOutputGain_ * dcRight, masterAdaaPreviousRight_, 1.0f, 1.0f),
            -1.0f, 1.0f);
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
            masterAdaaPreviousLeft_ = masterAdaaPreviousRight_ = 0.0f;
        }
        anyVoiceActive_ = hasActiveVoices;
    }

    updateActiveVoiceCount();
}

} // namespace drumalor
