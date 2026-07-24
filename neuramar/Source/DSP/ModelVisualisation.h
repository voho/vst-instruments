#pragma once

#include "NeuralModel.h"

#include <array>
#include <cstddef>

namespace neuramar
{

// A compact, display-ready reduction of a learned memory. Everything the
// editor draws is derived here, in the JUCE-free layer, so the reduction is
// unit-tested on every platform and the editor stays a thin renderer. The
// structure is fixed-size and copyable; building one performs no allocation.
struct ModelAnatomy
{
    // 96 log-spaced cells across 20 Hz - 20 kHz is 32 per decade: fine enough
    // to separate the low partials of a bass note, coarse enough that a whole
    // trajectory stays small enough to copy at UI frame rate.
    static constexpr std::size_t binCount = 96;
    static constexpr std::size_t sliceCount = 24;
    static constexpr float lowestFrequencyHz = 20.0f;
    static constexpr float highestFrequencyHz = 20000.0f;
    // Display floor. A learned Core spans far more than 72 dB, but anything
    // below this is inaudible next to the partials that define the timbre.
    static constexpr float displayFloorDb = 72.0f;

    bool valid { false };
    float rootFrequencyHz { 0.0f };
    float durationSeconds { 0.0f };
    float inharmonicity { 0.0f };
    // Peak linear amplitude found anywhere in the trajectory. Every stored
    // curve is a 0..1 display height relative to it.
    float peakAmplitude { 0.0f };

    std::array<float, binCount> binFrequencyHz {};
    std::array<std::array<float, binCount>, sliceCount> core {};
    std::array<std::array<float, binCount>, sliceCount> air {};
    std::array<std::array<float, binCount>, sliceCount> bone {};
    // Per-slice layer loudness, already mapped to the same 0..1 display scale.
    std::array<float, sliceCount> coreLevel {};
    std::array<float, sliceCount> airLevel {};
    std::array<float, sliceCount> boneLevel {};
    std::array<float, sliceCount> sliceTimeSeconds {};
};

// Fills `destination` from an immutable model. Safe to call on any thread that
// owns the model; never called from the audio path.
void buildModelAnatomy(const NeuralModel& model,
                       ModelAnatomy& destination) noexcept;

// Clears `destination` to the "no memory yet" state.
void clearModelAnatomy(ModelAnatomy& destination) noexcept;

// Horizontal display mapping. `normalisedPosition` is 0 at 20 Hz and 1 at
// 20 kHz; both directions clamp instead of producing out-of-range values.
[[nodiscard]] float anatomyFrequencyAt(float normalisedPosition) noexcept;
[[nodiscard]] float anatomyPositionOf(float frequencyHz) noexcept;

// Selects the trajectory slice nearest a normalised time in [0, 1].
[[nodiscard]] std::size_t anatomySliceAt(float normalisedTime) noexcept;

// Deterministic meter ballistics for the editor: fast attack, slow release,
// both expressed as a per-frame coefficient in [0, 1]. Non-finite inputs
// collapse to the target rather than latching a NaN into the display.
[[nodiscard]] float followMeter(float current, float target,
                                float attackCoefficient,
                                float releaseCoefficient) noexcept;

// Maps a linear amplitude to the same 0..1 display height the anatomy curves
// use, so an editor can place readouts on the identical scale.
[[nodiscard]] float anatomyDisplayHeight(float amplitude,
                                         float peakAmplitude) noexcept;

} // namespace neuramar
