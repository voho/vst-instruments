// Circuit-reference suite.
//
// Evidence-backed checks compare the realtime model against an independent
// numerical solve, closed-form result, firmware vector or service-document
// figure. Explicitly named compatibility profiles also carry broad safety and
// monotonicity regressions; those are product invariants, not hardware claims.

#include "DSP/YouKnow106Chorus.h"
#include "DSP/YouKnow106Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace youknow106
{
// Narrow seam into the private blocks. Not part of the plug-in API.
struct YouKnow106TestAccess
{
    using Cascade = YouKnow106Engine::OtaCascade;

    static constexpr float headroom() noexcept
    {
        return YouKnow106Engine::otaHeadroomVolts;
    }

    static constexpr float feedbackHeadroom() noexcept
    {
        return YouKnow106Engine::VoicedResonanceCompatibilityProfile::
            loopHeadroomVolts;
    }

    static std::vector<float> renderCascade(const std::vector<float>& input,
                                            float g, float feedback)
    {
        Cascade cascade;
        cascade.reset();
        std::vector<float> output(input.size());
        for (std::size_t index = 0; index < input.size(); ++index)
            output[index] = cascade.process(input[index], g, feedback);
        return output;
    }

    static std::array<float, 4> cascadeDerivativeAfterRetime(
        const std::array<float, 4>& voltage,
        const std::array<float, 4>& derivative,
        float previousG, float nextG) noexcept
    {
        Cascade cascade;
        cascade.voltage = voltage;
        for (std::size_t stage = 0; stage < cascade.state.size(); ++stage)
            cascade.state[stage] = voltage[stage]
                                 + previousG * derivative[stage];
        cascade.retime(previousG, nextG);

        std::array<float, 4> realised {};
        for (std::size_t stage = 0; stage < realised.size(); ++stage)
            realised[stage] = (cascade.state[stage] - cascade.voltage[stage])
                            / nextG;
        return realised;
    }

    // The decimation kernel as built, so the suite can measure the transfer
    // the last stage actually applies rather than trusting a design formula.
    static std::vector<float> halfbandKernel()
    {
        YouKnow106Engine engine;
        engine.buildHalfbandKernel();
        return { engine.halfbandKernel_.begin(), engine.halfbandKernel_.end() };
    }

    // The IR3109's own internal input divider and the differential pair's
    // linear span it implies. Private on the engine because nothing outside it
    // needs them; the suite needs them to check the drive budget.
    static constexpr float stageAttenuation() noexcept
    {
        return YouKnow106Engine::stageAttenuation;
    }

    static constexpr float otaHeadroomVolts() noexcept
    {
        return YouKnow106Engine::otaHeadroomVolts;
    }

    static float bbdTransfer(float input) noexcept
    {
        return Chorus::bbdTransfer(input);
    }

    static float transferLossStep(float& state, float input) noexcept
    {
        return Chorus::transferLossStep(state, input);
    }

    static std::array<float, 2> correlatedChorusNoiseStep(
        std::uint32_t& commonState, std::uint32_t& orthogonalState,
        float correlation) noexcept
    {
        const auto sample = Chorus::correlatedRandomStep(
            commonState, orthogonalState, correlation);
        return { sample.lineA, sample.lineB };
    }

    static float chorusToneStep(double& phase, float frequencyHz,
                                float sampleRate) noexcept
    {
        return Chorus::deterministicToneStep(phase, frequencyHz, sampleRate);
    }

    static void configureOptionalChorusNoise(
        Chorus& chorus, float commonAmplitude, float correlation,
        float humAmplitude, float humFrequencyHz,
        float clockSpurAmplitude, float clockSpurHarmonic) noexcept
    {
        chorus.optionalNoise_.commonRandomAmplitude = commonAmplitude;
        chorus.optionalNoise_.commonRandomCorrelation = correlation;
        chorus.optionalNoise_.humAmplitude = humAmplitude;
        chorus.optionalNoise_.humFrequencyHz = humFrequencyHz;
        chorus.optionalNoise_.clockSpurAmplitude = clockSpurAmplitude;
        chorus.optionalNoise_.clockSpurHarmonic = clockSpurHarmonic;
    }

    static float chorusWetGain(const Chorus& chorus) noexcept
    {
        return chorus.wetGain_;
    }

    struct ChorusPhysicalState
    {
        std::array<float, Chorus::cellPairs> cellsA {};
        int writeIndexA { 0 };
        double clockPhaseA { 0.0 };
        float heldA { 0.0f };
        float transferStateA { 0.0f };
        std::uint32_t lineNoiseA { 0u };
        double lfoPhase { 0.0 };
        float wetGain { 0.0f };
        std::uint32_t commonNoise { 0u };
        std::uint32_t orthogonalNoise { 0u };
        double humPhase { 0.0 };
        double clockSpurPhaseA { 0.0 };
        bool primed { false };

        bool operator==(const ChorusPhysicalState&) const = default;
    };

    static ChorusPhysicalState chorusPhysicalState(
        const Chorus& chorus) noexcept
    {
        return { chorus.lineA_.cells,
                 chorus.lineA_.writeIndex,
                 chorus.lineA_.clockPhase,
                 chorus.lineA_.held,
                 chorus.lineA_.transferState,
                 chorus.lineA_.noiseState,
                 chorus.lfoPhase_,
                 chorus.wetGain_,
                 chorus.commonNoiseState_,
                 chorus.orthogonalNoiseState_,
                 chorus.humPhase_,
                 chorus.clockSpurPhaseA_,
                 chorus.primed_ };
    }

    static bool chorusAudioRateSupportIsClear(
        const Chorus& chorus) noexcept
    {
        const auto clear = [](const Chorus::Line& line) {
            return line.previousInput == 0.0f
                && line.inputCouplingState == 0.0f
                && line.antiAliasState == 0.0f
                && line.antiAliasFirst.s1 == 0.0f
                && line.antiAliasFirst.s2 == 0.0f
                && line.antiAliasSecond.s1 == 0.0f
                && line.antiAliasSecond.s2 == 0.0f
                && line.tapSumState == 0.0f
                && line.reconstructionFirst.s1 == 0.0f
                && line.reconstructionFirst.s2 == 0.0f
                && line.reconstructionSecond.s1 == 0.0f
                && line.reconstructionSecond.s2 == 0.0f
                && line.outputCouplingState == 0.0f;
        };
        return clear(chorus.lineA_) && clear(chorus.lineB_);
    }

    static std::vector<float> renderOutputCoupling(
        const std::vector<float>& input, double sampleRate)
    {
        YouKnow106Engine::HighPass coupling;
        coupling.reset();
        const float g = std::tan(
            static_cast<float>(3.14159265358979323846)
            * YouKnow106Engine::outputCouplingCornerHz()
            / static_cast<float>(sampleRate));
        std::vector<float> output(input.size());
        for (std::size_t index = 0; index < input.size(); ++index)
            output[index] = coupling.process(
                input[index], g, 0.0f,
                YouKnow106Engine::outputCouplingHighGain());
        return output;
    }

    static std::vector<float> renderLoadedOutputCoupling(
        const std::vector<float>& input, double sampleRate,
        float volumePosition)
    {
        YouKnow106Engine::HighPass coupling;
        coupling.reset();
        const float g = std::tan(
            static_cast<float>(3.14159265358979323846)
            * YouKnow106Engine::outputCouplingCornerHz(volumePosition)
            / static_cast<float>(sampleRate));
        const float gain =
            YouKnow106Engine::outputCouplingHighGain(volumePosition);
        std::vector<float> output(input.size());
        for (std::size_t index = 0; index < input.size(); ++index)
            output[index] = coupling.process(input[index], g, 0.0f, gain);
        return output;
    }

    static std::vector<float> renderVoiceBusCoupling(
        const std::vector<float>& input, double sampleRate)
    {
        YouKnow106Engine::HighPass coupling;
        coupling.reset();
        const float g = std::tan(
            static_cast<float>(3.14159265358979323846)
            * YouKnow106Engine::voiceBusCouplingCornerHz()
            / static_cast<float>(sampleRate));
        std::vector<float> output(input.size());
        for (std::size_t index = 0; index < input.size(); ++index)
            output[index] = coupling.process(input[index], g, 0.0f, 1.0f);
        return output;
    }

    static std::vector<float> renderCommonVcaInputCoupling(
        const std::vector<float>& input, double sampleRate)
    {
        YouKnow106Engine::HighPass coupling;
        coupling.reset();
        const float g = std::tan(
            static_cast<float>(3.14159265358979323846)
            * YouKnow106Engine::commonVcaInputCouplingCornerHz()
            / static_cast<float>(sampleRate));
        std::vector<float> output(input.size());
        for (std::size_t index = 0; index < input.size(); ++index)
            output[index] = coupling.process(input[index], g, 0.0f, 1.0f);
        return output;
    }

    static void setChorusWetGain(Chorus& chorus, float gain) noexcept
    {
        chorus.wetGain_ = gain;
    }

    static std::uint16_t attackLevelAfterRetrigger(std::uint16_t level,
                                                   std::uint16_t increment) noexcept
    {
        YouKnow106Engine::Envelope envelope;
        envelope.level = level;
        envelope.value = YouKnow106Engine::envelopeDacFraction(level);
        envelope.noteOn();
        envelope.tick(increment, 0xffffu, 0u, 0xffffu);
        return envelope.level;
    }
};
} // namespace youknow106

namespace
{
using namespace youknow106;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectNear(double actual, double expected, double tolerance,
                const std::string& message)
{
    if (!(std::abs(actual - expected) <= tolerance))
    {
        ++failures;
        std::cerr << "FAIL: " << message << " (got " << actual << ", expected "
                  << expected << " +/- " << tolerance << ")\n";
    }
}

constexpr double pi = 3.14159265358979323846;

// --------------------------------------------------------------------------
// Independent reference: the four transconductor stages integrated with a
// fourth-order Runge-Kutta step at 64x the model's rate, straight from
//     C dVn/dt = Ig tanh((V(n-1) - Vn) / H),  V0 = input - k fb(V4)
// with fb the compatibility profile's declared nonlinear return. This is an
// independent numerical check of the implementation's ODE, not evidence that
// the profile constants are a measured original-unit transfer.
// --------------------------------------------------------------------------
std::vector<double> referenceCascade(const std::vector<double>& input, double sampleRate,
                                     double cutoffHz, double feedback,
                                     int oversample = 16)
{
    const double headroom = YouKnow106TestAccess::headroom();
    const double loopHeadroom = YouKnow106TestAccess::feedbackHeadroom();
    const double omega = 2.0 * pi * cutoffHz;
    const double step = 1.0 / (sampleRate * oversample);

    std::array<double, 4> voltage {};
    std::vector<double> output(input.size());

    const auto derivative = [&](const std::array<double, 4>& state, double drive) {
        std::array<double, 4> slope {};
        double previous = drive
            - feedback * loopHeadroom * std::tanh(state[3] / loopHeadroom);
        for (int stage = 0; stage < 4; ++stage)
        {
            slope[static_cast<std::size_t>(stage)] =
                omega * headroom
                * std::tanh((previous - state[static_cast<std::size_t>(stage)]) / headroom);
            previous = state[static_cast<std::size_t>(stage)];
        }
        return slope;
    };

    double previousInput = 0.0;
    for (std::size_t index = 0; index < input.size(); ++index)
    {
        const double target = input[index];
        for (int sub = 0; sub < oversample; ++sub)
        {
            const double a = previousInput
                + (target - previousInput) * (sub / static_cast<double>(oversample));
            const double b = previousInput
                + (target - previousInput) * ((sub + 0.5) / oversample);
            const double c = previousInput
                + (target - previousInput) * ((sub + 1.0) / oversample);

            const auto k1 = derivative(voltage, a);
            std::array<double, 4> temp {};
            for (int n = 0; n < 4; ++n)
                temp[static_cast<std::size_t>(n)] = voltage[static_cast<std::size_t>(n)]
                    + 0.5 * step * k1[static_cast<std::size_t>(n)];
            const auto k2 = derivative(temp, b);
            for (int n = 0; n < 4; ++n)
                temp[static_cast<std::size_t>(n)] = voltage[static_cast<std::size_t>(n)]
                    + 0.5 * step * k2[static_cast<std::size_t>(n)];
            const auto k3 = derivative(temp, b);
            for (int n = 0; n < 4; ++n)
                temp[static_cast<std::size_t>(n)] = voltage[static_cast<std::size_t>(n)]
                    + step * k3[static_cast<std::size_t>(n)];
            const auto k4 = derivative(temp, c);

            for (int n = 0; n < 4; ++n)
                voltage[static_cast<std::size_t>(n)] +=
                    (step / 6.0) * (k1[static_cast<std::size_t>(n)]
                                    + 2.0 * k2[static_cast<std::size_t>(n)]
                                    + 2.0 * k3[static_cast<std::size_t>(n)]
                                    + k4[static_cast<std::size_t>(n)]);
        }
        previousInput = target;
        output[index] = voltage[3];
    }
    return output;
}

double steadyStatePeak(const std::vector<double>& signal)
{
    double peak = 0.0;
    for (std::size_t index = signal.size() * 3 / 4; index < signal.size(); ++index)
        peak = std::max(peak, std::abs(signal[index]));
    return peak;
}

double steadyStatePeak(const std::vector<float>& signal)
{
    double peak = 0.0;
    for (std::size_t index = signal.size() * 3 / 4; index < signal.size(); ++index)
        peak = std::max(peak, static_cast<double>(std::abs(signal[index])));
    return peak;
}

// --------------------------------------------------------------------------

void testCascadeAgainstReferenceSolve()
{
    constexpr double sampleRate = 192000.0;

    for (double cutoff : { 120.0, 900.0, 4200.0 })
    {
        for (double feedback : { 0.0, 2.0, 3.0, 3.8 })
        {
            // A resonant cascade takes about Q cycles to settle, and Q here is
            // 1/(4 - k). Measuring before it has settled would compare a
            // transient against a steady-state result.
            const int length = static_cast<int>(std::min(
                65536.0, std::max(8192.0, 90.0 * sampleRate / cutoff)));
            // Small enough that the differential pairs stay linear, so the
            // closed-form 1/(4 - k) result at cutoff also applies.
            const double amplitude = 1.0e-4;
            std::vector<double> referenceInput(static_cast<std::size_t>(length));
            std::vector<float> modelInput(static_cast<std::size_t>(length));
            for (int index = 0; index < length; ++index)
            {
                const double value = amplitude
                    * std::sin(2.0 * pi * cutoff * index / sampleRate);
                referenceInput[static_cast<std::size_t>(index)] = value;
                modelInput[static_cast<std::size_t>(index)] = static_cast<float>(value);
            }

            const auto reference =
                referenceCascade(referenceInput, sampleRate, cutoff, feedback);
            const float g = static_cast<float>(std::tan(pi * cutoff / sampleRate));
            const auto model = YouKnow106TestAccess::renderCascade(
                modelInput, g, static_cast<float>(feedback));

            const double referenceGain = steadyStatePeak(reference) / amplitude;
            const double modelGain = steadyStatePeak(model) / amplitude;
            const double theory = 1.0 / (4.0 - feedback);

            const std::string label = "cascade fc=" + std::to_string(cutoff)
                                    + " k=" + std::to_string(feedback);
            expectNear(20.0 * std::log10(referenceGain / theory), 0.0, 0.6,
                       label + ": reference solve does not meet the analytic result");
            expectNear(20.0 * std::log10(modelGain / referenceGain), 0.0, 0.6,
                       label + ": model disagrees with the reference solve");
        }
    }

    // Changing HQ changes dt, not a voice card's capacitor charge or physical
    // derivative. The trapezoidal carry must be re-expressed on the new grid
    // rather than reused as though its old g had the new units.
    constexpr std::array voltage { 0.25f, -0.5f, 0.75f, -1.0f };
    constexpr std::array derivative { 0.125f, -0.25f, 0.5f, -0.75f };
    const auto retimed = YouKnow106TestAccess::cascadeDerivativeAfterRetime(
        voltage, derivative, 0.03125f, 0.125f);
    for (std::size_t stage = 0; stage < derivative.size(); ++stage)
        expectNear(retimed[stage], derivative[stage], 2.0e-6,
                   "HQ retiming changed OTA stage derivative "
                       + std::to_string(stage));
}

void testCascadeOscillationThreshold()
{
    constexpr double sampleRate = 192000.0;
    constexpr double cutoff = 500.0;
    const float g = static_cast<float>(std::tan(pi * cutoff / sampleRate));

    const auto ring = [&](float feedback) {
        std::vector<float> input(24000, 0.0f);
        input[0] = 0.5f;
        const auto output = YouKnow106TestAccess::renderCascade(input, g, feedback);
        double peak = 0.0;
        for (std::size_t index = output.size() - 4000; index < output.size(); ++index)
            peak = std::max(peak, static_cast<double>(std::abs(output[index])));
        return peak;
    };

    expect(ring(3.6f) < 1.0e-4,
           "four-pole cascade sustains below its oscillation threshold");
    expect(ring(4.3f) > 1.0e-3,
           "four-pole cascade does not oscillate above its threshold");

    // A cascade driven far past the threshold must still settle to a bounded
    // limit cycle rather than running away.
    const double hard = ring(8.0f);
    expect(std::isfinite(hard) && hard < 40.0,
           "cascade is unbounded when driven far past its threshold");
}

void testCascadeSurvivesAdversarialControl()
{
    // Sweeping the control voltage violently across the whole range at audio
    // rate is not musical, but automation can produce it and the implicit solve
    // must not diverge or emit a non-finite sample.
    std::vector<float> input(20000);
    for (std::size_t index = 0; index < input.size(); ++index)
        input[index] = 3.0f * std::sin(0.31f * static_cast<float>(index));

    YouKnow106TestAccess::Cascade cascade;
    cascade.reset();
    bool finite = true;
    for (std::size_t index = 0; index < input.size(); ++index)
    {
        const float g = (index % 2 == 0) ? 0.0004f : 30.0f;
        const float k = (index % 3 == 0) ? 0.0f : 4.4f;
        const float value = cascade.process(input[index], g, k);
        finite = finite && std::isfinite(value) && std::abs(value) < 1.0e4f;
    }
    expect(finite, "cascade diverged under an adversarial control sweep");
}

void testNoteTimerLaw()
{
    // One reference divided by an integer: pitch is quantised, and how coarsely
    // depends on how large that integer is.
    struct Case { DcoRange range; double clock; };
    const Case cases[] = { { DcoRange::Sixteen, 1.0e6 },
                           { DcoRange::Eight, 2.0e6 },
                           { DcoRange::Four, 4.0e6 } };

    for (const auto& item : cases)
    {
        expectNear(YouKnow106Engine::rangeClockHz(item.range), item.clock, 1.0,
                   "range divider clock");

        // The instrument's own keyboard is five octaves from C2, and the
        // 16-bit counters cannot reach further down than that at 16' anyway.
        // The range switch transposes by whole octaves, so the count is the
        // same in every range and the tuning error is too.
        const double octaves = std::log2(item.clock / 2.0e6);
        double worstCents = 0.0;
        for (int note = 36; note <= 96; ++note)
        {
            const double wanted = 440.0 * std::pow(2.0, (note - 69) / 12.0);
            const auto divider = YouKnow106Engine::dcoDivider(wanted);
            const double produced =
                YouKnow106Engine::dcoQuantisedFrequency(divider, item.range);
            const double sounding = wanted * std::pow(2.0, octaves);
            expectNear(static_cast<double>(divider),
                       std::floor(2.0e6 / wanted + 0.5), 0.5,
                       "programmed divider is not the nearest integer");
            worstCents = std::max(worstCents,
                                  std::abs(1200.0 * std::log2(produced / sounding)));
        }
        // Quantisation grows with pitch; even at the top of the range it stays
        // well inside a cent at these clocks.
        expect(worstCents < 1.0,
               "note timer quantisation exceeds one cent across the keyboard");
    }

    // The counters are 16 bits wide, so the lowest note the 16' range can
    // produce is its clock divided by 65535 -- about 15.3 Hz. Asking for
    // anything lower does not transpose further down, it simply stops.
    const auto floored = YouKnow106Engine::dcoDivider(8.0);
    expectNear(YouKnow106Engine::dcoQuantisedFrequency(floored, DcoRange::Sixteen),
               15.26, 0.02, "the 16' range does not floor at the counter's width");
    expect(YouKnow106Engine::dcoQuantisedFrequency(
               YouKnow106Engine::dcoDivider(4.0), DcoRange::Sixteen)
           == YouKnow106Engine::dcoQuantisedFrequency(floored, DcoRange::Sixteen),
           "asking for an impossible pitch keeps transposing downwards");

    // Every voice divides the same reference, so two voices asked for the same
    // pitch get exactly the same integer: no spread at all.
    expect(YouKnow106Engine::dcoDivider(261.63)
           == YouKnow106Engine::dcoDivider(261.63),
           "note timer is not deterministic");
}

void testCutoffControlLaw()
{
    // The instrument's own service anchor: converter code 6272 must self-
    // oscillate at 248 Hz, and code 6272 + 2286 two octaves above that.
    expectNear(YouKnow106Engine::vcfCutoffHz(6272.0f), 248.0, 1.0,
               "cutoff law misses the service calibration anchor");
    expectNear(YouKnow106Engine::vcfCutoffHz(6272.0f + 2286.0f), 992.0, 4.0,
               "cutoff law misses the second calibration anchor");

    // One octave is 1143 counts throughout the validated, uncapped range.
    for (float counts : { 0.0f, 2000.0f, 6000.0f, 9000.0f })
        expectNear(YouKnow106Engine::vcfCutoffHz(counts + 1143.0f)
                       / YouKnow106Engine::vcfCutoffHz(counts),
                   2.0, 0.01, "cutoff law is not 1143 counts per octave");

    expectNear(YouKnow106Engine::vcfCutoffHz(0.0f), 5.53, 0.01,
               "cutoff law base frequency");

    // The panel reads as a 0..127 byte driving the converter 128 counts at a
    // time, so the whole travel is 16256 counts.
    expectNear(YouKnow106Engine::vcfPanelCounts(1.0f), 16256.0, 0.5,
               "cutoff panel travel");
    expectNear(YouKnow106Engine::vcfPanelCounts(0.0f), 0.0, 0.5,
               "cutoff panel travel starts at zero");
    expect(YouKnow106Engine::vcfPanelCounts(0.5f)
               == YouKnow106Engine::vcfPanelCounts(0.503f),
           "cutoff panel position is not quantised to the converter's byte");

    // OQ-18 does not justify the former 24 kHz tanh knee or 52.2 kHz
    // asymptote. The default product policy is the unchanged exponential law
    // followed by a plainly named 50 kHz numerical safety cap.
    for (float counts : { 0.0f, 6272.0f, 9000.0f, 12000.0f, 13716.0f })
    {
        const double exponential = YouKnow106Engine::vcfBaseFrequencyHz
            * std::exp2(counts / YouKnow106Engine::vcfCountsPerOctave);
        expectNear(YouKnow106Engine::vcfCutoffHz(counts), exponential,
                   std::max(1.0e-4, exponential * 2.0e-6),
                   "50 kHz policy altered the validated exponential range");
    }

    float previousCutoff = YouKnow106Engine::vcfCutoffHz(0.0f);
    for (int counts = 1; counts <= 20000; ++counts)
    {
        const float cutoff = YouKnow106Engine::vcfCutoffHz(static_cast<float>(counts));
        expect(std::isfinite(cutoff) && cutoff >= previousCutoff,
               "default cutoff policy is not finite and monotone");
        expect(cutoff <= 50000.0f,
               "default cutoff policy exceeds its named 50 kHz safety cap");
        previousCutoff = cutoff;
    }
    expectNear(YouKnow106Engine::vcfCutoffHz(20000.0f), 50000.0, 1.0e-3,
               "default cutoff policy never reaches its 50 kHz safety cap");
    expectNear(YouKnow106Engine::vcfEffectiveCutoffHz(20000.0f, 8.0f),
               50000.0, 1.0e-3,
               "resonance trim escaped the final 50 kHz cutoff safety cap");

    // The transconductor's control-current saturation has to be invisible
    // through the musical range and only bend the top: what it replaced was a
    // single pole that pulled a 5 kHz cutoff 48 cents flat and a 16 kHz one by
    // 143. Below 2.7 kHz the correction is under five cents.
    for (float counts = 0.0f; counts <= 10100.0f; counts += 100.0f)
    {
        const float effective = YouKnow106Engine::vcfEffectiveCutoffHz(counts, 0.0f);
        const float law = YouKnow106Engine::vcfCutoffHz(counts);
        const double cents = 1200.0 * std::log2(effective / law);
        expect(cents <= 0.0 && cents > -5.0,
               "control-current saturation is not transparent below 2.7 kHz: "
                   + std::to_string(cents) + " cents at " + std::to_string(counts)
                   + " counts");
    }
    // And the saturation must never lift a cutoff, at any code.
    for (float counts = 0.0f; counts <= 20000.0f; counts += 50.0f)
        expect(YouKnow106Engine::vcfEffectiveCutoffHz(counts, 0.0f)
                   <= YouKnow106Engine::vcfCutoffHz(counts) + 1.0e-3f,
               "control-current saturation raised a cutoff above the anti-log law");

    // A measured code-to-frequency table for a real voice card, gain
    // calibrated so DAC 1568 reads the service manual's own 248 Hz anchor.
    // Third-party and not independently verified, so it is a comparison
    // fixture rather than a hardware assertion -- but the shipping law has to
    // stay inside a musically meaningful distance of it. The revision this
    // replaced was 143 cents flat at DAC 3328.
    struct MeasuredCutoff { float dacCode; double hertz; };
    constexpr std::array<MeasuredCutoff, 8> measured {{
        { 1024.0f, 67.2 },   { 1568.0f, 248.0 },   { 2560.0f, 2725.0 },
        { 2816.0f, 5048.0 }, { 3072.0f, 9297.0 },  { 3328.0f, 16779.0 },
        { 3584.0f, 27876.0 }, { 4064.0f, 50792.0 }
    }};
    for (const auto& point : measured)
    {
        const float counts = point.dacCode * YouKnow106Engine::vcfDacCountStep
                           + YouKnow106Engine::vcfConverterCarryCounts(
                                 point.dacCode * YouKnow106Engine::vcfDacCountStep);
        const double cents = 1200.0 * std::log2(
            YouKnow106Engine::vcfEffectiveCutoffHz(counts, 0.0f) / point.hertz);
        expect(std::abs(cents) < 45.0,
               "cutoff law is " + std::to_string(cents)
                   + " cents from the measured card at DAC "
                   + std::to_string(static_cast<int>(point.dacCode)));
    }

    // The R-2R ladder's major carry. A slow sweep crossing mid-scale steps by
    // roughly 23 cents, and the two smaller boundary errors sit either side of
    // it. An ideal ladder has none of this, so it is scaled by Unit Character
    // in the converter write rather than living in the law.
    constexpr float perCent = YouKnow106Engine::vcfCountsPerOctave / 1200.0f;
    expectNear(YouKnow106Engine::vcfConverterCarryCounts(0.0f), 0.0, 1.0e-6,
               "the converter carries an offset below its first bit boundary");
    expectNear(YouKnow106Engine::vcfConverterCarryCounts(8192.0f)
                   - YouKnow106Engine::vcfConverterCarryCounts(8188.0f),
               23.31 * perCent, 1.0e-4,
               "the major carry at DAC 2048 is not the measured 23.31 cents");
    expectNear(YouKnow106Engine::vcfConverterCarryCounts(4096.0f)
                   - YouKnow106Engine::vcfConverterCarryCounts(4092.0f),
               -4.64 * perCent, 1.0e-4,
               "the carry at DAC 1024 is not the measured -4.64 cents");
    expectNear(YouKnow106Engine::vcfConverterCarryCounts(12288.0f)
                   - YouKnow106Engine::vcfConverterCarryCounts(12284.0f),
               -4.48 * perCent, 1.0e-4,
               "the carry at DAC 3072 is not the measured -4.48 cents");
}

void testStoredControlDigitalVectors()
{
    // This firmware-verified byte -> work-word -> DAC path is independent of
    // whichever replaceable analogue resonance profile consumes the voltage.
    struct Vector
    {
        int byte;
        std::uint16_t alignedWord;
        std::uint16_t dacCode;
    };
    constexpr std::array<Vector, 3> vectors {{
        { 0,   0x0000u, 0x0000u },
        { 64,  0x2000u, 0x0800u },
        { 127, 0x3f80u, 0x0fe0u }
    }};
    for (const auto& vector : vectors)
    {
        const float position = static_cast<float>(vector.byte) / 127.0f;
        expect(YouKnow106Engine::storedControlAlignedWord(position)
                   == vector.alignedWord,
               "stored control has the wrong aligned work word at byte "
                   + std::to_string(vector.byte));
        expect(YouKnow106Engine::storedControlDacCode(position) == vector.dacCode,
               "stored control has the wrong physical DAC code at byte "
                   + std::to_string(vector.byte));
    }
}

void testVoicedResonanceCompatibilityProfile()
{
    using Profile = YouKnow106Engine::VoicedResonanceCompatibilityProfile;

    // These deliberately avoid treating today's voiced coefficients as
    // hardware anchors. A measured profile may replace every analogue number
    // while retaining this minimal realtime safety/shape contract.
    float previousLoopGain = -1.0f;
    float previousCompensation = -1.0f;
    float previousTrim = -1.0f;
    for (int byte = 0; byte <= 127; ++byte)
    {
        const float panel = static_cast<float>(byte) / 127.0f;
        const float loopGain = Profile::loopGain(panel);
        const float compensation = Profile::inputCompensation(loopGain);
        const float trim = Profile::frequencyTrim(loopGain);

        expect(std::isfinite(loopGain) && loopGain >= previousLoopGain,
               "voiced resonance loop-gain profile is not finite and monotone");
        expect(std::isfinite(compensation)
                   && compensation >= previousCompensation,
               "voiced resonance compensation is not finite and monotone");
        expect(std::isfinite(trim) && trim >= previousTrim,
               "voiced resonance frequency correction is not finite and monotone");

        previousLoopGain = loopGain;
        previousCompensation = compensation;
        previousTrim = trim;
    }
    expect(previousLoopGain > Profile::loopGain(0.0f),
           "voiced resonance loop-gain profile does not respond to its control");
    expect(previousCompensation > Profile::inputCompensation(0.0f),
           "voiced resonance compensation does not respond to loop gain");
    expect(previousTrim > Profile::frequencyTrim(0.0f),
           "voiced resonance frequency correction does not respond to loop gain");
}

void testEnvelopeAndAmplifierLaws()
{
    // Hash-matched B-2 coefficient vectors. The compact generators in the
    // engine must reproduce these values without embedding a proprietary table.
    struct EnvelopeVector
    {
        int code;
        std::uint16_t attack;
        std::uint16_t fall;
        int attackPasses;
        int releasePasses;
    };
    constexpr std::array<EnvelopeVector, 3> vectors {{
        { 0,   16384u,  4096u,   1,    4 },
        { 64,    127u, 65276u, 129,  984 },
        { 127,    21u, 65524u, 781, 6083 }
    }};
    for (const auto& vector : vectors)
    {
        const float position = static_cast<float>(vector.code) / 127.0f;
        expect(YouKnow106Engine::envelopeAttackIncrement(position) == vector.attack,
               "B-2 attack coefficient vector mismatch at code "
                   + std::to_string(vector.code));
        expect(YouKnow106Engine::envelopeDecayReleaseMultiplier(position) == vector.fall,
               "B-2 decay/release coefficient vector mismatch at code "
                   + std::to_string(vector.code));

        std::uint16_t release = YouKnow106Engine::envelopePeak;
        int passes = 0;
        while (release != 0u && passes <= vector.releasePasses)
        {
            release = YouKnow106Engine::envelopeReleaseLevel(release, vector.fall);
            ++passes;
        }
        expect(passes == vector.releasePasses && release == 0u,
               "B-2 release-to-zero pass count mismatch at code "
                   + std::to_string(vector.code));
        expectNear(YouKnow106Engine::envelopeAttackSeconds(position),
                   vector.attackPasses * 0.0042, 1.0e-5,
                   "B-2 attack duration mismatch at code "
                       + std::to_string(vector.code));
        expectNear(YouKnow106Engine::envelopeReleaseSeconds(position),
                   vector.releasePasses * 0.0042, 1.0e-4,
                   "B-2 release duration mismatch at code "
                       + std::to_string(vector.code));
    }

    // The multiply helper intentionally omits the low-byte x low-byte term.
    // For this vector the complete 16x16 product would yield 0x0626, while
    // the B-2 helper yields 0x0625.
    expect(YouKnow106Engine::envelopeReleaseLevel(0x1234u, 0x5678u) == 0x0625u,
           "decay helper restored the intentionally omitted low-low product");
    expect(YouKnow106Engine::envelopeDecayLevel(
               0x1334u, 0x0100u, 0x5678u) == 0x0725u,
           "decay is not sustain plus the exact truncated distance product");
    expect(YouKnow106Engine::envelopeAttackLevel(0x3ff0u, 0x0020u) == 0x3fffu,
           "integer attack does not saturate at 14 bits");
    expectNear(YouKnow106Engine::envelopeDacFraction(0x0003u), 0.0, 0.0,
               "envelope low recurrence bits leaked into the physical DAC");
    expectNear(YouKnow106Engine::envelopeDacFraction(0x0004u), 1.0 / 4095.0,
               1.0e-9, "envelope DAC does not discard exactly two low bits");
    expectNear(YouKnow106Engine::envelopeDacFraction(0x3fffu), 1.0, 0.0,
               "envelope DAC does not reach full scale at the 14-bit peak");
    expect(YouKnow106TestAccess::attackLevelAfterRetrigger(0x1800u, 0x007fu)
               == 0x187fu,
           "retrigger clears the live envelope accumulator before attack");

    // Sustain is the stored byte shifted by seven, including the exact midpoint.
    expect(YouKnow106Engine::storedControlAlignedWord(0.0f) == 0u
           && YouKnow106Engine::storedControlAlignedWord(64.0f / 127.0f) == 0x2000u
           && YouKnow106Engine::storedControlAlignedWord(1.0f) == 0x3f80u,
           "sustain byte does not map to 0 / 0x2000 / 0x3f80");

    // Decay's UI convention remains time to -20 dB, but the reported value is
    // now obtained by iterating the exact integer recurrence.
    expectNear(YouKnow106Engine::envelopeDecaySeconds(0.0f), 0.0042, 1.0e-6,
               "fastest decay-to-minus-20-dB is not one pass");
    expectNear(YouKnow106Engine::envelopeDecaySeconds(64.0f / 127.0f),
               527.0 * 0.0042, 1.0e-4,
               "mid decay-to-minus-20-dB misses the integer recurrence");
    expectNear(YouKnow106Engine::envelopeDecaySeconds(1.0f),
               5137.0 * 0.0042, 1.0e-4,
               "slowest decay-to-minus-20-dB misses the integer recurrence");

    // The voice amplifier is a current-controlled OTA behind a grounded-base
    // volts-to-amps stage, so its gain is linear in the control voltage above
    // the turn-on with the transistor's own exponential knee below it. These
    // check that shape against the schematic rather than against a chosen
    // curve; the remaining open part of OQ-19 is a measured BA662 gain sweep,
    // which would fix the turn-on point, not the law.
    using VoiceVcaLaw = YouKnow106Engine::VoiceVcaControlLaw;
    float previousGain = -1.0f;
    for (int step = 0; step <= 1000; ++step)
    {
        const float gain = VoiceVcaLaw::gain(step / 1000.0f);
        expect(std::isfinite(gain) && gain >= 0.0f && gain >= previousGain,
               "the voice-VCA control law is not finite and monotone");
        previousGain = gain;
    }
    expectNear(VoiceVcaLaw::gain(1.0f), 1.0, 1.0e-6,
               "full control does not give the voice VCA unity gain");

    // Linear in control above the turn-on: equal control steps are equal gain
    // steps, which an exponential law cannot do. Checked well clear of the
    // knee, whose whole width is a couple of per cent of the span.
    for (float control = 0.1f; control <= 0.9f; control += 0.1f)
    {
        const double linear = (static_cast<double>(control) - VoiceVcaLaw::turnOn)
                            / (1.0 - VoiceVcaLaw::turnOn);
        expectNear(VoiceVcaLaw::gain(control), linear, 2.0e-6,
                   "the voice VCA is not linear in control above its turn-on");
    }

    // And exponential below it, at the grounded-base stage's own 60 mV per
    // decade -- kT/q times ln 10, referred to the converter's 10 V span. One
    // decade of control below the turn-on against two decades below it, where
    // the softplus is already close to its exponential asymptote.
    {
        const float decade = VoiceVcaLaw::knee * 2.302585f;
        const double upper = VoiceVcaLaw::gain(VoiceVcaLaw::turnOn - decade);
        const double lower = VoiceVcaLaw::gain(VoiceVcaLaw::turnOn - 2.0f * decade);
        expectNear(upper / lower, 10.0, 0.5,
                   "the voice VCA's low-level knee is not 60 mV per decade");
        expect(VoiceVcaLaw::gain(VoiceVcaLaw::deadband) == 0.0f,
               "the voice VCA does not shut below its declared deadband");
        // A card sitting at the largest control offset the Unit Character
        // ceiling can present must still count as shut, or its voice never
        // retires. 0.004 per unit of Unit Character, bounded at two.
        expect(VoiceVcaLaw::gain(2.0f * 0.004f) < VoiceVcaLaw::silenceGain,
               "the worst card control offset escapes the silence threshold");
    }

    // VCA LEVEL is not this per-voice law. It drives the common jack-board VCA
    // after the six voices are summed. These are regression guards for the
    // current provisional three-point byte-to-dB fit, not a settled Roland
    // byte-to-GC1 law. NEC's separate -5.9 mV/dB GC1 device boundary does not
    // supply that missing mapping. The declared input is
    // p=b/127=DAC12/4064 and the legacy display coordinate is x=-5+10p, so the
    // midpoint below is explicitly x=0.
    const auto patchLevelDb = [](float position) {
        return 20.0 * std::log10(YouKnow106Engine::patchLevelGain(position));
    };
    expectNear(patchLevelDb(0.0f), -15.0, 1.0e-4,
               "VCA LEVEL minimum misses the reported -5-panel anchor");
    expectNear(patchLevelDb(0.5f), -12.5, 1.0e-4,
               "VCA LEVEL centre misses the reported zero-panel anchor");
    expectNear(patchLevelDb(1.0f), 5.0, 1.0e-4,
               "VCA LEVEL maximum misses the reported +5-panel anchor");
    expect(YouKnow106Engine::patchLevelGain(0.0f) > 0.0f,
           "VCA LEVEL minimum incorrectly mutes the patch");

    // The jack-board gain chain is fixed by resistor ratios. IC1a attenuates
    // every voice before the shared VCA and BBDs; IC6 supplies the dry/wet
    // output gains after the BBDs. Their net small-signal gains must therefore
    // be 10/39 and 10/47, not the unity voice sum used by the old model.
    expectNear(YouKnow106Engine::voiceSummerGain, 3.3 / 33.0, 1.0e-7,
               "voice summer is not 3.3 kOhm / 33 kOhm");
    expectNear(YouKnow106Engine::voiceBusInput(6.0f), 0.6, 1.0e-7,
               "the six-voice bus does not enter the shared path at 0.1 per voice");
    constexpr double busCapacitance = 10.0e-6;
    constexpr double busResistance = 33000.0;
    const double expectedBusCorner =
        1.0 / (2.0 * pi * busCapacitance * busResistance);
    expectNear(YouKnow106Engine::voiceBusCouplingCornerHz(), expectedBusCorner,
               1.0e-6, "voice-bus C14/R39 coupling corner");
    constexpr double selectedInputResistance = 47000.0;
    const double loadedBusResistance =
        busResistance * selectedInputResistance
        / (busResistance + selectedInputResistance);
    const double loadedBusCorner =
        1.0 / (2.0 * pi * busCapacitance * loadedBusResistance);
    expectNear(YouKnow106Engine::voiceBusCouplingCornerHz(HighPassMode::One),
               loadedBusCorner, 1.0e-6,
               "flat HPF leg does not load C14 through its 47 kOhm input");
    expectNear(YouKnow106Engine::voiceBusCouplingCornerHz(HighPassMode::Boost),
               loadedBusCorner, 1.0e-6,
               "boost HPF leg does not load C14 through R25");
    expectNear(YouKnow106Engine::voiceBusCouplingCornerHz(HighPassMode::Two),
               expectedBusCorner, 1.0e-6,
               "C10 incorrectly loads C14 at sub-hertz frequencies");
    expectNear(YouKnow106Engine::voiceBusCouplingCornerHz(HighPassMode::Three),
               expectedBusCorner, 1.0e-6,
               "C11 incorrectly loads C14 at sub-hertz frequencies");
    expectNear(YouKnow106Engine::commonVcaInputCouplingCornerHz(),
               expectedBusCorner, 1.0e-6,
               "common-VCA C12/R36 input-coupling corner");
    expectNear(YouKnow106Engine::voiceSummerGain * Chorus::dryMixGain,
               10.0 / 39.0, 1.0e-6,
               "net per-voice dry gain misses the jack-board ratios");
    expectNear(YouKnow106Engine::voiceSummerGain * Chorus::wetMixGain,
               10.0 / 47.0, 1.0e-6,
               "net per-voice wet gain misses the jack-board ratios");
}

void testPulseWidthAndHighPassLaws()
{
    // Enabled PWM cannot reach either rail. Pulse Off is a separate documented
    // -0.8 V state that pins the comparator high.
    expectNear(YouKnow106Engine::pwmControlVolts(0.0f), 6.0, 1.0e-5,
               "pulse threshold with the control at rest");
    expectNear(YouKnow106Engine::pwmControlVolts(1.0f), 0.6, 1.0e-5,
               "pulse threshold at full depth");
    expectNear(YouKnow106Engine::pwmDutyCycle(6.0f), 0.5, 1.0e-5,
               "threshold at half the ramp does not bisect it");
    expectNear(YouKnow106Engine::pwmDutyCycle(0.6f), 0.95, 1.0e-5,
               "narrowest pulse");
    expectNear(YouKnow106Engine::pwmDutyCycle(-0.8f), 1.0, 1.0e-6,
               "pulse-off control does not pin the comparator high");
    expectNear(YouKnow106Engine::pwmDutyCycle(3.0f, 1.03f),
               1.0 - 3.0 / (12.0 * 1.03), 1.0e-6,
               "a stronger physical ramp did not widen the comparator pulse");
    expectNear(YouKnow106Engine::pwmDutyCycle(3.0f, 0.97f),
               1.0 - 3.0 / (12.0 * 0.97), 1.0e-6,
               "a weaker physical ramp did not narrow the comparator pulse");
    expectNear(YouKnow106Engine::pwmDutyCycle(6.0f, 0.25f), 0.0, 0.0,
               "an under-compensated ramp cannot pin the comparator low");
    expectNear(YouKnow106Engine::pwmDutyCycle(-0.8f, 1.03f), 1.0, 1.0e-6,
               "ramp variation defeated the pulse-off pinned state");
    for (float depth = 0.0f; depth <= 1.0f; depth += 0.05f)
    {
        const float duty = YouKnow106Engine::pwmDutyCycle(
            YouKnow106Engine::pwmControlVolts(depth));
        expect(duty > 0.0f && duty < 1.0f, "pulse reaches a rail");
    }

    // The per-card comparator offset is calibrated to leave a 48% to 52% duty
    // window across the six voices, so the voltage the engine draws against has
    // to be the one that produces exactly two points either way. Asserting the
    // duty rather than the voltage is the point: the voltage is a means, and a
    // change to the duty law that quietly rescaled it would pass otherwise.
    {
        const float offsetVolts = 0.24f;
        const float mid = 3.0f;   // away from the law's 50% floor, so it moves both ways
        const double wide = YouKnow106Engine::pwmDutyCycle(mid - offsetVolts);
        const double narrow = YouKnow106Engine::pwmDutyCycle(mid + offsetVolts);
        expectNear(0.5 * (wide - narrow), 0.02, 5.0e-4,
                   "the per-card comparator offset is not +/-2 points of duty");
        // And at the panel's own 50% end the law floors, so the same offset can
        // only widen the pulse -- 50% is the narrowest the control can ask for.
        expectNear(YouKnow106Engine::pwmDutyCycle(6.0f + offsetVolts), 0.5, 1.0e-5,
                   "an offset pushed the pulse below the panel's 50% floor");
        expectNear(YouKnow106Engine::pwmDutyCycle(6.0f - offsetVolts), 0.52, 5.0e-4,
                   "an offset at the 50% floor does not widen by two points");
    }

    // Only two of the four high-pass legs filter; one boosts and one passes.
    // The boost is the measured shelf: +10.5 dB at DC, +1.41 dB in the high
    // band, corner near 59 Hz. The cut corners follow from the network's own
    // part values.
    expectNear(20.0 * std::log10(
                   YouKnow106Engine::highPassShelfGain(HighPassMode::Boost)),
               10.5, 0.05, "bass boost does not lift the low band 10.5 dB");
    expectNear(20.0 * std::log10(
                   YouKnow106Engine::highPassHighGain(HighPassMode::Boost)),
               1.41, 0.05, "bass boost does not lift the high band 1.41 dB");
    expectNear(YouKnow106Engine::highPassCornerHz(HighPassMode::Boost), 59.4, 0.5,
               "bass boost shelf corner");
    expectNear(YouKnow106Engine::highPassShelfGain(HighPassMode::One), 1.0, 1.0e-6,
               "the flat leg does not pass the low band untouched");
    expectNear(YouKnow106Engine::highPassHighGain(HighPassMode::One), 1.0, 1.0e-6,
               "the flat leg does not pass the high band untouched");
    expect(YouKnow106Engine::highPassShelfGain(HighPassMode::Two) == 0.0f
           && YouKnow106Engine::highPassShelfGain(HighPassMode::Three) == 0.0f,
           "a cutting leg returns part of the low band");
    // Computed from the schematic parts rather than written down, so the
    // assertion says where the number comes from: the two cutting legs use
    // C10 15 nF and C11 4.7 nF through the 47 kOhm resistor pack.
    const double feedOhms = 47.0e3;
    const auto corner = [feedOhms] (double farads) {
        return 1.0 / (2.0 * pi * feedOhms * farads);
    };
    expectNear(YouKnow106Engine::highPassCornerHz(HighPassMode::Two),
               corner(15.0e-9), 0.5,
               "middle high-pass corner is not 47 kOhm against 15 nF");
    expectNear(YouKnow106Engine::highPassCornerHz(HighPassMode::Three),
               corner(4.7e-9), 0.5,
               "top high-pass corner is not 47 kOhm against 4.7 nF");

    // The service schematic's final stereo coupling paths are identical:
    // IC6 -> C17/C20 10 uF -> R54/R57 1.5 kOhm -> one 10 kOhm VOLUME
    // track. These no-argument helpers retain the earlier unloaded/full-track
    // comparison boundary; the runtime's internally loaded law follows below.
    constexpr double capacitance = 10.0e-6;
    constexpr double seriesResistance = 1500.0;
    constexpr double potResistance = 10000.0;
    const double expectedCorner =
        1.0 / (2.0 * pi * capacitance
               * (seriesResistance + potResistance));
    const double expectedHighGain =
        potResistance / (seriesResistance + potResistance);
    expectNear(YouKnow106Engine::outputCouplingCornerHz(), expectedCorner,
               1.0e-5, "final output-coupling corner");
    expectNear(YouKnow106Engine::outputCouplingHighGain(), expectedHighGain,
               1.0e-7, "final output-coupling high-frequency gain");

    // The selector ladder and headphone amplifier are connected to each wiper
    // internally even with no external jack load. A nominal-linear B track is
    // therefore slightly loaded in circuit, and that same load moves the C17/
    // C20 pole with shaft position.
    constexpr double selectorLadder = 33000.0 + 6800.0 + 1500.0;
    constexpr double headphoneInput = 1000.0 + 100000.0;
    const double internalWiperLoad =
        selectorLadder * headphoneInput / (selectorLadder + headphoneInput);
    const auto expectedLoadedGain = [&](double position) {
        const double lower = position * potResistance;
        const double loadedLower = lower > 0.0
            ? lower * internalWiperLoad / (lower + internalWiperLoad) : 0.0;
        const double upper = (1.0 - position) * potResistance;
        return loadedLower / (seriesResistance + upper + loadedLower);
    };
    const auto expectedLoadedCorner = [&](double position) {
        const double lower = position * potResistance;
        const double loadedLower = lower > 0.0
            ? lower * internalWiperLoad / (lower + internalWiperLoad) : 0.0;
        const double upper = (1.0 - position) * potResistance;
        return 1.0 / (2.0 * pi * capacitance
                      * (seriesResistance + upper + loadedLower));
    };
    for (const float position : { 0.0f, 0.5f, 1.0f })
    {
        expectNear(YouKnow106Engine::outputCouplingHighGain(position),
                   expectedLoadedGain(position), 1.0e-7,
                   "loaded 10KB wiper gain at shaft position "
                       + std::to_string(position));
        expectNear(YouKnow106Engine::outputCouplingCornerHz(position),
                   expectedLoadedCorner(position), 1.0e-5,
                   "loaded output-coupling corner at shaft position "
                       + std::to_string(position));
    }
    expectNear(YouKnow106Engine::outputCouplingHighGain(0.5f)
                   / YouKnow106Engine::outputCouplingHighGain(1.0f),
               0.4763, 5.0e-4,
               "the loaded nominal-linear pot midpoint escaped its circuit law");

    // Exercise the realised loaded pole too, not only its static helper. At a
    // fixed shaft position the exact TPT step is highGain*pole^n/(1+g).
    constexpr double loadedRate = 48000.0;
    constexpr int loadedSamples = 48000;
    const std::vector<float> loadedStep(loadedSamples, 1.0f);
    for (const float position : { 0.25f, 0.5f, 1.0f })
    {
        const double corner =
            YouKnow106Engine::outputCouplingCornerHz(position);
        const double gain =
            YouKnow106Engine::outputCouplingHighGain(position);
        const double g = std::tan(pi * corner / loadedRate);
        const double pole = (1.0 - g) / (1.0 + g);
        const auto response = YouKnow106TestAccess::renderLoadedOutputCoupling(
            loadedStep, loadedRate, position);
        for (const int sample : { 0, 12000, loadedSamples - 1 })
            expectNear(response[static_cast<std::size_t>(sample)],
                       gain * std::pow(pole, sample) / (1.0 + g), 2.0e-7,
                       "loaded output pole misses its fixed-position response at "
                           + std::to_string(position));
    }

    // The realised topology-preserving pole must retain the same physical
    // time constant at every supported host-rate family. A unit step through
    // a high-pass is highGain*exp(-t/tau), independent of block processing.
    constexpr double timeConstant = capacitance
                                  * (seriesResistance + potResistance);
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const int samples = static_cast<int>(std::ceil(sampleRate * 1.2));
        const std::vector<float> step(static_cast<std::size_t>(samples), 1.0f);
        const auto response =
            YouKnow106TestAccess::renderOutputCoupling(step, sampleRate);
        const int atTau = static_cast<int>(std::llround(
            sampleRate * timeConstant));
        const double g = std::tan(pi * expectedCorner / sampleRate);
        const double pole = (1.0 - g) / (1.0 + g);
        const auto expectedStepAt = [&](int sample) {
            return expectedHighGain * std::pow(pole, sample) / (1.0 + g);
        };
        expectNear(response[static_cast<std::size_t>(atTau)],
                   expectedStepAt(atTau), 2.0e-6,
                   "final output coupling misses its 115 ms time constant at "
                       + std::to_string(static_cast<int>(sampleRate)) + " Hz");
        expectNear(response.back(), expectedStepAt(samples - 1), 2.0e-7,
                   "final output coupling has a sample-rate-dependent decay");
    }


    // C14/R39 precedes the switch and therefore remains in circuit for every
    // HPF position. Verify its 330 ms R39-only boundary at host-rate and HQ-rate
    // families; the mode-specific 47 kOhm loading was checked separately above.
    constexpr double busCapacitance = 10.0e-6;
    constexpr double busResistance = 33000.0;
    constexpr double busTimeConstant = busCapacitance * busResistance;
    const double busCorner =
        1.0 / (2.0 * pi * busTimeConstant);
    for (const double sampleRate : { 44100.0, 48000.0, 192000.0 })
    {
        const int samples = static_cast<int>(std::ceil(sampleRate * 1.2));
        const std::vector<float> step(static_cast<std::size_t>(samples), 1.0f);
        const auto response =
            YouKnow106TestAccess::renderVoiceBusCoupling(step, sampleRate);
        const int atTau = static_cast<int>(std::llround(
            sampleRate * busTimeConstant));
        const double g = std::tan(pi * busCorner / sampleRate);
        const double pole = (1.0 - g) / (1.0 + g);
        const auto expectedStepAt = [&](int sample) {
            return std::pow(pole, sample) / (1.0 + g);
        };
        expectNear(response[static_cast<std::size_t>(atTau)],
                   expectedStepAt(atTau), 2.0e-6,
                   "voice-bus coupling misses its 330 ms time constant at "
                       + std::to_string(static_cast<int>(sampleRate)) + " Hz");
        expectNear(response.back(), expectedStepAt(samples - 1), 2.0e-7,
                   "voice-bus coupling has a sample-rate-dependent decay");

        const auto vcaResponse =
            YouKnow106TestAccess::renderCommonVcaInputCoupling(step, sampleRate);
        expectNear(vcaResponse[static_cast<std::size_t>(atTau)],
                   expectedStepAt(atTau), 2.0e-6,
                   "common-VCA input coupling misses its 330 ms time constant at "
                       + std::to_string(static_cast<int>(sampleRate)) + " Hz");
        expectNear(vcaResponse.back(), expectedStepAt(samples - 1), 2.0e-7,
                   "common-VCA input coupling has a sample-rate-dependent decay");
    }
}

void testModulationAndGlideLaws()
{
    // Exact hash-matched B-2 rate vectors. A signed cycle is four clamped
    // 0..0x1fff ramps, each lasting ceil(8192 / coefficient) scan passes.
    struct LfoVector { int code; std::uint16_t coefficient; int rampPasses; };
    constexpr std::array<LfoVector, 3> lfoVectors {{
        { 0,      5u, 1639 },
        { 64,   666u,   13 },
        { 127, 4096u,    2 }
    }};
    for (const auto& vector : lfoVectors)
    {
        const float position = static_cast<float>(vector.code) / 127.0f;
        expect(YouKnow106Engine::lfoRateIncrement(position) == vector.coefficient,
               "B-2 LFO coefficient vector mismatch at code "
                   + std::to_string(vector.code));
        expectNear(YouKnow106Engine::lfoRateHz(position),
                   1.0 / (4.0 * vector.rampPasses * 0.0042), 2.0e-6,
                   "B-2 LFO rate vector mismatch at code "
                       + std::to_string(vector.code));
    }

    // Delay is never an immediate jump: it is an attack-table silent hold plus
    // one of eight exact fade bins. Verify every stored byte against that
    // integer construction so the hold cannot drift away from attack's source.
    for (int byte = 0; byte <= 127; ++byte)
    {
        const float position = static_cast<float>(byte) / 127.0f;
        const int attack = YouKnow106Engine::envelopeAttackIncrement(position);
        const int fade = YouKnow106Engine::lfoDelayFadeIncrement(position);
        const int holdPasses = (16384 + attack - 1) / attack;
        const int fadePasses = (65536 + fade - 1) / fade;
        expectNear(YouKnow106Engine::lfoDelaySeconds(position),
                   (holdPasses + fadePasses) * 0.0042, 1.0e-5,
                   "LFO delay does not use exact attack-hold plus fade at byte "
                       + std::to_string(byte));
    }
    expectNear(YouKnow106Engine::lfoDelaySeconds(0.0f), 0.0126, 1.0e-6,
               "LFO delay byte zero is not one hold plus two fade passes");
    expectNear(YouKnow106Engine::lfoDelaySeconds(1.0f), 4.3554, 1.0e-4,
               "longest LFO delay misses the exact hold-plus-fade duration");

    struct FadeBin { int first; int last; std::uint16_t coefficient; int passes; };
    constexpr std::array<FadeBin, 8> fadeBins {{
        {   0,  15, 65535u,   2 },
        {  16,  31,  1049u,  63 },
        {  32,  47,   524u, 126 },
        {  48,  63,   350u, 188 },
        {  64,  79,   256u, 256 },
        {  80,  95,   256u, 256 },
        {  96, 111,   256u, 256 },
        { 112, 127,   256u, 256 }
    }};
    for (const auto& bin : fadeBins)
    {
        for (const int byte : { bin.first, bin.last })
        {
            const auto coefficient = YouKnow106Engine::lfoDelayFadeIncrement(
                static_cast<float>(byte) / 127.0f);
            expect(coefficient == bin.coefficient,
                   "LFO delay fade bin coefficient mismatch at byte "
                       + std::to_string(byte));
            expect((65536 + coefficient - 1) / coefficient == bin.passes,
                   "LFO delay fade pass count mismatch at byte "
                       + std::to_string(byte));
        }
    }

    // Portamento is an eight-bit ADC. Raw 0/1 are immediate, then 2n and 2n+1
    // share an eight-bit coefficient selected by raw>>1.
    expect(YouKnow106Engine::portamentoSeconds(0.0f) == 0.0f,
           "portamento is not switched off at the bottom of its travel");
    expect(YouKnow106Engine::portamentoIncrement(0.0f) == 0u,
           "raw portamento code zero is not immediate");
    expect(YouKnow106Engine::portamentoSeconds(1.0f / 255.0f) == 0.0f,
           "raw portamento code one does not retain the zero/immediate entry");
    expect(YouKnow106Engine::portamentoIncrement(1.0f / 255.0f) == 0u,
           "raw portamento code one selects a nonzero coefficient");
    expect(YouKnow106Engine::portamentoIncrement(2.0f / 255.0f) == 255u
           && YouKnow106Engine::portamentoIncrement(3.0f / 255.0f) == 255u,
           "raw portamento codes 2/3 miss coefficient 255");
    expectNear(YouKnow106Engine::portamentoSeconds(2.0f / 255.0f),
               13.0 * 0.0042, 1.0e-6,
               "first active portamento code does not respect the 8-bit coefficient ceiling");
    expect(YouKnow106Engine::portamentoIncrement(127.0f / 255.0f) == 13u
           && YouKnow106Engine::portamentoIncrement(128.0f / 255.0f) == 13u,
           "raw portamento codes 127/128 miss coefficient 13");
    expectNear(YouKnow106Engine::portamentoSeconds(127.0f / 255.0f),
               237.0 * 0.0042, 1.0e-6,
               "raw portamento code 127 misses the 995.4 ms vector");
    expectNear(YouKnow106Engine::portamentoSeconds(128.0f / 255.0f),
               237.0 * 0.0042, 1.0e-6,
               "raw portamento code 128 misses the 995.4 ms vector");
    expect(YouKnow106Engine::portamentoIncrement(254.0f / 255.0f) == 1u
           && YouKnow106Engine::portamentoIncrement(1.0f) == 1u,
           "raw portamento codes 254/255 miss coefficient one");
    expectNear(YouKnow106Engine::portamentoSeconds(254.0f / 255.0f),
               3072.0 * 0.0042, 1.0e-4,
               "raw portamento code 254 misses the 12.9024 second vector");
    expectNear(YouKnow106Engine::portamentoSeconds(1.0f),
               3072.0 * 0.0042, 1.0e-4,
               "raw portamento code 255 misses the 12.9024 second vector");
    for (int raw = 2; raw < 255; raw += 2)
        expect(YouKnow106Engine::portamentoIncrement(raw / 255.0f)
                   == YouKnow106Engine::portamentoIncrement((raw + 1) / 255.0f),
               "paired portamento ADC codes select different coefficients at raw "
                   + std::to_string(raw));
}

void testConverterQueueAndOutputReference()
{
    using Destination = YouKnow106Engine::ConverterDestination;
    using Write = YouKnow106Engine::ConverterWrite;
    constexpr std::array<Write, YouKnow106Engine::converterWritesPerPass> expected {{
        { Destination::Resonance, -1 },
        { Destination::CommonVca, -1 },
        { Destination::Sub, -1 },
        { Destination::Pitch, 0 },
        { Destination::Pitch, 1 },
        { Destination::Pitch, 2 },
        { Destination::Pitch, 3 },
        { Destination::Pitch, 4 },
        { Destination::Pitch, 5 },
        { Destination::Pwm, -1 },
        { Destination::Vcf, 0 },
        { Destination::VoiceVca, 0 },
        { Destination::Vcf, 1 },
        { Destination::VoiceVca, 1 },
        { Destination::Vcf, 2 },
        { Destination::VoiceVca, 2 },
        { Destination::Vcf, 3 },
        { Destination::VoiceVca, 3 },
        { Destination::Vcf, 4 },
        { Destination::VoiceVca, 4 },
        { Destination::Vcf, 5 },
        { Destination::VoiceVca, 5 },
        { Destination::Noise, -1 }
    }};
    const auto& actual = YouKnow106Engine::converterWriteOrder();
    for (std::size_t index = 0; index < expected.size(); ++index)
        expect(actual[index].destination == expected[index].destination
                   && actual[index].voice == expected[index].voice,
               "B-2 converter write-order mismatch at ordinal "
                   + std::to_string(index));
    const auto resonanceWrites = std::count_if(
        actual.begin(), actual.end(), [](const Write& write) {
            return write.destination == Destination::Resonance
                && write.voice == -1;
        });
    expect(resonanceWrites == 1,
           "a converter pass does not contain exactly one shared resonance write");

    const auto normalized = YouKnow106Engine::converterEventPhases(
        YouKnow106Engine::ConverterTimingProfile::NormalizedServiceChart);
    expect(normalized.front() == 0.0 && normalized.back() < 1.0,
           "normalized converter profile does not fit inside one pass");
    for (std::size_t index = 1; index < normalized.size(); ++index)
        expect(normalized[index] > normalized[index - 1],
               "normalized converter profile collapsed or reordered an event");
    expect(normalized[3] != normalized[8],
           "normalized profile phase-locks the first and sixth DCO writes");

    const auto phaseZero = YouKnow106Engine::converterEventPhases(
        YouKnow106Engine::ConverterTimingProfile::PhaseZeroDiagnostic);
    expect(std::all_of(phaseZero.begin(), phaseZero.end(), [](double phase) {
               return phase == 0.0;
           }),
           "phase-zero diagnostic profile contains an invented timestamp");

    // The compatibility reference is chosen to make the newly explicit final
    // boundary unity, preserving existing sessions while declaring -18 dBFS
    // RMS as a product convention rather than an analogue property.
    expectNear(YouKnow106Engine::outputReferenceGain(
                   YouKnow106Engine::compatibilityOutputReferenceRmsVolts),
               1.0, 1.0e-6,
               "compatibility output reference does not preserve unity gain");
    expectNear(YouKnow106Engine::outputReferenceGain(1.0f),
               YouKnow106Engine::internalVoltsPerUnit
                   * YouKnow106Engine::minus18DbfsAmplitude,
               1.0e-7, "one-volt RMS output reference violates the boundary law");
    const double lower = 0.25 * YouKnow106Engine::outputReferenceGain(1.0f);
    const double doubled = 0.50 * YouKnow106Engine::outputReferenceGain(1.0f);
    expectNear(20.0 * std::log10(doubled / lower), 6.020599913, 1.0e-7,
               "doubling analogue output does not add 6.0206 dB at the boundary");
    expectNear(YouKnow106Engine::outputReferenceGain(2.0f),
               0.5 * YouKnow106Engine::outputReferenceGain(1.0f), 1.0e-7,
               "doubling Vref does not halve only the final-boundary gain");
}

void testPanelLawsInvert()
{
    // What a control displays is the value the circuit produces, so a host
    // letting someone type that value needs a way back to the travel. Round
    // tripping is the only thing that makes the typed number mean anything.
    const auto roundTrip = [](float position, auto forward, auto inverse,
                              const std::string& name) {
        const float value = forward(position);
        const float back = inverse(value);
        expectNear(back, position, 0.01,
                   name + " does not round trip through its displayed value");
    };

    for (float position = 0.0f; position <= 1.0f; position += 0.125f)
    {
        roundTrip(position, YouKnow106Engine::envelopeAttackSeconds,
                  YouKnow106Engine::panelPositionForAttack, "attack");
        roundTrip(position, YouKnow106Engine::envelopeDecaySeconds,
                  YouKnow106Engine::panelPositionForDecay, "decay");
        roundTrip(position, YouKnow106Engine::envelopeReleaseSeconds,
                  YouKnow106Engine::panelPositionForRelease, "release");
        roundTrip(position, YouKnow106Engine::lfoDelaySeconds,
                  YouKnow106Engine::panelPositionForLfoDelay, "modulation delay");

        // The modulator's rate is quantised onto whole passes, so many
        // positions read the same frequency and a typed value can only land
        // on a canonical one. What must hold is that the position it lands
        // on *produces* the typed frequency.
        const float rate = YouKnow106Engine::lfoRateHz(position);
        expectNear(YouKnow106Engine::lfoRateHz(
                       YouKnow106Engine::panelPositionForLfoRate(rate)),
                   rate, 1.0e-4,
                   "modulation rate does not round trip through its displayed value");
    }

    // Cutoff round trips only while the exponential result remains below the
    // explicit 50 kHz product cap; all higher codes necessarily share it.
    for (float position = 0.0f; position <= 0.80f; position += 0.1f)
        roundTrip(position, [](float p) {
                      return YouKnow106Engine::vcfCutoffHz(
                          YouKnow106Engine::vcfPanelCounts(p));
                  },
                  YouKnow106Engine::panelPositionForCutoff, "cutoff");

    const double uncappedAt13716 = YouKnow106Engine::vcfBaseFrequencyHz
        * std::exp2(13716.0 / YouKnow106Engine::vcfCountsPerOctave);
    expectNear(YouKnow106Engine::vcfCutoffHz(13716.0f), uncappedAt13716,
               uncappedAt13716 * 2.0e-6,
               "the cutoff safety policy introduced a pre-cap knee");
    const float ceilingHz = YouKnow106Engine::vcfCutoffHz(
        YouKnow106Engine::vcfPanelCounts(1.0f));
    expectNear(ceilingHz, 50000.0, 1.0e-3,
               "the panel top does not land on the named 50 kHz product cap");
    expectNear(YouKnow106Engine::vcfCutoffHz(16383.0f), 50000.0, 1.0e-3,
               "the accumulator top does not retain the 50 kHz product cap");

    // Portamento's bottom detent is Off, and both raw-code pairing and 8.8-step
    // projection create plateaus. Its inverse therefore returns a canonical
    // code producing the same displayed time, not necessarily the original
    // position inside that plateau.
    for (float position = 0.1f; position <= 1.0f; position += 0.15f)
    {
        const float seconds = YouKnow106Engine::portamentoSeconds(position);
        expectNear(YouKnow106Engine::portamentoSeconds(
                       YouKnow106Engine::panelPositionForPortamento(seconds)),
                   seconds, 1.0e-6,
                   "portamento does not round trip through its displayed value");
    }
    expectNear(YouKnow106Engine::panelPositionForPortamento(0.0f), 0.0, 1.0e-6,
               "a glide time of zero does not read as switched off");

    // A typed value outside the control's range must land on its nearest end
    // rather than anywhere else.
    expectNear(YouKnow106Engine::panelPositionForCutoff(1.0f), 0.0, 1.0e-6,
               "a cutoff below the range does not clamp to the bottom");
    expectNear(YouKnow106Engine::panelPositionForLfoRate(1000.0f), 1.0, 1.0e-6,
               "a rate above the range does not clamp to the top");
}

void testComparatorEdgesSitOnOneThreshold()
{
    // The comparator is one threshold on the ramp, so its two edges per cycle
    // have to sit at the same ramp voltage. The falling edge therefore belongs
    // partway into the reset, where the descending segment passes back through
    // that voltage -- not at the start of the reset, where the ramp is still at
    // its positive rail. This solves the ramp's own geometry independently of
    // the model rather than re-deriving the model's formula.
    const auto rampAt = [](double phase, double reset) {
        const double rise = std::max(1.0 - reset, 1.0e-4);
        if (phase < rise)
            return static_cast<double>(YouKnow106Engine::rampSegmentVoltage(
                static_cast<float>(phase / rise)));
        return 1.0 - 2.0 * (phase - rise) / reset;
    };

    for (double reset : { 0.0001, 0.01, 0.055, 0.25 })
    {
        for (float duty : { 0.05f, 0.25f, 0.5f, 0.75f, 0.95f })
        {
            const double riseEdge = YouKnow106Engine::pulseRisePhase(
                duty, static_cast<float>(reset));
            const double fallEdge = YouKnow106Engine::pulseFallPhase(
                duty, static_cast<float>(reset));

            expectNear(rampAt(fallEdge, reset), rampAt(riseEdge, reset), 2.0e-3,
                       "the comparator's two edges are not at the same ramp "
                       "voltage");
            expect(fallEdge > riseEdge,
                   "the comparator falls before it rises");
            expect(fallEdge <= 1.0 + 1.0e-6,
                   "the comparator's falling edge left the cycle");
            // And it is inside the reset segment, not at its start: that is the
            // whole point.
            expect(fallEdge >= 1.0 - reset - 1.0e-6,
                   "the falling edge is not inside the reset segment");
        }
    }

    // With a negligible reset the high interval is the requested duty, so the
    // correction cannot have moved the ordinary case.
    for (float duty : { 0.05f, 0.5f, 0.95f })
    {
        const double high = YouKnow106Engine::pulseFallPhase(duty, 1.0e-6f)
                          - YouKnow106Engine::pulseRisePhase(duty, 1.0e-6f);
        expectNear(high, duty, 1.0e-4,
                   "a vanishing reset does not give the requested duty");
    }
}

void testChorusIsAtItsSettingFromTheFirstSample()
{
    // A patch loaded with the effect switched on is not a player reaching for
    // the button: there is nothing to glide from. Measured on the wet path
    // alone -- the line adds its output to dry at IC6, so subtracting IC6's
    // amplified dry contribution leaves exactly what the effect contributed,
    // with no note onset or modulation depth mixed into the reading.
    constexpr double sampleRate = 192000.0;
    Chorus chorus;
    chorus.prepare(sampleRate);

    const int length = static_cast<int>(sampleRate * 0.5);
    std::vector<float> wet(static_cast<std::size_t>(length));
    for (int index = 0; index < length; ++index)
    {
        const float input = std::sin(2.0f * static_cast<float>(pi) * 220.0f
                                     * static_cast<float>(index)
                                     / static_cast<float>(sampleRate));
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(input, ChorusMode::Two, 0.0f, left, right);
        wet[static_cast<std::size_t>(index)] =
            left - input * Chorus::dryMixGain;
    }

    const auto rms = [&](double fromSeconds, double toSeconds) {
        const auto from = static_cast<std::size_t>(sampleRate * fromSeconds);
        const auto to = std::min(static_cast<std::size_t>(sampleRate * toSeconds),
                                 wet.size());
        double sum = 0.0;
        for (std::size_t index = from; index < to; ++index)
            sum += static_cast<double>(wet[index]) * wet[index];
        return to > from ? std::sqrt(sum / static_cast<double>(to - from)) : 0.0;
    };

    // From the moment the longest delay has filled, against the settled level.
    const double early = rms(0.006, 0.016);
    const double settled = rms(0.4, 0.5);
    expect(settled > 0.1, "the wet path never reached its level at all");
    expectNear(early, settled, settled * 0.1,
               "the effect glided up to its setting instead of starting there");
}

void testJuno60FallbackBucketBrigadeTiming()
{
    // The part is 256 stages clocked in two phases, so its delay is
    // 128 / clock -- which is the datasheet's 12.8 ms at its 10 kHz minimum.
    expectNear(Chorus::delaySecondsForClock(10000.0f), 0.0128, 1.0e-9,
               "line delay at the minimum clock");
    expectNear(Chorus::delaySecondsForClock(200000.0f), 0.00064, 1.0e-9,
               "line delay at the maximum clock");
    expectNear(Chorus::clockForDelaySeconds(Chorus::delaySecondsForClock(43210.0f)),
               43210.0, 1.0e-3, "delay and clock are not reciprocal");

    // Both modes sweep the same delay range and differ only in rate. The depth
    // is still the JUNO-60's; the rates are its scale re-split by this
    // instrument's own timing-resistance ratio.
    const auto one = Chorus::settingsFor(ChorusMode::One);
    const auto two = Chorus::settingsFor(ChorusMode::Two);

    // The T-network the mode switch drives, straight off the schematic. Assert
    // both legs, because the ratio alone would still pass if each were wrong by
    // the same factor.
    expectNear(Chorus::lfoTimingOhms(true), 6.4352941e6, 1.0,
               "mode I effective timing resistance");
    expectNear(Chorus::lfoTimingOhms(false), 3.9638889e6, 1.0,
               "mode II effective timing resistance");
    expectNear(Chorus::modeRateRatio(), 1.6234799, 1.0e-6,
               "the schematic's mode-rate ratio changed");
    expect(Chorus::lfoTimingOhms(true) > Chorus::lfoTimingOhms(false),
           "mode I must integrate through the larger resistance, so it is the "
           "slower mode");

    // Assert the *relation* the schematic fixes, not the resulting literals: a
    // re-derivation that split the geometric mean the wrong way round would
    // reproduce two plausible rates and this is what catches it.
    expectNear(two.rateHz / one.rateHz, Chorus::modeRateRatio(), 1.0e-5,
               "the mode rates do not carry the schematic's timing ratio");
    expect(two.rateHz > one.rateHz,
           "mode II is not the faster leg the manual and the ratio both need");
    // The absolute scale is still the sibling's and must stay exactly there:
    // this is a re-split of an unmeasured scale, not a new one.
    expectNear(std::sqrt(one.rateHz * two.rateHz),
               std::sqrt(Chorus::siblingMeasuredRateOneHz
                         * Chorus::siblingMeasuredRateTwoHz),
               1.0e-5,
               "the re-split moved the still-unmeasured absolute scale");
    // Both still round to the owner's manual's published figures. Assert the
    // rounding rather than a +/-0.05 band, which would leave mode II only
    // 0.002 Hz of margin and would break on any future re-split.
    expectNear(std::floor(one.rateHz * 10.0 + 0.5) / 10.0, 0.5, 1.0e-9,
               "mode I no longer rounds to the published about-0.5 Hz");
    expectNear(std::floor(two.rateHz * 10.0 + 0.5) / 10.0, 0.8, 1.0e-9,
               "mode II no longer rounds to the published about-0.8 Hz");
    // The sibling's own ratio is superseded. Fail loudly if it comes back.
    expect(std::abs(two.rateHz / one.rateHz
                    - Chorus::siblingMeasuredRateTwoHz
                          / Chorus::siblingMeasuredRateOneHz) > 1.0e-3,
           "the sibling's 1.682 rate ratio was reintroduced");

    expectNear(one.sweepSeconds, two.sweepSeconds, 1.0e-9,
               "the two modes do not share a sweep depth");
    // A Juno-60's measured sweep, standing in and labelled as such: no
    // qualifying capture of a Juno-106's own chorus has been located.
    expectNear(one.centreDelaySeconds - one.sweepSeconds, 0.00166, 1.0e-6,
               "shortest modulated delay");
    expectNear(one.centreDelaySeconds + one.sweepSeconds, 0.00535, 1.0e-6,
               "longest modulated delay");
    // Both ends have to land inside the part's own clock window, which is what
    // says the capture describes this circuit rather than some other one: 256
    // stages give a delay of 128 / f_clock, and the MN3009 is rated 10-200 kHz.
    for (const double delay : { one.centreDelaySeconds - one.sweepSeconds,
                                one.centreDelaySeconds + one.sweepSeconds })
    {
        const double clockHz = 128.0 / delay;
        expect(clockHz > 10.0e3 && clockHz < 200.0e3,
               "a sweep endpoint needs a clock outside the part's rated window");
    }
    const auto off = Chorus::settingsFor(ChorusMode::Off);
    expectNear(off.wetGain, 0.0, 1.0e-9,
               "the wet path is not silent when the effect is switched out");
    expectNear(off.sweepSeconds, one.sweepSeconds, 1.0e-9,
               "bypass stopped the modulation behind the wet mute");
    expectNear(off.centreDelaySeconds, one.centreDelaySeconds, 1.0e-9,
               "bypass moved the running delay line away from mode I");
    // Wet over dry is the ratio of the two input resistors into IC6's shared
    // 100 kOhm feedback. Keep both absolute gains as well as their ratio: the
    // absolute factor belongs after the nonlinear BBD and cannot be folded
    // into its input without changing the sound.
    expectNear(Chorus::dryMixGain, 100.0 / 39.0, 1.0e-6,
               "IC6 dry gain");
    expectNear(Chorus::wetMixGain, 100.0 / 47.0, 1.0e-6,
               "IC6 wet gain");
    expectNear(one.wetGain, 39.0 / 47.0, 1.0e-3, "line gain");
    expectNear(20.0 * std::log10(one.wetGain), -1.62, 0.01,
               "the wet path does not sit 1.62 dB below the dry");
    expectNear(Chorus::wetMuteTimeConstantSeconds, 0.005, 1.0e-9,
               "the labelled product mute time constant changed");
    expectNear(Chorus::wetMuteTimeConstantSeconds * std::log(9.0f),
               0.010986, 1.0e-6,
               "the mute's 10-90% duration is not its 5 ms exponential tau");

    // Observe the dry law through the actual processor too. With the effect
    // off, the wet return is muted but both BBDs keep running behind it.
    {
        Chorus chorus;
        chorus.prepare(48000.0);
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(0.1f, ChorusMode::Off, 0.0f, left, right);
        expectNear(left, 0.1 * Chorus::dryMixGain, 1.0e-6,
                   "IC6 dry gain is absent from the rendered chorus output");
        expectNear(right, left, 1.0e-9,
                   "chorus bypass moved the dry signal off centre");
    }

    // The hardware interlock and its one enable plus one I/II control line
    // cannot encode a fourth mode. Old sessions may still contain OneTwo, so
    // that compatibility value has one deterministic canonical result: II.
    const auto both = Chorus::settingsFor(ChorusMode::OneTwo);
    expectNear(both.rateHz, two.rateHz, 1.0e-9,
               "legacy I+II did not canonicalise to II's rate");
    expectNear(both.sweepSeconds, two.sweepSeconds, 1.0e-9,
               "legacy I+II did not canonicalise to II's depth");
    expectNear(both.centreDelaySeconds, two.centreDelaySeconds, 1.0e-9,
               "legacy I+II did not canonicalise to II's centre delay");
    expectNear(both.wetGain, two.wetGain, 1.0e-9,
               "legacy I+II did not canonicalise to II's line gain");

    // And it has to be observable, not just tabulated: run the effect in each
    // mode and count how far the modulation oscillator actually travels.
    const auto phaseTravel = [](ChorusMode mode) {
        Chorus chorus;
        chorus.prepare(48000.0);
        float left = 0.0f, right = 0.0f;
        double previous = chorus.getLfoPhase();
        double travel = 0.0;
        for (int n = 0; n < 48000; ++n)
        {
            chorus.process(0.0f, mode, 0.0f, left, right);
            const double now = chorus.getLfoPhase();
            travel += now >= previous ? now - previous : now + 1.0 - previous;
            previous = now;
        }
        return travel;
    };
    const double travelOne = phaseTravel(ChorusMode::One);
    const double travelTwo = phaseTravel(ChorusMode::Two);
    const double travelBoth = phaseTravel(ChorusMode::OneTwo);
    const double travelOff = phaseTravel(ChorusMode::Off);
    expectNear(travelBoth, travelTwo, 1.0e-6,
               "legacy I+II renders a different clock programme from II");
    expectNear(travelOff, travelOne, 1.0e-6,
               "the chorus oscillator stopped or changed speed in bypass");
    // The derived ratio has to survive all the way to rendered audio, not just
    // sit in the table the renderer reads.
    expectNear(travelTwo / travelOne, Chorus::modeRateRatio(), 1.0e-3,
               "the rendered mode rates do not carry the schematic ratio");

    // The whole modulated range must stay inside the part's rated clock window,
    // otherwise the model would be running a part outside its specification.
    const float slowest = Chorus::clockForDelaySeconds(
        one.centreDelaySeconds + one.sweepSeconds);
    const float fastest = Chorus::clockForDelaySeconds(
        one.centreDelaySeconds - one.sweepSeconds);
    expect(slowest >= Chorus::minimumClockHz && fastest <= Chorus::maximumClockHz,
           "modulation drives the delay line outside its rated clock range");

    // Two lines clocked in antiphase must actually differ, and the effect must
    // be silent -- apart from its own noise -- when switched out.
    Chorus chorus;
    chorus.prepare(192000.0);
    double difference = 0.0;
    for (int index = 0; index < 96000; ++index)
    {
        const float input = std::sin(2.0f * static_cast<float>(pi) * 220.0f
                                     * static_cast<float>(index) / 192000.0f);
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(input, ChorusMode::Two, 1.0f, left, right);
        if (index > 48000)
            difference += static_cast<double>(left - right) * (left - right);
    }
    expect(difference > 1.0, "the two delay lines are producing the same signal");
}

void testChorusNoiseComponents()
{
    constexpr float sampleRate = 48000.0f;
    constexpr int renderLength = 16384;

    // The existing independent per-line xorshift sources retain fixed seeds.
    // Two fresh instances must therefore render bit-identically, including the
    // asynchronous clock-edge sequence on which that noise is generated.
    Chorus first;
    Chorus second;
    first.prepare(sampleRate);
    second.prepare(sampleRate);
    bool identical = true;
    double independentEnergy = 0.0;
    for (int index = 0; index < renderLength; ++index)
    {
        float firstLeft = 0.0f;
        float firstRight = 0.0f;
        float secondLeft = 0.0f;
        float secondRight = 0.0f;
        first.process(0.0f, ChorusMode::Two, 1.0f, firstLeft, firstRight);
        second.process(0.0f, ChorusMode::Two, 1.0f, secondLeft, secondRight);
        identical = identical && firstLeft == secondLeft
                              && firstRight == secondRight;
        independentEnergy += static_cast<double>(firstLeft) * firstLeft
                           + static_cast<double>(firstRight) * firstRight;
    }
    expect(identical, "fixed chorus-noise seeds did not reproduce exactly");
    expect(independentEnergy > 0.0,
           "the preserved independent wet-line component is silent");

    // Explicit zeroes for every new component are the production default.
    // Keep this as an exact comparison so merely splitting the architecture
    // cannot alter compatibility renders through an extra add or multiply.
    Chorus explicitZero;
    Chorus implicitZero;
    explicitZero.prepare(sampleRate);
    implicitZero.prepare(sampleRate);
    YouKnow106TestAccess::configureOptionalChorusNoise(
        explicitZero, 0.0f, -0.75f, 0.0f, 60.0f, 0.0f, 2.0f);
    bool zeroProfileIsTransparent = true;
    for (int index = 0; index < 4096; ++index)
    {
        const float input = static_cast<float>(
            0.1 * std::sin(2.0 * pi * 173.0 * index / sampleRate));
        float explicitLeft = 0.0f;
        float explicitRight = 0.0f;
        float implicitLeft = 0.0f;
        float implicitRight = 0.0f;
        explicitZero.process(input, ChorusMode::One, 1.0f,
                             explicitLeft, explicitRight);
        implicitZero.process(input, ChorusMode::One, 1.0f,
                             implicitLeft, implicitRight);
        zeroProfileIsTransparent = zeroProfileIsTransparent
            && explicitLeft == implicitLeft && explicitRight == implicitRight;
    }
    expect(zeroProfileIsTransparent,
           "disabled optional chorus-noise components changed the render");

    // A single master still defeats independent, common, hum and clock-spur
    // hypotheses together.  The non-zero values below are synthetic fixtures,
    // explicitly not hardware calibration.
    Chorus masterMuted;
    masterMuted.prepare(sampleRate);
    YouKnow106TestAccess::configureOptionalChorusNoise(
        masterMuted, 0.01f, 0.4f, 0.01f, 50.0f, 0.01f, 1.0f);
    bool masterSilencesEverything = true;
    for (int index = 0; index < 2048; ++index)
    {
        float left = 0.0f;
        float right = 0.0f;
        masterMuted.process(0.0f, ChorusMode::Two, 0.0f, left, right);
        masterSilencesEverything = masterSilencesEverything
            && left == 0.0f && right == 0.0f;
    }
    expect(masterSilencesEverything,
           "noiseScale no longer mutes every declared chorus-noise component");

    // The common layer is built from one common and one orthogonal seeded
    // process. rho=+1 duplicates channels exactly; rho=-1 changes only sign.
    constexpr std::size_t syntheticLength = 2048;
    std::vector<float> positiveA(syntheticLength);
    std::vector<float> positiveB(syntheticLength);
    std::uint32_t commonOne = 0xd1b54a35u;
    std::uint32_t orthogonalOne = 0x94d049bbu;
    std::uint32_t commonReplay = commonOne;
    std::uint32_t orthogonalReplay = orthogonalOne;
    bool replayedExactly = true;
    bool duplicatedExactly = true;
    for (std::size_t index = 0; index < syntheticLength; ++index)
    {
        const auto sample = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonOne, orthogonalOne, 1.0f);
        const auto replay = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonReplay, orthogonalReplay, 1.0f);
        positiveA[index] = sample[0];
        positiveB[index] = sample[1];
        replayedExactly = replayedExactly
            && sample[0] == replay[0] && sample[1] == replay[1];
        duplicatedExactly = duplicatedExactly && sample[0] == sample[1];
    }
    expect(replayedExactly,
           "the synthetic common component did not replay from fixed seeds");
    expect(duplicatedExactly,
           "rho=+1 did not duplicate the synthetic common component");

    std::uint32_t commonNegative = 0xd1b54a35u;
    std::uint32_t orthogonalNegative = 0x94d049bbu;
    bool invertedExactly = true;
    for (std::size_t index = 0; index < syntheticLength; ++index)
    {
        const auto sample = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonNegative, orthogonalNegative, -1.0f);
        invertedExactly = invertedExactly && sample[1] == -sample[0];
    }
    expect(invertedExactly,
           "rho=-1 did not invert the synthetic common component");

    // Coherence is averaged over independent DFT blocks, rather than using a
    // single-block identity that would read one for any two non-zero vectors.
    constexpr int blockLength = 256;
    constexpr int blockCount = 8;
    constexpr int coherenceBin = 23;
    double autoA = 0.0;
    double autoB = 0.0;
    std::complex<double> cross {};
    for (int block = 0; block < blockCount; ++block)
    {
        std::complex<double> spectrumA {};
        std::complex<double> spectrumB {};
        for (int index = 0; index < blockLength; ++index)
        {
            const double angle = -2.0 * pi * coherenceBin * index / blockLength;
            const auto rotation = std::exp(std::complex<double>(0.0, angle));
            const auto offset = static_cast<std::size_t>(block * blockLength + index);
            spectrumA += static_cast<double>(positiveA[offset]) * rotation;
            spectrumB += static_cast<double>(positiveB[offset]) * rotation;
        }
        autoA += std::norm(spectrumA);
        autoB += std::norm(spectrumB);
        cross += spectrumA * std::conj(spectrumB);
    }
    const double coherence = std::norm(cross) / (autoA * autoB);
    expectNear(coherence, 1.0, 1.0e-12,
               "duplicated synthetic channels do not have coherence one");

    // Establish the A/B cross-spectrum convention explicitly.  Swapping the
    // line labels exchanges their individual spectra and conjugates A*conj(B).
    std::uint32_t commonMixed = 0x243f6a89u;
    std::uint32_t orthogonalMixed = 0xb7e15163u;
    std::vector<float> mixedA(4096);
    std::vector<float> mixedB(4096);
    for (std::size_t index = 0; index < mixedA.size(); ++index)
    {
        const auto sample = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonMixed, orthogonalMixed, 0.35f);
        mixedA[index] = sample[0];
        mixedB[index] = sample[1];
    }
    for (const int bin : { 19, 113, 509 })
    {
        std::complex<double> spectrumA {};
        std::complex<double> spectrumB {};
        for (std::size_t index = 0; index < mixedA.size(); ++index)
        {
            const double angle = -2.0 * pi * bin * index / mixedA.size();
            const auto rotation = std::exp(std::complex<double>(0.0, angle));
            spectrumA += static_cast<double>(mixedA[index]) * rotation;
            spectrumB += static_cast<double>(mixedB[index]) * rotation;
        }
        const auto originalCross = spectrumA * std::conj(spectrumB);
        const auto swappedCross = spectrumB * std::conj(spectrumA);
        const double originalPowerA = std::norm(spectrumA);
        const double originalPowerB = std::norm(spectrumB);
        const double swappedPowerA = std::norm(spectrumB);
        const double swappedPowerB = std::norm(spectrumA);
        expectNear(swappedPowerA, originalPowerB, 0.0,
                   "line swap changed B's individual spectrum");
        expectNear(swappedPowerB, originalPowerA, 0.0,
                   "line swap changed A's individual spectrum");
        expectNear(swappedCross.real(), std::conj(originalCross).real(), 1.0e-9,
                   "line swap changed cross-spectrum magnitude");
        expectNear(swappedCross.imag(), std::conj(originalCross).imag(), 1.0e-9,
                   "line swap did not conjugate the cross-spectrum");
    }

    // Hum and clock feedthrough are deterministic oscillators even though
    // their production amplitudes are zero and their frequencies are unknown.
    double tonePhase = 0.0;
    double replayPhase = 0.0;
    bool toneReplayed = true;
    double toneEnergy = 0.0;
    for (int index = 0; index < 2048; ++index)
    {
        const float tone = YouKnow106TestAccess::chorusToneStep(
            tonePhase, 997.0f, sampleRate);
        const float replay = YouKnow106TestAccess::chorusToneStep(
            replayPhase, 997.0f, sampleRate);
        toneReplayed = toneReplayed && tone == replay;
        toneEnergy += static_cast<double>(tone) * tone;
    }
    expect(toneReplayed && toneEnergy > 100.0,
           "the deterministic hum/clock-spur oscillator is not reproducible");
}

void testChorusBypassStateAndWetMuteTiming()
{
    constexpr float sampleRate = 48000.0f;
    constexpr int excitationLength = 6000;

    Chorus alwaysOn;
    Chorus bypassedForComparison;
    Chorus bypassedNaturally;
    alwaysOn.prepare(sampleRate);
    bypassedForComparison.prepare(sampleRate);
    bypassedNaturally.prepare(sampleRate);

    // Off uses the same running mode-I clock programme behind its wet shunts.
    // Feed all three instances identically while only one return is audible.
    for (int index = 0; index < excitationLength; ++index)
    {
        const float input = static_cast<float>(
            0.2 * std::sin(2.0 * pi * 311.0 * index / sampleRate));
        float left = 0.0f;
        float right = 0.0f;
        alwaysOn.process(input, ChorusMode::One, 0.0f, left, right);
        bypassedForComparison.process(
            input, ChorusMode::Off, 0.0f, left, right);
        bypassedNaturally.process(input, ChorusMode::Off, 0.0f, left, right);
    }

    // Remove only the mute-gain difference through the test seam. The wet
    // output capacitor legitimately evolved against 22 kOhm while bypassed
    // rather than 22 kOhm || 47 kOhm while connected, so the resumed samples
    // are no longer bit-identical. Their small error energy still proves the
    // BBDs, main support filters and modulation oscillator kept running.
    YouKnow106TestAccess::setChorusWetGain(
        bypassedForComparison,
        YouKnow106TestAccess::chorusWetGain(alwaysOn));
    double resumedEnergy = 0.0;
    double referenceEnergy = 0.0;
    double differenceEnergy = 0.0;
    for (int index = 0; index < 2048; ++index)
    {
        float onLeft = 0.0f;
        float onRight = 0.0f;
        float bypassedLeft = 0.0f;
        float bypassedRight = 0.0f;
        alwaysOn.process(0.0f, ChorusMode::One, 0.0f, onLeft, onRight);
        bypassedForComparison.process(
            0.0f, ChorusMode::One, 0.0f, bypassedLeft, bypassedRight);
        const double differenceLeft = bypassedLeft - onLeft;
        const double differenceRight = bypassedRight - onRight;
        differenceEnergy += differenceLeft * differenceLeft
                          + differenceRight * differenceRight;
        referenceEnergy += static_cast<double>(onLeft) * onLeft
                         + static_cast<double>(onRight) * onRight;
        resumedEnergy += static_cast<double>(bypassedLeft) * bypassedLeft
                       + static_cast<double>(bypassedRight) * bypassedRight;
    }
    expect(differenceEnergy < referenceEnergy * 0.01 + 1.0e-12,
           "chorus Off froze/reset core state instead of changing only the "
           "documented wet-output capacitor load");
    expect(resumedEnergy > 1.0e-8,
           "the evolved BBD state contained no resumable signal");

    // Exercise the actual unmute too: an evolved, previously bypassed line has
    // a tail as its gain rises, while a fresh line fed silence has none.
    Chorus fresh;
    fresh.prepare(sampleRate);
    double naturalEnergy = 0.0;
    double freshEnergy = 0.0;
    for (int index = 0; index < 2048; ++index)
    {
        float naturalLeft = 0.0f;
        float naturalRight = 0.0f;
        float freshLeft = 0.0f;
        float freshRight = 0.0f;
        bypassedNaturally.process(
            0.0f, ChorusMode::One, 0.0f, naturalLeft, naturalRight);
        fresh.process(0.0f, ChorusMode::One, 0.0f, freshLeft, freshRight);
        naturalEnergy += static_cast<double>(naturalLeft) * naturalLeft
                       + static_cast<double>(naturalRight) * naturalRight;
        freshEnergy += static_cast<double>(freshLeft) * freshLeft
                     + static_cast<double>(freshRight) * freshRight;
    }
    expect(naturalEnergy > freshEnergy + 1.0e-8,
           "re-enabling chorus restarted empty BBDs instead of evolved state");

    const auto tenToNinetySeconds = [](float rate) {
        Chorus chorus;
        chorus.prepare(rate);
        float left = 0.0f;
        float right = 0.0f;
        // Prime in Off so switching on is a player action and therefore glides.
        chorus.process(0.0f, ChorusMode::Off, 0.0f, left, right);
        const float target = Chorus::settingsFor(ChorusMode::One).wetGain;
        int tenPercent = -1;
        int ninetyPercent = -1;
        for (int index = 0; index < static_cast<int>(rate); ++index)
        {
            chorus.process(0.0f, ChorusMode::One, 0.0f, left, right);
            const float fraction = YouKnow106TestAccess::chorusWetGain(chorus)
                                 / target;
            if (tenPercent < 0 && fraction >= 0.1f)
                tenPercent = index;
            if (ninetyPercent < 0 && fraction >= 0.9f)
            {
                ninetyPercent = index;
                break;
            }
        }
        return static_cast<double>(ninetyPercent - tenPercent) / rate;
    };

    const double at48k = tenToNinetySeconds(48000.0f);
    const double at192k = tenToNinetySeconds(192000.0f);
    const double expected = Chorus::wetMuteTimeConstantSeconds * std::log(9.0);
    expectNear(at48k, expected, 2.0 / 48000.0,
               "wet-mute 10-90% time changed at 48 kHz");
    expectNear(at192k, expected, 2.0 / 192000.0,
               "wet-mute 10-90% time changed at 192 kHz");
    expectNear(at48k, at192k, 2.0 / 48000.0,
               "wet-mute timing is not sample-rate invariant");
}

void testChorusRateChangePreservesPhysicalState()
{
    Chorus chorus;
    chorus.prepare(192000.0);
    YouKnow106TestAccess::configureOptionalChorusNoise(
        chorus, 1.0e-4f, 0.3f, 1.0e-4f, 50.0f, 1.0e-4f, 1.0f);
    float left = 0.0f;
    float right = 0.0f;
    for (int sample = 0; sample < 8192; ++sample)
    {
        const float input = 0.2f * std::sin(
            static_cast<float>(2.0 * pi * 440.0 * sample / 192000.0));
        chorus.process(input, ChorusMode::One, 1.0f, left, right);
    }

    const auto before = YouKnow106TestAccess::chorusPhysicalState(chorus);
    double bucketEnergy = 0.0;
    for (const float value : before.cellsA)
        bucketEnergy += static_cast<double>(value) * value;
    expect(bucketEnergy > 1.0e-6,
           "chorus rate-change fixture never filled the physical BBD");

    chorus.prepare(48000.0, true);
    const auto after = YouKnow106TestAccess::chorusPhysicalState(chorus);
    expect(after == before,
           "a numerical sample-rate change power-cycled BBD/free-running state");
    expect(YouKnow106TestAccess::chorusAudioRateSupportIsClear(chorus),
           "a chorus rate change reused TPT carries expressed in the old timestep");
}

void testBucketBrigadeDatasheetAnchors()
{
    // Measure the static line transfer independently of the delay and support
    // filters. The two amplitudes are the MN3009 datasheet's own distortion
    // conditions, converted through the model's 2.6 V-per-unit node scale.
    const auto thdAt = [](double rmsVolts) {
        constexpr int samples = 32768;
        constexpr int cycles = 37;
        constexpr int lastHarmonic = 63;
        constexpr double voltsPerUnit = 2.6;
        const double peak = std::sqrt(2.0) * rmsVolts / voltsPerUnit;

        std::vector<float> output(samples);
        for (int index = 0; index < samples; ++index)
        {
            const double phase = 2.0 * pi * cycles * index / samples;
            output[static_cast<std::size_t>(index)] =
                YouKnow106TestAccess::bbdTransfer(
                    static_cast<float>(peak * std::sin(phase)));
        }

        double fundamental = 0.0;
        double distortionSquared = 0.0;
        for (int harmonic = 1; harmonic <= lastHarmonic; ++harmonic)
        {
            std::complex<double> accumulator {};
            for (int index = 0; index < samples; ++index)
            {
                const double phase = 2.0 * pi * cycles * harmonic * index / samples;
                accumulator += static_cast<double>(
                    output[static_cast<std::size_t>(index)])
                    * std::exp(std::complex<double>(0.0, -phase));
            }
            const double amplitude = 2.0 * std::abs(accumulator) / samples;
            if (harmonic == 1)
                fundamental = amplitude;
            else
                distortionSquared += amplitude * amplitude;
        }
        return std::sqrt(distortionSquared) / fundamental;
    };

    expectNear(thdAt(0.78), 0.003, 2.0e-4,
               "BBD distortion at the 0.78 Vrms datasheet condition");
    expectNear(thdAt(1.5), 0.025, 1.0e-3,
               "BBD distortion at the 1.5 Vrms input-swing condition");

    // The datasheet's -3 dB response at 12 kHz / 40 kHz describes the complete
    // held-output device. The rendered line already supplies the rectangular
    // zero-order hold, whose aperture contributes sinc(f/f_clock), so the
    // charge-transfer pole must supply only the residual loss. Measure that
    // pole as it is actually stepped and combine it with the independent ZOH
    // aperture; applying -3 dB to both mechanisms would fail at about -4.33 dB.
    constexpr double clockRate = 40000.0;
    constexpr double frequency = 12000.0;
    constexpr int settle = 2048;
    constexpr int window = 10000; // exactly 3000 cycles
    float transferState = 0.0f;
    std::complex<double> accumulator {};
    for (int index = 0; index < settle + window; ++index)
    {
        const double phase = 2.0 * pi * frequency * index / clockRate;
        const float output = YouKnow106TestAccess::transferLossStep(
            transferState, static_cast<float>(std::sin(phase)));
        if (index >= settle)
            accumulator += static_cast<double>(output)
                         * std::exp(std::complex<double>(0.0, -phase));
    }
    const double transferGain = 2.0 * std::abs(accumulator) / window;
    const double ratio = frequency / clockRate;
    const double heldAperture = std::abs(std::sin(pi * ratio) / (pi * ratio));
    expectNear(20.0 * std::log10(transferGain * heldAperture), -3.0, 0.01,
               "charge-transfer loss double-counts the held-output aperture");
}

// A filter coefficient is only right in company with the update it is used
// with. Asserting the formula would prove nothing, so this measures where the
// corner of the pairing the line actually runs ends up, at the two rates the
// engine uses. The mismatched pairing this catches put the 9.9 kHz corner at
// 4.6 kHz -- inaudible as a bug report, obvious as a dull chorus.
// The high-pass as the signal actually meets it, not as a table of laws.
//
// This exists because the stage was moved from inside each voice to the summed
// bus and every other check in the suite passed either way: the laws are pure
// functions and stayed true, so nothing was guarding that the filter was still
// reached at all. A stage wired to nothing would have gone unnoticed.
void testHighPassReachesTheSummedSignal()
{
    const auto rmsFor = [](HighPassMode mode, int note) {
        YouKnow106Engine engine;
        engine.prepare(48000.0, 512, false);
        EngineParameters parameters;
        parameters.highPass = mode;
        parameters.cutoff = 1.0f;      // filter wide open, so the high-pass is
        parameters.resonance = 0.0f;   // the only thing shaping the band
        parameters.attack = 0.0f;
        parameters.sustain = 1.0f;
        parameters.calibration = 0.0f; // no dispersion, so this is repeatable
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(note, 0.9f);

        std::vector<float> left(24000), right(24000);
        engine.process(left.data(), right.data(), static_cast<int>(left.size()));
        double sum = 0.0;
        const std::size_t half = left.size() / 2;
        for (std::size_t index = half; index < left.size(); ++index)
            sum += static_cast<double>(left[index]) * left[index];
        return std::sqrt(sum / static_cast<double>(left.size() - half));
    };

    // A low note, where every leg of the network has something to act on.
    const double flatLow = rmsFor(HighPassMode::One, 28);
    expect(flatLow > 1.0e-3, "the flat leg rendered nothing to measure");

    const double boostLow = rmsFor(HighPassMode::Boost, 28);
    const double cutTwoLow = rmsFor(HighPassMode::Two, 28);
    const double cutThreeLow = rmsFor(HighPassMode::Three, 28);

    expect(boostLow > flatLow * 1.2,
           "the boost leg did not lift a low note above the flat leg");
    expect(cutTwoLow < flatLow * 0.8,
           "the middle cut leg did not attenuate a low note");
    expect(cutThreeLow < cutTwoLow,
           "the top cut leg is not darker than the middle one on a low note");

    // And the ordering has to come from the corner rather than from a gain
    // trim, so it must fade as the note rises above the corners.
    const double flatHigh = rmsFor(HighPassMode::One, 76);
    const double cutThreeHigh = rmsFor(HighPassMode::Three, 76);
    expect(flatHigh > 1.0e-3, "the flat leg rendered nothing at the top");
    expect(cutThreeHigh > flatHigh * 0.6,
           "the top cut leg attenuates a high note as if it were a level control");
    expect(cutThreeLow / flatLow < cutThreeHigh / flatHigh,
           "the high-pass cuts a high note as hard as a low one");
}

void testSupportFilterCornersLandWhereAsked()
{
    const auto gainAt = [](double frequency, float g, float sampleRate) {
        // A whole number of cycles in the window, so the correlation is exact
        // and no leakage creeps into the estimate.
        constexpr int cycles = 64;
        constexpr int settle = 4096;
        const int window = static_cast<int>(
            std::llround(cycles * static_cast<double>(sampleRate) / frequency));
        float state = 0.0f;
        std::complex<double> accumulator {};
        for (int index = 0; index < settle + window; ++index)
        {
            const double phase = 2.0 * pi * frequency * index / sampleRate;
            const float output = Chorus::supportFilterStep(
                state, static_cast<float>(std::sin(phase)), g);
            if (index >= settle)
                accumulator += static_cast<double>(output)
                             * std::exp(std::complex<double>(0.0, -phase));
        }
        return 2.0 * std::abs(accumulator) / window;
    };

    const auto highPassGainAt = [](double frequency, float g, float sampleRate) {
        constexpr int cycles = 64;
        constexpr int settle = 4096;
        const int window = static_cast<int>(
            std::llround(cycles * static_cast<double>(sampleRate) / frequency));
        float state = 0.0f;
        std::complex<double> accumulator {};
        for (int index = 0; index < settle + window; ++index)
        {
            const double phase = 2.0 * pi * frequency * index / sampleRate;
            const float input = static_cast<float>(std::sin(phase));
            const float low = Chorus::supportFilterStep(state, input, g);
            if (index >= settle)
                accumulator += static_cast<double>(input - low)
                             * std::exp(std::complex<double>(0.0, -phase));
        }
        return 2.0 * std::abs(accumulator) / window;
    };

    // A lowpass passes direct current untouched, whatever its corner. Checking
    // it separately means the corner search below can be normalised against an
    // exact figure rather than another measurement.
    {
        const float g = Chorus::onePoleG(9900.0f, 192000.0f);
        float state = 0.0f;
        float settled = 0.0f;
        for (int index = 0; index < 8192; ++index)
            settled = Chorus::supportFilterStep(state, 1.0f, g);
        expectNear(settled, 1.0, 1.0e-5, "the support filter does not pass DC");
    }

    // The two newly explicit single-pole networks are checked through the
    // coefficients supportChainFor() actually installs, so accidentally
    // leaving either field at its default cannot hide behind onePoleG's own
    // unit checks. C44/R120 is a wet-only high-pass; the BBD tap-summing node
    // is a low-pass under the documented ideal-source approximation.
    {
        const auto chain = Chorus::supportChainFor(48000.0f);
        expectNear(highPassGainAt(15.9155, chain.inputCouplingG, 48000.0f),
                   0.70710678, 0.005,
                   "the wet coupling high-pass is not at C44/R120's corner");
        constexpr double capacitor = 1.0e-6;
        constexpr double bleed = 22000.0;
        constexpr double mixer = 47000.0;
        const double connectedResistance = bleed * mixer / (bleed + mixer);
        const double mutedCorner = 1.0 / (2.0 * pi * capacitor * bleed);
        const double connectedCorner =
            1.0 / (2.0 * pi * capacitor * connectedResistance);
        expectNear(Chorus::wetOutputCouplingCornerHz(false), mutedCorner,
                   1.0e-5, "muted wet-output coupling corner");
        expectNear(Chorus::wetOutputCouplingCornerHz(true), connectedCorner,
                   1.0e-5, "engaged wet-output coupling corner");
        expectNear(highPassGainAt(
                       mutedCorner, chain.wetOutputCouplingMutedG, 48000.0f),
                   0.70710678, 0.005,
                   "muted C28/C25 wet-output pole is misplaced");
        expectNear(highPassGainAt(
                       connectedCorner,
                       chain.wetOutputCouplingConnectedG, 48000.0f),
                   0.70710678, 0.005,
                   "engaged C28/C25 wet-output pole is misplaced");
    }
    {
        const auto chain = Chorus::supportChainFor(192000.0f);
        expectNear(gainAt(23461.38, chain.idealSourceTapPoleG, 192000.0f),
                   0.70710678, 0.005,
                   "the BBD tap-summing pole is not at its nominal corner");
    }

    const auto measureCorner = [&gainAt](float requested, float sampleRate) {
        const float g = Chorus::onePoleG(requested, sampleRate);
        // Bisect for the half-power point rather than assuming the shape. The
        // lower bound is far below any corner the circuit uses; a filter that
        // converged onto it would fail the assertion, which is the point.
        double low = 200.0;
        double high = sampleRate * 0.45;
        for (int step = 0; step < 40; ++step)
        {
            const double middle = 0.5 * (low + high);
            if (gainAt(middle, g, sampleRate) > 0.70710678)
                low = middle;
            else
                high = middle;
        }
        return 0.5 * (low + high);
    };

    // The internal rate the engine runs the chorus at, and the lowest host rate
    // it accepts -- where the prewarping matters most.
    expectNear(measureCorner(9900.0f, 192000.0f), 9900.0, 60.0,
               "anti-alias corner at the internal rate");
    expectNear(measureCorner(9500.0f, 192000.0f), 9500.0, 60.0,
               "reconstruction corner at the internal rate");
    expectNear(measureCorner(9900.0f, 48000.0f), 9900.0, 60.0,
               "one-pole corner without oversampling");

    // The two-pole sections either side of the line. Their Q comes from the
    // capacitor ratio alone, so it is asserted from the parts rather than
    // written down -- and the corner is measured the same way the one-pole's
    // is, by bisecting the realised half-power point, because a coefficient
    // that agrees with its own recursion is the only thing worth checking.
    expectNear(Chorus::sallenKeyQ(820.0e-12f, 680.0e-12f), 0.5494, 1.0e-3,
               "the first section's Q is not its capacitor ratio");
    expectNear(Chorus::sallenKeyQ(1.8e-9f, 270.0e-12f), 1.2910, 1.0e-3,
               "the second section's Q is not its capacitor ratio");

    const auto biquadGainAt = [](double frequency, const Chorus::BiquadCoefficients& c,
                                 float sampleRate) {
        constexpr int cycles = 64;
        constexpr int settle = 4096;
        const int window = static_cast<int>(
            std::llround(cycles * static_cast<double>(sampleRate) / frequency));
        Chorus::BiquadState state {};
        std::complex<double> accumulator {};
        for (int index = 0; index < settle + window; ++index)
        {
            const double phase = 2.0 * pi * frequency * index / sampleRate;
            const float output = Chorus::biquadStep(
                state, static_cast<float>(std::sin(phase)), c);
            if (index >= settle)
                accumulator += static_cast<double>(output)
                             * std::exp(std::complex<double>(0.0, -phase));
        }
        return 2.0 * std::abs(accumulator) / window;
    };

    // A two-pole lowpass has one property that pins both of its coefficients at
    // once: its gain *at* the corner is exactly Q. Asserting that is sharper
    // than bisecting for a half-power point, because the half-power point moves
    // with Q and so would pass for a section whose damping was wrong in a way
    // that happened to shift the -3 dB frequency back.
    //
    // A section running at half its intended Q -- which is what an extra factor
    // of two in the damping term produces -- fails this by a factor of two.
    for (const float rate : { 192000.0f, 48000.0f })
    {
        struct Section { float hz; float q; const char* name; };
        for (const auto& section : { Section { 9688.0f, 0.5494f, "first" },
                                     Section { 10377.0f, 1.2910f, "second" } })
        {
            const auto c = Chorus::sallenKeyCoefficients(section.hz, section.q, rate);
            const double reference = biquadGainAt(20.0, c, rate);
            const double atCorner = biquadGainAt(section.hz, c, rate);
            expectNear(atCorner / reference, section.q, 0.02,
                       std::string("the ") + section.name
                           + " two-pole section's gain at its corner is not its Q");
        }
    }

    // And a two-pole section passes DC like any other lowpass.
    {
        const auto c = Chorus::sallenKeyCoefficients(9688.0f, 0.5494f, 192000.0f);
        Chorus::BiquadState state {};
        float settled = 0.0f;
        for (int index = 0; index < 8192; ++index)
            settled = Chorus::biquadStep(state, 1.0f, c);
        expectNear(settled, 1.0, 1.0e-5, "a two-pole section does not pass DC");
    }
}

void testCorrectionResidualsVanishAtTheEdges()
{
    // The bandlimiting residuals are built by integration at construction, so
    // the cheapest way to catch a broken table is to confirm a rendered ramp
    // has the harmonic series a ramp should have and nothing else. The engine
    // suite measures the alias floor; here we only confirm the oscillator is
    // producing a ramp at the pitch the note timer was programmed for.
    YouKnow106Engine engine;
    engine.prepare(192000.0, 512, false);

    EngineParameters parameters;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = false;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.envDepth = 0.0f;
    parameters.keyFollow = 0.0f;
    parameters.attack = 0.0f;
    parameters.sustain = 1.0f;
    parameters.vcaLevel = 1.0f;
    parameters.volume = 1.0f;
    parameters.calibration = 0.0f;
    parameters.chorus = ChorusMode::Off;
    engine.setParameters(parameters);
    engine.noteOn(69, 1.0f);

    constexpr int blockSize = 512;
    constexpr int blocks = 192;
    std::vector<float> left(blockSize * blocks);
    std::vector<float> right(blockSize * blocks);
    for (int block = 0; block < blocks; ++block)
        engine.process(left.data() + block * blockSize,
                       right.data() + block * blockSize, blockSize);

    // Measure between the first and last rising crossing rather than counting
    // them over a fixed window: the count alone only resolves to one cycle.
    std::size_t first = 0;
    std::size_t last = 0;
    int intervals = -1;
    for (std::size_t index = left.size() / 2 + 1; index < left.size(); ++index)
        if (left[index - 1] <= 0.0f && left[index] > 0.0f)
        {
            if (intervals < 0)
                first = index;
            last = index;
            ++intervals;
        }
    expect(intervals > 50, "the oscillator produced too few cycles to measure");
    const double measured = intervals > 0
        ? intervals * 192000.0 / static_cast<double>(last - first) : 0.0;
    const double programmed = YouKnow106Engine::dcoQuantisedFrequency(
        YouKnow106Engine::dcoDivider(440.0), DcoRange::Eight);
    expectNear(measured, programmed, 0.5,
               "the rendered ramp is not at the frequency the timer was given");
}
void testOutputSummerIsLinearBelowItsRails()
{
    // IC6 runs on +/-15 V and the audio it carries is a few volts, so the stage
    // must be numerically linear there and bend only as it nears the rail. A
    // tanh cannot do this: its distortion rises as (V/asymptote)^2 from zero,
    // which is what put roughly 0.3% third harmonic on every sample.
    const auto thirdHarmonicFraction = [](double peakVolts) {
        constexpr int points = 4096;
        const double amplitude =
            peakVolts / static_cast<double>(YouKnow106Engine::internalVoltsPerUnit);
        double first = 0.0;
        double third = 0.0;
        for (int index = 0; index < points; ++index)
        {
            const double angle = 2.0 * 3.14159265358979323846
                               * static_cast<double>(index) / points;
            const double output = YouKnow106Engine::outputSummerClip(
                static_cast<float>(amplitude * std::cos(angle)));
            first += output * std::cos(angle);
            third += output * std::cos(3.0 * angle);
        }
        return std::abs(third) / std::max(std::abs(first), 1.0e-18);
    };

    // The declared nominal internal coordinate, and a hot passage above it.
    expect(thirdHarmonicFraction(2.6) < 5.0e-4,
           "the output summer distorts at its own nominal level");
    expect(thirdHarmonicFraction(5.0) < 5.0e-3,
           "the output summer distorts well below its rails");

    // It must still be a bound: no input may drive the stage past the rail.
    constexpr float rail = YouKnow106Engine::outputSummerRailVolts
                         / YouKnow106Engine::internalVoltsPerUnit;
    for (const float drive : { 10.0f, 100.0f, 1.0e6f })
    {
        expect(std::abs(YouKnow106Engine::outputSummerClip(drive)) <= rail,
               "the output summer swung past its supply rail");
        expect(std::abs(YouKnow106Engine::outputSummerClip(-drive)) <= rail,
               "the output summer swung past its negative supply rail");
    }
    expect(std::isfinite(YouKnow106Engine::outputSummerClip(
               std::numeric_limits<float>::max())),
           "the output summer did not survive an extreme finite input");

    // Odd symmetry: the summer has no offset to add.
    expectNear(YouKnow106Engine::outputSummerClip(3.0f),
               -YouKnow106Engine::outputSummerClip(-3.0f), 1.0e-6,
               "the output summer is not symmetric");
}

void testFilterDriveMatchesTheDerivedBudget()
{
    // Working back from Roland's own ADJUSTMENT table gives a drive figure the
    // model can be held to. The VCA GAIN step sets 4.8 Vp-p at the VCF output
    // against 6 Vp-p at the VCA output, and the mixer node reaches pin 1 with
    // no series attenuator, so a full-level source arrives at about +/-2.4 V.
    // The IR3109's own internal 560 Ohm divider then refers that to the
    // differential pair as +/-19.6 mV -- about 2.8 times the AS662D's 0.25%
    // THD reference, which is why the pairs sit at the edge of their linear
    // region rather than deep in saturation.
    //
    // This is a check on a *voiced* constant, `filterInputAttenuation`, which
    // turns out to land on the derived budget to the digit. It is asserted so
    // that it cannot drift away from it unnoticed.
    constexpr double sourceVolts = 6.0;          // sawMixVolts
    constexpr double filterInputAttenuation = 0.40;
    const double atFilterInput = sourceVolts * filterInputAttenuation;
    expectNear(atFilterInput, 2.4, 1.0e-9,
               "a full-level source no longer arrives at the derived +/-2.4 V");

    const double atDifferentialPair =
        atFilterInput * YouKnow106TestAccess::stageAttenuation();
    expectNear(atDifferentialPair, 0.0196, 5.0e-5,
               "the transconductor pair is not driven at the derived 19.6 mV");

    // And the pair's own linear span, which is what makes that a soft edge
    // rather than a hard one: 2 kT/q referred through the same divider.
    expectNear(YouKnow106TestAccess::otaHeadroomVolts(), 2.0 * 0.026
                   / (560.0 / 68560.0), 1.0e-4,
               "the transconductor headroom is no longer 2 Vt over the "
               "IR3109's own input divider");
    expect(atDifferentialPair < 0.5 * 2.0 * 0.026,
           "the differential pair is driven past its own thermal span");
}

void testDecimatorProtectsTheTopOfTheBand()
{
    // The last decimation stage runs at twice the host rate, so everything it
    // fails to remove around the host rate lands back inside the audio band.
    // A 44.1 kHz host is the hard case: 20 kHz sits at 0.227 of the stage's
    // own rate, only just below the quarter-rate crossover, and the content
    // that folds onto it comes from just above.
    const auto kernel = YouKnow106TestAccess::halfbandKernel();
    expect(kernel.size() == 63, "the decimation kernel is not 63 taps");

    const auto response = [&kernel](double hertz, double stageRate) {
        std::complex<double> accumulator {};
        const double omega = 2.0 * pi * hertz / stageRate;
        for (std::size_t tap = 0; tap < kernel.size(); ++tap)
            accumulator += static_cast<double>(kernel[tap])
                         * std::exp(std::complex<double>(
                               0.0, -omega * static_cast<double>(tap)));
        return 20.0 * std::log10(std::max(std::abs(accumulator), 1.0e-30));
    };

    expectNear(response(0.0, 96000.0), 0.0, 1.0e-6,
               "the decimation kernel does not have unity gain at DC");

    struct HostCase { double host; double passbandDb; double foldDb; };
    // Both bounds are what this kernel measures, with a little margin. The
    // Blackman-Harris design they replace could not meet either at 44.1 kHz:
    // it was 0.85 dB down at 20 kHz and rejected the fold onto 19.1 kHz by
    // only 31.7 dB.
    constexpr std::array<HostCase, 2> cases {{
        { 44100.0, -0.6, -44.0 },
        { 48000.0, -0.1, -78.0 }
    }};
    for (const auto& host : cases)
    {
        const double stageRate = 2.0 * host.host;
        for (double hertz = 0.0; hertz <= 20000.0; hertz += 250.0)
        {
            const double gain = response(hertz, stageRate);
            expect(gain <= 0.05 && gain >= host.passbandDb,
                   "the decimator is " + std::to_string(gain)
                       + " dB at " + std::to_string(hertz) + " Hz on a "
                       + std::to_string(static_cast<int>(host.host))
                       + " Hz host");
        }
        // Content folding onto 19.1 kHz arrives from either side of the host
        // rate, and both images have to be rejected.
        for (const double source : { host.host - 19100.0, host.host + 19100.0 })
            expect(response(source, stageRate) < host.foldDb,
                   "content at " + std::to_string(source)
                       + " Hz folds onto 19.1 kHz at only "
                       + std::to_string(response(source, stageRate)) + " dB");
    }
}
} // namespace

int main()
{
    testOutputSummerIsLinearBelowItsRails();
    testDecimatorProtectsTheTopOfTheBand();
    testFilterDriveMatchesTheDerivedBudget();
    testCascadeAgainstReferenceSolve();
    testCascadeOscillationThreshold();
    testCascadeSurvivesAdversarialControl();
    testNoteTimerLaw();
    testCutoffControlLaw();
    testStoredControlDigitalVectors();
    testVoicedResonanceCompatibilityProfile();
    testEnvelopeAndAmplifierLaws();
    testPulseWidthAndHighPassLaws();
    testModulationAndGlideLaws();
    testConverterQueueAndOutputReference();
    testPanelLawsInvert();
    testComparatorEdgesSitOnOneThreshold();
    testChorusIsAtItsSettingFromTheFirstSample();
    testJuno60FallbackBucketBrigadeTiming();
    testChorusNoiseComponents();
    testChorusBypassStateAndWetMuteTiming();
    testChorusRateChangePreservesPhysicalState();
    testBucketBrigadeDatasheetAnchors();
    testSupportFilterCornersLandWhereAsked();
    testHighPassReachesTheSummedSignal();
    testCorrectionResidualsVanishAtTheEdges();
    
    // SOTA physical modeling tests
    {
        YouKnow106TestAccess::Cascade cascade;
        cascade.reset();
        cascade.offsetVoltage = { 0.0020f, -0.0015f, 0.0018f, -0.0010f };

        float positiveSum = 0.0f;
        float negativeSum = 0.0f;
        for (int i = 0; i < 1000; ++i)
        {
            float in = 2.0f * std::sin(2.0f * 3.141592653589793f * static_cast<float>(i) / 100.0f);
            float out = cascade.process(in, 0.5f, 2.0f);
            if (!std::isfinite(out))
            {
                std::cerr << "FAIL: OtaCascade output not finite with stage offsets\n";
                ++failures;
                break;
            }
            if (out > 0.0f) positiveSum += out;
            else negativeSum += std::abs(out);
        }
        if (std::abs(positiveSum - negativeSum) <= 1.0e-4f)
        {
            std::cerr << "FAIL: OtaCascade stage offsets did not produce asymmetric response\n";
            ++failures;
        }

        // Test Op-Amp Slew-Rate Limiting
        {
            EngineParameters params;
            params.enableOpAmpSlewLimiting = true;
            YouKnow106Engine engine;
            engine.prepare(44100.0, 256, true);
            engine.setParameters(params);

            std::vector<float> left(256, 0.0f);
            std::vector<float> right(256, 0.0f);
            engine.noteOn(72, 1.0f);
            engine.process(left.data(), right.data(), 256);

            for (int i = 1; i < 256; ++i)
            {
                if (!std::isfinite(left[i]) || !std::isfinite(right[i]))
                {
                    std::cerr << "FAIL: Op-Amp slew limiting output not finite\n";
                    ++failures;
                    break;
                }
            }
        }

    }

    if (failures != 0)
    {
        std::cerr << failures << " YouKnow106 circuit check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All YouKnow106 circuit checks passed.\n";
    return EXIT_SUCCESS;
}
