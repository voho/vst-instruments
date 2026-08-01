#pragma once

#include <array>
#include <cstdint>

namespace youknow106
{

// Panel switch. Two latching buttons that select between them rather than
// combining: the instrument's patch memory stores chorus as one on/off bit and
// one mode bit, so there is no both-at-once setting. The earlier instrument in
// the same family had one, and it is deliberately absent here.
enum class ChorusMode { Off, One, Two };

// Two 256-stage bucket-brigade delay lines with anti-phase clock modulation,
// as used by the modelled instrument's stereo chorus. There is deliberately no
// compander: the hiss the line adds is part of the effect's signature, so
// removing it would be a different effect.
//
// Everything here is JUCE-free so the regression suites can drive it directly
// and compare it against an independently integrated reference.
class Chorus
{
public:
    // MN3009-class line: 256 stages, two-phase clocked, so one full delay is
    // stages / (2 * clock). At the 10 kHz minimum clock that is the part's
    // quoted 12.8 ms maximum.
    static constexpr int stages = 256;
    static constexpr int cellPairs = stages / 2;

    // Clock frequencies the modelled analogue front end can reach. The line
    // aliases everything above half the clock, so the model refuses to run it
    // outside the part's rated window.
    static constexpr float minimumClockHz = 10000.0f;
    static constexpr float maximumClockHz = 200000.0f;
    // Lowest host rate the engine accepts. The fastest clock against it is the
    // most edges a single sample can ever have to consume.
    static constexpr float minimumSampleRate = 8000.0f;
    static constexpr int maximumShiftsPerSample =
        static_cast<int>(maximumClockHz / minimumSampleRate) + 2;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Advances one sample. `noiseScale` scales the modelled BBD noise floor;
    // 1.0 is the hardware level and 0.0 removes it entirely.
    void process(float input, ChorusMode mode, float noiseScale,
                 float& left, float& right) noexcept;

    // The nominal (unmodulated) delay in seconds for a clock frequency.
    [[nodiscard]] static constexpr float delaySecondsForClock(float clockHz) noexcept
    {
        return static_cast<float>(cellPairs) / clockHz;
    }
    [[nodiscard]] static constexpr float clockForDelaySeconds(float seconds) noexcept
    {
        return static_cast<float>(cellPairs) / seconds;
    }

    // Modulation constants per panel mode, exposed so the suites can assert
    // them against the documented figures rather than re-deriving them.
    struct ModeSettings
    {
        float rateHz { 0.0f };
        float centreDelaySeconds { 0.0f };
        float sweepSeconds { 0.0f };
        float wetGain { 0.0f };
    };

    [[nodiscard]] static ModeSettings settingsFor(ChorusMode mode) noexcept;

    // The support filters either side of the line, exposed as a coefficient and
    // a single step so the suites can measure where the corner actually lands
    // rather than trusting that the two match. They only agree for one pairing:
    // this coefficient belongs to this recursion and to no other.
    [[nodiscard]] static float onePoleG(float cutoffHz, float sampleRate) noexcept;
    static float supportFilterStep(float& state, float input, float g) noexcept;

    [[nodiscard]] float getLfoPhase() const noexcept { return lfoPhase_; }

private:
    // One bucket-brigade line: a shift register of cell pairs clocked
    // asynchronously to the host rate, with the input resampled onto the clock
    // grid and the output held between clock edges exactly as the part does.
    struct Line
    {
        std::array<float, cellPairs> cells {};
        int writeIndex { 0 };
        double clockPhase { 0.0 };
        float held { 0.0f };
        float previousInput { 0.0f };
        float antiAliasState { 0.0f };
        // One pole each side. The line's zero-order-hold images sit at the
        // clock rate, which the engine's own decimation removes, so the
        // support filters only have to do what the hardware's do.
        float reconstructionState { 0.0f };
        float transferState { 0.0f };
        std::uint32_t noiseState { 0x9e3779b9u };

        void reset(std::uint32_t seed) noexcept;
        float process(float input, float clockHz, float sampleRate,
                      float antiAliasG, float reconstructionG,
                      float noiseScale) noexcept;
    };

    Line lineA_ {};
    Line lineB_ {};
    float sampleRate_ { 48000.0f };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    float lfoPhase_ { 0.0f };
    float antiAliasG_ { 0.1f };
    float reconstructionG_ { 0.1f };
    float wetGain_ { 0.0f };
    float rateHz_ { 0.0f };
    float centreDelay_ { 0.0032f };
    float sweep_ { 0.0f };
};

} // namespace youknow106
