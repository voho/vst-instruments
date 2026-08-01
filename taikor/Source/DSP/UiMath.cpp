#include "DSP/UiMath.h"

#include <cmath>

namespace taikor::ui
{
namespace
{
constexpr float minimumLinear = 1.0e-6f;
} // namespace

float clamp (float value, float low, float high) noexcept
{
    if (! (value == value))
        return low;
    return value < low ? low : (value > high ? high : value);
}

float onePoleCoefficient (float seconds, float updateRateHz) noexcept
{
    if (! (updateRateHz > 0.0f))
        return 1.0f;
    if (! (seconds > 0.0f))
        return 1.0f;
    return 1.0f - std::exp (-1.0f / (seconds * updateRateHz));
}

float decayMultiplier (float decibels, float seconds, float updateRateHz) noexcept
{
    if (! (updateRateHz > 0.0f) || ! (seconds > 0.0f))
        return 0.0f;
    const float updates = seconds * updateRateHz;
    return std::pow (10.0f, decibels / (20.0f * updates));
}

float meterPositionForLinear (float linear, float floorDecibels) noexcept
{
    if (! (floorDecibels < 0.0f))
        return 0.0f;
    const float bounded = linear > minimumLinear ? linear : minimumLinear;
    const float decibels = 20.0f * std::log10 (bounded);
    return clamp ((decibels - floorDecibels) / -floorDecibels, 0.0f, 1.0f);
}

float linearForMeterPosition (float position, float floorDecibels) noexcept
{
    if (! (floorDecibels < 0.0f))
        return 0.0f;
    const float bounded = clamp (position, 0.0f, 1.0f);
    const float decibels = floorDecibels + bounded * -floorDecibels;
    return std::pow (10.0f, decibels / 20.0f);
}

void MeterBallistics::reset() noexcept
{
    level = 0.0f;
    peak = 0.0f;
    holdCountdown = 0.0f;
}

void MeterBallistics::update (float target, float attackCoefficient,
                              float releaseCoefficient, float peakFall,
                              float holdUpdates) noexcept
{
    const float bounded = target > 0.0f ? target : 0.0f;
    const float coefficient = bounded > level ? attackCoefficient : releaseCoefficient;
    level += clamp (coefficient, 0.0f, 1.0f) * (bounded - level);

    if (level >= peak)
    {
        peak = level;
        holdCountdown = holdUpdates;
    }
    else if (holdCountdown > 0.0f)
    {
        holdCountdown -= 1.0f;
    }
    else
    {
        peak *= clamp (peakFall, 0.0f, 1.0f);
        if (peak < level)
            peak = level;
    }
}

RowLayout rowLayout (int extent, int columns, int gap, int count) noexcept
{
    RowLayout layout;
    if (columns <= 0 || extent <= 0)
        return layout;

    const int totalGap = gap * (columns - 1);
    const int available = extent - totalGap;
    layout.cellSize = available > columns ? available / columns : 1;

    const int used = count > 0 && count <= columns
        ? count * layout.cellSize + gap * (count - 1)
        : columns * layout.cellSize + totalGap;
    layout.origin = (extent - used) / 2;
    if (layout.origin < 0)
        layout.origin = 0;

    return layout;
}

int cellOffset (const RowLayout& layout, int gap, int index) noexcept
{
    if (index <= 0)
        return layout.origin;
    return layout.origin + index * (layout.cellSize + gap);
}

HeadPoint headPointFor (float normalisedRadius, float angleRadians) noexcept
{
    const float radius = clamp (normalisedRadius, 0.0f, 1.0f);
    HeadPoint point;
    point.x = radius * std::cos (angleRadians);
    // Screen coordinates run downwards, so the sine is negated once here
    // rather than at every call site.
    point.y = -radius * std::sin (angleRadians);
    return point;
}

float semitonesBetween (float frequencyHz, float referenceHz) noexcept
{
    if (! (frequencyHz > 0.0f) || ! (referenceHz > 0.0f))
        return 0.0f;
    return 12.0f * std::log2 (frequencyHz / referenceHz);
}

float mix (float a, float b, float amount) noexcept
{
    return a + (b - a) * clamp (amount, 0.0f, 1.0f);
}

float smoothStep (float edge0, float edge1, float value) noexcept
{
    if (edge0 == edge1)
        return value < edge0 ? 0.0f : 1.0f;
    const float t = clamp ((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace taikor::ui
