#include "UiMath.h"

#include <algorithm>
#include <cmath>

namespace drumalor::ui
{
namespace
{
constexpr float logOfTen = 2.30258509299404568402f;

float sanitize (float value, float fallback) noexcept
{
    return std::isfinite (value) ? value : fallback;
}
} // namespace

float onePoleCoefficient (float seconds, float updateRateHz) noexcept
{
    updateRateHz = std::max (1.0f, sanitize (updateRateHz, 30.0f));
    seconds = std::max (0.0f, sanitize (seconds, 0.0f));
    if (seconds <= 0.0f)
        return 1.0f;
    return 1.0f - std::exp (-1.0f / std::max (1.0f, seconds * updateRateHz));
}

float decayMultiplier (float decibels, float seconds, float updateRateHz) noexcept
{
    updateRateHz = std::max (1.0f, sanitize (updateRateHz, 30.0f));
    seconds = std::max (0.0f, sanitize (seconds, 0.0f));
    decibels = std::min (0.0f, sanitize (decibels, -60.0f));
    if (seconds <= 0.0f)
        return 0.0f;
    return std::exp (decibels * logOfTen / (20.0f * std::max (1.0f, seconds * updateRateHz)));
}

float meterPositionForLinear (float linear, float floorDecibels) noexcept
{
    floorDecibels = std::min (-1.0f, sanitize (floorDecibels, -60.0f));
    linear = std::max (0.0f, sanitize (linear, 0.0f));
    const float floorLinear = std::exp (floorDecibels * logOfTen / 20.0f);
    if (linear <= floorLinear)
        return 0.0f;
    const float decibels = 20.0f * std::log10 (linear);
    return std::clamp (1.0f - decibels / floorDecibels, 0.0f, 1.0f);
}

float linearForMeterPosition (float position, float floorDecibels) noexcept
{
    floorDecibels = std::min (-1.0f, sanitize (floorDecibels, -60.0f));
    position = std::clamp (sanitize (position, 0.0f), 0.0f, 1.0f);
    const float decibels = floorDecibels * (1.0f - position);
    return std::exp (decibels * logOfTen / 20.0f);
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
    target = std::max (0.0f, sanitize (target, 0.0f));
    attackCoefficient = std::clamp (sanitize (attackCoefficient, 1.0f), 0.0f, 1.0f);
    releaseCoefficient = std::clamp (sanitize (releaseCoefficient, 0.2f), 0.0f, 1.0f);
    peakFall = std::clamp (sanitize (peakFall, 0.9f), 0.0f, 1.0f);
    holdUpdates = std::max (0.0f, sanitize (holdUpdates, 0.0f));

    const float coefficient = target > level ? attackCoefficient : releaseCoefficient;
    level = std::clamp (level + coefficient * (target - level), 0.0f, 16.0f);

    if (target >= peak)
    {
        peak = std::min (16.0f, target);
        holdCountdown = holdUpdates;
    }
    else if (holdCountdown > 0.0f)
    {
        holdCountdown -= 1.0f;
    }
    else
    {
        peak *= peakFall;
    }
    peak = std::max (peak, level);
}

RowLayout rowLayout (int extent, int columns, int gap, int count) noexcept
{
    RowLayout layout;
    columns = std::max (1, columns);
    gap = std::max (0, gap);
    count = std::clamp (count, 1, columns);
    extent = std::max (0, extent);

    layout.cellSize = std::max (1, (extent - gap * (columns - 1)) / columns);
    const int used = layout.cellSize * count + gap * (count - 1);
    layout.origin = std::max (0, (extent - used) / 2);
    return layout;
}

int cellOffset (const RowLayout& layout, int gap, int index) noexcept
{
    gap = std::max (0, gap);
    index = std::max (0, index);
    return layout.origin + index * (std::max (1, layout.cellSize) + gap);
}

float mix (float a, float b, float amount) noexcept
{
    amount = std::clamp (sanitize (amount, 0.0f), 0.0f, 1.0f);
    return sanitize (a, 0.0f) + amount * (sanitize (b, 0.0f) - sanitize (a, 0.0f));
}

float smoothStep (float edge0, float edge1, float value) noexcept
{
    edge0 = sanitize (edge0, 0.0f);
    edge1 = sanitize (edge1, 1.0f);
    value = sanitize (value, 0.0f);
    const float span = edge1 - edge0;
    if (std::abs (span) < 1.0e-12f)
        return value >= edge1 ? 1.0f : 0.0f;
    const float normalized = std::clamp ((value - edge0) / span, 0.0f, 1.0f);
    return normalized * normalized * (3.0f - 2.0f * normalized);
}

} // namespace drumalor::ui
