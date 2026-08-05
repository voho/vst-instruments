#include "YouKnow106Engine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace youknow106
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 6.28318530717958647692f;

// Signal levels use the established 2.6 V-per-unit model coordinate so the
// transconductor and BBD nonlinearities retain their existing drive. The
// service notes' 12 Vpp oscillator adjustment and 4 Vpp TP8 noise adjustment
// do not by themselves establish the complete loaded source-to-filter transfer;
// the 0.40 mixer coordinate remains a voiced compatibility value (OQ-15).
constexpr float filterInputAttenuation = 0.40f;
constexpr float voltsToSample = 1.0f / YouKnow106Engine::internalVoltsPerUnit;

// Voiced source-coordinate values constrained by the service anchors: saw and
// pulse are adjusted near 12 Vpp, while shared noise is adjusted to 4.0 Vpp at
// TP8. Intervening loading is still open, and the sub value has no equivalent
// end-to-end anchor, so these must not be presented as measured mixer voltages.
constexpr float sawMixVolts = 6.0f;
constexpr float pulseMixVolts = 6.0f;
constexpr float subMixVolts = 5.0f;
constexpr float noiseMixVolts = 2.0f;

// Voice-summer output coupling ahead of the four-position HPF selector:
// IC1a -> C14 10 uF NP -> R39 33 kOhm -> IC3 common input.
constexpr float voiceBusCouplingCapacitanceF = 10.0e-6f;
constexpr float voiceBusCouplingResistanceOhms = 33000.0f;

// Manufacturer application input for IC5/uPC1252H2, populated by Roland as
// C12 10 uF NP followed by R36 33 kOhm.
constexpr float commonVcaInputCapacitanceF = 10.0e-6f;
constexpr float commonVcaInputResistanceOhms = 33000.0f;

// Stereo post-IC6 coupling, identically C17/R54/VR1 and C20/R57/VR1. The fixed
// internal load is the complete selector ladder in parallel with IC7's input;
// external jack loads, normaling and driven headphone behavior remain OQ-17.
constexpr float outputCouplingCapacitanceF = 10.0e-6f;
constexpr float outputCouplingSeriesOhms = 1500.0f;
constexpr float outputCouplingPotOhms = 10000.0f;
constexpr float outputSelectorLadderOhms = 33000.0f + 6800.0f + 1500.0f;
constexpr float headphoneInputOhms = 1000.0f + 100000.0f;
constexpr float outputWiperInternalLoadOhms =
    outputSelectorLadderOhms * headphoneInputOhms
    / (outputSelectorLadderOhms + headphoneInputOhms);

// Voiced microscopic excitation at the filter input. It is distinct from the
// shared audible TP8 noise and gives an otherwise perfectly silent numerical
// filter a deterministic self-oscillation seed. No healthy-card capture fixes
// its 20 uV compatibility value yet.
constexpr float filterNoiseVolts = 2.0e-5f;

float clamp01(float value) noexcept
{
    return std::clamp(value, 0.0f, 1.0f);
}

float sanitised(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

std::uint8_t storedControlByte(float value) noexcept
{
    return static_cast<std::uint8_t>(
        std::floor(clamp01(sanitised(value, 0.0f)) * 127.0f + 0.5f));
}

float storedControlFraction(float value) noexcept
{
    return static_cast<float>(storedControlByte(value)) / 127.0f;
}

std::uint8_t portamentoAdcByte(float value) noexcept
{
    return static_cast<std::uint8_t>(
        std::floor(clamp01(sanitised(value, 0.0f)) * 255.0f + 0.5f));
}

} // namespace

// ---------------------------------------------------------------------------
// Modelled hardware laws
// ---------------------------------------------------------------------------

double YouKnow106Engine::rangeClockHz(DcoRange range) noexcept
{
    switch (range)
    {
        case DcoRange::Sixteen: return masterClockHz / 8.0;
        case DcoRange::Four:    return masterClockHz / 2.0;
        case DcoRange::Eight:
        default:                return masterClockHz / 4.0;
    }
}

std::uint32_t YouKnow106Engine::dcoDivider(double frequencyHz) noexcept
{
    if (!(frequencyHz > 0.0) || !std::isfinite(frequencyHz))
        return maximumDivider;

    const double exact = rangeClockHz(DcoRange::Eight) / frequencyHz;
    if (!(exact > 0.0) || !std::isfinite(exact))
        return maximumDivider;

    const double rounded = std::floor(exact + 0.5);
    const double limited = std::clamp(rounded,
                                      static_cast<double>(minimumDivider),
                                      static_cast<double>(maximumDivider));
    return static_cast<std::uint32_t>(limited);
}

double YouKnow106Engine::dcoQuantisedFrequency(std::uint32_t divider,
                                               DcoRange range) noexcept
{
    const std::uint32_t limited = std::clamp(divider, minimumDivider, maximumDivider);
    return rangeClockHz(range) / static_cast<double>(limited);
}

float YouKnow106Engine::vcfCutoffHz(float counts) noexcept
{
    // The digital sum upstream is already clamped to the 14-bit accumulator;
    // the margin here only covers analogue trim and drift. No dense capture
    // establishes a high-code knee, so retain the validated exponential law
    // and identify the published 50 kHz endpoint as a transparent product cap.
    const float safe = std::clamp(sanitised(counts, 0.0f), -2000.0f, 20000.0f);
    const float hz = vcfBaseFrequencyHz * std::exp2(safe / vcfCountsPerOctave);
    return std::clamp(hz, 1.0f, vcfSafetyCapHz);
}

float YouKnow106Engine::vcfPanelCounts(float panelPosition) noexcept
{
    // The panel slider is read as a 0..127 byte and the converter is driven
    // with that byte times 128, so the whole slider spans 16256 counts.
    const float byte = std::floor(clamp01(panelPosition) * 127.0f + 0.5f);
    return byte * 128.0f;
}

float YouKnow106Engine::VoicedResonanceCompatibilityProfile::loopGain(
    float panelPosition) noexcept
{
    // This piecewise curve is retained solely for YouKnow106 preset/sound
    // compatibility. No qualifying original-unit sweep establishes its
    // intermediate points, threshold position or maximum as hardware facts.
    const float position = clamp01(panelPosition);
    if (position <= nominalOscillationTravel)
        return 2.3277778f * position + 2.3518519f * position * position;
    const float past = (position - nominalOscillationTravel)
                     / (1.0f - nominalOscillationTravel);
    return nominalOscillationFeedback
         + (maximumFeedback - nominalOscillationFeedback) * past;
}

float YouKnow106Engine::VoicedResonanceCompatibilityProfile::inputCompensation(
    float feedback) noexcept
{
    // The direction and coefficient are part of the same voiced profile as
    // loopGain(). They preserve the existing high-Q drive character without
    // asserting a measured JUNO-106 compensation transfer.
    const float k = std::clamp(sanitised(feedback, 0.0f), 0.0f, 8.0f);
    return 1.0f + inputCompensationPerFeedback * k;
}

float YouKnow106Engine::VoicedResonanceCompatibilityProfile::frequencyTrim(
    float feedback) noexcept
{
    // This correction is a compatibility calibration of the model itself.
    // The service procedure motivates allowing a per-card adjustment, but it
    // does not establish this feedback-dependent coefficient or curve.
    const float k = std::clamp(sanitised(feedback, 0.0f), 0.0f, 8.0f);
    const float fraction = std::min(k / nominalOscillationFeedback, 1.2f);
    return 1.0f + frequencyTrimAmount * fraction * fraction;
}

float YouKnow106Engine::vcfEffectiveCutoffHz(float counts,
                                             float feedback,
                                             float calibration) noexcept
{
    const float rawHz = vcfCutoffHz(counts)
                      * VoicedResonanceCompatibilityProfile::frequencyTrim(feedback);
    // Physical anti-log transistor parasitic emitter resistance (R_e) compression
    // at high control currents (> 8 kHz).
    const float compressedHz = rawHz / (1.0f + calibration * (rawHz / 120000.0f));
    return std::min(vcfSafetyCapHz, compressedHz);
}

namespace
{
// Compact laws independently derived from the hash-matched B-2 regions. They
// reproduce all observable coefficients while avoiding a ROM/table dump.
std::uint16_t attackIncrementForByte(std::uint8_t byte) noexcept
{
    const int index = byte;
    if (index == 0)
        return 0x4000u;
    if (index <= 63)
    {
        int value = (8192 + index / 2) / index;
        // Three values in the otherwise rounded harmonic region are one count
        // lower in the verified image.
        if (index == 3 || index == 20 || index == 28)
            --value;
        return static_cast<std::uint16_t>(value);
    }
    if (index <= 80)
        return static_cast<std::uint16_t>(127 - 11 * (index - 64) / 4);
    if (index <= 86)
        return static_cast<std::uint16_t>(83 - (17 * (index - 80) + 1) / 6);
    if (index <= 107)
        return static_cast<std::uint16_t>(64 - 3 * (index - 87) / 2);
    if (index <= 121)
    {
        const int numerator = 429 - 6 * (index - 108);
        return static_cast<std::uint16_t>((2 * numerator + 13) / 26);
    }
    return static_cast<std::uint16_t>(26 - (index - 122));
}

std::uint16_t decayReleaseMultiplierForByte(std::uint8_t byte) noexcept
{
    std::uint32_t value = 0x1000u;
    for (int index = 1; index <= byte; ++index)
    {
        if (index <= 4)       value += 0x2000u;
        else if (index == 5)  value += 0x1000u;
        else if (index <= 15) value += 0x0800u;
        else if (index <= 43) value += 0x0080u;
        else if (index <= 65) value += 0x000cu;
        else if (index <= 123)value += 0x0004u;
        else                  value += 0x0001u;
    }
    return static_cast<std::uint16_t>(value);
}

std::uint16_t lfoRateIncrementForByte(std::uint8_t byte) noexcept
{
    int value = 5;
    for (int index = 1; index <= byte; ++index)
    {
        if (index <= 2)        value += 10;
        else if (index <= 5)   value += 15;
        else if (index <= 63)  value += 10;
        else if (index <= 95)  value += 16;
        else if (index <= 102) value += 52;
        else if (index == 103) value += 54;
        else if (index <= 105) value += 70;
        else if (index <= 109) value += 80;
        else if (index <= 119) value += 100;
        else if (index <= 122) value += 120;
        else if (index <= 126) value += 150;
        else                   value += 96;
    }
    return static_cast<std::uint16_t>(value);
}

std::uint16_t lfoDelayFadeIncrementForByte(std::uint8_t byte) noexcept
{
    switch (byte >> 4u)
    {
        case 0: return 0xffffu;
        case 1: return 1049u;
        case 2: return 524u;
        case 3: return 350u;
        default: return 256u;
    }
}

std::uint8_t portamentoIncrementForIndex(std::uint8_t index) noexcept
{
    if (index == 0u)
        return 0u;
    if (index <= 25u)
        return static_cast<std::uint8_t>(263 - 8 * index);
    if (index <= 47u)
        return static_cast<std::uint8_t>(113 - 2 * index);
    if (index == 48u)
        return 18u;
    if (index == 49u)
        return 17u;
    if (index <= 61u)
        return static_cast<std::uint8_t>(29 - (index + 2) / 4);
    return static_cast<std::uint8_t>(
        std::max(1, 26 - (static_cast<int>(index) + 3) / 5));
}

std::uint16_t truncatedDecayProduct(std::uint16_t value,
                                    std::uint16_t coefficient) noexcept
{
    const std::uint16_t valueHigh = value >> 8u;
    const std::uint16_t valueLow = value & 0xffu;
    const std::uint16_t coefficientHigh = coefficient >> 8u;
    const std::uint16_t coefficientLow = coefficient & 0xffu;
    return static_cast<std::uint16_t>(
        coefficientHigh * valueHigh
        + ((coefficientLow * valueHigh) >> 8u)
        + ((coefficientHigh * valueLow) >> 8u));
}
} // namespace

std::uint16_t YouKnow106Engine::storedControlAlignedWord(float panelPosition) noexcept
{
    return static_cast<std::uint16_t>(storedControlByte(panelPosition)) << 7u;
}

std::uint16_t YouKnow106Engine::storedControlDacCode(float panelPosition) noexcept
{
    return static_cast<std::uint16_t>(storedControlByte(panelPosition)) << 5u;
}

std::uint16_t YouKnow106Engine::envelopeAttackIncrement(
    float panelPosition) noexcept
{
    return attackIncrementForByte(storedControlByte(panelPosition));
}

std::uint16_t YouKnow106Engine::envelopeDecayReleaseMultiplier(
    float panelPosition) noexcept
{
    return decayReleaseMultiplierForByte(storedControlByte(panelPosition));
}

std::uint16_t YouKnow106Engine::lfoRateIncrement(float panelPosition) noexcept
{
    return lfoRateIncrementForByte(storedControlByte(panelPosition));
}

std::uint16_t YouKnow106Engine::lfoDelayFadeIncrement(
    float panelPosition) noexcept
{
    return lfoDelayFadeIncrementForByte(storedControlByte(panelPosition));
}

std::uint8_t YouKnow106Engine::portamentoIncrement(float panelPosition) noexcept
{
    const auto raw = portamentoAdcByte(panelPosition);
    return raw == 0u ? 0u : portamentoIncrementForIndex(raw >> 1u);
}

std::uint16_t YouKnow106Engine::envelopeAttackLevel(
    std::uint16_t level, std::uint16_t increment) noexcept
{
    const std::uint32_t next = static_cast<std::uint32_t>(level) + increment;
    return static_cast<std::uint16_t>(
        std::min<std::uint32_t>(next, envelopePeak));
}

std::uint16_t YouKnow106Engine::envelopeDecayLevel(
    std::uint16_t level, std::uint16_t sustain,
    std::uint16_t multiplier) noexcept
{
    const auto target = std::min(sustain, envelopePeak);
    const auto current = std::min(level, envelopePeak);
    if (current <= target)
        return target;
    const auto distance = static_cast<std::uint16_t>(current - target);
    return static_cast<std::uint16_t>(
        target + truncatedDecayProduct(distance, multiplier));
}

std::uint16_t YouKnow106Engine::envelopeReleaseLevel(
    std::uint16_t level, std::uint16_t multiplier) noexcept
{
    const auto current = static_cast<std::uint16_t>(std::min(level, envelopePeak));
    return truncatedDecayProduct(current, multiplier);
}

float YouKnow106Engine::envelopeDacFraction(std::uint16_t level) noexcept
{
    constexpr float inverseDacPeak = 1.0f / 4095.0f;
    const auto limited = static_cast<std::uint16_t>(
        std::min(level, envelopePeak));
    return static_cast<float>(limited >> 2u) * inverseDacPeak;
}

float YouKnow106Engine::envelopeAttackSeconds(float panelPosition) noexcept
{
    const int increment = envelopeAttackIncrement(panelPosition);
    const int passes = (envelopePeak + increment - 1) / increment;
    return static_cast<float>(passes / controlScanHz);
}

float YouKnow106Engine::envelopeDecaySeconds(float panelPosition) noexcept
{
    // Keep the conventional -20 dB display point, but reach it through the
    // exact integer helper rather than a continuous exponential estimate.
    const auto multiplier = envelopeDecayReleaseMultiplier(panelPosition);
    std::uint16_t level = envelopePeak;
    const std::uint16_t threshold = envelopePeak / 10u;
    int passes = 0;
    while (level > threshold && passes < 100000)
    {
        level = envelopeReleaseLevel(level, multiplier);
        ++passes;
    }
    return static_cast<float>(passes / controlScanHz);
}

float YouKnow106Engine::envelopeReleaseSeconds(float panelPosition) noexcept
{
    const auto multiplier = envelopeDecayReleaseMultiplier(panelPosition);
    std::uint16_t level = envelopePeak;
    int passes = 0;
    while (level != 0u && passes < 100000)
    {
        level = envelopeReleaseLevel(level, multiplier);
        ++passes;
    }
    return static_cast<float>(passes / controlScanHz);
}

float YouKnow106Engine::lfoRateHz(float panelPosition) noexcept
{
    // The rate the accumulator mechanism actually produces: whole passes per
    // half-sweep, so fast settings land on the B-2 state machine's quantised
    // grid.
    const int coefficient = lfoRateIncrement(panelPosition);
    const int passes = (8192 + coefficient - 1) / coefficient;
    return static_cast<float>(controlScanHz) / (4.0f * passes);
}

float YouKnow106Engine::lfoDelaySeconds(float panelPosition) noexcept
{
    // Total time from the note to full modulation depth: a silent hold that
    // grows across the whole travel at the attack table's own rate, plus the
    // stepped fade.
    const int holdIncrement = envelopeAttackIncrement(panelPosition);
    const int fadeIncrement = lfoDelayFadeIncrement(panelPosition);
    const int holdPasses = (16384 + holdIncrement - 1) / holdIncrement;
    const int fadePasses = (65536 + fadeIncrement - 1) / fadeIncrement;
    return static_cast<float>((holdPasses + fadePasses) / controlScanHz);
}

float YouKnow106Engine::portamentoSeconds(float panelPosition) noexcept
{
    // The performance pot is read as an eight-bit ADC value. Raw zero is the
    // explicit Off path and raw one selects the observed zero/immediate entry;
    // the active raw pairs select indices 1..127 through raw>>1.
    const int stepUnits = portamentoIncrement(panelPosition);
    if (stepUnits == 0)
        return 0.0f;
    const int passes = (3072 + stepUnits - 1) / stepUnits;
    return static_cast<float>(passes / controlScanHz);
}

float YouKnow106Engine::outputReferenceGain(float referenceRmsVolts) noexcept
{
    if (!(referenceRmsVolts > 0.0f) || !std::isfinite(referenceRmsVolts))
        return 1.0f;
    return internalVoltsPerUnit * minus18DbfsAmplitude / referenceRmsVolts;
}

const std::array<YouKnow106Engine::ConverterWrite,
                 YouKnow106Engine::converterWritesPerPass>&
YouKnow106Engine::converterWriteOrder() noexcept
{
    static constexpr std::array<ConverterWrite, converterWritesPerPass> order {{
        { ConverterDestination::Resonance, -1 },
        { ConverterDestination::CommonVca, -1 },
        { ConverterDestination::Sub, -1 },
        { ConverterDestination::Pitch, 0 },
        { ConverterDestination::Pitch, 1 },
        { ConverterDestination::Pitch, 2 },
        { ConverterDestination::Pitch, 3 },
        { ConverterDestination::Pitch, 4 },
        { ConverterDestination::Pitch, 5 },
        { ConverterDestination::Pwm, -1 },
        { ConverterDestination::Vcf, 0 },
        { ConverterDestination::VoiceVca, 0 },
        { ConverterDestination::Vcf, 1 },
        { ConverterDestination::VoiceVca, 1 },
        { ConverterDestination::Vcf, 2 },
        { ConverterDestination::VoiceVca, 2 },
        { ConverterDestination::Vcf, 3 },
        { ConverterDestination::VoiceVca, 3 },
        { ConverterDestination::Vcf, 4 },
        { ConverterDestination::VoiceVca, 4 },
        { ConverterDestination::Vcf, 5 },
        { ConverterDestination::VoiceVca, 5 },
        { ConverterDestination::Noise, -1 }
    }};
    return order;
}

std::array<double, YouKnow106Engine::converterWritesPerPass>
YouKnow106Engine::converterEventPhases(ConverterTimingProfile profile) noexcept
{
    std::array<double, converterWritesPerPass> phases {};
    if (profile == ConverterTimingProfile::PhaseZeroDiagnostic)
        return phases;

    // The service chart establishes sequential activity spread across the
    // pass, but its drawing is not a calibrated timing capture. A normalized
    // ordinal grid is therefore an explicit compatibility profile: enough to
    // preserve non-simultaneous DCO programming without promoting made-up
    // microseconds to JUNO-106 facts.
    for (std::size_t ordinal = 0; ordinal < phases.size(); ++ordinal)
        phases[ordinal] = static_cast<double>(ordinal)
                        / static_cast<double>(phases.size());
    return phases;
}

float YouKnow106Engine::panelPositionForAttack(float seconds) noexcept
{
    if (!(seconds > 0.0f) || !std::isfinite(seconds))
        return 0.0f;
    if (seconds <= envelopeAttackSeconds(0.0f))
        return 0.0f;
    if (seconds >= envelopeAttackSeconds(1.0f))
        return 1.0f;

    int bestByte = 0;
    float bestError = std::numeric_limits<float>::infinity();
    for (int byte = 0; byte <= 127; ++byte)
    {
        const float realised = envelopeAttackSeconds(
            static_cast<float>(byte) / 127.0f);
        const float error = std::abs(std::log(realised / seconds));
        if (error < bestError)
        {
            bestError = error;
            bestByte = byte;
        }
    }
    return static_cast<float>(bestByte) / 127.0f;
}

float YouKnow106Engine::panelPositionForDecay(float seconds) noexcept
{
    if (!(seconds > 0.0f) || !std::isfinite(seconds))
        return 0.0f;
    if (seconds <= envelopeDecaySeconds(0.0f))
        return 0.0f;
    if (seconds >= envelopeDecaySeconds(1.0f))
        return 1.0f;

    int bestByte = 0;
    float bestError = std::numeric_limits<float>::infinity();
    for (int byte = 0; byte <= 127; ++byte)
    {
        const float realised = envelopeDecaySeconds(
            static_cast<float>(byte) / 127.0f);
        const float error = std::abs(std::log(realised / seconds));
        if (error < bestError)
        {
            bestError = error;
            bestByte = byte;
        }
    }
    return static_cast<float>(bestByte) / 127.0f;
}

float YouKnow106Engine::panelPositionForRelease(float seconds) noexcept
{
    if (!(seconds > 0.0f) || !std::isfinite(seconds))
        return 0.0f;
    if (seconds <= envelopeReleaseSeconds(0.0f))
        return 0.0f;
    if (seconds >= envelopeReleaseSeconds(1.0f))
        return 1.0f;

    int bestByte = 0;
    float bestError = std::numeric_limits<float>::infinity();
    for (int byte = 0; byte <= 127; ++byte)
    {
        const float realised = envelopeReleaseSeconds(
            static_cast<float>(byte) / 127.0f);
        const float error = std::abs(std::log(realised / seconds));
        if (error < bestError)
        {
            bestError = error;
            bestByte = byte;
        }
    }
    return static_cast<float>(bestByte) / 127.0f;
}

float YouKnow106Engine::panelPositionForLfoRate(float hertz) noexcept
{
    if (!(hertz > 0.0f) || !std::isfinite(hertz))
        return 0.0f;
    if (hertz <= lfoRateHz(0.0f))
        return 0.0f;
    if (hertz >= lfoRateHz(1.0f))
        return 1.0f;

    int bestByte = 0;
    float bestError = std::numeric_limits<float>::infinity();
    for (int byte = 0; byte <= 127; ++byte)
    {
        const float realised = lfoRateHz(static_cast<float>(byte) / 127.0f);
        const float error = std::abs(std::log(realised / hertz));
        if (error < bestError)
        {
            bestError = error;
            bestByte = byte;
        }
    }
    return static_cast<float>(bestByte) / 127.0f;
}

float YouKnow106Engine::panelPositionForLfoDelay(float seconds) noexcept
{
    if (!(seconds > 0.0f) || !std::isfinite(seconds))
        return 0.0f;
    if (seconds >= lfoDelaySeconds(1.0f))
        return 1.0f;

    int bestByte = 0;
    float bestError = std::numeric_limits<float>::infinity();
    for (int byte = 0; byte <= 127; ++byte)
    {
        const float realised = lfoDelaySeconds(static_cast<float>(byte) / 127.0f);
        const float error = std::abs(realised - seconds);
        if (error < bestError)
        {
            bestError = error;
            bestByte = byte;
        }
    }
    return static_cast<float>(bestByte) / 127.0f;
}

float YouKnow106Engine::panelPositionForPortamento(float secondsPerOctave) noexcept
{
    if (!(secondsPerOctave > 0.0f) || !std::isfinite(secondsPerOctave))
        return 0.0f;
    if (secondsPerOctave >= portamentoSeconds(1.0f))
        return 1.0f;
    if (secondsPerOctave <= portamentoSeconds(2.0f / 255.0f))
        return 2.0f / 255.0f;

    // The realised law has repeated values: raw pairs address one coefficient.
    // Return the first canonical
    // ADC code producing the closest displayed seconds-per-octave value.
    int bestRaw = 2;
    float bestError = std::numeric_limits<float>::infinity();
    for (int raw = 2; raw <= 255; ++raw)
    {
        const float realised = portamentoSeconds(static_cast<float>(raw) / 255.0f);
        const float error = std::abs(std::log(realised / secondsPerOctave));
        if (error < bestError)
        {
            bestError = error;
            bestRaw = raw;
        }
    }
    return static_cast<float>(bestRaw) / 255.0f;
}

float YouKnow106Engine::panelPositionForCutoff(float hertz) noexcept
{
    if (!(hertz > 0.0f) || !std::isfinite(hertz))
        return 0.0f;
    const float counts = vcfCountsPerOctave
                       * std::log2(std::max(hertz, vcfBaseFrequencyHz) / vcfBaseFrequencyHz);
    // The travel is read as a 0..127 byte driving the converter 128 counts at
    // a time, so this is the inverse of that quantisation, not of a continuum.
    return std::clamp(counts / (127.0f * 128.0f), 0.0f, 1.0f);
}

float YouKnow106Engine::pwmControlVolts(float depth) noexcept
{
    // The comparator threshold runs from +6 V, where the ramp is bisected and
    // the pulse is square, down to +0.6 V, where it is 95% wide. It cannot be
    // driven to either rail, so 0% and 100% are unreachable.
    return 6.0f - 5.4f * clamp01(depth);
}

// Where the comparator's two edges sit within one cycle, as fractions of the
// period. The threshold is the ramp voltage the rise crosses on the way up, and
// the comparator holds until the descending reset passes back through the same
// voltage -- not until the reset begins, where the ramp is still at its
// positive rail. At a high note the reset is a noticeable part of the period,
// so dropping at its start would shorten the high interval by about
// duty * reset: a 95% pulse asked for at the top of the 4' range would come out
// nearer 90%.
float YouKnow106Engine::rampSegmentVoltage(float risePosition) noexcept
{
    return 2.0f * clamp01(risePosition) - 1.0f;
}

float YouKnow106Engine::pulseRisePhase(float duty, float resetFraction) noexcept
{
    const float reset = std::clamp(resetFraction, 0.0f, 0.25f);
    const float rise = std::max(1.0f - reset, 1.0e-4f);
    return rise * (1.0f - std::clamp(duty, 0.0f, 1.0f));
}

float YouKnow106Engine::pulseFallPhase(float duty, float resetFraction) noexcept
{
    const float reset = std::clamp(resetFraction, 0.0f, 0.25f);
    const float rise = std::max(1.0f - reset, 1.0e-4f);
    // The ramp runs 0..1 over the rise and falls linearly back over the reset,
    // both mapped to -1..+1. Solving the falling segment for the rise's
    // threshold gives the fraction of the reset spent still above it.
    const float threshold = 1.0f - std::clamp(duty, 0.0f, 1.0f);
    return rise + reset * std::clamp(1.0f - threshold, 0.0f, 1.0f);
}

float YouKnow106Engine::pwmDutyCycle(float controlVolts) noexcept
{
    return pwmDutyCycle(controlVolts, 1.0f);
}

float YouKnow106Engine::pwmDutyCycle(float controlVolts,
                                     float rampAmplitudeScale) noexcept
{
    // Pulse Off writes -0.8 V. That sits below the ramp and leaves the
    // comparator permanently high while the oscillator itself keeps running.
    if (std::isfinite(controlVolts) && controlVolts < 0.0f)
        return 1.0f;

    // The nominal ramp spans 12 V peak to peak. Its compensation hold lags a
    // pitch step, and the optional card-current error changes the same ramp's
    // slope; both therefore move the comparator crossing as well as the saw.
    const float volts = std::clamp(sanitised(controlVolts, 6.0f), 0.6f, 6.0f);
    const float scale = std::clamp(
        sanitised(rampAmplitudeScale, 1.0f), 0.25f, 4.0f);
    return std::clamp(1.0f - volts / (12.0f * scale), 0.0f, 1.0f);
}

float YouKnow106Engine::VoicedVoiceVcaCompatibilityProfile::gain(
    float control) noexcept
{
    // Preserve YouKnow106's established quasi-linear response, exponential
    // low-control knee and hard-zero policy. OQ-19 keeps all three numerical
    // choices voiced; a measurement floor alone would not prove this deadband.
    const float level = clamp01(sanitised(control, 0.0f));
    if (level <= deadband)
        return 0.0f;
    const float kneeDb = kneeDbPerControlUnit * std::max(0.0f, knee - level);
    return level * std::pow(10.0f, -kneeDb / 20.0f);
}

float YouKnow106Engine::patchLevelGain(float dacFraction) noexcept
{
    // NEC specifies the common jack-board uPC1252H2's GC1 device transfer as
    // -5.9 mV/dB typical. What remains unknown is Roland's converter/hold and
    // jack-board mapping from p=b/127=DAC12/4064 to GC1 voltage and offset. The
    // compatibility display coordinate is explicitly x=-5+10p; its three
    // x=-5/0/+5 anchors motivate -15/-12.5/+5 dB respectively. The cubic below
    // is the declared provisional p-to-dB mapping, algebraically equivalent to
    // a voiced GC1 curve under the NEC slope, not a measured byte-to-voltage
    // law.
    //
    // This is intentionally identified as a three-point fit, not an exact
    // potentiometer model: a full panel-byte/control-voltage sweep from a
    // calibrated unit would be needed to determine the intermediate curve.
    const float position = clamp01(dacFraction);
    const float decibels = -15.0f + 20.0f * position * position * position;
    return std::pow(10.0f, decibels / 20.0f);
}

float YouKnow106Engine::highPassCornerHz(HighPassMode mode) noexcept
{
    // Four legs of a switched network, selected by a CMOS multiplexer: a
    // shelving boost, a straight-through leg, and two progressively higher
    // corners. Each cut leg is its own series capacitor -- C10 15 nF, C11
    // 4.7 nF -- against the same 47 kOhm feed into the summing amplifier's
    // virtual earth, with a 1 MOhm bleed to ground. The 47 kOhm is the timing
    // resistance; the 1 MOhm is far too high to be.
    //
    // Two earlier revisions got this wrong in different ways, and the second
    // was wrong in a way that concealed itself: 15 kOhm against 47 nF has the
    // same product as 47 kOhm against 15 nF, so position 2 came out right by
    // coincidence while position 3 stayed 13 Hz off. Reading the schematic's
    // own designators is what separated them. The boost's corner is unchanged,
    // being the measured shelf's own pole rather than a part value -- and the
    // boost is a summed two-zero/two-pole shelf, not one RC, which is why it
    // has never been described by a corner alone here.
    switch (mode)
    {
        case HighPassMode::Boost: return 59.4f;
        case HighPassMode::Two:   return 225.8f;   // 47 kOhm x 15 nF
        case HighPassMode::Three: return 720.5f;   // 47 kOhm x 4.7 nF
        case HighPassMode::One:
        default:                  return 59.4f;
    }
}

float YouKnow106Engine::highPassShelfGain(HighPassMode mode) noexcept
{
    // How much of the low band the leg returns. The boost position is a real
    // measured shelf: +10.5 dB at DC, verified against a hardware noise
    // sweep -- far more than the +3 dB an earlier account reported. The
    // straight-through leg returns the low band untouched, and the two
    // cutting legs discard it.
    switch (mode)
    {
        case HighPassMode::Boost: return std::pow(10.0f, 10.5f / 20.0f);
        case HighPassMode::One:   return 1.0f;
        case HighPassMode::Two:
        case HighPassMode::Three:
        default:                  return 0.0f;
    }
}

float YouKnow106Engine::highPassHighGain(HighPassMode mode) noexcept
{
    // The boost leg lifts the high band a little too -- the measured shelf
    // settles at +1.41 dB well above its corner. Every other leg passes the
    // high band at unity.
    switch (mode)
    {
        case HighPassMode::Boost: return std::pow(10.0f, 1.41f / 20.0f);
        case HighPassMode::One:
        case HighPassMode::Two:
        case HighPassMode::Three:
        default:                  return 1.0f;
    }
}

float YouKnow106Engine::outputCouplingCornerHz() noexcept
{
    return 1.0f / (twoPi * outputCouplingCapacitanceF
                   * (outputCouplingSeriesOhms + outputCouplingPotOhms));
}

float YouKnow106Engine::voiceBusCouplingCornerHz() noexcept
{
    return 1.0f / (twoPi * voiceBusCouplingCapacitanceF
                   * voiceBusCouplingResistanceOhms);
}

float YouKnow106Engine::voiceBusCouplingCornerHz(HighPassMode mode) noexcept
{
    if (mode == HighPassMode::Boost || mode == HighPassMode::One)
    {
        constexpr float selectedInputOhms = 47000.0f;
        constexpr float parallelOhms =
            voiceBusCouplingResistanceOhms * selectedInputOhms
            / (voiceBusCouplingResistanceOhms + selectedInputOhms);
        return 1.0f / (twoPi * voiceBusCouplingCapacitanceF * parallelOhms);
    }
    return voiceBusCouplingCornerHz();
}

float YouKnow106Engine::commonVcaInputCouplingCornerHz() noexcept
{
    return 1.0f / (twoPi * commonVcaInputCapacitanceF
                   * commonVcaInputResistanceOhms);
}

float YouKnow106Engine::outputCouplingHighGain() noexcept
{
    return outputCouplingPotOhms
         / (outputCouplingSeriesOhms + outputCouplingPotOhms);
}

float YouKnow106Engine::outputCouplingCornerHz(float volumePosition) noexcept
{
    const float position = clamp01(sanitised(volumePosition, 0.0f));
    const float lowerTrack = position * outputCouplingPotOhms;
    const float loadedLower = lowerTrack > 0.0f
        ? lowerTrack * outputWiperInternalLoadOhms
            / (lowerTrack + outputWiperInternalLoadOhms)
        : 0.0f;
    const float upperTrack = (1.0f - position) * outputCouplingPotOhms;
    const float resistance = outputCouplingSeriesOhms
                           + upperTrack + loadedLower;
    return 1.0f / (twoPi * outputCouplingCapacitanceF * resistance);
}

float YouKnow106Engine::outputCouplingHighGain(float volumePosition) noexcept
{
    const float position = clamp01(sanitised(volumePosition, 0.0f));
    const float lowerTrack = position * outputCouplingPotOhms;
    if (!(lowerTrack > 0.0f))
        return 0.0f;
    const float loadedLower = lowerTrack * outputWiperInternalLoadOhms
                            / (lowerTrack + outputWiperInternalLoadOhms);
    const float upperTrack = (1.0f - position) * outputCouplingPotOhms;
    return loadedLower / (outputCouplingSeriesOhms
                          + upperTrack + loadedLower);
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

double YouKnow106Engine::midiToHz(double midiNote) noexcept
{
    return 440.0 * std::pow(2.0, (midiNote - 69.0) / 12.0);
}

std::uint32_t YouKnow106Engine::hash32(std::uint32_t value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float YouKnow106Engine::hashBipolar(std::uint32_t value) noexcept
{
    return static_cast<float>(hash32(value) & 0xffffffu) * (2.0f / 16777215.0f) - 1.0f;
}

float YouKnow106Engine::resetFraction(double periodSeconds) noexcept
{
    if (!(periodSeconds > 0.0))
        return 0.25f;
    const double fraction = static_cast<double>(rampResetSeconds) / periodSeconds;
    return static_cast<float>(std::clamp(fraction, 1.0e-6, 0.25));
}

// ---------------------------------------------------------------------------
// Bandlimiting
// ---------------------------------------------------------------------------

void YouKnow106Engine::BandlimitedTrack::reset() noexcept
{
    ring.fill(0.0f);
    delay.fill(0.0f);
    base = 0;
    primed = false;
}

void YouKnow106Engine::BandlimitedTrack::prime(float value) noexcept
{
    if (primed)
        return;
    delay.fill(value);
    primed = true;
}

float YouKnow106Engine::BandlimitedTrack::advance(float naive) noexcept
{
    prime(naive);

    const float output = delay[0] + ring[static_cast<std::size_t>(base)];
    ring[static_cast<std::size_t>(base)] = 0.0f;
    base = base + 1 < correctionRing ? base + 1 : 0;

    for (int i = 0; i + 1 < correctionHalfWidth; ++i)
        delay[static_cast<std::size_t>(i)] = delay[static_cast<std::size_t>(i + 1)];
    delay[static_cast<std::size_t>(correctionHalfWidth - 1)] = naive;

    return output;
}

void YouKnow106Engine::buildCorrectionTables() noexcept
{
    // Integrate a Blackman-windowed sinc to obtain the bandlimited step, then
    // subtract the ideal step to leave the residual. Integrating once more
    // gives the slope residual. Doing this numerically rather than fitting a
    // polynomial means the residual is right by construction; the earlier
    // closed-form attempt was not, and it raised the alias floor instead of
    // lowering it.
    constexpr int length = correctionTableLength;
    constexpr double step = 1.0 / correctionOversample;

    std::array<double, length> impulse {};
    for (int i = 0; i < length; ++i)
    {
        const double t = static_cast<double>(i) * step - correctionHalfWidth;
        const double sinc = std::abs(t) < 1.0e-12
            ? 1.0
            : std::sin(3.14159265358979323846 * t) / (3.14159265358979323846 * t);
        const double phase = static_cast<double>(i) / static_cast<double>(length - 1);
        const double window = 0.42
                            - 0.5 * std::cos(2.0 * 3.14159265358979323846 * phase)
                            + 0.08 * std::cos(4.0 * 3.14159265358979323846 * phase);
        impulse[static_cast<std::size_t>(i)] = sinc * window;
    }

    // Trapezoidal running integral, normalised so the step ends at exactly one.
    std::array<double, length> stepResponse {};
    double accumulator = 0.0;
    stepResponse[0] = 0.0;
    for (int i = 1; i < length; ++i)
    {
        accumulator += 0.5 * step
                     * (impulse[static_cast<std::size_t>(i - 1)]
                        + impulse[static_cast<std::size_t>(i)]);
        stepResponse[static_cast<std::size_t>(i)] = accumulator;
    }
    const double total = stepResponse[length - 1];
    if (std::abs(total) > 1.0e-12)
        for (auto& value : stepResponse)
            value /= total;

    std::array<double, length> rampResponse {};
    accumulator = 0.0;
    rampResponse[0] = 0.0;
    for (int i = 1; i < length; ++i)
    {
        accumulator += 0.5 * step
                     * (stepResponse[static_cast<std::size_t>(i - 1)]
                        + stepResponse[static_cast<std::size_t>(i)]);
        rampResponse[static_cast<std::size_t>(i)] = accumulator;
    }

    for (int i = 0; i < length; ++i)
    {
        const double t = static_cast<double>(i) * step - correctionHalfWidth;
        const double idealStep = t >= 0.0 ? 1.0 : 0.0;
        const double idealRamp = t >= 0.0 ? t : 0.0;
        stepResidual_[static_cast<std::size_t>(i)] =
            static_cast<float>(stepResponse[static_cast<std::size_t>(i)] - idealStep);
        slopeResidual_[static_cast<std::size_t>(i)] =
            static_cast<float>(rampResponse[static_cast<std::size_t>(i)] - idealRamp);
    }
}

// `samplesAgo` is how far back inside the sample just rendered the event sits,
// in [0, 1). Output sample `j` of the correction ring is `j - halfWidth`
// samples away from the sample just rendered, so the residual is read at
// `j - halfWidth + samplesAgo` and the table is offset by the half width.
void YouKnow106Engine::addStep(BandlimitedTrack& track, float height,
                               float samplesAgo) const noexcept
{
    if (!(std::abs(height) > 0.0f))
        return;
    const float offset = std::clamp(samplesAgo, 0.0f, 1.0f);
    for (int j = 0; j < correctionRing; ++j)
    {
        const float position = (static_cast<float>(j) + offset)
                             * static_cast<float>(correctionOversample);
        const int index = std::clamp(static_cast<int>(position + 0.5f), 0,
                                     correctionTableLength - 1);
        const int slot = (track.base + j) % correctionRing;
        track.ring[static_cast<std::size_t>(slot)] +=
            height * stepResidual_[static_cast<std::size_t>(index)];
    }
}

void YouKnow106Engine::addSlope(BandlimitedTrack& track, float slopeStep,
                                float samplesAgo) const noexcept
{
    if (!(std::abs(slopeStep) > 0.0f))
        return;
    const float offset = std::clamp(samplesAgo, 0.0f, 1.0f);
    for (int j = 0; j < correctionRing; ++j)
    {
        const float position = (static_cast<float>(j) + offset)
                             * static_cast<float>(correctionOversample);
        const int index = std::clamp(static_cast<int>(position + 0.5f), 0,
                                     correctionTableLength - 1);
        const int slot = (track.base + j) % correctionRing;
        track.ring[static_cast<std::size_t>(slot)] +=
            slopeStep * slopeResidual_[static_cast<std::size_t>(index)];
    }
}

// ---------------------------------------------------------------------------
// Envelope
// ---------------------------------------------------------------------------

void YouKnow106Engine::Envelope::reset() noexcept
{
    stage = EnvelopeStage::Idle;
    level = 0u;
    value = 0.0f;
}

void YouKnow106Engine::Envelope::noteOn() noexcept
{
    stage = EnvelopeStage::Attack;
}

void YouKnow106Engine::Envelope::noteOff() noexcept
{
    if (stage != EnvelopeStage::Idle)
        stage = EnvelopeStage::Release;
}

float YouKnow106Engine::Envelope::tick(std::uint16_t attackIncrement,
                                       std::uint16_t decayMultiplier,
                                       std::uint16_t sustain,
                                       std::uint16_t releaseMultiplier) noexcept
{
    switch (stage)
    {
        case EnvelopeStage::Attack:
            level = envelopeAttackLevel(level, attackIncrement);
            if (level >= envelopePeak)
            {
                level = envelopePeak;
                stage = EnvelopeStage::Decay;
            }
            break;

        case EnvelopeStage::Decay:
        case EnvelopeStage::Sustain:
            // One state, as in the firmware: above the sustain level the
            // distance decays multiplicatively; at or below it the level
            // snaps to the target, which is also what happens when the
            // slider is pushed *up* mid-note.
            if (level > sustain)
            {
                level = envelopeDecayLevel(level, sustain, decayMultiplier);
                if (level <= sustain)
                {
                    level = sustain;
                    stage = EnvelopeStage::Sustain;
                }
            }
            else
            {
                level = sustain;
                stage = EnvelopeStage::Sustain;
            }
            break;

        case EnvelopeStage::Release:
            level = envelopeReleaseLevel(level, releaseMultiplier);
            if (level == 0u)
            {
                stage = EnvelopeStage::Idle;
            }
            break;

        case EnvelopeStage::Idle:
        default:
            level = 0u;
            break;
    }

    // The two low recurrence bits stay in RAM and influence the next pass,
    // but they do not reach the 12-bit converter on this pass.
    value = envelopeDacFraction(level);
    return value;
}

// ---------------------------------------------------------------------------
// Oscillator, filter and high-pass state
// ---------------------------------------------------------------------------

void YouKnow106Engine::Dco::reset() noexcept
{
    phase = 0.0;
    pulseState = -1.0f;
    subState = 1.0f;
    saw.reset();
    pulse.reset();
    sub.reset();
}

void YouKnow106Engine::restartDcoBandlimited(
    Voice& voice, double previousPeriodSamples) noexcept
{
    auto& dco = voice.dco;
    if (!dco.saw.primed || !dco.pulse.primed || !dco.sub.primed)
    {
        // Nothing from this cell has reached the delayed output timeline yet,
        // so this is a true cold start rather than an audible discontinuity.
        dco.reset();
        voice.pulseDutyPrimed = false;
        return;
    }

    const auto rampGeometry = [this](double periodSamples) {
        const double safePeriod = std::max(periodSamples, 1.0e-9);
        const double reset = static_cast<double>(
            resetFraction(safePeriod * inverseOversampledRate_));
        const double rise = std::max(1.0 - reset, 1.0e-4);
        return std::array { safePeriod, reset, rise };
    };
    const auto oldGeometry = rampGeometry(previousPeriodSamples);
    const auto newGeometry = rampGeometry(dco.periodSamples);
    const double oldReset = oldGeometry[1];
    const double oldRise = oldGeometry[2];
    const double oldPhase = dco.phase;
    const float oldSaw = oldPhase < oldRise
        ? 2.0f * clamp01(static_cast<float>(oldPhase / oldRise)) - 1.0f
        : 1.0f - 2.0f * static_cast<float>(
              (oldPhase - oldRise) / oldReset);
    constexpr float newSaw = -1.0f;

    const float oldSlope = oldPhase < oldRise
        ? 2.0f / static_cast<float>(oldRise * oldGeometry[0])
        : -2.0f / static_cast<float>(oldReset * oldGeometry[0]);
    const float newSlope =
        2.0f / static_cast<float>(newGeometry[2] * newGeometry[0]);

    // The write happens before the current naive sample enters the track. In
    // this delayed-input residual convention that is offset zero: the track's
    // own four-sample delay supplies the non-causal half of the symmetric
    // correction. Offset one would advance the residual without advancing the
    // delayed hard step and create a larger, double-sided discontinuity.
    constexpr float currentNaiveTimestamp = 0.0f;
    // renderVoice advances the normalized phase before it submits the next
    // naive sample. Express the value discontinuity on that same event-side
    // sample grid: both the abandoned and restarted ramps have advanced once
    // by their respective slopes. Using only newSaw-oldSaw would leave the
    // slope difference behind as a small hard step on wide/high retargets.
    addStep(dco.saw, (newSaw + newSlope) - (oldSaw + oldSlope),
            currentNaiveTimestamp);
    addSlope(dco.saw, newSlope - oldSlope, currentNaiveTimestamp);
    addStep(dco.pulse, -1.0f - dco.pulseState, currentNaiveTimestamp);
    addStep(dco.sub, 1.0f - dco.subState, currentNaiveTimestamp);

    dco.phase = 0.0;
    dco.pulseState = -1.0f;
    dco.subState = 1.0f;
    // The restart correction above owns the abandoned comparator timeline.
    // The first sample on the new ramp starts a fresh moving-threshold solve
    // rather than interpolating from a duty that belonged to the old phase.
    voice.pulseDutyPrimed = false;
}

void YouKnow106Engine::OtaCascade::reset() noexcept
{
    state.fill(0.0f);
    voltage.fill(0.0f);
}

void YouKnow106Engine::OtaCascade::retime(float previousG,
                                          float nextG) noexcept
{
    if (!(previousG > 0.0f) || !std::isfinite(previousG)
        || !(nextG >= 0.0f) || !std::isfinite(nextG))
    {
        // With no usable derivative history, retaining charge is still less
        // destructive than clearing a continuously powered voice card.
        state = voltage;
        return;
    }

    // After a trapezoidal step, state - voltage is g times the derivative at
    // that physical capacitor. A live HQ change alters dt (and therefore g),
    // not the capacitor voltage or transconductance. Preserve the derivative
    // by scaling only that carry into the new timestep.
    const float ratio = nextG / previousG;
    for (std::size_t stage = 0; stage < state.size(); ++stage)
        state[stage] = voltage[stage]
                     + (state[stage] - voltage[stage]) * ratio;
}

// One trapezoidally integrated step of the four transconductor stages with the
// inverting resonance return closed around them. The unknowns are the four
// stage voltages; the Jacobian is lower bidiagonal apart from a single corner
// term contributed by the feedback, so the Newton step is solved directly
// rather than with a general linear solver.
//
// Stage equation: Vn = s_n + g * H * tanh((V_{n-1} - V_n) / H), with
// H = 2 Vt / attenuation, the differential pair's linear span referred to the
// stage input, and V_0 = input - k * fb(V_4). The compatibility profile uses
// a circuit-shaped nonlinear return, fb(V) = Hfb * tanh(V / Hfb), so its loop
// remains bounded. The selected divider and headroom are part of that voiced
// profile, not a measured code-to-loop transfer.
float YouKnow106Engine::OtaCascade::process(float input, float g,
                                            float feedback,
                                            float headroom,
                                            bool enableEarlyEffect) noexcept
{
    const float inverseHeadroom = 1.0f / std::max(headroom, 1.0e-5f);
    constexpr float feedbackHeadroom =
        VoicedResonanceCompatibilityProfile::loopHeadroomVolts;
    constexpr int maximumIterations = 8;

    const float gLimited = std::clamp(g, 0.0f, 64.0f);
    const float k = std::clamp(feedback, 0.0f, 8.0f);

    std::array<float, 4> selfDerivative {};
    std::array<float, 4> previousDerivative {};
    std::array<float, 4> residual {};

    for (int iteration = 0; iteration < maximumIterations; ++iteration)
    {
        const float feedbackTanh = std::tanh(voltage[3] / feedbackHeadroom);
        const float feedbackSech2 = 1.0f - feedbackTanh * feedbackTanh;
        float previous = input - k * feedbackHeadroom * feedbackTanh;
        for (int n = 0; n < 4; ++n)
        {
            const float earlyMod = enableEarlyEffect ? (1.0f + 0.005f * (voltage[static_cast<std::size_t>(n)] * inverseHeadroom)) : 1.0f;
            const float stageG = gLimited * earlyMod;
            const float x = (previous - voltage[static_cast<std::size_t>(n)]
                             + offsetVoltage[static_cast<std::size_t>(n)]) * inverseHeadroom;
            const float t = std::tanh(x);
            const float sech2 = 1.0f - t * t;
            residual[static_cast<std::size_t>(n)] =
                voltage[static_cast<std::size_t>(n)]
                - state[static_cast<std::size_t>(n)] - stageG * headroom * t;
            selfDerivative[static_cast<std::size_t>(n)] = 1.0f + stageG * sech2;
            previousDerivative[static_cast<std::size_t>(n)] = -stageG * sech2;
            previous = voltage[static_cast<std::size_t>(n)];
        }

        // Solve the bidiagonal system twice: once for the residual and once for
        // the corner column, then combine. This is the rank-one correction that
        // closes the resonance loop without forming a 4x4 matrix.
        const auto solveBidiagonal = [&](const std::array<float, 4>& rhs) {
            std::array<float, 4> x {};
            x[0] = rhs[0] / selfDerivative[0];
            for (int n = 1; n < 4; ++n)
                x[static_cast<std::size_t>(n)] =
                    (rhs[static_cast<std::size_t>(n)]
                     - previousDerivative[static_cast<std::size_t>(n)]
                       * x[static_cast<std::size_t>(n - 1)])
                    / selfDerivative[static_cast<std::size_t>(n)];
            return x;
        };

        const auto a = solveBidiagonal(residual);
        std::array<float, 4> corner {};
        // The corner column is the loop's derivative with respect to the
        // fourth pole, which carries the return pair's own compression.
        corner[0] = previousDerivative[0] * (-k * feedbackSech2);
        const auto b = solveBidiagonal(corner);

        float denominator = 1.0f + b[3];
        if (std::abs(denominator) < 1.0e-9f)
            denominator = denominator < 0.0f ? -1.0e-9f : 1.0e-9f;
        const float scale = a[3] / denominator;

        float largest = 0.0f;
        for (int n = 0; n < 4; ++n)
        {
            const float delta = std::clamp(
                a[static_cast<std::size_t>(n)] - b[static_cast<std::size_t>(n)] * scale,
                -32.0f, 32.0f);
            voltage[static_cast<std::size_t>(n)] -= delta;
            largest = std::max(largest, std::abs(delta));
        }

        if (largest < 1.0e-7f)
            break;
    }

    for (int n = 0; n < 4; ++n)
    {
        // Trapezoidal carry: s_next = 2 V - s.
        state[static_cast<std::size_t>(n)] =
            2.0f * voltage[static_cast<std::size_t>(n)]
            - state[static_cast<std::size_t>(n)];
        if (!std::isfinite(state[static_cast<std::size_t>(n)]))
        {
            state[static_cast<std::size_t>(n)] = 0.0f;
            voltage[static_cast<std::size_t>(n)] = 0.0f;
        }
    }

    return voltage[3];
}

void YouKnow106Engine::HighPass::reset() noexcept
{
    state = 0.0;
}

float YouKnow106Engine::HighPass::process(float input, float g,
                                          float shelfGain,
                                          float highGain) noexcept
{
    // Topology-preserving single pole. The cutting legs pass the high band at
    // unity and discard the low band; the boost leg is a real shelf, lifting
    // the low band strongly and the high band slightly, as the measured
    // network does.
    const double v = (static_cast<double>(input) - state)
                   * static_cast<double>(g)
                   / (1.0 + static_cast<double>(g));
    const double low = v + state;
    state = low + v;
    if (!std::isfinite(state))
        state = 0.0;
    const double high = static_cast<double>(input) - low;
    return static_cast<float>(static_cast<double>(highGain) * high
                            + static_cast<double>(shelfGain) * low);
}

void YouKnow106Engine::HalfbandDecimator::reset() noexcept
{
    left.fill(0.0f);
    right.fill(0.0f);
    writeIndex = 0;
}

// ---------------------------------------------------------------------------
// Construction and preparation
// ---------------------------------------------------------------------------

YouKnow106Engine::YouKnow106Engine() noexcept
{
    buildHalfbandKernel();
    buildCorrectionTables();
    buildVoiceCards();
    clearHeldNotes();
}

void YouKnow106Engine::buildHalfbandKernel() noexcept
{
    // Blackman-Harris windowed half-band. The window is chosen over a Kaiser
    // because the C++ standard special-function Bessel is not available on
    // every toolchain this project builds with, and its stopband is already
    // well below the noise floor of everything upstream of it.
    constexpr int centre = (halfbandTaps - 1) / 2;
    float sum = 0.0f;
    for (int n = 0; n < halfbandTaps; ++n)
    {
        const float offset = static_cast<float>(n - centre);
        float ideal;
        if (std::abs(offset) < 1.0e-6f)
        {
            ideal = 0.5f;
        }
        else if (((n - centre) & 1) == 0)
        {
            // Every other non-centre half-band tap is analytically zero.
            // Spelling that out avoids platform-dependent sin(k*pi) crumbs
            // and lets downsamplePair skip those multiplies exactly.
            ideal = 0.0f;
        }
        else
        {
            const float x = pi * offset * 0.5f;
            ideal = 0.5f * std::sin(x) / x;
        }

        const float t = static_cast<float>(n) / static_cast<float>(halfbandTaps - 1);
        const float window = 0.35875f
                           - 0.48829f * std::cos(twoPi * t)
                           + 0.14128f * std::cos(2.0f * twoPi * t)
                           - 0.01168f * std::cos(3.0f * twoPi * t);
        halfbandKernel_[static_cast<std::size_t>(n)] = ideal * window;
        sum += halfbandKernel_[static_cast<std::size_t>(n)];
    }

    // Normalise to exactly unity gain at DC so decimation cannot shift level.
    if (sum > 1.0e-9f)
        for (auto& tap : halfbandKernel_)
            tap /= sum;
}

void YouKnow106Engine::buildVoiceCards() noexcept
{
    // One draw per real dispersion mechanism. The envelopes themselves carry
    // no draw: they are computed digitally in the shared processor, so every
    // voice's envelope is identical and what disperses is the analogue chain
    // each one drives.
    for (int index = 0; index < maxVoices; ++index)
    {
        auto& card = cards_[static_cast<std::size_t>(index)];
        const std::uint32_t seed = static_cast<std::uint32_t>(index) * 2654435761u + 17u;
        card.rampCurrentError = hashBipolar(seed);
        card.comparatorOffset = hashBipolar(seed + 1u);
        card.cutoffOffsetError = hashBipolar(seed + 2u);
        card.resonanceError = hashBipolar(seed + 3u);
        card.vcaOffset = hashBipolar(seed + 4u);
        card.cutoffScaleError = hashBipolar(seed + 5u);
        card.subLevelError = hashBipolar(seed + 6u);
        card.driftPhase = 0.5f * (hashBipolar(seed + 7u) + 1.0f);
        card.vcaGainError = hashBipolar(seed + 8u);
        card.noiseLevelError = hashBipolar(seed + 9u);
        for (std::size_t stage = 0; stage < 4; ++stage)
        {
            card.vcfStageOffsets[stage] =
                0.0015f * hashBipolar(seed + 10u + static_cast<std::uint32_t>(stage));
        }
        card.driftValue = 0.0f;
        card.driftState = seed | 1u;
    }
}

void YouKnow106Engine::refreshVoiceCardStageOffsets() noexcept
{
    // A differential pair's input offset has a population mean of zero: there
    // is no nominal V_os a calibrated model could carry, and no service step
    // trims one. The draw above is signed and unbiased, so the whole term is
    // card-to-card spread and belongs entirely to Unit Character -- at zero,
    // the calibrated nominal model, it must be absent rather than merely
    // small. The card array itself stays the unscaled physical draw, as every
    // other card field does.
    const float amount = activeParameters_.enableVcfStageOffsets
                       ? activeParameters_.calibration : 0.0f;
    for (auto& voice : voices_)
    {
        const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
        for (std::size_t stage = 0; stage < 4; ++stage)
            voice.filter.offsetVoltage[stage] = card.vcfStageOffsets[stage] * amount;
    }
}

void YouKnow106Engine::prepare(double sampleRate, int /*maxBlockSize*/,
                               bool oversamplingEnabled)
{
    sampleRate_ = std::clamp(sampleRate, 8000.0, maximumSupportedSampleRate);
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    oversamplingRequested_ = oversamplingEnabled;
    oversamplingEnabled_ = oversamplingEnabled;
    updateProcessingRate();
    prepared_ = true;
    reset();
}

void YouKnow106Engine::updateProcessingRate(bool preserveFreeRunningState) noexcept
{
    const double previousProcessingRate = oversampledRate_;
    if (!oversamplingEnabled_ || sampleRate_ >= minimumHqProcessingRate)
        oversampling_ = 1;
    else if (sampleRate_ >= minimumHqProcessingRate / 2.0)
        oversampling_ = 2;
    else
        oversampling_ = maximumOversampleFactor;

    oversampledRate_ = sampleRate_ * oversampling_;
    inverseOversampledRate_ = static_cast<float>(1.0 / oversampledRate_);
    noiseRateScale_ = static_cast<float>(
        std::sqrt(oversampledRate_ / noiseReferenceRateHz));
    voiceBusCouplingG_ = std::tan(
        pi * voiceBusCouplingCornerHz() * inverseOversampledRate_);
    commonVcaInputCouplingG_ = std::tan(
        pi * commonVcaInputCouplingCornerHz() * inverseOversampledRate_);
    outputCouplingG_ = std::tan(
        pi * outputCouplingCornerHz() * inverseSampleRate_);
    const double deepest = totalLatencySamples(maximumOversampleFactor);
    const double running = totalLatencySamples(oversampling_);
    latencyPadSamples_ = std::clamp(
        static_cast<int>(std::floor(deepest - running + 0.5)),
        0, latencyPadRingSize - 1);
    latencyPadLeft_.fill(0.0f);
    latencyPadRight_.fill(0.0f);
    latencyPadWriteIndex_ = 0;
    oversamplingQuietSamples_ =
        std::max(1, static_cast<int>(sampleRate_ * outputPathQuietSeconds));
    rateTransitionStep_ = 1.0f / std::max(
        1.0f, static_cast<float>(sampleRate_ * rateTransitionSeconds));
    // A live quality change is a numerical implementation detail, not a power
    // cycle of the assigner or modulator. Their phases are stored in passes or
    // normalized cycles, so they survive unchanged. Only a first prepare/reset
    // starts a new scan. Preserve the remaining drift interval in seconds too.
    if (preserveFreeRunningState && driftControlCountdown_ > 0
        && previousProcessingRate > 0.0)
    {
        driftControlCountdown_ = std::max(
            1, static_cast<int>(std::llround(
                   driftControlCountdown_ * oversampledRate_
                   / previousProcessingRate)));
    }
    else if (!preserveFreeRunningState)
    {
        driftControlCountdown_ = 0;
    }
    chorus_.prepare(oversampledRate_, preserveFreeRunningState);
}

bool YouKnow106Engine::setOversamplingEnabled(bool enabled) noexcept
{
    oversamplingRequested_ = enabled;
    return applyPendingOversamplingIfIdle();
}

bool YouKnow106Engine::applyPendingOversamplingIfIdle() noexcept
{
    if (oversamplingRequested_ == oversamplingEnabled_)
    {
        // A request can be withdrawn while the old path is fading. Bring it
        // back without touching any rate-dependent state.
        if (rateTransition_ == RateTransition::FadingOut)
            rateTransition_ = RateTransition::FadingIn;
        return true;
    }
    // The last voice retiring is not the same as the instrument being quiet.
    // The delay lines still hold up to their longest setting and the
    // decimators have their own group delay, so changing the rate waits until
    // what is left in them has gone.
    if (anyVoiceActive_)
    {
        if (rateTransition_ == RateTransition::FadingOut)
            rateTransition_ = RateTransition::FadingIn;
        return false;
    }
    if (oversamplingIdleSamples_ < oversamplingQuietSamples_)
        return false;

    // Once a safety fade begins, finish it even if Chorus or its optional
    // noise control changes meanwhile. Re-evaluating the source here could
    // turn the gain back to one halfway down and expose the very state reset
    // the fade protects. Applying this to every idle quality change also
    // covers a wet-mute or filter tail whose current control already reads
    // silent, at a fixed and negligible five-millisecond cost.
    if (rateTransition_ != RateTransition::FadingOut)
    {
        rateTransition_ = RateTransition::FadingOut;
        return false;
    }
    if (rateTransitionGain_ > 0.0f)
        return false;

    oversamplingEnabled_ = oversamplingRequested_;
    updateProcessingRate(true);
    rebuildRateDependentVoiceState();
    clearRateDependentOutputPath(true);
    rateTransitionGain_ = 0.0f;
    rateTransition_ = RateTransition::FadingIn;
    return true;
}

// Sample-grid histories downstream of the voices: delay lines, decimation
// stages and latency pad. A live HQ change preserves the physical C14, HPF and
// C12 states while recomputing their coefficients for the new rate; a hard
// reset clears them. The final C17/C20 capacitors run at the host rate and
// always survive a live HQ rebuild, or asymmetric PWM would make a large step.
void YouKnow106Engine::clearRateDependentOutputPath(
    bool preserveFreeRunningState) noexcept
{
    firstDecimator_.reset();
    secondDecimator_.reset();
    // updateProcessingRate() has already replaced the chorus coefficients,
    // retained its BBD buckets/free-running phases, and cleared only support-
    // filter TPT carries that embed the old timestep. A hard reset also clears
    // the physical/free-running effect state.
    if (!preserveFreeRunningState)
        chorus_.reset(false);
    if (!preserveFreeRunningState)
    {
        voiceBusCoupling_.reset();
        highPass_.reset();
        commonVcaInputCoupling_.reset();
    }
    latencyPadLeft_.fill(0.0f);
    latencyPadRight_.fill(0.0f);
    latencyPadWriteIndex_ = 0;
}

void YouKnow106Engine::clearOutputPath() noexcept
{
    clearRateDependentOutputPath(false);
    outputCouplingLeft_.reset();
    outputCouplingRight_.reset();
}

void YouKnow106Engine::rebuildRateDependentVoiceState() noexcept
{
    for (auto& voice : voices_)
    {
        const float previousFilterG = voice.filterG;
        const double frequency = dcoQuantisedFrequency(
            voice.dco.divider, activeParameters_.range);
        voice.dco.periodSamples = frequency > 0.0
                                ? oversampledRate_ / frequency : 1.0e6;

        // Residual kernels are measured in internal samples. The safety fade
        // has reached zero, so discard their old-rate tails and prime the new
        // timeline at the oscillator's continuing physical phase.
        const double reset = static_cast<double>(
            resetFraction(voice.dco.periodSamples * inverseOversampledRate_));
        const double rise = std::max(1.0 - reset, 1.0e-4);
        const double phase = voice.dco.phase;
        const float saw = phase < rise
            ? 2.0f * clamp01(static_cast<float>(phase / rise)) - 1.0f
            : 1.0f - 2.0f * static_cast<float>((phase - rise) / reset);
        voice.dco.saw.reset();
        voice.dco.pulse.reset();
        voice.dco.sub.reset();
        voice.dco.saw.prime(saw);
        voice.dco.pulse.prime(voice.dco.pulseState);
        voice.dco.sub.prime(voice.dco.subState);

        if (voice.active || voice.cardIndex < hardwareVoices)
        {
            updateVoiceAudio(voice, activeParameters_);
            voice.filter.retime(previousFilterG, voice.filterG);
        }
    }
}

double YouKnow106Engine::totalLatencySamples(int factor) noexcept
{
    const int limited = std::max(1, factor);
    // The oscillator's residual tracks delay by their own half width, and that
    // delay is in internal samples -- so it shrinks, in output samples, as the
    // factor grows, while the decimators' delay grows. Both have to be counted,
    // or the two configurations do not line up.
    double latency = static_cast<double>(correctionHalfWidth)
                   / static_cast<double>(limited);
    constexpr double half = (halfbandTaps - 1) / 2.0;
    for (int step = limited; step > 1; step /= 2)
        latency += half / static_cast<double>(step);
    return latency;
}

int YouKnow106Engine::getProcessingLatencySamples() const noexcept
{
    // Always the deepest configuration's figure, whatever is running. The
    // quality setting can change while the host is playing, and a plug-in that
    // renegotiated its latency mid-transport would make the host re-align
    // everything around it; padding the shallower settings costs half a
    // millisecond and keeps the number the host was told true.
    return static_cast<int>(
        std::floor(totalLatencySamples(maximumOversampleFactor) + 0.5));
}

void YouKnow106Engine::applyLatencyPad(float& left, float& right) noexcept
{
    if (latencyPadSamples_ <= 0)
        return;

    latencyPadLeft_[static_cast<std::size_t>(latencyPadWriteIndex_)] = left;
    latencyPadRight_[static_cast<std::size_t>(latencyPadWriteIndex_)] = right;
    const int readIndex =
        (latencyPadWriteIndex_ - latencyPadSamples_ + latencyPadRingSize)
        % latencyPadRingSize;
    left = latencyPadLeft_[static_cast<std::size_t>(readIndex)];
    right = latencyPadRight_[static_cast<std::size_t>(readIndex)];
    latencyPadWriteIndex_ = (latencyPadWriteIndex_ + 1) % latencyPadRingSize;
}

void YouKnow106Engine::reset()
{
    for (auto& voice : voices_)
    {
        voice = Voice {};
        voice.dco.reset();
        voice.filter.reset();
        voice.envelope.reset();
    }
    for (int index = 0; index < maxVoices; ++index)
    {
        auto& voice = voices_[static_cast<std::size_t>(index)];
        voice.cardIndex = index;
        // Microscopic filter excitation belongs to the continuously powered
        // card, so seed it once per slot rather than once per MIDI assignment.
        voice.noiseState = hash32(static_cast<std::uint32_t>(index)
                                  * 2246822519u + 1u) | 1u;
    }
    // `voice = Voice {}` above zeroed the offsets, and the cards outlive a
    // reset, so put them back before anything can render a symmetric filter.
    refreshVoiceCardStageOffsets();

    clearOutputPath();
    clearHeldNotes();
    rateTransition_ = RateTransition::Idle;
    rateTransitionGain_ = 1.0f;

    thermalWarmupSeconds_ = 0.0f;
    powerSupplyDroop_ = 0.0f;
    lfoAccumulator_ = 0u;
    lfoRising_ = true;
    lfoPolarity_ = 1.0f;
    lfoValue_ = 0.0f;
    lfoDelayLevel_ = 0.0f;
    updateSharedScan(activeParameters_, lfoValue_);
    resonanceCv_ = resonanceCvTarget_;
    sharedVca_ = sharedVcaTarget_;
    pwmVolts_ = pwmVoltsTarget_;
    subCv_ = subCvTarget_;
    noiseCv_ = noiseCvTarget_;
    // The hold has to go with the level: a note arriving at the very first
    // sample of a new run gives the scan no idle pass in which to clear it, so
    // a hold left over from the previous run would be skipped.
    lfoDelayHoldoff_ = 0u;
    lfoDelayFade_ = 0u;
    controlScanPhase_ = 1.0;
    converterEventPhases_ = converterEventPhases(
        ConverterTimingProfile::NormalizedServiceChart);
    nextConverterWrite_ = 0;
    converterPassLfoGated_ = 0.0f;
    assignmentRescanPending_ = false;
    assignmentRescanPassArmed_ = false;
    rescanPreviousUnisonMembers_.fill(false);
    rescanUnisonMidiValid_ = false;
    rescanUnisonMidi_ = 60.0f;
    anyKeyDown_ = false;
    noiseState_ = 0x6d2b79f5u;
    // Both the live value and the target, or a run that stopped with the bender
    // pushed over would start the next one there: hosts are not obliged to
    // resend a neutral controller when the transport restarts, and nothing
    // would bring it back until the player touched the wheel.
    pitchBendTarget_ = 0.0f;
    modWheelTarget_ = 0.0f;
    pitchBend_ = 0.0f;
    modWheel_ = 0.0f;
    sustainPedalDown_ = false;
    generation_ = 0;
    activeVoiceCount_ = 0;
    anyVoiceActive_ = false;
    displayEnvelope_ = 0.0f;
    displayLfo_ = 0.0f;
    displayVoiceMask_ = 0;
    driftControlCountdown_ = 0;
    panelGlidePrimed_ = false;
    // A reset leaves nothing in the output path, so a quality change asked for
    // before the first block does not have to wait for one that never comes.
    oversamplingIdleSamples_ = oversamplingQuietSamples_;
}

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------

EngineParameters YouKnow106Engine::sanitise(const EngineParameters& parameters) noexcept
{
    EngineParameters result = parameters;

    const auto fix01 = [](float& value, float fallback) {
        value = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
    };

    fix01(result.lfoRate, 0.42f);
    fix01(result.lfoDelay, 0.0f);
    fix01(result.dcoLfoDepth, 0.0f);
    fix01(result.pwmDepth, 0.30f);
    fix01(result.subLevel, 0.0f);
    fix01(result.noiseLevel, 0.0f);
    fix01(result.cutoff, 0.62f);
    fix01(result.resonance, 0.10f);
    fix01(result.envDepth, 0.35f);
    fix01(result.vcfLfoDepth, 0.0f);
    fix01(result.keyFollow, 0.50f);
    fix01(result.vcaLevel, 0.80f);
    fix01(result.attack, 0.0f);
    fix01(result.decay, 0.45f);
    fix01(result.sustain, 0.70f);
    fix01(result.release, 0.30f);
    fix01(result.portamento, 0.0f);
    fix01(result.benderDcoDepth, 0.30f);
    fix01(result.benderVcfDepth, 0.0f);
    fix01(result.benderLfoDepth, 0.0f);
    fix01(result.volume, 0.80f);
    fix01(result.velocityDepth, 0.0f);
    fix01(result.calibration, 0.70f);
    fix01(result.chorusNoise, 1.0f);

    result.masterTuneCents = std::isfinite(result.masterTuneCents)
                           ? std::clamp(result.masterTuneCents, -50.0f, 50.0f)
                           : 0.0f;
    result.keyTranspose = std::clamp(result.keyTranspose, -12, 12);
    result.polyphony = std::clamp(result.polyphony, 1, maxVoices);
    return result;
}

// The shared high-pass, whose parts are one network rather than six. Recomputed
// where the switch is read rather than per voice, which is also what stops six
// voices each writing the same three values.
void YouKnow106Engine::updateSharedHighPass(const EngineParameters& parameters) noexcept
{
    voiceBusCouplingG_ = std::tan(
        pi * voiceBusCouplingCornerHz(parameters.highPass)
        * inverseOversampledRate_);
    const float corner = highPassCornerHz(parameters.highPass);
    highPassG_ =
        std::tan(pi * std::min(corner, static_cast<float>(oversampledRate_) * 0.45f)
                 * inverseOversampledRate_);
    highPassShelf_ = highPassShelfGain(parameters.highPass);
    highPassHigh_ = highPassHighGain(parameters.highPass);
}

void YouKnow106Engine::setParameters(const EngineParameters& parameters)
{
    const auto next = sanitise(parameters);
    const bool assignModeChanged = next.keyMode != activeParameters_.keyMode;
    const bool unisonVoiceCountChanged = next.polyphony != activeParameters_.polyphony
                                      && (next.keyMode == KeyMode::Unison
                                          || activeParameters_.keyMode
                                                 == KeyMode::Unison);
    const bool outputPathIdle = !prepared_
        || (!anyVoiceActive_
            && oversamplingIdleSamples_ >= oversamplingQuietSamples_);
    targetParameters_ = next;
    // Switch positions land immediately. Main VOLUME is the only continuous
    // panel control applied outside the scanned converter path; it glides in
    // the render loop so host automation cannot make a block-boundary step.
    activeParameters_ = targetParameters_;
    // Unit Character scales the stage offsets, so they follow the panel.
    refreshVoiceCardStageOffsets();

    // A host normally delivers its saved snapshot after prepare(). If the
    // output path is empty, prime every shared hold from that snapshot instead
    // of letting the first attack hear the constructor's stale patch. This is
    // especially important now that PWM, sub and noise correctly have one hold
    // for all cards rather than six incidental opportunities to catch up.
    if (outputPathIdle)
    {
        updateSharedScan(next, lfoValue_);
        resonanceCv_ = resonanceCvTarget_;
        sharedVca_ = sharedVcaTarget_;
        pwmVolts_ = pwmVoltsTarget_;
        subCv_ = subCvTarget_;
        noiseCv_ = noiseCvTarget_;
    }

    // The original assigner handles either POLY-button transition by gating
    // the six assignments, clearing its tables, and rescanning held keys. A
    // mutable plug-in voice-count has no hardware counterpart, but rebuilding
    // a live Unison stack is the only coherent equivalent when that count
    // changes.
    if (prepared_ && (assignModeChanged || unisonVoiceCountChanged))
        beginVoiceAssignmentRescan();
}

// Constant rate in pitch: a wider leap takes proportionally longer, rather than
// every glide finishing in the same time. Zero means the control is off, and a
// note steps straight to its pitch.
float YouKnow106Engine::glideStepPerScan(float portamento) noexcept
{
    const int stepUnits = portamentoIncrement(portamento);
    if (stepUnits == 0)
        return 0.0f;
    return static_cast<float>(stepUnits) / 256.0f;
}

int YouKnow106Engine::voiceLimit() const noexcept
{
    return std::clamp(activeParameters_.polyphony, 1, maxVoices);
}

// ---------------------------------------------------------------------------
// Note handling
// ---------------------------------------------------------------------------

bool YouKnow106Engine::rememberHeldNote(int midiNote, float velocity) noexcept
{
    if (midiNote < 0 || midiNote > 127)
        return false;
    const auto index = static_cast<std::size_t>(midiNote);
    const bool firstPress = heldNoteCounts_[index] == 0;
    if (firstPress)
        heldNoteVelocities_[index] = velocity;
    if (heldNoteCounts_[index] < std::numeric_limits<std::uint16_t>::max())
        ++heldNoteCounts_[index];
    return firstPress;
}

bool YouKnow106Engine::forgetHeldNote(int midiNote) noexcept
{
    if (midiNote < 0 || midiNote > 127)
        return false;
    const auto index = static_cast<std::size_t>(midiNote);
    if (heldNoteCounts_[index] == 0)
        return false;
    --heldNoteCounts_[index];
    return heldNoteCounts_[index] == 0;
}

void YouKnow106Engine::clearHeldNotes() noexcept
{
    heldNoteVelocities_.fill(0.0f);
    heldNoteCounts_.fill(0);
}

int YouKnow106Engine::highestHeldNote() const noexcept
{
    for (int note = 127; note >= 0; --note)
        if (heldNoteCounts_[static_cast<std::size_t>(note)] > 0)
            return note;
    return -1;
}

void YouKnow106Engine::releaseVoiceKey(Voice& voice) noexcept
{
    voice.keyDown = false;
    voice.releaseStamp = ++generation_;
    if (sustainPedalDown_)
    {
        voice.sustained = true;
    }
    else
    {
        voice.releasing = true;
        voice.envelope.noteOff();
    }
}

void YouKnow106Engine::dropFromUnison(Voice& voice) noexcept
{
    voice.unisonMember = false;
    if (voice.keyDown)
        releaseVoiceKey(voice);
}

bool YouKnow106Engine::anyVoiceSounding() const noexcept
{
    for (const auto& voice : voices_)
        if (voice.active)
            return true;
    return false;
}

void YouKnow106Engine::beginVoiceAssignmentRescan() noexcept
{
    // The POLY-button handler gates every current assignment and clears the
    // assigner's note/voice tables. Its keyboard pass is separate, so do not
    // collapse the gate-off and the replacement Note On into one host event.
    if (!assignmentRescanPending_)
    {
        rescanPreviousUnisonMembers_.fill(false);
        rescanUnisonMidiValid_ = false;
        for (int slot = 0; slot < maxVoices; ++slot)
        {
            const auto& voice = voices_[static_cast<std::size_t>(slot)];
            if (!voice.active || !voice.unisonMember)
                continue;
            rescanPreviousUnisonMembers_[static_cast<std::size_t>(slot)] = true;
            if (!rescanUnisonMidiValid_)
            {
                rescanUnisonMidi_ = voice.currentMidi;
                rescanUnisonMidiValid_ = true;
            }
        }
    }

    assignmentRescanPending_ = true;
    // A handler may arrive after some voice writes in the current pass. Only a
    // wholly subsequent pass can guarantee that all six CPUs observed gate-off.
    assignmentRescanPassArmed_ = false;
    for (auto& voice : voices_)
    {
        voice.unisonMember = false;
        if (voice.active && voice.keyDown)
            releaseVoiceKey(voice);
        // Only the assigner loses this. The voice CPU's last pitch byte, DCO
        // phase and portamento accumulator all survive the table clear.
        voice.hasAllocatorHistory = false;
        voice.lastRootMidi = -1;
        voice.releaseStamp = 0;
    }
    generation_ = 0;
    updateActiveVoiceCount();
}

void YouKnow106Engine::completeVoiceAssignmentRescan() noexcept
{
    if (!assignmentRescanPending_)
        return;

    // The physical matrix is scanned from its high address down. Solo Unison
    // consumes the first set bit once; the poly modes continue descending and
    // therefore give a limited pool to the highest held keys.
    anyKeyDown_ = highestHeldNote() >= 0;
    if (activeParameters_.keyMode == KeyMode::Unison)
    {
        const int note = highestHeldNote();
        if (note >= 0)
            assignHeldNote(note,
                           heldNoteVelocities_[static_cast<std::size_t>(note)]);
    }
    else
    {
        for (int note = 127; note >= 0; --note)
        {
            const auto index = static_cast<std::size_t>(note);
            if (heldNoteCounts_[index] != 0)
                assignHeldNote(note, heldNoteVelocities_[index]);
        }
    }

    assignmentRescanPending_ = false;
    assignmentRescanPassArmed_ = false;
    rescanPreviousUnisonMembers_.fill(false);
    rescanUnisonMidiValid_ = false;
    updateActiveVoiceCount();
}

// The delay is a hold followed by a fade. Both start again for a new phrase.
void YouKnow106Engine::rearmLfoDelay() noexcept
{
    lfoDelayHoldoff_ = 0u;
    lfoDelayFade_ = 0u;
    lfoDelayLevel_ = 0.0f;
}

int YouKnow106Engine::findVoiceForNote(int midiNote) const noexcept
{
    const int limit = voiceLimit();
    for (int slot = 0; slot < limit; ++slot)
    {
        const auto& voice = voices_[static_cast<std::size_t>(slot)];
        if (voice.active && voice.keyDown && voice.rootMidi == midiNote)
            return slot;
    }
    return -1;
}

// The key assigner never steals a sounding key. With every voice held, a
// further note is simply dropped -- which is the assigner firmware's own
// policy, and the reason dense chords lose notes on it. A voice whose key has
// been let go is available again even while its release rings.
int YouKnow106Engine::allocateVoice(int midiNote) noexcept
{
    const int limit = voiceLimit();
    const auto available = [this](int slot) {
        const auto& voice = voices_[static_cast<std::size_t>(slot)];
        // Sustain belongs to the voice CPU, not the key assigner. A key-up
        // frees the slot immediately even while the pedal keeps its old tail
        // audible; the next assignment is allowed to retrigger that slot.
        return !voice.active || !voice.keyDown;
    };

    if (activeParameters_.keyMode == KeyMode::Poly2)
    {
        // A plain linear scan from the first voice up, so low slots are
        // reused immediately and their release tails chopped; only the most
        // recent notes keep a full release. That truncation is the point of
        // the mode, and it is also what makes its per-voice glide musical.
        for (int slot = 0; slot < limit; ++slot)
            if (available(slot))
                return slot;
        return -1;
    }

    // Note memory first: a free voice whose *last* note -- even one whose
    // release has long finished -- matches the incoming pitch is taken, so a
    // repeated note lands on the voice already sitting at its pitch and
    // filter state. Otherwise the free voice that has been released longest
    // is taken, which is what preserves the freshest tails when keys come up
    // out of order.
    for (int slot = 0; slot < limit; ++slot)
    {
        const auto& voice = voices_[static_cast<std::size_t>(slot)];
        if (available(slot) && voice.hasAllocatorHistory
            && voice.lastRootMidi == midiNote)
            return slot;
    }

    int best = -1;
    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
    for (int slot = 0; slot < limit; ++slot)
    {
        const auto& voice = voices_[static_cast<std::size_t>(slot)];
        if (!available(slot))
            continue;
        // A slot that has never played counts as released at the dawn of
        // time, so an empty instrument fills from the first voice up.
        const std::uint64_t stamp = voice.hasAllocatorHistory
                                  ? voice.releaseStamp : 0;
        if (stamp < oldest || best < 0)
        {
            oldest = stamp;
            best = slot;
        }
    }
    return best;
}

void YouKnow106Engine::initialiseVoice(Voice& voice, int slot, int midiNote,
                                       float velocity) noexcept
{
    const auto& parameters = activeParameters_;
    const bool wasSounding = voice.active;
    const bool voiceWasRunning = voice.keyDown || voice.sustained;
    const int voiceMidi = midiNote + parameters.keyTranspose;
    const bool pitchChanged = !voice.hasVoicePitchHistory
                           || voice.lastVoiceMidi != voiceMidi;

    voice.cardIndex = slot;
    // A no-op while slot and card index always agree, but written so a future
    // assigner change cannot silently leave a voice on another card's offsets.
    refreshVoiceCardStageOffsets();
    voice.active = true;
    voice.keyDown = true;
    voice.sustained = false;
    voice.releasing = false;
    voice.rootMidi = midiNote;
    voice.velocity = velocity;
    voice.generation = ++generation_;
    voice.energy = 0.0f;

    const float target = static_cast<float>(voiceMidi);
    voice.targetMidi = target;

    // Physical sample-and-hold capacitor residual charge leakage: re-striking a
    // sounding voice before its envelope decays produces an analog key-click pulse.
    if (wasSounding && voice.envelope.level > 0u && parameters.calibration > 0.0f)
    {
        const float residualFraction = static_cast<float>(voice.envelope.level) / static_cast<float>(envelopePeak);
        voice.vcaControl += residualFraction * 0.08f * parameters.calibration;
    }

    voice.glideSemitonesPerScan = glideStepPerScan(parameters.portamento);
    if (voice.glideSemitonesPerScan > 0.0f)
    {
        // The glide integrator is per voice and survives retirement, so a
        // reassigned voice slides from whatever *its CPU* last played -- notes
        // several allocator assignments back, exactly the instrument's own
        // poly-glide behaviour. Only a CPU that has never received a note in
        // this run starts where it is asked to.
        if (!wasSounding)
            voice.currentMidi = voice.hasVoicePitchHistory
                              ? voice.currentMidi : target;
    }
    else
    {
        voice.currentMidi = target;
    }
    voice.lastRootMidi = midiNote;
    voice.hasAllocatorHistory = true;
    voice.lastVoiceMidi = voiceMidi;
    voice.hasVoicePitchHistory = true;

    // A running note timer is not restarted by a legato pitch message. A
    // different pitch on a free/releasing voice requests a restart, but the
    // voice CPU consumes it only on that voice's next scan update.
    if (pitchChanged && !voiceWasRunning)
        voice.dcoResetPending = true;

    if (!wasSounding)
        voice.vca = 0.0f;
    voice.envelope.noteOn();
}

void YouKnow106Engine::silenceVoice(Voice& voice) noexcept
{
    voice.active = false;
    voice.keyDown = false;
    voice.sustained = false;
    voice.releasing = false;
    voice.unisonMember = false;
    voice.rootMidi = -1;
    voice.vca = 0.0f;
    voice.vcaControlTarget = 0.0f;
    voice.vcaControl = 0.0f;
    voice.energy = 0.0f;
    voice.envelope.reset();
    if (voice.cardIndex >= hardwareVoices)
    {
        // Product-extension slots have no powered hardware card while idle.
        // Since renderVoice deliberately stops advancing them, retaining an
        // old resonant filter or oscillator timeline here would freeze it in
        // amber and resurrect it on a later assignment. Keep the intended
        // digital note/portamento memories below, but reconstruct the virtual
        // audio cell from silence next time it is used.
        voice.dco.reset();
        voice.pulseDutyPrimed = false;
        voice.filter.reset();
        voice.noiseState = hash32(
            static_cast<std::uint32_t>(voice.cardIndex) * 2246822519u + 1u) | 1u;
    }
    // Physical slots deliberately keep their free-running DCO, filter and
    // card-noise state. Every slot keeps both digital note memories,
    // currentMidi and releaseStamp for the assigner/portamento policy.
}

void YouKnow106Engine::noteOn(int midiNote, float velocity)
{
    if (midiNote < 0 || midiNote > 127)
        return;
    noteOnInternal(midiNote, std::clamp(velocity, 0.0f, 1.0f));
}

void YouKnow106Engine::noteOnInternal(int midiNote, float velocity) noexcept
{
    // The assigner consumes changes in the keyboard bitfield, not repeated
    // writes of a bit that is already high. Keep the count balanced for MIDI
    // streams that overlap equal notes, but do not retrigger on that repeat.
    if (!rememberHeldNote(midiNote, velocity))
        return;

    // A phrase starts when no key was down -- release tails still ringing do
    // not count, because the firmware's retrigger latch watches the key-gate
    // mask, not envelope activity. A note played over ringing releases
    // therefore restarts the delay, which the previous
    // wait-for-silence condition got wrong.
    if (!anyKeyDown_)
        rearmLfoDelay();

    anyKeyDown_ = true;
    // A POLY handler has cleared the allocator and is waiting for the keyboard
    // scan. This new bit will be discovered by that same descending pass.
    if (assignmentRescanPending_)
        return;
    assignHeldNote(midiNote, velocity);
}

void YouKnow106Engine::assignHeldNote(int midiNote, float velocity) noexcept
{
    if (activeParameters_.keyMode == KeyMode::Unison)
    {
        // Every voice takes the same note, and every note timer divides the
        // same reference by the same integer, so there is no pitch spread at
        // all: what separates the six is the analogue block after them. Adding
        // a detune here would be inventing a behaviour the instrument does not
        // have. Each slot's glide starts from that slot's own pitch history,
        // exactly as its per-voice integrator does; on the hardware all six
        // always share that history, so a slot woken into a wider stack than
        // it left -- a voice count the hardware cannot change -- adopts the
        // stack's position rather than inventing one of its own.
        const int limit = voiceLimit();
        float stackMidi = 0.0f;
        bool haveStackMidi = false;
        for (int slot = 0; slot < limit && !haveStackMidi; ++slot)
        {
            const auto& voice = voices_[static_cast<std::size_t>(slot)];
            if (voice.active && voice.unisonMember)
            {
                stackMidi = voice.currentMidi;
                haveStackMidi = true;
            }
        }
        if (!haveStackMidi && assignmentRescanPending_
            && rescanUnisonMidiValid_)
        {
            stackMidi = rescanUnisonMidi_;
            haveStackMidi = true;
        }
        for (int slot = 0; slot < limit; ++slot)
        {
            auto& voice = voices_[static_cast<std::size_t>(slot)];
            const bool joiningWidenedStack = assignmentRescanPending_
                && !rescanPreviousUnisonMembers_[static_cast<std::size_t>(slot)];
            const bool freshVoiceCpu = !voice.hasVoicePitchHistory;
            initialiseVoice(voice, slot, midiNote, velocity);
            if ((freshVoiceCpu || joiningWidenedStack) && haveStackMidi
                && voice.glideSemitonesPerScan > 0.0f)
                voice.currentMidi = stackMidi;
            voice.unisonMember = true;
        }
        // A voice count lowered while a wider stack was sounding leaves slots
        // above the new count still keyed to the old note. They leave the stack
        // here rather than holding that note against the new one, which would
        // turn Unison into a chord.
        for (int slot = limit; slot < maxVoices; ++slot)
        {
            auto& voice = voices_[static_cast<std::size_t>(slot)];
            if (voice.active && voice.unisonMember)
                dropFromUnison(voice);
        }
        updateActiveVoiceCount();
        return;
    }

    const int existing = findVoiceForNote(midiNote);
    const int slot = existing >= 0 ? existing : allocateVoice(midiNote);
    if (slot < 0)
        return; // Every key is held: the note is dropped, as on the hardware.

    auto& voice = voices_[static_cast<std::size_t>(slot)];
    initialiseVoice(voice, slot, midiNote, velocity);
    voice.unisonMember = false;
    updateActiveVoiceCount();
}

void YouKnow106Engine::noteOff(int midiNote)
{
    if (midiNote < 0 || midiNote > 127)
        return;
    noteOffInternal(midiNote);
}

void YouKnow106Engine::reassertKeyMode() noexcept
{
    if (prepared_)
        beginVoiceAssignmentRescan();
}

void YouKnow106Engine::noteOffInternal(int midiNote) noexcept
{
    // No high-to-low bit transition means no firmware handler. In particular,
    // an unmatched Note Off must not make Solo Unison gate and rescan.
    if (!forgetHeldNote(midiNote))
        return;

    const int remaining = highestHeldNote();
    anyKeyDown_ = remaining >= 0;

    // The old assignments have already been gated. Only the held-key table
    // needs changing before the pending descending scan reaches it.
    if (assignmentRescanPending_)
        return;

    if (activeParameters_.keyMode == KeyMode::Unison)
    {
        // Any physical key-up makes the Solo Unison handler gate the stack,
        // clear its note table and rescan the keyboard. The highest remaining
        // key therefore wins, and every surviving note is a fresh envelope
        // attack rather than a legato hand-off.
        if (remaining >= 0)
        {
            beginVoiceAssignmentRescan();
            return;
        }

        for (auto& voice : voices_)
            if (voice.active && voice.unisonMember && voice.keyDown)
                releaseVoiceKey(voice);
        return;
    }

    for (auto& voice : voices_)
    {
        if (!voice.active || voice.rootMidi != midiNote || !voice.keyDown)
            continue;
        releaseVoiceKey(voice);
    }
}

void YouKnow106Engine::releaseAllNotes()
{
    clearHeldNotes();
    anyKeyDown_ = false;
    assignmentRescanPending_ = false;
    assignmentRescanPassArmed_ = false;
    rescanPreviousUnisonMembers_.fill(false);
    rescanUnisonMidiValid_ = false;
    for (auto& voice : voices_)
    {
        if (!voice.active || !voice.keyDown)
            continue;
        releaseVoiceKey(voice);
    }
}

void YouKnow106Engine::allNotesOff()
{
    clearHeldNotes();
    anyKeyDown_ = false;
    assignmentRescanPending_ = false;
    assignmentRescanPassArmed_ = false;
    rescanPreviousUnisonMembers_.fill(false);
    rescanUnisonMidiValid_ = false;
    sustainPedalDown_ = false;
    for (auto& voice : voices_)
        silenceVoice(voice);
    // A hard stop has to be silent now, not once the delay lines have run out.
    // Cutting the voices alone would leave the chorus playing back the last
    // few milliseconds of a held chord after the panic.
    clearOutputPath();
    oversamplingIdleSamples_ = oversamplingQuietSamples_;
    updateActiveVoiceCount();
}

void YouKnow106Engine::setPitchBend(float normalisedBipolar) noexcept
{
    pitchBendTarget_ = std::clamp(sanitised(normalisedBipolar, 0.0f), -1.0f, 1.0f);
}

void YouKnow106Engine::setModWheel(float amount) noexcept
{
    modWheelTarget_ = clamp01(sanitised(amount, 0.0f));
}

void YouKnow106Engine::setSustainPedal(bool down) noexcept
{
    if (sustainPedalDown_ == down)
        return;
    sustainPedalDown_ = down;
    if (down)
        return;

    for (auto& voice : voices_)
        if (voice.active && voice.sustained && !voice.keyDown)
        {
            voice.sustained = false;
            voice.releasing = true;
            voice.envelope.noteOff();
        }
}

void YouKnow106Engine::updateActiveVoiceCount() noexcept
{
    int count = 0;
    int mask = 0;
    for (int slot = 0; slot < maxVoices; ++slot)
        if (voices_[static_cast<std::size_t>(slot)].active)
        {
            ++count;
            if (slot < 16)
                mask |= 1 << slot;
        }
    activeVoiceCount_ = count;
    anyVoiceActive_ = count > 0;
    displayVoiceMask_ = mask;
}

// ---------------------------------------------------------------------------
// Modulation
// ---------------------------------------------------------------------------

void YouKnow106Engine::advanceLfo(const EngineParameters& parameters) noexcept
{
    // The modulator is firmware: it advances once per converter scan and holds
    // its value in between. That staircase is audible as a faint roughness on
    // deep, slow vibrato, and smoothing it away would be modelling a different
    // instrument. The caller invokes this exactly once per pass.

    // A clamped accumulator, not a phase: the rate coefficient is added until
    // the span clamps, the direction flips there, and a polarity flip at the
    // bottom folds the two sweeps into a bipolar triangle. The clamp discards
    // whatever the last step overshot by, so fast settings quantise onto
    // whole passes per sweep.
    const std::uint16_t coefficient = lfoRateIncrement(parameters.lfoRate);
    if (lfoRising_)
    {
        const std::uint32_t next =
            static_cast<std::uint32_t>(lfoAccumulator_) + coefficient;
        if (next >= 0x2000u)
        {
            lfoAccumulator_ = 0x1fffu;
            lfoRising_ = false;
        }
        else
            lfoAccumulator_ = static_cast<std::uint16_t>(next);
    }
    else
    {
        if (coefficient > lfoAccumulator_)
        {
            lfoAccumulator_ = 0u;
            lfoRising_ = true;
            lfoPolarity_ = -lfoPolarity_;
        }
        else
            lfoAccumulator_ = static_cast<std::uint16_t>(
                lfoAccumulator_ - coefficient);
    }
    lfoValue_ = lfoPolarity_
              * static_cast<float>(lfoAccumulator_) / 8191.0f;

    // Delay: a silent hold that advances at the attack table's own rate, then
    // the stepped fade. The pair is re-armed the moment a note starts with no
    // key down -- release tails still ringing keep their vibrato, because the
    // firmware's retrigger latch watches the keys, not the envelopes.
    if (lfoDelayHoldoff_ < 0x4000u)
    {
        lfoDelayHoldoff_ = std::min<std::uint32_t>(
            0x4000u,
            lfoDelayHoldoff_ + envelopeAttackIncrement(parameters.lfoDelay));
        lfoDelayLevel_ = 0.0f;
    }
    else if (lfoDelayFade_ < 0x10000u)
    {
        lfoDelayFade_ = std::min<std::uint32_t>(
            0x10000u,
            lfoDelayFade_ + lfoDelayFadeIncrement(parameters.lfoDelay));
        if (lfoDelayFade_ >= 0x10000u)
            lfoDelayLevel_ = 1.0f;
        else
            lfoDelayLevel_ = static_cast<float>(lfoDelayFade_ >> 8u) / 255.0f;
    }
    else
        lfoDelayLevel_ = 1.0f;

    displayLfo_ = lfoValue_ * lfoDelayLevel_;
}

void YouKnow106Engine::updateVoiceCardDrift(VoiceCard& card) noexcept
{
    // A slow, bounded wander of the analogue control chain. It is deliberately
    // small: the oscillators share one reference, so this instrument does not
    // drift the way six free-running oscillators would.
    card.driftState ^= card.driftState << 13;
    card.driftState ^= card.driftState >> 17;
    card.driftState ^= card.driftState << 5;
    const float excitation =
        static_cast<float>(card.driftState & 0xffffu) * (2.0f / 65535.0f) - 1.0f;
    card.driftValue = card.driftValue * 0.9992f + excitation * 0.004f;
}

void YouKnow106Engine::updateVoiceScan(Voice& voice,
                                       const EngineParameters& parameters,
                                       float lfoGated) noexcept
{
    updateVoiceEnvelopeAndPitch(voice, parameters, lfoGated);
    updateVoiceVcfTarget(voice, parameters, lfoGated);
    updateVoiceVcaTarget(voice, parameters);
}

void YouKnow106Engine::updateVoiceEnvelopeAndPitch(
    Voice& voice, const EngineParameters& parameters, float lfoGated) noexcept
{

    // Stored tone controls and the relevant sensitivity controls are consumed
    // on their seven-bit grid. Portamento is deliberately absent here: that
    // non-stored performance pot uses an eight-bit ADC code and raw>>1 selector.
    const auto byte7 = [](float value) { return storedControlFraction(value); };

    // --- Envelope ---------------------------------------------------------
    // A linear attack and multiplicative falling segments, advanced once per
    // scan. The published minimum of 1.5 ms is shorter than one scan pass, so
    // the shortest attack the instrument can actually produce is one pass
    // long. There is no per-voice rate error here: the generator is the
    // shared processor, so six voices' envelopes are digitally identical and
    // only the analogue chain after them disperses.
    // Recurrence, widths and coefficient laws are resolved for the supplied,
    // hash-matched B-2 image.
    voice.attackIncrement = envelopeAttackIncrement(parameters.attack);
    voice.decayMultiplier = envelopeDecayReleaseMultiplier(parameters.decay);
    voice.releaseMultiplier = envelopeDecayReleaseMultiplier(parameters.release);

    voice.envelope.tick(voice.attackIncrement, voice.decayMultiplier,
                        storedControlAlignedWord(parameters.sustain),
                        voice.releaseMultiplier);

    // --- Pitch ------------------------------------------------------------
    // Recomputed from the key rather than cached at note-on, so moving the
    // transpose control takes a held note with it.
    if (voice.rootMidi >= 0)
    {
        const int voiceMidi = voice.rootMidi + parameters.keyTranspose;
        voice.targetMidi = static_cast<float>(voiceMidi);
        // Transpose is part of the pitch byte delivered to the voice CPU, not
        // of the assigner's physical-key memory. A held-note transpose change
        // therefore becomes the CPU's new reset-comparison history too. If
        // this stayed at the note-on value, replaying the same resulting board
        // pitch after release would falsely restart the free-running DCO.
        const bool pitchChanged = !voice.hasVoicePitchHistory
                               || voice.lastVoiceMidi != voiceMidi;
        // With either run bit set this is a legato pitch message and the DCO
        // stays free-running. During an ordinary release both bits are clear,
        // so the voice CPU applies its normal different-pitch reset rule.
        if (pitchChanged && !voice.keyDown && !voice.sustained)
            voice.dcoResetPending = true;
        voice.lastVoiceMidi = voiceMidi;
        voice.hasVoicePitchHistory = true;
    }

    // Taken from the control as it stands, not from what it read when the key
    // went down. The glide rate is a resistance in the pitch integrator's path,
    // and turning that control while a note is sliding changes the slide --
    // including turning it off, which lands the note on its pitch at the next
    // scan rather than leaving it crawling.
    voice.glideSemitonesPerScan = glideStepPerScan(parameters.portamento);

    if (voice.glideSemitonesPerScan > 0.0f)
    {
        const float distance = voice.targetMidi - voice.currentMidi;
        const float step = std::min(std::abs(distance), voice.glideSemitonesPerScan);
        voice.currentMidi += distance < 0.0f ? -step : step;
    }
    else
    {
        voice.currentMidi = voice.targetMidi;
    }

    // The bender's own modulation axis and the panel's modulator slider are
    // summed by the firmware, so pushing both reaches deeper than either
    // alone -- up to twice one slider's span, the byte arithmetic's own
    // bound.
    const float lfoPitchDepth = std::min(
        2.0f, byte7(parameters.dcoLfoDepth)
                  + byte7(parameters.benderLfoDepth) * modWheel_);
    const float cents = parameters.masterTuneCents
        + parameters.benderDcoDepth * benderPitchCents * pitchBend_
        + lfoPitchDepth * lfoPitchCents * lfoGated;
    const double midi = static_cast<double>(voice.currentMidi)
                      + static_cast<double>(cents) / 100.0;

    voice.dco.divider = dcoDivider(midiToHz(midi));
    const double frequency = dcoQuantisedFrequency(voice.dco.divider, parameters.range);
    voice.dco.periodSamples = frequency > 0.0 ? oversampledRate_ / frequency : 1.0e6;
    // The compensation voltage the firmware writes for this pitch. The count
    // above reprogrammes the timer instantly; this target reaches the
    // integrator through the hold capacitor's slew, and the ratio of the two
    // is the momentary amplitude error every pitch step leaves behind.
    voice.dcoCvTarget = frequency > 0.0 ? static_cast<float>(frequency) : 1.0f;
}

void YouKnow106Engine::updateVoiceVcfTarget(
    Voice& voice, const EngineParameters& parameters, float lfoGated) noexcept
{
    const auto byte7 = [](float value) { return storedControlFraction(value); };
    const float envelope = voice.envelope.value;

    // --- Filter cutoff, summed in converter counts ------------------------
    float counts = vcfPanelCounts(parameters.cutoff);
    const float envelopeSign =
        parameters.envPolarity == EnvPolarity::Normal ? 1.0f : -1.0f;
    counts += envelopeSign * byte7(parameters.envDepth) * vcfEnvelopeCounts
            * envelope;
    counts += byte7(parameters.vcfLfoDepth) * vcfLfoCounts * lfoGated;
    counts += byte7(parameters.benderVcfDepth) * vcfBenderCounts * pitchBend_;
    counts += byte7(parameters.keyFollow) * vcfCountsPerOctave
            * (voice.currentMidi - vcfKeyFollowCentreMidi) / 12.0f;
    // The firmware clamps the sum to its 14-bit accumulator -- so the digital
    // part of the control voltage can never ask for less than the law's base
    // frequency -- and hands the converter the top twelve bits, so it moves
    // in 4-count steps. The analogue trims and drift ride on top of this at
    // the audio grid, below the converter's own resolution, exactly where
    // the hardware's trimmers sit.
    counts = std::clamp(counts, 0.0f, vcfCountsCeiling);
    voice.cutoffCountsTarget =
        vcfDacCountStep * std::floor(counts / vcfDacCountStep);
}

void YouKnow106Engine::updateVoiceVcaTarget(
    Voice& voice, const EngineParameters& parameters) noexcept
{
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float tolerance = parameters.calibration;
    const float envelope = voice.envelope.value;

    // --- Amplifier control ------------------------------------------------
    const float velocityGain = 1.0f
        - parameters.velocityDepth * (1.0f - voice.velocity);
    const float control = parameters.vcaMode == VcaMode::Envelope
                        ? envelope
                        : (voice.keyDown || voice.sustained ? 1.0f : 0.0f);
    voice.vcaControlTarget = clamp01(control * velocityGain
                                     + card.vcaOffset * 0.004f * tolerance);
}

void YouKnow106Engine::updateSharedScan(const EngineParameters& parameters,
                                        float lfoRaw) noexcept
{
    const auto converterFraction = [](float value) {
        return static_cast<float>(storedControlDacCode(value)) / 4064.0f;
    };
    resonanceCvTarget_ = converterFraction(parameters.resonance);
    sharedVcaTarget_ = converterFraction(parameters.vcaLevel);
    subCvTarget_ = converterFraction(parameters.subLevel);
    noiseCvTarget_ = converterFraction(parameters.noiseLevel);

    if (!parameters.pulseEnabled)
    {
        // The service notes specify this control state directly. What the
        // pinned leg contributes through the downstream mixer remains open,
        // so renderVoice retains the existing audio-path gate for now.
        pwmVoltsTarget_ = -0.8f;
        return;
    }

    float pwmAmount = converterFraction(parameters.pwmDepth);
    if (parameters.pwmSource == PwmSource::Lfo)
        pwmAmount *= 0.5f * (1.0f + lfoRaw);
    pwmVoltsTarget_ = pwmControlVolts(clamp01(pwmAmount));
}

void YouKnow106Engine::performConverterWrite(
    const ConverterWrite& write, const EngineParameters& parameters,
    float lfoGated) noexcept
{
    const auto converterFraction = [](float value) {
        return static_cast<float>(storedControlDacCode(value)) / 4064.0f;
    };
    const auto validPhysicalVoice = [&write] {
        return write.voice >= 0 && write.voice < hardwareVoices;
    };

    float currentDacFraction = 0.0f;
    switch (write.destination)
    {
        case ConverterDestination::Resonance: currentDacFraction = converterFraction(parameters.resonance); break;
        case ConverterDestination::CommonVca: currentDacFraction = converterFraction(parameters.vcaLevel); break;
        case ConverterDestination::Sub: currentDacFraction = converterFraction(parameters.subLevel); break;
        case ConverterDestination::Noise: currentDacFraction = converterFraction(parameters.noiseLevel); break;
        case ConverterDestination::Pwm: currentDacFraction = converterFraction(parameters.pwmDepth); break;
        default: break;
    }

    if (parameters.enableMuxCrosstalk && std::abs(currentDacFraction - previousDacFraction_) > 1.0e-5f)
    {
        const float dacStep = currentDacFraction - previousDacFraction_;
        const float injection = dacStep * 0.0025f;
        if (validPhysicalVoice())
        {
            auto& voice = voices_[static_cast<std::size_t>(write.voice)];
            if (write.destination == ConverterDestination::Vcf)
                voice.cutoffCountsTarget += injection * vcfCountsPerOctave;
            else if (write.destination == ConverterDestination::VoiceVca)
                voice.vcaControlTarget += injection;
        }
    }
    previousDacFraction_ = currentDacFraction;

    switch (write.destination)
    {
        case ConverterDestination::Resonance:
            resonanceCvTarget_ = converterFraction(parameters.resonance);
            break;
        case ConverterDestination::CommonVca:
            sharedVcaTarget_ = converterFraction(parameters.vcaLevel);
            break;
        case ConverterDestination::Sub:
            subCvTarget_ = converterFraction(parameters.subLevel);
            break;
        case ConverterDestination::Pitch:
            if (validPhysicalVoice())
            {
                auto& voice = voices_[static_cast<std::size_t>(write.voice)];
                const double previousPeriod = voice.dco.periodSamples;
                updateVoiceEnvelopeAndPitch(voice, parameters, lfoGated);
                if (voice.dcoResetPending)
                {
                    restartDcoBandlimited(voice, previousPeriod);
                    voice.dcoResetPending = false;
                }
            }
            break;
        case ConverterDestination::Pwm:
            if (!parameters.pulseEnabled)
            {
                pwmVoltsTarget_ = -0.8f;
                break;
            }
            {
                float amount = converterFraction(parameters.pwmDepth);
                if (parameters.pwmSource == PwmSource::Lfo)
                    amount *= 0.5f * (1.0f + lfoValue_);
                pwmVoltsTarget_ = pwmControlVolts(clamp01(amount));
            }
            break;
        case ConverterDestination::Vcf:
            if (validPhysicalVoice())
                updateVoiceVcfTarget(
                    voices_[static_cast<std::size_t>(write.voice)],
                    parameters, lfoGated);
            break;
        case ConverterDestination::VoiceVca:
            if (validPhysicalVoice())
                updateVoiceVcaTarget(
                    voices_[static_cast<std::size_t>(write.voice)], parameters);
            break;
        case ConverterDestination::Noise:
            noiseCvTarget_ = converterFraction(parameters.noiseLevel);
            break;
    }
}

void YouKnow106Engine::updateVoiceAudio(Voice& voice,
                                        const EngineParameters& parameters) noexcept
{
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float tolerance = parameters.calibration;

    // The regeneration control voltage is shared -- one converter output for
    // all six loops -- but each voice's loop amplifier has its own gain
    // spread.
    // Voiced, and back to being voiced. The service procedure trims each loop
    // to a 4.8 Vpp self-oscillation peak but states no tolerance on the result,
    // so what spread survives the adjustment is documented nowhere located. A
    // revision briefly anchored 5% to a source describing the *untrimmed*
    // component class, which is a different question: a trimmed mechanism's
    // residual is not its parts' tolerance.
    const float resonancePanel = clamp01(resonanceCv_
        + card.resonanceError * 0.02f * tolerance);
    voice.feedback =
        VoicedResonanceCompatibilityProfile::loopGain(resonancePanel);
    voice.inputCompensation =
        VoicedResonanceCompatibilityProfile::inputCompensation(voice.feedback);

    // The analogue side of the cutoff chain: the two per-voice trimmers --
    // one scales the control voltage, one offsets it -- imperfectly set, and
    // the slow thermal wander, all riding below the converter's own
    // resolution on the slewed digital value. The five-per-cent scale and
    // tenth-octave offset spans are voiced Unit Character policies, not
    // measured post-calibration residual distributions.
    // A sagging rail pulls the cutoff reference down with it. `tolerance` is
    // applied here and only here: the droop state itself is a pure load
    // measure, so this mechanism scales linearly with Unit Character like its
    // eighteen siblings rather than quadratically.
    const float psuCutoffShift =
        -powerSupplyDroop_ * railToCutoffCountsPerVolt * tolerance;
    const float analogCounts = voice.cutoffCounts
        * (1.0f + card.cutoffScaleError * 0.05f * tolerance)
        + card.cutoffOffsetError * 0.07f * vcfCountsPerOctave * tolerance
        + card.driftValue * 40.0f * tolerance
        + psuCutoffShift;
    const float cutoffHz = vcfEffectiveCutoffHz(analogCounts, voice.feedback, tolerance);
    const float limited =
        std::min(cutoffHz, static_cast<float>(oversampledRate_) * 0.45f);
    voice.filterG = std::tan(pi * limited * inverseOversampledRate_);

    voice.vca = VoicedVoiceVcaCompatibilityProfile::gain(voice.vcaControl)
              * (1.0f + card.vcaGainError * 0.03f * tolerance);

    // The IR3109 stage offsets used to be rewritten here, every audio sample,
    // from values that never change. They now live in
    // refreshVoiceCardStageOffsets, called where the card or the panel moves.
}

void YouKnow106Engine::updatePulseComparator(
    Voice& voice, const EngineParameters& parameters) noexcept
{
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float amplitudeScale = dcoRampAmplitudeScale(voice, parameters);
    // The alignment window is 48% to 52% duty across cards: +/-0.24 V is
    // +/-2 points on the 12 V ramp. Pulse Off remains separate at -0.8 V and
    // pins the comparator high even while this card's VCA is shut.
    const float threshold = pwmVolts_
                          + card.comparatorOffset * 0.24f
                                * parameters.calibration;
    voice.pulseDuty = pwmDutyCycle(threshold, amplitudeScale);
}

float YouKnow106Engine::dcoRampAmplitudeScale(
    const Voice& voice, const EngineParameters& parameters) const noexcept
{
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float heldCompensation = std::clamp(
        voice.dcoCv / std::max(voice.dcoCvTarget, 1.0e-3f), 0.25f, 4.0f);
    const float cardCurrent =
        1.0f + card.rampCurrentError * 0.03f * parameters.calibration;
    return heldCompensation * cardCurrent;
}

bool YouKnow106Engine::pulseMixEnabled(bool requested, float duty) noexcept
{
    return requested && duty < 1.0f;
}

// ---------------------------------------------------------------------------
// Voice rendering
// ---------------------------------------------------------------------------

float YouKnow106Engine::renderVoice(Voice& voice, const EngineParameters& parameters,
                                    float noiseSample) noexcept
{
    // Extension slots have no continuously powered voice card behind them.
    // Their digital portamento state still advances on the converter pass,
    // but an unassigned slot has no DCO/filter/audio state that must run.
    if (!voice.active && voice.cardIndex >= hardwareVoices)
        return 0.0f;

    auto& dco = voice.dco;
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];

    const double increment = dco.periodSamples > 1.0e-9
                           ? 1.0 / dco.periodSamples : 0.0;
    const double reset = static_cast<double>(
        resetFraction(dco.periodSamples * inverseOversampledRate_));
    const double rise = std::max(1.0 - reset, 1.0e-4);

    const double previousPhase = dco.phase;
    const double unwrapped = dco.phase + increment;
    const bool wrapped = unwrapped >= 1.0;
    dco.phase = wrapped ? unwrapped - std::floor(unwrapped) : unwrapped;
    const double phase = dco.phase;

    // How far back inside this sample an event at unwrapped position `p` sits.
    const auto samplesAgo = [&](double p) {
        return increment > 0.0
            ? static_cast<float>(std::clamp((unwrapped - p) / increment, 0.0, 1.0))
            : 0.0f;
    };
    const auto insideThisSample = [&](double p) {
        return p > previousPhase && p <= unwrapped;
    };

    // A note timer can outrun the sample clock: the divider bottoms out at
    // eight, so the top range reaches half a megahertz, and at the lowest host
    // rate the model accepts that is some sixty cycles inside one sample. Each
    // of them resets the ramp, works the comparator and clocks the divider, and
    // collapsing them into one wrap would hold the pulse low for whole periods
    // and drop the sub an octave. So every crossed cycle is walked. The bound
    // is a runaway guard rather than a limit that is reached: sixty-four covers
    // the fastest note the timer can be programmed for against the slowest rate
    // the engine runs at.
    constexpr int maximumWrapsPerSample = 64;
    const double lastCycle =
        std::min(std::floor(unwrapped), static_cast<double>(maximumWrapsPerSample));

    // --- Ramp -------------------------------------------------------------
    // The compensation voltage keeps the amplitude constant at 12 Vpp -- but
    // it arrives through the hold capacitor while the timer count steps
    // instantly, so every pitch step leaves a momentary amplitude error
    // until the voltage catches up. The ratio of the slewed voltage to the
    // one the current pitch calls for *is* that error.
    const float compensationScale = std::clamp(
        voice.dcoCv / std::max(voice.dcoCvTarget, 1.0e-3f), 0.25f, 4.0f);
    // From the parts' own tolerance classes, which is legitimate here because
    // the ramp has no per-voice trimmer, so nothing removes their spread: the
    // charging resistors are marked FX on the board -- metal-oxide film, 1% --
    // and the 1 nF timing capacitors carry code G, 2%. Worst case that is
    // -2.93% to +3.07% of slope, which is the 3% used here.
    const float amplitude = sawMixVolts
                          * dcoRampAmplitudeScale(voice, parameters);

    float sawNaive = phase < rise
        ? 2.0f * clamp01(static_cast<float>(phase / rise)) - 1.0f
        : 1.0f - 2.0f * static_cast<float>((phase - rise) / reset);

    if (parameters.enableExponentialReset && phase >= rise)
    {
        const float resetPhaseNorm = static_cast<float>((phase - rise) / reset);
        constexpr float expDecayRate = 4.0f; // tau = reset / 4 (~0.55 us JFET RC discharge)
        const float expFactor = (std::exp(-expDecayRate * resetPhaseNorm) - std::exp(-expDecayRate))
                              / (1.0f - std::exp(-expDecayRate));
        sawNaive = 2.0f * expFactor - 1.0f;
    }

    // The ramp reset corner slope discontinuities are repaired with slope residuals.
    const float slopeAtStart = 2.0f / static_cast<float>(rise);
    const float slopeAtEnd = slopeAtStart;
    const float fallSlopeStart = parameters.enableExponentialReset ? -8.0f / static_cast<float>(reset) : -2.0f / static_cast<float>(reset);
    const float fallSlopeEnd = parameters.enableExponentialReset ? fallSlopeStart * std::exp(-4.0f) : fallSlopeStart;
    const float incrementF = static_cast<float>(increment);

    for (double base = 0.0; base <= lastCycle; base += 1.0)
    {
        if (insideThisSample(base + rise))
            addSlope(dco.saw, (fallSlopeStart - slopeAtEnd) * incrementF,
                     samplesAgo(base + rise));
        if (insideThisSample(base + 1.0))
            addSlope(dco.saw, (slopeAtStart - fallSlopeEnd) * incrementF,
                     samplesAgo(base + 1.0));
    }

    const float sawOut = dco.saw.advance(sawNaive) * amplitude;

    // --- Pulse ------------------------------------------------------------
    // The comparator flips when the ramp crosses the threshold on the way up
    // and again when the reset drags it back down past it.  The threshold is a
    // slewing analogue hold, so those crossing positions move *during* this
    // sample.  Treating the current duty as if it had been fixed for the whole
    // interval can move an edge behind the already-advanced ramp and skip it
    // for a complete oscillator cycle -- the periodic blip heard on deep PWM.
    const float duty = std::clamp(voice.pulseDuty, 0.0f, 1.0f);
    const float previousDuty = voice.pulseDutyPrimed
        ? std::clamp(voice.previousPulseDuty, 0.0f, 1.0f) : duty;
    const double dutyDelta = static_cast<double>(duty - previousDuty);

    struct ComparatorEvent
    {
        double time { 0.0 }; // 0..1 across this internal sample
        float state { -1.0f };
    };
    std::array<ComparatorEvent, maximumWrapsPerSample * 2 + 4> events {};
    int eventCount = 0;
    const auto appendCrossing = [&events, &eventCount](double numerator,
                                                       double denominator,
                                                       float state) noexcept
    {
        if (std::abs(denominator) <= 1.0e-14)
            return;
        const double time = numerator / denominator;
        // An event at zero belongs to the preceding interval.  Admit a tiny
        // overrun at one for round-off, then clamp it onto this endpoint.
        if (time > 1.0e-12 && time <= 1.0 + 1.0e-12
            && eventCount < static_cast<int>(events.size()))
            events[static_cast<std::size_t>(eventCount++)] = {
                std::min(time, 1.0), state
            };
    };

    // With d(t)=d0+t*(d1-d0), the rising boundary is
    // base+rise*(1-d(t)) and the falling boundary is
    // base+rise+reset*d(t).  Solve each against p(t)=p0+t*increment.
    // The sign of the relative velocity says which side of the comparator the
    // crossing enters; PWM can move a boundary faster than a very low DCO.
    const double riseVelocity = increment + rise * dutyDelta;
    const double fallVelocity = increment - reset * dutyDelta;
    for (double base = 0.0; base <= lastCycle; base += 1.0)
    {
        appendCrossing(base + rise * (1.0 - previousDuty) - previousPhase,
                       riseVelocity,
                       riseVelocity > 0.0 ? 1.0f : -1.0f);
        appendCrossing(base + rise + reset * previousDuty - previousPhase,
                       fallVelocity,
                       fallVelocity > 0.0 ? -1.0f : 1.0f);
    }

    // Usually there are zero or two events.  A fixed, allocation-free
    // insertion sort also covers the bounded multi-cycle case at the timer's
    // extreme divider values and preserves rise-before-fall ordering when two
    // zero-width edges coincide.
    for (int index = 1; index < eventCount; ++index)
    {
        const auto event = events[static_cast<std::size_t>(index)];
        int insertion = index;
        while (insertion > 0
               && events[static_cast<std::size_t>(insertion - 1)].time
                    > event.time)
        {
            events[static_cast<std::size_t>(insertion)] =
                events[static_cast<std::size_t>(insertion - 1)];
            --insertion;
        }
        events[static_cast<std::size_t>(insertion)] = event;
    }

    for (int index = 0; index < eventCount; ++index)
    {
        const auto& event = events[static_cast<std::size_t>(index)];
        if (event.state == dco.pulseState)
            continue;
        addStep(dco.pulse, event.state - dco.pulseState,
                static_cast<float>(1.0 - event.time));
        dco.pulseState = event.state;
    }

    // Clamping at the 0/100% limits makes the boundary piecewise rather than
    // perfectly linear.  Reconcile the end point to the physical comparator's
    // memoryless truth; in the ordinary 5..95% range the solved events already
    // land here exactly, so this is only a numerical/pinned-state guard.
    const double riseEdge =
        static_cast<double>(pulseRisePhase(duty, static_cast<float>(reset)));
    const double fallEdge =
        static_cast<double>(pulseFallPhase(duty, static_cast<float>(reset)));
    const float comparatorAtEnd = duty >= 1.0f
        ? 1.0f
        : (duty <= 0.0f
               ? -1.0f
               : (phase >= riseEdge && phase < fallEdge ? 1.0f : -1.0f));
    if (comparatorAtEnd != dco.pulseState)
    {
        addStep(dco.pulse, comparatorAtEnd - dco.pulseState, 0.0f);
        dco.pulseState = comparatorAtEnd;
    }
    voice.previousPulseDuty = duty;
    voice.pulseDutyPrimed = true;

    // The pulse is amplitude-compensated by the same control voltage as the
    // ramp, so it carries the same momentary scale error on pitch steps.
    const float pulseOut = dco.pulse.advance(dco.pulseState)
                         * pulseMixVolts * compensationScale;

    // --- Sub --------------------------------------------------------------
    // A flip-flop halves the note clock, so the sub is an exact square one
    // octave down and takes no part in pulse-width modulation. The terminal
    // pulse that fires the ramp's discharge is also what clocks the divider,
    // so the sub's edges land at the reset's *start*, not at the cycle
    // boundary. Its level is the shared scanned SUB voltage consumed by every
    // card; its amplitude is a logic square and takes no part in the ramp's
    // compensation.
    for (double base = 0.0; base <= lastCycle; base += 1.0)
    {
        if (!insideThisSample(base + rise))
            continue;
        const float target = -dco.subState;
        addStep(dco.sub, target - dco.subState, samplesAgo(base + rise));
        dco.subState = target;
    }
    const float subGain = subMixVolts * subCv_
        * (1.0f + card.subLevelError * 0.03f * parameters.calibration);
    const float cmosAsymmetry = dco.subState > 0.0f
        ? (1.0f + 0.003f * parameters.calibration)
        : -(1.0f - 0.003f * parameters.calibration);
    const float subOut = dco.sub.advance(cmosAsymmetry) * subGain;

    // --- Summing node (Thévenin Passive Mixer Network) -----------------------
    // Saw (100k), Pulse (100k), Sub (100k), Noise (100k) into IR3109 input (68k).
    // Enabling multiple waveform switches increases node admittance, loading down
    // signal levels naturally.
    float mixed = 0.0f;
    int activeLegCount = 0;

    if (parameters.sawEnabled)
    {
        mixed += sawOut;
        ++activeLegCount;
    }
    if (pulseMixEnabled(parameters.pulseEnabled, voice.pulseDuty))
    {
        mixed += pulseOut;
        ++activeLegCount;
    }
    if (parameters.subLevel > 0.0f || subCv_ > 0.0f)
    {
        mixed += subOut;
        ++activeLegCount;
    }
    if (parameters.noiseLevel > 0.0f || noiseCv_ > 0.0f)
    {
        mixed += noiseSample * noiseMixVolts * noiseCv_
               * (1.0f + card.noiseLevelError * 0.03f * parameters.calibration);
        ++activeLegCount;
    }

    if (activeLegCount > 0 && parameters.calibration > 0.0f)
    {
        constexpr float gIn = 1.0f / 68.0f;
        constexpr float gLeg = 1.0f / 100.0f;
        constexpr float gNominal1 = gIn + gLeg;
        const float gTotal = gIn + static_cast<float>(activeLegCount) * gLeg;
        const float rawFactor = gNominal1 / gTotal;
        const float nodeLoadingFactor = 1.0f + (rawFactor - 1.0f) * parameters.calibration;
        mixed *= nodeLoadingFactor;
    }

    voice.noiseState ^= voice.noiseState << 13;
    voice.noiseState ^= voice.noiseState >> 17;
    voice.noiseState ^= voice.noiseState << 5;
    const float microscopicNoise =
        (static_cast<float>(voice.noiseState & 0xffffffu)
             * (2.0f / 16777215.0f) - 1.0f) * filterNoiseVolts;

    // --- Filter, amplifier -------------------------------------------------
    // No high-pass here. The schematic puts it on the jack board, downstream of
    // the summing amplifier, so it is one shared stage after all six voices
    // rather than a leg inside each -- see the mix.
    const float filterInput = mixed * filterInputAttenuation
                            * voice.inputCompensation
                            + microscopicNoise * noiseRateScale_;
    // Physical thermal warmup curve: V_t(T) = k * T / q from 25°C to 40°C
    const float psuThermalOffset = parameters.enableSpatialThermalGradient
        ? 4.0f * std::exp(-static_cast<float>(voice.cardIndex) / 2.5f) * parameters.calibration
        : 0.0f;
    const float tempRise = 15.0f * parameters.calibration;
    const float tempC = 25.0f + psuThermalOffset + tempRise * (1.0f - std::exp(-thermalWarmupSeconds_ / 900.0f));
    const float dynamicThermalVoltage = 0.026f * ((tempC + 273.15f) / 298.15f);
    const float dynamicHeadroom = 2.0f * dynamicThermalVoltage / stageAttenuation;
    const float filtered = voice.filter.process(filterInput, voice.filterG,
                                                voice.feedback, dynamicHeadroom,
                                                parameters.enableVcfEarlyEffect);

    if (!voice.active)
        return 0.0f;

    // Physical BA662 / uPC1252H2 control voltage feedthrough: differential pair
    // transistor V_be mismatch causes small additive CV leakage during fast envelope transients (thump).
    const float vcaCvFeedthrough = (card.vcaOffset * 0.002f + 0.0008f) * voice.vcaControl * parameters.calibration;
    const float output = (filtered + vcaCvFeedthrough) * voice.vca * voltsToSample;

    voice.energy += voiceEnergyFollower_ * (std::abs(output) - voice.energy);
    return std::isfinite(output) ? output : 0.0f;
}

// ---------------------------------------------------------------------------
// Decimation
// ---------------------------------------------------------------------------

void YouKnow106Engine::downsamplePair(HalfbandDecimator& decimator,
                                      float firstLeft, float firstRight,
                                      float secondLeft, float secondRight,
                                      float& outputLeft, float& outputRight) noexcept
{
    decimator.left[static_cast<std::size_t>(decimator.writeIndex)] = firstLeft;
    decimator.right[static_cast<std::size_t>(decimator.writeIndex)] = firstRight;
    decimator.writeIndex = (decimator.writeIndex + 1) & (halfbandRingSize - 1);
    decimator.left[static_cast<std::size_t>(decimator.writeIndex)] = secondLeft;
    decimator.right[static_cast<std::size_t>(decimator.writeIndex)] = secondRight;
    decimator.writeIndex = (decimator.writeIndex + 1) & (halfbandRingSize - 1);

    float sumLeft = 0.0f;
    float sumRight = 0.0f;
    int index = (decimator.writeIndex - 1) & (halfbandRingSize - 1);
    for (int tap = 0; tap < halfbandTaps; ++tap)
    {
        const float coefficient = halfbandKernel_[static_cast<std::size_t>(tap)];
        if (coefficient != 0.0f)
        {
            sumLeft += coefficient * decimator.left[static_cast<std::size_t>(index)];
            sumRight += coefficient * decimator.right[static_cast<std::size_t>(index)];
        }
        index = (index - 1) & (halfbandRingSize - 1);
    }

    outputLeft = sumLeft;
    outputRight = sumRight;
}

// ---------------------------------------------------------------------------
// Block processing
// ---------------------------------------------------------------------------

void YouKnow106Engine::process(float* left, float* right, int numSamples)
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    if (!prepared_)
    {
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
        return;
    }

    applyPendingOversamplingIfIdle();

    // Complete a fade and rebuild at the exact host-sample boundary even when
    // that point lies inside a large host block. Processing the two pieces
    // recursively is bounded to one split: the second piece begins at zero,
    // applies the pending rate, and fades in. Besides avoiding a long mute, it
    // lets every per-call rate-derived coefficient below be recomputed for the
    // correct side of the boundary.
    if (rateTransition_ == RateTransition::FadingOut
        && oversamplingRequested_ != oversamplingEnabled_
        && rateTransitionGain_ > 0.0f)
    {
        const int samplesUntilZero = std::max(
            1, static_cast<int>(std::ceil(
                   rateTransitionGain_ / rateTransitionStep_)));
        if (samplesUntilZero < numSamples)
        {
            process(left, right, samplesUntilZero);
            process(left + samplesUntilZero, right + samplesUntilZero,
                    numSamples - samplesUntilZero);
            return;
        }
    }

    const auto& parameters = activeParameters_;

    // One shared network, so its coefficients are settled once here rather than
    // six times inside the voice loop. It has to come after the pending
    // oversampling switch, which is what moves the rate they are prewarped
    // against.
    updateSharedHighPass(parameters);

    if (!panelGlidePrimed_)
    {
        glidedVolume_ = parameters.volume;
        panelGlidePrimed_ = true;
    }

    // Each converter destination owns a separately named hold network. Only
    // the VCF and voice-VCA values are evidence-backed; the other constants
    // remain isolated compatibility policies until their RCs are established.
    const auto slewFor = [this](float seconds) {
        return 1.0f - std::exp(-inverseOversampledRate_ / seconds);
    };
    const float vcfSlew = slewFor(vcfHoldSlewSeconds);
    const float voiceVcaSlew = slewFor(voiceVcaHoldSlewSeconds);
    const float dcoSlew = slewFor(dcoHoldSlewSecondsVoiced);
    const float resonanceSlew = slewFor(resonanceHoldSlewSecondsVoiced);
    const float commonVcaSlew = slewFor(commonVcaHoldSlewSecondsVoiced);
    const float pwmSlew = slewFor(pwmHoldSlewSecondsVoiced);
    const float subSlew = slewFor(subHoldSlewSecondsVoiced);
    const float noiseSlew = slewFor(noiseHoldSlewSecondsVoiced);
    voiceEnergyFollower_ = slewFor(voiceEnergyFollowerSeconds);
    const float outputGlide =
        1.0f - std::exp(-inverseSampleRate_ / panelGlideSeconds);
    const double scanPhasePerInternalSample = controlScanHz / oversampledRate_;
    const float outputBoundaryGain =
        outputReferenceGain(compatibilityOutputReferenceRmsVolts);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float outputLeft = 0.0f;
        float outputRight = 0.0f;
        bool sounding = false;

        // Two decimation stages at 4x, one at 2x, none at 1x. The inner loop
        // renders one oversampled frame.
        std::array<float, maximumOversampleFactor> stageLeft {};
        std::array<float, maximumOversampleFactor> stageRight {};

        for (int step = 0; step < oversampling_; ++step)
        {
            // One converter serves the whole instrument. The service chart
            // and the hash-matched B-2 code establish the complete ordinal
            // write order and show sequential activity across the pass. The
            // normalized timing profile preserves that qualitative fact while
            // leaving exact physical offsets open.
            bool converterPassCompleted = false;
            if (controlScanPhase_ >= 1.0)
            {
                controlScanPhase_ -= 1.0;
                nextConverterWrite_ = 0;
                if (assignmentRescanPending_)
                    assignmentRescanPassArmed_ = true;
                const float bendMagnitude = std::floor(
                    std::abs(pitchBendTarget_) * 255.0f + 0.5f) / 255.0f;
                pitchBend_ = pitchBendTarget_ < 0.0f ? -bendMagnitude
                                                     : bendMagnitude;
                modWheel_ =
                    std::floor(modWheelTarget_ * 127.0f + 0.5f) / 127.0f;

                advanceLfo(parameters);
                converterPassLfoGated_ = lfoValue_ * lfoDelayLevel_;

                // Slots above the six physical cards are an explicit product
                // extension. They reuse one complete logical update at the
                // pass boundary without pretending that the B-2 scans them.
                for (int slot = hardwareVoices; slot < maxVoices; ++slot)
                {
                    auto& voice = voices_[static_cast<std::size_t>(slot)];
                    const double previousPeriod = voice.dco.periodSamples;
                    updateVoiceScan(voice, parameters, converterPassLfoGated_);
                    if (voice.dcoResetPending)
                    {
                        restartDcoBandlimited(voice, previousPeriod);
                        voice.dcoResetPending = false;
                    }
                }
            }

            const auto& writes = converterWriteOrder();
            while (nextConverterWrite_ < writes.size()
                   && converterEventPhases_[nextConverterWrite_]
                          <= controlScanPhase_ + 1.0e-12)
            {
                performConverterWrite(writes[nextConverterWrite_], parameters,
                                      converterPassLfoGated_);
                ++nextConverterWrite_;
                if (nextConverterWrite_ == writes.size())
                    converterPassCompleted = true;
            }
            resonanceCv_ +=
                (resonanceCvTarget_ - resonanceCv_) * resonanceSlew;
            sharedVca_ +=
                (sharedVcaTarget_ - sharedVca_) * commonVcaSlew;
            pwmVolts_ += (pwmVoltsTarget_ - pwmVolts_) * pwmSlew;
            subCv_ += (subCvTarget_ - subCv_) * subSlew;
            noiseCv_ += (noiseCvTarget_ - noiseCv_) * noiseSlew;
            thermalWarmupSeconds_ += static_cast<float>(inverseOversampledRate_);

            if (--driftControlCountdown_ <= 0)
            {
                // A fixed wall-clock rate. Counting internal samples instead
                // would make the modelled component wander four times faster
                // with oversampling on, so the same patch would drift
                // differently depending on a quality setting.
                driftControlCountdown_ = std::max(
                    1, static_cast<int>(oversampledRate_ / driftUpdateHz));
                for (auto& card : cards_)
                    updateVoiceCardDrift(card);
            }

            // One noise generator feeds every voice, so noise grows as more
            // keys are held instead of staying put.
            noiseState_ ^= noiseState_ << 13;
            noiseState_ ^= noiseState_ >> 17;
            noiseState_ ^= noiseState_ << 5;
            const float noiseSample =
                (static_cast<float>(noiseState_ & 0xffffffu)
                     * (2.0f / 16777215.0f) - 1.0f) * noiseRateScale_;

            // Polyphonic current draw loads the +/-15 V regulators, so the
            // rails sag as more cards work. This is the DC part only. The
            // rectifier's own 100/120 Hz ripple is deliberately NOT modelled:
            // Service Notes p. 16 gives a 3300 uF reservoir per rail behind a
            // 0.25 A secondary, so the unregulated ripple is about 0.76 Vpp at
            // 50 Hz, and the M5230L regulators after it reject roughly 60 dB of
            // that. What reaches a card is on the order of 50 ppm of 15 V --
            // some 0.03 cents of cutoff shift through the transfer below. It is
            // derivably inaudible, not merely unmeasured.
            //
            // The sum is kept as a pure load measure. Unit Character scales the
            // consequence once, where the droop is applied.
            float totalVoiceEnergy = 0.0f;
            for (const auto& v : voices_)
            {
                if (v.active)
                    totalVoiceEnergy += v.energy;
            }
            powerSupplyDroop_ = totalVoiceEnergy * 0.0015f;

            float mono = 0.0f;
            float loudestEnvelope = 0.0f;

            // Every slot, not just the first `limit` of them. The voice count
            // bounds what the key assigner may take; lowering it must stop new
            // notes rather than freeze notes that are already sounding.
            for (int slot = 0; slot < maxVoices; ++slot)
            {
                auto& voice = voices_[static_cast<std::size_t>(slot)];

                // Each hold capacitor's own slew turns the scan's staircase
                // back into a continuous control voltage before it reaches
                // its converter. The amplifier's hold is the slow one.
                voice.cutoffCounts +=
                    (voice.cutoffCountsTarget - voice.cutoffCounts) * vcfSlew;
                voice.dcoCv +=
                    (voice.dcoCvTarget - voice.dcoCv) * dcoSlew;
                voice.vcaControl +=
                    (voice.vcaControlTarget - voice.vcaControl) * voiceVcaSlew;
                updatePulseComparator(voice, parameters);

                if (voice.active || slot < hardwareVoices)
                    updateVoiceAudio(voice, parameters);

                if (!voice.active)
                {
                    // The six physical DCO/filter cards remain powered behind
                    // their closed VCAs; extension slots stop inside renderVoice.
                    renderVoice(voice, parameters, noiseSample);
                    continue;
                }

                mono += renderVoice(voice, parameters, noiseSample);
                loudestEnvelope = std::max(loudestEnvelope, voice.envelope.value);

                // Retire on the voiced amplifier profile actually being shut.
                // A card's optional control offset can sit above a small raw
                // threshold yet remain inside that profile's hard-zero region;
                // without this check a silent voice would block a deferred
                // quality change forever.
                if (voice.envelope.stage == EnvelopeStage::Idle
                    && !voice.keyDown && !voice.sustained
                    && VoicedVoiceVcaCompatibilityProfile::gain(
                           voice.vcaControl) <= 0.0f)
                    silenceVoice(voice);
                else
                    sounding = true;
            }

            displayEnvelope_ = loudestEnvelope;

            // The POLY/unison handler gated and cleared at the host event.
            // Reassign only after the complete ordered pass, so every physical
            // voice CPU has observed gate-off before any replacement Note On.
            if (converterPassCompleted && assignmentRescanPending_
                && assignmentRescanPassArmed_)
                completeVoiceAssignmentRescan();
            controlScanPhase_ += scanPhasePerInternalSample;

            // One high-pass, on the summed voices. The schematic carries a
            // single set of parts for it -- on the jack board, downstream of
            // the summing amplifier -- not one set per voice, so this is where
            // it belongs. An earlier revision ran it inside each voice ahead of
            // that voice's own filter, which is a different circuit: a
            // high-pass feeding a resonant lowpass is not the same as one
            // following it, because what the high-pass removes is what the
            // resonance would otherwise have had to work on.
            const float coupled = voiceBusCoupling_.process(
                voiceBusInput(mono), voiceBusCouplingG_, 0.0f, 1.0f);
            const float shaped = highPass_.process(coupled,
                                                   highPassG_,
                                                   highPassShelf_, highPassHigh_);

            // VCA LEVEL is the one common uPC1252H2 on the jack board, after
            // the voice sum and HPF. The six voice-module VCAs above are driven
            // only by ENV/GATE (plus the optional velocity extension).
            const float vcaInput = commonVcaInputCoupling_.process(
                shaped, commonVcaInputCouplingG_, 0.0f, 1.0f);
            const float levelled = vcaInput * patchLevelGain(sharedVca_);

            // The chorus input coupling capacitors sit in its two wet branches;
            // dry bypasses them. IC6 applies its component-derived dry/wet
            // gains when they recombine. Its +/-15 V clipping point has not
            // been measured under the output load, so no invented low-voltage
            // rail is inserted here; the main volume control follows it.
            float wetLeft = levelled;
            float wetRight = levelled;
            chorus_.process(levelled, parameters.chorus, parameters.chorusNoise,
                            wetLeft, wetRight, parameters.enableBbdCapacitanceNonlinearity,
                            parameters.enableChorusThiranAndClockBleed);

            // TA75558S IC6 output summer op-amp soft saturation on +/-15V rails (~13.5V headroom)
            if (parameters.calibration > 0.0f)
            {
                constexpr float opampHeadroom = 5.19f;
                const float cal = parameters.calibration;
                const float satL = std::tanh(wetLeft / opampHeadroom) * opampHeadroom;
                const float satR = std::tanh(wetRight / opampHeadroom) * opampHeadroom;
                wetLeft = (1.0f - cal) * wetLeft + cal * satL;
                wetRight = (1.0f - cal) * wetRight + cal * satR;
            }

            // TA75558S IC6 output summer op-amp dynamic slew-rate limiting (SR = 1.7 V/us)
            if (parameters.enableOpAmpSlewLimiting)
            {
                const float maxStep = static_cast<float>(653846.15 / oversampledRate_);
                const float deltaL = wetLeft - outputSlewStateLeft_;
                outputSlewStateLeft_ += std::clamp(deltaL, -maxStep, maxStep);
                wetLeft = outputSlewStateLeft_;

                const float deltaR = wetRight - outputSlewStateRight_;
                outputSlewStateRight_ += std::clamp(deltaR, -maxStep, maxStep);
                wetRight = outputSlewStateRight_;
            }
            else
            {
                outputSlewStateLeft_ = wetLeft;
                outputSlewStateRight_ = wetRight;
            }

            stageLeft[static_cast<std::size_t>(step)] = wetLeft;
            stageRight[static_cast<std::size_t>(step)] = wetRight;
        }

        if (oversampling_ == 4)
        {
            float firstLeft = 0.0f;
            float firstRight = 0.0f;
            float secondLeft = 0.0f;
            float secondRight = 0.0f;
            downsamplePair(firstDecimator_, stageLeft[0], stageRight[0],
                           stageLeft[1], stageRight[1], firstLeft, firstRight);
            downsamplePair(firstDecimator_, stageLeft[2], stageRight[2],
                           stageLeft[3], stageRight[3], secondLeft, secondRight);
            downsamplePair(secondDecimator_, firstLeft, firstRight,
                           secondLeft, secondRight, outputLeft, outputRight);
        }
        else if (oversampling_ == 2)
        {
            downsamplePair(firstDecimator_, stageLeft[0], stageRight[0],
                           stageLeft[1], stageRight[1], outputLeft, outputRight);
        }
        else
        {
            outputLeft = stageLeft[0];
            outputRight = stageRight[0];
        }

        applyLatencyPad(outputLeft, outputRight);

        glidedVolume_ += (parameters.volume - glidedVolume_) * outputGlide;

        // C17/C20, R54/R57 and VR1 are one loaded network, not a fixed pole
        // followed by an unrelated gain. The 41.3 kOhm selector ladder and
        // 101 kOhm headphone input load each wiper at every shaft position;
        // moving Volume changes both the settled gain and the resistance seen
        // by the still-continuous capacitor state.
        outputCouplingG_ = std::tan(
            pi * outputCouplingCornerHz(glidedVolume_) * inverseSampleRate_);
        const float outputCouplingGain =
            outputCouplingHighGain(glidedVolume_);
        outputLeft = outputCouplingLeft_.process(
            outputLeft, outputCouplingG_, 0.0f, outputCouplingGain);
        outputRight = outputCouplingRight_.process(
            outputRight, outputCouplingG_, 0.0f, outputCouplingGain);

        // How long the voices have been gone, which is what a pending quality
        // change waits on: the output path needs that long to run dry.
        if (sounding)
            oversamplingIdleSamples_ = 0;
        else if (oversamplingIdleSamples_ < oversamplingQuietSamples_)
            ++oversamplingIdleSamples_;

        const float transitionGain = rateTransitionGain_;
        left[sample] = std::isfinite(outputLeft)
                     ? outputLeft * outputBoundaryGain * transitionGain
                     : 0.0f;
        right[sample] = std::isfinite(outputRight)
                      ? outputRight * outputBoundaryGain * transitionGain
                      : 0.0f;

        if (rateTransition_ == RateTransition::FadingOut)
        {
            rateTransitionGain_ = std::max(
                0.0f, rateTransitionGain_ - rateTransitionStep_);
        }
        else if (rateTransition_ == RateTransition::FadingIn)
        {
            rateTransitionGain_ = std::min(
                1.0f, rateTransitionGain_ + rateTransitionStep_);
            if (rateTransitionGain_ >= 1.0f)
                rateTransition_ = RateTransition::Idle;
        }
    }

    updateActiveVoiceCount();
}

} // namespace youknow106
