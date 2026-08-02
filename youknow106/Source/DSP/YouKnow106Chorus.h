#pragma once

#include <array>
#include <cstdint>

namespace youknow106
{

// Panel state of the two latching chorus buttons.
//
// **I+II is an addition this plug-in makes, not modelled hardware.** The
// instrument's own manual is explicit -- "It is not possible to use I and II at
// the same time" -- and the board agrees: it carries one chorus enable bit and
// one binary I/II bit, not two independent switches. Owners who want the mode
// fit a board modification to get it.
//
// It is kept here deliberately, as a feature rather than as a claim, because it
// is useful and because turning both buttons off and on independently is what
// this panel offers. Everything about how it *behaves* is still derived from
// the circuit: each button's timing resistor in parallel, so the conductances
// and with them the rates add. What is not claimed is that a Juno-106 can do
// it. (The 1.376 Hz that follows is likewise not the 9.75 Hz a Juno-60 reaches
// with both buttons down; that instrument's chorus is a different circuit.)
//
// The patch memory cannot hold all four states in any case. It stores the
// effect as one on/off bit and one mode bit, so a saved patch can only say off,
// I or II -- which is consistent with there being no fourth state to store. The
// SysEx writer records that limitation rather than pretending otherwise.
enum class ChorusMode { Off, One, Two, OneTwo };

// The mode the two buttons select. Neither button is the only "off" state.
[[nodiscard]] constexpr ChorusMode chorusModeFor(bool one, bool two) noexcept
{
    if (one && two)
        return ChorusMode::OneTwo;
    if (two)
        return ChorusMode::Two;
    if (one)
        return ChorusMode::One;
    return ChorusMode::Off;
}

[[nodiscard]] constexpr bool chorusOneEngaged(ChorusMode mode) noexcept
{
    return mode == ChorusMode::One || mode == ChorusMode::OneTwo;
}

[[nodiscard]] constexpr bool chorusTwoEngaged(ChorusMode mode) noexcept
{
    return mode == ChorusMode::Two || mode == ChorusMode::OneTwo;
}

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

    // One of the emitter-follower two-pole sections either side of the line.
    // Each is an equal-resistor Sallen-Key, so its Q is fixed by the ratio of
    // its two capacitors alone: Q = 0.5 * sqrt(C_feedback / C_shunt).
    struct BiquadCoefficients
    {
        float g { 0.1f };   // tan(pi * fc / fs)
        float k { 2.0f };   // 1 / Q
    };
    [[nodiscard]] static BiquadCoefficients sallenKeyCoefficients(
        float cutoffHz, float q, float sampleRate) noexcept;
    [[nodiscard]] static float sallenKeyQ(float feedbackFarads,
                                          float shuntFarads) noexcept;

    // Topology-preserving state-variable section, taken at its lowpass output.
    // Two states, advanced together, so it stays stable while the delay either
    // side of it is being swept.
    struct BiquadState
    {
        float s1 { 0.0f };
        float s2 { 0.0f };
        void reset() noexcept { s1 = 0.0f; s2 = 0.0f; }
    };
    static float biquadStep(BiquadState&, float input,
                            const BiquadCoefficients&) noexcept;

    // Every coefficient the two chains need, built once per prepare(). The
    // sections are fixed networks -- nothing on the panel reaches them -- so
    // there is nothing to recompute per sample.
    struct SupportChain
    {
        float passiveG { 0.1f };            // R122 / C52, ahead of the line
        BiquadCoefficients antiAliasFirst {};
        BiquadCoefficients antiAliasSecond {};
        BiquadCoefficients reconstructionFirst {};
        BiquadCoefficients reconstructionSecond {};
    };
    [[nodiscard]] static SupportChain supportChainFor(float sampleRate) noexcept;

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
        // Five poles in, four out, matching the board: two Sallen-Key sections
        // plus one passive pole ahead of the line, two Sallen-Key sections
        // after it.
        //
        // When the engine oversamples, the line's zero-order-hold images at the
        // clock rate land above the host band and the decimators remove them;
        // with oversampling off the clock exceeds the host Nyquist and the
        // images fold, with only the output chain to soften them. That is a
        // documented cost of the low-quality setting, not of the model -- and
        // four poles soften it considerably better than the one this replaced.
        float antiAliasState { 0.0f };
        BiquadState antiAliasFirst {};
        BiquadState antiAliasSecond {};
        BiquadState reconstructionFirst {};
        BiquadState reconstructionSecond {};
        float transferState { 0.0f };
        std::uint32_t noiseState { 0x9e3779b9u };

        void reset(std::uint32_t seed) noexcept;
        float process(float input, float clockHz, float sampleRate,
                      const SupportChain& support, float noiseScale) noexcept;
    };

    Line lineA_ {};
    Line lineB_ {};
    float sampleRate_ { 48000.0f };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    float lfoPhase_ { 0.0f };
    SupportChain support_ {};
    float wetGain_ { 0.0f };
    float rateHz_ { 0.0f };
    float centreDelay_ { 0.0032f };
    float sweep_ { 0.0f };
    // Whether the glided settings have a starting point yet.
    bool primed_ { false };
};

} // namespace youknow106
