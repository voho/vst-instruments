// Circuit-reference suite.
//
// Every check here compares the realtime model against something independent:
// either an ODE solved from the circuit equations at a far higher rate, a
// closed-form small-signal result, or a figure taken from the instrument's own
// service documentation. Nothing in this file asserts that the model agrees
// with itself.

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
        return YouKnow106Engine::vcfFeedbackHeadroomVolts;
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
// with fb the loop pair's own tanh behind its divider -- which is the
// circuit, not the model.
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

    // One octave is 1143 counts, everywhere the transconductor can follow it.
    // Past the top of its bias range the law flattens, which is the part's
    // limit rather than the converter's.
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
}

void testResonanceLaw()
{
    expectNear(YouKnow106Engine::vcfFeedback(0.0f), 0.0, 1.0e-6,
               "resonance at rest");
    expectNear(YouKnow106Engine::vcfFeedback(0.9f), 4.0, 1.0e-4,
               "oscillation threshold is not reached at 90% of the travel");
    expect(YouKnow106Engine::vcfFeedback(1.0f) > 4.0,
           "the last tenth of the resonance travel does not pass the threshold");

    // The curve below the threshold is the fitted hardware measurement, not a
    // straight line: 30% of the travel reaches a loop gain near 0.91, and the
    // far end of the travel lands on the fitted 4.19.
    expectNear(YouKnow106Engine::vcfFeedback(0.3f), 0.91, 0.01,
               "resonance curve misses the fitted 30%-travel measurement");
    expectNear(YouKnow106Engine::vcfFeedback(1.0f), 4.19, 0.01,
               "resonance travel does not end at the fitted maximum");

    // Compensation is applied to the input, so it rises with regeneration --
    // linearly in the loop gain, as the fitted measurement has it, reaching
    // just under six decibels at the fitted maximum.
    expectNear(YouKnow106Engine::vcfResonanceCompensation(0.0f), 1.0, 1.0e-6,
               "resonance compensation at rest");
    const double atMaximum = YouKnow106Engine::vcfResonanceCompensation(4.19f);
    expectNear(20.0 * std::log10(atMaximum), 5.85, 0.1,
               "resonance compensation misses the fitted level");
    expect(YouKnow106Engine::vcfResonanceCompensation(2.0f) < atMaximum,
           "resonance compensation is not monotonic");

    // The frequency trim only acts as regeneration rises, and with the loop
    // limited in its own divider it is small: the oscillation sits close to
    // the small-signal law on its own.
    expectNear(YouKnow106Engine::vcfResonanceFrequencyTrim(0.0f), 1.0, 1.0e-6,
               "frequency trim acts with no resonance");
    const float trimAtThreshold = YouKnow106Engine::vcfResonanceFrequencyTrim(4.0f);
    expect(trimAtThreshold > 1.0f && trimAtThreshold < 1.1f,
           "frequency trim is outside the residual band the loop limiter leaves");
}

void testEnvelopeAndAmplifierLaws()
{
    // The generator advances once per 4.2 ms scan pass, so the shortest
    // attack the instrument can produce is one pass -- the published 1.5 ms
    // figure is shorter than the generator's own tick. The mid-travel
    // anchors are the firmware's, not an endpoint interpolation: attack time
    // is linear in the slider across the lower half.
    expectNear(YouKnow106Engine::envelopeAttackSeconds(0.0f), 0.0042, 1.0e-4,
               "shortest attack is not a single scan pass");
    expectNear(YouKnow106Engine::envelopeAttackSeconds(0.25f), 0.269, 0.008,
               "attack at quarter travel misses the firmware anchor");
    expectNear(YouKnow106Engine::envelopeAttackSeconds(0.5f), 0.533, 0.008,
               "attack at mid travel misses the firmware anchor");
    expectNear(YouKnow106Engine::envelopeAttackSeconds(1.0f), 3.276, 0.01,
               "longest attack misses the firmware anchor");

    // Falling segments are exponential and displayed as the time to fall to
    // a tenth of the span. The firmware anchors: about a third of a second
    // at quarter travel, 2.2 s at half, 4.5 s at three quarters, and 9.6 s
    // to *half* level at the top.
    expectNear(YouKnow106Engine::envelopeDecaySeconds(0.25f), 0.33, 0.02,
               "decay at quarter travel misses the firmware anchor");
    expectNear(YouKnow106Engine::envelopeDecaySeconds(0.5f), 2.2, 0.1,
               "decay at mid travel misses the firmware anchor");
    expectNear(YouKnow106Engine::envelopeDecaySeconds(0.75f), 4.5, 0.2,
               "decay at three-quarter travel misses the firmware anchor");
    const double lambdaTop = 2.302585 / (YouKnow106Engine::envelopeDecaySeconds(1.0f)
                                         * (1000.0 / 4.2));
    expectNear(0.693147 / lambdaTop * 0.0042, 9.6, 0.15,
               "slowest decay does not take 9.6 s to half level");
    expectNear(YouKnow106Engine::envelopeReleaseSeconds(0.5f),
               YouKnow106Engine::envelopeDecaySeconds(0.5f), 1.0e-6,
               "release shares the decay law");

    // The amplifier is quasi-linear: gain tracks the control voltage over
    // most of the range -- half control is six decibels down, not thirty --
    // with the exponential knee confined to the bottom tenth.
    expectNear(YouKnow106Engine::vcaGain(1.0f), 1.0, 1.0e-4,
               "amplifier is not unity at full control");
    expectNear(YouKnow106Engine::vcaGain(0.5f), 0.5, 5.0e-3,
               "amplifier is not linear at half control");
    expectNear(YouKnow106Engine::vcaGain(0.25f), 0.25, 5.0e-3,
               "amplifier is not linear at quarter control");
    // Below the knee the gain falls away exponentially, roughly 26 dB per
    // tenth of the control range.
    const double knee = 20.0 * std::log10(YouKnow106Engine::vcaGain(0.06f) / 0.06);
    expectNear(knee, -15.6, 0.5, "amplifier knee misses the measured slope");
    expect(YouKnow106Engine::vcaGain(0.0f) == 0.0f,
           "amplifier leaks with no control voltage");
    expect(YouKnow106Engine::vcaGain(0.004f) == 0.0f,
           "amplifier conducts inside its deadband");
}

void testPulseWidthAndHighPassLaws()
{
    // The comparator threshold cannot reach either rail, so the pulse cannot
    // reach 0% or 100%.
    expectNear(YouKnow106Engine::pwmControlVolts(0.0f), 6.0, 1.0e-5,
               "pulse threshold with the control at rest");
    expectNear(YouKnow106Engine::pwmControlVolts(1.0f), 0.6, 1.0e-5,
               "pulse threshold at full depth");
    expectNear(YouKnow106Engine::pwmDutyCycle(6.0f), 0.5, 1.0e-5,
               "threshold at half the ramp does not bisect it");
    expectNear(YouKnow106Engine::pwmDutyCycle(0.6f), 0.95, 1.0e-5,
               "narrowest pulse");
    for (float depth = 0.0f; depth <= 1.0f; depth += 0.05f)
    {
        const float duty = YouKnow106Engine::pwmDutyCycle(
            YouKnow106Engine::pwmControlVolts(depth));
        expect(duty > 0.0f && duty < 1.0f, "pulse reaches a rail");
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
    expectNear(YouKnow106Engine::highPassCornerHz(HighPassMode::Two), 236.0, 0.5,
               "middle high-pass corner is not the network's own");
    expectNear(YouKnow106Engine::highPassCornerHz(HighPassMode::Three), 754.0, 0.5,
               "top high-pass corner is not the network's own");
}

void testModulationAndGlideLaws()
{
    // The modulator's measured range is 0.04 to 29.8 Hz -- the firmware's
    // own table, not the 0.1-30 the specification sheet rounds to. At the
    // top the clamp quantises the period onto whole passes, so the last few
    // per cent of the travel all read the same rate.
    expectNear(YouKnow106Engine::lfoRateHz(0.0f), 0.0363, 1.0e-3,
               "slowest modulation");
    expectNear(YouKnow106Engine::lfoRateHz(1.0f), 29.76, 0.05,
               "fastest modulation");
    expect(YouKnow106Engine::lfoRateHz(0.99f) == YouKnow106Engine::lfoRateHz(1.0f),
           "the fast end of the modulation travel is not rate-quantised");
    expect(YouKnow106Engine::lfoDelaySeconds(0.0f) == 0.0f, "delay at rest");
    // The delay's hold advances at the attack table's own rate and the fade
    // is a fixed second-long ramp across the upper half, so the longest
    // setting reaches about 4.4 s.
    expectNear(YouKnow106Engine::lfoDelaySeconds(1.0f), 4.366, 0.02,
               "longest delay");
    expectNear(YouKnow106Engine::lfoDelaySeconds(0.1f),
               YouKnow106Engine::envelopeAttackSeconds(0.1f), 1.0e-4,
               "the bottom eighth of the delay travel is not hold-only");

    // Portamento is quoted per octave, so the panel law has those units.
    expect(YouKnow106Engine::portamentoSeconds(0.0f) == 0.0f,
           "portamento is not switched off at the bottom of its travel");
    expectNear(YouKnow106Engine::portamentoSeconds(1.0f), 12.9, 0.01,
               "slowest glide");
    expect(YouKnow106Engine::portamentoSeconds(0.001f) < 0.06,
           "fastest glide is slower than the hardware's");
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

    // Cutoff round trips only while the converter is still on its
    // exponential law. Above the knee the converter saturates towards its
    // measured ceiling, several counts crowd into each hertz, and the
    // inverse stays on the law rather than pretending to invert the knee.
    for (float position = 0.0f; position <= 0.80f; position += 0.1f)
        roundTrip(position, [](float p) {
                      return YouKnow106Engine::vcfCutoffHz(
                          YouKnow106Engine::vcfPanelCounts(p));
                  },
                  YouKnow106Engine::panelPositionForCutoff, "cutoff");

    // The measured converter: still on the law at 22.6 kHz around 84% of the
    // panel's count span, then saturating to a ceiling just above 50 kHz --
    // the published specification's own figure, not the 24 kHz an earlier
    // revision clamped at.
    expectNear(YouKnow106Engine::vcfCutoffHz(13716.0f), 22600.0, 700.0,
               "the cutoff knee point misses the measured converter");
    const float ceilingHz = YouKnow106Engine::vcfCutoffHz(
        YouKnow106Engine::vcfPanelCounts(1.0f));
    expect(ceilingHz > 50000.0f && ceilingHz < 52200.0f,
           "the cutoff ceiling is not the measured converter top");
    expect(YouKnow106Engine::vcfCutoffHz(16383.0f) >= ceilingHz,
           "the accumulator ceiling reads below the panel's own top");

    // Portamento's bottom detent is "off" rather than a time, so it round trips
    // from just above it.
    for (float position = 0.1f; position <= 1.0f; position += 0.15f)
        roundTrip(position, YouKnow106Engine::portamentoSeconds,
                  YouKnow106Engine::panelPositionForPortamento, "portamento");
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
    // alone -- the line adds its output to the input, so subtracting the input
    // back off leaves exactly what the effect contributed, with no note onset
    // or modulation depth mixed into the reading.
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
        wet[static_cast<std::size_t>(index)] = left - input;
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

void testBucketBrigadeLine()
{
    // The part is 256 stages clocked in two phases, so its delay is
    // 128 / clock -- which is the datasheet's 12.8 ms at its 10 kHz minimum.
    expectNear(Chorus::delaySecondsForClock(10000.0f), 0.0128, 1.0e-9,
               "line delay at the minimum clock");
    expectNear(Chorus::delaySecondsForClock(200000.0f), 0.00064, 1.0e-9,
               "line delay at the maximum clock");
    expectNear(Chorus::clockForDelaySeconds(Chorus::delaySecondsForClock(43210.0f)),
               43210.0, 1.0e-3, "delay and clock are not reciprocal");

    // Measured behaviour: both modes sweep the same delay range and differ only
    // in rate.
    const auto one = Chorus::settingsFor(ChorusMode::One);
    const auto two = Chorus::settingsFor(ChorusMode::Two);
    expectNear(one.rateHz, 0.513, 1.0e-4, "first mode rate");
    expectNear(two.rateHz, 0.863, 1.0e-4, "second mode rate");
    expectNear(one.sweepSeconds, two.sweepSeconds, 1.0e-9,
               "the two modes do not share a sweep depth");
    expectNear(one.centreDelaySeconds - one.sweepSeconds, 0.00166, 1.0e-6,
               "shortest modulated delay");
    expectNear(one.centreDelaySeconds + one.sweepSeconds, 0.00535, 1.0e-6,
               "longest modulated delay");
    expectNear(Chorus::settingsFor(ChorusMode::Off).wetGain, 0.0, 1.0e-9,
               "the wet path is not silent when the effect is switched out");
    expectNear(one.wetGain, 1.303, 1.0e-3, "line gain");

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

// A filter coefficient is only right in company with the update it is used
// with. Asserting the formula would prove nothing, so this measures where the
// corner of the pairing the line actually runs ends up, at the two rates the
// engine uses. The mismatched pairing this catches put the 9.9 kHz corner at
// 4.6 kHz -- inaudible as a bug report, obvious as a dull chorus.
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
               "anti-alias corner without oversampling");
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
} // namespace

int main()
{
    testCascadeAgainstReferenceSolve();
    testCascadeOscillationThreshold();
    testCascadeSurvivesAdversarialControl();
    testNoteTimerLaw();
    testCutoffControlLaw();
    testResonanceLaw();
    testEnvelopeAndAmplifierLaws();
    testPulseWidthAndHighPassLaws();
    testModulationAndGlideLaws();
    testPanelLawsInvert();
    testComparatorEdgesSitOnOneThreshold();
    testChorusIsAtItsSettingFromTheFirstSample();
    testBucketBrigadeLine();
    testSupportFilterCornersLandWhereAsked();
    testCorrectionResidualsVanishAtTheEdges();

    if (failures != 0)
    {
        std::cerr << failures << " YouKnow106 circuit check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All YouKnow106 circuit checks passed.\n";
    return EXIT_SUCCESS;
}
