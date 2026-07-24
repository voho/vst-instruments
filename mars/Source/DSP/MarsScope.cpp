#include "MarsScope.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace mars
{
namespace
{
float finiteSample(float value) noexcept
{
    return std::isfinite(value) ? value : 0.0f;
}
} // namespace

void ScopeReducer::setColumns(int columns) noexcept
{
    const int requested = std::clamp(columns, minimumColumns, maximumColumns);
    if (requested == columns_)
        return;

    columns_ = requested;
    minimum_.fill(0.0f);
    maximum_.fill(0.0f);
}

void ScopeReducer::setBallistics(float framesPerSecond, float releaseSeconds,
                                 float holdSeconds) noexcept
{
    framesPerSecond = std::isfinite(framesPerSecond)
        ? std::clamp(framesPerSecond, 1.0f, 240.0f) : 12.0f;
    releaseSeconds = std::isfinite(releaseSeconds)
        ? std::clamp(releaseSeconds, 0.01f, 10.0f) : 0.6f;
    holdSeconds = std::isfinite(holdSeconds)
        ? std::clamp(holdSeconds, 0.0f, 10.0f) : 1.0f;

    releaseCoefficient_ = std::exp(-1.0f / (framesPerSecond * releaseSeconds));
    // The RMS bar settles roughly four times faster than the peak falls back,
    // which keeps it readable without turning it into a second peak meter.
    rmsCoefficient_ = 1.0f - std::exp(-4.0f / (framesPerSecond * releaseSeconds));
    holdFrames_ = static_cast<int>(std::lround(framesPerSecond * holdSeconds));
    holdCounter_ = std::min(holdCounter_, holdFrames_);
}

void ScopeReducer::reset() noexcept
{
    minimum_.fill(0.0f);
    maximum_.fill(0.0f);
    peak_ = 0.0f;
    peakHold_ = 0.0f;
    rms_ = 0.0f;
    holdCounter_ = 0;
}

void ScopeReducer::reduce(const float* samples, int count) noexcept
{
    const int columns = columns_;
    if (samples == nullptr || count <= 0)
    {
        for (int column = 0; column < columns; ++column)
        {
            minimum_[static_cast<std::size_t>(column)] = 0.0f;
            maximum_[static_cast<std::size_t>(column)] = 0.0f;
        }
        count = 0;
    }

    double sumOfSquares = 0.0;
    float blockPeak = 0.0f;
    float carried = 0.0f;

    for (int column = 0; column < columns && count > 0; ++column)
    {
        const auto begin = static_cast<int>(
            static_cast<long long>(count) * column / columns);
        const auto end = static_cast<int>(
            static_cast<long long>(count) * (column + 1) / columns);

        float lowest = carried;
        float highest = carried;
        if (end > begin)
        {
            lowest = finiteSample(samples[begin]);
            highest = lowest;
            for (int index = begin; index < end; ++index)
            {
                const float value = finiteSample(samples[index]);
                lowest = std::min(lowest, value);
                highest = std::max(highest, value);
                sumOfSquares += static_cast<double>(value) * static_cast<double>(value);
                blockPeak = std::max(blockPeak, std::abs(value));
            }
            // A column narrower than one sample repeats the last value it saw
            // instead of collapsing the trace to the centre line.
            carried = finiteSample(samples[end - 1]);
        }

        minimum_[static_cast<std::size_t>(column)] = lowest;
        maximum_[static_cast<std::size_t>(column)] = highest;
    }

    const float blockRms = count > 0
        ? static_cast<float>(std::sqrt(sumOfSquares / static_cast<double>(count)))
        : 0.0f;

    // Instant attack, exponential release: a transient is never under-read and
    // the display still settles smoothly.
    peak_ = std::max(blockPeak, peak_ * releaseCoefficient_);
    if (peak_ < 1.0e-6f)
        peak_ = 0.0f;
    rms_ += rmsCoefficient_ * (blockRms - rms_);
    if (rms_ < 1.0e-6f)
        rms_ = 0.0f;

    if (blockPeak >= peakHold_)
    {
        peakHold_ = blockPeak;
        holdCounter_ = holdFrames_;
    }
    else if (holdCounter_ > 0)
    {
        --holdCounter_;
    }
    else
    {
        peakHold_ *= releaseCoefficient_;
        if (peakHold_ < 1.0e-6f)
            peakHold_ = 0.0f;
    }
}

float ScopeReducer::columnMinimum(int column) const noexcept
{
    if (column < 0 || column >= columns_)
        return 0.0f;
    return minimum_[static_cast<std::size_t>(column)];
}

float ScopeReducer::columnMaximum(int column) const noexcept
{
    if (column < 0 || column >= columns_)
        return 0.0f;
    return maximum_[static_cast<std::size_t>(column)];
}

int ScopeReducer::findRisingEdge(const float* samples, int count) noexcept
{
    if (samples == nullptr || count < 4)
        return 0;

    const int limit = count / 2;
    for (int index = 1; index < limit; ++index)
    {
        const float previous = finiteSample(samples[index - 1]);
        const float current = finiteSample(samples[index]);
        if (previous <= 0.0f && current > 0.0f)
            return index;
    }
    return 0;
}

float ScopeReducer::amplitudeToMeter(float amplitude, float floorDecibels) noexcept
{
    if (!std::isfinite(amplitude))
        return 0.0f;
    floorDecibels = std::isfinite(floorDecibels)
        ? std::clamp(floorDecibels, -120.0f, -6.0f) : -60.0f;

    const float magnitude = std::abs(amplitude);
    if (magnitude <= 1.0e-7f)
        return 0.0f;

    const float decibels = 20.0f * std::log10(magnitude);
    return std::clamp((decibels - floorDecibels) / -floorDecibels, 0.0f, 1.0f);
}

float ScopeReducer::meterWarmth(float meterValue) noexcept
{
    if (!std::isfinite(meterValue))
        return 0.0f;

    // -12 dBFS on the default -60 dB window is 0.8 of the bar. Below the knee
    // the ramp stays cool; above it the last fifth of the meter heats quickly.
    constexpr float knee = 0.8f;
    const float value = std::clamp(meterValue, 0.0f, 1.0f);
    if (value <= knee)
        return 0.35f * (value / knee);
    return 0.35f + 0.65f * ((value - knee) / (1.0f - knee));
}

} // namespace mars
