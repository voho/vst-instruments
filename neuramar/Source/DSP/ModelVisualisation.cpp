#include "ModelVisualisation.h"

#include <algorithm>
#include <cmath>

namespace neuramar
{
namespace
{
constexpr float logLowest = 4.321928094887362f;   // log2(20)
constexpr float logHighest = 14.287712379549449f; // log2(20000)
constexpr float logSpan = logHighest - logLowest;

// The bin grid's octave position depends only on its index, which never
// changes, not on the slice, band, or model being drawn. depositBand() used
// to recompute it from scratch on every one of its binCount iterations, and
// buildModelAnatomy() calls depositBand() sliceCount * airBandCount times per
// published memory (24 * 16 = 384), so the same 96 values were being
// rederived 384 times over. The formula is a compile-time-constant linear
// interpolation (no transcendental calls involved), so it can be resolved
// once as a constexpr table instead of once per depositBand() call - the
// same "hoist what does not depend on the loop" pattern already applied to
// stretchedHarmonicRatios below.
[[nodiscard]] constexpr std::array<float, ModelAnatomy::binCount>
makeBinOctaves() noexcept
{
    std::array<float, ModelAnatomy::binCount> octaves {};
    for (std::size_t bin = 0; bin < ModelAnatomy::binCount; ++bin)
        octaves[bin] = logLowest + logSpan * static_cast<float>(bin)
            / static_cast<float>(ModelAnatomy::binCount - 1);
    return octaves;
}

constexpr std::array<float, ModelAnatomy::binCount> binOctaves
    = makeBinOctaves();

[[nodiscard]] float finiteOr(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

[[nodiscard]] std::ptrdiff_t binIndexOf(float frequencyHz) noexcept
{
    if (!(frequencyHz > 0.0f) || !std::isfinite(frequencyHz))
        return -1;
    const float position = (std::log2(frequencyHz) - logLowest) / logSpan;
    if (!(position >= 0.0f) || position > 1.0f)
        return -1;
    const auto index = static_cast<std::ptrdiff_t>(
        position * static_cast<float>(ModelAnatomy::binCount - 1) + 0.5f);
    return std::clamp<std::ptrdiff_t>(
        index, 0, static_cast<std::ptrdiff_t>(ModelAnatomy::binCount) - 1);
}

void depositPeak(std::array<float, ModelAnatomy::binCount>& bins,
                 float frequencyHz, float amplitude, int spread) noexcept
{
    const auto centre = binIndexOf(frequencyHz);
    if (centre < 0 || !(amplitude > 0.0f) || !std::isfinite(amplitude))
        return;

    for (int offset = -spread; offset <= spread; ++offset)
    {
        const auto index = centre + offset;
        if (index < 0
            || index >= static_cast<std::ptrdiff_t>(ModelAnatomy::binCount))
            continue;
        // A triangular skirt keeps a single partial visible at display
        // resolution without pretending it has real bandwidth.
        const int distance = offset < 0 ? -offset : offset;
        const float taper = spread > 0
            ? 1.0f - 0.65f * static_cast<float>(distance)
                / static_cast<float>(spread)
            : 1.0f;
        auto& cell = bins[static_cast<std::size_t>(index)];
        cell = std::max(cell, amplitude * taper);
    }
}

// A band's centre octave and clamped width depend only on the model's fitted
// Air geometry, which is fixed for the whole trajectory, not on the slice
// being drawn or its amplitude. buildModelAnatomy() calls this once per band
// per slice (sliceCount * airBandCount = 24 * 16 = 384 times per published
// memory), and the same centreHz/bandwidthOctaves pair recurs across all 24
// slices of a given band, so resolving std::log2(centreHz) and the width
// clamp here on every call rederived the same 16 values 24 times over. The
// caller now hoists both into an AirBandGeometry, resolved once per band, and
// passes the already-validated octave and width through - the same "hoist
// what does not depend on the loop" pattern already applied to binOctaves and
// stretchedHarmonicRatios above.
void depositBandAtOctave(std::array<float, ModelAnatomy::binCount>& bins,
                         float centreOctave, float width,
                         float amplitude) noexcept
{
    if (!(amplitude > 0.0f) || !std::isfinite(amplitude))
        return;

    for (std::size_t bin = 0; bin < ModelAnatomy::binCount; ++bin)
    {
        const float distance = (binOctaves[bin] - centreOctave)
            / (0.6f * width);
        if (std::abs(distance) > 3.0f)
            continue;
        const float shape = std::exp(-0.5f * distance * distance);
        bins[bin] = std::max(bins[bin], amplitude * shape);
    }
}

struct AirBandGeometry
{
    bool valid { false };
    float centreOctave { 0.0f };
    float width { 1.0f };
};

[[nodiscard]] AirBandGeometry makeAirBandGeometry(
    float centreHz, float bandwidthOctaves) noexcept
{
    if (!(centreHz > 0.0f) || !std::isfinite(centreHz))
        return {};
    return { true, std::log2(centreHz),
             std::clamp(finiteOr(bandwidthOctaves, 1.0f), 0.10f, 4.0f) };
}
} // namespace

float anatomyFrequencyAt(float normalisedPosition) noexcept
{
    const float position = std::clamp(
        finiteOr(normalisedPosition, 0.0f), 0.0f, 1.0f);
    return std::exp2(logLowest + logSpan * position);
}

float anatomyPositionOf(float frequencyHz) noexcept
{
    if (!(frequencyHz > 0.0f) || !std::isfinite(frequencyHz))
        return 0.0f;
    return std::clamp((std::log2(frequencyHz) - logLowest) / logSpan,
                      0.0f, 1.0f);
}

std::size_t anatomySliceAt(float normalisedTime) noexcept
{
    const float time = std::clamp(finiteOr(normalisedTime, 0.0f), 0.0f, 1.0f);
    const auto index = static_cast<std::size_t>(
        time * static_cast<float>(ModelAnatomy::sliceCount - 1) + 0.5f);
    return std::min(index, ModelAnatomy::sliceCount - 1);
}

float followMeter(float current, float target, float attackCoefficient,
                  float releaseCoefficient) noexcept
{
    target = std::clamp(finiteOr(target, 0.0f), 0.0f, 1.0f);
    if (!std::isfinite(current))
        return target;
    current = std::clamp(current, 0.0f, 1.0f);
    const float coefficient = std::clamp(
        finiteOr(target > current ? attackCoefficient : releaseCoefficient,
                 1.0f),
        0.0f, 1.0f);
    return std::clamp(current + coefficient * (target - current), 0.0f, 1.0f);
}

float anatomyDisplayHeight(float amplitude, float peakAmplitude) noexcept
{
    amplitude = finiteOr(amplitude, 0.0f);
    peakAmplitude = finiteOr(peakAmplitude, 0.0f);
    if (!(amplitude > 0.0f) || !(peakAmplitude > 0.0f))
        return 0.0f;
    const float decibels = 20.0f * std::log10(amplitude / peakAmplitude);
    return std::clamp(1.0f + decibels / ModelAnatomy::displayFloorDb,
                      0.0f, 1.0f);
}

void clearModelAnatomy(ModelAnatomy& destination) noexcept
{
    destination = ModelAnatomy {};
    for (std::size_t bin = 0; bin < ModelAnatomy::binCount; ++bin)
    {
        destination.binFrequencyHz[bin] = anatomyFrequencyAt(
            static_cast<float>(bin)
            / static_cast<float>(ModelAnatomy::binCount - 1));
    }
}

void buildModelAnatomy(const NeuralModel& model,
                       ModelAnatomy& destination) noexcept
{
    clearModelAnatomy(destination);
    const auto& metadata = model.metadata();
    const float root = finiteOr(metadata.rootFrequencyHz, 0.0f);
    const float duration = std::max(finiteOr(metadata.durationSeconds, 0.0f),
                                    0.0f);
    if (!(root > 0.0f))
        return;

    destination.rootFrequencyHz = root;
    destination.durationSeconds = duration;
    destination.inharmonicity = model.inharmonicity();

    const auto airCentres = model.airCentreFrequenciesHz();
    const auto airWidths = model.airBandwidthOctaves();
    const auto boneRatios = model.boneFrequencyRatios();
    const auto boneReliabilities = model.boneModeReliabilities();

    std::array<float, ModelAnatomy::sliceCount> corePower {};
    std::array<float, ModelAnatomy::sliceCount> airPower {};
    std::array<float, ModelAnatomy::sliceCount> bonePower {};
    float peak = 0.0f;

    // The stretched-partial ratio depends only on the model's own fitted
    // inharmonicity, which is fixed above, not on the slice being drawn. It
    // is built once here instead of being recomputed sliceCount times for
    // every one of the harmonicCount partials in the loop below.
    std::array<float, NeuralModel::harmonicCount> stretchedHarmonicRatios {};
    for (std::size_t harmonic = 0; harmonic < NeuralModel::harmonicCount;
         ++harmonic)
        stretchedHarmonicRatios[harmonic] = stretchedHarmonicRatio(
            static_cast<float>(harmonic + 1), destination.inharmonicity);

    // Likewise, each Air band's centre octave and clamped width depend only
    // on the model's fitted geometry, fixed above and not on the slice below.
    std::array<AirBandGeometry, NeuralModel::airBandCount> airBandGeometry {};
    for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
        airBandGeometry[band] = makeAirBandGeometry(
            airCentres[band], airWidths[band]);

    for (std::size_t slice = 0; slice < ModelAnatomy::sliceCount; ++slice)
    {
        const float normalisedTime = static_cast<float>(slice)
            / static_cast<float>(ModelAnatomy::sliceCount - 1);
        destination.sliceTimeSeconds[slice] = normalisedTime * duration;

        SynthesisFrame frame;
        model.evaluate(normalisedTime, frame);
        const float pitchRatio = std::clamp(
            finiteOr(frame.pitchRatio, 1.0f), 0.25f, 4.0f);
        const float fundamental = root * pitchRatio;

        double coreSquares = 0.0;
        for (std::size_t harmonic = 0;
             harmonic < NeuralModel::harmonicCount; ++harmonic)
        {
            const float amplitude = std::max(
                finiteOr(frame.harmonicAmplitudes[harmonic], 0.0f), 0.0f);
            const float frequency = fundamental
                * stretchedHarmonicRatios[harmonic];
            depositPeak(destination.core[slice], frequency, amplitude, 1);
            coreSquares += static_cast<double>(amplitude) * amplitude;
            peak = std::max(peak, amplitude);
        }

        double airSquares = 0.0;
        for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
        {
            const float amplitude = std::max(
                finiteOr(frame.airAmplitudes[band], 0.0f), 0.0f);
            const auto& geometry = airBandGeometry[band];
            if (geometry.valid)
                depositBandAtOctave(destination.air[slice],
                                    geometry.centreOctave, geometry.width,
                                    amplitude);
            airSquares += static_cast<double>(amplitude) * amplitude;
            peak = std::max(peak, amplitude);
        }

        double boneSquares = 0.0;
        for (std::size_t mode = 0; mode < NeuralModel::boneModeCount; ++mode)
        {
            if (!(boneReliabilities[mode] > 0.0f))
                continue;
            const float amplitude = std::max(
                finiteOr(frame.boneAmplitudes[mode], 0.0f), 0.0f);
            depositPeak(destination.bone[slice],
                        fundamental * finiteOr(boneRatios[mode], 1.0f),
                        amplitude, 2);
            boneSquares += static_cast<double>(amplitude) * amplitude;
            peak = std::max(peak, amplitude);
        }

        corePower[slice] = static_cast<float>(std::sqrt(coreSquares));
        airPower[slice] = static_cast<float>(std::sqrt(airSquares));
        bonePower[slice] = static_cast<float>(std::sqrt(boneSquares));
    }

    if (!(peak > 0.0f))
        return;

    destination.peakAmplitude = peak;
    // Layer meters share one peak, the loudest any layer reaches across the
    // whole trajectory, so their relative balance stays readable instead of
    // each layer being normalised to its own maximum.
    float layerPeak = 0.0f;
    for (std::size_t slice = 0; slice < ModelAnatomy::sliceCount; ++slice)
        layerPeak = std::max({ layerPeak, corePower[slice], airPower[slice],
                               bonePower[slice] });

    for (std::size_t slice = 0; slice < ModelAnatomy::sliceCount; ++slice)
    {
        for (std::size_t bin = 0; bin < ModelAnatomy::binCount; ++bin)
        {
            destination.core[slice][bin] = anatomyDisplayHeight(
                destination.core[slice][bin], peak);
            destination.air[slice][bin] = anatomyDisplayHeight(
                destination.air[slice][bin], peak);
            destination.bone[slice][bin] = anatomyDisplayHeight(
                destination.bone[slice][bin], peak);
        }
        destination.coreLevel[slice] = anatomyDisplayHeight(
            corePower[slice], layerPeak);
        destination.airLevel[slice] = anatomyDisplayHeight(
            airPower[slice], layerPeak);
        destination.boneLevel[slice] = anatomyDisplayHeight(
            bonePower[slice], layerPeak);
    }
    destination.valid = true;
}

std::ptrdiff_t binIndexOfForTests(float frequencyHz) noexcept
{
    return binIndexOf(frequencyHz);
}

void depositPeakForTests(std::array<float, ModelAnatomy::binCount>& bins,
                         float frequencyHz, float amplitude,
                         int spread) noexcept
{
    depositPeak(bins, frequencyHz, amplitude, spread);
}

std::array<float, 3> makeAirBandGeometryForTests(
    float centreHz, float bandwidthOctaves) noexcept
{
    const auto geometry = makeAirBandGeometry(centreHz, bandwidthOctaves);
    return { geometry.valid ? 1.0f : 0.0f, geometry.centreOctave,
             geometry.width };
}

void depositBandAtOctaveForTests(
    std::array<float, ModelAnatomy::binCount>& bins, float centreOctave,
    float width, float amplitude) noexcept
{
    depositBandAtOctave(bins, centreOctave, width, amplitude);
}

} // namespace neuramar
