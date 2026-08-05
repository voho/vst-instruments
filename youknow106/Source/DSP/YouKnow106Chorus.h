#pragma once

#include <array>
#include <cstdint>

namespace youknow106
{

// Panel state of the two latching chorus buttons.
//
// The instrument has three states: off, I and II. Its owner manual explicitly
// says I and II cannot be used simultaneously, and the jack board receives one
// enable line plus one binary I/II line. `OneTwo` remains as an input-compatibility
// value for sessions written by the short-lived plug-in extension; the engine
// canonicalises it to II and never synthesises an invented fourth clock mode.
enum class ChorusMode { Off, One, Two, OneTwo };

// The mode the two buttons select. Neither button engaged is the only Off state.
[[nodiscard]] constexpr ChorusMode chorusModeFor(bool one, bool two) noexcept
{
    if (one && two)
        return ChorusMode::Two;
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

    // IC6 is the final inverting dry/wet summer on each output. Its 100 kOhm
    // feedback resistor sees dry through 39 kOhm and wet through 47 kOhm.
    // Polarity is intentionally omitted because both paths invert together;
    // these magnitudes preserve the analogue drive into the following output
    // and VOLUME stages.
    static constexpr float dryMixGain = 100.0f / 39.0f;
    static constexpr float wetMixGain = 100.0f / 47.0f;
    static constexpr float wetToDryGain = wetMixGain / dryMixGain;
    // Explicit plug-in declick policy, not a measured mute-transistor value.
    static constexpr float wetMuteTimeConstantSeconds = 0.005f;

    // A live engine-quality change alters only the numerical sample grid.
    // `preserveState` retains the BBD buckets and all free-running phases/RNGs;
    // audio-rate TPT carries are cleared under the engine's zero-gain fade
    // because they contain the old timestep rather than literal voltages.
    void prepare(double sampleRate,
                 bool preserveState = false) noexcept;
    void reset(bool preserveLfoPhase = false) noexcept;

    // Advances one sample. `noiseScale` is the single master for every
    // declared chorus-noise component; 1.0 preserves the compatibility hiss
    // and 0.0 removes all of them. A calibrated hardware noise reference has
    // not yet been located, so the optional common/hum/spur amplitudes are zero.
    void process(float input, ChorusMode mode, float noiseScale,
                 float& left, float& right,
                 bool enableCapacitanceNonlinearity = true) noexcept;

    // The nominal (unmodulated) delay in seconds for a clock frequency.
    [[nodiscard]] static constexpr float delaySecondsForClock(float clockHz) noexcept
    {
        return static_cast<float>(cellPairs) / clockHz;
    }
    [[nodiscard]] static constexpr float clockForDelaySeconds(float seconds) noexcept
    {
        return static_cast<float>(cellPairs) / seconds;
    }

    // ------------------------------------------------------------------
    // The modulation oscillator, from Service Notes p. 15 (jack board, IC1
    // uPC062 and IC2a M5218L).
    //
    // IC1b integrates: C3 sits across pins 6 and 7 and pin 5 is grounded.
    // IC1a compares: R6 47 kOhm returns its own output to pin 3 while R15
    // 1 kOhm holds pin 3 down, so the feedback is positive, and R7 33 kOhm
    // brings the integrator's output back to pin 2 to close the loop. That is
    // an integrator-plus-comparator pair, whose capacitor is charged from a
    // constant current for the whole of each half cycle. Its output is a
    // straight, symmetric triangle -- not the exponential segments an RC
    // relaxation oscillator would give -- and `triangle()` below is that shape.
    // IC2a then inverts it once through R10/R9 33 kOhm, so the second line's
    // modulation is exactly the negative of the first's rather than an
    // independent oscillator: TP4 carries the triangle and TP3 its inverse.
    //
    // The CHORUS I/II line drives JFET Tr1, which *shorts R3* -- it does not
    // insert it. R5 reaches the integrator's virtual ground through R8, and the
    // shunt leg returns to ground, so both legs terminate at 0 V and the
    // charging current sees
    //
    //     R_eff = R8 * R5 / (R_sh || R8) + R8 ,
    //
    // with R_sh = R4 alone while Tr1 conducts and R4 + R3 while it does not.
    // The mode with the larger R_eff integrates more slowly, which is mode I.
    static constexpr double lfoSeriesOhms = 1.0e6;         // R5
    static constexpr double lfoIntegratorOhms = 2.2e6;     // R8
    static constexpr double lfoShuntOhms = 680.0e3;        // R4
    static constexpr double lfoModeShuntOhms = 2.2e6;      // R3, shorted in I
    // R15 / (R15 + R6). The comparator trips when the triangle reaches this
    // fraction of the saturated output, so the triangle spans +/- that fraction
    // and the oscillation is f = 1 / (4 * beta * R_eff * C3). Every term of
    // that expression is now known except C3, which the schematic does not
    // print -- so one reading of C3, or of one period at TP4, would fix the
    // absolute rates this model still borrows from a sibling. See OQ-01.
    static constexpr double lfoThresholdRatio = 1.0 / 48.0;

    [[nodiscard]] static constexpr double lfoTimingOhms(bool modeOne) noexcept
    {
        const double shunt = modeOne ? lfoShuntOhms
                                     : lfoShuntOhms + lfoModeShuntOhms;
        const double parallel = shunt * lfoIntegratorOhms
                              / (shunt + lfoIntegratorOhms);
        return lfoIntegratorOhms * lfoSeriesOhms / parallel + lfoIntegratorOhms;
    }

    // 6.4352941 MOhm over 3.9638889 MOhm. This ratio is the JUNO-106's own,
    // derived from its own schematic; only the absolute scale below is still
    // borrowed. The sibling's measured pair implies 1.682, which agrees to
    // about 3.6% -- close enough to say the two instruments share this circuit,
    // not close enough to keep using the sibling's decimals here.
    [[nodiscard]] static constexpr double modeRateRatio() noexcept
    {
        return lfoTimingOhms(true) / lfoTimingOhms(false);
    }

    // The JUNO-60's measured rates. They remain this model's only source for
    // the absolute scale, and are kept here verbatim so the re-split below can
    // be checked against them rather than against a rounded result.
    static constexpr double siblingMeasuredRateOneHz = 0.513;
    static constexpr double siblingMeasuredRateTwoHz = 0.863;

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
    // C28/C25 couple the two reconstructed wet lines into 22 kOhm bleeds.
    // When the series JFET is on, IC6's 47 kOhm wet input loads that node too.
    [[nodiscard]] static float wetOutputCouplingCornerHz(
        bool wetConnected) noexcept;

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
        float inputCouplingG { 0.001f };    // C44 / R120, wet path only
        float passiveG { 0.1f };            // R122 / C52, ahead of the line
        // Ideal-source approximation for R118/R119, R117 and C45.  The name
        // keeps the still-open MN3009 output loading out of the claim.
        float idealSourceTapPoleG { 0.1f };
        float wetOutputCouplingMutedG { 0.001f };
        float wetOutputCouplingConnectedG { 0.001f };
        BiquadCoefficients antiAliasFirst {};
        BiquadCoefficients antiAliasSecond {};
        BiquadCoefficients reconstructionFirst {};
        BiquadCoefficients reconstructionSecond {};
    };
    [[nodiscard]] static SupportChain supportChainFor(float sampleRate) noexcept;

    [[nodiscard]] double getLfoPhase() const noexcept { return lfoPhase_; }

private:
    friend struct YouKnow106TestAccess;

    // OQ-03 keeps the compatibility hiss and the still-unknown mechanisms as
    // distinct components.  Every number in this profile is voiced/unknown,
    // not a JUNO-106 measurement.  The optional amplitudes therefore default
    // to zero; `noiseScale` in process() remains the one master applied to all
    // of them when future evidence supplies defensible values.
    struct OptionalNoiseComponents
    {
        float commonRandomAmplitude { 0.0f };
        // Desired stereo correlation of the synthetic common layer.  The
        // default is a voiced placeholder and is inaudible while its amplitude
        // is zero.
        float commonRandomCorrelation { 1.0f };
        float humAmplitude { 0.0f };
        // A voiced placeholder for deterministic tests only: mains frequency
        // is market/unit dependent and has not been measured at this node.
        float humFrequencyHz { 50.0f };
        float clockSpurAmplitude { 0.0f };
        // The spur follows each modulated BBD clock.  Which harmonic dominates
        // is unknown, so unity is only a disabled, voiced placeholder.
        float clockSpurHarmonic { 1.0f };
    };

    struct StereoNoiseSample
    {
        float lineA { 0.0f };
        float lineB { 0.0f };
    };

    // Pure deterministic component steps.  They are private implementation
    // details, with a narrow friend seam for regression fixtures.
    [[nodiscard]] static StereoNoiseSample correlatedRandomStep(
        std::uint32_t& commonState, std::uint32_t& orthogonalState,
        float correlation) noexcept;
    [[nodiscard]] static float deterministicToneStep(
        double& phase, float frequencyHz, float sampleRate) noexcept;

    // The two parasitic mechanisms inside the BBD, kept as separate pure
    // steps so the circuit suite can check them against the part's distortion
    // and frequency-response figures without the support filters obscuring the
    // result.
    [[nodiscard]] static float bbdTransfer(float input) noexcept;
    static float transferLossStep(float& state, float input,
                                  float clockHz = 26000.0f) noexcept;

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
        // The wet input has its own coupling high-pass and five low-pass
        // poles: two Sallen-Key sections and one passive pole. The
        // output has the tap-summing pole followed by two Sallen-Key sections.
        //
        // When the engine oversamples, the line's zero-order-hold images at the
        // clock rate land above the host band and the decimators remove them;
        // with oversampling off the clock exceeds the host Nyquist and the
        // images fold, with only the output chain to soften them. That is a
        // documented cost of the low-quality setting, not of the model -- and
        // five output poles soften it considerably better than the one this
        // replaced.
        float inputCouplingState { 0.0f };
        float antiAliasState { 0.0f };
        BiquadState antiAliasFirst {};
        BiquadState antiAliasSecond {};
        float tapSumState { 0.0f };
        BiquadState reconstructionFirst {};
        BiquadState reconstructionSecond {};
        float outputCouplingState { 0.0f };
        float transferState { 0.0f };
        std::uint32_t noiseState { 0x9e3779b9u };

        void reset(std::uint32_t seed) noexcept;
        void resetAudioRateSupport() noexcept;
        float process(float input, float clockHz, float sampleRate,
                      const SupportChain& support, float outputCouplingG,
                      float noiseScale) noexcept;
    };

    Line lineA_ {};
    Line lineB_ {};
    float sampleRate_ { 48000.0f };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    // Double precision keeps the sub-hertz free-running phase smooth at the
    // engine's highest internal rates and over long sessions.
    double lfoPhase_ { 0.0 };
    SupportChain support_ {};
    float wetGain_ { 0.0f };
    float rateHz_ { 0.0f };
    float centreDelay_ { 0.0032f };
    float sweep_ { 0.0f };
    OptionalNoiseComponents optionalNoise_ {};
    std::uint32_t commonNoiseState_ { 0xd1b54a35u };
    std::uint32_t orthogonalNoiseState_ { 0x94d049bbu };
    double humPhase_ { 0.0 };
    double clockSpurPhaseA_ { 0.0 };
    double clockSpurPhaseB_ { 0.0 };
    // Whether the glided settings have a starting point yet.
    bool primed_ { false };
};

} // namespace youknow106
