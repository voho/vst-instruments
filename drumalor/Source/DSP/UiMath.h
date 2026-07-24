#pragma once

namespace drumalor::ui
{

// Pure, JUCE-free presentation mathematics shared by the editor. Keeping meter
// ballistics, level curves and grid geometry here means the parts of the
// interface that can actually be wrong numerically are unit-tested on every
// platform, while the JUCE layer stays a thin renderer.

// Exponential smoothing coefficient that reaches roughly 63 % of a step in
// `seconds` when applied once per update at `updateRateHz`.
[[nodiscard]] float onePoleCoefficient (float seconds, float updateRateHz) noexcept;

// Per-update multiplier that falls by `decibels` (a negative value) over
// `seconds`. Used for meter peak-hold fall and pad afterglow.
[[nodiscard]] float decayMultiplier (float decibels, float seconds,
                                     float updateRateHz) noexcept;

// Maps a linear amplitude onto 0..1 along a decibel scale with a fixed floor,
// so a meter stays readable at the low levels a drum tail actually occupies.
[[nodiscard]] float meterPositionForLinear (float linear, float floorDecibels) noexcept;

// Inverse of meterPositionForLinear; used to place scale ticks.
[[nodiscard]] float linearForMeterPosition (float position, float floorDecibels) noexcept;

// Asymmetric meter ballistics: fast attack, slow release, plus a peak marker
// that holds for a while and then falls at a fixed rate.
struct MeterBallistics
{
    float level { 0.0f };
    float peak { 0.0f };
    float holdCountdown { 0.0f };

    void reset() noexcept;
    void update (float target, float attackCoefficient, float releaseCoefficient,
                 float peakFall, float holdUpdates) noexcept;
};

// Equal-width cells with a fixed gap, optionally holding fewer cells than the
// grid is sized for so a short row stays centred under a longer one.
struct RowLayout
{
    int cellSize { 1 };
    int origin { 0 };
};

[[nodiscard]] RowLayout rowLayout (int extent, int columns, int gap, int count) noexcept;

// Cell rectangle offsets inside a row laid out by rowLayout().
[[nodiscard]] int cellOffset (const RowLayout& layout, int gap, int index) noexcept;

[[nodiscard]] float mix (float a, float b, float amount) noexcept;
[[nodiscard]] float smoothStep (float edge0, float edge1, float value) noexcept;

} // namespace drumalor::ui
