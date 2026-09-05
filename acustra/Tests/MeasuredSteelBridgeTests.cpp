// The optional Fylde bank measures scalar bridge mobility, not radiation.
// Keep the original/default and nylon paths exact, and check passivity,
// tuning, host block partitioning and live construction changes separately
// from whether listeners prefer the measured alternative.
#include "DSP/AcustraEngine.h"
#include "DSP/MeasuredBridgeData.h"
#include "DSP/MeasuredSteelBridgeData.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace acustra
{
struct AcustraEngineTestAccess
{
    static bool usesScalarBridge(const AcustraEngine& engine)
    {
        const auto& load = engine.bridgeLoad_;
        return load.immediateHeave > 0.0f
            && load.immediateCross == 0.0f && load.immediateRock == 0.0f
            && std::all_of(load.residueRock.begin(), load.residueRock.end(),
                           [] (float residue) { return residue == 0.0f; })
            && std::all_of(load.residueCross.begin(), load.residueCross.end(),
                           [] (float residue) { return residue == 0.0f; });
    }
};
}

namespace
{
using acustra::AcustraEngine;
using acustra::BridgeModel;
using acustra::EngineParameters;
using acustra::StringMaterial;
constexpr std::array<int, 6> chord { 40, 47, 52, 56, 59, 64 };
int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct Audio { std::vector<float> left, right; };

Audio render(EngineParameters parameters, int rate, int block,
             int midiNote = -1, double seconds = 1.0)
{
    auto engine = std::make_unique<AcustraEngine>();
    engine->setParameters(parameters);
    engine->prepare(rate, block);
    if (midiNote < 0)
        for (int note : chord)
            engine->noteOn(note, 0.9f);
    else
    {
        engine->setSympatheticStringsEnabled(false);
        engine->noteOn(midiNote, 0.72f);
    }
    const int samples = static_cast<int>(rate * seconds);
    Audio audio { std::vector<float>(samples), std::vector<float>(samples) };
    for (int offset = 0; offset < samples; offset += block)
        engine->process(audio.left.data() + offset, audio.right.data() + offset,
                        std::min(block, samples - offset));
    return audio;
}

bool identical(const Audio& a, const Audio& b)
{
    return a.left == b.left && a.right == b.right;
}

double peak(const Audio& audio)
{
    double maximum = 0.0;
    for (const auto* channel : { &audio.left, &audio.right })
        for (float sample : *channel)
        {
            expect(std::isfinite(sample), "measured bridge produced non-finite audio");
            maximum = std::max(maximum, std::abs(static_cast<double>(sample)));
        }
    return maximum;
}

// Hann-window DFT, searched locally around H1. Autocorrelation instead finds
// a weighted pseudo-period in a stiff string, not its fundamental frequency.
double fundamentalCents(const Audio& audio, int midiNote, int rate)
{
    const double expected = 440.0 * std::exp2((midiNote - 69.0) / 12.0);
    const int start = static_cast<int>(0.55 * rate);
    const int count = static_cast<int>(0.37 * rate);
    std::vector<double> windowed(count);
    for (int sample = 0; sample < count; ++sample)
        windowed[sample] = 0.25
            * (1.0 - std::cos(2.0 * std::numbers::pi * sample / (count - 1)))
            * (audio.left[start + sample] + audio.right[start + sample]);
    const auto power = [&] (double cents)
    {
        const double angle = 2.0 * std::numbers::pi
            * expected * std::exp2(cents / 1200.0) / rate;
        const double stepReal = std::cos(angle), stepImaginary = std::sin(angle);
        double oscillatorReal = 1.0, oscillatorImaginary = 0.0;
        double real = 0.0, imaginary = 0.0;
        for (double value : windowed)
        {
            real += value * oscillatorReal;
            imaginary += value * oscillatorImaginary;
            const double nextReal = oscillatorReal * stepReal
                                  - oscillatorImaginary * stepImaginary;
            oscillatorImaginary = oscillatorReal * stepImaginary
                                 + oscillatorImaginary * stepReal;
            oscillatorReal = nextReal;
        }
        return real * real + imaginary * imaginary;
    };
    double low = -25.0, high = 25.0;
    for (int iteration = 0; iteration < 32; ++iteration)
    {
        const double first = (2.0 * low + high) / 3.0;
        const double second = (low + 2.0 * high) / 3.0;
        if (power(first) < power(second))
            low = first;
        else
            high = second;
    }
    return 0.5 * (low + high);
}

void testBankAndOutput()
{
    // Positive semidefinite includes a scalar heave bank. Requiring a
    // nonzero rocking residue would invent information the experiment lacks.
    for (const auto& mode : acustra::detail::measuredFyldeBridgeModes)
        expect(mode.frequency >= 60.0f && mode.frequency <= 10000.0f
                   && mode.q > 0.0f && mode.heave > 0.0f
                   && mode.cross == 0.0f && mode.rock == 0.0f,
               "the measured scalar bridge left its passive fitted domain");
    double maximumPeak = 0.0, maximumCents = 0.0;
    for (int rate : { 44100, 48000, 96000 })
    {
        EngineParameters parameters;
        const auto legacy = render(parameters, rate, 64);
        parameters.bridgeModel = BridgeModel::Original;
        expect(identical(legacy, render(parameters, rate, 127)),
               "explicit Original changed the default bridge samples");
        parameters.bridgeModel = static_cast<BridgeModel>(123);
        expect(identical(legacy, render(parameters, rate, 64)),
               "invalid bridge model did not fall back to Original");
        parameters.bridgeModel = BridgeModel::FyldeSteel;
        const auto measured = render(parameters, rate, 64);
        expect(!identical(legacy, measured), "the measured bridge selector is inert");
        expect(identical(measured, render(parameters, rate, 127)),
               "measured bridge audio depends on host block partitioning");
        const double measuredPeak = peak(measured);
        maximumPeak = std::max(maximumPeak, measuredPeak);
        expect(measuredPeak > 1.0e-4 && measuredPeak < 0.95,
               "default measured-bridge chord is silent or lacks output headroom");
        auto engine = std::make_unique<AcustraEngine>();
        engine->setParameters(parameters);
        engine->prepare(rate, 64);
        expect(acustra::AcustraEngineTestAccess::usesScalarBridge(*engine),
               "the measured bank retained stale rocking or cross residues");
        for (int note : { 40, 45, 52, 64 })
        {
            const auto audio = render(parameters, rate, 64, note);
            const double cents = fundamentalCents(audio, note, rate);
            maximumCents = std::max(maximumCents, std::abs(cents));
            expect(std::abs(cents) < 1.5,
                   "measured bridge missed steady MIDI " + std::to_string(note)
                       + " at " + std::to_string(rate) + " Hz by "
                       + std::to_string(cents) + " cents");
        }
    }
    std::cout << "Fylde bridge: default chord peak " << maximumPeak
              << ", worst isolated H1 " << maximumCents << " cents\n";
}

void testNylonIsUnchanged()
{
    for (int rate : { 44100, 48000, 96000 })
    {
        EngineParameters parameters;
        parameters.stringMaterial = StringMaterial::Nylon;
        const auto legacy = render(parameters, rate, 64);
        parameters.bridgeModel = BridgeModel::FyldeSteel;
        expect(identical(legacy, render(parameters, rate, 127)),
               "selecting a steel bridge changed nylon samples");
        auto reference = std::make_unique<AcustraEngine>();
        auto switching = std::make_unique<AcustraEngine>();
        parameters.bridgeModel = BridgeModel::Original;
        for (auto* engine : { reference.get(), switching.get() })
        {
            engine->setParameters(parameters);
            engine->prepare(rate, 64);
            for (int note : chord)
                engine->noteOn(note, 0.8f);
        }
        std::array<float, 64> left {}, right {}, referenceLeft {}, referenceRight {};
        for (int block = 0; block < 400; ++block)
        {
            if (block == 100)
                for (auto* engine : { reference.get(), switching.get() })
                    engine->noteOn(65, 0.5f); // retain the previous top string
            parameters.bridgeModel = block % 2 == 0
                ? BridgeModel::FyldeSteel : BridgeModel::Original;
            switching->setParameters(parameters);
            switching->process(left.data(), right.data(), 64);
            reference->process(referenceLeft.data(), referenceRight.data(), 64);
            expect(left == referenceLeft && right == referenceRight,
                   "switching steel bridge under nylon altered a held note or tail");
        }
    }
}

void testSwitchingUnderAChord()
{
    double maximumRatio = 0.0;
    for (int rate : { 44100, 48000, 96000 })
        for (BridgeModel initial : { BridgeModel::Original, BridgeModel::FyldeSteel })
        {
            EngineParameters parameters;
            parameters.bridgeModel = initial;
            parameters.outputGain = 0.04f; // expose release spikes before limiting
            auto engine = std::make_unique<AcustraEngine>();
            engine->setParameters(parameters);
            engine->prepare(rate, 64);
            for (int note : chord)
                engine->noteOn(note, 0.85f);
            const auto advance = [&] (double seconds)
            {
                const int samples = static_cast<int>(seconds * rate);
                Audio audio { std::vector<float>(samples), std::vector<float>(samples) };
                for (int offset = 0; offset < samples; offset += 64)
                    engine->process(audio.left.data() + offset,
                                    audio.right.data() + offset,
                                    std::min(64, samples - offset));
                return peak(audio);
            };
            advance(1.2);
            const double before = advance(0.05);
            parameters.bridgeModel = initial == BridgeModel::Original
                ? BridgeModel::FyldeSteel : BridgeModel::Original;
            engine->setParameters(parameters);
            expect(engine->getActiveVoiceCount() == 6,
                   "bridge switching reset a held chord");
            const double after = advance(0.05);
            maximumRatio = std::max(maximumRatio, after / before);
            // Same 2x release-transient gate as changing strings/tuning in
            // AcustraEngineTests; this does not judge the new steady timbre.
            expect(before > 1.0e-6 && after < 2.0 * before,
                   "bridge switch release peak at " + std::to_string(rate)
                       + " Hz grew " + std::to_string(after / before) + " times");
            expect(acustra::AcustraEngineTestAccess::usesScalarBridge(*engine)
                       == (parameters.bridgeModel == BridgeModel::FyldeSteel),
                   "live bridge switching selected the wrong mobility bank");
        }
    std::cout << "Fylde bridge: worst switch peak ratio " << maximumRatio << '\n';
}
}

int main()
{
    testBankAndOutput();
    testNylonIsUnchanged();
    testSwitchingUnderAChord();
    if (failures != 0)
        return 1;
    std::cout << "All measured steel bridge tests passed\n";
    return 0;
}
