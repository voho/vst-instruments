#pragma once

#include <array>

namespace mars
{

/*
    Display arithmetic for the front panel's signal scope and output meter.

    None of this is JUCE code: the editor stays a thin renderer and every
    reduction, ballistic, and mapping below is covered by the JUCE-free
    regression suite. The reducer owns fixed storage, so a UI frame performs no
    allocation either.
*/
class ScopeReducer
{
public:
    static constexpr int maximumColumns = 256;
    static constexpr int minimumColumns = 2;

    void setColumns(int columns) noexcept;
    [[nodiscard]] int getColumns() const noexcept { return columns_; }

    // `framesPerSecond` is the editor's repaint rate. The release time governs
    // the meter fall-back and the hold time how long a peak marker sticks.
    void setBallistics(float framesPerSecond, float releaseSeconds,
                       float holdSeconds) noexcept;

    void reset() noexcept;

    // Distributes `count` samples evenly across the configured columns, storing
    // the minimum and maximum of each, and advances the meter ballistics with
    // the block's peak and RMS.
    void reduce(const float* samples, int count) noexcept;

    [[nodiscard]] float columnMinimum(int column) const noexcept;
    [[nodiscard]] float columnMaximum(int column) const noexcept;
    [[nodiscard]] float peak() const noexcept { return peak_; }
    [[nodiscard]] float peakHold() const noexcept { return peakHold_; }
    [[nodiscard]] float rms() const noexcept { return rms_; }

    // Index of the first upward zero crossing inside the first half of the
    // block, so a periodic waveform is drawn from a stable starting phase.
    // Returns zero when the block contains no usable crossing.
    [[nodiscard]] static int findRisingEdge(const float* samples, int count) noexcept;

    // Linear amplitude to a 0..1 meter position across a decibel window ending
    // at full scale. Non-finite input reads as silence.
    [[nodiscard]] static float amplitudeToMeter(float amplitude,
                                                float floorDecibels = -60.0f) noexcept;

    // 0..1 position along a calm-to-hot colour ramp. The knee sits where the
    // meter passes -12 dBFS so the panel only warms up near clipping.
    [[nodiscard]] static float meterWarmth(float meterValue) noexcept;

private:
    std::array<float, maximumColumns> minimum_ {};
    std::array<float, maximumColumns> maximum_ {};
    int columns_ { 96 };
    int holdFrames_ { 12 };
    int holdCounter_ { 0 };
    float releaseCoefficient_ { 0.72f };
    float rmsCoefficient_ { 0.45f };
    float peak_ { 0.0f };
    float peakHold_ { 0.0f };
    float rms_ { 0.0f };
};

} // namespace mars
