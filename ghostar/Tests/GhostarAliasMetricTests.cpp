// The measuring instrument, measured. Every signal here is synthesised, so
// the alias content is known exactly and the metric's answer can be checked
// against it rather than against another render.
//
// These exist because the audit's first metric was wrong in a way nothing
// could catch: it reported its -200 dB floor for aliases twenty dB above
// the acceptance gate, and the only symptom was a table that looked good.

#include "AliasMetric.h"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using ghostar::aliasmetric::Residual;
using ghostar::aliasmetric::spectralResidualDb;

constexpr double sampleRate = 48000.0;
constexpr double pi = 3.14159265358979323846;
constexpr double seconds = 3.0;

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAILED: " << message << '\n';
    }
}

[[nodiscard]] std::size_t sampleCount()
{
    return static_cast<std::size_t>(seconds * sampleRate);
}

void addTone(std::vector<float>& signal, double frequency, double amplitude,
             double phase = 0.0)
{
    for (std::size_t n = 0; n < signal.size(); ++n)
        signal[n] += static_cast<float>(
            amplitude * std::sin(2.0 * pi * frequency
                                     * static_cast<double>(n) / sampleRate
                                 + phase));
}

[[nodiscard]] double energy(const std::vector<float>& signal)
{
    double total = 0.0;
    for (const float value : signal)
        total += static_cast<double>(value) * static_cast<double>(value);
    return total;
}

// The amplitude a sinusoid needs so that its energy is `db` relative to the
// energy already in `signal`.
[[nodiscard]] double amplitudeFor(const std::vector<float>& signal, double db)
{
    const double target = std::pow(10.0, db / 10.0) * energy(signal);
    return std::sqrt(2.0 * target / static_cast<double>(signal.size()));
}

[[nodiscard]] std::vector<float> harmonicSeries(double f0, int partials)
{
    std::vector<float> signal(sampleCount(), 0.0f);
    for (int k = 1; k <= partials; ++k)
        if (k * f0 < 20000.0)
            addTone(signal, k * f0, 1.0 / k, 0.31 * k);
    return signal;
}

// An added component clear of a partial must be reported at its own level.
// Under the old ceiling every one of these sat inside a 70 Hz plateau of
// permission and reported the metric's -200 dB floor instead.
void testAComponentClearOfAPartialIsReportedAtItsLevel()
{
    std::vector<float> reference(sampleCount(), 0.0f);
    addTone(reference, 1000.0, 1.0);
    for (const double offsetHz : { 40.0, 70.0, 200.0 })
    {
        auto shipping = reference;
        addTone(shipping, 1000.0 + offsetHz, amplitudeFor(reference, -20.0),
                0.9);
        const auto measured =
            spectralResidualDb(reference, shipping, sampleRate);
        check(measured.excessDb > -23.0 && measured.excessDb < -17.0,
              "a -20 dB component " + std::to_string(static_cast<int>(offsetHz))
                  + " Hz from a partial was not reported at its own level "
                    "(excess " + std::to_string(measured.excessDb) + " dB)");
    }
}

// Closer in, the added component's own main lobe starts to overlap the
// region a partial's skirt could explain, so some of its energy is
// absorbed. What matters is that it is still SEEN — the old ceiling floored
// it completely — and that the loss is bounded rather than total.
void testAComponentNearAPartialIsStillSeen()
{
    const double f0 = 1046.50;
    const auto reference = harmonicSeries(f0, 18);
    const double binHz = sampleRate / 16384.0;
    for (const double offsetBins : { 6.0, 8.0, 12.0 })
    {
        auto shipping = reference;
        addTone(shipping, 5.0 * f0 + offsetBins * binHz,
                amplitudeFor(reference, -40.0), 0.7);
        const auto measured =
            spectralResidualDb(reference, shipping, sampleRate);
        check(measured.excessDb > -85.0,
              "a -40 dB alias " + std::to_string(static_cast<int>(offsetBins))
                  + " bins from a partial vanished entirely (excess "
                  + std::to_string(measured.excessDb) + " dB)");
    }
}

// The one case no magnitude comparison can resolve: a component landing on
// a partial is arithmetically the same as that partial being a little
// louder. The instrument must not pretend otherwise - it must declare, in
// blindDb, that something this loud could be hiding.
void testAnAliasOnAPartialIsDeclaredRatherThanMissed()
{
    const double f0 = 1046.50;
    const auto reference = harmonicSeries(f0, 18);
    auto shipping = reference;
    addTone(shipping, 5.0 * f0, amplitudeFor(reference, -40.0), 0.7);
    const auto measured = spectralResidualDb(reference, shipping, sampleRate);
    check(measured.blindDb > -40.0,
          "a -40 dB alias sitting on a partial was neither reported nor "
          "declared as hideable (blind floor "
              + std::to_string(measured.blindDb) + " dB)");
}

// The tolerance the ceiling exists for must still work: a reference whose
// partials are displaced by a fraction of a bin is the same sound, not
// added content. Without this the metric would trade one failure for the
// opposite one.
void testASubBinDisplacementIsNotAddedContent()
{
    const double f0 = 1046.50;
    const double binHz = sampleRate / 16384.0;
    const auto reference = harmonicSeries(f0, 18);
    // Every partial moved by a third of a bin — a detuning in the fifth
    // decimal place, which is what two renders at different rates produce.
    const auto shipping = harmonicSeries(f0 + binHz / 3.0 / 5.0, 18);
    const auto measured = spectralResidualDb(reference, shipping, sampleRate);
    check(measured.excessDb < -55.0,
          "a sub-bin partial displacement was read as added content "
          "(excess "
              + std::to_string(measured.excessDb) + " dB)");
    check(measured.drift > 0.0,
          "the displacement the ceiling forgave was not measured");
}

// A clean render must measure as clean.
void testAnIdenticalRenderHasNoExcess()
{
    const auto reference = harmonicSeries(1046.50, 18);
    const auto measured = spectralResidualDb(reference, reference, sampleRate);
    check(measured.excessDb < -100.0,
          "a render identical to its reference reported excess");
    check(measured.audibleDb < -100.0,
          "a render identical to its reference reported a residual");
}

// The instrument must state what it could have missed. A figure with no
// blind floor beside it is what let the first metric's -200.0 rows be read
// as "no alias".
void testTheBlindFloorIsPublishedAndBounded()
{
    const auto reference = harmonicSeries(1046.50, 18);
    const auto measured = spectralResidualDb(reference, reference, sampleRate);
    // It is not zero, and it must not be: the +1 dB level tolerance alone
    // means a component under a partial can hide. What matters is that the
    // figure is published, so an excess reading is never read as proof.
    check(measured.blindDb > -60.0,
          "the blind floor is implausibly low — the level tolerance alone "
          "leaves more room than this");
    check(measured.blindDb < 0.0,
          "the blind floor exceeds the signal itself");
}

// Aliasing spread across many bins, as a real fold family is, rather than
// one convenient tone.
void testADenseFoldFamilyIsReported()
{
    const double f0 = 1046.50;
    const auto reference = harmonicSeries(f0, 18);
    auto shipping = reference;
    // Twelve images at |k*f0 - fs| style offsets: none on the harmonic grid,
    // several close to it.
    const double each = amplitudeFor(reference, -40.0) / std::sqrt(12.0);
    for (int k = 1; k <= 12; ++k)
        addTone(shipping, std::fmod(19.0 * k * f0, 20000.0) + 137.0, each,
                0.13 * k);
    const auto measured = spectralResidualDb(reference, shipping, sampleRate);
    check(measured.excessDb > -50.0,
          "a -40 dB fold family spread over twelve images was not reported "
          "(excess "
              + std::to_string(measured.excessDb) + " dB)");
}
} // namespace

int main()
{
    testAnIdenticalRenderHasNoExcess();
    testAComponentClearOfAPartialIsReportedAtItsLevel();
    testAComponentNearAPartialIsStillSeen();
    testAnAliasOnAPartialIsDeclaredRatherThanMissed();
    testASubBinDisplacementIsNotAddedContent();
    testTheBlindFloorIsPublishedAndBounded();
    testADenseFoldFamilyIsReported();

    if (failures != 0)
    {
        std::cerr << failures << " Ghostar alias metric check(s) failed.\n";
        return 1;
    }
    std::cout << "All Ghostar alias metric checks passed.\n";
    return 0;
}
