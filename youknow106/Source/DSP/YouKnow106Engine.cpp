#include "YouKnow106Engine.h"

#include <algorithm>
#include <cmath>
#include <limits>

#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#elif defined(__x86_64__) && defined(__SSE2__)
#include <emmintrin.h>
#endif

#if defined(YOUKNOW106_WORK_AUDIT)
#include "../../Tools/OversamplingAuditSupport.h"

#define YOUKNOW106_COUNT_DOMAIN_WORK(field, amount)                         \
    do                                                                      \
    {                                                                       \
        if (auto* counters =                                                \
                youknow106::oversampling_audit::activeDomainWorkCounters)   \
            counters->field += (amount);                                    \
    } while (false)
#endif

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

// Voiced source-coordinate values. Saw and pulse are constrained by the service
// anchor near 12 Vpp; intervening loading is still open, and the sub value has
// no equivalent end-to-end anchor, so none of these may be presented as
// measured mixer voltages.
constexpr float sawMixVolts = 6.0f;
constexpr float pulseMixVolts = 6.0f;
constexpr float subMixVolts = 5.0f;
// The noise coordinate names the SHAPED rail, not the raw generator, and that
// distinction is the whole of it. The service procedure adjusts VR32 for 4 Vpp
// at TP8 -- the CH1 voice VCA output -- and a +/-2 V figure written onto the
// source *ahead* of the shaping below does not arrive there as 4 Vpp, because
// the 4.82 kHz pole keeps only 7.27% of a white source's power (-11.383 dB).
// A previous revision made exactly that substitution and left the audible noise
// 11.38 dB light: referred to the model's own calibrated 4.8 Vpp
// self-oscillation it measured -23.35 dB where the paired TP8 figures put it
// between -8.5 and -12.6 dB, the spread being what crest convention a scope
// trace of random noise is read with.
//
// 2.0 / sqrt(0.0727330) = 7.4161 restores precisely what the shaping discards,
// so the same +/-2 V now describes the rail the adjustment measures. It is a
// mechanical correction of the misplacement rather than a fit, which is why it
// assumes no crest convention; it lands at -11.96 dB against self-oscillation,
// inside the anchored band at its conservative end.
//
// Raising it required the output boundary to be referred to the summer model's
// provisional asymptote first (see outputBoundaryGain). Without that, a
// six-note NOISE-10 chord peaks
// at +1.97 dBFS: one shared generator sums coherently across held voices, at
// 20*log10(N), so noise chords reach full scale far sooner than oscillator
// chords do. That is what "high Noise settings sound broken" was.
//
// What stays open is the absolute source-coordinate boundary, not the ordered
// topology below: the anchors fix the product of this constant and
// filterInputAttenuation, and only the coincidence between the deficit and the
// shaping loss says the noise leg alone was light. OQ-15/OQ-16.
constexpr float noiseMixVolts = 7.4161f;

// The noise generator's support circuit, module board p. 13: Tr21 (2SC945,
// factory-selected for noise) with R104 470 kOhm collector load, coupled by
// C42 1 uF into the BA662 level OTA whose input pin sits on a 4.7 kOhm bias
// resistor -- a 33.9 Hz high-pass -- and whose output is loaded by C41 100 pF
// against R79 330 kOhm -- a 4.82 kHz pole -- before the buffered NOISE rail.
// The audible source is therefore band-shaped by its own circuit, not flat.
// The shaping passes its passband at unity, so it does not change in-band
// density -- but it does change total power: the 4.82 kHz pole keeps only
// 6982.4 Hz of noise-equivalent bandwidth out of the 96 kHz the source is
// white across at the 192 kHz internal rate, which is 7.3% of its power, or
// -11.38 dB of RMS. That loss is why the pre-filter coordinate above cannot be
// read as the TP8 figure (OQ-16).
constexpr float noiseCouplingCapacitanceF = 1.0e-6f;      // C42
constexpr float noiseCouplingLoadOhms = 4700.0f;          // R81, IC14 input
constexpr float noiseOtaLoadCapacitanceF = 100.0e-12f;    // C41
constexpr float noiseOtaLoadResistanceOhms = 330000.0f;   // R79

// Voice-summer output coupling ahead of the four-position HPF selector:
// IC1a -> C14 10 uF NP -> IC3 common input, with R39 33 kOhm from that
// common node to ground. The selected Cut leg also leaves its mux-side
// 1 MOhm bleed connected directly to the common node.
constexpr float voiceBusCouplingCapacitanceF = 10.0e-6f;
constexpr float voiceBusCouplingResistanceOhms = 33000.0f;
constexpr float highPassCutBleedResistanceOhms = 1000000.0f; // R21 / R23

// Per-voice module-input coupling, module board p. 13: the summed WAVE node
// reaches the voice module's pin 1 VCF IN only through C56/C50 10 uF NP. The
// topology is settled -- the 2026-08-07 designator read lists this capacitor
// with the rest of the mixer node -- and it is why no mixer DC can reach the
// filter core or the voice VCA behind it.
//
// The capacitor is the read part; the resistance it works against is not.
// R99/R102 33 kOhm is *not* this pole's load -- the 2026-08-20 p. 13 junction
// read re-roles it as the sub switch transistor's collector load returning to
// the shared SUB LEVEL rail, retiring the 2026-08-07 "bridge across the diode"
// reading, so the sub's conducting path reaches the WAVE line through 60 kOhm
// (R101/R97 27 kOhm behind D6/D5, then this 33 kOhm) -- and the node's
// termination, together with the WAVE output's
// source impedance, is exactly OQ-15's remaining measurement. 33 kOhm is
// therefore a voiced stand-in, taken by analogy with the two settled
// 10 uF NP / 33 kOhm couplings downstream (C14/R39 and C12/R36), and what
// this pole does is insensitive to the choice: every plausible 10-100 kOhm
// termination lands the corner between 0.16 and 1.6 Hz, far below the lowest
// note either way. The audible content is the DC block itself, not the corner.
constexpr float moduleCouplingCapacitanceF = 10.0e-6f;      // C56 / C50
constexpr float moduleCouplingResistanceOhms = 33000.0f;    // voiced, OQ-15

// Per-voice coupling out of the filter and into the amplifier, module board
// pp. 18-19: pin 3 VCF OUT reaches pin 9 VCA IN only through C59 1 uF/50 V NP
// and the VR27/R108 network. The capacitor is anchored -- it is in the voice
// module's VCA row of the research contract and in OQ-19's own topology
// listing -- and Roland's service procedure trims VR30/25/20/15/10/5 through
// R112 2.2 MOhm at this same node for minimum thump, which is the factory
// saying in a procedure that pin 9 is meant to sit at zero.
//
// As with C56/C50 above, the capacitor is the read part and the resistance it
// works against is not: neither R108 nor VR27's setting is in tree, so the
// load is voiced and bracketed rather than claimed. 33 kOhm gives 4.82 Hz and
// 100 kOhm gives 1.59 Hz; both are far below the lowest note the instrument
// plays, so what this pole does -- block the DC the pulse comparator's duty
// asymmetry leaves on the filter output, before the envelope multiplies it --
// is insensitive to the choice inside the bracket. 33 kOhm is taken by the
// same analogy the module input uses, with the settled 33 kOhm loads
// downstream (C14/R39, C12/R36).
constexpr float vcaInputCouplingCapacitanceF = 1.0e-6f;     // C59
constexpr float vcaInputCouplingResistanceOhms = 33000.0f;  // voiced, OQ-19

// Manufacturer application input for IC5/uPC1252H2, populated by Roland as
// C12 10 uF NP followed by R36 33 kOhm.
constexpr float commonVcaInputCapacitanceF = 10.0e-6f;
constexpr float commonVcaInputResistanceOhms = 33000.0f;

// Stored VCA LEVEL control path on the jack board. Roland's p. 8 converter
// chart gives the +4..-6 V buffer span and the firmware stores byte b as the
// physical 12-bit code b<<5. Page 15 then shows R30/C7 at the held node, R32
// into IC5 GC1, R31 to ground and R165 to +15 V. The DAC uses the usual ideal
// 4096-step R-2R convention; the largest reachable stored code is 4064.
constexpr float commonVcaDacReferenceVolts = 5.0f;
constexpr float commonVcaDacSteps = 4096.0f;
constexpr float commonVcaMaximumDacCode = 4064.0f;
constexpr float commonVcaBufferOffsetVolts = 4.0f;
constexpr float commonVcaBufferGain = -2.0f;
constexpr float commonVcaR30Ohms = 2200.0f;
constexpr float commonVcaR32Ohms = 1500.0f;
constexpr float commonVcaR31Ohms = 47.0f;
constexpr float commonVcaR165Ohms = 15000.0f;
constexpr float commonVcaBiasVolts = 15.0f;
constexpr float commonVcaC7Farads = 10.0e-6f;
constexpr float commonVcaControlVoltsPerDecibel = -5.9e-3f;

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

// The 0-1 fraction a converter destination's stored panel value maps to at
// the physical DAC, shared by updateSharedScan, performConverterWrite and
// passiveHoldWriteTarget alike (RESONANCE, common VCA, SUB and NOISE all read
// this same conversion) rather than each of the three carrying its own
// identical copy of the lambda. PWM has B-2's separate doubled-byte product.
float converterDacFraction(float value) noexcept
{
    return static_cast<float>(YouKnow106Engine::storedControlDacCode(value))
        / 4064.0f;
}

std::uint8_t controlAdcByte(float value) noexcept
{
    return static_cast<std::uint8_t>(
        std::floor(clamp01(sanitised(value, 0.0f)) * 255.0f + 0.5f));
}

std::int16_t dcoBendCommand(float normalised) noexcept
{
    // The assigner left-aligns the fourteen-bit MIDI word and sends its high
    // byte to B-2 as two one-sided magnitudes. Recombining those bytes gives
    // signed -127..127 with high-byte values 127 and 128 both at rest.
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic1.txt#L1068-L1087
    const double bounded = std::clamp(
        static_cast<double>(sanitised(normalised, 0.0f)), -1.0, 1.0);
    const long wheel = std::clamp(
        std::lround(8192.0 + bounded * 8192.0), 0L, 16383L);
    const int high = static_cast<int>(wheel) >> 6;
    return static_cast<std::int16_t>(
        high >= 128 ? high - 128 : -(127 - high));
}

std::int32_t dcoBendWordForCommand(std::int16_t command,
                                   std::uint8_t sensitivity) noexcept
{
    // B-2 multiplies the one-sided bend byte by the DCO sensitivity ADC, then
    // forms 0.75*product plus its high byte before extracting the 8.8 pitch
    // word. Spell the individual truncating shifts exactly; replacing them by
    // a floating 1.75 scale changes low-level and endpoint codes.
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L1006-L1059
    const std::uint32_t magnitude = command == 0
        ? 0u : static_cast<std::uint32_t>(2 * std::abs(static_cast<int>(command)) + 1);
    const std::uint32_t product = magnitude * sensitivity;
    const auto word = static_cast<std::int32_t>(
        ((product >> 1u) + (product >> 2u) + (product >> 8u)) >> 4u);
    return command < 0 ? -word : word;
}

// The single-pole RC corner frequency a capacitor in series with a resistance
// to ground gives, shared by every coupling/loading law below that reduces to
// exactly that network: voice-bus, module, VCA-input and common-VCA-input
// coupling, both noise support poles, and both output-coupling laws each
// carried their own copy of the identical 1/(2*pi*R*C) before this helper.
// highPassCornerHz's four-leg switched network stays its own derivation --
// it selects between four distinct branches, not one fixed R and C.
float rcCornerHz(float capacitanceF, float resistanceOhms) noexcept
{
    return 1.0f / (twoPi * capacitanceF * resistanceOhms);
}

// The loaded lower-track resistance and the total pole resistance the output
// coupling network presents at a given VOLUME shaft position. Both the
// loaded corner frequency and the loaded passthrough gain below are read off
// this same wiper network, so it is solved once here rather than carrying
// two independently maintained copies of the identical track-loading algebra.
struct OutputCouplingWiperNetwork
{
    float loadedLower;
    float resistance;
};

OutputCouplingWiperNetwork outputCouplingWiperNetworkFor(
    float volumePosition) noexcept
{
    const float position = clamp01(sanitised(volumePosition, 0.0f));
    const float lowerTrack = position * outputCouplingPotOhms;
    const float loadedLower = lowerTrack > 0.0f
        ? lowerTrack * outputWiperInternalLoadOhms
            / (lowerTrack + outputWiperInternalLoadOhms)
        : 0.0f;
    const float upperTrack = (1.0f - position) * outputCouplingPotOhms;
    return { loadedLower, outputCouplingSeriesOhms + upperTrack + loadedLower };
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

namespace
{
// The hash-matched B-2 image stores two proprietary 104-word pitch tables.
// Keep the recovered clamp/interpolation mechanism exact without putting
// either table (or a reversible residual dump) in this project. These compact
// smooth generators were fitted against the semantic words at 0x0f30 and
// 0x0e60:
// https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L1845-L1883
//
// Across all 104 anchors the divider generator is within four counts (56 are
// exact), and after the firmware interpolation it stays within 1.272 cents at
// every valid 8.8 coordinate. The CV generator is the intended 32-code/octave
// exponential, saturates at 4095, and differs from the recovered image by at
// most one code. These anchors are therefore derived, not ROM-resolved; the
// surrounding unsigned clamp and truncating interpolation below are exact.
std::uint32_t derivedDcoDividerAnchor(int index) noexcept
{
    const double octave = std::exp2(-static_cast<double>(index) / 12.0);
    const double value = -0.25 + 61382.5 * octave
                       + 169.0 * octave * octave;
    return static_cast<std::uint32_t>(std::floor(value + 0.5));
}

std::uint16_t derivedDcoCvAnchor(int index) noexcept
{
    const double value = 32.0 * std::exp2(
        static_cast<double>(index) / 12.0);
    return static_cast<std::uint16_t>(std::min(
        4095.0, std::floor(value + 0.5)));
}

std::uint16_t aggregatePitchWord(double midiNote,
                                 std::int32_t controlOffset = 0) noexcept
{
    // B-2 starts the shared master coordinate at 0x1818, then adds the
    // per-voice 8.8 portamento word. The engine's portamento state already
    // advances on that 1/256-semitone grid. Exact integer control terms can be
    // added independently; any remaining continuous controls are rounded only
    // once when this final host adapter produces the word.
    constexpr double b2MasterPitchWord = 0x1818;
    const double units = b2MasterPitchWord + midiNote * 256.0
                       + static_cast<double>(controlOffset);
    const double bounded = std::clamp(units, 0.0, 65535.0);
    return static_cast<std::uint16_t>(std::floor(bounded + 0.5));
}
} // namespace

YouKnow106Engine::DcoPitchPair YouKnow106Engine::dcoPitchPair(
    std::uint16_t pitchWord) noexcept
{
    // B-2 uses h=48..150 as table indices 0..102. Entry 103 is lookahead
    // only; both clamps discard the fractional byte. The two products are
    // unsigned and divide by 256 with truncation, exactly as the paired MUL
    // sequences at 0x0440..0x0489 do.
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L682-L780
    const int high = pitchWord >> 8u;
    int index = 0;
    std::uint32_t fraction = pitchWord & 0xffu;
    if (high <= 0x2f)
        fraction = 0u;
    else if (high >= 0x97)
    {
        index = 0x66;
        fraction = 0u;
    }
    else
        index = high - 0x30;

    const std::uint32_t divider = derivedDcoDividerAnchor(index);
    const std::uint32_t nextDivider = derivedDcoDividerAnchor(index + 1);
    const std::uint32_t cvCode = derivedDcoCvAnchor(index);
    const std::uint32_t nextCvCode = derivedDcoCvAnchor(index + 1);
    return {
        divider - fraction * (divider - nextDivider) / 256u,
        static_cast<std::uint16_t>(
            cvCode + fraction * (nextCvCode - cvCode) / 256u)
    };
}

std::int16_t YouKnow106Engine::masterTunePitchWordOffset(
    double cents) noexcept
{
    // IC29 subtracts 128 from the processed Tune ADC byte. The plug-in keeps
    // its declared continuous +/-50-cent host parameter and maps it onto that
    // signed-byte grid; clamping before lround also keeps hostile finite input
    // away from the integral conversion's undefined overflow range.
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L1107-L1119
    const double bounded = std::isfinite(cents)
        ? std::clamp(cents, -50.0, 50.0) : 0.0;
    const long units = std::lround(bounded * 2.56);
    return static_cast<std::int16_t>(std::clamp(units, -128L, 127L));
}

std::int32_t YouKnow106Engine::dcoPitchBendWordOffset(
    float normalisedBipolar, float depth) noexcept
{
    return dcoBendWordForCommand(
        dcoBendCommand(normalisedBipolar), controlAdcByte(depth));
}

std::uint8_t YouKnow106Engine::dcoLfoDepthScale(
    std::uint8_t storedDepth) noexcept
{
    // Exact compact form of B-2's 128-byte DCO-LFO depth table at 0x0a80.
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L1765-L1773
    const int depth = std::min<int>(storedDepth, 127);
    if (depth < 3)
        return 0u;
    if (depth < 65)
        return static_cast<std::uint8_t>(depth - 2);
    if (depth < 96)
        return static_cast<std::uint8_t>(2 * depth - 66);
    if (depth < 125)
        return static_cast<std::uint8_t>(4 * depth - 256);
    if (depth == 125)
        return 248u;
    return 255u;
}

std::int32_t YouKnow106Engine::dcoLfoPitchWordOffset(
    std::uint16_t accumulator, bool positivePolarity,
    std::uint8_t delayByte, std::uint8_t storedDepth,
    std::uint8_t modWheel, std::uint8_t benderSensitivity) noexcept
{
    // The two high-byte products and saturating add at 0x032d..0x0338 produce
    // one depth byte. The 16x8 multiply at 0x033b..0x034f then divides by
    // eight, which is (accumulator * depth) >> 11 in pitch-word units.
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L541-L560
    const std::uint32_t panel =
        (static_cast<std::uint32_t>(dcoLfoDepthScale(storedDepth))
         * delayByte) >> 8u;
    const std::uint32_t controller =
        (2u * std::min<std::uint32_t>(modWheel, 127u)
         * benderSensitivity) >> 8u;
    const std::uint32_t depth = std::min(255u, panel + controller);
    const std::uint32_t word =
        (std::min<std::uint32_t>(accumulator, 0x1fffu) * depth) >> 11u;
    return positivePolarity ? static_cast<std::int32_t>(word)
                            : -static_cast<std::int32_t>(word);
}

std::uint32_t YouKnow106Engine::dcoDivider(double frequencyHz) noexcept
{
    if (!(frequencyHz > 0.0) || !std::isfinite(frequencyHz))
        return maximumDivider;

    const double midiNote = 69.0 + 12.0 * std::log2(frequencyHz / 440.0);
    return dcoPitchPair(aggregatePitchWord(midiNote)).divider;
}

double YouKnow106Engine::dcoQuantisedFrequency(std::uint32_t divider,
                                               DcoRange range) noexcept
{
    const std::uint32_t limited = std::clamp(divider, minimumDivider, maximumDivider);
    return rangeClockHz(range) / static_cast<double>(limited);
}

namespace
{
// The anti-log converter's own transfer, with no endpoint policy on it. The
// transconductor's control-current saturation is what actually bends the top
// of this law, and it has to see the unclamped value or the product cap would
// be limiting the input to the physics instead of the result of it.
float vcfAntilogHz(float counts) noexcept
{
    const float safe = std::clamp(sanitised(counts, 0.0f), -2000.0f, 20000.0f);
    return YouKnow106Engine::vcfBaseFrequencyHz
         * std::exp2(safe / YouKnow106Engine::vcfCountsPerOctave);
}
} // namespace

float YouKnow106Engine::vcfCutoffHz(float counts) noexcept
{
    // The digital sum upstream is already clamped to the 14-bit accumulator;
    // the margin here only covers analogue trim and drift. This is the
    // converter's law alone -- what the transconductor can still follow is
    // vcfEffectiveCutoffHz -- with the published 50 kHz endpoint as a
    // transparent product cap.
    return std::clamp(vcfAntilogHz(counts), 1.0f, vcfSafetyCapHz);
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

namespace
{
// ------------------------------------------------------------------------
// The cascade's own limit cycle, solved by harmonic balance.
//
// A compressive nonlinearity inside an integrator lowers that integrator's
// pole in proportion to its first-harmonic gain. For a sinusoid of amplitude
// A driving tanh(v / H) that gain is the classical sinusoidal-input
// describing function
//
//     N(a) = (2 / (pi a)) * integral_0^pi tanh(a sin t) sin t dt,   a = A / H,
//
// which is 1 - a^2/4 + O(a^4) at small a (int sin^2 = pi/2, int sin^4 =
// 3pi/8) and falls to 4/(pi a) once the tanh is square. It is identically 1
// at a = 0, and that is the property the frequency correction is built on:
// with no limit cycle there is no droop to correct.
//
// The cascade carries two different nonlinearities on two different
// headrooms, and both are needed. The four stage pairs compress on
// `otaHeadroomVolts` = 2 Vt / stageAttenuation and set the *frequency*; the
// resonance return compresses on `loopHeadroomVolts` = 2 Vt * 67.7 and sets
// the *amplitude*. Referring one node to the other's headroom is what makes
// a single-node account of this miss: at the service anchor the fourth
// stage's drive is a = 0.35 and supplies 64 cents on its own, while the
// first stage's is a = 1.17, because a four-pole loop oscillating at its own
// corner carries sqrt(2) more amplitude at every step back towards the
// input. All four have to be carried.
//
// Harmonic balance, with the loop running at its own corner scaled by each
// stage's gain N_n and D = w_osc / w_corner:
//
//   phase:      sum_n atan(D / N_n) = pi
//   amplitude:  k * N_fb(A / Hfb) * prod_n 1 / sqrt(1 + (D / N_n)^2) = 1
//   drive:      a_n = |V_n| * (D / N_n) / H, with |V_n| walking back from
//               the output by sqrt(1 + (D / N_{n+1})^2) per stage
//
// At A = 0 every N_n is 1, the phase condition gives D = tan(pi/4) = 1 and
// the amplitude condition gives k = 4: the threshold is
// `nominalOscillationFeedback` exactly, and it is not a fitted number.
//
// The solve is parameterised by amplitude rather than by loop gain, which
// removes the outer root-find: every A determines the loop that sustains it.
// Sweeping A and inverting the resulting monotone map gives the correction
// on a uniform grid in loop gain, once, at first use.
// ------------------------------------------------------------------------
constexpr double piHigh = 3.14159265358979323846;
constexpr double describingCeiling = 8.0;
constexpr int describingSteps = 512;

// Composite Simpson over the integrand's own quarter period; it is smooth and
// bounded, so 64 panels hold it far inside the interpolation error of the
// table it fills.
double describingIntegral(double a) noexcept
{
    constexpr int panels = 64;
    const double width = 0.5 * piHigh / static_cast<double>(panels);
    const auto sample = [a](double t) {
        const double s = std::sin(t);
        return std::tanh(a * s) * s;
    };
    double sum = 0.0;
    for (int panel = 0; panel < panels; ++panel)
    {
        const double left = width * static_cast<double>(panel);
        sum += width / 6.0
             * (sample(left) + 4.0 * sample(left + 0.5 * width)
                + sample(left + width));
    }
    return 2.0 * sum;
}

// N(a), tabulated on [0, 8] and continued by its own 1/a asymptote above it.
const std::array<double, describingSteps + 1> describingTable = [] {
    std::array<double, describingSteps + 1> values {};
    values[0] = 1.0;
    for (int index = 1; index <= describingSteps; ++index)
    {
        const double argument = describingCeiling
            * static_cast<double>(index) / static_cast<double>(describingSteps);
        values[static_cast<std::size_t>(index)] =
            2.0 / (piHigh * argument) * describingIntegral(argument);
    }
    return values;
}();

double describingGain(double a) noexcept
{
    const double magnitude = std::abs(a);
    if (magnitude >= describingCeiling)
        return describingTable[describingSteps] * describingCeiling / magnitude;
    const double position = magnitude / describingCeiling
                          * static_cast<double>(describingSteps);
    const auto lower = static_cast<std::size_t>(position);
    const double fraction = position - static_cast<double>(lower);
    return describingTable[lower] * (1.0 - fraction)
         + describingTable[lower + 1] * fraction;
}

// The oscillation frequency the phase condition puts on a cascade whose four
// stages have been slowed to N_n of their control corner. Newton on a sum of
// arctangents, seeded at the caller's previous answer.
double oscillationRatio(const std::array<double, 4>& gains, double seed) noexcept
{
    double ratio = std::max(seed, 1.0e-6);
    for (int iteration = 0; iteration < 24; ++iteration)
    {
        double phase = 0.0;
        double slope = 0.0;
        for (const double gain : gains)
        {
            const double x = ratio / gain;
            phase += std::atan(x);
            slope += (1.0 / gain) / (1.0 + x * x);
        }
        const double step = (piHigh - phase) / slope;
        ratio = std::max(1.0e-6, ratio + step);
        if (std::abs(step) < 1.0e-13 * ratio)
            break;
    }
    return ratio;
}

struct LimitCycle
{
    double loopGain;
    double droop;
};

// The loop that sustains a limit cycle of the given output amplitude, and the
// factor by which that limit cycle drags its own corner down. `gains` carries
// the previous amplitude's solution in and this one's out, so the sweep that
// builds the table converges in two or three passes instead of a dozen.
LimitCycle limitCycleFor(double amplitude, double stageHeadroom,
                         double returnHeadroom,
                         std::array<double, 4>& gains) noexcept
{
    std::array<double, 4> ratios { 1.0, 1.0, 1.0, 1.0 };
    double droop = 1.0;
    for (int iteration = 0; iteration < 32; ++iteration)
    {
        droop = oscillationRatio(gains, droop);
        for (std::size_t stage = 0; stage < 4; ++stage)
            ratios[stage] = droop / gains[stage];

        std::array<double, 4> nodes {};
        nodes[3] = amplitude;
        for (int stage = 2; stage >= 0; --stage)
        {
            const auto index = static_cast<std::size_t>(stage);
            nodes[index] = nodes[index + 1]
                * std::sqrt(1.0 + ratios[index + 1] * ratios[index + 1]);
        }

        double moved = 0.0;
        for (std::size_t stage = 0; stage < 4; ++stage)
        {
            const double next =
                describingGain(nodes[stage] * ratios[stage] / stageHeadroom);
            moved = std::max(moved, std::abs(next - gains[stage]));
            gains[stage] = next;
        }
        if (moved < 1.0e-11)
            break;
    }

    double loss = describingGain(amplitude / returnHeadroom);
    for (const double ratio : ratios)
        loss /= std::sqrt(1.0 + ratio * ratio);
    return LimitCycle { 1.0 / loss, droop };
}
} // namespace

float YouKnow106Engine::VoicedResonanceCompatibilityProfile::frequencyTrim(
    float feedback) noexcept
{
    // The correction that puts the oscillation back on the control law is the
    // reciprocal of the droop the limit cycle imposes on the corner. Built
    // once by sweeping the limit-cycle amplitude and resampling the resulting
    // loop gain onto a uniform grid from the oscillation threshold to the
    // clamp this function already carried. Parameterising the sweep by
    // amplitude rather than by loop gain is what keeps it cheap: every
    // amplitude determines the loop that sustains it outright, so there is no
    // outer root-find. `prepare` warms this so no audio callback pays for it.
    constexpr int trimSteps = 128;
    constexpr float trimCeiling = 8.0f;
    static const std::array<float, trimSteps + 1> table = [] {
        constexpr int sweep = 1024;
        constexpr double amplitudeCeiling = 12.0;
        std::array<float, trimSteps + 1> values {};
        values[0] = 1.0f;
        const double step = (static_cast<double>(trimCeiling)
                             - static_cast<double>(nominalOscillationFeedback))
                          / static_cast<double>(trimSteps);
        int filled = 0;
        LimitCycle previous { static_cast<double>(nominalOscillationFeedback), 1.0 };
        std::array<double, 4> gains { 1.0, 1.0, 1.0, 1.0 };
        for (int index = 1; index <= sweep && filled < trimSteps; ++index)
        {
            const double amplitude = amplitudeCeiling
                * static_cast<double>(index) / static_cast<double>(sweep);
            const LimitCycle current = limitCycleFor(
                amplitude, static_cast<double>(otaHeadroomVolts),
                static_cast<double>(loopHeadroomVolts), gains);
            while (filled < trimSteps)
            {
                const double wanted =
                    static_cast<double>(nominalOscillationFeedback)
                    + step * static_cast<double>(filled + 1);
                if (wanted > current.loopGain)
                    break;
                const double span = current.loopGain - previous.loopGain;
                const double blend = span > 0.0
                    ? (wanted - previous.loopGain) / span : 0.0;
                const double droop = previous.droop
                    + blend * (current.droop - previous.droop);
                values[static_cast<std::size_t>(++filled)] =
                    static_cast<float>(1.0 / droop);
            }
            previous = current;
        }
        // The sweep covers the clamp with room to spare; hold the last solved
        // value if a future headroom ever shortens it.
        for (int index = filled + 1; index <= trimSteps; ++index)
            values[static_cast<std::size_t>(index)] =
                values[static_cast<std::size_t>(index - 1)];
        return values;
    }();

    const float k = std::clamp(sanitised(feedback, 0.0f), 0.0f, trimCeiling);
    if (k <= nominalOscillationFeedback)
        return 1.0f;
    const float position = (k - nominalOscillationFeedback)
        / (trimCeiling - nominalOscillationFeedback)
        * static_cast<float>(trimSteps);
    const auto lower = static_cast<std::size_t>(position);
    if (lower >= trimSteps)
        return table[trimSteps];
    const float fraction = position - static_cast<float>(lower);
    return table[lower] * (1.0f - fraction) + table[lower + 1] * fraction;
}

float YouKnow106Engine::vcfConverterCarryCounts(float counts) noexcept
{
    // Cents to counts: 1143 counts is an octave and 1200 cents is an octave.
    constexpr float perCent = vcfCountsPerOctave / 1200.0f;
    // Cumulative excess step at each of the three top bit boundaries. The
    // converter takes the top twelve bits, so a DAC code is four counts.
    float carry = 0.0f;
    if (counts >= 4096.0f)   // DAC 1024
        carry += -4.64f * perCent;
    if (counts >= 8192.0f)   // DAC 2048 -- the major carry
        carry += 23.31f * perCent;
    if (counts >= 12288.0f)  // DAC 3072
        carry += -4.48f * perCent;
    return carry;
}

float YouKnow106Engine::vcfEffectiveCutoffHz(float counts,
                                             float feedback) noexcept
{
    const float rawHz = vcfAntilogHz(counts)
                      * VoicedResonanceCompatibilityProfile::frequencyTrim(feedback);
    // The transconductor's control current saturates internally, so the pole
    // stops following the anti-log converter near the top of the slider. The
    // generalized algebraic clip keeps the law numerically exact through the
    // musical range -- under five cents of correction below 2.7 kHz -- and
    // bends it only as the current approaches its own limit.
    const double normalised = static_cast<double>(rawHz)
                            / static_cast<double>(vcfControlSaturationHz);
    const double exponent = static_cast<double>(vcfControlSaturationExponent);
    const double saturated = static_cast<double>(rawHz)
        / algebraicSoftClipDenominator(normalised, exponent);
    return std::min(vcfSafetyCapHz, static_cast<float>(saturated));
}

float YouKnow106Engine::chassisGradientCelsius(int cardIndex) noexcept
{
    // A per-card constant, read once per voice per internal sample. The
    // exponential belongs in a table, not in the audio path.
    static const std::array<float, maxVoices> profile = [] {
        std::array<float, maxVoices> values {};
        for (int card = 0; card < maxVoices; ++card)
            values[static_cast<std::size_t>(card)] = chassisGradientPeakCelsius
                * std::exp(-static_cast<float>(card) / chassisGradientCards);
        return values;
    }();

    const int index = std::max(cardIndex, 0);
    if (index < maxVoices)
        return profile[static_cast<std::size_t>(index)];
    return chassisGradientPeakCelsius
         * std::exp(-static_cast<float>(index) / chassisGradientCards);
}

float YouKnow106Engine::chassisGradientMeanCelsius() noexcept
{
    // A constant, but not one the language will fold: std::exp is not
    // constexpr. Thermal-scale refreshes read it for every card, so compute it
    // once rather than repeating the same six exponentials per refresh.
    static const float mean = [] {
        float total = 0.0f;
        for (int card = 0; card < hardwareVoices; ++card)
            total += chassisGradientCelsius(card);
        return total / static_cast<float>(hardwareVoices);
    }();
    return mean;
}

float YouKnow106Engine::boundedThermalFilterOmegaStep(
    float baseOmegaStep, const EngineParameters& parameters,
    int cardIndex) noexcept
{
    const double spread = parameters.enableSpatialThermalGradient
        ? 1.0 + static_cast<double>(vcfCutoffTempcoPerCelsius)
            * static_cast<double>(parameters.calibration)
            * (static_cast<double>(chassisGradientCelsius(cardIndex))
               - static_cast<double>(chassisGradientMeanCelsius()))
        : 1.0;
    return static_cast<float>(OtaCascade::clampOmegaStep(
        static_cast<double>(baseOmegaStep) * spread));
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
    const auto raw = controlAdcByte(panelPosition);
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
    // Firmware completion time: the pass that crosses the silent-hold
    // threshold also performs the fade's first add, so those two stages share
    // one pass rather than being concatenated end to end.
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L577-L607
    const int holdIncrement = envelopeAttackIncrement(panelPosition);
    const int fadeIncrement = lfoDelayFadeIncrement(panelPosition);
    const int holdPasses = (16384 + holdIncrement - 1) / holdIncrement;
    const int fadePasses = (65536 + fadeIncrement - 1) / fadeIncrement;
    return static_cast<float>((holdPasses + fadePasses - 1) / controlScanHz);
}

namespace
{
// Bender-board PORTAMENTO network, Service Notes p. 16 (2026-08-20 read):
// VR2 50KB linear track across +5 V, wiper through SW1 into R16 47 kOhm to
// ground at the slave ADC node. Both mapping directions share these values.
constexpr float portamentoTrackOhms = 50000.0f;
constexpr float portamentoLoadOhms = 47000.0f;
} // namespace

float YouKnow106Engine::portamentoTravelAdcFraction(float travel) noexcept
{
    // With lower-section resistance x*R_T loaded by R_L, the wiper divider
    // solves to x*R_L / (R_L + x*(1-x)*R_T): exact 0 and 1 at the track
    // ends, 0.39496 at half travel.
    const float x = clamp01(sanitised(travel, 0.0f));
    return x * portamentoLoadOhms
         / (portamentoLoadOhms + x * (1.0f - x) * portamentoTrackOhms);
}

float YouKnow106Engine::portamentoTravelForAdcFraction(float fraction) noexcept
{
    // The forward law rearranges to the quadratic
    // f*R_T*x^2 + (R_L - f*R_T)*x - f*R_L = 0, whose positive root inverts
    // it exactly; f = 0 short-circuits the division at a = 0.
    const float f = clamp01(sanitised(fraction, 0.0f));
    if (f == 0.0f)
        return 0.0f;
    const float a = f * portamentoTrackOhms;
    const float b = portamentoLoadOhms - f * portamentoTrackOhms;
    const float c = -f * portamentoLoadOhms;
    const float discriminant = b * b - 4.0f * a * c;
    const float root = (-b + std::sqrt(std::max(discriminant, 0.0f)))
                     / (2.0f * a);
    return clamp01(root);
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

float YouKnow106Engine::outputSummerClip(float value) noexcept
{
    static_assert(outputSummerClipExponent == 8.0f);
    if (!std::isfinite(value))
        return 0.0f;

    constexpr float asymptoteUnits =
        outputSummerSwingAsymptoteVolts / internalVoltsPerUnit;
    // The exponent is fixed at eight, so multiplies and three square roots
    // preserve the same algebraic curve without two general-purpose powers.
    // Double keeps even FLT_MAX's normalized eighth power finite, letting an
    // extreme input approach the asymptote instead of folding back to zero.
    const double normalised = std::abs(static_cast<double>(value))
                            / static_cast<double>(asymptoteUnits);
    const double squared = normalised * normalised;
    const double fourth = squared * squared;
    const double eighth = fourth * fourth;
    const double denominator = std::sqrt(std::sqrt(std::sqrt(1.0 + eighth)));
    return static_cast<float>(static_cast<double>(value) / denominator);
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

    if (profile == ConverterTimingProfile::MeasuredChartGeometry)
    {
        // The Service Notes p. 8 "D/A & S/H TIMING CHART", measured from the
        // 400 dpi print on 2026-08-20: stroke-center x coordinates of the 24
        // slot boundaries, in page pixels, with the 4.2 ms arrow spanning the
        // leading NOISE write (x 4165.5) to the trailing one (x 6052), i.e.
        // 1886.5 px per pass. The chart is NOISE-first; this queue starts at
        // RESONANCE (x 4216), so each phase is (x - 4216) / 1886.5 with the
        // pass-closing NOISE write taken from the next pass's leading stroke.
        // Boundary uncertainty is +/-3 px (+/-7 us); the widths quantize onto
        // a 10:7:5 drafting grid, so these are the figure's deliberate
        // proportions, not calibrated hardware timestamps.
        constexpr double resonanceStrokePixel = 4216.0;
        constexpr double passSpanPixels = 1886.5;
        constexpr std::array<double, converterWritesPerPass> strokePixels {
            4216.0,  // RESONANCE
            4313.5,  // VCA LEVEL
            4412.5,  // SUB
            4515.5, 4616.5, 4718.5, 4819.5, 4920.0, 5021.0, // DCO CV CH1-6
            5121.0,  // PWM
            5172.0, 5240.5,  // VCF1 / VCA1
            5309.0, 5379.0,  // VCF2 / VCA2
            5450.5, 5521.5,  // VCF3 / VCA3
            5594.0, 5664.5,  // VCF4 / VCA4
            5735.5, 5803.0,  // VCF5 / VCA5
            5877.0, 5982.0,  // VCF6 / VCA6
            6052.0   // NOISE (the chart's next-pass leading stroke)
        };
        for (std::size_t ordinal = 0; ordinal < phases.size(); ++ordinal)
            phases[ordinal] = (strokePixels[ordinal] - resonanceStrokePixel)
                            / passSpanPixels;
        return phases;
    }

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

namespace
{
// Several panel controls have a monotonic law, mapping 0..maxByte hardware
// travel to a realised time or rate, with no closed-form inverse. This
// brute-force nearest-byte search is shared by all of them: it matches in log
// space, proportionally, which is what makes a control's inverse agree with
// its own displayed value at every byte rather than only at the two ends of
// its range. `law` and the loop bounds/divisor still vary per control, so the
// search itself is the only part pulled out.
template <typename Law>
float nearestBytePositionByLogRatio(int minByte, int maxByte, float divisor,
                                    float target, Law law) noexcept
{
    int bestByte = minByte;
    float bestError = std::numeric_limits<float>::infinity();
    for (int byte = minByte; byte <= maxByte; ++byte)
    {
        const float realised = law(static_cast<float>(byte) / divisor);
        const float error = std::abs(std::log(realised / target));
        if (error < bestError)
        {
            bestError = error;
            bestByte = byte;
        }
    }
    return static_cast<float>(bestByte) / divisor;
}
} // namespace

float YouKnow106Engine::panelPositionForAttack(float seconds) noexcept
{
    if (!(seconds > 0.0f) || !std::isfinite(seconds))
        return 0.0f;
    if (seconds <= envelopeAttackSeconds(0.0f))
        return 0.0f;
    if (seconds >= envelopeAttackSeconds(1.0f))
        return 1.0f;

    return nearestBytePositionByLogRatio(0, 127, 127.0f, seconds,
                                         envelopeAttackSeconds);
}

float YouKnow106Engine::panelPositionForDecay(float seconds) noexcept
{
    if (!(seconds > 0.0f) || !std::isfinite(seconds))
        return 0.0f;
    if (seconds <= envelopeDecaySeconds(0.0f))
        return 0.0f;
    if (seconds >= envelopeDecaySeconds(1.0f))
        return 1.0f;

    return nearestBytePositionByLogRatio(0, 127, 127.0f, seconds,
                                         envelopeDecaySeconds);
}

float YouKnow106Engine::panelPositionForRelease(float seconds) noexcept
{
    if (!(seconds > 0.0f) || !std::isfinite(seconds))
        return 0.0f;
    if (seconds <= envelopeReleaseSeconds(0.0f))
        return 0.0f;
    if (seconds >= envelopeReleaseSeconds(1.0f))
        return 1.0f;

    return nearestBytePositionByLogRatio(0, 127, 127.0f, seconds,
                                         envelopeReleaseSeconds);
}

float YouKnow106Engine::panelPositionForLfoRate(float hertz) noexcept
{
    if (!(hertz > 0.0f) || !std::isfinite(hertz))
        return 0.0f;
    if (hertz <= lfoRateHz(0.0f))
        return 0.0f;
    if (hertz >= lfoRateHz(1.0f))
        return 1.0f;

    return nearestBytePositionByLogRatio(0, 127, 127.0f, hertz, lfoRateHz);
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
    // The shared search returns the first canonical ADC code producing the
    // closest displayed seconds-per-octave value.
    return nearestBytePositionByLogRatio(2, 255, 255.0f, secondsPerOctave,
                                         portamentoSeconds);
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

std::uint16_t YouKnow106Engine::pwmDacCode(
    float panelPosition, PwmSource source, std::uint16_t lfoAccumulator,
    bool positivePolarity) noexcept
{
    // FF47 is the stored byte doubled to 0..254. The two partial MULs at
    // 0773..077B are exactly floor(P * Q / 256); subtracting that from 0x3fff
    // and dropping loadDac's two unused low bits is equivalently
    // 0x0fff - floor(P * Q / 1024).
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L298-L313
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L1144-L1197
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L1292-L1302
    const std::uint32_t depth = 2u * storedControlByte(panelPosition);
    const std::uint32_t accumulator = std::min<std::uint32_t>(
        lfoAccumulator, 0x1fffu);
    const std::uint32_t modulation = source == PwmSource::Manual
        ? 0x3fffu
        : (positivePolarity ? 0x2000u + accumulator
                            : 0x2000u - accumulator);
    return static_cast<std::uint16_t>(
        0x0fffu - ((depth * modulation) >> 10u));
}

float YouKnow106Engine::pwmDacVolts(std::uint16_t code) noexcept
{
    // Roland calibrates code 0x0fff to +6 V / 50% duty. Square Off makes B-2
    // save zero, which the same linear converter path presents as the printed
    // -0.8 V comparator-pinning state. The +0.6 V / 95% row is the loaded
    // physical slider's nominal top, not the seven-bit data format's endpoint.
    // https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=9
    // https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=19
    const double fraction = static_cast<double>(std::min<std::uint16_t>(
        code, 0x0fffu)) / 4095.0;
    return static_cast<float>(-0.8 + 6.8 * fraction);
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

namespace
{
// Both comparator edges share this reset/rise pair. Callers that need both
// edges -- renderVoice's end-of-sample reconciliation calls both back to
// back with the same arguments -- would otherwise clamp and re-derive the
// same two floats twice per voice per internal sample for no reason.
struct PulseRampGeometry
{
    float reset;
    float rise;
};

PulseRampGeometry pulseRampGeometry(float resetFraction) noexcept
{
    const float reset = std::clamp(resetFraction, 0.0f, 0.25f);
    return { reset, std::max(1.0f - reset, 1.0e-4f) };
}
} // namespace

float YouKnow106Engine::pulseRisePhase(float duty, float resetFraction) noexcept
{
    const auto geometry = pulseRampGeometry(resetFraction);
    return geometry.rise * (1.0f - std::clamp(duty, 0.0f, 1.0f));
}

float YouKnow106Engine::pulseFallPhase(float duty, float resetFraction) noexcept
{
    const auto geometry = pulseRampGeometry(resetFraction);
    // The ramp runs 0..1 over the rise and falls linearly back over the reset,
    // both mapped to -1..+1. Solving the falling segment for the rise's
    // threshold gives the fraction of the reset spent still above it.
    const float threshold = 1.0f - std::clamp(duty, 0.0f, 1.0f);
    return geometry.rise
         + geometry.reset * std::clamp(1.0f - threshold, 0.0f, 1.0f);
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
    // The resistor-limited physical slider normally stops near +0.6 V, but
    // B-2 accepts all seven-bit SysEx values. That digital overrange can ask
    // for 0..+0.6 V before finally crossing below zero and pinning the output,
    // so retain the established +6 V / 50% floor but move the lower clamp from
    // the physical slider stop to the comparator ramp's zero-volt rail.
    const float volts = std::clamp(sanitised(controlVolts, 6.0f), 0.0f, 6.0f);
    const float scale = std::clamp(
        sanitised(rampAmplitudeScale, 1.0f), 0.25f, 4.0f);
    return std::clamp(1.0f - volts / (12.0f * scale), 0.0f, 1.0f);
}

float YouKnow106Engine::VoiceVcaControlLaw::gain(float control) noexcept
{
    // The grounded-base stage motivates a quasi-linear response above
    // conduction, but V_be depends on current and the BA662's low-current gm is
    // not measured here. Softplus is a smooth compatibility approximation to
    // that shape, normalised so full control is unity gain; its provisional
    // turn-on and knee choose the low-level curvature.
    const float level = clamp01(sanitised(control, 0.0f));
    if (level <= deadband)
        return 0.0f;
    const float x = (level - turnOn) / knee;
    // log1p(exp(x)) is x to the last bit long before x reaches thirty, and the
    // exponential would overflow well after that; take the limit early so the
    // linear region costs one comparison rather than two transcendentals.
    const float softplus = x > 30.0f ? x : std::log1p(std::exp(x));
    return knee * softplus / (1.0f - turnOn);
}

float YouKnow106Engine::commonVcaControlVolts(float dacFraction) noexcept
{
    const float position = clamp01(dacFraction);
    const float converterVolts = commonVcaDacReferenceVolts
        * commonVcaMaximumDacCode * position / commonVcaDacSteps;
    const float holdVolts = commonVcaBufferOffsetVolts
                          + commonVcaBufferGain * converterVolts;

    // At DC C7 is open, so the held voltage sees R30+R32 in series. GC1 is
    // additionally tied to ground by R31 and biased from +15 V by R165.
    constexpr float holdSeriesOhms = commonVcaR30Ohms + commonVcaR32Ohms;
    return (holdVolts / holdSeriesOhms
            + commonVcaBiasVolts / commonVcaR165Ohms)
         / (1.0f / holdSeriesOhms
            + 1.0f / commonVcaR31Ohms
            + 1.0f / commonVcaR165Ohms);
}

float YouKnow106Engine::commonVcaHoldTimeConstantSeconds() noexcept
{
    // With ideal voltage sources AC-grounded, C7 sees R30 in parallel with
    // R32 plus the GC1 bias network. Nominal: 908.249 ohms * 10 uF = 9.08249 ms.
    constexpr float gcBiasOhms =
        commonVcaR31Ohms * commonVcaR165Ohms
        / (commonVcaR31Ohms + commonVcaR165Ohms);
    constexpr float farSideOhms = commonVcaR32Ohms + gcBiasOhms;
    constexpr float theveninOhms =
        commonVcaR30Ohms * farSideOhms
        / (commonVcaR30Ohms + farSideOhms);
    return theveninOhms * commonVcaC7Farads;
}

float YouKnow106Engine::patchLevelGain(float dacFraction) noexcept
{
    // NEC's typical control constant is linear in dB. Installed rail, resistor,
    // capacitor and IC spread remain measurement questions; they are not
    // replaced here by synthetic random offsets.
    const float decibels = commonVcaControlVolts(dacFraction)
                         / commonVcaControlVoltsPerDecibel;
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
    // own designators is what separated them.
    //
    // The boost's corner is the branch's own dominant pole, read at designator
    // level from a complete 300 dpi scan of p. 15 (2026-08-07): Y3 crosses
    // C9 47 nF in parallel with R22 47 kOhm into the node C8 10 nF shunts to
    // ground, so the pole is R22*(C9+C8), or 59.41 Hz, and the section's
    // 72.05 Hz zero all but cancels the 72.34 Hz pole of IC4b's C6-bypassed
    // feedback -- which is why one corner describes a two-stage branch to
    // within 0.016 dB.
    switch (mode)
    {
        case HighPassMode::Boost: return 59.4083f; // R22 x (C9 + C8)
        case HighPassMode::Two:   return 225.8f;   // 47 kOhm x 15 nF
        case HighPassMode::Three: return 720.5f;   // 47 kOhm x 4.7 nF
        case HighPassMode::One:
        default:                  return 59.4083f;
    }
}

float YouKnow106Engine::highPassShelfGain(HighPassMode mode) noexcept
{
    // How much of the low band the leg returns. The boost position's DC gain
    // is derived from the branch: the dry R25 leg at unity plus IC4b's
    // DC-coupled leg -- R22 passes DC around C9, C6 leaves the full
    // 1 + R18/R19 = 11 stage gain, and R24 220 kOhm reaches the R29 47 kOhm
    // summing bus -- for 1 + (47/220)*11 = 3.35, or +10.50 dB. A hardware
    // noise sweep independently landed on the same figure, far more than the
    // +3 dB an earlier account reported. The straight-through leg returns the
    // low band untouched, and the two cutting legs discard it.
    switch (mode)
    {
        case HighPassMode::Boost: return 1.0f + (47.0f / 220.0f) * 11.0f;
        case HighPassMode::One:   return 1.0f;
        case HighPassMode::Two:
        case HighPassMode::Three:
        default:                  return 0.0f;
    }
}

float YouKnow106Engine::highPassHighGain(HighPassMode mode) noexcept
{
    // The boost leg lifts the high band a little too. Above the corner C9
    // carries the branch and C8 divides it -- C9/(C9+C8) = 47/57 -- while C6
    // shorts IC4b's feedback to unity gain, so the plateau is
    // 1 + (47/220)*(47/57) = 1.17616, or +1.41 dB, exactly where the hardware
    // noise sweep settled. Every other leg passes the high band at unity.
    switch (mode)
    {
        case HighPassMode::Boost:
            return 1.0f + (47.0f / 220.0f) * (47.0f / 57.0f);
        case HighPassMode::One:
        case HighPassMode::Two:
        case HighPassMode::Three:
        default:                  return 1.0f;
    }
}

float YouKnow106Engine::outputCouplingCornerHz() noexcept
{
    return rcCornerHz(outputCouplingCapacitanceF,
                       outputCouplingSeriesOhms + outputCouplingPotOhms);
}

float YouKnow106Engine::voiceBusCouplingCornerHz() noexcept
{
    return rcCornerHz(voiceBusCouplingCapacitanceF,
                       voiceBusCouplingResistanceOhms);
}

float YouKnow106Engine::voiceBusCouplingCornerHz(HighPassMode mode) noexcept
{
    float selectedInputOhms = 0.0f;
    switch (mode)
    {
        case HighPassMode::Boost:
        case HighPassMode::One:
            selectedInputOhms = 47000.0f;
            break;
        case HighPassMode::Two:
        case HighPassMode::Three:
            selectedInputOhms = highPassCutBleedResistanceOhms;
            break;
        default:
            return voiceBusCouplingCornerHz();
    }
    const float parallelOhms =
        voiceBusCouplingResistanceOhms * selectedInputOhms
        / (voiceBusCouplingResistanceOhms + selectedInputOhms);
    return rcCornerHz(voiceBusCouplingCapacitanceF, parallelOhms);
}

float YouKnow106Engine::commonVcaInputCouplingCornerHz() noexcept
{
    return rcCornerHz(commonVcaInputCapacitanceF,
                       commonVcaInputResistanceOhms);
}

float YouKnow106Engine::moduleCouplingCornerHz() noexcept
{
    return rcCornerHz(moduleCouplingCapacitanceF, moduleCouplingResistanceOhms);
}

float YouKnow106Engine::vcaInputCouplingCornerHz() noexcept
{
    return rcCornerHz(vcaInputCouplingCapacitanceF,
                       vcaInputCouplingResistanceOhms);
}

float YouKnow106Engine::noiseSourceHighPassHz() noexcept
{
    return rcCornerHz(noiseCouplingCapacitanceF, noiseCouplingLoadOhms);
}

float YouKnow106Engine::noiseSourceLowPassHz() noexcept
{
    return rcCornerHz(noiseOtaLoadCapacitanceF, noiseOtaLoadResistanceOhms);
}

float YouKnow106Engine::outputBoundaryGain() noexcept
{
    // One internal unit is internalVoltsPerUnit. The provisional summer
    // asymptote through the volume wiper's maximum passband gain defines this
    // model's steady-state 0 dBFS policy; OQ-05 still owns the physical swing.
    const float fullScaleVolts = outputSummerSwingAsymptoteVolts
                               * outputCouplingHighGain(1.0f);
    if (!(fullScaleVolts > 0.0f) || !std::isfinite(fullScaleVolts))
        return 1.0f;
    // Expressed through the same Vref helper every other boundary question
    // uses: the reference is the RMS that lands on -18 dBFS once full scale is
    // the model asymptote, so the two cannot drift apart.
    return outputReferenceGain(minus18DbfsAmplitude * fullScaleVolts);
}

float YouKnow106Engine::outputSummerResistorNoiseDensity() noexcept
{
    // Each input resistor's own voltage noise is multiplied by its inverting
    // signal gain Rf/Rin. The feedback resistor appears directly at the
    // output. Uncorrelated sources add as powers, never as amplitudes.
    const float equivalentResistance = outputSummerFeedbackOhms
        + outputSummerFeedbackOhms * outputSummerFeedbackOhms
            / outputSummerDryInputOhms
        + outputSummerFeedbackOhms * outputSummerFeedbackOhms
            / outputSummerWetInputOhms;
    return std::sqrt(4.0f * boltzmannConstant * outputNoiseTemperatureKelvin
                     * equivalentResistance);
}

float YouKnow106Engine::outputWiperNoiseResistance(
    float volumePosition) noexcept
{
    // At thermal equilibrium the complete passive network's open-circuit
    // voltage noise is 4kTR_th. With IC6's ideal small-signal output grounded,
    // the wiper sees its upper track plus R54/R57, its lower track, the selector
    // ladder and the headphone-amplifier input as parallel paths to ground.
    const float position = clamp01(sanitised(volumePosition, 0.0f));
    const float upper = outputCouplingSeriesOhms
                      + (1.0f - position) * outputCouplingPotOhms;
    const float lower = position * outputCouplingPotOhms;
    if (lower == 0.0f)
        return 0.0f;
    float conductance = 1.0f / upper + 1.0f / outputWiperInternalLoadOhms;
    conductance += 1.0f / lower;
    return 1.0f / conductance;
}

float YouKnow106Engine::outputCouplingHighGain() noexcept
{
    return outputCouplingPotOhms
         / (outputCouplingSeriesOhms + outputCouplingPotOhms);
}

float YouKnow106Engine::outputCouplingCornerHz(float volumePosition) noexcept
{
    return rcCornerHz(
        outputCouplingCapacitanceF,
        outputCouplingWiperNetworkFor(volumePosition).resistance);
}

float YouKnow106Engine::outputCouplingHighGain(float volumePosition) noexcept
{
    const auto network = outputCouplingWiperNetworkFor(volumePosition);
    if (!(network.loadedLower > 0.0f))
        return 0.0f;
    return network.loadedLower / network.resistance;
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

std::uint32_t YouKnow106Engine::xorshift32(std::uint32_t state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float YouKnow106Engine::bipolarFromState(std::uint32_t state) noexcept
{
    return static_cast<float>(state & 0xffffffu) * (2.0f / 16777215.0f) - 1.0f;
}

float YouKnow106Engine::hashBipolar(std::uint32_t value) noexcept
{
    return bipolarFromState(hash32(value));
}

float YouKnow106Engine::resetFraction(double periodSeconds) noexcept
{
    if (!(periodSeconds > 0.0))
        return 0.25f;
    const double fraction = static_cast<double>(rampResetSeconds) / periodSeconds;
    return static_cast<float>(std::clamp(fraction, 1.0e-6, 0.25));
}

double YouKnow106Engine::dcoPositiveBaseRail(
    double totalRampScale) noexcept
{
    // The stored ramp state x is mapped to physical volts as
    // V = 6 * totalScale * (x + 1). Solve V=+15 V for x rather than clamping
    // x itself: compensation and card-current scale are part of the same
    // physical ramp and therefore move its base-coordinate supply crossing.
    const double safeScale = std::max(totalRampScale, 1.0e-6);
    return static_cast<double>(dcoPositiveRailVolts)
         / (0.5 * static_cast<double>(rampAmplitudeVolts) * safeScale)
         - 1.0;
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

    const int delayIndex = base % correctionHalfWidth;
    const float output = delay[static_cast<std::size_t>(delayIndex)]
                       + ring[static_cast<std::size_t>(base)];
    ring[static_cast<std::size_t>(base)] = 0.0f;
    delay[static_cast<std::size_t>(delayIndex)] = naive;
    base = base + 1 < correctionRing ? base + 1 : 0;

    return output;
}

const YouKnow106Engine::CorrectionTables&
YouKnow106Engine::correctionTables() noexcept
{
    static const CorrectionTables tables = [] {
        // Integrate a Blackman-windowed sinc to obtain the continuous
        // bandlimited step. Integrating once more gives the bandlimited ramp.
        // The ideal step is deliberately not subtracted here: its residual has
        // a unit jump at t=0, and interpolating that discontinuous table made
        // the lookup interval before zero emit a premature fractional edge.
        // The slope residual is continuous at zero, so it remains stored
        // directly and retains precision near the kernel boundary.
        constexpr int length = correctionTableLength;
        constexpr double step = 1.0 / correctionOversample;

        // A finite reconstruction kernel needs a transition band of its own.
        // Centring the ideal cutoff exactly on Nyquist left the newly recovered
        // B-2 note-84/8' grid's 24.109 kHz pulse harmonic only 66.1 dB down
        // when it folded to 19.991 kHz at a 44.1 kHz host. A 1.5%-of-Nyquist
        // guard restores more than 80 dB rejection there while retaining the
        // complete 0..15 kHz strict band and staying within 0.15 dB at 20 kHz.
        constexpr double cutoff = 0.985;
        std::array<double, length> impulse {};
        for (int i = 0; i < length; ++i)
        {
            const double t = static_cast<double>(i) * step
                           - correctionHalfWidth;
            const double sinc = std::abs(t) < 1.0e-12
                ? cutoff
                : std::sin(3.14159265358979323846 * cutoff * t)
                    / (3.14159265358979323846 * t);
            const double phase = static_cast<double>(i)
                               / static_cast<double>(length - 1);
            const double window = 0.42
                - 0.5 * std::cos(2.0 * 3.14159265358979323846 * phase)
                + 0.08 * std::cos(4.0 * 3.14159265358979323846 * phase);
            impulse[static_cast<std::size_t>(i)] = sinc * window;
        }

        // Trapezoidal running integral, normalised so the step ends at one.
        std::array<double, length> stepResponse {};
        double accumulator = 0.0;
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
        for (int i = 1; i < length; ++i)
        {
            accumulator += 0.5 * step
                         * (stepResponse[static_cast<std::size_t>(i - 1)]
                            + stepResponse[static_cast<std::size_t>(i)]);
            rampResponse[static_cast<std::size_t>(i)] = accumulator;
        }

        CorrectionTables result;
        for (int i = 0; i < length; ++i)
        {
            result.stepResponse[static_cast<std::size_t>(i)] =
                static_cast<float>(stepResponse[static_cast<std::size_t>(i)]);
            const double t = static_cast<double>(i) * step
                           - correctionHalfWidth;
            const double idealRamp = t >= 0.0 ? t : 0.0;
            result.slopeResidual[static_cast<std::size_t>(i)] =
                static_cast<float>(rampResponse[static_cast<std::size_t>(i)]
                                   - idealRamp);
        }
        return result;
    }();
    return tables;
}

// The linear-interpolated read of an oversampled correction table at ring
// sample `ringIndex`, `offset` subsamples into it. addStep and addSlope both
// walk their own table this same way, sample for sample -- factored out so
// neither repeats the identical clamp/lerp arithmetic for every one of
// correctionRing samples of every event.
float YouKnow106Engine::interpolatedCorrectionSample(
    const std::array<float, correctionTableLength>& table,
    int ringIndex, float offset) noexcept
{
    const float position = (static_cast<float>(ringIndex) + offset)
                         * static_cast<float>(correctionOversample);
    const int lower = std::clamp(static_cast<int>(position), 0,
                                 correctionTableLength - 2);
    const float fraction = std::clamp(
        position - static_cast<float>(lower), 0.0f, 1.0f);
    return table[static_cast<std::size_t>(lower)]
         + (table[static_cast<std::size_t>(lower + 1)]
            - table[static_cast<std::size_t>(lower)]) * fraction;
}

// `samplesAgo` is how far back inside the sample just rendered the event sits,
// in [0, 1). Output sample `j` of the correction ring is `j - halfWidth`
// samples away from the sample just rendered, so the residual is read at
// `j - halfWidth + samplesAgo` and the table is offset by the half width.
//
// The continuous response table is read with linear interpolation, not nearest
// neighbour. The ideal step is then evaluated exactly at the query time.
// Keeping the discontinuity out of the interpolated data is essential: even a
// dense table otherwise blends across the unit jump immediately before t=0.
void YouKnow106Engine::addStep(BandlimitedTrack& track, float height,
                               float samplesAgo) const noexcept
{
    if (!(std::abs(height) > 0.0f))
        return;
    const auto& table = correctionTables().stepResponse;
    const float offset = std::clamp(samplesAgo, 0.0f, 1.0f);
    // `track.base + j` only ever wraps the ring once as j runs 0..correctionRing-1,
    // since track.base already sits in [0, correctionRing). Walking `slot` forward
    // with the same increment-or-reset the ring's own writer uses (see
    // BandlimitedTrack::advance) reaches the identical index every iteration
    // without a modulo by the non-power-of-two ring size on each of them.
    int slot = track.base;
    for (int j = 0; j < correctionRing; ++j)
    {
        const float response = interpolatedCorrectionSample(table, j, offset);
        const float queryTime = static_cast<float>(j - correctionHalfWidth)
                              + offset;
        const float residual = response - (queryTime >= 0.0f ? 1.0f : 0.0f);
        track.ring[static_cast<std::size_t>(slot)] += height * residual;
        slot = slot + 1 < correctionRing ? slot + 1 : 0;
    }
}

void YouKnow106Engine::addSlope(BandlimitedTrack& track, float slopeStep,
                                float samplesAgo) const noexcept
{
    if (!(std::abs(slopeStep) > 0.0f))
        return;
    const auto& table = correctionTables().slopeResidual;
    const float offset = std::clamp(samplesAgo, 0.0f, 1.0f);
    // See addStep's identical walk above for why this avoids a per-iteration
    // modulo.
    int slot = track.base;
    for (int j = 0; j < correctionRing; ++j)
    {
        const float residual = interpolatedCorrectionSample(table, j, offset);
        track.ring[static_cast<std::size_t>(slot)] += slopeStep * residual;
        slot = slot + 1 < correctionRing ? slot + 1 : 0;
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

std::uint32_t YouKnow106Engine::Dco::mode3HalfClocks(
    std::uint32_t count, bool outHigh) noexcept
{
    return outHigh ? (count + 1u) / 2u : count / 2u;
}

bool YouKnow106Engine::Dco::programMode3(
    double clocksToNextInputEdge) noexcept
{
    // A mode word resets the CE and forces OUT high immediately. The selected
    // input clock keeps running while CE waits for the later LSB/MSB pair.
    // IC35/TP5 is one shared clock, so the engine supplies that global phase
    // rather than recovering six incompatible phases from the PIT counters.
    const double nextClock = std::isfinite(clocksToNextInputEdge)
                          && clocksToNextInputEdge > 0.0
        ? clocksToNextInputEdge : 1.0;

    const bool positiveEdge = !pitOutHigh;
    pitOutHigh = true;
    pendingDividerValid = false;
    pitState = PitState::awaitingCount;
    pitClocksToEvent = nextClock;
    return positiveEdge;
}

void YouKnow106Engine::Dco::stageMode3Count(std::uint32_t count) noexcept
{
    // The count register has one value, so another complete pair before the
    // transfer replaces the older pending pair -- including a write equal to
    // the active CE count.
    pendingDivider = count;
    pendingDividerValid = true;
    if (pitState == PitState::awaitingCount)
        pitState = PitState::awaitingInitialLoad;
}

YouKnow106Engine::Dco::PitEvent
YouKnow106Engine::Dco::consumePitEvent() noexcept
{
    if (pitState == PitState::awaitingInitialLoad)
    {
        if (pendingDividerValid)
            divider = pendingDivider;
        pendingDividerValid = false;
        pitState = PitState::running;
        pitOutHigh = true;
        pitClocksToEvent = static_cast<double>(
            mode3HalfClocks(divider, true));
        return PitEvent::initialLoad;
    }

    pitOutHigh = !pitOutHigh;
    if (pendingDividerValid)
        divider = pendingDivider;
    pendingDividerValid = false;
    pitClocksToEvent = static_cast<double>(
        mode3HalfClocks(divider, pitOutHigh));
    return pitOutHigh ? PitEvent::risingEdge : PitEvent::fallingEdge;
}

void YouKnow106Engine::Dco::reset() noexcept
{
    pendingDivider = divider;
    pendingDividerValid = false;
    pitState = PitState::stopped;
    pitOutHigh = true;
    pitWriteState = PitWriteState::idle;
    pitWriteDivider = divider;
    cpuStatesToWrite = 0.0;
    coldInitialLoadPending = false;
    pitClocksToEvent = 0.0;
    rampValue = -1.0;
    rampSlopePerSecond = 0.0;
    resetSecondsRemaining = 0.0;
    positiveRailHeld = false;
    renderScale = 1.0f;
    pulseState = -1.0f;
    subState = 1.0f;
    saw.reset();
    pulse.reset();
    sub.reset();
}

void YouKnow106Engine::beginDcoDischarge(
    Voice& voice, float samplesAgo, bool addCorrections) noexcept
{
    auto& dco = voice.dco;
    dco.positiveRailHeld = false;
    const double intervalSeconds = 1.0 / oversampledRate_;
    const float oldSlope = static_cast<float>(
        dco.rampSlopePerSecond * static_cast<double>(dco.renderScale)
        * intervalSeconds);
    const double periodSeconds = std::max(
        dco.periodSamples / oversampledRate_, 1.0e-12);
    const double resetSeconds = std::max(
        static_cast<double>(resetFraction(periodSeconds)) * periodSeconds,
        1.0e-12);
    dco.rampSlopePerSecond = (-1.0 - dco.rampValue) / resetSeconds;
    dco.resetSecondsRemaining = resetSeconds;
    const float newSlope = static_cast<float>(
        dco.rampSlopePerSecond * static_cast<double>(dco.renderScale)
        * intervalSeconds);
    if (addCorrections && dco.saw.primed)
        addSlope(dco.saw, newSlope - oldSlope, samplesAgo);

    const float nextSub = -dco.subState;
    if (addCorrections && dco.sub.primed)
        addStep(dco.sub, nextSub - dco.subState, samplesAgo);
    dco.subState = nextSub;
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(dcoSubTransitions, 1);
#endif
}

void YouKnow106Engine::beginDcoCharge(
    Voice& voice, float samplesAgo, bool addCorrections) noexcept
{
    auto& dco = voice.dco;
    dco.positiveRailHeld = false;
    const double intervalSeconds = 1.0 / oversampledRate_;
    const float oldSlope = static_cast<float>(
        dco.rampSlopePerSecond * static_cast<double>(dco.renderScale)
        * intervalSeconds);
    dco.rampValue = -1.0;
    dco.resetSecondsRemaining = 0.0;
    dco.renderScale = dcoLaunchScale(voice);
    const double periodSeconds = std::max(
        dco.periodSamples / oversampledRate_, 1.0e-12);
    const double resetSeconds = static_cast<double>(
        resetFraction(periodSeconds)) * periodSeconds;
    dco.rampSlopePerSecond = 2.0 / std::max(
        periodSeconds - resetSeconds, periodSeconds * 1.0e-4);
    const float newSlope = static_cast<float>(
        dco.rampSlopePerSecond * static_cast<double>(dco.renderScale)
        * intervalSeconds);
    if (addCorrections && dco.saw.primed)
        addSlope(dco.saw, newSlope - oldSlope, samplesAgo);
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(dcoCycleWraps, 1);
#endif
}

void YouKnow106Engine::updateActiveDcoPeriod(
    Dco& dco, DcoRange range) noexcept
{
    const double frequency = dcoQuantisedFrequency(dco.divider, range);
    dco.periodSamples = frequency > 0.0
                      ? oversampledRate_ / frequency : 1.0e6;
}

void YouKnow106Engine::beginRangeClockTransition(
    DcoRange previous, DcoRange next) noexcept
{
    if (previous == next)
        return;

    const double previousClockHz = rangeClockHz(previous);
    const double nextClockHz = rangeClockHz(next);
    const double rateRatio = nextClockHz / previousClockHz;
    const double clockTolerance = std::max(
        1.0e-15, (1.0 / oversampledRate_) * 1.0e-9) * previousClockHz;
    const double oldClocksToFalling = rangeClockClocksToFallingEdge_;
    double oldClocksToReload = rangeClockClocksToReload_;
    if (!rangeClockTransitionPending_)
    {
        const double rawTickClocks = previousClockHz / masterClockHz;
        const double highClocks = 1.0 - rawTickClocks;
        // At exact reload equality, the old preset wins before the PF write.
        // The new preset is therefore captured at the following reload. This
        // compatibility policy is independent of PIT /WR ordering at falling.
        oldClocksToReload = oldClocksToFalling
                                > highClocks + clockTolerance
            ? oldClocksToFalling - highClocks
            : oldClocksToFalling + rawTickClocks;
    }

    const bool fallingBeforeReload =
        oldClocksToFalling + clockTolerance < oldClocksToReload;
    const double newClocksToReload = oldClocksToReload * rateRatio;
    const double newRawTickClocks = nextClockHz / masterClockHz;
    const double firstNewFallingAfterReload =
        newClocksToReload + 1.0 - newRawTickClocks;
    const double newClocksToFalling = fallingBeforeReload
        ? oldClocksToFalling * rateRatio
        : firstNewFallingAfterReload;
    for (auto& voice : voices_)
    {
        auto& dco = voice.dco;
        if (dco.pitState == Dco::PitState::stopped
            || !(dco.pitClocksToEvent > 0.0))
            continue;

        // Preserve the number of TP5 falling/count edges still owed. A PF
        // write made during TP5's one-raw-tick low interval changes the very
        // next falling edge; a write made earlier leaves the imminent old
        // falling edge intact and changes the following interval.
        const double fallingEdgesAfterNext = std::max(
            0.0, std::round(
                dco.pitClocksToEvent - oldClocksToFalling));
        dco.pitClocksToEvent = newClocksToFalling
                             + fallingEdgesAfterNext;
    }

    rangeClockClocksToFallingEdge_ = newClocksToFalling;
    rangeClockClocksToReload_ = newClocksToReload;
    rangeClockTransitionPending_ = true;
}

double YouKnow106Engine::rangeClockClocksToNextFallingEdge(
    double elapsedSeconds, DcoRange range) const noexcept
{
    const double clockHz = rangeClockHz(range);
    const double clocksAdvanced = std::max(0.0, elapsedSeconds) * clockHz;
    const double clockTolerance = std::max(
        1.0e-15, (1.0 / oversampledRate_) * 1.0e-9) * clockHz;
    const auto stablePhase = [clockTolerance](double clocks) {
        double phase = std::fmod(clocks, 1.0);
        if (phase < 0.0)
            phase += 1.0;
        if (phase <= clockTolerance || 1.0 - phase <= clockTolerance)
            phase = 1.0;
        return phase;
    };

    if (!rangeClockTransitionPending_)
        return stablePhase(
            rangeClockClocksToFallingEdge_ - clocksAdvanced);

    if (clocksAdvanced + clockTolerance < rangeClockClocksToReload_)
    {
        if (clocksAdvanced + clockTolerance
            < rangeClockClocksToFallingEdge_)
        {
            return rangeClockClocksToFallingEdge_ - clocksAdvanced;
        }

        // The old-modulus falling edge has happened, but its reload has not.
        // The following falling edge is one selected period minus one raw
        // 8 MHz tick after that pending reload.
        return 1.0 - std::max(
            0.0, clocksAdvanced - rangeClockClocksToFallingEdge_);
    }

    const double clocksAfterReload =
        std::max(0.0, clocksAdvanced - rangeClockClocksToReload_);
    return stablePhase(
        1.0 - clockHz / masterClockHz - clocksAfterReload);
}

void YouKnow106Engine::advanceRangeClock(DcoRange range) noexcept
{
    const double clockHz = rangeClockHz(range);
    const double intervalSeconds = 1.0 / oversampledRate_;
    const double clockTolerance = std::max(
        1.0e-15, intervalSeconds * 1.0e-9) * clockHz;
    const double clocksAdvanced = clockHz * intervalSeconds;
    const double nextFalling = rangeClockClocksToNextFallingEdge(
        intervalSeconds, range);
    if (rangeClockTransitionPending_)
    {
        if (clocksAdvanced + clockTolerance < rangeClockClocksToReload_)
        {
            rangeClockClocksToReload_ -= clocksAdvanced;
            rangeClockClocksToFallingEdge_ = nextFalling;
            return;
        }

        rangeClockClocksToReload_ = 0.0;
        rangeClockTransitionPending_ = false;
    }
    rangeClockClocksToFallingEdge_ = nextFalling;
}

void YouKnow106Engine::writeDcoMode3Control(
    Voice& voice, double clocksToNextInputEdge, float samplesAgo,
    bool addCorrections) noexcept
{
    auto& dco = voice.dco;
    const bool coldStart = dco.pitState == Dco::PitState::stopped;
    dco.coldInitialLoadPending = dco.coldInitialLoadPending || coldStart;

    // Mode programming forces OUT high. That is a physical C54/sub event only
    // when the previously stored output was low; an already-high output does
    // not acquire a fabricated edge or a fabricated sub polarity.
    if (dco.programMode3(clocksToNextInputEdge))
        beginDcoDischarge(voice, samplesAgo, addCorrections);
}

void YouKnow106Engine::prestageDcoPitchTransaction(
    Voice& voice, double clocksToNextInputEdge, float samplesAgo,
    bool addCorrections) noexcept
{
    auto& dco = voice.dco;
    const float previousCvTarget = voice.dcoCvTarget;
    const std::uint32_t count = updateVoiceEnvelopeAndPitch(
        voice, activeParameters_);
    const float cvTarget = voice.dcoCvTarget;
    voice.dcoCvTarget = previousCvTarget;

    const bool writesControlWord = voice.dcoResetPending;
    voice.dcoResetPending = false;
    voice.dcoPitchTransactionValid = true;
    voice.dcoPitchTransactionColdStart = writesControlWord
        && dco.pitState == Dco::PitState::stopped;
    voice.dcoPitchTransactionCvTarget = cvTarget;
    dco.pitWriteDivider = count;

    if (writesControlWord)
        writeDcoMode3Control(
            voice, clocksToNextInputEdge, samplesAgo, addCorrections);
    dco.pitWriteState = Dco::PitWriteState::awaitingLsb;
    dco.cpuStatesToWrite = pitControlToLsbStates;
}

void YouKnow106Engine::programDcoCount(
    Voice& voice, std::uint32_t count, bool writesControlWord) noexcept
{
    auto& dco = voice.dco;
    dco.pitWriteDivider = count;
    if (!writesControlWord)
    {
        // Direct calls serve extension slots and the construction-only first
        // PhaseZeroDiagnostic pitch event, neither of which has a preceding
        // physical pre-stage. Preserve their established fallback anchor: LSB
        // now, then the recovered 11-state (2.75 us) spacing to MSB.
        // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L732-L741
        dco.pitWriteState = Dco::PitWriteState::awaitingMsb;
        dco.cpuStatesToWrite = pitLsbToMsbStates;
        return;
    }

    const bool coldStart = dco.pitState == Dco::PitState::stopped;
    if (coldStart)
    {
        // Engine construction has no earlier firmware scan from which to
        // inherit a charged DCO hold. Retain the established deterministic
        // initialization policy: the first programmed cell starts settled.
        voice.dcoCv = voice.dcoCvTarget;
    }
    writeDcoMode3Control(
        voice, rangeClockClocksToFallingEdge_, 1.0f, true);

    // Direct cold-start fallback anchors the control store here. IC29 B-2's
    // reset branch then reaches LSB after 55 CPU states, and both paths take
    // 11 more states to MSB. Ordinary physical transactions use their
    // recovered T-389/T-334/T-323 positions instead.
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L783-L794
    // https://datasheet4u.com/pdf/298676/UPD7810.pdf#page=17
    dco.pitWriteState = Dco::PitWriteState::awaitingLsb;
    dco.cpuStatesToWrite = pitControlToLsbStates;
}

void YouKnow106Engine::OtaCascade::reset() noexcept
{
    state.fill(0.0);
    inputHistory.fill(0.0);
    inputHistoryCount = 0;
    previousOmegaStep = 0.0;
    previousFeedback = 0.0;
    previousHeadroom = 0.0;
    parameterHistoryPrimed = false;
}

void YouKnow106Engine::OtaCascade::retime(float previousStep,
                                          float nextStep) noexcept
{
    // The capacitor voltages are physical and remain untouched. The three
    // older input endpoints are uniformly spaced on the old numerical grid;
    // they cannot be reinterpreted as samples on the new grid. Collapse them
    // to the one endpoint both grids share. updateProcessingRate calls this
    // only inside the existing zero-gain rate transition, and three new
    // internal samples refill the support before the fade is audible.
    inputHistory.fill(inputHistory[0]);
    inputHistoryCount = 0;
    if (parameterHistoryPrimed)
    {
        const double previous = std::max(
            static_cast<double>(previousStep), 0.0);
        const double next = std::max(static_cast<double>(nextStep), 0.0);
        // Both arguments are the actual post-thermal, post-grid-cap
        // intervals on their respective grids. Scaling by their ratio maps
        // the last physical control endpoint exactly even when one side of a
        // rate change is capped. The zero branch is only reachable at reset.
        previousOmegaStep = std::clamp(
            previous > 0.0 ? previousOmegaStep * next / previous : next,
            0.0, maximumOmegaStep);
    }
}

double YouKnow106Engine::OtaCascade::reconstructInput(
    double current, const std::array<double, 3>& history,
    double intervalPosition) noexcept
{
    // Lagrange interpolation through endpoint coordinates
    // { current@1, previous@0, previous2@-1, previous3@-2 }.
    // It is exact for every polynomial through degree three, uses no future
    // endpoint and adds no delay. Its worst coefficient L1 norm on [0,1] is
    // 1.631131; the circuit suite fences both that finite gain and the
    // deliberately exposed Nyquist-alternating overshoot.
    constexpr std::array<double, 4> nodes {
        1.0, 0.0, -1.0, -2.0
    };
    const std::array<double, 4> samples {
        current, history[0], history[1], history[2]
    };
    double result = 0.0;
    for (std::size_t point = 0; point < nodes.size(); ++point)
    {
        double weight = 1.0;
        for (std::size_t other = 0; other < nodes.size(); ++other)
            if (other != point)
                weight *= (intervalPosition - nodes[other])
                        / (nodes[point] - nodes[other]);
        result += weight * samples[point];
    }
    return result;
}

double YouKnow106Engine::OtaCascade::clampOmegaStep(double value) noexcept
{
    return std::clamp(std::isfinite(value) ? value : 0.0,
                      0.0, maximumOmegaStep);
}

YouKnow106Engine::VcfHoldInterval
YouKnow106Engine::exactVcfHoldInterval(
    float state, float target, bool hasEvent, double eventPosition,
    float eventTarget, double intervalSeconds) noexcept
{
    VcfHoldInterval result;
    const double initial = std::isfinite(state)
        ? static_cast<double>(state) : 0.0;
    const double initialTarget = std::isfinite(target)
        ? static_cast<double>(target) : initial;
    const double nextTarget = std::isfinite(eventTarget)
        ? static_cast<double>(eventTarget) : initialTarget;
    const double duration = std::isfinite(intervalSeconds)
        ? std::max(intervalSeconds, 0.0) : 0.0;
    const double event = std::clamp(
        std::isfinite(eventPosition) ? eventPosition : 1.0,
        0.0, 1.0);
    const double lambda = duration
        / static_cast<double>(vcfHoldSlewSeconds);

    for (std::size_t point = 0; point < result.value.size(); ++point)
    {
        const double position = OtaCascade::controlNodePositions[point];
        double value = initialTarget
            + (initial - initialTarget) * std::exp(-position * lambda);
        if (hasEvent && event <= position)
        {
            const double response = -std::expm1(
                -(position - event) * lambda);
            value += (nextTarget - initialTarget) * response;
        }
        result.value[point] = value;
    }
    result.endpoint = static_cast<float>(result.value.back());
    return result;
}

double YouKnow106Engine::exactOnePoleHoldEndpoint(
    double state, float target, bool hasEvent, double eventPosition,
    float eventTarget, double intervalSeconds, double timeConstantSeconds,
    double fullIntervalDecay) noexcept
{
    const double initial = std::isfinite(state) ? state : 0.0;
    const double initialTarget = std::isfinite(target)
        ? static_cast<double>(target) : initial;
    const double duration = std::isfinite(intervalSeconds)
        ? std::max(intervalSeconds, 0.0) : 0.0;
    const double timeConstant = std::isfinite(timeConstantSeconds)
                                    && timeConstantSeconds > 0.0
        ? timeConstantSeconds : 1.0;
    const double decay = std::isfinite(fullIntervalDecay)
        ? std::clamp(fullIntervalDecay, 0.0, 1.0)
        : std::exp(-duration / timeConstant);
    double endpoint = initialTarget + (initial - initialTarget) * decay;
    if (hasEvent)
    {
        const double event = std::clamp(
            std::isfinite(eventPosition) ? eventPosition : 1.0,
            0.0, 1.0);
        const double nextTarget = std::isfinite(eventTarget)
            ? static_cast<double>(eventTarget) : initialTarget;
        // Superposition needs only the response of the target step over the
        // suffix. At p=0 it is the whole interval; at p=1 expm1(0) is exactly
        // zero, so a right-endpoint write cannot influence preceding time.
        const double suffixResponse = -std::expm1(
            -(1.0 - event) * duration / timeConstant);
        endpoint += (nextTarget - initialTarget) * suffixResponse;
    }
    return endpoint;
}

YouKnow106Engine::PwmHoldCoefficients
YouKnow106Engine::pwmHoldCoefficients(double intervalSeconds) noexcept
{
    const double duration = std::isfinite(intervalSeconds)
        ? std::max(intervalSeconds, 0.0) : 0.0;
    const double firstTime = static_cast<double>(pwmHoldFirstPoleSeconds);
    const double secondTime = static_cast<double>(pwmHoldSecondPoleSeconds);
    const double firstDecay = std::exp(-duration / firstTime);
    const double secondDecay = std::exp(-duration / secondTime);
    return {
        firstDecay,
        secondDecay,
        firstTime / (firstTime - secondTime)
            * (firstDecay - secondDecay)
    };
}

YouKnow106Engine::PwmHoldState YouKnow106Engine::advancePwmHold(
    PwmHoldState state, double target,
    const PwmHoldCoefficients& coefficients) noexcept
{
    const double initialFirst = state.first;
    state.first = coefficients.firstDecay * initialFirst
                + (1.0 - coefficients.firstDecay) * target;
    state.second = coefficients.firstToSecond * initialFirst
                 + coefficients.secondDecay * state.second
                 + (1.0 - coefficients.secondDecay
                    - coefficients.firstToSecond) * target;
    return state;
}

YouKnow106Engine::PwmHoldState YouKnow106Engine::exactPwmHoldEndpoint(
    PwmHoldState state, float target, bool hasEvent,
    double eventPosition, float eventTarget, double intervalSeconds,
    const PwmHoldCoefficients& fullIntervalCoefficients) noexcept
{
    const double initialTarget = std::isfinite(target)
        ? static_cast<double>(target) : state.first;
    if (!hasEvent)
        return advancePwmHold(state, initialTarget, fullIntervalCoefficients);

    const double duration = std::isfinite(intervalSeconds)
        ? std::max(intervalSeconds, 0.0) : 0.0;
    const double event = std::clamp(
        std::isfinite(eventPosition) ? eventPosition : 1.0,
        0.0, 1.0);
    const double nextTarget = std::isfinite(eventTarget)
        ? static_cast<double>(eventTarget) : initialTarget;
    state = advancePwmHold(
        state, initialTarget, pwmHoldCoefficients(event * duration));
    return advancePwmHold(
        state, nextTarget,
        pwmHoldCoefficients((1.0 - event) * duration));
}

namespace
{
constexpr double vcfTanhFineLimit = 5.0;
constexpr double vcfTanhLimit = 19.0;
constexpr std::size_t vcfTanhFineIntervals = 160;
constexpr std::size_t vcfTanhTailIntervals = 56;
constexpr double vcfTanhFineWidth = 1.0 / 32.0;
constexpr double vcfTanhTailWidth = 1.0 / 4.0;
constexpr double vcfTanhFineScale = 32.0;
constexpr double vcfTanhTailScale = 4.0;

struct VcfTanhHermiteCoefficient
{
    double constant {};
    double linear {};
    double quadratic {};
    double cubic {};
};

// Each table is built once, off the audio path, from libm's exact-mode values
// and analytic slopes. The fine table covers every argument in the profiled
// single-note and six-note fixtures; the coarse tail preserves the prior
// far-tail saturation behaviour without occupying the hot table's footprint.
template <std::size_t intervals>
std::array<VcfTanhHermiteCoefficient, intervals> makeVcfTanhHermiteTable(
    double start, double width)
{
    struct Node
    {
        double value {};
        double slope {};
    };
    std::array<Node, intervals + 1u> nodes {};
    for (std::size_t index = 0; index < nodes.size(); ++index)
    {
        const double value = std::tanh(
            start + width * static_cast<double>(index));
        nodes[index] = { value, 1.0 - value * value };
    }
    for (std::size_t index = 0; index < intervals; ++index)
        if (nodes[index + 1u].value == nodes[index].value)
        {
            nodes[index].slope = 0.0;
            nodes[index + 1u].slope = 0.0;
        }

    std::array<VcfTanhHermiteCoefficient, intervals> table {};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        const Node left = nodes[index];
        const Node right = nodes[index + 1u];
        // Once adjacent exact-mode nodes round to the same double, the
        // representable function is flat. Their shared node slopes were
        // zeroed above so neighbouring intervals meet it continuously;
        // make the plateau itself explicit too.
        if (right.value == left.value)
        {
            table[index] = { left.value, 0.0, 0.0, 0.0 };
            continue;
        }
        const double delta = right.value - left.value;
        const double leftSlope = width * left.slope;
        const double rightSlope = width * right.slope;
        table[index] = {
            left.value,
            leftSlope,
            3.0 * delta - 2.0 * leftSlope - rightSlope,
            -2.0 * delta + leftSlope + rightSlope
        };
    }
    return table;
}

const auto vcfTanhFineTable = makeVcfTanhHermiteTable<vcfTanhFineIntervals>(
    0.0, vcfTanhFineWidth);
const auto vcfTanhTailTable = makeVcfTanhHermiteTable<vcfTanhTailIntervals>(
    vcfTanhFineLimit, vcfTanhTailWidth);

// The body of `zonedHermiteTanhUnchecked`, force-inlined into the solver's
// right-hand side. As an outlined call it was the single largest consumer in
// the whole-engine profile (~10M calls/s at the default rung); inlining keeps
// the identical expression tree -- same table, same polynomial, same rounding
// -- while letting the nine independent evaluations per RHS overlap.
[[gnu::always_inline]] inline double zonedHermiteTanhImpl(
    double value) noexcept
{
    const double magnitude = std::abs(value);
    if (magnitude < vcfTanhFineLimit)
    {
        const double position = magnitude * vcfTanhFineScale;
        const std::size_t interval = static_cast<std::size_t>(position);
        const double fraction = position - static_cast<double>(interval);
        const auto& coefficient = vcfTanhFineTable[interval];
        const double result = ((coefficient.cubic * fraction
                              + coefficient.quadratic) * fraction
                              + coefficient.linear) * fraction
                              + coefficient.constant;
        return std::copysign(result, value);
    }
    if (magnitude >= vcfTanhLimit)
        return std::copysign(1.0, value);

    const double position = (magnitude - vcfTanhFineLimit)
                          * vcfTanhTailScale;
    const std::size_t interval = static_cast<std::size_t>(position);
    const double fraction = position - static_cast<double>(interval);
    const auto& coefficient = vcfTanhTailTable[interval];
    const double result = ((coefficient.cubic * fraction
                          + coefficient.quadratic) * fraction
                          + coefficient.linear) * fraction
                          + coefficient.constant;
    return std::copysign(result, value);
}

// The PolyZoned rung's inner zone: tanh(x)/x on |x| < 1 as a degree-five
// polynomial in u = x^2 (Chebyshev fit of tanh(sqrt(u))/sqrt(u) on [0, 1];
// max |x*Q(x^2) - tanh(x)| = 4.31e-7 over the zone, an order below the 5e-6
// the measured inner-zone candidates already admitted). The kernel sits on
// the solver's serial dependency chain -- k1 feeds k2 feeds k3 -- so it is
// written in Estrin form: independent first-order pairs combined through
// u^2, four fused steps deep where Horner needs six. Wider alternatives
// were measured and rejected here: a Pade [9/8] core pays a divide on that
// chain, and a two-piece degree-13 fit pays still more depth plus a select;
// both lost to this shallower kernel end to end. |x| < 1 covers 95..99.8%
// of the arguments the profiled scenarios produce; the rest fall through to
// the established zoned Hermite tables.
[[gnu::always_inline]] inline double vcfInnerTanhFactor(double u) noexcept
{
    const double uSquared = u * u;
    const double top = -0.00305822903759879 * u + 0.016720855179576926;
    const double high = -0.051585988404218436 * u + 0.1328072064598552;
    const double low = -0.3332895137021704 * u + 0.9999993948006496;
    return (top * uSquared + high) * uSquared + low;
}

// Full-range scalar form of the PolyZoned nonlinearity: the inner
// polynomial where it is valid, the established zoned Hermite tables beyond.
[[gnu::always_inline]] inline double polyZonedTanhImpl(double value) noexcept
{
    const double u = value * value;
    if (u < 1.0)
        return value * vcfInnerTanhFactor(u);
    return zonedHermiteTanhImpl(value);
}

// Evaluates the zoned nonlinearity across four independent arguments -- the
// four stage nonlinearities of one right-hand-side evaluation, whose
// arguments depend only on the state vector and never on each other's
// outputs, so the four scalar chains overlap in flight.
[[gnu::always_inline]] inline std::array<double, 4> polyZonedTanhBatch(
    const std::array<double, 4>& argument) noexcept
{
    return { polyZonedTanhImpl(argument[0]),
             polyZonedTanhImpl(argument[1]),
             polyZonedTanhImpl(argument[2]),
             polyZonedTanhImpl(argument[3]) };
}
} // namespace

double YouKnow106Engine::OtaCascade::zonedHermiteTanh(double value) noexcept
{
    if (std::isnan(value))
        return value;
    const double magnitude = std::abs(value);
    // Direct callers retain the denormal bypass. The integrated Fast path
    // starts from validated finite state; its table polynomial is bit-exact
    // for subnormals, so it need not repeat this cold guard 90 times per card.
    if (magnitude < std::numeric_limits<double>::min())
        return value;
    return zonedHermiteTanhUnchecked(value);
}

double YouKnow106Engine::OtaCascade::zonedHermiteTanhUnchecked(
    double value) noexcept
{
    return zonedHermiteTanhImpl(value);
}

double YouKnow106Engine::OtaCascade::cubicEarlyTanh(double value) noexcept
{
    const double magnitude = std::abs(value);
    if (magnitude >= 1.5)
        return std::copysign(1.0, value);
    return value * (1.0 - (4.0 / 27.0) * value * value);
}

double YouKnow106Engine::OtaCascade::closedLoopSpectralFactor(
    double feedback) noexcept
{
    // Four identical one-poles closed through `feedback` put the loop roots at
    // s/w = -1 + feedback^(1/4) * exp(i*(pi + 2*pi*m)/4). The farthest of the
    // four from the origin is the one whose exponential lands in the third or
    // fourth quadrant, at distance sqrt((1 + q)^2 + q^2) with
    // q = feedback^(1/4) / sqrt(2). At the sanitized feedback ceiling of eight
    // that is 2.494; with the loop open it is exactly one.
    const double bounded = std::clamp(
        std::isfinite(feedback) ? feedback : 0.0, 0.0, 8.0);
    const double q = std::sqrt(std::sqrt(bounded)) * 0.70710678118654752440;
    const double real = 1.0 + q;
    const double factor = std::sqrt(real * real + q * q);
    // `planTableau`'s short circuit trusts the declared ceiling, so hold the
    // formula to it here rather than restating the constant in a comment. The
    // clamp is the identity for every admissible feedback.
    return std::min(factor, maximumClosedLoopSpectralFactor);
}

// Advance the continuous four-stage OTA equations over one internal interval.
// The default rung's two fixed half-interval Merson steps use five
// right-hand-side evaluations each; the two cheaper rungs run classic RK4 over
// the same interval, and `planTableau` decides which of the three this
// interval can take. The only circuit state is capacitor voltage, and the
// causal cubic supplies input between the two known sample endpoints.
// The compatibility profile closes a circuit-shaped nonlinear resonance
// return, Hfb*tanh(V4/Hfb), so the loop remains bounded beyond oscillation.
template <bool useCubicEarly>
float YouKnow106Engine::OtaCascade::process(float input, float omegaStep,
                                            float feedback,
                                            float headroom,
                                            bool enableEarlyEffect,
                                            float calibration,
                                            const ControlTrajectory* trajectory,
                                            VcfTanhMode tanhMode,
                                            VcfSolverMode solverMode) noexcept
{
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(vcfSteps, 1);
    if (trajectory != nullptr)
    {
        YOUKNOW106_COUNT_DOMAIN_WORK(vcfExactControlIntervals, 1);
        YOUKNOW106_COUNT_DOMAIN_WORK(vcfExactControlNodes,
                                     controlNodePositions.size());
    }
#endif
    constexpr double feedbackHeadroom =
        VoicedResonanceCompatibilityProfile::loopHeadroomVolts;
    const double currentOmega = clampOmegaStep(
        static_cast<double>(omegaStep));
    const double currentFeedback = std::clamp(
        std::isfinite(feedback) ? static_cast<double>(feedback) : 0.0,
        0.0, 8.0);
    const double currentHeadroom = std::max(
        std::isfinite(headroom) ? static_cast<double>(headroom) : 0.0,
        1.0e-5);
    const double currentInput = std::isfinite(input)
        ? static_cast<double>(input) : 0.0;
    const double currentCalibration = std::clamp(
        std::isfinite(calibration) ? static_cast<double>(calibration) : 0.0,
        0.0, static_cast<double>(EngineParameters::calibrationCeiling));

    if (!parameterHistoryPrimed)
    {
        previousOmegaStep = currentOmega;
        previousFeedback = currentFeedback;
        previousHeadroom = currentHeadroom;
        parameterHistoryPrimed = true;
    }

    // A bounded state, the finite histories and card trims maintained by the
    // engine, and the sanitized controls can only form finite Merson arguments.
    // Recover a hostile standalone state once here so Fast does not repeat the
    // helper's NaN check at every nonlinear evaluation.
    const bool useFastTanh = useCubicEarly
                          || tanhMode != VcfTanhMode::Exact;
    if (useFastTanh
        && !std::all_of(state.begin(), state.end(), [](double value) {
               return std::abs(value) <= 64.0;
           }))
    {
#if defined(YOUKNOW106_WORK_AUDIT)
        YOUKNOW106_COUNT_DOMAIN_WORK(vcfRecoveries, 1);
#endif
        state.fill(0.0);
        inputHistory.fill(currentInput);
        inputHistoryCount = 2;
        previousOmegaStep = currentOmega;
        previousFeedback = currentFeedback;
        previousHeadroom = currentHeadroom;
        return 0.0f;
    }

    struct IntegrationNode
    {
        double position {};
        std::array<double, 2> linear {};
        std::array<double, 3> quadratic {};
        std::array<double, 4> cubic {};
    };
    static constexpr auto makeNodes = []<std::size_t count>(
        const std::array<double, count>& positions) {
        std::array<IntegrationNode, count> result {};
        for (std::size_t point = 0; point < count; ++point)
        {
            const double t = positions[point];
            result[point] = {
                t,
                { t, 1.0 - t },
                { 0.5 * t * (t + 1.0), 1.0 - t * t,
                  0.5 * t * (t - 1.0) },
                { t * (t + 1.0) * (t + 2.0) / 6.0,
                  -(t - 1.0) * (t + 1.0) * (t + 2.0) / 2.0,
                  (t - 1.0) * t * (t + 2.0) / 2.0,
                  -(t - 1.0) * t * (t + 1.0) / 6.0 }
            };
        }
        return result;
    };
    static constexpr auto nodes = makeNodes(controlNodePositions);
    constexpr std::size_t pointCount = nodes.size();
    constexpr double substep = 0.5;

    // Which tableau this interval runs. `MersonHalfSteps` is unconditional and
    // pays nothing for the bound; the two RK4 rungs each name the cheapest
    // tableau they will accept and let `planTableau` decide from the largest
    // stage pole and the largest loop gain the interval can present -- both
    // can be escalated, and both fall back to Merson where classic RK4's
    // stability region ends. The trajectory's interior nodes carry the hold's
    // own curvature and can leave the endpoint interval, so they are scanned
    // when one is supplied.
    const Tableau plannedTableau = [&] {
        if (solverMode == VcfSolverMode::MersonHalfSteps)
            return Tableau::MersonHalf;
        double planOmega = std::max(previousOmegaStep, currentOmega);
        double planFeedback = std::max(previousFeedback, currentFeedback);
        if (trajectory != nullptr)
            for (std::size_t point = 0; point < pointCount; ++point)
            {
                planOmega = std::max(
                    planOmega, clampOmegaStep(trajectory->omegaStep[point]));
                const double candidate = trajectory->feedback[point];
                if (std::isfinite(candidate))
                    planFeedback = std::max(
                        planFeedback, std::clamp(candidate, 0.0, 8.0));
            }
        // The stage capacitor spread scales each pole independently, and the
        // Early effect scales every stage rate by at most 1 + earlyAmount.
        double largestScale = 0.0;
        for (const float scale : gScale)
            largestScale = std::max(largestScale,
                                    static_cast<double>(std::abs(scale)));
        const double earlyCeiling = enableEarlyEffect
            ? 1.0 + static_cast<double>(otaEarlyEffectCoefficient)
                        * currentCalibration
            : 1.0;
        return planTableau(solverMode,
                           planOmega * largestScale * earlyCeiling,
                           planFeedback);
    }();
    // A full-interval rung reads three of the seven control nodes. Skipping
    // the rest is not an approximation: the tableau never evaluates there, so
    // the reconstruction, the control interpolation and the per-stage omega
    // product at those ordinals have no reader.
    const unsigned int nodeMask = tableauNodeMask(plannedTableau);

    std::array<double, pointCount> inputAt {};
    std::array<double, pointCount> omegaAt {};
    std::array<double, pointCount> feedbackAt {};
    std::array<double, pointCount> headroomAt {};
    // A settled interval -- both endpoints equal, no hold trajectory -- is
    // most of what an instrument does, and there the node interpolation
    // below is arithmetically the identity: a + p * (b - a) with b == a is
    // exactly a for every finite a. Broadcasting the endpoint is therefore
    // not an approximation, just the same values without the per-node
    // arithmetic.
    const bool settledControls = trajectory == nullptr
        && previousOmegaStep == currentOmega
        && previousFeedback == currentFeedback
        && previousHeadroom == currentHeadroom;
    const double settledHeadroom = std::max(currentHeadroom, 1.0e-5);
    for (std::size_t point = 0; point < pointCount; ++point)
    {
        if ((nodeMask >> point & 1u) == 0u)
            continue;
#if defined(YOUKNOW106_WORK_AUDIT)
        YOUKNOW106_COUNT_DOMAIN_WORK(vcfInputReconstructions, 1);
#endif
        if (inputHistoryCount == 0)
            inputAt[point] = nodes[point].linear[0] * currentInput
                           + nodes[point].linear[1] * inputHistory[0];
        else if (inputHistoryCount == 1)
            inputAt[point] = nodes[point].quadratic[0] * currentInput
                + nodes[point].quadratic[1] * inputHistory[0]
                + nodes[point].quadratic[2] * inputHistory[1];
        else
            inputAt[point] = nodes[point].cubic[0] * currentInput
                + nodes[point].cubic[1] * inputHistory[0]
                + nodes[point].cubic[2] * inputHistory[1]
                + nodes[point].cubic[3] * inputHistory[2];
        if (settledControls)
        {
            omegaAt[point] = currentOmega;
            feedbackAt[point] = currentFeedback;
            headroomAt[point] = settledHeadroom;
        }
        else if (trajectory != nullptr)
        {
            omegaAt[point] = clampOmegaStep(
                trajectory->omegaStep[point]);
            feedbackAt[point] = std::clamp(
                std::isfinite(trajectory->feedback[point])
                    ? trajectory->feedback[point] : 0.0,
                0.0, 8.0);
            headroomAt[point] = std::max(
                std::isfinite(trajectory->headroom[point])
                    ? trajectory->headroom[point] : 0.0,
                1.0e-5);
        }
        else
        {
            const double position = nodes[point].position;
            omegaAt[point] = previousOmegaStep
                + position * (currentOmega - previousOmegaStep);
            feedbackAt[point] = previousFeedback
                + position * (currentFeedback - previousFeedback);
            headroomAt[point] = std::max(
                previousHeadroom
                    + position * (currentHeadroom - previousHeadroom),
                1.0e-5);
        }
    }

    const auto advanceOne = [](const std::array<double, 4>& origin,
                               const std::array<double, 4>& slope,
                               double distance) {
        std::array<double, 4> result {};
        for (std::size_t stage = 0; stage < result.size(); ++stage)
            result[stage] = origin[stage] + distance * slope[stage];
        return result;
    };
    const auto advanceTwo = [](const std::array<double, 4>& origin,
                               double step,
                               const std::array<double, 4>& a, double wa,
                               const std::array<double, 4>& b, double wb) {
        std::array<double, 4> result {};
        for (std::size_t stage = 0; stage < result.size(); ++stage)
            result[stage] = origin[stage] + step
                * (wa * a[stage] + wb * b[stage]);
        return result;
    };
    const auto advanceThree = [](const std::array<double, 4>& origin,
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
    const auto advanceFour = [](const std::array<double, 4>& origin,
                                double step,
                                const std::array<double, 4>& a, double wa,
                                const std::array<double, 4>& b, double wb,
                                const std::array<double, 4>& c, double wc,
                                const std::array<double, 4>& d, double wd) {
        std::array<double, 4> result {};
        for (std::size_t stage = 0; stage < result.size(); ++stage)
            result[stage] = origin[stage] + step
                * (wa * a[stage] + wb * b[stage] + wc * c[stage]
                   + wd * d[stage]);
        return result;
    };

    const bool applyEarlyEffect = enableEarlyEffect
                               && currentCalibration > 0.0;
    const double earlyAmount =
        static_cast<double>(otaEarlyEffectCoefficient) * currentCalibration;
    std::array<double, 4> stageScale {};
    std::array<double, 4> stageOffset {};
    for (std::size_t stage = 0; stage < stageScale.size(); ++stage)
    {
        stageScale[stage] = static_cast<double>(gScale[stage]);
        stageOffset[stage] = static_cast<double>(offsetVoltage[stage]);
    }
    std::array<std::array<double, 4>, pointCount> stageOmegaAt {};
    for (std::size_t point = 0; point < pointCount; ++point)
    {
        if ((nodeMask >> point & 1u) == 0u)
            continue;
        for (std::size_t stage = 0; stage < stageScale.size(); ++stage)
            stageOmegaAt[point][stage] = omegaAt[point] * stageScale[stage];
    }

    // The tableau walks are shared by every right-hand side: they advance
    // `state` through the closure with whichever derivative the dispatch
    // below built, so the ladder exists once rather than once per kernel.
    const auto integrateSteps = [&]<Tableau tableau>(
                                    const auto& derivative) {
        if constexpr (tableau == Tableau::MersonHalf)
        {
            for (int step = 0; step < integrationSubsteps; ++step)
            {
#if defined(YOUKNOW106_WORK_AUDIT)
                YOUKNOW106_COUNT_DOMAIN_WORK(vcfIntegrationSubsteps, 1);
#endif
                const std::size_t origin = static_cast<std::size_t>(3 * step);
                const auto k1 = derivative(state, inputAt[origin], origin);
                const auto k2 = derivative(
                    advanceOne(state, k1, substep / 3.0),
                    inputAt[origin + 1u], origin + 1u);
                const auto k3 = derivative(
                    advanceTwo(state, substep, k1, 1.0 / 6.0,
                               k2, 1.0 / 6.0),
                    inputAt[origin + 1u], origin + 1u);
                const auto k4 = derivative(
                    advanceTwo(state, substep, k1, 1.0 / 8.0,
                               k3, 3.0 / 8.0),
                    inputAt[origin + 2u], origin + 2u);
                const auto k5 = derivative(
                    advanceThree(state, substep, k1, 1.0 / 2.0,
                                 k3, -3.0 / 2.0, k4, 2.0),
                    inputAt[origin + 3u], origin + 3u);
                state = advanceThree(state, substep, k1, 1.0 / 6.0,
                                     k4, 2.0 / 3.0, k5, 1.0 / 6.0);
            }
        }
        else
        {
            // Classic RK4: one node at the start of the sub-interval, two
            // at its midpoint and one at its end. The half-step rung walks
            // that shape twice over the established 0/1/4/1/2 and 1/2/3/4/1
            // ordinals; the full-interval rung walks it once over 0/1/2/1.
            constexpr bool halfSteps = tableau == Tableau::Rk4Half;
            constexpr int steps = halfSteps ? 2 : 1;
            constexpr double stepSize = halfSteps ? 0.5 : 1.0;
            for (int step = 0; step < steps; ++step)
            {
#if defined(YOUKNOW106_WORK_AUDIT)
                YOUKNOW106_COUNT_DOMAIN_WORK(vcfIntegrationSubsteps, 1);
#endif
                const std::size_t start = halfSteps
                    ? static_cast<std::size_t>(3 * step) : 0u;
                const std::size_t middle = halfSteps
                    ? static_cast<std::size_t>(2 + 3 * step) : 3u;
                const std::size_t end = halfSteps
                    ? static_cast<std::size_t>(3 + 3 * step) : 6u;
                const auto k1 = derivative(state, inputAt[start], start);
                const auto k2 = derivative(
                    advanceOne(state, k1, 0.5 * stepSize),
                    inputAt[middle], middle);
                const auto k3 = derivative(
                    advanceOne(state, k2, 0.5 * stepSize),
                    inputAt[middle], middle);
                const auto k4 = derivative(
                    advanceOne(state, k3, stepSize), inputAt[end], end);
                state = advanceFour(state, stepSize,
                                    k1, 1.0 / 6.0, k2, 1.0 / 3.0,
                                    k3, 1.0 / 3.0, k4, 1.0 / 6.0);
            }
        }
    };

    // Fast amortizes normalization over the nodes this tableau actually reads
    // -- seven on the default rung, five or three on the cheaper ones. In the
    // Character-on path, those reciprocals replace 90 RHS divisions per card
    // interval on the default rung. Exact keeps the established division
    // expressions and their frozen rounding behavior.
    const auto integrate = [&]<bool useReciprocal, Tableau tableau>(
                               const auto& nonlinear) {
        std::array<double, pointCount> inverseHeadroomAt {};
        if constexpr (useReciprocal)
            for (std::size_t point = 0; point < pointCount; ++point)
                if ((tableauNodeMask(tableau) >> point & 1u) != 0u)
                    inverseHeadroomAt[point] = 1.0 / headroomAt[point];

        // Four doubles are an arm64 HFA: by value keeps RK states in d0-d3;
        // a const reference forces every temporary through addressable memory.
        const auto derivative = [&](std::array<double, 4> value,
                                    double drive, std::size_t point) {
            std::array<double, 4> result {};
            const double runningHeadroom = headroomAt[point];
            const auto normalise = [&](double input) {
                if constexpr (useReciprocal)
                    return input * inverseHeadroomAt[point];
                return input / runningHeadroom;
            };
#if defined(YOUKNOW106_WORK_AUDIT)
            YOUKNOW106_COUNT_DOMAIN_WORK(vcfRhsEvaluations, 1);
            YOUKNOW106_COUNT_DOMAIN_WORK(vcfFeedbackEvaluations, 1);
            if (applyEarlyEffect)
                YOUKNOW106_COUNT_DOMAIN_WORK(vcfEarlyEvaluations,
                                             result.size());
#endif
            const double feedbackArgument = [&] {
                if constexpr (useReciprocal)
                    return value[3] * (1.0 / feedbackHeadroom);
                return value[3] / feedbackHeadroom;
            }();
            double previous = drive - feedbackAt[point] * feedbackHeadroom
                * nonlinear(feedbackArgument);
            for (std::size_t stage = 0; stage < result.size(); ++stage)
            {
#if defined(YOUKNOW106_WORK_AUDIT)
                YOUKNOW106_COUNT_DOMAIN_WORK(vcfStageEvaluations, 1);
#endif
                const double early = [&] {
                    if (!applyEarlyEffect)
                        return 1.0;
                    if constexpr (useCubicEarly)
                        return 1.0 + earlyAmount
                            * cubicEarlyTanh(normalise(value[stage]));
                    return 1.0 + earlyAmount
                        * nonlinear(normalise(value[stage]));
                }();
                result[stage] = stageOmegaAt[point][stage]
                    * early * runningHeadroom
                    * nonlinear(normalise(
                        previous - value[stage]
                        + stageOffset[stage]));
                previous = value[stage];
            }
            return result;
        };

        integrateSteps.template operator()<tableau>(derivative);
    };

    // The PolyZoned kernel: the same derivative expressions with the four
    // stage nonlinearities batched. Their arguments depend only on the state
    // vector, never on each other's outputs, so the inner polynomial can be
    // evaluated unconditionally across the batch -- branch-free and
    // load-free, which the scalar zoned kernel cannot be -- and the rare
    // out-of-zone lane is patched afterwards through the established Hermite
    // tables. The feedback return stays scalar: stage zero's argument needs
    // its result.
    const auto integratePoly = [&]<Tableau tableau> {
        std::array<double, pointCount> inverseHeadroomAt {};
        for (std::size_t point = 0; point < pointCount; ++point)
            if ((tableauNodeMask(tableau) >> point & 1u) != 0u)
                inverseHeadroomAt[point] = 1.0 / headroomAt[point];

        const auto derivative = [&](std::array<double, 4> value,
                                    double drive, std::size_t point) {
            const double inverseHeadroom = inverseHeadroomAt[point];
            const double runningHeadroom = headroomAt[point];
#if defined(YOUKNOW106_WORK_AUDIT)
            YOUKNOW106_COUNT_DOMAIN_WORK(vcfRhsEvaluations, 1);
            YOUKNOW106_COUNT_DOMAIN_WORK(vcfFeedbackEvaluations, 1);
            YOUKNOW106_COUNT_DOMAIN_WORK(vcfStageEvaluations, 4);
            if (applyEarlyEffect)
                YOUKNOW106_COUNT_DOMAIN_WORK(vcfEarlyEvaluations, 4);
#endif
            // With the resonance loop open the return term is exactly zero
            // whatever the fourth capacitor holds, and its evaluation is the
            // one nonlinearity stage zero's argument has to wait for -- so
            // an open loop skips it rather than computing a value only to
            // multiply it away. Identical arithmetic either way.
            const double loopReturn = feedbackAt[point] == 0.0
                ? drive
                : drive - feedbackAt[point] * feedbackHeadroom
                    * polyZonedTanhImpl(value[3] * (1.0 / feedbackHeadroom));

            const std::array<double, 4> stageArg {
                (loopReturn - value[0] + stageOffset[0]) * inverseHeadroom,
                (value[0] - value[1] + stageOffset[1]) * inverseHeadroom,
                (value[1] - value[2] + stageOffset[2]) * inverseHeadroom,
                (value[2] - value[3] + stageOffset[3]) * inverseHeadroom
            };
            const std::array<double, 4> stageTanh =
                polyZonedTanhBatch(stageArg);

            std::array<double, 4> early {};
            if (!applyEarlyEffect)
                early.fill(1.0);
            else if constexpr (useCubicEarly)
                for (std::size_t stage = 0; stage < early.size(); ++stage)
                    early[stage] = 1.0 + earlyAmount
                        * cubicEarlyTanh(value[stage] * inverseHeadroom);
            else
            {
                const std::array<double, 4> earlyTanh = polyZonedTanhBatch({
                    value[0] * inverseHeadroom, value[1] * inverseHeadroom,
                    value[2] * inverseHeadroom, value[3] * inverseHeadroom });
                for (std::size_t stage = 0; stage < early.size(); ++stage)
                    early[stage] = 1.0 + earlyAmount * earlyTanh[stage];
            }

            std::array<double, 4> result {};
            for (std::size_t stage = 0; stage < result.size(); ++stage)
                result[stage] = stageOmegaAt[point][stage]
                    * early[stage] * runningHeadroom * stageTanh[stage];
            return result;
        };

        integrateSteps.template operator()<tableau>(derivative);
    };

    // One switch per interval over a value that is constant for the whole
    // parameter snapshot. Each arm instantiates only the integration shell;
    // the derivative it calls is shared, so the ladder does not clone the hot
    // right-hand side -- an experiment that did clone one measured 6% slower.
    const auto integrateWithTableau = [&]<bool useReciprocal>(
                                          const auto& nonlinear) {
        switch (plannedTableau)
        {
            case Tableau::Rk4Half:
                integrate.template operator()<useReciprocal, Tableau::Rk4Half>(
                    nonlinear);
                return;
            case Tableau::Rk4Full:
                integrate.template operator()<useReciprocal, Tableau::Rk4Full>(
                    nonlinear);
                return;
            case Tableau::MersonHalf:
                break;
        }
        integrate.template operator()<useReciprocal, Tableau::MersonHalf>(
            nonlinear);
    };

    if (tanhMode == VcfTanhMode::PolyZoned)
        switch (plannedTableau)
        {
            case Tableau::Rk4Half:
                integratePoly.template operator()<Tableau::Rk4Half>();
                break;
            case Tableau::Rk4Full:
                integratePoly.template operator()<Tableau::Rk4Full>();
                break;
            case Tableau::MersonHalf:
            default:
                integratePoly.template operator()<Tableau::MersonHalf>();
                break;
        }
    else if constexpr (useCubicEarly)
        integrateWithTableau.template operator()<true>(
            [](double value) noexcept {
                return zonedHermiteTanhImpl(value);
            });
    else
        switch (tanhMode)
        {
            case VcfTanhMode::ZonedHermite:
                integrateWithTableau.template operator()<true>(
                    [](double value) noexcept {
                        return zonedHermiteTanhImpl(value);
                    });
                break;
            case VcfTanhMode::Exact:
            default:
                integrateWithTableau.template operator()<false>(
                    [](double value) noexcept {
                        return std::tanh(value);
                    });
                break;
        }

    for (std::size_t point = inputHistory.size() - 1u; point > 0u; --point)
        inputHistory[point] = inputHistory[point - 1u];
    inputHistory[0] = currentInput;
    inputHistoryCount = std::min(inputHistoryCount + 1, 2);
    previousOmegaStep = currentOmega;
    previousFeedback = currentFeedback;
    previousHeadroom = currentHeadroom;

    // The comparison also rejects NaNs and infinities.  A separate isfinite
    // predicate was redundant and measurably enlarged this hot loop.
    const bool valid = std::all_of(
        state.begin(), state.end(), [](double value) {
            return std::abs(value) <= 64.0;
        });
    if (!valid)
    {
#if defined(YOUKNOW106_WORK_AUDIT)
        YOUKNOW106_COUNT_DOMAIN_WORK(vcfRecoveries, 1);
#endif
        state.fill(0.0);
        inputHistory.fill(currentInput);
        inputHistoryCount = 2;
        return 0.0f;
    }
    return static_cast<float>(state[3]);
}

#if defined(YOUKNOW106_HAS_VCF_PAIR_SIMD)
bool YouKnow106Engine::OtaCascade::tryProcessSettledRk4Pair(
    OtaCascade& first, float firstInput, float firstOmegaStep,
    float firstFeedback, float firstHeadroom,
    OtaCascade& second, float secondInput, float secondOmegaStep,
    float secondFeedback, float secondHeadroom,
    bool enableEarlyEffect, float calibration,
    float& firstOutput, float& secondOutput) noexcept
{
    constexpr double feedbackHeadroom =
        VoicedResonanceCompatibilityProfile::loopHeadroomVolts;
    const double currentCalibration = std::clamp(
        std::isfinite(calibration) ? static_cast<double>(calibration) : 0.0,
        0.0, static_cast<double>(EngineParameters::calibrationCeiling));
    const bool applyEarlyEffect = enableEarlyEffect
                               && currentCalibration > 0.0;
    const double earlyAmount =
        static_cast<double>(otaEarlyEffectCoefficient) * currentCalibration;
    const double earlyCeiling = applyEarlyEffect ? 1.0 + earlyAmount : 1.0;

    struct Lane
    {
        OtaCascade* cascade {};
        double input {};
        double omega {};
        double feedback {};
        double headroom {};
        double inverseHeadroom {};
        Tableau tableau { Tableau::MersonHalf };
        std::array<double, 5> drive {};
        std::array<double, 4> stageOmega {};
        std::array<double, 4> stageOffset {};
    };

    const auto prepareLane = [&](OtaCascade& cascade, float input,
                                 float omegaStep, float feedback,
                                 float headroom, Lane& lane) {
        const double currentOmega = clampOmegaStep(
            static_cast<double>(omegaStep));
        const double currentFeedback = std::clamp(
            std::isfinite(feedback) ? static_cast<double>(feedback) : 0.0,
            0.0, 8.0);
        const double currentHeadroom = std::max(
            std::isfinite(headroom) ? static_cast<double>(headroom) : 0.0,
            1.0e-5);
        if (!cascade.parameterHistoryPrimed
            || cascade.inputHistoryCount != 2
            || cascade.previousOmegaStep != currentOmega
            || cascade.previousFeedback != currentFeedback
            || cascade.previousHeadroom != currentHeadroom
            || !std::all_of(
                cascade.state.begin(), cascade.state.end(), [](double value) {
                    return std::abs(value) <= 64.0;
                }))
            return false;

        double largestScale = 0.0;
        for (const float scale : cascade.gScale)
            largestScale = std::max(
                largestScale, static_cast<double>(std::abs(scale)));
        const Tableau tableau = planTableau(
            VcfSolverMode::Rk4Single,
            currentOmega * largestScale * earlyCeiling, currentFeedback);
        if (tableau == Tableau::MersonHalf)
            return false;

        lane.cascade = &cascade;
        lane.input = std::isfinite(input) ? static_cast<double>(input) : 0.0;
        lane.omega = currentOmega;
        lane.feedback = currentFeedback;
        lane.headroom = currentHeadroom;
        lane.inverseHeadroom = 1.0 / currentHeadroom;
        lane.tableau = tableau;

        const auto reconstruct = [&](double currentWeight,
                                     double firstWeight,
                                     double secondWeight,
                                     double thirdWeight) {
            return currentWeight * lane.input
                + firstWeight * cascade.inputHistory[0]
                + secondWeight * cascade.inputHistory[1]
                + thirdWeight * cascade.inputHistory[2];
        };
        // Both RK4 tableaux read 0, 1/2 and 1. The half-step pair alone also
        // reads 1/4 and 3/4. These are the identical cubic weights produced
        // by `makeNodes` in `process`.
        lane.drive[0] = reconstruct(0.0,    1.0,    0.0,     0.0);
        lane.drive[2] = reconstruct(0.3125, 0.9375, -0.3125, 0.0625);
        lane.drive[4] = reconstruct(1.0,    0.0,    0.0,     0.0);
        if (tableau == Tableau::Rk4Half)
        {
            lane.drive[1] = reconstruct(
                0.1171875, 1.0546875, -0.2109375, 0.0390625);
            lane.drive[3] = reconstruct(
                0.6015625, 0.6015625, -0.2578125, 0.0546875);
        }
        for (std::size_t stage = 0; stage < lane.stageOmega.size(); ++stage)
        {
            lane.stageOmega[stage] = currentOmega
                * static_cast<double>(cascade.gScale[stage]);
            lane.stageOffset[stage] =
                static_cast<double>(cascade.offsetVoltage[stage]);
        }
        return true;
    };

    Lane lanes[2];
    if (!prepareLane(first, firstInput, firstOmegaStep, firstFeedback,
                     firstHeadroom, lanes[0])
        || !prepareLane(second, secondInput, secondOmegaStep, secondFeedback,
                        secondHeadroom, lanes[1]))
        return false;
    if (lanes[0].tableau != lanes[1].tableau)
        return false;

#if defined(__aarch64__) && defined(__ARM_NEON)
    using Pair = float64x2_t;
    const auto pack = [](double low, double high) {
        return vsetq_lane_f64(high, vdupq_n_f64(low), 1);
    };
#define pairLow(value) vgetq_lane_f64((value), 0)
#define pairHigh(value) vgetq_lane_f64((value), 1)
#define pairSplat(value) vdupq_n_f64(value)
#define pairAdd(first, second) vaddq_f64((first), (second))
#define pairSubtract(first, second) vsubq_f64((first), (second))
#define pairMultiply(first, second) vmulq_f64((first), (second))
#define pairMultiplyScalar(value, scalar) vmulq_n_f64((value), (scalar))
#define pairMultiplyAdd(addend, first, second) \
    vfmaq_f64((addend), (first), (second))
#define pairMultiplyAddScalar(addend, value, scalar) \
    vfmaq_n_f64((addend), (value), (scalar))
#elif defined(__x86_64__) && defined(__SSE2__)
    using Pair = __m128d;
    const auto pack = [](double low, double high) {
        return _mm_set_pd(high, low);
    };
#define pairLow(value) _mm_cvtsd_f64(value)
#define pairHigh(value) _mm_cvtsd_f64(_mm_unpackhi_pd((value), (value)))
#define pairSplat(value) _mm_set1_pd(value)
#define pairAdd(first, second) _mm_add_pd((first), (second))
#define pairSubtract(first, second) _mm_sub_pd((first), (second))
#define pairMultiply(first, second) _mm_mul_pd((first), (second))
#define pairMultiplyScalar(value, scalar) \
    _mm_mul_pd((value), _mm_set1_pd(scalar))
    // Baseline x86_64 has SSE2 but not FMA. Keep the scalar path's separate
    // multiply and add rounding rather than changing the solve along with its
    // scheduling.
#define pairMultiplyAdd(addend, first, second) \
    _mm_add_pd((addend), _mm_mul_pd((first), (second)))
#define pairMultiplyAddScalar(addend, value, scalar) \
    _mm_add_pd((addend), _mm_mul_pd((value), _mm_set1_pd(scalar)))
#endif
    using PairState = std::array<Pair, 4>;
    const auto polyTanhPair = [&](Pair value) {
        const Pair u = pairMultiply(value, value);
        if (pairLow(u) >= 1.0 || pairHigh(u) >= 1.0)
            return pack(polyZonedTanhImpl(pairLow(value)),
                        polyZonedTanhImpl(pairHigh(value)));

        const Pair uSquared = pairMultiply(u, u);
        const Pair top = pairMultiplyAddScalar(
            pairSplat(0.016720855179576926), u,
            -0.00305822903759879);
        const Pair high = pairMultiplyAddScalar(
            pairSplat(0.1328072064598552), u,
            -0.051585988404218436);
        const Pair low = pairMultiplyAddScalar(
            pairSplat(0.9999993948006496), u,
            -0.3332895137021704);
        const Pair upper = pairMultiplyAdd(high, top, uSquared);
        const Pair factor = pairMultiplyAdd(low, upper, uSquared);
        return pairMultiply(value, factor);
    };
    const auto cubicEarlyPair = [&](Pair value) {
        if (std::abs(pairLow(value)) >= 1.5
            || std::abs(pairHigh(value)) >= 1.5)
            return pack(cubicEarlyTanh(pairLow(value)),
                        cubicEarlyTanh(pairHigh(value)));

        const Pair scaled = pairMultiplyScalar(value, -(4.0 / 27.0));
        const Pair factor = pairMultiplyAdd(pairSplat(1.0), scaled, value);
        return pairMultiply(value, factor);
    };

    PairState state;
    PairState stageOmega;
    PairState stageOffset;
    for (std::size_t stage = 0; stage < state.size(); ++stage)
    {
        state[stage] = pack(first.state[stage], second.state[stage]);
        stageOmega[stage] = pack(lanes[0].stageOmega[stage],
                                 lanes[1].stageOmega[stage]);
        stageOffset[stage] = pack(lanes[0].stageOffset[stage],
                                  lanes[1].stageOffset[stage]);
    }
    const Pair inverseHeadroom = pack(lanes[0].inverseHeadroom,
                                      lanes[1].inverseHeadroom);
    const Pair runningHeadroom = pack(lanes[0].headroom, lanes[1].headroom);

    const auto derivative = [&](const PairState& value, Pair drive) {
        const Pair feedbackArgument = pairMultiplyScalar(
            value[3], 1.0 / feedbackHeadroom);
        const Pair feedbackTanh = polyTanhPair(feedbackArgument);
        const double firstLoopReturn = lanes[0].feedback == 0.0
            ? pairLow(drive)
            : pairLow(drive)
                - lanes[0].feedback * feedbackHeadroom
                    * pairLow(feedbackTanh);
        const double secondLoopReturn = lanes[1].feedback == 0.0
            ? pairHigh(drive)
            : pairHigh(drive)
                - lanes[1].feedback * feedbackHeadroom
                    * pairHigh(feedbackTanh);
        const Pair loopReturn = pack(firstLoopReturn, secondLoopReturn);

        PairState stageArgument {
            pairMultiply(pairAdd(pairSubtract(loopReturn, value[0]),
                                 stageOffset[0]), inverseHeadroom),
            pairMultiply(pairAdd(pairSubtract(value[0], value[1]),
                                 stageOffset[1]), inverseHeadroom),
            pairMultiply(pairAdd(pairSubtract(value[1], value[2]),
                                 stageOffset[2]), inverseHeadroom),
            pairMultiply(pairAdd(pairSubtract(value[2], value[3]),
                                 stageOffset[3]), inverseHeadroom)
        };
        PairState stageTanh;
        for (std::size_t stage = 0; stage < stageTanh.size(); ++stage)
            stageTanh[stage] = polyTanhPair(stageArgument[stage]);

        PairState early;
        if (!applyEarlyEffect)
        {
            for (auto& valueAtStage : early)
                valueAtStage = pairSplat(1.0);
        }
        else
        {
            for (std::size_t stage = 0; stage < early.size(); ++stage)
            {
                const Pair earlyTanh = cubicEarlyPair(
                    pairMultiply(value[stage], inverseHeadroom));
                early[stage] = pairMultiplyAddScalar(
                    pairSplat(1.0), earlyTanh, earlyAmount);
            }
        }

        PairState result;
        for (std::size_t stage = 0; stage < result.size(); ++stage)
        {
            result[stage] = pairMultiply(stageOmega[stage], early[stage]);
            result[stage] = pairMultiply(result[stage], runningHeadroom);
            result[stage] = pairMultiply(result[stage], stageTanh[stage]);
        }
        return result;
    };
    const auto advanceOne = [](const PairState& origin,
                               const PairState& slope, double distance) {
        PairState result;
        for (std::size_t stage = 0; stage < result.size(); ++stage)
            result[stage] = pairMultiplyAddScalar(
                origin[stage], slope[stage], distance);
        return result;
    };
    const auto finishRk4 = [](const PairState& origin,
                              const PairState& k1,
                              const PairState& k2,
                              const PairState& k3,
                              const PairState& k4, double stepSize) {
        PairState result;
        for (std::size_t stage = 0; stage < result.size(); ++stage)
        {
            Pair weighted = pairMultiplyScalar(k2[stage], 1.0 / 3.0);
            weighted = pairMultiplyAddScalar(
                weighted, k1[stage], 1.0 / 6.0);
            weighted = pairMultiplyAddScalar(
                weighted, k3[stage], 1.0 / 3.0);
            weighted = pairMultiplyAddScalar(
                weighted, k4[stage], 1.0 / 6.0);
            result[stage] = pairMultiplyAddScalar(
                origin[stage], weighted, stepSize);
        }
        return result;
    };

    const auto advanceRk4 = [&](std::size_t start, std::size_t middle,
                                std::size_t end, double stepSize) {
        const PairState origin = state;
        const Pair k1Drive = pack(lanes[0].drive[start],
                                  lanes[1].drive[start]);
        const Pair middleDrive = pack(lanes[0].drive[middle],
                                      lanes[1].drive[middle]);
        const Pair endDrive = pack(lanes[0].drive[end],
                                   lanes[1].drive[end]);
        const PairState k1 = derivative(origin, k1Drive);
        const PairState k2 = derivative(
            advanceOne(origin, k1, 0.5 * stepSize), middleDrive);
        const PairState k3 = derivative(
            advanceOne(origin, k2, 0.5 * stepSize), middleDrive);
        const PairState k4 = derivative(
            advanceOne(origin, k3, stepSize), endDrive);
        state = finishRk4(origin, k1, k2, k3, k4, stepSize);
    };
    if (lanes[0].tableau == Tableau::Rk4Full)
        advanceRk4(0u, 2u, 4u, 1.0);
    else
    {
        advanceRk4(0u, 1u, 2u, 0.5);
        advanceRk4(2u, 3u, 4u, 0.5);
    }

    for (std::size_t stage = 0; stage < state.size(); ++stage)
    {
        first.state[stage] = pairLow(state[stage]);
        second.state[stage] = pairHigh(state[stage]);
    }

#undef pairLow
#undef pairHigh
#undef pairSplat
#undef pairAdd
#undef pairSubtract
#undef pairMultiply
#undef pairMultiplyScalar
#undef pairMultiplyAdd
#undef pairMultiplyAddScalar

    const auto finishLane = [](Lane& lane, float& output) {
        auto& cascade = *lane.cascade;
        for (std::size_t point = cascade.inputHistory.size() - 1u;
             point > 0u; --point)
            cascade.inputHistory[point] = cascade.inputHistory[point - 1u];
        cascade.inputHistory[0] = lane.input;
        cascade.inputHistoryCount = std::min(cascade.inputHistoryCount + 1, 2);
        cascade.previousOmegaStep = lane.omega;
        cascade.previousFeedback = lane.feedback;
        cascade.previousHeadroom = lane.headroom;

        const bool valid = std::all_of(
            cascade.state.begin(), cascade.state.end(), [](double value) {
                return std::abs(value) <= 64.0;
            });
        if (!valid)
        {
            cascade.state.fill(0.0);
            cascade.inputHistory.fill(lane.input);
            cascade.inputHistoryCount = 2;
            output = 0.0f;
            return;
        }
        output = static_cast<float>(cascade.state[3]);
    };
    finishLane(lanes[0], firstOutput);
    finishLane(lanes[1], secondOutput);
    return true;
}

#if defined(_MSC_VER)
#define YOUKNOW106_MERSON_NOINLINE __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
#define YOUKNOW106_MERSON_NOINLINE __attribute__((noinline))
#else
#define YOUKNOW106_MERSON_NOINLINE
#endif
YOUKNOW106_MERSON_NOINLINE
bool YouKnow106Engine::OtaCascade::tryProcessSettledMersonPair(
    OtaCascade& first, float firstInput, float firstOmegaStep,
    float firstFeedback, float firstHeadroom,
    OtaCascade& second, float secondInput, float secondOmegaStep,
    float secondFeedback, float secondHeadroom,
    bool enableEarlyEffect, float calibration,
    float& firstOutput, float& secondOutput) noexcept
{
    constexpr double feedbackHeadroom =
        VoicedResonanceCompatibilityProfile::loopHeadroomVolts;
    const double currentCalibration = std::clamp(
        std::isfinite(calibration) ? static_cast<double>(calibration) : 0.0,
        0.0, static_cast<double>(EngineParameters::calibrationCeiling));
    const bool applyEarlyEffect = enableEarlyEffect
                               && currentCalibration > 0.0;
    const double earlyAmount =
        static_cast<double>(otaEarlyEffectCoefficient) * currentCalibration;
    const double earlyCeiling = applyEarlyEffect ? 1.0 + earlyAmount : 1.0;

    struct Lane
    {
        OtaCascade* cascade {};
        double input {};
        double omega {};
        double feedback {};
        double headroom {};
        double inverseHeadroom {};
        std::array<double, 7> drive {};
        std::array<double, 4> stageOmega {};
        std::array<double, 4> stageOffset {};
    };

    const auto prepareLane = [&](OtaCascade& cascade, float input,
                                 float omegaStep, float feedback,
                                 float headroom, Lane& lane) {
        const double currentOmega = clampOmegaStep(
            static_cast<double>(omegaStep));
        const double currentFeedback = std::clamp(
            std::isfinite(feedback) ? static_cast<double>(feedback) : 0.0,
            0.0, 8.0);
        const double currentHeadroom = std::max(
            std::isfinite(headroom) ? static_cast<double>(headroom) : 0.0,
            1.0e-5);
        if (!cascade.parameterHistoryPrimed
            || cascade.inputHistoryCount != 2
            || cascade.previousOmegaStep != currentOmega
            || cascade.previousFeedback != currentFeedback
            || cascade.previousHeadroom != currentHeadroom
            || !std::all_of(
                cascade.state.begin(), cascade.state.end(), [](double value) {
                    return std::abs(value) <= 64.0;
                }))
            return false;

        double largestScale = 0.0;
        for (const float scale : cascade.gScale)
            largestScale = std::max(
                largestScale, static_cast<double>(std::abs(scale)));
        if (planTableau(
                VcfSolverMode::Rk4Single,
                currentOmega * largestScale * earlyCeiling,
                currentFeedback) != Tableau::MersonHalf)
            return false;

        lane.cascade = &cascade;
        lane.input = std::isfinite(input) ? static_cast<double>(input) : 0.0;
        lane.omega = currentOmega;
        lane.feedback = currentFeedback;
        lane.headroom = currentHeadroom;
        lane.inverseHeadroom = 1.0 / currentHeadroom;

        const auto reconstruct = [&](double currentWeight,
                                     double firstWeight,
                                     double secondWeight,
                                     double thirdWeight) {
            return currentWeight * lane.input
                + firstWeight * cascade.inputHistory[0]
                + secondWeight * cascade.inputHistory[1]
                + thirdWeight * cascade.inputHistory[2];
        };
        // These are the seven causal-cubic values produced by `makeNodes` in
        // `process`, in Merson's 0, 1/6, 1/4, 1/2, 2/3, 3/4 and 1 order.
        lane.drive[0] = reconstruct(0.0,    1.0,    0.0,     0.0);
        lane.drive[1] = reconstruct(
            0x1.1f9add3c0ca45p-4, 0x1.0da12f684bda1p+0,
            -0x1.3425ed097b426p-3, 0x1.ba781948b0fcfp-6);
        lane.drive[2] = reconstruct(
            0.1171875, 1.0546875, -0.2109375, 0.0390625);
        lane.drive[3] = reconstruct(
            0.3125, 0.9375, -0.3125, 0.0625);
        lane.drive[4] = reconstruct(
            0x1.f9add3c0ca457p-2, 0x1.7b425ed097b42p-1,
            -0x1.2f684bda12f68p-2, 0x1.f9add3c0ca458p-5);
        lane.drive[5] = reconstruct(
            0.6015625, 0.6015625, -0.2578125, 0.0546875);
        lane.drive[6] = reconstruct(1.0,    0.0,    0.0,     0.0);
        for (std::size_t stage = 0; stage < lane.stageOmega.size(); ++stage)
        {
            lane.stageOmega[stage] = currentOmega
                * static_cast<double>(cascade.gScale[stage]);
            lane.stageOffset[stage] =
                static_cast<double>(cascade.offsetVoltage[stage]);
        }
        return true;
    };

    Lane lanes[2];
    if (!prepareLane(first, firstInput, firstOmegaStep, firstFeedback,
                     firstHeadroom, lanes[0])
        || !prepareLane(second, secondInput, secondOmegaStep, secondFeedback,
                        secondHeadroom, lanes[1]))
        return false;

#if defined(__aarch64__) && defined(__ARM_NEON)
    using Pair = float64x2_t;
    const auto pack = [](double low, double high) {
        return vsetq_lane_f64(high, vdupq_n_f64(low), 1);
    };
#define pairLow(value) vgetq_lane_f64((value), 0)
#define pairHigh(value) vgetq_lane_f64((value), 1)
#define pairSplat(value) vdupq_n_f64(value)
#define pairAdd(first, second) vaddq_f64((first), (second))
#define pairSubtract(first, second) vsubq_f64((first), (second))
#define pairMultiply(first, second) vmulq_f64((first), (second))
#define pairMultiplyScalar(value, scalar) vmulq_n_f64((value), (scalar))
#define pairMultiplyAdd(addend, first, second) \
    vfmaq_f64((addend), (first), (second))
#define pairMultiplyAddScalar(addend, value, scalar) \
    vfmaq_n_f64((addend), (value), (scalar))
#elif defined(__x86_64__) && defined(__SSE2__)
    using Pair = __m128d;
    const auto pack = [](double low, double high) {
        return _mm_set_pd(high, low);
    };
#define pairLow(value) _mm_cvtsd_f64(value)
#define pairHigh(value) _mm_cvtsd_f64(_mm_unpackhi_pd((value), (value)))
#define pairSplat(value) _mm_set1_pd(value)
#define pairAdd(first, second) _mm_add_pd((first), (second))
#define pairSubtract(first, second) _mm_sub_pd((first), (second))
#define pairMultiply(first, second) _mm_mul_pd((first), (second))
#define pairMultiplyScalar(value, scalar) \
    _mm_mul_pd((value), _mm_set1_pd(scalar))
    // Baseline x86_64 has SSE2 but not FMA. Keep the scalar path's separate
    // multiply and add rounding rather than changing the solve along with its
    // scheduling.
#define pairMultiplyAdd(addend, first, second) \
    _mm_add_pd((addend), _mm_mul_pd((first), (second)))
#define pairMultiplyAddScalar(addend, value, scalar) \
    _mm_add_pd((addend), _mm_mul_pd((value), _mm_set1_pd(scalar)))
#endif
    using PairState = std::array<Pair, 4>;
    const auto polyTanhPair = [&](Pair value) {
        const Pair u = pairMultiply(value, value);
        if (pairLow(u) >= 1.0 || pairHigh(u) >= 1.0)
            return pack(polyZonedTanhImpl(pairLow(value)),
                        polyZonedTanhImpl(pairHigh(value)));

        const Pair uSquared = pairMultiply(u, u);
        const Pair top = pairMultiplyAddScalar(
            pairSplat(0.016720855179576926), u,
            -0.00305822903759879);
        const Pair high = pairMultiplyAddScalar(
            pairSplat(0.1328072064598552), u,
            -0.051585988404218436);
        const Pair low = pairMultiplyAddScalar(
            pairSplat(0.9999993948006496), u,
            -0.3332895137021704);
        const Pair upper = pairMultiplyAdd(high, top, uSquared);
        const Pair factor = pairMultiplyAdd(low, upper, uSquared);
        return pairMultiply(value, factor);
    };
    const auto cubicEarlyPair = [&](Pair value) {
        if (std::abs(pairLow(value)) >= 1.5
            || std::abs(pairHigh(value)) >= 1.5)
            return pack(cubicEarlyTanh(pairLow(value)),
                        cubicEarlyTanh(pairHigh(value)));

        const Pair scaled = pairMultiplyScalar(value, -(4.0 / 27.0));
        const Pair factor = pairMultiplyAdd(pairSplat(1.0), scaled, value);
        return pairMultiply(value, factor);
    };

    PairState state;
    PairState stageOmega;
    PairState stageOffset;
    for (std::size_t stage = 0; stage < state.size(); ++stage)
    {
        state[stage] = pack(first.state[stage], second.state[stage]);
        stageOmega[stage] = pack(lanes[0].stageOmega[stage],
                                 lanes[1].stageOmega[stage]);
        stageOffset[stage] = pack(lanes[0].stageOffset[stage],
                                  lanes[1].stageOffset[stage]);
    }
    const Pair inverseHeadroom = pack(lanes[0].inverseHeadroom,
                                      lanes[1].inverseHeadroom);
    const Pair runningHeadroom = pack(lanes[0].headroom, lanes[1].headroom);

    const auto derivative = [&](const PairState& value, Pair drive) {
        const Pair feedbackArgument = pairMultiplyScalar(
            value[3], 1.0 / feedbackHeadroom);
        const Pair feedbackTanh = polyTanhPair(feedbackArgument);
        const double firstLoopReturn = lanes[0].feedback == 0.0
            ? pairLow(drive)
            : pairLow(drive)
                - lanes[0].feedback * feedbackHeadroom
                    * pairLow(feedbackTanh);
        const double secondLoopReturn = lanes[1].feedback == 0.0
            ? pairHigh(drive)
            : pairHigh(drive)
                - lanes[1].feedback * feedbackHeadroom
                    * pairHigh(feedbackTanh);
        const Pair loopReturn = pack(firstLoopReturn, secondLoopReturn);

        PairState stageArgument {
            pairMultiply(pairAdd(pairSubtract(loopReturn, value[0]),
                                 stageOffset[0]), inverseHeadroom),
            pairMultiply(pairAdd(pairSubtract(value[0], value[1]),
                                 stageOffset[1]), inverseHeadroom),
            pairMultiply(pairAdd(pairSubtract(value[1], value[2]),
                                 stageOffset[2]), inverseHeadroom),
            pairMultiply(pairAdd(pairSubtract(value[2], value[3]),
                                 stageOffset[3]), inverseHeadroom)
        };
        PairState stageTanh;
        for (std::size_t stage = 0; stage < stageTanh.size(); ++stage)
            stageTanh[stage] = polyTanhPair(stageArgument[stage]);

        PairState early;
        if (!applyEarlyEffect)
        {
            for (auto& valueAtStage : early)
                valueAtStage = pairSplat(1.0);
        }
        else
        {
            for (std::size_t stage = 0; stage < early.size(); ++stage)
            {
                const Pair earlyTanh = cubicEarlyPair(
                    pairMultiply(value[stage], inverseHeadroom));
                early[stage] = pairMultiplyAddScalar(
                    pairSplat(1.0), earlyTanh, earlyAmount);
            }
        }

        PairState result;
        for (std::size_t stage = 0; stage < result.size(); ++stage)
        {
            result[stage] = pairMultiply(stageOmega[stage], early[stage]);
            result[stage] = pairMultiply(result[stage], runningHeadroom);
            result[stage] = pairMultiply(result[stage], stageTanh[stage]);
        }
        return result;
    };
    const auto advanceOne = [](const PairState& origin,
                               const PairState& slope, double distance) {
        PairState result;
        for (std::size_t stage = 0; stage < result.size(); ++stage)
            result[stage] = pairMultiplyAddScalar(
                origin[stage], slope[stage], distance);
        return result;
    };
    const auto advanceTwo = [](const PairState& origin, double stepSize,
                               const PairState& firstSlope,
                               double firstWeight,
                               const PairState& secondSlope,
                               double secondWeight) {
        PairState result;
        for (std::size_t stage = 0; stage < result.size(); ++stage)
        {
            Pair weighted = pairMultiplyScalar(
                secondSlope[stage], secondWeight);
            weighted = pairMultiplyAddScalar(
                weighted, firstSlope[stage], firstWeight);
            result[stage] = pairMultiplyAddScalar(
                origin[stage], weighted, stepSize);
        }
        return result;
    };
    const auto advanceThree = [](const PairState& origin, double stepSize,
                                 const PairState& firstSlope,
                                 double firstWeight,
                                 const PairState& secondSlope,
                                 double secondWeight,
                                 const PairState& thirdSlope,
                                 double thirdWeight) {
        PairState result;
        for (std::size_t stage = 0; stage < result.size(); ++stage)
        {
            Pair weighted = pairMultiplyScalar(
                secondSlope[stage], secondWeight);
            weighted = pairMultiplyAddScalar(
                weighted, firstSlope[stage], firstWeight);
            weighted = pairMultiplyAddScalar(
                weighted, thirdSlope[stage], thirdWeight);
            result[stage] = pairMultiplyAddScalar(
                origin[stage], weighted, stepSize);
        }
        return result;
    };

    for (const std::size_t start : { 0u, 3u })
    {
        const PairState origin = state;
        const Pair k1Drive = pack(lanes[0].drive[start],
                                  lanes[1].drive[start]);
        const Pair sharedDrive = pack(lanes[0].drive[start + 1u],
                                      lanes[1].drive[start + 1u]);
        const Pair k4Drive = pack(lanes[0].drive[start + 2u],
                                  lanes[1].drive[start + 2u]);
        const Pair k5Drive = pack(lanes[0].drive[start + 3u],
                                  lanes[1].drive[start + 3u]);
        const PairState k1 = derivative(origin, k1Drive);
        const PairState k2 = derivative(
            advanceOne(origin, k1, 1.0 / 6.0), sharedDrive);
        const PairState k3 = derivative(
            advanceTwo(origin, 0.5, k1, 1.0 / 6.0,
                       k2, 1.0 / 6.0), sharedDrive);
        const PairState k4 = derivative(
            advanceTwo(origin, 0.5, k1, 1.0 / 8.0,
                       k3, 3.0 / 8.0), k4Drive);
        const PairState k5 = derivative(
            advanceThree(origin, 0.5, k1, 1.0 / 2.0,
                         k3, -3.0 / 2.0, k4, 2.0), k5Drive);
        state = advanceThree(origin, 0.5, k1, 1.0 / 6.0,
                             k4, 2.0 / 3.0, k5, 1.0 / 6.0);
    }

    for (std::size_t stage = 0; stage < state.size(); ++stage)
    {
        first.state[stage] = pairLow(state[stage]);
        second.state[stage] = pairHigh(state[stage]);
    }

#undef pairLow
#undef pairHigh
#undef pairSplat
#undef pairAdd
#undef pairSubtract
#undef pairMultiply
#undef pairMultiplyScalar
#undef pairMultiplyAdd
#undef pairMultiplyAddScalar

    const auto finishLane = [](Lane& lane, float& output) {
        auto& cascade = *lane.cascade;
        for (std::size_t point = cascade.inputHistory.size() - 1u;
             point > 0u; --point)
            cascade.inputHistory[point] = cascade.inputHistory[point - 1u];
        cascade.inputHistory[0] = lane.input;
        cascade.inputHistoryCount = std::min(cascade.inputHistoryCount + 1, 2);
        cascade.previousOmegaStep = lane.omega;
        cascade.previousFeedback = lane.feedback;
        cascade.previousHeadroom = lane.headroom;

        const bool valid = std::all_of(
            cascade.state.begin(), cascade.state.end(), [](double value) {
                return std::abs(value) <= 64.0;
            });
        if (!valid)
        {
            cascade.state.fill(0.0);
            cascade.inputHistory.fill(lane.input);
            cascade.inputHistoryCount = 2;
            output = 0.0f;
            return;
        }
        output = static_cast<float>(cascade.state[3]);
    };
    finishLane(lanes[0], firstOutput);
    finishLane(lanes[1], secondOutput);
    return true;
}

YOUKNOW106_MERSON_NOINLINE
bool YouKnow106Engine::OtaCascade::tryProcessSettledMersonQuad(
    const std::array<OtaCascade*, 4>& cascades,
    const std::array<float, 4>& inputs,
    const std::array<float, 4>& omegaSteps,
    const std::array<float, 4>& feedbacks,
    const std::array<float, 4>& headrooms,
    bool enableEarlyEffect, float calibration,
    std::array<float, 4>& outputs) noexcept
{
    constexpr float feedbackHeadroom = static_cast<float>(
        VoicedResonanceCompatibilityProfile::loopHeadroomVolts);
    const double currentCalibration = std::clamp(
        std::isfinite(calibration) ? static_cast<double>(calibration) : 0.0,
        0.0, static_cast<double>(EngineParameters::calibrationCeiling));
    const bool applyEarlyEffect = enableEarlyEffect
                               && currentCalibration > 0.0;
    const double earlyAmountDouble =
        static_cast<double>(otaEarlyEffectCoefficient)
        * currentCalibration;
    const float earlyAmount = static_cast<float>(earlyAmountDouble);
    const double earlyCeiling = applyEarlyEffect
        ? 1.0 + earlyAmountDouble : 1.0;

    struct Lane
    {
        OtaCascade* cascade {};
        double input {};
        double omega {};
        double feedback {};
        double headroom {};
        float inverseHeadroom {};
        std::array<float, 7> drive {};
        std::array<float, 4> stageOmega {};
        std::array<float, 4> stageOffset {};
    };
    std::array<Lane, 4> lanes;
    for (std::size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
    {
        auto& cascade = *cascades[laneIndex];
        auto& lane = lanes[laneIndex];
        const double currentOmega = clampOmegaStep(
            static_cast<double>(omegaSteps[laneIndex]));
        const double currentFeedback = std::clamp(
            std::isfinite(feedbacks[laneIndex])
                ? static_cast<double>(feedbacks[laneIndex]) : 0.0,
            0.0, 8.0);
        const double currentHeadroom = std::max(
            std::isfinite(headrooms[laneIndex])
                ? static_cast<double>(headrooms[laneIndex]) : 0.0,
            1.0e-5);
        if (!cascade.parameterHistoryPrimed
            || cascade.inputHistoryCount != 2
            || cascade.previousOmegaStep != currentOmega
            || cascade.previousFeedback != currentFeedback
            || cascade.previousHeadroom != currentHeadroom
            || !std::all_of(
                cascade.state.begin(), cascade.state.end(), [](double value) {
                    return std::abs(value) <= 64.0;
                }))
            return false;

        double largestScale = 0.0;
        for (const float scale : cascade.gScale)
            largestScale = std::max(
                largestScale, static_cast<double>(std::abs(scale)));
        if (planTableau(
                VcfSolverMode::Rk4Single,
                currentOmega * largestScale * earlyCeiling,
                currentFeedback) != Tableau::MersonHalf)
            return false;

        lane.cascade = &cascade;
        lane.input = std::isfinite(inputs[laneIndex])
            ? static_cast<double>(inputs[laneIndex]) : 0.0;
        lane.omega = currentOmega;
        lane.feedback = currentFeedback;
        lane.headroom = currentHeadroom;
        lane.inverseHeadroom = static_cast<float>(1.0 / currentHeadroom);
        const auto reconstruct = [&](double currentWeight,
                                     double firstWeight,
                                     double secondWeight,
                                     double thirdWeight) {
            return static_cast<float>(
                currentWeight * lane.input
                + firstWeight * cascade.inputHistory[0]
                + secondWeight * cascade.inputHistory[1]
                + thirdWeight * cascade.inputHistory[2]);
        };
        lane.drive[0] = reconstruct(0.0,    1.0,    0.0,     0.0);
        lane.drive[1] = reconstruct(
            0x1.1f9add3c0ca45p-4, 0x1.0da12f684bda1p+0,
            -0x1.3425ed097b426p-3, 0x1.ba781948b0fcfp-6);
        lane.drive[2] = reconstruct(
            0.1171875, 1.0546875, -0.2109375, 0.0390625);
        lane.drive[3] = reconstruct(
            0.3125, 0.9375, -0.3125, 0.0625);
        lane.drive[4] = reconstruct(
            0x1.f9add3c0ca457p-2, 0x1.7b425ed097b42p-1,
            -0x1.2f684bda12f68p-2, 0x1.f9add3c0ca458p-5);
        lane.drive[5] = reconstruct(
            0.6015625, 0.6015625, -0.2578125, 0.0546875);
        lane.drive[6] = reconstruct(1.0,    0.0,    0.0,     0.0);
        for (std::size_t stage = 0; stage < lane.stageOmega.size(); ++stage)
        {
            lane.stageOmega[stage] = static_cast<float>(
                currentOmega * static_cast<double>(cascade.gScale[stage]));
            lane.stageOffset[stage] = cascade.offsetVoltage[stage];
        }
    }

#if defined(__aarch64__) && defined(__ARM_NEON)
    using Quad = float32x4_t;
    const auto quadLoad = [](const std::array<float, 4>& values) {
        return vld1q_f32(values.data());
    };
    const auto quadStore = [](Quad value, std::array<float, 4>& values) {
        vst1q_f32(values.data(), value);
    };
#define quadSplat(value) vdupq_n_f32(value)
#define quadAdd(first, second) vaddq_f32((first), (second))
#define quadSubtract(first, second) vsubq_f32((first), (second))
#define quadMultiply(first, second) vmulq_f32((first), (second))
#define quadMultiplyScalar(value, scalar) vmulq_n_f32((value), (scalar))
#define quadMultiplyAdd(addend, first, second) \
    vfmaq_f32((addend), (first), (second))
#define quadMultiplyAddScalar(addend, value, scalar) \
    vfmaq_n_f32((addend), (value), (scalar))
#define quadAnyGreaterEqual(value, limit) \
    (vmaxvq_f32(vabsq_f32(value)) >= (limit))
#elif defined(__x86_64__) && defined(__SSE2__)
    using Quad = __m128;
    const auto quadLoad = [](const std::array<float, 4>& values) {
        return _mm_loadu_ps(values.data());
    };
    const auto quadStore = [](Quad value, std::array<float, 4>& values) {
        _mm_storeu_ps(values.data(), value);
    };
#define quadSplat(value) _mm_set1_ps(value)
#define quadAdd(first, second) _mm_add_ps((first), (second))
#define quadSubtract(first, second) _mm_sub_ps((first), (second))
#define quadMultiply(first, second) _mm_mul_ps((first), (second))
#define quadMultiplyScalar(value, scalar) \
    _mm_mul_ps((value), _mm_set1_ps(scalar))
#define quadMultiplyAdd(addend, first, second) \
    _mm_add_ps((addend), _mm_mul_ps((first), (second)))
#define quadMultiplyAddScalar(addend, value, scalar) \
    _mm_add_ps((addend), _mm_mul_ps((value), _mm_set1_ps(scalar)))
#define quadAnyGreaterEqual(value, limit) \
    (_mm_movemask_ps(_mm_cmpge_ps( \
        _mm_andnot_ps(_mm_set1_ps(-0.0f), (value)), \
        _mm_set1_ps(limit))) != 0)
#endif
    using QuadState = std::array<Quad, 4>;
    const auto packField = [&](const auto& member, std::size_t point) {
        std::array<float, 4> values;
        for (std::size_t lane = 0; lane < lanes.size(); ++lane)
            values[lane] = member(lanes[lane], point);
        return quadLoad(values);
    };
    const auto polyTanhQuad = [&](Quad value) {
        const Quad u = quadMultiply(value, value);
        const Quad uSquared = quadMultiply(u, u);
        const Quad top = quadMultiplyAddScalar(
            quadSplat(0.016720855179576926f), u,
            -0.00305822903759879f);
        const Quad high = quadMultiplyAddScalar(
            quadSplat(0.1328072064598552f), u,
            -0.051585988404218436f);
        const Quad low = quadMultiplyAddScalar(
            quadSplat(0.9999993948006496f), u,
            -0.3332895137021704f);
        const Quad upper = quadMultiplyAdd(high, top, uSquared);
        const Quad factor = quadMultiplyAdd(low, upper, uSquared);
        Quad result = quadMultiply(value, factor);
        if (quadAnyGreaterEqual(value, 1.0f))
        {
            std::array<float, 4> values;
            std::array<float, 4> results;
            quadStore(value, values);
            quadStore(result, results);
            for (std::size_t lane = 0; lane < values.size(); ++lane)
                if (std::abs(values[lane]) >= 1.0f)
                    results[lane] = static_cast<float>(
                        polyZonedTanhImpl(
                            static_cast<double>(values[lane])));
            result = quadLoad(results);
        }
        return result;
    };
    const auto cubicEarlyQuad = [&](Quad value) {
        const Quad scaled = quadMultiplyScalar(value, -(4.0f / 27.0f));
        const Quad factor = quadMultiplyAdd(quadSplat(1.0f), scaled, value);
        Quad result = quadMultiply(value, factor);
        if (quadAnyGreaterEqual(value, 1.5f))
        {
            std::array<float, 4> values;
            std::array<float, 4> results;
            quadStore(value, values);
            quadStore(result, results);
            for (std::size_t lane = 0; lane < values.size(); ++lane)
                if (std::abs(values[lane]) >= 1.5f)
                    results[lane] = static_cast<float>(
                        cubicEarlyTanh(values[lane]));
            result = quadLoad(results);
        }
        return result;
    };

    QuadState state;
    QuadState stageOmega;
    QuadState stageOffset;
    for (std::size_t stage = 0; stage < state.size(); ++stage)
    {
        std::array<float, 4> values;
        for (std::size_t lane = 0; lane < lanes.size(); ++lane)
            values[lane] = static_cast<float>(lanes[lane].cascade->state[stage]);
        state[stage] = quadLoad(values);
        stageOmega[stage] = packField(
            [](const Lane& lane, std::size_t point) {
                return lane.stageOmega[point];
            }, stage);
        stageOffset[stage] = packField(
            [](const Lane& lane, std::size_t point) {
                return lane.stageOffset[point];
            }, stage);
    }
    const Quad inverseHeadroom = packField(
        [](const Lane& lane, std::size_t) {
            return lane.inverseHeadroom;
        }, 0u);
    const Quad runningHeadroom = packField(
        [](const Lane& lane, std::size_t) {
            return static_cast<float>(lane.headroom);
        }, 0u);
    const Quad feedbackGain = packField(
        [&](const Lane& lane, std::size_t) {
            return static_cast<float>(lane.feedback) * feedbackHeadroom;
        }, 0u);
    std::array<Quad, 7> drives;
    for (std::size_t point = 0; point < drives.size(); ++point)
        drives[point] = packField(
            [](const Lane& lane, std::size_t index) {
                return lane.drive[index];
            }, point);

    const auto derivative = [&](const QuadState& value, Quad drive) {
        const Quad feedbackArgument = quadMultiplyScalar(
            value[3], 1.0f / feedbackHeadroom);
        const Quad loopReturn = quadSubtract(
            drive, quadMultiply(feedbackGain,
                                polyTanhQuad(feedbackArgument)));
        QuadState stageArgument {
            quadMultiply(quadAdd(quadSubtract(loopReturn, value[0]),
                                 stageOffset[0]), inverseHeadroom),
            quadMultiply(quadAdd(quadSubtract(value[0], value[1]),
                                 stageOffset[1]), inverseHeadroom),
            quadMultiply(quadAdd(quadSubtract(value[1], value[2]),
                                 stageOffset[2]), inverseHeadroom),
            quadMultiply(quadAdd(quadSubtract(value[2], value[3]),
                                 stageOffset[3]), inverseHeadroom)
        };
        QuadState result;
        for (std::size_t stage = 0; stage < result.size(); ++stage)
        {
            Quad early = quadSplat(1.0f);
            if (applyEarlyEffect)
                early = quadMultiplyAddScalar(
                    early,
                    cubicEarlyQuad(quadMultiply(
                        value[stage], inverseHeadroom)),
                    earlyAmount);
            result[stage] = quadMultiply(stageOmega[stage], early);
            result[stage] = quadMultiply(result[stage], runningHeadroom);
            result[stage] = quadMultiply(
                result[stage], polyTanhQuad(stageArgument[stage]));
        }
        return result;
    };
    const auto advanceOne = [](const QuadState& origin,
                               const QuadState& slope, float distance) {
        QuadState result;
        for (std::size_t stage = 0; stage < result.size(); ++stage)
            result[stage] = quadMultiplyAddScalar(
                origin[stage], slope[stage], distance);
        return result;
    };
    const auto advanceTwo = [](const QuadState& origin, float stepSize,
                               const QuadState& firstSlope,
                               float firstWeight,
                               const QuadState& secondSlope,
                               float secondWeight) {
        QuadState result;
        for (std::size_t stage = 0; stage < result.size(); ++stage)
        {
            Quad weighted = quadMultiplyScalar(
                secondSlope[stage], secondWeight);
            weighted = quadMultiplyAddScalar(
                weighted, firstSlope[stage], firstWeight);
            result[stage] = quadMultiplyAddScalar(
                origin[stage], weighted, stepSize);
        }
        return result;
    };
    const auto advanceThree = [](const QuadState& origin, float stepSize,
                                 const QuadState& firstSlope,
                                 float firstWeight,
                                 const QuadState& secondSlope,
                                 float secondWeight,
                                 const QuadState& thirdSlope,
                                 float thirdWeight) {
        QuadState result;
        for (std::size_t stage = 0; stage < result.size(); ++stage)
        {
            Quad weighted = quadMultiplyScalar(
                secondSlope[stage], secondWeight);
            weighted = quadMultiplyAddScalar(
                weighted, firstSlope[stage], firstWeight);
            weighted = quadMultiplyAddScalar(
                weighted, thirdSlope[stage], thirdWeight);
            result[stage] = quadMultiplyAddScalar(
                origin[stage], weighted, stepSize);
        }
        return result;
    };
    for (const std::size_t start : { 0u, 3u })
    {
        const QuadState origin = state;
        const QuadState k1 = derivative(origin, drives[start]);
        const QuadState k2 = derivative(
            advanceOne(origin, k1, 1.0f / 6.0f), drives[start + 1u]);
        const QuadState k3 = derivative(
            advanceTwo(origin, 0.5f, k1, 1.0f / 6.0f,
                       k2, 1.0f / 6.0f), drives[start + 1u]);
        const QuadState k4 = derivative(
            advanceTwo(origin, 0.5f, k1, 1.0f / 8.0f,
                       k3, 3.0f / 8.0f), drives[start + 2u]);
        const QuadState k5 = derivative(
            advanceThree(origin, 0.5f, k1, 0.5f,
                         k3, -1.5f, k4, 2.0f), drives[start + 3u]);
        state = advanceThree(origin, 0.5f, k1, 1.0f / 6.0f,
                             k4, 2.0f / 3.0f, k5, 1.0f / 6.0f);
    }

    for (std::size_t stage = 0; stage < state.size(); ++stage)
    {
        std::array<float, 4> values;
        quadStore(state[stage], values);
        for (std::size_t lane = 0; lane < lanes.size(); ++lane)
            lanes[lane].cascade->state[stage] = values[lane];
    }

#undef quadSplat
#undef quadAdd
#undef quadSubtract
#undef quadMultiply
#undef quadMultiplyScalar
#undef quadMultiplyAdd
#undef quadMultiplyAddScalar
#undef quadAnyGreaterEqual

    for (std::size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
    {
        auto& lane = lanes[laneIndex];
        auto& cascade = *lane.cascade;
        for (std::size_t point = cascade.inputHistory.size() - 1u;
             point > 0u; --point)
            cascade.inputHistory[point] = cascade.inputHistory[point - 1u];
        cascade.inputHistory[0] = lane.input;
        cascade.inputHistoryCount = std::min(cascade.inputHistoryCount + 1, 2);
        cascade.previousOmegaStep = lane.omega;
        cascade.previousFeedback = lane.feedback;
        cascade.previousHeadroom = lane.headroom;
        const bool valid = std::all_of(
            cascade.state.begin(), cascade.state.end(), [](double value) {
                return std::abs(value) <= 64.0;
            });
        if (!valid)
        {
            cascade.state.fill(0.0);
            cascade.inputHistory.fill(lane.input);
            cascade.inputHistoryCount = 2;
            outputs[laneIndex] = 0.0f;
        }
        else
        {
            outputs[laneIndex] = static_cast<float>(cascade.state[3]);
        }
    }
    return true;
}
#undef YOUKNOW106_MERSON_NOINLINE
#endif

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
    // the low band strongly and the high band slightly, as the derived
    // branch does.
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
    (void) correctionTables();
    // Function-local statics are thread-safe, but their first-use guards and
    // exponentials do not belong in the first audio callback.
    (void) chassisGradientMeanCelsius();
    buildVoiceCards();
    refreshVoiceCardThermalScales();
    clearHeldNotes();
}

void YouKnow106Engine::buildHalfbandKernel() noexcept
{
    // Kaiser-windowed half-band. The historical sixty-three-tap
    // Blackman-Harris boundary had a deep far stopband but spent too many taps
    // reaching it: its main lobe put the last decimation stage's transition at
    // roughly 18 to 30 kHz. At a 44.1 kHz host that left the top of the audio
    // band inside the transition -- 0.85 dB down at 20 kHz, and content
    // folding onto 19.1 kHz rejected by only 31.7 dB.
    //
    // Kaiser trades stopband depth for transition width continuously. At 95
    // taps the selected common-host design passes the expanded 44.1/48 kHz DCO
    // audit: the shorter 63-tap boundary leaked the 25.1 kHz sixth pulse
    // harmonic back onto 19.0 kHz at 44.1 kHz. The longer boundary keeps that
    // line below the declared -70 dBc numerical-fidelity gate while retaining
    // the 20 kHz passband contract.
    //
    // The Bessel function is written out below rather than taken from the
    // standard special-function header, which is not available on every
    // toolchain this project builds with -- the reason the window was
    // originally avoided.
    const auto besselI0 = [](double x) noexcept {
        double sum = 1.0;
        double term = 1.0;
        for (int k = 1; k < 64; ++k)
        {
            const double ratio = x / (2.0 * static_cast<double>(k));
            term *= ratio * ratio;
            sum += term;
            if (term < 1.0e-18 * sum)
                break;
        }
        return sum;
    };
    // The B-2 pitch grid also places that same 24.109 kHz line just inside the
    // last 44.1 kHz decimator's transition. Beta 7.7 narrows that transition
    // enough to retain about 1.5 dB of margin over the -70 dBc gate while the
    // measured far stopband remains approximately 80 dB. Tap count, latency
    // and hot-loop work are unchanged.
    constexpr double kaiserBeta = 7.7;
    const double besselDenominator = besselI0(kaiserBeta);

    constexpr int centre = (halfbandTaps - 1) / 2;
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

        const double t = 2.0 * static_cast<double>(n)
                             / static_cast<double>(halfbandTaps - 1) - 1.0;
        const double window =
            besselI0(kaiserBeta * std::sqrt(std::max(0.0, 1.0 - t * t)))
            / besselDenominator;
        halfbandKernel_[static_cast<std::size_t>(n)] =
            ideal * static_cast<float>(window);
    }

    // The ideal and window equations are symmetric, but C++ does not require
    // independent libm evaluations at +/-x to round identically. The paired
    // hot loop needs bit-equal coefficients, so make that design invariant
    // explicit before normalisation instead of merely observing it locally.
    for (int tap = 0; tap < centre; ++tap)
        halfbandKernel_[static_cast<std::size_t>(halfbandTaps - 1 - tap)] =
            halfbandKernel_[static_cast<std::size_t>(tap)];

    double sum = 0.0;
    for (const float tap : halfbandKernel_)
        sum += static_cast<double>(tap);

    // Normalise to exactly unity gain at DC so decimation cannot shift level.
    if (sum > 1.0e-9)
    {
        for (auto& tap : halfbandKernel_)
            tap = static_cast<float>(static_cast<double>(tap) / sum);

        double normalisedSum = 0.0;
        for (const float tap : halfbandKernel_)
            normalisedSum += static_cast<double>(tap);
        halfbandKernel_[static_cast<std::size_t>(centre)] +=
            static_cast<float>(1.0 - normalisedSum);
    }

    // Compact the taps the decimator will actually accumulate. Normalisation
    // above divides every entry by one positive scalar and then corrects the
    // centre, so the analytic zeros are still exactly zero here and this
    // reproduces the same set the old per-tap branch selected, in the same
    // order.
    halfbandActiveTapCount_ = 0;
    for (int tap = 0; tap < halfbandTaps; ++tap)
    {
        const float coefficient = halfbandKernel_[static_cast<std::size_t>(tap)];
        if (coefficient == 0.0f)
            continue;
        auto& active = halfbandActiveTaps_[
            static_cast<std::size_t>(halfbandActiveTapCount_++)];
        active.coefficient = coefficient;
        active.tap = tap;
    }
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
        card.vcaControlOffset = hashBipolar(seed + 4u);
        card.cutoffScaleError = hashBipolar(seed + 5u);
        card.subLevelError = hashBipolar(seed + 6u);
        card.driftPhase = 0.5f * (hashBipolar(seed + 7u) + 1.0f);
        card.vcaGainError = hashBipolar(seed + 8u);
        card.noiseLevelError = hashBipolar(seed + 9u);
        card.agingWeight = 0.5f * (hashBipolar(seed + 30u) + 1.0f);
        card.agingCutoffCounts = 0.0f;
        for (std::size_t stage = 0; stage < 4; ++stage)
        {
            card.vcfStageOffsets[stage] =
                0.0015f * hashBipolar(seed + 10u + static_cast<std::uint32_t>(stage));
            card.vcfStageGErrors[stage] =
                hashBipolar(seed + 20u + static_cast<std::uint32_t>(stage));
        }
        card.driftValue = 0.0f;
        card.driftState = seed | 1u;
    }
}

void YouKnow106Engine::refreshAgedUnitState() noexcept
{
    // The aged-unit extension precomputes here -- called only where `aging`
    // can actually change (setParameters and reset), never on the per-note
    // path -- so the render paths stay pure: the per-card flatward cutoff
    // shift in converter counts, and one shared noise-trim gain. Both are
    // exactly inert at aging zero.
    const float aging = activeParameters_.aging;
    for (auto& card : cards_)
        card.agingCutoffCounts =
            aging > 0.0f ? agingCutoffDriftCents / 1200.0f * vcfCountsPerOctave
                               * aging * card.agingWeight
                         : 0.0f;
    agedNoiseGain_ =
        aging > 0.0f
            ? std::pow(10.0f, agingNoiseDriftDecibels * aging / 20.0f)
            : 1.0f;
}

void YouKnow106Engine::refreshVoiceCardStageTrims() noexcept
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
        {
            // The draw is volts at the pair; the cascade sums it with
            // module-node voltages that reach the pair through the anchored
            // 560/68560 divider, so the node-coordinate offset is the draw
            // divided by that attenuation. Handing the pair value to the node
            // unconverted -- as a previous revision did -- scales the
            // mechanism down 122x and mutes it.
            voice.filter.offsetVoltage[stage] =
                card.vcfStageOffsets[stage] / stageAttenuation * amount;
            // Each stage integrates into its own capacitor, so the four poles
            // do not coincide the way one shared `g` makes them. Four
            // mathematically identical poles give a resonance peak and a
            // self-oscillation more symmetric and purer than any real
            // four-section filter produces. Like the offsets, this is signed
            // and unbiased -- there is no nominal mismatch, so at Unit
            // Character zero all four collapse to unity.
            voice.filter.gScale[stage] =
                1.0f + card.vcfStageGErrors[stage] * vcfStageCapacitorTolerance
                           * amount;
        }
    }
}

void YouKnow106Engine::refreshVoiceCardThermalScales() noexcept
{
    for (int index = 0; index < maxVoices; ++index)
    {
        auto& card = cards_[static_cast<std::size_t>(index)];
        card.thermalFilterOmegaScale =
            activeParameters_.enableSpatialThermalGradient
                ? 1.0 + static_cast<double>(vcfCutoffTempcoPerCelsius)
                    * static_cast<double>(activeParameters_.calibration)
                    * (static_cast<double>(chassisGradientCelsius(index))
                       - static_cast<double>(chassisGradientMeanCelsius()))
                : 1.0;
    }
}

void YouKnow106Engine::prepare(double sampleRate, int maxBlockSize,
                               bool oversamplingEnabled)
{
    prepare(sampleRate, maxBlockSize,
            oversamplingEnabled ? maximumOversampleFactor
                                : minimumOversampleFactor);
}

void YouKnow106Engine::prepare(double sampleRate, int /*maxBlockSize*/,
                               int requestedFactor)
{
    // The frequency correction's limit-cycle table is solved on first use.
    // Touch it here, where blocking is allowed, so the first audio callback
    // never pays for it.
    (void) VoicedResonanceCompatibilityProfile::frequencyTrim(0.0f);

    // A host that has not negotiated a rate yet, or one reporting a nonsense
    // one, must not be able to put a zero, a negative or a NaN on the internal
    // grid: every coefficient below divides by it. NaN fails both clamp
    // comparisons, so it is caught explicitly rather than passed through.
    sampleRate_ = std::isfinite(sampleRate)
                    ? std::clamp(sampleRate, minimumSupportedSampleRate,
                                 maximumSupportedSampleRate)
                    : 48000.0;
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    oversamplingRequested_ = sanitiseOversampleFactor(requestedFactor);
    oversamplingApplied_ = oversamplingRequested_;
    chorus_.prepareSupportRates(sampleRate_);
    updateProcessingRate();
    prepared_ = true;
    reset();
}

void YouKnow106Engine::updateProcessingRate(bool preserveFreeRunningState) noexcept
{
    const double previousProcessingRate = oversampledRate_;
    oversampling_ = effectiveOversampleFactor(oversamplingApplied_);

    oversampledRate_ = sampleRate_ * oversampling_;
    inverseOversampledRate_ = static_cast<float>(1.0 / oversampledRate_);
    noiseRateScale_ = static_cast<float>(
        std::sqrt(oversampledRate_ / noiseReferenceRateHz));
    const auto slewFor = [this](float seconds) {
        return 1.0f - std::exp(-inverseOversampledRate_ / seconds);
    };
    processingCoefficients_.vcfSlew = slewFor(vcfHoldSlewSeconds);
    processingCoefficients_.dcoSlew = slewFor(dcoHoldSlewSecondsVoiced);
    processingCoefficients_.resonanceSlew =
        slewFor(resonanceHoldSlewSecondsVoiced);
    processingCoefficients_.noiseSlew = slewFor(noiseHoldSlewSecondsVoiced);
    processingCoefficients_.internalIntervalSeconds = 1.0 / oversampledRate_;
    processingCoefficients_.voiceVcaDecay = std::exp(
        -processingCoefficients_.internalIntervalSeconds
        / static_cast<double>(voiceVcaHoldSlewSeconds));
    processingCoefficients_.commonVcaTime =
        static_cast<double>(commonVcaHoldTimeConstantSeconds());
    processingCoefficients_.commonVcaDecay = std::exp(
        -processingCoefficients_.internalIntervalSeconds
        / processingCoefficients_.commonVcaTime);
    processingCoefficients_.subDecay = std::exp(
        -processingCoefficients_.internalIntervalSeconds
        / static_cast<double>(subHoldSlewSeconds));
    processingCoefficients_.pwmFullInterval = pwmHoldCoefficients(
        processingCoefficients_.internalIntervalSeconds);
    voiceEnergyFollower_ = slewFor(voiceEnergyFollowerSeconds);
    processingCoefficients_.outputGlide =
        1.0f - std::exp(-inverseSampleRate_ / panelGlideSeconds);
    processingCoefficients_.scanPhasePerInternalSample =
        controlScanHz / oversampledRate_;
    processingCoefficients_.outputBoundaryGain = outputBoundaryGain();
    processingCoefficients_.outputSlewMaxStep =
        static_cast<float>(outputSummerSlewRateVoltsPerSecond
                           * voltsToSample / oversampledRate_);
    processingCoefficients_.outputSummerBandwidthBlend = 1.0f - std::exp(
        -2.0f * std::numbers::pi_v<float> * outputSummerBandwidthHz()
        / oversampledRate_);
    // bipolarFromState() is uniform [-1,1] with RMS 1/sqrt(3). Integrating a
    // one-sided V/sqrt(Hz) density to the host Nyquist frequency therefore
    // needs sqrt(3*Fs/2). Noise is generated after decimation: doing it in
    // every quality domain would change its physical bandwidth and waste CPU.
    processingCoefficients_.outputSummerNoiseScale =
        outputSummerResistorNoiseDensity()
        * std::sqrt(1.5f * static_cast<float>(sampleRate_))
        * voltsToSample;

    // C14's load and the following HPF are selected by one panel switch.  Their
    // coefficients move only with that mode or this internal rate.
    updateSharedHighPass(activeParameters_);
    moduleCouplingG_ = std::tan(
        pi * moduleCouplingCornerHz() * inverseOversampledRate_);
    vcaInputCouplingG_ = std::tan(
        pi * vcaInputCouplingCornerHz() * inverseOversampledRate_);
    commonVcaInputCouplingG_ = std::tan(
        pi * commonVcaInputCouplingCornerHz() * inverseOversampledRate_);
    noiseSourceHighPassG_ = std::tan(
        pi * noiseSourceHighPassHz() * inverseOversampledRate_);
    // C41/R79's physical 4.82 kHz corner lies above Nyquist on the supported
    // 8--9.6 kHz HQ-off endpoint grids. Feeding that frequency straight to
    // tan() makes the TPT coefficient negative and its state recursion
    // unstable. Keep the component-derived helper unchanged, but limit this
    // numerical design corner on the actual internal grid, as the other TPT
    // support filters do. Standard rates and 8 kHz HQ (32 kHz internal) remain
    // on the exact component corner.
    const float noiseSourceLowPassDesignHz = std::min(
        noiseSourceLowPassHz(), static_cast<float>(oversampledRate_) * 0.45f);
    noiseSourceLowPassG_ = std::tan(
        pi * noiseSourceLowPassDesignHz * inverseOversampledRate_);
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
    // starts a new scan. DCO periodSamples is only a coordinate for a physical
    // period already in flight: scale that retained period with the grid rather
    // than reconstructing it from a newly moved RANGE switch before the next
    // PIT edge. Preserve the remaining drift interval in seconds too.
    if (preserveFreeRunningState && previousProcessingRate > 0.0)
    {
        const double rateRatio = oversampledRate_ / previousProcessingRate;
        for (auto& voice : voices_)
            voice.dco.periodSamples *= rateRatio;
    }
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
    // The cutoff chain's memo is keyed on counts and loop gain, both of which
    // survive a quality change untouched, while the coefficient it produces
    // is measured in internal samples and does not. Retire it here so a card
    // whose holds have settled exactly cannot be handed back the old grid's
    // answer.
    for (auto& voice : voices_)
    {
        voice.cutoffChainCounts = -1.0e30f;
        voice.cutoffChainFeedback = -1.0e30f;
    }
    chorus_.prepare(oversampledRate_, preserveFreeRunningState);
}

int YouKnow106Engine::effectiveOversampleFactor(int requestedFactor) const noexcept
{
    // The deepest rung worth running at this host rate: the bandlimiting target
    // is minimumHqProcessingRate, and going past it buys nothing but CPU. The
    // player's request is then capped by that, so selecting 4x on a 192 kHz
    // host quietly runs at 1x rather than at 768 kHz.
    int ceilingFactor;
    if (sampleRate_ >= minimumHqProcessingRate)
        ceilingFactor = 1;
    else if (sampleRate_ >= minimumHqProcessingRate / 2.0)
        ceilingFactor = 2;
    else
        ceilingFactor = maximumOversampleFactor;

    return std::min(sanitiseOversampleFactor(requestedFactor), ceilingFactor);
}

bool YouKnow106Engine::setOversamplingEnabled(bool enabled) noexcept
{
    return setOversamplingFactor(enabled ? maximumOversampleFactor
                                         : minimumOversampleFactor);
}

bool YouKnow106Engine::setOversamplingFactor(int factor) noexcept
{
    oversamplingRequested_ = sanitiseOversampleFactor(factor);
    return applyPendingOversamplingIfIdle();
}

bool YouKnow106Engine::applyPendingOversamplingIfIdle() noexcept
{
    // Nothing here is about the rung that was asked for; it is about the grid
    // the engine would actually run. Two different rungs resolve to the same
    // internal rate whenever the host is already fast enough -- 4x and 2x both
    // run at 2x on a 96 kHz host -- and rebuilding the whole output path to
    // arrive back at the rate it is already on would spend a safety fade on a
    // change nobody can hear. Adopt the request and leave the grid alone.
    if (effectiveOversampleFactor(oversamplingRequested_) == oversampling_)
    {
        oversamplingApplied_ = oversamplingRequested_;
        // A request can also be withdrawn while the old path is fading. Bring
        // it back without touching any rate-dependent state.
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

    // A physical pitch transaction is anchored to the current internal grid:
    // its PIT preparation starts at T-389 CPU states and its paired converter
    // hold is committed at T. Let that sub-sample transaction retire before a
    // quality rebuild moves T to a different grid. The physical transaction
    // itself spans at most 97.25 us; because this guard is polled at process
    // boundaries, the already-muted rebuild can occur at the next host block.
    for (int slot = 0; slot < hardwareVoices; ++slot)
    {
        const auto& voice = voices_[static_cast<std::size_t>(slot)];
        if (voice.dcoPitchTransactionValid
            || voice.dco.pitWriteState != Dco::PitWriteState::idle)
            return false;
    }

    oversamplingApplied_ = oversamplingRequested_;
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
    // updateProcessingRate() has already replaced the chorus transitions,
    // retained its BBD buckets/free-running phases, and reinitialised the
    // sample-grid support histories and continuous-support coordinates under
    // this zero-gain boundary. A hard reset also clears the physical/free-
    // running effect state.
    if (!preserveFreeRunningState)
        chorus_.reset(false);
    if (!preserveFreeRunningState)
    {
        voiceBusCoupling_.reset();
        highPass_.reset();
        commonVcaInputCoupling_.reset();
        noiseSourceHighPass_.reset();
        noiseSourceLowPass_.reset();
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
    // IC6's own output node. It was the one mutable state in the output path
    // this did not clear, so `reset()` did not put the engine in one state:
    // what it rendered next depended on what it had been rendering before. The
    // leak is tiny -- the slew limit is 1.0 V/us, so the integrator collapses
    // within an internal sample or two of a stop -- but a reset that does not
    // reset is not something a deterministic re-render can rely on.
    outputSlewStateLeft_ = 0.0f;
    outputSlewStateRight_ = 0.0f;
    outputBandwidthStateLeft_ = 0.0f;
    outputBandwidthStateRight_ = 0.0f;
    outputNoiseStateLeft_ = 0x91e10da5u;
    outputNoiseStateRight_ = 0xd1b54a35u;
    outputWiperNoiseStateLeft_ = 0x94d049bbu;
    outputWiperNoiseStateRight_ = 0x8538ecadu;
}

void YouKnow106Engine::rebuildRateDependentVoiceState() noexcept
{
    for (auto& voice : voices_)
    {
        const float previousFilterOmegaStep = voice.filterOmegaStep;
        const float previousEffectiveFilterOmegaStep =
            boundedThermalFilterOmegaStep(
                previousFilterOmegaStep, activeParameters_, voice.cardIndex);
        // Residual kernels are measured in internal samples. The safety fade
        // has reached zero, so discard their old-rate tails and prime the new
        // timeline at the continuing capacitor voltage. PIT countdown is held
        // in selected-clock periods and the ramp slope in volts per second, so
        // neither physical state is retimed by this sample-grid rebuild.
        const float saw = static_cast<float>(
            voice.dco.rampValue * static_cast<double>(voice.dco.renderScale)
            + (static_cast<double>(voice.dco.renderScale) - 1.0));
        voice.dco.saw.reset();
        voice.dco.pulse.reset();
        voice.dco.sub.reset();
        voice.dco.saw.prime(saw);
        voice.dco.pulse.prime(voice.dco.pulseState);
        voice.dco.sub.prime(voice.dco.subState);

        if (voice.active || voice.cardIndex < hardwareVoices)
        {
            updateVoiceAudio(voice, activeParameters_);
            const float nextEffectiveFilterOmegaStep =
                boundedThermalFilterOmegaStep(
                    voice.filterOmegaStep, activeParameters_, voice.cardIndex);
            voice.filter.retime(previousEffectiveFilterOmegaStep,
                                nextEffectiveFilterOmegaStep);
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
    // everything around it; padding the shallower settings by at most 17 host
    // samples keeps the number the host was told true.
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
        voice.moduleCoupling.reset();
        voice.vcaInputCoupling.reset();
        voice.vcaInputVolts = 0.0f;
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
    refreshVoiceRampCurrentScales();
    // `voice = Voice {}` above zeroed the offsets, and the cards outlive a
    // reset, so put them back before anything can render a symmetric filter.
    refreshVoiceCardStageTrims();
    refreshAgedUnitState();

    clearOutputPath();
    clearHeldNotes();
    rateTransition_ = RateTransition::Idle;
    rateTransitionGain_ = 1.0f;

    thermalWarmupSeconds_ = 0.0;
    thermalWarmupFraction_ = 0.0f;
    powerSupplyDroop_ = 0.0f;
    lfoAccumulator_ = 0u;
    lfoRising_ = true;
    lfoPolarity_ = 1.0f;
    lfoValue_ = 0.0f;
    lfoDelayLevel_ = 0.0f;
    lfoDelayByte_ = 0u;
    updateSharedScan(activeParameters_);
    resonanceCv_ = resonanceCvTarget_;
    sharedVca_ = sharedVcaTarget_;
    pwmVoltsFirstPole_ = pwmVoltsTarget_;
    pwmVolts_ = pwmVoltsTarget_;
    subCv_ = subCvTarget_;
    noiseCv_ = noiseCvTarget_;
    primeStartupVoiceWaveNodes(activeParameters_);
    // The hold has to go with the level: a note arriving at the very first
    // sample of a new run gives the scan no idle pass in which to clear it, so
    // a hold left over from the previous run would be skipped.
    lfoDelayHoldoff_ = 0u;
    lfoDelayFade_ = 0u;
    // The two oscillators have no recoverable startup relation. One full
    // selected period is the deterministic arbitrary IC35 phase; unlike the
    // old card-local policy it is one phase shared by all six PIT inputs.
    rangeClockClocksToFallingEdge_ = 1.0;
    rangeClockClocksToReload_ = 0.0;
    rangeClockTransitionPending_ = false;
    controlScanPhase_ = 1.0;
    converterEventPhases_ = converterEventPhases(converterTimingProfile_);
    nextConverterWrite_ = 0;
    converterPassLfoGated_ = 0.0f;
    passiveHoldEventLatch_ = {};
    exactVcfControlInterval_.fill(false);
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
    dcoPitchBendWord_ = 0;
    dcoLfoPitchWord_ = 0;
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

void YouKnow106Engine::resetForHostStop()
{
    // The chassis does not return to ambient because the transport stopped.
    // The warm-up timer and the fraction derived from it are the whole of the
    // free-running physical state `reset()` clears -- the voice-card trims
    // outlive it already, and the rail droop is an instantaneous load measure
    // of voices this call is about to silence, so zero is its correct value
    // once they are gone rather than a cold supply beside a warm chassis.
    //
    // This is a decision about what a host reset means, not a measurement. It
    // moves a boundary the engine draws elsewhere: `reset()` is written as a
    // power cycle -- the comment on the quality switch says a live rate change
    // is not one "because a prepare/reset is" -- and on that reading a host
    // that resets on every transport stop is simply asking for a power cycle
    // each time. The reading taken here is that a transport stop is not one:
    // the modelled instrument is not switched off when the player stops the
    // song, and a 900 s warm-up that restarts at every stop never runs at all.
    // `prepare()` remains the cold path, and it is the one a rate change,
    // a device change and a fresh instance all go through.
    const double warmupSeconds = thermalWarmupSeconds_;
    const float warmupFraction = thermalWarmupFraction_;
    reset();
    thermalWarmupSeconds_ = warmupSeconds;
    thermalWarmupFraction_ = warmupFraction;
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
    fix01(result.aging, 0.0f);
    // Unlike every other 0-1 control, Unit Character extends to 2: 0 is the
    // digital reference, 1 matches real hardware, and the headroom to 2
    // extrapolates every blended mechanism past its physical draw. The old
    // 0-100 range this comment once described is gone -- the comparison
    // tools stopped using it, and past 1 the affine blends walk through
    // their nominals -- so the ceiling lives in `calibrationCeiling`.
    // Clamping to fix01's 0-1 would still silently discard the top half.
    result.calibration = std::isfinite(result.calibration)
        ? std::clamp(result.calibration, 0.0f, EngineParameters::calibrationCeiling) : 1.0f;
    fix01(result.chorusNoise, Chorus::defaultNoiseScale);

    result.masterTuneCents = std::isfinite(result.masterTuneCents)
                           ? std::clamp(result.masterTuneCents, -50.0f, 50.0f)
                           : 0.0f;
    result.keyTranspose = std::clamp(result.keyTranspose, -12, 12);
    result.polyphony = std::clamp(result.polyphony, 1, maxVoices);
    if (result.vcfTanhMode != VcfTanhMode::Exact
        && result.vcfTanhMode != VcfTanhMode::ZonedHermite
        && result.vcfTanhMode != VcfTanhMode::PolyZoned)
        result.vcfTanhMode = VcfTanhMode::Exact;
    if (result.vcfFastEarlyMode != VcfFastEarlyMode::Hermite
        && result.vcfFastEarlyMode != VcfFastEarlyMode::Cubic)
        result.vcfFastEarlyMode = VcfFastEarlyMode::Hermite;
    if (result.vcfSolverMode != VcfSolverMode::MersonHalfSteps
        && result.vcfSolverMode != VcfSolverMode::Rk4HalfSteps
        && result.vcfSolverMode != VcfSolverMode::Rk4Single)
        result.vcfSolverMode = VcfSolverMode::MersonHalfSteps;
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

float YouKnow106Engine::processMainNoiseSource(
    float rawNoise, float level, bool levelBeforeC41) noexcept
{
    // Module board p. 13 is ordered, not merely a pair of commuting fixed
    // filters: Tr21 crosses C42 into the BA662 level OTA, then C41/R79 loads
    // that OTA's output. At a fixed level the old post-C41 scalar has the same
    // transfer, but during a scanned level move it exposes the wrong stored
    // charge. Keep C42 running unconditionally and drive the existing C41
    // state with the controlled OTA output; no extra pole or reset is added.
    const float coupled = noiseSourceHighPass_.process(
        rawNoise, noiseSourceHighPassG_, 0.0f, 1.0f);
    // On coarse HQ-off grids below about 19.3 kHz, C41's 33 us memory is
    // shorter than one sample while the established bilinear support pole is
    // negative. Exposing that state would turn a physical monotonic discharge
    // into an invented alternating tail. Preserve the qualified fixed-level
    // filter there and treat the sub-sample control memory as instantaneous.
    const bool resolvedC41Memory =
        levelBeforeC41 && noiseSourceLowPassG_ <= 1.0f;
    const float otaOutput = coupled * (resolvedC41Memory ? level : 1.0f);
    const float shaped = noiseSourceLowPass_.process(
        otaOutput, noiseSourceLowPassG_, 1.0f, 0.0f);
    const float rail = shaped * noiseMixVolts;
    return resolvedC41Memory ? rail : rail * level;
}

void YouKnow106Engine::setParameters(const EngineParameters& parameters)
{
    // Before the first valid prepared audio interval, even an equal snapshot has
    // the one-shot responsibility of priming the physical holds below.  Once
    // audio time has begun, an equal complete image has no ordered converter
    // write or assignment side effect and can return exactly.
    const bool startupSnapshot = !prepared_ || !panelGlidePrimed_;
    if (!startupSnapshot && parameters == activeParameters_
        && parameters == targetParameters_)
        return;

    const auto next = sanitise(parameters);
    if (!startupSnapshot && next == activeParameters_
        && next == targetParameters_)
        return;

    const bool assignModeChanged = next.keyMode != activeParameters_.keyMode;
    const bool unisonVoiceCountChanged = next.polyphony != activeParameters_.polyphony
                                      && (next.keyMode == KeyMode::Unison
                                          || activeParameters_.keyMode
                                                 == KeyMode::Unison);
    const bool highPassChanged = next.highPass != activeParameters_.highPass;
    const bool rangeChanged = next.range != activeParameters_.range;
    const bool stageTrimsChanged =
        next.calibration != activeParameters_.calibration
        || next.enableVcfStageOffsets
               != activeParameters_.enableVcfStageOffsets;
    const bool thermalScalesChanged = startupSnapshot
        || next.calibration != activeParameters_.calibration
        || next.enableSpatialThermalGradient
               != activeParameters_.enableSpatialThermalGradient;
    const bool rampCurrentScalesChanged = startupSnapshot
        || next.calibration != activeParameters_.calibration;
    const bool agingChanged = next.aging != activeParameters_.aging;
    // Before the first valid prepared audio interval, a host snapshot is the
    // power-up image rather than a timed panel move. `panelGlidePrimed_` is
    // already the exact one-shot marker for that boundary: invalid/zero calls
    // return before setting it, and reset clears it. Do not use output-path
    // silence here. Once audio time has started, the hardware scanner keeps
    // running through ordinary silence and after panic.
    targetParameters_ = next;
    if (rangeChanged && !startupSnapshot)
    {
        // IC35 is one shared 8 MHz synchronous divider. PF7/PF6 present
        // 14/12/8 while its inverted carry feeds both /LD and every PIT CLK:
        // a switch cannot truncate the count already in progress. Retain the
        // old-cycle remainder on every card, then use the new modulus after
        // that common reload edge.
        // https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=13
        // https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=17
        // https://toshiba.semicon-storage.com/info/TC74HC161AF_datasheet_en_20140301.pdf?did=10907&prodName=TC74HC161AF#page=2
        beginRangeClockTransition(activeParameters_.range, next.range);
    }
    else if (startupSnapshot)
    {
        // A restored pre-audio panel image is the power-up selection, not a
        // timed PF write from the constructor's default range. Adopt its
        // modulus without manufacturing an old-range cycle first.
        rangeClockClocksToFallingEdge_ = 1.0;
        rangeClockClocksToReload_ = 0.0;
        rangeClockTransitionPending_ = false;
    }
    // Switch positions land immediately. Main VOLUME is the only continuous
    // panel control applied outside the scanned converter path; it glides in
    // the render loop so host automation cannot make a block-boundary step.
    activeParameters_ = targetParameters_;
    useCubicEarly_ =
        activeParameters_.vcfTanhMode != VcfTanhMode::Exact
        && activeParameters_.vcfFastEarlyMode == VcfFastEarlyMode::Cubic;
    // Unit Character scales the stage offsets, so they follow the panel; the
    // aged-unit precompute follows only panel edits and reset, not note-ons.
    if (stageTrimsChanged)
        refreshVoiceCardStageTrims();
    if (thermalScalesChanged)
        refreshVoiceCardThermalScales();
    if (rampCurrentScalesChanged)
        refreshVoiceRampCurrentScales();
    if (agingChanged)
        refreshAgedUnitState();
    if (highPassChanged)
        updateSharedHighPass(activeParameters_);

    // A host may deliver its saved snapshot before prepare(), after prepare(),
    // or more than once while restoring state. Until audio time begins, prime
    // every shared hold from the newest complete snapshot instead of letting
    // the first attack hear the constructor's stale patch. Once any valid
    // prepared interval has run, every edit takes the normal ordered 23-write
    // converter path, even if the instrument has been quiet long enough for a
    // quality switch or has just received panic.
    if (startupSnapshot)
    {
        // No audio interval can own a pending physical write before startup.
        // A newer restore snapshot therefore supersedes any speculative latch
        // assembled by a test/host sequence that has not yet processed audio.
        passiveHoldEventLatch_ = {};
        exactVcfControlInterval_.fill(false);
        updateSharedScan(next);
        resonanceCv_ = resonanceCvTarget_;
        sharedVca_ = sharedVcaTarget_;
        pwmVoltsFirstPole_ = pwmVoltsTarget_;
        pwmVolts_ = pwmVoltsTarget_;
        subCv_ = subCvTarget_;
        noiseCv_ = noiseCvTarget_;
        primeStartupVoiceWaveNodes(next);
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

float YouKnow106Engine::resolveGlideStepPerScan(float portamento) noexcept
{
    if (portamento != glideLawPortamento_)
    {
        glideLawPortamento_ = portamento;
        glideLawStepPerScan_ = glideStepPerScan(portamento);
    }
    return glideLawStepPerScan_;
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
    const bool sentVoiceOff = std::any_of(
        voices_.begin(), voices_.end(), [](const Voice& voice) {
            return voice.active && voice.keyDown;
        });
    if (sentVoiceOff)
        finishProtectedPitWritesBeforeSerialVoiceCommand();
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
    if (sentVoiceOff)
    {
        restartVoiceBoardScanAfterSerialVoiceCommand();
        // The restart begins a wholly subsequent pass at RESONANCE. That pass
        // is therefore the one that may prove every card observed gate-off;
        // waiting to arm at its following wrap would add a fictitious pass.
        assignmentRescanPassArmed_ = true;
    }
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
    lfoDelayByte_ = 0u;
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
    const int voiceMidi = midiNote + parameters.keyTranspose;
    const bool resetDco = pitchChangeRequestsDcoReset(voice, voiceMidi);

    // reset() permanently binds array slot N to card N. Every caller passes the
    // voice from that same slot, so this defensive assignment is a no-op and the
    // card trims already installed by reset()/setParameters() remain current.
    // In particular, a note-on must not recompute all 16 cards' four stages.
    voice.cardIndex = slot;
    // An idle extension slot represents no powered analogue card: silenceVoice
    // deliberately clears it. Construct its new virtual WAVE node at the
    // current settled mean before opening the VCA, rather than treating that
    // construction as a zero-to-DC capacitor step.
    if (!wasSounding && slot >= hardwareVoices)
        primeVoiceWaveNode(voice, parameters);
    // Wake from the freewheel by resuming, not flushing: frozen reconstruction
    // and filter state measured closer to the always-rendered reference than a
    // from-silence rebuild (retrigger nulls improved about 6 dB when nothing
    // was reset). C56/C50 is advanced separately by the cheap path below.
    // Everything is bounded and finite, the comparator reconciles against its
    // memoryless truth, and the VCA is still closed while the few stale
    // reconstruction samples flush through.
    voice.freewheeling = false;
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

    voice.glideSemitonesPerScan = resolveGlideStepPerScan(
        portamentoTravelAdcFraction(parameters.portamento));
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

    // A running note timer takes the count-only path for a legato pitch message.
    // A different pitch on a free/releasing voice requests the Mode-3
    // control-word path, but the voice CPU consumes it only on that voice's
    // next scan update. Explicit OUT polarity then decides whether forcing high
    // produces the positive C54/sub edge.
    if (resetDco)
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
        voice.dcoResetPending = true;
        voice.pulseThresholdPrimed = false;
        voice.filter.reset();
        voice.moduleCoupling.reset();
        voice.vcaInputCoupling.reset();
        voice.vcaInputVolts = 0.0f;
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

void YouKnow106Engine::finishProtectedPitWritesBeforeSerialVoiceCommand() noexcept
{
    // A fractional passive-hold event has already written its DAC/mux and
    // advanced the physical capacitor inside the preceding sample. Its public
    // target is deferred only until the next boundary poll. Preserve that
    // completed port store before the non-returning handler discards the poll.
    if (passiveHoldEventLatch_.valid)
    {
        const float target = passiveHoldEventLatch_.target;
        performConverterWrite(
            passiveHoldEventLatch_.write, activeParameters_,
            converterPassLfoGated_, &target);
        passiveHoldEventLatch_ = {};
    }

    // Finish everything protected by DI before the Voice On/Off handler is
    // allowed to mutate note state. The reset path enters DI four states before
    // its T-389 control store, so that narrow interval must capture the old
    // voice payload here; doing it after assignment would program the new note.
    // The running path reaches DI 13 states after T-389, leaving 42 states to
    // its LSB. Every awaiting-MSB state is protected. Exact boundary ties use
    // the declared PIT/DI-first policy.
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L732-L741
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L783-L794
    for (int slot = 0; slot < hardwareVoices; ++slot)
    {
        auto& voice = voices_[static_cast<std::size_t>(slot)];
        auto& dco = voice.dco;
        const bool pitchScanWouldRequestReset = voice.rootMidi >= 0
            && pitchChangeRequestsDcoReset(
                voice, voice.rootMidi + activeParameters_.keyTranspose);
        const bool resetPrestageIsProtected =
            dco.pitWriteState
                    == Dco::PitWriteState::awaitingPitchPrestage
            && (voice.dcoResetPending || pitchScanWouldRequestReset)
            && dco.cpuStatesToWrite <= pitResetDiToControlStates;
        if (resetPrestageIsProtected)
            prestageDcoPitchTransaction(
                voice, rangeClockClocksToFallingEdge_, 1.0f, true);

        const bool lsbIsProtected =
            dco.pitWriteState == Dco::PitWriteState::awaitingLsb
            && (dco.pitState == Dco::PitState::awaitingCount
                || dco.cpuStatesToWrite <= pitDiToLsbStates);
        const bool msbIsProtected =
            dco.pitWriteState == Dco::PitWriteState::awaitingMsb;
        if (lsbIsProtected || msbIsProtected)
            dco.stageMode3Count(dco.pitWriteDivider);

        voice.dcoPitchTransactionValid = false;
        voice.dcoPitchTransactionColdStart = false;
        dco.pitWriteState = Dco::PitWriteState::idle;
        dco.cpuStatesToWrite = 0.0;
    }
}

void YouKnow106Engine::restartVoiceBoardScanAfterSerialVoiceCommand() noexcept
{
    // The recovered B-2 serial ISR does not RETI from Voice On/Off. It loads
    // SP with $ffff and jumps through $02eb to the beginning of the main loop,
    // so an interrupted converter pass resumes at RESONANCE rather than at its
    // former ordinal. Treat the engine's logical note command as that completed
    // handler boundary. The 31.25-kbaud arrival phase and the installed NMOS
    // uPD7810's automatic entry latency remain unmeasured and are deliberately
    // not turned into random timing here.
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L204-L244
    const bool passBoundaryWasAlreadyDue = controlScanPhase_ >= 1.0;
    if (!passBoundaryWasAlreadyDue)
    {
        // The jump begins a fresh loop, so its early onset calculation runs
        // again. The restart itself does not rerun the late LFO/PWM update;
        // preserve that state and refresh only what 030D-03A1 derives from the
        // already-stored oscillator value.
        // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L526-L607
        advanceLfoDelay(activeParameters_);
        dcoLfoPitchWord_ = dcoLfoPitchWordOffset(
            lfoAccumulator_, lfoPolarity_ >= 0.0f, lfoDelayByte_,
            storedControlByte(activeParameters_.dcoLfoDepth),
            storedControlByte(modWheelTarget_),
            controlAdcByte(activeParameters_.benderLfoDepth));
        converterPassLfoGated_ = lfoValue_ * lfoDelayLevel_;
    }
    controlScanPhase_ = passBoundaryWasAlreadyDue ? 1.0 : 0.0;
    nextConverterWrite_ = 0;
    passiveHoldEventLatch_ = {};
}

void YouKnow106Engine::assignHeldNote(int midiNote, float velocity) noexcept
{
    if (activeParameters_.keyMode == KeyMode::Unison)
    {
        finishProtectedPitWritesBeforeSerialVoiceCommand();
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
        restartVoiceBoardScanAfterSerialVoiceCommand();
        return;
    }

    const int existing = findVoiceForNote(midiNote);
    const int slot = existing >= 0 ? existing : allocateVoice(midiNote);
    if (slot < 0)
        return; // Every key is held: the note is dropped, as on the hardware.

    finishProtectedPitWritesBeforeSerialVoiceCommand();
    auto& voice = voices_[static_cast<std::size_t>(slot)];
    initialiseVoice(voice, slot, midiNote, velocity);
    voice.unisonMember = false;
    updateActiveVoiceCount();
    restartVoiceBoardScanAfterSerialVoiceCommand();
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

        finishProtectedPitWritesBeforeSerialVoiceCommand();
        for (auto& voice : voices_)
            if (voice.active && voice.unisonMember && voice.keyDown)
                releaseVoiceKey(voice);
        restartVoiceBoardScanAfterSerialVoiceCommand();
        return;
    }

    const bool sentVoiceOff = std::any_of(
        voices_.begin(), voices_.end(), [midiNote](const Voice& voice) {
            return voice.active && voice.rootMidi == midiNote
                && voice.keyDown;
        });
    if (sentVoiceOff)
        finishProtectedPitWritesBeforeSerialVoiceCommand();
    for (auto& voice : voices_)
    {
        if (!voice.active || voice.rootMidi != midiNote || !voice.keyDown)
            continue;
        releaseVoiceKey(voice);
    }
    if (sentVoiceOff)
        restartVoiceBoardScanAfterSerialVoiceCommand();
}

void YouKnow106Engine::releaseAllNotes()
{
    clearHeldNotes();
    anyKeyDown_ = false;
    assignmentRescanPending_ = false;
    assignmentRescanPassArmed_ = false;
    rescanPreviousUnisonMembers_.fill(false);
    rescanUnisonMidiValid_ = false;
    const bool sentVoiceOff = std::any_of(
        voices_.begin(), voices_.end(), [](const Voice& voice) {
            return voice.active && voice.keyDown;
        });
    if (sentVoiceOff)
        finishProtectedPitWritesBeforeSerialVoiceCommand();
    for (auto& voice : voices_)
    {
        if (!voice.active || !voice.keyDown)
            continue;
        releaseVoiceKey(voice);
    }
    if (sentVoiceOff)
        restartVoiceBoardScanAfterSerialVoiceCommand();
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

    advanceLfoDelay(parameters);
}

void YouKnow106Engine::advanceLfoDelay(
    const EngineParameters& parameters) noexcept
{
    // Delay: a silent hold that advances at the attack table's own rate, then
    // the stepped fade. The pair is re-armed the moment a note starts with no
    // key down -- release tails still ringing keep their vibrato, because the
    // firmware's retrigger latch watches the keys, not the envelopes.
    if (lfoDelayHoldoff_ < 0x4000u)
    {
        lfoDelayHoldoff_ = std::min<std::uint32_t>(
            0x4000u,
            lfoDelayHoldoff_ + envelopeAttackIncrement(parameters.lfoDelay));
        lfoDelayByte_ = 0u;
    }

    // OFFI branches straight into DADDNC when the just-stored holdoff crosses
    // 0x3fff. Keep the two tests independent so that crossing pass also makes
    // the fade's first step instead of inserting one extra 4.2 ms hold.
    // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L577-L607
    if (lfoDelayHoldoff_ >= 0x4000u && lfoDelayFade_ < 0x10000u)
    {
        lfoDelayFade_ = std::min<std::uint32_t>(
            0x10000u,
            lfoDelayFade_ + lfoDelayFadeIncrement(parameters.lfoDelay));
        lfoDelayByte_ = lfoDelayFade_ >= 0x10000u
            ? 255u : static_cast<std::uint8_t>(lfoDelayFade_ >> 8u);
    }
    else if (lfoDelayFade_ >= 0x10000u)
        lfoDelayByte_ = 255u;

    lfoDelayLevel_ = static_cast<float>(lfoDelayByte_) / 255.0f;
    displayLfo_ = lfoValue_ * lfoDelayLevel_;
}

void YouKnow106Engine::updateVoiceCardDrift(VoiceCard& card) noexcept
{
    // A slow, bounded wander of the analogue control chain. It is deliberately
    // small: the oscillators share one reference, so this instrument does not
    // drift the way six free-running oscillators would.
    card.driftState = xorshift32(card.driftState);
    const float excitation =
        static_cast<float>(card.driftState & 0xffffu) * (2.0f / 65535.0f) - 1.0f;
    card.driftValue = card.driftValue * 0.9992f + excitation * 0.004f;
}

std::uint32_t YouKnow106Engine::updateVoiceScan(
    Voice& voice, const EngineParameters& parameters, float lfoGated) noexcept
{
    const std::uint32_t count = updateVoiceEnvelopeAndPitch(
        voice, parameters);
    updateVoiceVcfTarget(voice, parameters, lfoGated);
    updateVoiceVcaTarget(voice, parameters);
    return count;
}

bool YouKnow106Engine::pitchChangeRequestsDcoReset(
    const Voice& voice, int voiceMidi) noexcept
{
    return (!voice.hasVoicePitchHistory || voice.lastVoiceMidi != voiceMidi)
        && !voice.keyDown && !voice.sustained;
}

std::uint32_t YouKnow106Engine::updateVoiceEnvelopeAndPitch(
    Voice& voice, const EngineParameters& parameters) noexcept
{
    // --- Envelope ---------------------------------------------------------
    // A linear attack and multiplicative falling segments, advanced once per
    // scan. The published minimum of 1.5 ms is shorter than one scan pass, so
    // the shortest attack the instrument can actually produce is one pass
    // long. There is no per-voice rate error here: the generator is the
    // shared processor, so six voices' envelopes are digitally identical and
    // only the analogue chain after them disperses.
    // Recurrence, widths and coefficient laws are resolved for the supplied,
    // hash-matched B-2 image. All three are the same shared-processor answer
    // for every voice, so each is resolved only when its panel position has
    // actually moved since the last voice asked -- see the note on the
    // envelopeLaw* members.
    if (parameters.attack != envelopeLawAttack_)
    {
        envelopeLawAttack_ = parameters.attack;
        envelopeLawAttackIncrement_ = envelopeAttackIncrement(parameters.attack);
    }
    if (parameters.decay != envelopeLawDecay_)
    {
        envelopeLawDecay_ = parameters.decay;
        envelopeLawDecayMultiplier_ =
            envelopeDecayReleaseMultiplier(parameters.decay);
    }
    if (parameters.release != envelopeLawRelease_)
    {
        envelopeLawRelease_ = parameters.release;
        envelopeLawReleaseMultiplier_ =
            envelopeDecayReleaseMultiplier(parameters.release);
    }
    voice.attackIncrement = envelopeLawAttackIncrement_;
    voice.decayMultiplier = envelopeLawDecayMultiplier_;
    voice.releaseMultiplier = envelopeLawReleaseMultiplier_;

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
        // With either run bit set this is a legato pitch message and the DCO
        // stays free-running. During an ordinary release both bits are clear,
        // so the voice CPU applies its normal different-pitch reset rule.
        if (pitchChangeRequestsDcoReset(voice, voiceMidi))
            voice.dcoResetPending = true;
        voice.lastVoiceMidi = voiceMidi;
        voice.hasVoicePitchHistory = true;
    }

    // Taken from the control as it stands, not from what it read when the key
    // went down. The glide rate is a resistance in the pitch integrator's path,
    // and turning that control while a note is sliding changes the slide --
    // including turning it off, which lands the note on its pitch at the next
    // scan rather than leaving it crawling. Every sounding voice reads the same
    // shared PORTAMENTO position here, so this goes through the memoized
    // resolver rather than recomputing the table lookup once per voice.
    voice.glideSemitonesPerScan = resolveGlideStepPerScan(
        portamentoTravelAdcFraction(parameters.portamento));

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

    const std::int32_t controlOffset =
        static_cast<std::int32_t>(
            masterTunePitchWordOffset(parameters.masterTuneCents))
        + dcoPitchBendWord_ + dcoLfoPitchWord_;

    const DcoPitchPair pitch = dcoPitchPair(aggregatePitchWord(
        static_cast<double>(voice.currentMidi), controlOffset));
    // Roland explicitly says DCO CV contains no RANGE data: RANGE changes the
    // timer clock and selects the ramp resistor, not this compensation hold.
    // https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=9
    // This is the unshifted 12-bit code the firmware writes for the same 8.8
    // pitch coordinate. The paired count is staged below by the converter
    // destination. A
    // running M82C53 count-only write leaves the active CE/period untouched
    // until the next OUT transition. The code reaches the integrator through
    // the hold capacitor's slew; code*active-count is the relative ramp charge
    // rather than a fabricated hertz proxy.
    voice.dcoCvTarget = static_cast<float>(pitch.cvCode);
    return pitch.divider;
}

void YouKnow106Engine::updateVoiceVcfTarget(
    Voice& voice, const EngineParameters& parameters, float lfoGated) noexcept
{
    voice.cutoffCountsTarget = voiceVcfTarget(
        voice, parameters, lfoGated);
}

float YouKnow106Engine::voiceVcfTarget(
    const Voice& voice, const EngineParameters& parameters,
    float lfoGated) const noexcept
{
    const auto byte7 = [](float value) { return storedControlFraction(value); };
    const float envelope = voice.envelope.value;

    // --- Filter cutoff, summed in converter counts ------------------------
    float counts = vcfPanelCounts(parameters.cutoff);
    const float envelopeSign =
        parameters.envPolarity == EnvPolarity::Normal ? 1.0f : -1.0f;
    // The velocity extension rides here rather than on a curve of its own.
    // ENV into the VCF is the only path this instrument has from the envelope
    // to the cutoff, so scaling its amount by the same gain the amplifier
    // applies makes a quieter note one whose filter envelope opened less far
    // -- with no new law and no new constant. The panel byte is still the
    // byte the firmware stored; the extension multiplies what that byte asks
    // for, exactly as it multiplies the amplifier's own control.
    counts += envelopeSign * byte7(parameters.envDepth) * vcfEnvelopeCounts
            * envelope * velocityGain(parameters, voice);
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
    const float code = vcfDacCountStep * std::floor(counts / vcfDacCountStep);
    // The ladder's own integral non-linearity rides on the code it just
    // produced, so it stays on the hold capacitor and reaches the filter.
    // Crossing mid-scale on a slow sweep therefore steps by about 23 cents,
    // as a real card's does.
    return code + vcfConverterCarryCounts(code) * parameters.calibration;
}

void YouKnow106Engine::updateVoiceVcaTarget(
    Voice& voice, const EngineParameters& parameters) noexcept
{
    voice.vcaControlTarget = voiceVcaTarget(voice, parameters);
}

float YouKnow106Engine::voiceVcaTarget(
    const Voice& voice, const EngineParameters& parameters) const noexcept
{
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float tolerance = parameters.calibration;
    const float envelope = voice.envelope.value;

    // --- Amplifier control ------------------------------------------------
    const float control = parameters.vcaMode == VcaMode::Envelope
                        ? envelope
                        : (voice.keyDown || voice.sustained ? 1.0f : 0.0f);
    return clamp01(
        control * velocityGain(parameters, voice)
        + card.vcaControlOffset * 0.004f * tolerance);
}

float YouKnow106Engine::velocityGain(const EngineParameters& parameters,
                                     const Voice& voice) noexcept
{
    return 1.0f - parameters.velocityDepth * (1.0f - voice.velocity);
}

void YouKnow106Engine::updateSharedScan(
    const EngineParameters& parameters) noexcept
{
    resonanceCvTarget_ = converterDacFraction(parameters.resonance);
    sharedVcaTarget_ = converterDacFraction(parameters.vcaLevel);
    subCvTarget_ = converterDacFraction(parameters.subLevel);
    noiseCvTarget_ = converterDacFraction(parameters.noiseLevel);

    // A pre-audio snapshot is the one exceptional direct prime. Once the scan
    // runs, this same code is formed beside advanceLfo and held for the PWM
    // destination just as B-2 holds FF4F.
    converterPassPwmDacCode_ = parameters.pulseEnabled
        ? pwmDacCode(parameters.pwmDepth, parameters.pwmSource,
                     lfoAccumulator_, lfoPolarity_ >= 0.0f)
        : 0u;
    pwmVoltsTarget_ = pwmDacVolts(converterPassPwmDacCode_);
}

void YouKnow106Engine::performConverterWrite(
    const ConverterWrite& write, const EngineParameters& parameters,
    float lfoGated, const float* passiveHoldTargetOverride) noexcept
{
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(converterWrites, 1);
#endif
    const auto validPhysicalVoice = [&write] {
        return write.voice >= 0 && write.voice < hardwareVoices;
    };

    // No charge-injection or converter-glitch term is applied here. Both were
    // written into cutoffCountsTarget / vcaControlTarget, which the switch
    // below assigns from scratch on the very same write, so neither could ever
    // reach the output -- the isolated comparison renders measured both at
    // -360 dBc, bit-identical. A physical injection would have to land on the
    // *slewed hold state*. The installed mux and 0.01-uF hold are known, but
    // the loaded injection and acquisition behavior are not (OQ-07/08).
    switch (write.destination)
    {
        case ConverterDestination::Resonance:
            resonanceCvTarget_ = passiveHoldTargetOverride != nullptr
                ? *passiveHoldTargetOverride
                : converterDacFraction(parameters.resonance);
            break;
        case ConverterDestination::CommonVca:
            sharedVcaTarget_ = passiveHoldTargetOverride != nullptr
                ? *passiveHoldTargetOverride
                : converterDacFraction(parameters.vcaLevel);
            break;
        case ConverterDestination::Sub:
            subCvTarget_ = passiveHoldTargetOverride != nullptr
                ? *passiveHoldTargetOverride
                : converterDacFraction(parameters.subLevel);
            break;
        case ConverterDestination::Pitch:
            if (validPhysicalVoice())
            {
                auto& voice = voices_[static_cast<std::size_t>(write.voice)];
                if (voice.dcoPitchTransactionValid)
                {
                    voice.dcoCvTarget =
                        voice.dcoPitchTransactionCvTarget;
                    if (voice.dcoPitchTransactionColdStart)
                        voice.dcoCv = voice.dcoCvTarget;
                    voice.dcoPitchTransactionValid = false;
                    voice.dcoPitchTransactionColdStart = false;
                }
                else
                {
                    // The first PhaseZeroDiagnostic T has no preceding
                    // modelled pass in which T-389 could exist. Keep that
                    // construction-only/direct-test fallback deterministic;
                    // every subsequent physical transaction is pre-staged.
                    const std::uint32_t count = updateVoiceEnvelopeAndPitch(
                        voice, parameters);
                    programDcoCount(
                        voice, count, voice.dcoResetPending);
                    voice.dcoResetPending = false;
                }
            }
            break;
        case ConverterDestination::Pwm:
            // B-2 computes FF4F directly from the raw LFO accumulator and PWM
            // depth, independently of the onset byte used by DCO and VCF, then
            // writes that saved word on the following envelope loop.
            // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L900-L905
            // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L1144-L1197
            if (passiveHoldTargetOverride != nullptr)
            {
                pwmVoltsTarget_ = *passiveHoldTargetOverride;
                break;
            }
            pwmVoltsTarget_ = pwmDacVolts(converterPassPwmDacCode_);
            break;
        case ConverterDestination::Vcf:
            if (validPhysicalVoice())
            {
                auto& voice =
                    voices_[static_cast<std::size_t>(write.voice)];
                if (passiveHoldTargetOverride != nullptr)
                    voice.cutoffCountsTarget = *passiveHoldTargetOverride;
                else
                    updateVoiceVcfTarget(voice, parameters, lfoGated);
            }
            break;
        case ConverterDestination::VoiceVca:
            if (validPhysicalVoice())
            {
                auto& voice = voices_[static_cast<std::size_t>(write.voice)];
                if (passiveHoldTargetOverride != nullptr)
                    voice.vcaControlTarget = *passiveHoldTargetOverride;
                else
                    updateVoiceVcaTarget(voice, parameters);
            }
            break;
        case ConverterDestination::Noise:
            noiseCvTarget_ = converterDacFraction(parameters.noiseLevel);
            break;
    }
}

bool YouKnow106Engine::isPassiveHoldWrite(
    const ConverterWrite& write) noexcept
{
    switch (write.destination)
    {
        case ConverterDestination::Resonance:
        case ConverterDestination::CommonVca:
        case ConverterDestination::Sub:
        case ConverterDestination::Pwm:
            return true;
        case ConverterDestination::Vcf:
        case ConverterDestination::VoiceVca:
            return write.voice >= 0 && write.voice < hardwareVoices;
        case ConverterDestination::Pitch:
        case ConverterDestination::Noise:
            return false;
    }
    return false;
}

float YouKnow106Engine::passiveHoldWriteTarget(
    const ConverterWrite& write, const EngineParameters& parameters,
    float lfoGated) const noexcept
{
    switch (write.destination)
    {
        case ConverterDestination::Resonance:
            return converterDacFraction(parameters.resonance);
        case ConverterDestination::CommonVca:
            return converterDacFraction(parameters.vcaLevel);
        case ConverterDestination::Sub:
            return converterDacFraction(parameters.subLevel);
        case ConverterDestination::Pwm:
            return pwmDacVolts(converterPassPwmDacCode_);
        case ConverterDestination::Vcf:
            if (write.voice >= 0 && write.voice < hardwareVoices)
                return voiceVcfTarget(
                    voices_[static_cast<std::size_t>(write.voice)],
                    parameters, lfoGated);
            break;
        case ConverterDestination::VoiceVca:
            if (write.voice >= 0 && write.voice < hardwareVoices)
                return voiceVcaTarget(
                    voices_[static_cast<std::size_t>(write.voice)], parameters);
            break;
        case ConverterDestination::Pitch:
        case ConverterDestination::Noise:
            break;
    }
    return 0.0f;
}

void YouKnow106Engine::scheduleUpcomingDcoPitchPrestages(
    double phase, double phasePerInternalSample) noexcept
{
    if (!(phasePerInternalSample > 0.0))
        return;

    constexpr std::size_t firstPitchOrdinal = 3u;
    const double cpuStatesPerInterval = voiceCpuStateHz / oversampledRate_;
    const double stateTolerance = std::max(
        1.0e-9, cpuStatesPerInterval * 1.0e-9);

    for (int slot = 0; slot < hardwareVoices; ++slot)
    {
        auto& voice = voices_[static_cast<std::size_t>(slot)];
        auto& dco = voice.dco;
        if (voice.dcoPitchTransactionValid
            || dco.pitWriteState != Dco::PitWriteState::idle)
            continue;

        const std::size_t ordinal = firstPitchOrdinal
                                  + static_cast<std::size_t>(slot);
        const bool remainsInCurrentPass =
            nextConverterWrite_ < converterWritesPerPass
            && ordinal >= nextConverterWrite_;
        const double nominalEventPhase = converterEventPhases_[ordinal]
                                       + (remainsInCurrentPass ? 0.0 : 1.0);

        // T is the converter event's actual left-boundary poll, rather than
        // the policy profile's ideal phase. Count those exact internal
        // boundaries ahead, then place the recovered preparation 389 CPU
        // states before T. Subtracting the converter poll's own aperture makes
        // a phase lying within 1e-12 of a boundary belong to that boundary,
        // exactly as the queue comparison does.
        const double intervalsToT = std::max(
            1.0, std::ceil(
                (nominalEventPhase - phase - 1.0e-12)
                / phasePerInternalSample));
        const double statesToPrestage =
            intervalsToT * cpuStatesPerInterval - dcoPitchPrestageStates;
        if (statesToPrestage < -stateTolerance
            || statesToPrestage > cpuStatesPerInterval + stateTolerance)
            continue;

        dco.pitWriteState = Dco::PitWriteState::awaitingPitchPrestage;
        dco.cpuStatesToWrite = std::clamp(
            statesToPrestage, 0.0, cpuStatesPerInterval);
    }
}

bool YouKnow106Engine::latchUpcomingPassiveHoldEvent(
    double phase, double phasePerInternalSample,
    const EngineParameters& parameters) noexcept
{
    // prepare() clamps the host to at least 8 kHz. Even with HQ disabled this
    // is at most 5/168 of a pass, smaller than the normalized 1/23 ordinal
    // spacing, so a physical interval can contain no more than one converter
    // write. That fixed bound keeps this a scalar latch rather than a queue.
    static_assert(5.0 / 168.0 < 1.0 / converterWritesPerPass);
    if (passiveHoldEventLatch_.valid || !(phasePerInternalSample > 0.0))
        return false;

    const auto& writes = converterWriteOrder();
    const double intervalEnd = phase + phasePerInternalSample;
    std::size_t ordinal = nextConverterWrite_;
    bool nextPass = false;
    double eventPhase = 0.0;
    if (ordinal < writes.size())
        eventPhase = converterEventPhases_[ordinal];
    else if (intervalEnd >= 1.0)
    {
        ordinal = 0u;
        nextPass = true;
        eventPhase = 1.0 + converterEventPhases_[ordinal];
    }
    else
        return false;

    // The normal boundary poll owns events at the left edge. Peeking is only
    // for the open/closed physical interval (phase, phase + delta].
    if (!(eventPhase > phase + 1.0e-12
          && eventPhase <= intervalEnd + 1.0e-12))
        return false;

    const auto& write = writes[ordinal];
    if (!isPassiveHoldWrite(write))
        return false;

    passiveHoldEventLatch_.valid = true;
    passiveHoldEventLatch_.nextPass = nextPass;
    passiveHoldEventLatch_.ordinal = ordinal;
    passiveHoldEventLatch_.write = write;
    passiveHoldEventLatch_.target = passiveHoldWriteTarget(
        write, parameters, converterPassLfoGated_);
    passiveHoldEventLatch_.eventPosition = std::clamp(
        (eventPhase - phase) / phasePerInternalSample, 0.0, 1.0);
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(passiveHoldFractionalEventPeeks, 1);
    if (write.destination == ConverterDestination::Resonance
        || write.destination == ConverterDestination::Vcf)
        YOUKNOW106_COUNT_DOMAIN_WORK(vcfFractionalEventPeeks, 1);
#endif
    return true;
}

float YouKnow106Engine::CircuitDerivedResonanceProfile::loopGain(
    float panelPosition) noexcept
{
    // Linear above the grounded-base stage's junction onset, zero below it,
    // sharing the voiced profile's amplitude-anchored endpoint. The per-card
    // slope trim cancels here by construction: the calibrated card's full
    // travel lands on the same maximumFeedback the service trim realises.
    const float position = clamp01(panelPosition);
    const float active = std::max(0.0f, position - onsetTravel)
                       / (1.0f - onsetTravel);
    return VoicedResonanceCompatibilityProfile::maximumFeedback * active;
}

void YouKnow106Engine::selectConverterTimingProfile(
    ConverterTimingProfile profile) noexcept
{
    converterTimingProfile_ = profile;
}

float YouKnow106Engine::resonanceFeedbackFor(
    float resonanceCv, const VoiceCard& card, float calibration,
    bool circuitDerivedShape) noexcept
{
    // The regeneration control voltage is shared -- one converter output for
    // all six loops -- but each voice's loop amplifier has its own gain
    // spread.
    // Voiced, and back to being voiced. The service procedure trims each loop
    // to a 4.8 Vpp self-oscillation peak but states no tolerance on the result,
    // so what spread survives the adjustment is documented nowhere located. A
    // revision briefly anchored 5% to a source describing the *untrimmed*
    // component class, which is a different question: a trimmed mechanism's
    // residual is not its parts' tolerance.
    const float resonancePanel = clamp01(resonanceCv
        + card.resonanceError * 0.02f * calibration);
    return circuitDerivedShape
             ? CircuitDerivedResonanceProfile::loopGain(resonancePanel)
             : VoicedResonanceCompatibilityProfile::loopGain(resonancePanel);
}

float YouKnow106Engine::cutoffAnalogCounts(
    float cutoffCounts, const VoiceCard& card, float calibration,
    float powerSupplyDroop) noexcept
{
    // The analogue side of the cutoff chain: the two per-voice trimmers --
    // one scales the control voltage, one offsets it -- imperfectly set, and
    // the slow thermal wander, all riding below the converter's own
    // resolution on the slewed digital value. The five-per-cent scale and
    // tenth-octave offset spans are voiced Unit Character policies, not
    // measured post-calibration residual distributions.
    // A sagging rail pulls the cutoff reference down with it. `calibration`
    // is applied here and only here: the droop state itself is a pure load
    // measure, so this mechanism scales linearly with Unit Character like its
    // eighteen siblings rather than quadratically.
    const float psuCutoffShift =
        -powerSupplyDroop * railToCutoffCountsPerVolt * calibration;
    // The trim residual is bounded by Roland's own printed acceptance
    // (p. 19 procedures 7/8: repeat "until within +/-10 cents" at both check
    // points): one +/-10-cent draw at the code-6272 FREQ point, one at the
    // WIDTH point two octaves up, the line through them elsewhere. The
    // former +/-0.07 octave and +/-5%-of-total-counts here were voiced with
    // no bounding source; a freshly calibrated card cannot legitimately
    // disperse past what the service procedure accepts at the points it
    // checks. Field drift beyond the windows is the separate `aging`
    // mechanism's, precomputed per card.
    const float trimSpanPosition =
        (cutoffCounts - vcfFreqTrimAnchorCounts) / vcfWidthTrimSpanCounts;
    const float trimResidualCounts =
        (card.cutoffOffsetError * (1.0f - trimSpanPosition)
         + card.cutoffScaleError * trimSpanPosition)
        * vcfTrimResidualOctaves * vcfCountsPerOctave * calibration;
    return cutoffCounts
        + trimResidualCounts
        + card.driftValue * 40.0f * calibration
        + psuCutoffShift
        + card.agingCutoffCounts;
}

float YouKnow106Engine::rampCurrentScaleFor(
    const VoiceCard& card, float calibration) noexcept
{
    // The ramp's charging resistor carries the same per-card tolerance class
    // as every other analogue trim here. updatePulseComparator solves the
    // comparator crossing against this cycle's ramp slope, and renderVoice's
    // amplitude has to scale the rendered ramp by that identical slope; the
    // former resolves it here and caches it on the voice (Voice::
    // rampCurrentScale) so the latter reads the exact value the comparator
    // was solved against instead of re-deriving it.
    return 1.0f + card.rampCurrentError * 0.03f * calibration;
}

void YouKnow106Engine::refreshVoiceRampCurrentScales() noexcept
{
    for (auto& voice : voices_)
    {
        const auto& card =
            cards_[static_cast<std::size_t>(voice.cardIndex)];
        voice.rampCurrentScale =
            rampCurrentScaleFor(card, activeParameters_.calibration);
        if (voice.dco.positiveRailHeld)
        {
            const double totalScale =
                static_cast<double>(voice.dco.renderScale)
                * static_cast<double>(voice.rampCurrentScale);
            voice.dco.rampValue = dcoPositiveBaseRail(totalScale);
            voice.dco.rampSlopePerSecond = 0.0;
        }
    }
}

void YouKnow106Engine::updateVoiceAudio(Voice& voice,
                                        const EngineParameters& parameters) noexcept
{
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(voiceAudioUpdates, 1);
#endif
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float tolerance = parameters.calibration;

    voice.feedback = resonanceFeedbackFor(
        resonanceCv_, card, tolerance,
        parameters.useCircuitDerivedResonanceShape);
    voice.inputCompensation =
        VoicedResonanceCompatibilityProfile::inputCompensation(voice.feedback);

    const float analogCounts = cutoffAnalogCounts(
        voice.cutoffCounts, card, tolerance, powerSupplyDroop_);
    // The chain from counts to the physical omega*dt interval costs an exp2
    // and two double pow calls per card, per internal sample -- and it is a
    // pure function of the two values compared here. A card whose hold has
    // settled and whose drift step has not landed presents bit-identical
    // inputs for thousands of samples in a row, which is most of what an idle
    // instrument does. The guard is exact equality, so the cache can only
    // return the value the chain would have recomputed.
    if (analogCounts != voice.cutoffChainCounts
        || voice.feedback != voice.cutoffChainFeedback)
    {
#if defined(YOUKNOW106_WORK_AUDIT)
        YOUKNOW106_COUNT_DOMAIN_WORK(cutoffMemoMisses, 1);
#endif
        const float cutoffHz = vcfEffectiveCutoffHz(analogCounts, voice.feedback);
        const float limited =
            std::min(cutoffHz, static_cast<float>(oversampledRate_) * 0.45f);
        voice.filterOmegaStep = twoPi * limited * inverseOversampledRate_;
        voice.cutoffChainCounts = analogCounts;
        voice.cutoffChainFeedback = voice.feedback;
    }
#if defined(YOUKNOW106_WORK_AUDIT)
    else
    {
        YOUKNOW106_COUNT_DOMAIN_WORK(cutoffMemoHits, 1);
    }
#endif

    // The retire check below the main scan loop asks this same law about
    // this same vcaControl a moment later, to see whether the card has
    // actually gone silent; cache the raw gain so it reads this value
    // instead of paying for another log1p/exp pair.
    voice.vcaGain = VoiceVcaControlLaw::gain(
        static_cast<float>(voice.vcaControl));
    voice.vca = voice.vcaGain
              * (1.0f + card.vcaGainError * 0.03f * tolerance);

    // The IR3109 stage offsets used to be rewritten here, every audio sample,
    // from values that never change. They now live in
    // refreshVoiceCardStageTrims, called where the card or the panel moves.
}

void YouKnow106Engine::updatePulseComparator(
    Voice& voice, const EngineParameters& parameters) noexcept
{
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(pulseComparatorUpdates, 1);
#endif
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    // The comparator compares the threshold against the ramp actually being
    // integrated, whose amplitude this cycle is the *frozen* per-cycle ratio
    // -- the same one the render carries -- not the instantaneous CV ratio,
    // which belongs to the next cycle's slope. Solving the duty against a
    // different amplitude than the rendered ramp put the solved edges on a
    // waveform that did not exist.
    const float cardCurrent = voice.rampCurrentScale;
    const float amplitudeScale = voice.dco.renderScale * cardCurrent;
    // The alignment window is 48% to 52% duty across cards: +/-0.24 V is
    // +/-2 points on the 12 V ramp. Pulse Off remains separate at -0.8 V and
    // pins the comparator high even while this card's VCA is shut.
    const float threshold = static_cast<float>(pwmVolts_)
                          + card.comparatorOffset * 0.24f
                                * parameters.calibration;
    voice.pulseThresholdVolts = sanitised(threshold, 6.0f);
    voice.pulsePinnedHigh = voice.pulseThresholdVolts < 0.0f;
    voice.pulseDuty = pwmDutyCycle(threshold, amplitudeScale);
}

void YouKnow106Engine::primeStartupVoiceWaveNodes(
    const EngineParameters& parameters) noexcept
{
    // A restored pre-audio snapshot describes a powered, already-settled
    // instrument. Prime the WAVE-node coupling capacitor at the periodic
    // source's DC mean so Pulse Off's documented constant-high comparator does
    // not become a fabricated power-on thump on the first note. Saw and SUB
    // are bipolar; the pulse mean is level * (2*duty - 1), including +level
    // for the pinned-high off state.
    for (auto& voice : voices_)
        primeVoiceWaveNode(voice, parameters);
}

float YouKnow106Engine::dcoLaunchScale(const Voice& voice) const noexcept
{
    const bool capturedCountReady = voice.dcoPitchTransactionValid
                                 && voice.dco.pitWriteState
                                        == Dco::PitWriteState::idle;
    const float targetCode = capturedCountReady
        ? voice.dcoPitchTransactionCvTarget : voice.dcoCvTarget;

    // A construction-only cold transaction has no earlier physical hold to
    // inherit. Initialise that first cell from its captured pair rather than
    // from an arbitrary power-on code. T then installs the same captured code
    // as both live target and settled value.
    if (capturedCountReady && voice.dcoPitchTransactionColdStart)
    {
        const float scale = targetCode
                          * static_cast<float>(voice.dco.divider)
                          / dcoRampReferenceProduct;
        return std::clamp(scale, 0.25f, 4.0f);
    }

    // The frozen per-cycle slope stands in for the integral of the slewing
    // compensation current across the rise it charges. Weighting the held-code
    // error by tau/period is that integral to first order: a long bass cycle
    // sees nearly the target code, while a short cycle keeps more of the launch
    // value. Multiplying the resulting code by the active count retains the
    // B-2 pair's settled ripple and its real high-note CV saturation; the
    // centre anchor removes any need to guess DAC volts or integrator gain.
    const float tauSamples = dcoHoldSlewSecondsVoiced
                           * static_cast<float>(oversampledRate_);
    const float weight = std::min(
        1.0f, tauSamples / static_cast<float>(
                  std::max(voice.dco.periodSamples, 1.0)));
    const float effectiveCode = targetCode
                              + (voice.dcoCv - targetCode) * weight;
    const float scale = effectiveCode
                      * static_cast<float>(voice.dco.divider)
                      / dcoRampReferenceProduct;
    return std::clamp(scale, 0.25f, 4.0f);
}

bool YouKnow106Engine::pulseMixEnabled(
    bool requested, float duty, bool couplePinnedLevel) noexcept
{
    return couplePinnedLevel || (requested && duty < 1.0f);
}

float YouKnow106Engine::pulseWaveNodeMean(
    const Voice& voice, const EngineParameters& parameters) noexcept
{
    return pulseMixEnabled(parameters.pulseEnabled, voice.pulseDuty,
                           parameters.enablePulseOffWaveNodeCoupling)
        ? pulseMixVolts * (2.0f * voice.pulseDuty - 1.0f)
        : 0.0f;
}

void YouKnow106Engine::primeVoiceWaveNode(
    Voice& voice, const EngineParameters& parameters) noexcept
{
    updatePulseComparator(voice, parameters);
    auto& dco = voice.dco;
    const double totalScale = static_cast<double>(dco.renderScale)
                            * static_cast<double>(voice.rampCurrentScale);
    const double rampVolts = 0.5 * static_cast<double>(rampAmplitudeVolts)
                           * totalScale * (dco.rampValue + 1.0);
    dco.pulseState = voice.pulsePinnedHigh
                  || rampVolts >= voice.pulseThresholdVolts
                   ? 1.0f : -1.0f;
    dco.pulse.reset();
    voice.previousPulseThresholdVolts = voice.pulseThresholdVolts;
    voice.previousPulsePinnedHigh = voice.pulsePinnedHigh;
    voice.pulseThresholdPrimed = true;
    voice.moduleCoupling.state = static_cast<double>(
        pulseWaveNodeMean(voice, parameters));
}

// ---------------------------------------------------------------------------
// Voice rendering
// ---------------------------------------------------------------------------

void YouKnow106Engine::advanceThermalWarmup() noexcept
{
    // Wall-clock seconds, not internal samples: the chassis warms at the same
    // rate whatever grid the model is solved on. The total is double so the
    // increment cannot round away against it -- see the member's declaration.
    thermalWarmupSeconds_ += inverseOversampledRate_;
    // The warm-up fraction is one chassis-wide number, advanced beside the
    // timer it derives from. Six voices asking six times per internal sample
    // for the same exponential of the same elapsed time is the same answer at
    // six times the price.
    thermalWarmupFraction_ =
        1.0f - std::exp(-static_cast<float>(thermalWarmupSeconds_) / 900.0f);
}

float YouKnow106Engine::dynamicOtaHeadroomVolts(
    const EngineParameters& parameters, int cardIndex) const noexcept
{
    const float psuThermalOffset = parameters.enableSpatialThermalGradient
        ? chassisGradientCelsius(cardIndex) * parameters.calibration
        : 0.0f;
    const float tempRise = 15.0f * parameters.calibration;
    const float tempC =
        25.0f + psuThermalOffset + tempRise * thermalWarmupFraction_;
    const float dynamicThermalVoltage = 0.026f * ((tempC + 273.15f) / 298.15f);
    return 2.0f * dynamicThermalVoltage / stageAttenuation;
}

void YouKnow106Engine::advanceDcoPitAndRamp(
    Voice& voice, DcoRange range, float previousThresholdVolts,
    float thresholdVolts, bool previousPinnedHigh, bool pinnedHigh,
    bool addCorrections) noexcept
{
    auto& dco = voice.dco;
    const double intervalSeconds = 1.0 / oversampledRate_;
    // Treat analytically coincident ramp/PIT corners as one event. This is a
    // billionth of an internal interval, many orders below the 1/64-sample
    // correction-table grid, but large enough to absorb final-operation ULPs.
    const double eventToleranceSeconds = std::max(
        1.0e-15, intervalSeconds * 1.0e-9);
    const double pitClockHz = rangeClockHz(range);
    const double thresholdStart = std::isfinite(previousThresholdVolts)
        ? static_cast<double>(previousThresholdVolts) : 6.0;
    const double thresholdEnd = std::isfinite(thresholdVolts)
        ? static_cast<double>(thresholdVolts) : thresholdStart;
    const double thresholdSlope = intervalSeconds > 0.0
        ? (thresholdEnd - thresholdStart) / intervalSeconds : 0.0;
    const bool comparatorPinnedForInterval =
        previousPinnedHigh && pinnedHigh;
    if (addCorrections && comparatorPinnedForInterval)
    {
        // Both endpoints are below the zero-volt ramp, so their linear
        // trajectory is below it for the whole interval. Do not synthesize
        // crossings or accumulate a correction for Pulse Off's pinned level.
        dco.pulseState = 1.0f;
    }

    // A supply-held capacitor stays at +15 V when Unit Character changes the
    // scale used to express that voltage in the base-ramp coordinate. This is
    // a coordinate reprojection, not a capacitor step, so it creates no BLEP.
    const double initialTotalRampScale =
        static_cast<double>(dco.renderScale)
        * static_cast<double>(voice.rampCurrentScale);
    if (dco.positiveRailHeld)
    {
        dco.rampValue = dcoPositiveBaseRail(initialTotalRampScale);
        dco.rampSlopePerSecond = 0.0;
    }

    const auto eventSamplesAgo = [&](double elapsed) {
        return intervalSeconds > 0.0
            ? static_cast<float>(std::clamp(
                  (intervalSeconds - elapsed) / intervalSeconds, 0.0, 1.0))
            : 0.0f;
    };
    const auto clocksToNextPitInputFalling = [&](double atElapsed) {
        return rangeClockClocksToNextFallingEdge(atElapsed, range);
    };

    // A live card-current edit changes the physical interpretation of the
    // retained base coordinate before this interval begins. Reconcile that
    // left-boundary truth here; otherwise a stationary ramp can cross solely
    // from the scale change and be repaired incorrectly at samplesAgo=0.
    if (addCorrections && !comparatorPinnedForInterval)
    {
        const double rampVolts = 0.5 * rampAmplitudeVolts
            * initialTotalRampScale * (dco.rampValue + 1.0);
        const float stateAtStart = rampVolts >= thresholdStart
            ? 1.0f : -1.0f;
        if (stateAtStart != dco.pulseState)
        {
            addStep(dco.pulse, stateAtStart - dco.pulseState, 1.0f);
            dco.pulseState = stateAtStart;
#if defined(YOUKNOW106_WORK_AUDIT)
            YOUKNOW106_COUNT_DOMAIN_WORK(dcoComparatorTransitions, 1);
#endif
        }
    }

    double elapsed = 0.0;
    // At 8 kHz, 1x, the 4 MHz range clock and divider 8, the closed interval
    // can contain 126 OUT transitions plus 63 reset completions and 63 supply
    // hits. A fixed pitch transaction adds one pre-stage and two byte events;
    // 512 leaves a real margin over the 254-event supported worst case while
    // still bounding malformed state without allocating anything. IC35 reload
    // is chassis-global and does not add one event to each card's walk.
    constexpr int maximumEventsPerInterval = 512;
    for (int eventIndex = 0;
         eventIndex < maximumEventsPerInterval
             && elapsed < intervalSeconds - eventToleranceSeconds;
         ++eventIndex)
    {
        const double remaining = intervalSeconds - elapsed;
        const double pitSeconds =
            dco.pitState == Dco::PitState::stopped
                || dco.pitState == Dco::PitState::awaitingCount
            ? std::numeric_limits<double>::infinity()
            : std::max(0.0, dco.pitClocksToEvent) / pitClockHz;
        const double cpuWriteSeconds =
            dco.pitWriteState == Dco::PitWriteState::idle
                ? std::numeric_limits<double>::infinity()
                : std::max(0.0, dco.cpuStatesToWrite) / voiceCpuStateHz;
        const double resetSeconds = dco.resetSecondsRemaining > 0.0
            ? dco.resetSecondsRemaining
            : std::numeric_limits<double>::infinity();
        // The current-source ramp cannot charge beyond the ideal +15 V supply
        // bound. A newly staged long count can otherwise leave the old
        // high-note slope running through a hybrid half-cycle and produce
        // hundreds of rails of impossible capacitor voltage before the next
        // positive OUT edge.
        const double totalRampScale =
            static_cast<double>(dco.renderScale)
            * static_cast<double>(voice.rampCurrentScale);
        const double positiveBaseRail =
            dcoPositiveBaseRail(totalRampScale);
        const bool positiveRailNeedsValueClamp =
            dco.rampValue > positiveBaseRail;
        const double positiveRailSeconds = positiveRailNeedsValueClamp
            ? 0.0
            : (dco.rampSlopePerSecond > 0.0
                   ? std::max(0.0, (positiveBaseRail - dco.rampValue)
                                     / dco.rampSlopePerSecond)
                   : std::numeric_limits<double>::infinity());
        const double segment = std::min(
            { remaining, pitSeconds, cpuWriteSeconds, resetSeconds,
              positiveRailSeconds });

        if (addCorrections && !comparatorPinnedForInterval && segment > 0.0)
        {
            // Solve the physical comparator directly. renderScale can change
            // when a reset completes inside this interval; deriving ramp volts
            // per segment gives the suffix its new scale without delaying the
            // threshold remap to the next sample.
            const double threshold = thresholdStart
                + (thresholdEnd - thresholdStart)
                    * (elapsed / intervalSeconds);
            const double rampVolts = 0.5 * rampAmplitudeVolts
                * totalRampScale * (dco.rampValue + 1.0);
            const double rampSlopeVolts = 0.5 * rampAmplitudeVolts
                * totalRampScale * dco.rampSlopePerSecond;
            const double relativeSlope = rampSlopeVolts - thresholdSlope;
            if (std::abs(relativeSlope) > 1.0e-14)
            {
                const double crossing = (threshold - rampVolts)
                                      / relativeSlope;
                if (crossing >= -eventToleranceSeconds
                    && crossing <= segment + eventToleranceSeconds)
                {
                    const float state = relativeSlope > 0.0 ? 1.0f : -1.0f;
                    if (state != dco.pulseState)
                    {
                        addStep(dco.pulse, state - dco.pulseState,
                                eventSamplesAgo(elapsed + std::min(
                                    std::max(0.0, crossing), segment)));
                        dco.pulseState = state;
#if defined(YOUKNOW106_WORK_AUDIT)
                        YOUKNOW106_COUNT_DOMAIN_WORK(
                            dcoComparatorTransitions, 1);
#endif
                    }
                }
            }
        }

        dco.rampValue += dco.rampSlopePerSecond * segment;
        if (dco.resetSecondsRemaining > 0.0)
            dco.resetSecondsRemaining = std::max(
                0.0, dco.resetSecondsRemaining - segment);
        if (dco.pitState == Dco::PitState::awaitingCount)
        {
            // CE is stopped, not TP5. Follow the chassis-global falling/count
            // phase analytically while the two count bytes are in flight.
            // Exact equality is PIT-first policy, so the completed count waits
            // for the following falling edge.
            dco.pitClocksToEvent = clocksToNextPitInputFalling(
                elapsed + segment);
        }
        else if (dco.pitState != Dco::PitState::stopped)
            dco.pitClocksToEvent = std::max(
                0.0, dco.pitClocksToEvent - segment * pitClockHz);
        if (dco.pitWriteState != Dco::PitWriteState::idle)
            dco.cpuStatesToWrite = std::max(
                0.0, dco.cpuStatesToWrite - segment * voiceCpuStateHz);
        elapsed += segment;

        const bool resetComplete =
            resetSeconds <= segment + eventToleranceSeconds;
        const bool positiveRailHit =
            positiveRailSeconds <= segment + eventToleranceSeconds;
        const bool pitEvent =
            pitSeconds <= segment + eventToleranceSeconds;
        const bool cpuWriteEvent =
            cpuWriteSeconds <= segment + eventToleranceSeconds;
        if (!resetComplete && !positiveRailHit && !pitEvent
            && !cpuWriteEvent)
            break;

        // Complete an older C54 discharge before processing a coincident new
        // OUT edge. The ordering is deterministic; the two events cannot
        // coincide in the supported steady-state count range.
        if (resetComplete)
            beginDcoCharge(
                voice, eventSamplesAgo(elapsed), addCorrections);

        // If a scaled cycle reaches the supply bound exactly at the next
        // positive OUT edge, preserve the incoming charge slope so discharge
        // emits one direct BLAMP correction rather than two coincident table
        // walks through an artificial zero-slope state. A genuinely early rail
        // (or a coincident non-rising PIT event) still enters the hold above.
        const bool railMeetsRisingOut = positiveRailHit && pitEvent
            && dco.pitState == Dco::PitState::running && !dco.pitOutHigh;
        if (positiveRailHit)
        {
            const double valueBeforeClamp = dco.rampValue;
            const bool chargingIntoRail = dco.rampSlopePerSecond > 0.0;
            const float oldSlope = static_cast<float>(
                dco.rampSlopePerSecond
                * static_cast<double>(dco.renderScale)
                * intervalSeconds);
            dco.rampValue = positiveBaseRail;
            if (addCorrections && dco.saw.primed
                && positiveRailNeedsValueClamp)
            {
                const float valueStep = static_cast<float>(
                    (positiveBaseRail - valueBeforeClamp)
                    * static_cast<double>(dco.renderScale));
                addStep(dco.saw, valueStep, eventSamplesAgo(elapsed));
            }

            // Charging reaches a real supply hold. A falling reset discovered
            // above a newly lowered coordinate rail is clamped at t=0 but
            // keeps falling; a live calibration change must not pause it.
            if (chargingIntoRail && !railMeetsRisingOut)
            {
                dco.rampSlopePerSecond = 0.0;
                dco.positiveRailHeld = true;
                if (addCorrections && dco.saw.primed)
                    addSlope(dco.saw, -oldSlope, eventSamplesAgo(elapsed));
            }
            else if (dco.rampSlopePerSecond < 0.0
                     && dco.resetSecondsRemaining > 0.0)
            {
                // The value clamp shortens the remaining fall. Retarget its
                // slope so the unchanged reset deadline still lands exactly on
                // -1 rather than carrying the old slope below the low rail.
                dco.rampSlopePerSecond =
                    (-1.0 - dco.rampValue) / dco.resetSecondsRemaining;
                const float newSlope = static_cast<float>(
                    dco.rampSlopePerSecond
                    * static_cast<double>(dco.renderScale)
                    * intervalSeconds);
                if (addCorrections && dco.saw.primed)
                    addSlope(dco.saw, newSlope - oldSlope,
                             eventSamplesAgo(elapsed));
            }
        }

        // This block deliberately precedes the byte-write block: a running
        // OUT transition tied with the MSB still belongs to the old count.
        if (pitEvent)
        {
            const auto event = dco.consumePitEvent();
            updateActiveDcoPeriod(dco, range);
            if (event == Dco::PitEvent::initialLoad)
            {
                // Cold start has no recovered pre-program capacitor state. Hold
                // the established low rail through one modelled reset interval,
                // then launch the first rise. This is initialization policy, not
                // a claim that loading CE generated an OUT edge.
                if (dco.coldInitialLoadPending)
                {
                    dco.coldInitialLoadPending = false;
                    dco.positiveRailHeld = false;
                    dco.rampValue = -1.0;
                    dco.rampSlopePerSecond = 0.0;
                    const double periodSeconds = std::max(
                        dco.periodSamples / oversampledRate_, 1.0e-12);
                    dco.resetSecondsRemaining = static_cast<double>(
                        resetFraction(periodSeconds)) * periodSeconds;
                }
            }
            else if (event == Dco::PitEvent::risingEdge)
            {
                beginDcoDischarge(
                    voice, eventSamplesAgo(elapsed), addCorrections);
            }
        }

        if (cpuWriteEvent)
        {
            if (dco.pitWriteState
                == Dco::PitWriteState::awaitingPitchPrestage)
            {
                // T is the existing converter boundary poll and the start of
                // ANI PA,$EF. The recovered no-interrupt paths place reset
                // control at T-389 states, running LSB at T-334, and both MSBs
                // at T-323. Compute pitch once at the common earliest point;
                // this is also where release-time transpose can request reset.
                // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L732-L741
                // https://github.com/ErroneousBosh/j106roms/blob/26926a04ff1939106820313e71e34b4ca2f67070/ic29.txt#L783-L794
                prestageDcoPitchTransaction(
                    voice, clocksToNextPitInputFalling(elapsed),
                    eventSamplesAgo(elapsed), addCorrections);
            }
            else if (dco.pitWriteState == Dco::PitWriteState::awaitingLsb)
            {
                dco.pitWriteState = Dco::PitWriteState::awaitingMsb;
                dco.cpuStatesToWrite = pitLsbToMsbStates;
            }
            else if (dco.pitWriteState == Dco::PitWriteState::awaitingMsb)
            {
                dco.stageMode3Count(dco.pitWriteDivider);
                dco.pitWriteState = Dco::PitWriteState::idle;
                dco.cpuStatesToWrite = 0.0;
            }
        }
    }

    if (addCorrections)
    {
        if (!comparatorPinnedForInterval)
        {
            const double totalRampScale =
                static_cast<double>(dco.renderScale)
                * static_cast<double>(voice.rampCurrentScale);
            const double rampVolts = 0.5 * rampAmplitudeVolts
                * totalRampScale * (dco.rampValue + 1.0);
            const float comparatorAtEnd = pinnedHigh
                ? 1.0f
                : (rampVolts >= thresholdEnd ? 1.0f : -1.0f);
            if (comparatorAtEnd != dco.pulseState)
            {
                addStep(dco.pulse, comparatorAtEnd - dco.pulseState, 0.0f);
                dco.pulseState = comparatorAtEnd;
#if defined(YOUKNOW106_WORK_AUDIT)
                YOUKNOW106_COUNT_DOMAIN_WORK(dcoComparatorTransitions, 1);
#endif
            }
        }
        voice.previousPulseThresholdVolts = static_cast<float>(thresholdEnd);
        voice.previousPulsePinnedHigh = pinnedHigh;
        voice.pulseThresholdPrimed = true;
    }
}

void YouKnow106Engine::freewheelVoiceCard(Voice& voice) noexcept
{
    voice.freewheeling = true;
    advanceDcoPitAndRamp(
        voice, activeParameters_.range,
        voice.pulseThresholdVolts, voice.pulseThresholdVolts,
        voice.pulsePinnedHigh, voice.pulsePinnedHigh, false);

    // The card-local microscopic noise source keeps running.
    voice.noiseState = xorshift32(voice.noiseState);

    // C56/C50 is a 0.482 Hz physical state, not reconstruction work. Follow the
    // free-running ramp/comparator endpoint at low pitch without paying for its
    // inaudible BLEP or filter solve. Above 100 Hz use the exact duty mean: at
    // the low 8 kHz processing boundary this also avoids sampling a >Nyquist
    // comparator into false DC, while the omitted capacitor ripple is bounded
    // to about 45 mV at the crossover and falls with frequency. Regressions
    // compare both the lowest pitch and the >1-cycle/sample extreme to Exact.
    // The legacy A/B path deliberately retains its former frozen state.
    if (activeParameters_.enablePulseOffWaveNodeCoupling)
    {
        auto& dco = voice.dco;
        const double totalScale = static_cast<double>(dco.renderScale)
                                * static_cast<double>(voice.rampCurrentScale);
        const double rampVolts = 0.5 * static_cast<double>(rampAmplitudeVolts)
                               * totalScale * (dco.rampValue + 1.0);
        dco.pulseState = voice.pulsePinnedHigh
                      || rampVolts >= voice.pulseThresholdVolts
                       ? 1.0f : -1.0f;
        constexpr double endpointTrackingMaximumHz = 100.0;
        const double carrierHz = oversampledRate_
            / std::max(dco.periodSamples, 1.0);
        const float pulseNode = carrierHz <= endpointTrackingMaximumHz
            ? dco.pulseState * pulseMixVolts
            : pulseWaveNodeMean(voice, activeParameters_);
        static_cast<void>(voice.moduleCoupling.process(
            pulseNode, moduleCouplingG_, 0.0f, 1.0f));
        voice.previousPulseThresholdVolts = voice.pulseThresholdVolts;
        voice.previousPulsePinnedHigh = voice.pulsePinnedHigh;
        voice.pulseThresholdPrimed = true;
    }
}

YouKnow106Engine::VoiceFilterFrame YouKnow106Engine::prepareVoiceFilter(
    Voice& voice, const EngineParameters& parameters,
    float noiseSample) noexcept
{
    // Extension slots have no continuously powered voice card behind them.
    // Their digital portamento state still advances on the converter pass,
    // but an unassigned slot has no DCO/filter/audio state that must run.
    if (!voice.active && voice.cardIndex >= hardwareVoices)
        return {};

    // A retired physical card's render is computed and then discarded: the
    // caller drops the sample, so the only thing this pass can change is the
    // free-running state a later reassignment starts from. Under the fast
    // tanh modes that state is advanced directly, at a fraction of the cost.
    // Exact keeps the established full render, so the reference kernel's
    // frozen fingerprints and work counters are untouched -- and switching
    // back to Exact is the way to switch this behaviour off.
    if (!voice.active && parameters.vcfTanhMode != VcfTanhMode::Exact)
    {
        freewheelVoiceCard(voice);
        return {};
    }

#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(dcoFrames, 1);
#endif
    auto& dco = voice.dco;
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const auto boundedOmegaStep = [&](float baseOmegaStep) {
        return static_cast<float>(OtaCascade::clampOmegaStep(
            static_cast<double>(baseOmegaStep)
                * card.thermalFilterOmegaScale));
    };

    // One shared event walk owns the M82C53 half-cycles, C54 ramp and
    // comparator. The timer runs from the selected crystal-derived clock; card
    // temperature therefore has no pitch term.
    const float thresholdVolts = voice.pulseThresholdVolts;
    const float previousThresholdVolts = voice.pulseThresholdPrimed
        ? voice.previousPulseThresholdVolts : thresholdVolts;
    const bool previousPinnedHigh = voice.pulseThresholdPrimed
        ? voice.previousPulsePinnedHigh : voice.pulsePinnedHigh;
    advanceDcoPitAndRamp(
        voice, parameters.range, previousThresholdVolts, thresholdVolts,
        previousPinnedHigh, voice.pulsePinnedHigh, true);

    // The compensation ratio is frozen when the discharge reaches the low rail.
    // Mapping about that rail keeps the capacitor voltage continuous when the
    // new charge slope is launched.
    const float sawNaive = static_cast<float>(
        dco.rampValue * static_cast<double>(dco.renderScale)
        + (static_cast<double>(dco.renderScale) - 1.0));
    const float amplitude = sawMixVolts * voice.rampCurrentScale;
    const float sawOut = dco.saw.advance(sawNaive) * amplitude;

    // Comparator and sub transitions were inserted at their PIT/ramp event
    // timestamps above; their logic levels are independent of ramp amplitude.
    const float pulseOut =
        dco.pulse.advance(dco.pulseState) * pulseMixVolts;
    const float subGain = subMixVolts * static_cast<float>(subCv_)
        * (1.0f + card.subLevelError * 0.03f * parameters.calibration);
    const float subOut = dco.sub.advance(dco.subState) * subGain;

    // --- Summing node --------------------------------------------------------
    // Module p. 13 (2026-08-07 designator read): saw and pulse leave the
    // waveshaper already summed on ONE per-voice WAVE output (IC12/IC8/IC4
    // pin 14 or 16), the sub joins that line through R101/R97 27k behind
    // D6/D5 from its own switch transistor -- 60k of conducting-path series
    // resistance back to the SUB LEVEL rail once R102/R99's 33k collector load
    // is counted (2026-08-20 junction read) -- the shared noise rail arrives
    // on its own leg, and C56/C50 couple the node into the voice module's
    // input.
    // No panel switch reaches this node: SAW is gated by a control rail at
    // the generator ("0: saw ON" at Tr24/R148), PULSE by the -0.8 V hold that
    // pins the comparator high on the fixed WAVE output, SUB by its collector
    // supply and NOISE by the level OTA. Exact pinned-node voltage and loading
    // remain OQ-15/OQ-11.
    // Generators mute or clamp; legs never switch. The node's loading is one
    // configuration-independent constant, which the established
    // filterInputAttenuation coordinate already absorbs -- an earlier
    // revision modelled four switchable 100k legs against the module's 68k
    // and attenuated any patch with both waveforms on by a phantom 1.76 dB.
    // The absolute source-to-filter budget remains OQ-15.
    float mixed = 0.0f;
    if (parameters.sawEnabled)
        mixed += sawOut;
    if (pulseMixEnabled(parameters.pulseEnabled, voice.pulseDuty,
                        parameters.enablePulseOffWaveNodeCoupling))
        mixed += pulseOut;
    mixed += subOut;
    mixed += noiseSample
           * (1.0f + card.noiseLevelError * 0.03f * parameters.calibration)
           * agedNoiseGain_;

    voice.noiseState = xorshift32(voice.noiseState);
    const float microscopicNoise =
        bipolarFromState(voice.noiseState) * filterNoiseVolts;

    // --- Filter, amplifier -------------------------------------------------
    // C56/C50 stand between the summed WAVE node and pin 1 VCF IN, so the
    // module -- and the voice VCA behind it -- never see the mixer's DC. An
    // enabled pulse carries the largest of it: the comparator's output is a
    // duty-asymmetric square, so its mean walks with PWM (at the 95 % duty the
    // hold's 0.6 V endpoint reaches, mean = 6 V * (2d - 1) = 5.4 V at the
    // node). Passed straight through, that DC would multiply by the envelope
    // in the voice VCA and leave an envelope-shaped thump that got *louder*
    // with PWM depth -- a step the instrument does not make.
    //
    // The panel HPF is a different stage and stays where it is: the schematic
    // puts it on the jack board, downstream of the summing amplifier, so it is
    // one shared stage after all six voices rather than a leg inside each --
    // see the mix.
    const float coupled = voice.moduleCoupling.process(
        mixed, moduleCouplingG_, 0.0f, 1.0f);
    // The microscopic card excitation is injected at the filter input, after
    // the source coordinate scale, so it stays outside this capacitor (OQ-16).
    const float filterInput = coupled * filterInputAttenuation
                            * voice.inputCompensation
                            + microscopicNoise * noiseRateScale_;
    // Physical thermal warmup curve: V_t(T) = k * T / q from 25°C to 40°C.
    const float dynamicHeadroom =
        dynamicOtaHeadroomVolts(parameters, voice.cardIndex);
    // The same gradient, through the control path's own temperature
    // coefficient, and about the six-card mean: the FREQ trim is set with the
    // instrument warm, so a calibrated unit carries the spread and not the
    // mean. Roughly +/-10 cents at Unit Character 1, an upper bound on what
    // R111's positor leaves uncancelled.
    // The continuous cascade advances directly at this internal boundary.
    // Two fixed Merson half-steps are a numerical solver, not a higher
    // modelled sample rate: they add neither an inter-domain boundary nor
    // latency. Apply the product-grid cap after the thermal card spread so Unit
    // Character cannot push the numerical interval past the proven boundary.
    const float effectiveFilterOmegaStep = boundedOmegaStep(
        voice.filterOmegaStep);
    OtaCascade::ControlTrajectory eventControlTrajectory;
    const OtaCascade::ControlTrajectory* controlTrajectory = nullptr;
    if (exactVcfControlInterval_[
            static_cast<std::size_t>(voice.cardIndex)])
    {
#if defined(YOUKNOW106_WORK_AUDIT)
        YOUKNOW106_COUNT_DOMAIN_WORK(vcfExactControlMaps, 6);
#endif
        const auto& cutoffInterval = cutoffVcfHoldIntervals_[
            static_cast<std::size_t>(voice.cardIndex)];
        const auto& resonanceInterval = resonanceVcfHoldInterval_;

        struct VcfControl
        {
            double omega {};
            double feedback {};
        };
        const auto mapHeldControls = [&](double cutoffCounts,
                                         double resonanceCv) {
            const float mappedFeedback = resonanceFeedbackFor(
                static_cast<float>(resonanceCv), card, parameters.calibration,
                parameters.useCircuitDerivedResonanceShape);
            const float mappedAnalogCounts = cutoffAnalogCounts(
                static_cast<float>(cutoffCounts), card, parameters.calibration,
                powerSupplyDroop_);
            const float cutoffHz = vcfEffectiveCutoffHz(
                mappedAnalogCounts, mappedFeedback);
            const float limited = std::min(
                cutoffHz, static_cast<float>(oversampledRate_) * 0.45f);
            const float baseOmega = twoPi * limited
                                  * inverseOversampledRate_;
            return VcfControl {
                boundedOmegaStep(baseOmega),
                mappedFeedback
            };
        };

        const auto mappedStart = mapHeldControls(
            cutoffInterval.value.front(),
            resonanceInterval.value.front());
        const VcfControl mappedEnd {
            effectiveFilterOmegaStep, voice.feedback
        };
        const double previousOmega = voice.filter.parameterHistoryPrimed
            ? voice.filter.previousOmegaStep
            : static_cast<double>(effectiveFilterOmegaStep);
        const double previousFeedback = voice.filter.parameterHistoryPrimed
            ? voice.filter.previousFeedback
            : static_cast<double>(voice.feedback);
        const double previousHeadroom = voice.filter.parameterHistoryPrimed
            ? voice.filter.previousHeadroom
            : static_cast<double>(dynamicHeadroom);
        for (std::size_t point = 1;
             point + 1u < OtaCascade::controlNodePositions.size(); ++point)
        {
            const double position =
                OtaCascade::controlNodePositions[point];
            const auto mapped = mapHeldControls(
                cutoffInterval.value[point],
                resonanceInterval.value[point]);
            const double baselineOmega = previousOmega + position
                * (static_cast<double>(effectiveFilterOmegaStep)
                   - previousOmega);
            const double baselineFeedback = previousFeedback + position
                * (static_cast<double>(voice.feedback)
                   - previousFeedback);
            const double mappedLinearOmega = mappedStart.omega + position
                * (mappedEnd.omega - mappedStart.omega);
            const double mappedLinearFeedback = mappedStart.feedback + position
                * (mappedEnd.feedback - mappedStart.feedback);
            eventControlTrajectory.omegaStep[point] = baselineOmega
                + (mapped.omega - mappedLinearOmega);
            eventControlTrajectory.feedback[point] = baselineFeedback
                + (mapped.feedback - mappedLinearFeedback);
            eventControlTrajectory.headroom[point] = previousHeadroom
                + position * (static_cast<double>(dynamicHeadroom)
                              - previousHeadroom);
        }
        // Make the two ownership boundaries exact rather than relying on
        // cancellation of independently rounded mappings. Analogue drift,
        // rail loading and thermal warmup therefore retain the established
        // previous-to-current interpolation; only the hold's within-interval
        // curvature is added at the five interior Merson abscissae.
        eventControlTrajectory.omegaStep.front() = previousOmega;
        eventControlTrajectory.feedback.front() = previousFeedback;
        eventControlTrajectory.headroom.front() = previousHeadroom;
        eventControlTrajectory.omegaStep.back() = effectiveFilterOmegaStep;
        eventControlTrajectory.feedback.back() = voice.feedback;
        eventControlTrajectory.headroom.back() = dynamicHeadroom;
        controlTrajectory = &eventControlTrajectory;
    }
    VoiceFilterFrame frame;
    frame.input = filterInput;
    frame.omegaStep = effectiveFilterOmegaStep;
    frame.headroom = dynamicHeadroom;
    if (controlTrajectory != nullptr)
    {
        frame.trajectory = eventControlTrajectory;
        frame.hasTrajectory = true;
    }
    frame.needsFilter = true;
    return frame;
}

float YouKnow106Engine::finishVoiceFilter(Voice& voice,
                                          float filtered) noexcept
{
    // C59 stands between pin 3 VCF OUT and pin 9 VCA IN, so the amplifier
    // never sees the filter's DC. The cascade makes DC of its own: the stage
    // offsets sit inside the loop, and an enabled pulse arrives duty
    // asymmetric, so the filter output's mean walks with PWM. Passed straight
    // through, that mean would be multiplied by the envelope and leave a
    // duty-dependent thump at every note-on and note-off -- which is what the
    // service procedure's VR30/R112 null exists to remove, and what the
    // module's own capacitor removes before it. The capacitor is a physical
    // node, so it is advanced for an inactive card too, ahead of the early
    // return below.
    const float vcaInput = voice.vcaInputCoupling.process(
        filtered, vcaInputCouplingG_, 0.0f, 1.0f);
    voice.vcaInputVolts = vcaInput;

    if (!voice.active)
        return 0.0f;

    // VR30 injects a signal-input null into the BA662 through R112; it is
    // separate from Tr20's control-current path and is adjusted per card to
    // minimise control-dependent output error. The former term reused
    // vcaControlOffset here, added an unexplained +0.8 mV bias and then
    // multiplied by control and VCA gain, producing an unsupported
    // control-squared pulse. Do not invent a residual until a calibrated
    // TP8--TP13 capture establishes its distribution.
    const float output = vcaInput * voice.vca * voltsToSample;

    voice.energy += voiceEnergyFollower_ * (std::abs(output) - voice.energy);
    return std::isfinite(output) ? output : 0.0f;
}

template <bool useCubicEarly>
float YouKnow106Engine::renderVoice(Voice& voice,
                                    const EngineParameters& parameters,
                                    float noiseSample) noexcept
{
    auto frame = prepareVoiceFilter(voice, parameters, noiseSample);
    if (!frame.needsFilter)
        return 0.0f;
    const auto* trajectory = frame.hasTrajectory ? &frame.trajectory : nullptr;
    const float filtered = voice.filter.process<useCubicEarly>(
        frame.input, frame.omegaStep, voice.feedback, frame.headroom,
        parameters.enableVcfEarlyEffect, parameters.calibration, trajectory,
        parameters.vcfTanhMode, parameters.vcfSolverMode);
    return finishVoiceFilter(voice, filtered);
}

template float YouKnow106Engine::renderVoice<false>(
    Voice&, const EngineParameters&, float) noexcept;
template float YouKnow106Engine::renderVoice<true>(
    Voice&, const EngineParameters&, float) noexcept;

#if defined(YOUKNOW106_HAS_VCF_PAIR_SIMD)
std::array<float, 2> YouKnow106Engine::renderVoicePair(
    Voice& first, Voice& second, const EngineParameters& parameters,
    float noiseSample) noexcept
{
    auto firstFrame = prepareVoiceFilter(first, parameters, noiseSample);
    auto secondFrame = prepareVoiceFilter(second, parameters, noiseSample);
    std::array<float, 2> filtered {};
    bool processed = false;
#if !defined(YOUKNOW106_WORK_AUDIT)
    if (firstFrame.needsFilter && secondFrame.needsFilter
        && !firstFrame.hasTrajectory && !secondFrame.hasTrajectory)
        processed =
            OtaCascade::tryProcessSettledRk4Pair(
                first.filter, firstFrame.input, firstFrame.omegaStep,
                first.feedback, firstFrame.headroom,
                second.filter, secondFrame.input, secondFrame.omegaStep,
                second.feedback, secondFrame.headroom,
                parameters.enableVcfEarlyEffect, parameters.calibration,
                filtered[0], filtered[1])
            || OtaCascade::tryProcessSettledMersonPair(
                first.filter, firstFrame.input, firstFrame.omegaStep,
                first.feedback, firstFrame.headroom,
                second.filter, secondFrame.input, secondFrame.omegaStep,
                second.feedback, secondFrame.headroom,
                parameters.enableVcfEarlyEffect, parameters.calibration,
                filtered[0], filtered[1]);
#endif
    if (!processed)
    {
        if (firstFrame.needsFilter)
            filtered[0] = first.filter.process<true>(
                firstFrame.input, firstFrame.omegaStep, first.feedback,
                firstFrame.headroom, parameters.enableVcfEarlyEffect,
                parameters.calibration,
                firstFrame.hasTrajectory ? &firstFrame.trajectory : nullptr,
                parameters.vcfTanhMode, parameters.vcfSolverMode);
        if (secondFrame.needsFilter)
            filtered[1] = second.filter.process<true>(
                secondFrame.input, secondFrame.omegaStep, second.feedback,
                secondFrame.headroom, parameters.enableVcfEarlyEffect,
                parameters.calibration,
                secondFrame.hasTrajectory ? &secondFrame.trajectory : nullptr,
                parameters.vcfTanhMode, parameters.vcfSolverMode);
    }

    return {
        firstFrame.needsFilter
            ? finishVoiceFilter(first, filtered[0]) : 0.0f,
        secondFrame.needsFilter
            ? finishVoiceFilter(second, filtered[1]) : 0.0f
    };
}

std::array<float, 4> YouKnow106Engine::renderVoiceQuad(
    const std::array<Voice*, 4>& voices,
    const EngineParameters& parameters, float noiseSample) noexcept
{
    std::array<VoiceFilterFrame, 4> frames;
    for (std::size_t lane = 0; lane < voices.size(); ++lane)
        frames[lane] = prepareVoiceFilter(
            *voices[lane], parameters, noiseSample);

    std::array<float, 4> filtered {};
    bool processed = std::all_of(
        frames.begin(), frames.end(), [](const VoiceFilterFrame& frame) {
            return frame.needsFilter && !frame.hasTrajectory;
        });
    if (processed)
    {
        std::array<OtaCascade*, 4> cascades;
        std::array<float, 4> inputs;
        std::array<float, 4> omegaSteps;
        std::array<float, 4> feedbacks;
        std::array<float, 4> headrooms;
        for (std::size_t lane = 0; lane < voices.size(); ++lane)
        {
            cascades[lane] = &voices[lane]->filter;
            inputs[lane] = frames[lane].input;
            omegaSteps[lane] = frames[lane].omegaStep;
            feedbacks[lane] = voices[lane]->feedback;
            headrooms[lane] = frames[lane].headroom;
        }
        processed = OtaCascade::tryProcessSettledMersonQuad(
            cascades, inputs, omegaSteps, feedbacks, headrooms,
            parameters.enableVcfEarlyEffect, parameters.calibration,
            filtered);
    }

    if (!processed)
    {
        const auto processPair = [&](std::size_t firstLane) {
            const std::size_t secondLane = firstLane + 1u;
            bool pairProcessed = false;
            if (frames[firstLane].needsFilter
                && frames[secondLane].needsFilter
                && !frames[firstLane].hasTrajectory
                && !frames[secondLane].hasTrajectory)
                pairProcessed = OtaCascade::tryProcessSettledRk4Pair(
                    voices[firstLane]->filter, frames[firstLane].input,
                    frames[firstLane].omegaStep,
                    voices[firstLane]->feedback, frames[firstLane].headroom,
                    voices[secondLane]->filter, frames[secondLane].input,
                    frames[secondLane].omegaStep,
                    voices[secondLane]->feedback,
                    frames[secondLane].headroom,
                    parameters.enableVcfEarlyEffect, parameters.calibration,
                    filtered[firstLane], filtered[secondLane])
                    || OtaCascade::tryProcessSettledMersonPair(
                        voices[firstLane]->filter, frames[firstLane].input,
                        frames[firstLane].omegaStep,
                        voices[firstLane]->feedback,
                        frames[firstLane].headroom,
                        voices[secondLane]->filter,
                        frames[secondLane].input,
                        frames[secondLane].omegaStep,
                        voices[secondLane]->feedback,
                        frames[secondLane].headroom,
                        parameters.enableVcfEarlyEffect,
                        parameters.calibration,
                        filtered[firstLane], filtered[secondLane]);
            if (pairProcessed)
                return;
            for (const std::size_t lane : { firstLane, secondLane })
                if (frames[lane].needsFilter)
                    filtered[lane] = voices[lane]->filter.process<true>(
                        frames[lane].input, frames[lane].omegaStep,
                        voices[lane]->feedback, frames[lane].headroom,
                        parameters.enableVcfEarlyEffect,
                        parameters.calibration,
                        frames[lane].hasTrajectory
                            ? &frames[lane].trajectory : nullptr,
                        parameters.vcfTanhMode,
                        parameters.vcfSolverMode);
        };
        processPair(0u);
        processPair(2u);
    }

    std::array<float, 4> result {};
    for (std::size_t lane = 0; lane < voices.size(); ++lane)
        if (frames[lane].needsFilter)
            result[lane] = finishVoiceFilter(*voices[lane], filtered[lane]);
    return result;
}
#endif

// ---------------------------------------------------------------------------
// Decimation
// ---------------------------------------------------------------------------

void YouKnow106Engine::downsamplePair(HalfbandDecimator& decimator,
                                      float firstLeft, float firstRight,
                                      float secondLeft, float secondRight,
                                      float& outputLeft, float& outputRight) noexcept
{
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(decimatorCalls, 1);
#endif
    decimator.left[static_cast<std::size_t>(decimator.writeIndex)] = firstLeft;
    decimator.right[static_cast<std::size_t>(decimator.writeIndex)] = firstRight;
    decimator.writeIndex = (decimator.writeIndex + 1) & (halfbandRingSize - 1);
    decimator.left[static_cast<std::size_t>(decimator.writeIndex)] = secondLeft;
    decimator.right[static_cast<std::size_t>(decimator.writeIndex)] = secondRight;
    decimator.writeIndex = (decimator.writeIndex + 1) & (halfbandRingSize - 1);

    float sumLeft = 0.0f;
    float sumRight = 0.0f;
    // The real half-band kernel is exactly symmetric after float
    // normalisation. Pairing its mirrored samples halves the multiplications
    // while preserving the same 95-tap response and group delay.
    const int newest = (decimator.writeIndex - 1) & (halfbandRingSize - 1);
    const int pairs = halfbandActiveTapCount_ / 2;
    for (int pair = 0; pair < pairs; ++pair)
    {
#if defined(YOUKNOW106_WORK_AUDIT)
        YOUKNOW106_COUNT_DOMAIN_WORK(decimatorNonzeroTapVisits, 2);
        YOUKNOW106_COUNT_DOMAIN_WORK(decimatorStereoMacs, 2);
#endif
        const auto& first = halfbandActiveTaps_[static_cast<std::size_t>(pair)];
        const auto& second = halfbandActiveTaps_[static_cast<std::size_t>(
            halfbandActiveTapCount_ - 1 - pair)];
        const int firstIndex = (newest - first.tap) & (halfbandRingSize - 1);
        const int secondIndex = (newest - second.tap) & (halfbandRingSize - 1);
        sumLeft += first.coefficient
            * (decimator.left[static_cast<std::size_t>(firstIndex)]
               + decimator.left[static_cast<std::size_t>(secondIndex)]);
        sumRight += first.coefficient
            * (decimator.right[static_cast<std::size_t>(firstIndex)]
               + decimator.right[static_cast<std::size_t>(secondIndex)]);
    }

    const auto& centre = halfbandActiveTaps_[static_cast<std::size_t>(pairs)];
    const int centreIndex = (newest - centre.tap) & (halfbandRingSize - 1);
    sumLeft += centre.coefficient
        * decimator.left[static_cast<std::size_t>(centreIndex)];
    sumRight += centre.coefficient
        * decimator.right[static_cast<std::size_t>(centreIndex)];
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(decimatorNonzeroTapVisits, 1);
    YOUKNOW106_COUNT_DOMAIN_WORK(decimatorStereoMacs, 2);
#endif

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
        && oversamplingRequested_ != oversamplingApplied_
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

    if (!panelGlidePrimed_)
    {
        glidedVolume_ = parameters.volume;
        panelGlidePrimed_ = true;
    }

    // Each converter destination owns a separately named hold network. VCF,
    // voice VCA, common VCA, PWM and SUB have evidence-backed post-hold
    // networks; resonance keeps Step 11's explicitly voiced 522 us companion
    // trajectory, while DCO and noise retain their isolated compatibility
    // policies until their RCs are established. Exact full-interval decays
    // are precomputed when the processing rate changes; only the rare interval
    // that actually contains a fractional write needs an event-position
    // exponential.
    // Hold, scan and output coefficients are refreshed by updateProcessingRate;
    // this reference keeps the sample equations below unchanged while avoiding
    // per-host-block exponentials and divisions.
    const auto& coefficients = processingCoefficients_;
    // Ordinary intervals use finite engine-owned state, sanitized targets and
    // precomputed finite decays. Keep exactOnePoleHoldEndpoint's full guards
    // for the rare physical event and direct hostile-input test paths.
    const auto advanceOrdinaryOnePoleHold = [](double state, float target,
                                                double decay) noexcept {
        const double resolvedTarget = static_cast<double>(target);
        return resolvedTarget + (state - resolvedTarget) * decay;
    };
    bool patchLevelCacheValid = false;
    std::uint32_t patchLevelCacheKey = 0u;
    float patchLevelCacheValue = 0.0f;
    bool outputCouplingCacheValid = false;
    std::uint32_t outputCouplingCacheKey = 0u;
    float outputCouplingCacheGain = 0.0f;
    float outputCouplingCacheNoiseScale = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
#if defined(YOUKNOW106_WORK_AUDIT)
        YOUKNOW106_COUNT_DOMAIN_WORK(hostFrames, 1);
#endif
        float outputLeft = 0.0f;
        float outputRight = 0.0f;
        bool sounding = false;

        // Two decimation stages at 4x, one at 2x, none at 1x. The inner loop
        // renders one oversampled frame.
        std::array<float, maximumOversampleFactor> stageLeft {};
        std::array<float, maximumOversampleFactor> stageRight {};

        for (int step = 0; step < oversampling_; ++step)
        {
#if defined(YOUKNOW106_WORK_AUDIT)
            YOUKNOW106_COUNT_DOMAIN_WORK(internalFrames, 1);
            YOUKNOW106_COUNT_DOMAIN_WORK(scanPolls, 1);
#endif
            struct PhysicalPassiveHoldEvent
            {
                bool active { false };
                ConverterWrite write { ConverterDestination::Resonance, -1 };
                double position { 0.0 };
                float previousTarget { 0.0f };
                float target { 0.0f };
            } physicalHoldEvent;
            const auto currentPassiveHoldTarget = [this](
                const ConverterWrite& write) noexcept
            {
                switch (write.destination)
                {
                    case ConverterDestination::Resonance:
                        return resonanceCvTarget_;
                    case ConverterDestination::CommonVca:
                        return sharedVcaTarget_;
                    case ConverterDestination::Sub:
                        return subCvTarget_;
                    case ConverterDestination::Pwm:
                        return pwmVoltsTarget_;
                    case ConverterDestination::Vcf:
                        if (write.voice >= 0 && write.voice < hardwareVoices)
                            return voices_[static_cast<std::size_t>(write.voice)]
                                .cutoffCountsTarget;
                        break;
                    case ConverterDestination::VoiceVca:
                        if (write.voice >= 0 && write.voice < hardwareVoices)
                            return voices_[static_cast<std::size_t>(write.voice)]
                                .vcaControlTarget;
                        break;
                    case ConverterDestination::Pitch:
                    case ConverterDestination::Noise:
                        break;
                }
                return 0.0f;
            };
            const float resonanceIntervalStart = resonanceCv_;
            // One converter serves the whole instrument. The service chart
            // and the hash-matched B-2 code establish the complete ordinal
            // write order and show sequential activity across the pass. The
            // normalized timing profile preserves that qualitative fact while
            // leaving exact physical offsets open.
            bool converterPassCompleted = false;
            if (controlScanPhase_ >= 1.0)
            {
#if defined(YOUKNOW106_WORK_AUDIT)
                YOUKNOW106_COUNT_DOMAIN_WORK(converterPassStarts, 1);
#endif
                controlScanPhase_ -= 1.0;
                nextConverterWrite_ = 0;
                if (assignmentRescanPending_)
                    assignmentRescanPassArmed_ = true;
                const float bendMagnitude = std::floor(
                    std::abs(pitchBendTarget_) * 255.0f + 0.5f) / 255.0f;
                pitchBend_ = pitchBendTarget_ < 0.0f ? -bendMagnitude
                                                     : bendMagnitude;
                dcoPitchBendWord_ = dcoBendWordForCommand(
                    dcoBendCommand(pitchBendTarget_),
                    controlAdcByte(parameters.benderDcoDepth));
                advanceLfo(parameters);
                dcoLfoPitchWord_ = dcoLfoPitchWordOffset(
                    lfoAccumulator_, lfoPolarity_ >= 0.0f, lfoDelayByte_,
                    storedControlByte(parameters.dcoLfoDepth),
                    storedControlByte(modWheelTarget_),
                    controlAdcByte(parameters.benderLfoDepth));
                converterPassLfoGated_ = lfoValue_ * lfoDelayLevel_;
                converterPassPwmDacCode_ = parameters.pulseEnabled
                    ? pwmDacCode(parameters.pwmDepth, parameters.pwmSource,
                                 lfoAccumulator_, lfoPolarity_ >= 0.0f)
                    : 0u;

                // Slots above the six physical cards are an explicit product
                // extension. They reuse one complete logical update at the
                // pass boundary without pretending that the B-2 scans them.
                for (int slot = hardwareVoices; slot < maxVoices; ++slot)
                {
                    auto& voice = voices_[static_cast<std::size_t>(slot)];
#if defined(YOUKNOW106_WORK_AUDIT)
                    YOUKNOW106_COUNT_DOMAIN_WORK(extensionScanUpdates, 1);
#endif
                    const std::uint32_t count = updateVoiceScan(
                        voice, parameters, converterPassLfoGated_);
                    programDcoCount(voice, count, voice.dcoResetPending);
                    voice.dcoResetPending = false;
                }
            }

            const auto& writes = converterWriteOrder();
            while (nextConverterWrite_ < writes.size()
                   && converterEventPhases_[nextConverterWrite_]
                          <= controlScanPhase_ + 1.0e-12)
            {
                const auto& write = writes[nextConverterWrite_];
                const bool relevant = isPassiveHoldWrite(write);
                const bool consumesLatch = passiveHoldEventLatch_.valid
                    && passiveHoldEventLatch_.ordinal == nextConverterWrite_
                    && passiveHoldEventLatch_.write.destination
                           == write.destination
                    && passiveHoldEventLatch_.write.voice == write.voice;
                float previousTarget = 0.0f;
                if (relevant && !consumesLatch)
                    previousTarget = currentPassiveHoldTarget(write);
                const float latchedTarget = consumesLatch
                    ? passiveHoldEventLatch_.target : 0.0f;
                performConverterWrite(
                    write, parameters, converterPassLfoGated_,
                    consumesLatch ? &latchedTarget : nullptr);
                if (relevant && !consumesLatch && !physicalHoldEvent.active)
                {
                    physicalHoldEvent.active = true;
                    physicalHoldEvent.write = write;
                    physicalHoldEvent.position = 0.0;
                    physicalHoldEvent.previousTarget = previousTarget;
                    physicalHoldEvent.target =
                        currentPassiveHoldTarget(write);
                }
                if (consumesLatch)
                {
#if defined(YOUKNOW106_WORK_AUDIT)
                    YOUKNOW106_COUNT_DOMAIN_WORK(
                        passiveHoldFractionalTargetCommits, 1);
                    if (write.destination == ConverterDestination::Resonance
                        || write.destination == ConverterDestination::Vcf)
                    YOUKNOW106_COUNT_DOMAIN_WORK(
                        vcfFractionalTargetCommits, 1);
#endif
                    passiveHoldEventLatch_ = {};
                }
                ++nextConverterWrite_;
                if (nextConverterWrite_ == writes.size())
                    converterPassCompleted = true;
            }

            scheduleUpcomingDcoPitchPrestages(
                controlScanPhase_, coefficients.scanPhasePerInternalSample);

            if (!physicalHoldEvent.active
                && latchUpcomingPassiveHoldEvent(
                    controlScanPhase_, coefficients.scanPhasePerInternalSample,
                    parameters))
            {
                physicalHoldEvent.active = true;
                physicalHoldEvent.write = passiveHoldEventLatch_.write;
                physicalHoldEvent.position =
                    passiveHoldEventLatch_.eventPosition;
                physicalHoldEvent.target = passiveHoldEventLatch_.target;
                physicalHoldEvent.previousTarget =
                    currentPassiveHoldTarget(physicalHoldEvent.write);
            }

            const bool resonanceEvent = physicalHoldEvent.active
                && physicalHoldEvent.write.destination
                       == ConverterDestination::Resonance;
            const bool cutoffHoldEvent = physicalHoldEvent.active
                && physicalHoldEvent.write.destination
                       == ConverterDestination::Vcf;
            if (resonanceEvent)
            {
                resonanceVcfHoldInterval_ = exactVcfHoldInterval(
                    resonanceIntervalStart,
                    physicalHoldEvent.previousTarget, true,
                    physicalHoldEvent.position, physicalHoldEvent.target,
                    coefficients.internalIntervalSeconds);
                resonanceCv_ = resonanceVcfHoldInterval_.endpoint;
            }
            else
            {
                if (cutoffHoldEvent)
                {
                    resonanceVcfHoldInterval_ = exactVcfHoldInterval(
                        resonanceIntervalStart, resonanceCvTarget_, false,
                        1.0, resonanceCvTarget_,
                        coefficients.internalIntervalSeconds);
                    resonanceCv_ = resonanceVcfHoldInterval_.endpoint;
                }
                else
                    resonanceCv_ +=
                        (resonanceCvTarget_ - resonanceCv_)
                            * coefficients.resonanceSlew;
            }
            const bool commonVcaEvent = physicalHoldEvent.active
                && physicalHoldEvent.write.destination
                       == ConverterDestination::CommonVca;
            sharedVca_ = commonVcaEvent
                ? exactOnePoleHoldEndpoint(
                    sharedVca_, physicalHoldEvent.previousTarget, true,
                    physicalHoldEvent.position, physicalHoldEvent.target,
                    coefficients.internalIntervalSeconds,
                    coefficients.commonVcaTime,
                    coefficients.commonVcaDecay)
                : advanceOrdinaryOnePoleHold(
                    sharedVca_, sharedVcaTarget_,
                    coefficients.commonVcaDecay);

            const bool pwmEvent = physicalHoldEvent.active
                && physicalHoldEvent.write.destination
                       == ConverterDestination::Pwm;
            PwmHoldState pwmState { pwmVoltsFirstPole_, pwmVolts_ };
            pwmState = exactPwmHoldEndpoint(
                pwmState,
                pwmEvent ? physicalHoldEvent.previousTarget : pwmVoltsTarget_,
                pwmEvent, physicalHoldEvent.position,
                pwmEvent ? physicalHoldEvent.target : pwmVoltsTarget_,
                coefficients.internalIntervalSeconds,
                coefficients.pwmFullInterval);
            pwmVoltsFirstPole_ = pwmState.first;
            pwmVolts_ = pwmState.second;

            const bool subEvent = physicalHoldEvent.active
                && physicalHoldEvent.write.destination
                       == ConverterDestination::Sub;
            subCv_ = subEvent
                ? exactOnePoleHoldEndpoint(
                    subCv_, physicalHoldEvent.previousTarget, true,
                    physicalHoldEvent.position, physicalHoldEvent.target,
                    coefficients.internalIntervalSeconds,
                    static_cast<double>(subHoldSlewSeconds),
                    coefficients.subDecay)
                : advanceOrdinaryOnePoleHold(
                    subCv_, subCvTarget_, coefficients.subDecay);
            noiseCv_ += (noiseCvTarget_ - noiseCv_) * coefficients.noiseSlew;
            advanceThermalWarmup();

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
            // keys are held instead of staying put. Its support circuit
            // band-shapes the rail: C42 into the level OTA's 4.7 kOhm input
            // bias high-passes at 33.9 Hz, the BA662 applies the scanned level,
            // and C41 against R79 low-passes that controlled output at
            // 4.82 kHz. One state still serves every voice; the passband stays
            // at unity, so in-band density keeps its established rate
            // normalisation.
            if (noiseState_ == 0u)
                noiseState_ = 0x6d2b79f5u;
            noiseState_ = xorshift32(noiseState_);
            const float rawNoise =
                bipolarFromState(noiseState_) * noiseRateScale_;
            const float noiseSample = processMainNoiseSource(
                rawNoise, noiseCv_, parameters.enableNoiseLevelBeforeC41);

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
            const auto updateVoiceForInterval = [&](int slot) {
                auto& voice = voices_[static_cast<std::size_t>(slot)];
#if defined(YOUKNOW106_WORK_AUDIT)
                YOUKNOW106_COUNT_DOMAIN_WORK(holdVoiceUpdates, 1);
#endif
                // Each hold capacitor's own slew turns the scan's staircase
                // back into a continuous control voltage before it reaches
                // its converter. The amplifier's hold is the slow one.
                const bool cutoffEvent = cutoffHoldEvent
                    && physicalHoldEvent.write.destination
                           == ConverterDestination::Vcf
                    && physicalHoldEvent.write.voice == slot;
                const float cutoffIntervalStart = voice.cutoffCounts;
                if (cutoffEvent)
                {
                    cutoffVcfHoldIntervals_[static_cast<std::size_t>(slot)] =
                        exactVcfHoldInterval(
                            cutoffIntervalStart,
                            physicalHoldEvent.previousTarget, true,
                            physicalHoldEvent.position,
                            physicalHoldEvent.target,
                            coefficients.internalIntervalSeconds);
                    voice.cutoffCounts =
                        cutoffVcfHoldIntervals_[static_cast<std::size_t>(slot)]
                            .endpoint;
                }
                else if (resonanceEvent)
                {
                    cutoffVcfHoldIntervals_[static_cast<std::size_t>(slot)] =
                        exactVcfHoldInterval(
                            cutoffIntervalStart, voice.cutoffCountsTarget,
                            false, 1.0, voice.cutoffCountsTarget,
                            coefficients.internalIntervalSeconds);
                    voice.cutoffCounts =
                        cutoffVcfHoldIntervals_[static_cast<std::size_t>(slot)]
                            .endpoint;
                }
                else
                {
                    voice.cutoffCounts +=
                        (voice.cutoffCountsTarget - voice.cutoffCounts)
                            * coefficients.vcfSlew;
                }
                // Every slot reaches this store before renderVoice can read its
                // entry, so clearing the whole array at the interval boundary
                // would only write the same values twice.
                exactVcfControlInterval_[static_cast<std::size_t>(slot)] =
                    resonanceEvent || cutoffEvent;
                voice.dcoCv +=
                    (voice.dcoCvTarget - voice.dcoCv) * coefficients.dcoSlew;
                const bool voiceVcaEvent = physicalHoldEvent.active
                    && physicalHoldEvent.write.destination
                           == ConverterDestination::VoiceVca
                    && physicalHoldEvent.write.voice == slot;
                voice.vcaControl = voiceVcaEvent
                    ? exactOnePoleHoldEndpoint(
                        voice.vcaControl,
                        physicalHoldEvent.previousTarget, true,
                        physicalHoldEvent.position,
                        physicalHoldEvent.target,
                        coefficients.internalIntervalSeconds,
                        static_cast<double>(voiceVcaHoldSlewSeconds),
                        coefficients.voiceVcaDecay)
                    : advanceOrdinaryOnePoleHold(
                        voice.vcaControl, voice.vcaControlTarget,
                        coefficients.voiceVcaDecay);
                // Neither value is needed for an inactive extension slot
                // (`!voice.active && cardIndex >= hardwareVoices`, and
                // `cardIndex` is always the slot). A freewheeling physical card
                // still needs the comparator duty when the Pulse-Off WAVE-node
                // model is live: its slow C56/C50 state follows the endpoint at
                // low carrier rates and that duty mean at high rates. The
                // expensive filter/audio coefficients remain skipped.
                const bool freewheels = !voice.active
                    && parameters.vcfTanhMode != VcfTanhMode::Exact;
                const bool hasAudioCell = voice.active || slot < hardwareVoices;
                if (hasAudioCell
                    && (!freewheels
                        || parameters.enablePulseOffWaveNodeCoupling))
                    updatePulseComparator(voice, parameters);
                if (hasAudioCell && !freewheels)
                {
                    updateVoiceAudio(voice, parameters);
                }
            };
            const auto accountRenderedVoice = [&](Voice& voice, float output) {
                mono += output;
                loudestEnvelope = std::max(
                    loudestEnvelope, voice.envelope.value);

                // Retire on the modelled amplifier actually being shut. A
                // card's optional control offset can hold the control voltage
                // just above the turn-on for ever, where the grounded-base
                // stage still passes an inaudible trickle; without an explicit
                // silence threshold a voice at -100 dB would block a deferred
                // quality change indefinitely.
                if (voice.envelope.stage == EnvelopeStage::Idle
                    && !voice.keyDown && !voice.sustained
                    && voice.vcaGain <= VoiceVcaControlLaw::silenceGain)
                    silenceVoice(voice);
                else
                    sounding = true;
            };
            const auto renderVoices = [&]<bool useCubicEarly> {
                for (int slot = 0; slot < maxVoices;)
                {
                    auto& voice = voices_[static_cast<std::size_t>(slot)];
                    updateVoiceForInterval(slot);

#if defined(YOUKNOW106_HAS_VCF_PAIR_SIMD) \
    && !defined(YOUKNOW106_WORK_AUDIT)
                    if constexpr (useCubicEarly)
                    {
                        if (slot + 3 < maxVoices && voice.active
                            && voices_[static_cast<std::size_t>(slot + 1)].active
                            && voices_[static_cast<std::size_t>(slot + 2)].active
                            && voices_[static_cast<std::size_t>(slot + 3)].active
                            && parameters.vcfTanhMode
                                   == VcfTanhMode::PolyZoned
                            && parameters.vcfSolverMode
                                   == VcfSolverMode::Rk4Single)
                        {
                            for (int offset = 1; offset < 4; ++offset)
                                updateVoiceForInterval(slot + offset);
                            const std::array<Voice*, 4> group {
                                &voice,
                                &voices_[static_cast<std::size_t>(slot + 1)],
                                &voices_[static_cast<std::size_t>(slot + 2)],
                                &voices_[static_cast<std::size_t>(slot + 3)]
                            };
                            const auto outputs = renderVoiceQuad(
                                group, parameters, noiseSample);
                            for (std::size_t lane = 0; lane < group.size();
                                 ++lane)
                                accountRenderedVoice(
                                    *group[lane], outputs[lane]);
                            slot += 4;
                            continue;
                        }
                        if (slot + 1 < maxVoices && voice.active
                            && voices_[static_cast<std::size_t>(slot + 1)].active
                            && parameters.vcfTanhMode
                                   == VcfTanhMode::PolyZoned
                            && parameters.vcfSolverMode
                                   == VcfSolverMode::Rk4Single)
                        {
                            auto& second = voices_[
                                static_cast<std::size_t>(slot + 1)];
                            updateVoiceForInterval(slot + 1);
                            const auto outputs = renderVoicePair(
                                voice, second, parameters, noiseSample);
                            accountRenderedVoice(voice, outputs[0]);
                            accountRenderedVoice(second, outputs[1]);
                            slot += 2;
                            continue;
                        }
                    }
#endif
                    if (!voice.active)
                    {
                        // The six physical DCO/filter cards remain powered behind
                        // their closed VCAs. Extension slots have no card state to
                        // advance, so avoid entering renderVoice only to take its
                        // identical early return.
                        if (voice.cardIndex < hardwareVoices)
                            renderVoice<useCubicEarly>(
                                voice, parameters, noiseSample);
                        ++slot;
                        continue;
                    }

                    accountRenderedVoice(
                        voice, renderVoice<useCubicEarly>(
                            voice, parameters, noiseSample));
                    ++slot;
                }
            };
            // The cubic Early multiplier is the shipped default since the
            // 2026-08-24 CPU pass, so neither arm is the unlikely one.
            if (useCubicEarly_)
                renderVoices.template operator()<true>();
            else
                renderVoices.template operator()<false>();

            // IC35/TP5 is shared by all six channels of the two M82C53s. Each
            // card-local event walk above read the same left-boundary phase;
            // advance that one physical divider once after every card consumes
            // this interval.
            advanceRangeClock(parameters.range);
            displayEnvelope_ = loudestEnvelope;

            // The POLY/unison handler gated and cleared at the host event.
            // Reassign only after the complete ordered pass, so every physical
            // voice CPU has observed gate-off before any replacement Note On.
            if (converterPassCompleted && assignmentRescanPending_
                && assignmentRescanPassArmed_)
                completeVoiceAssignmentRescan();
            controlScanPhase_ += coefficients.scanPhasePerInternalSample;

            // One high-pass, on the summed voices. The schematic carries a
            // single set of parts for it -- on the jack board, downstream of
            // the summing amplifier -- not one set per voice, so this is where
            // it belongs. An earlier revision ran it inside each voice ahead of
            // that voice's own filter, which is a different circuit: a
            // high-pass feeding a resonant lowpass is not the same as one
            // following it, because what the high-pass removes is what the
            // resonance would otherwise have had to work on.
            const float busIn = voiceBusInput(mono);
            float effectiveCouplingG = voiceBusCouplingG_;
            if (parameters.enableElectrolyticC14Nonlinearity && parameters.calibration > 0.0f)
            {
                const float inputMagnitude = std::abs(busIn);
                const float capMod = 1.0f + 0.15f * (inputMagnitude / (1.0f + inputMagnitude))
                                   * parameters.calibration;
                effectiveCouplingG *= capMod;
            }
            const float coupled = voiceBusCoupling_.process(
                busIn, effectiveCouplingG, 0.0f, 1.0f);
            const float shaped = highPass_.process(coupled,
                                                   highPassG_,
                                                   highPassShelf_, highPassHigh_);

            // VCA LEVEL is the one common uPC1252H2 on the jack board, after
            // the voice sum and HPF. The six voice-module VCAs above are driven
            // only by ENV/GATE (plus the optional velocity extension).
            const float vcaInput = commonVcaInputCoupling_.process(
                shaped, commonVcaInputCouplingG_, 0.0f, 1.0f);
            const float patchLevelInput = static_cast<float>(sharedVca_);
            const auto patchLevelKey =
                std::bit_cast<std::uint32_t>(patchLevelInput);
            if (!patchLevelCacheValid || patchLevelCacheKey != patchLevelKey)
            {
                patchLevelCacheValue = patchLevelGain(patchLevelInput);
                patchLevelCacheKey = patchLevelKey;
                patchLevelCacheValid = true;
            }
            const float levelled = vcaInput * patchLevelCacheValue;

            // The chorus input coupling capacitors sit in its two wet branches;
            // dry bypasses them. IC6 applies its component-derived dry/wet
            // gains when they recombine. Its +/-15 V clipping point has not
            // been measured under the output load, so no invented low-voltage
            // rail is inserted here; the main volume control follows it.
            float wetLeft = levelled;
            float wetRight = levelled;
            // A switched-off, fully settled chorus outputs bit-exactly the
            // dry routing whatever its muted lines hold, so the fast tanh
            // modes skip the BBD work behind it; the wet path rebuilds from
            // silence on engage. Exact keeps the established always-running
            // lines -- the same policy split as the voice-card freewheel.
            if (parameters.chorus != ChorusMode::Off
                || parameters.vcfTanhMode == VcfTanhMode::Exact
                || !chorus_.processBypassedWhenSettled(levelled, wetLeft,
                                                       wetRight))
                chorus_.process(levelled, parameters.chorus,
                                parameters.chorusNoise,
                                wetLeft, wetRight,
                                parameters.enableChorusClockBleed,
                                parameters.enableChorusHyperbolicSweep,
                                parameters.calibration,
                                parameters.useChorusRateNoiseHypothesis,
                                parameters.enableNarrowOneTwoChorus);

            // TA75558S IC6 has finite loaded output swing inside its +/-15 V
            // supplies. The modelled 13.5 V asymptote and knee are provisional
            // OQ-05 policy, not per-card tolerances, so Unit Character does not
            // scale them.
            const auto wetLeftKey = std::bit_cast<std::uint32_t>(wetLeft);
            const auto wetRightKey = std::bit_cast<std::uint32_t>(wetRight);
            wetLeft = outputSummerClip(wetLeft);
            // outputSummerClip's asymptote and exponent are fixed. Reuse only
            // for an identical float representation, preserving signed zero
            // and NaN payload distinctions as well as unequal stereo samples.
            wetRight = wetLeftKey == wetRightKey
                     ? wetLeft : outputSummerClip(wetRight);

            // TA75558S IC6 output slew limit. The datasheet's 1.0 V/us is a
            // typical value at unity gain and 2 kOhm, not a guaranteed limit
            // at the installed load. As a part policy -- like the shared swing
            // above -- it is not scaled by Unit Character: a previous revision
            // divided it by calibration, granting the pristine reference a
            // 10x faster op-amp and a full-character unit one slower than the
            // part's own datasheet.
            if (parameters.enableOpAmpSlewLimiting)
            {
                const float deltaL = wetLeft - outputSlewStateLeft_;
                outputSlewStateLeft_ += std::clamp(
                    deltaL, -coefficients.outputSlewMaxStep,
                    coefficients.outputSlewMaxStep);
                wetLeft = outputSlewStateLeft_;

                const float deltaR = wetRight - outputSlewStateRight_;
                outputSlewStateRight_ += std::clamp(
                    deltaR, -coefficients.outputSlewMaxStep,
                    coefficients.outputSlewMaxStep);
                wetRight = outputSlewStateRight_;
            }
            else
            {
                outputSlewStateLeft_ = wetLeft;
                outputSlewStateRight_ = wetRight;
            }

            // TA75558S open-loop gain is finite. With both signal legs
            // connected, the feedback/input resistor network sets the noise
            // gain and turns the part's 3 MHz typical GBW into the derived
            // closed-loop pole above. At ordinary rates the exponential is
            // effectively one (as the real pole is far above Nyquist); at
            // high-rate/offline operation this state supplies the missing
            // ultrasonic roll-off without adding another quality domain.
            outputBandwidthStateLeft_ +=
                coefficients.outputSummerBandwidthBlend
                * (wetLeft - outputBandwidthStateLeft_);
            outputBandwidthStateRight_ +=
                coefficients.outputSummerBandwidthBlend
                * (wetRight - outputBandwidthStateRight_);
            wetLeft = outputBandwidthStateLeft_;
            wetRight = outputBandwidthStateRight_;

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

        glidedVolume_ +=
            (parameters.volume - glidedVolume_) * coefficients.outputGlide;

        // C17/C20, R54/R57 and VR1 are one loaded network, not a fixed pole
        // followed by an unrelated gain. The 41.3 kOhm selector ladder and
        // 101 kOhm headphone input load each wiper at every shaft position;
        // moving Volume changes both the settled gain and the resistance seen
        // by the still-continuous capacitor state. outputCouplingCornerHz(float)
        // and outputCouplingHighGain(float) each solve that identical wiper
        // network independently -- fine for the two callers that only want one
        // of the two values, but this call site always wants both, so it is
        // solved once here and both results are read off the one network.
        const auto outputCouplingKey =
            std::bit_cast<std::uint32_t>(glidedVolume_);
        float outputCouplingGain;
        float outputWiperNoiseScale;
        if (!outputCouplingCacheValid
            || outputCouplingCacheKey != outputCouplingKey)
        {
            const auto outputCouplingNetwork =
                outputCouplingWiperNetworkFor(glidedVolume_);
            const float outputCouplingCorner = 1.0f
                / (twoPi * outputCouplingCapacitanceF
                   * outputCouplingNetwork.resistance);
            outputCouplingG_ = std::tan(
                pi * outputCouplingCorner * inverseSampleRate_);
            outputCouplingGain = outputCouplingNetwork.loadedLower > 0.0f
                ? outputCouplingNetwork.loadedLower
                    / outputCouplingNetwork.resistance
                : 0.0f;
            outputCouplingCacheKey = outputCouplingKey;
            outputCouplingCacheGain = outputCouplingGain;
            const float wiperNoiseDensity = std::sqrt(
                4.0f * boltzmannConstant * outputNoiseTemperatureKelvin
                * outputWiperNoiseResistance(glidedVolume_));
            outputWiperNoiseScale = wiperNoiseDensity
                * std::sqrt(1.5f * static_cast<float>(sampleRate_))
                * voltsToSample;
            outputCouplingCacheNoiseScale = outputWiperNoiseScale;
            outputCouplingCacheValid = true;
        }
        else
        {
            outputCouplingGain = outputCouplingCacheGain;
            outputWiperNoiseScale = outputCouplingCacheNoiseScale;
        }
        outputNoiseStateLeft_ = xorshift32(outputNoiseStateLeft_);
        outputNoiseStateRight_ = xorshift32(outputNoiseStateRight_);
        // Unit Character zero is the project's deterministic calibrated-
        // nominal reference, including an exact digital silence floor; one is
        // the declared physical reference. Keep that established endpoint
        // contract while introducing the derived real-resistor floor.
        const float outputNoiseLeft = bipolarFromState(outputNoiseStateLeft_)
                                    * parameters.calibration;
        const float outputNoiseRight = bipolarFromState(outputNoiseStateRight_)
                                     * parameters.calibration;
        outputLeft += outputNoiseLeft * coefficients.outputSummerNoiseScale;
        outputRight += outputNoiseRight * coefficients.outputSummerNoiseScale;
        outputLeft = outputCouplingLeft_.process(
            outputLeft, outputCouplingG_, 0.0f, outputCouplingGain);
        outputRight = outputCouplingRight_.process(
            outputRight, outputCouplingG_, 0.0f, outputCouplingGain);
        outputWiperNoiseStateLeft_ = xorshift32(outputWiperNoiseStateLeft_);
        outputWiperNoiseStateRight_ = xorshift32(outputWiperNoiseStateRight_);
        outputLeft += bipolarFromState(outputWiperNoiseStateLeft_)
                    * parameters.calibration * outputWiperNoiseScale;
        outputRight += bipolarFromState(outputWiperNoiseStateRight_)
                     * parameters.calibration * outputWiperNoiseScale;

        // How long the voices have been gone, which is what a pending quality
        // change waits on: the output path needs that long to run dry.
        if (sounding)
            oversamplingIdleSamples_ = 0;
        else if (oversamplingIdleSamples_ < oversamplingQuietSamples_)
            ++oversamplingIdleSamples_;

        const float transitionGain = rateTransitionGain_;
        left[sample] = std::isfinite(outputLeft)
                     ? outputLeft * coefficients.outputBoundaryGain
                           * transitionGain
                     : 0.0f;
        right[sample] = std::isfinite(outputRight)
                      ? outputRight * coefficients.outputBoundaryGain
                            * transitionGain
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

#if defined(YOUKNOW106_WORK_AUDIT)
#undef YOUKNOW106_COUNT_DOMAIN_WORK
#endif
