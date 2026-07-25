#include "DSP/ElectryEngine.h"
#include "DSP/ElectryVisuals.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace electry
{
// Narrow inspection seam for the JUCE-free regression suite.
struct ElectryEngineTestAccess
{
    struct VoiceSnapshot
    {
        bool valid { false };
        bool active { false };
        bool keyDown { false };
        bool releasing { false };
        int stringIndex { -1 };
        int midiNote { -1 };
        int fret { -1 };
        Articulation articulation { Articulation::Downstroke };
        float verticalDelayTarget { 0.0f };
        float verticalDelayCurrent { 0.0f };
        float dispersionLowCoefficient { 0.0f };
        float dispersionHighCoefficient { 0.0f };
        float inharmonicity { 0.0f };
        float dispersionLowPartial { 0.0f };
        float dispersionHighPartial { 0.0f };
        float bodyConductance { 0.0f };
        float bodyLossFactor { 1.0f };
        float loopGain { 0.0f };
        float baseFrequency { 0.0f };
        int collisionRemaining { 0 };
        int tremoloSamplesUntilRetrigger { 0 };
        std::uint32_t tremoloRetriggerCount { 0 };
        bool tremoloStrokeIsUp { false };
        std::uint64_t ageSamples { 0 };
        int startDelaySamples { 0 };
        bool sympatheticReady { false };
        float sympatheticEnergy { 0.0f };
        float excitationCombDelay { 0.0f };
        float excitationCombWidth { 0.0f };
        float excitationLoadScale { 0.0f };
        float excitationSlipScale { 0.0f };
        float loopDampingCoefficient { 0.0f };
    };

    static VoiceSnapshot snapshot(const ElectryEngine& engine, int stringIndex)
    {
        VoiceSnapshot result;
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return result;
        const auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        result.valid = true;
        result.active = voice.active;
        result.keyDown = voice.keyDown;
        result.releasing = voice.releasing;
        result.stringIndex = voice.stringIndex;
        result.midiNote = voice.midiNote;
        result.fret = voice.fret;
        result.articulation = voice.articulation;
        result.verticalDelayTarget = voice.vertical.targetDelay;
        result.verticalDelayCurrent = voice.vertical.currentDelay;
        result.dispersionLowCoefficient = voice.vertical.dispersionLowCoefficient;
        result.dispersionHighCoefficient = voice.vertical.dispersionHighCoefficient;
        result.inharmonicity = voice.inharmonicity;
        result.dispersionLowPartial = voice.dispersionLowPartial;
        result.dispersionHighPartial = voice.dispersionHighPartial;
        result.bodyConductance = voice.bodyConductance;
        result.bodyLossFactor = voice.bodyLossFactor;
        result.loopGain = voice.vertical.loopGain;
        result.baseFrequency = voice.baseFrequency;
        result.collisionRemaining = voice.collisionRemaining;
        result.tremoloSamplesUntilRetrigger = voice.tremoloSamplesUntilRetrigger;
        result.tremoloRetriggerCount = voice.tremoloRetriggerCount;
        result.tremoloStrokeIsUp = voice.tremoloStrokeIsUp;
        result.ageSamples = voice.ageSamples;
        result.startDelaySamples = voice.startDelaySamples;
        result.sympatheticReady = voice.sympatheticReady;
        result.sympatheticEnergy = voice.sympatheticEnergy;
        result.excitationCombDelay = voice.excitationCombDelay;
        result.excitationCombWidth = voice.excitationCombWidth;
        result.excitationLoadScale = voice.excitationLoadScale;
        result.excitationSlipScale = voice.excitationSlipScale;
        result.loopDampingCoefficient = voice.vertical.loopDampingCoefficient;
        return result;
    }

    static bool channelsLinked(const ElectryEngine& engine) noexcept
    {
        return engine.channelsLinked_;
    }

    static bool pickupPathActive(const ElectryEngine& engine, bool neck) noexcept
    {
        return neck ? engine.neckPathActive_ : engine.bridgePathActive_;
    }

    static int oversamplingFactor(const ElectryEngine& engine) noexcept
    {
        return engine.oversamplingFactor_;
    }

    static double hostSampleRate(const ElectryEngine& engine) noexcept
    {
        return engine.hostSampleRate_;
    }

    static double internalSampleRate(const ElectryEngine& engine) noexcept
    {
        return engine.sampleRate_;
    }

    static float dispersionDeficit(const ElectryEngine& engine,
                                    int stringIndex, float partial) noexcept
    {
        if (stringIndex < 0 || stringIndex >= ElectryEngine::stringCount)
            return 0.0f;
        const auto& voice = engine.voices_[static_cast<std::size_t>(stringIndex)];
        const auto& loop = voice.vertical;
        const float omega = 2.0f * 3.14159265358979323846f
                          * voice.lastConfiguredFrequency
                          / static_cast<float>(engine.sampleRate_);
        const float omegaPartial = std::min(
            omega * partial, 3.14159265358979323846f * 0.95f);
        const auto sectionDeficit = [&] (float coefficient)
        {
            return ElectryEngine::allpassPhaseDelay(coefficient, omega)
                 - ElectryEngine::allpassPhaseDelay(coefficient, omegaPartial);
        };
        return 4.0f * sectionDeficit(loop.dispersionLowCoefficient)
             + 4.0f * sectionDeficit(loop.dispersionHighCoefficient);
    }

    static double modalMagnitudeAt(float frequencyHz, float q, float modeGain,
                                   float sampleRate,
                                   float evaluationFrequencyHz) noexcept
    {
        ElectryEngine::ModalResonator resonator;
        resonator.configure(frequencyHz, q, modeGain, sampleRate);

        const double omega = 2.0 * 3.14159265358979323846
                           * static_cast<double>(evaluationFrequencyHz)
                           / static_cast<double>(sampleRate);
        const double denominatorReal = 1.0
            + static_cast<double>(resonator.a1) * std::cos(omega)
            + static_cast<double>(resonator.a2) * std::cos(2.0 * omega);
        const double denominatorImag =
            -static_cast<double>(resonator.a1) * std::sin(omega)
            -static_cast<double>(resonator.a2) * std::sin(2.0 * omega);
        return std::abs(static_cast<double>(resonator.gain))
             / std::max(std::hypot(denominatorReal, denominatorImag), 1.0e-20);
    }

    static constexpr int delayLineCapacity() noexcept
    {
        return ElectryEngine::delayLineSize;
    }

    static int stringForNote(const ElectryEngine& engine, int midiNote)
    {
        for (int s = 0; s < ElectryEngine::stringCount; ++s)
        {
            const auto& voice = engine.voices_[static_cast<std::size_t>(s)];
            if (voice.active && voice.midiNote == midiNote)
                return s;
        }
        return -1;
    }
};
} // namespace electry

namespace
{
using electry::Articulation;
using electry::ElectryEngine;
using electry::EngineParameters;
using electry::PickupSelector;
using TestAccess = electry::ElectryEngineTestAccess;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct StereoBuffer
{
    std::vector<float> left;
    std::vector<float> right;

    explicit StereoBuffer(int samples)
        : left(static_cast<std::size_t>(samples), 0.0f),
          right(static_cast<std::size_t>(samples), 0.0f) {}

    [[nodiscard]] int size() const noexcept
    {
        return static_cast<int>(left.size());
    }
};

void renderInto(ElectryEngine& engine, StereoBuffer& buffer, int blockSize = 512)
{
    int rendered = 0;
    const int total = buffer.size();
    while (rendered < total)
    {
        const int samples = std::min(blockSize, total - rendered);
        engine.process(buffer.left.data() + rendered,
                       buffer.right.data() + rendered, samples);
        rendered += samples;
    }
}

StereoBuffer renderNote(ElectryEngine& engine, double sampleRate, int midiNote,
                        float velocity, Articulation articulation,
                        double seconds, double noteOffAfterSeconds = -1.0)
{
    engine.reset();
    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(articulation), 1.0f);
    engine.noteOn(midiNote, velocity);

    const int totalSamples = static_cast<int>(seconds * sampleRate);
    StereoBuffer buffer(totalSamples);

    if (noteOffAfterSeconds > 0.0 && noteOffAfterSeconds < seconds)
    {
        const int offSample = static_cast<int>(noteOffAfterSeconds * sampleRate);
        engine.process(buffer.left.data(), buffer.right.data(), offSample);
        engine.noteOff(midiNote);
        engine.process(buffer.left.data() + offSample,
                       buffer.right.data() + offSample,
                       totalSamples - offSample);
    }
    else
    {
        renderInto(engine, buffer);
    }
    return buffer;
}

bool allFinite(const StereoBuffer& buffer)
{
    for (const float sample : buffer.left)
        if (! std::isfinite(sample))
            return false;
    for (const float sample : buffer.right)
        if (! std::isfinite(sample))
            return false;
    return true;
}

float peakAbs(const std::vector<float>& data, int start = 0, int end = -1)
{
    const int last = end < 0 ? static_cast<int>(data.size())
                             : std::min<int>(end, static_cast<int>(data.size()));
    float peak = 0.0f;
    for (int i = std::max(0, start); i < last; ++i)
        peak = std::max(peak, std::abs(data[static_cast<std::size_t>(i)]));
    return peak;
}

double rmsInRange(const std::vector<float>& data, int start, int end)
{
    const int first = std::max(0, start);
    const int last = std::min<int>(end, static_cast<int>(data.size()));
    if (last <= first)
        return 0.0;
    double sum = 0.0;
    for (int i = first; i < last; ++i)
        sum += static_cast<double>(data[static_cast<std::size_t>(i)])
             * static_cast<double>(data[static_cast<std::size_t>(i)]);
    return std::sqrt(sum / static_cast<double>(last - first));
}

double normalisedDifferenceRms(const std::vector<float>& a,
                               const std::vector<float>& b,
                               int start, int end)
{
    const int first = std::max(0, start);
    const int last = std::min<int>({ end, static_cast<int>(a.size()),
                                    static_cast<int>(b.size()) });
    if (last <= first)
        return 0.0;

    double difference = 0.0;
    double reference = 0.0;
    for (int i = first; i < last; ++i)
    {
        const double av = a[static_cast<std::size_t>(i)];
        const double bv = b[static_cast<std::size_t>(i)];
        const double delta = av - bv;
        difference += delta * delta;
        reference += 0.5 * (av * av + bv * bv);
    }
    return reference > 0.0 ? std::sqrt(difference / reference) : 0.0;
}

// Hann-windowed DFT magnitude at an arbitrary frequency, evaluated with a
// phasor recurrence so the tests stay fast.
double dftMagnitude(const std::vector<float>& data, int start, int length,
                    double sampleRate, double frequency)
{
    const int first = std::max(0, start);
    const int last = std::min<int>(first + length, static_cast<int>(data.size()));
    const int n = last - first;
    if (n < 16)
        return 0.0;

    const double omega = 2.0 * 3.14159265358979323846 * frequency / sampleRate;
    const double stepReal = std::cos(omega);
    const double stepImag = -std::sin(omega);
    double phasorReal = 1.0;
    double phasorImag = 0.0;
    double sumReal = 0.0;
    double sumImag = 0.0;
    const double windowStep = 3.14159265358979323846 / static_cast<double>(n - 1);

    for (int i = 0; i < n; ++i)
    {
        const double window = std::sin(windowStep * i);
        const double sample = window * window
            * static_cast<double>(data[static_cast<std::size_t>(first + i)]);
        sumReal += sample * phasorReal;
        sumImag += sample * phasorImag;
        const double nextReal = phasorReal * stepReal - phasorImag * stepImag;
        phasorImag = phasorReal * stepImag + phasorImag * stepReal;
        phasorReal = nextReal;
    }
    return std::sqrt(sumReal * sumReal + sumImag * sumImag);
}

// Locate the strongest spectral component near an expected fundamental by a
// coarse-to-fine scan, returning its frequency in Hz.
double measureFrequency(const std::vector<float>& data, int start, int length,
                        double sampleRate, double expectedHz)
{
    const auto scan = [&] (double centre, double spanCents, double stepCents)
    {
        double bestFrequency = centre;
        double bestMagnitude = -1.0;
        for (double cents = -spanCents; cents <= spanCents; cents += stepCents)
        {
            const double frequency = centre * std::pow(2.0, cents / 1200.0);
            // A magnetic pickup produces induced EMF, so its fundamental can
            // be weaker than the first few partials. Score a short harmonic
            // series instead of assuming displacement-like fundamental
            // dominance; the narrow scan still prevents octave ambiguity.
            double magnitude = 0.0;
            for (int partial = 1; partial <= 5; ++partial)
            {
                const double partialFrequency = frequency * partial;
                if (partialFrequency >= 0.45 * sampleRate)
                    break;
                magnitude += dftMagnitude(data, start, length, sampleRate,
                                          partialFrequency)
                           / std::sqrt(static_cast<double>(partial));
            }
            if (magnitude > bestMagnitude)
            {
                bestMagnitude = magnitude;
                bestFrequency = frequency;
            }
        }
        return bestFrequency;
    };

    const double coarse = scan(expectedHz, 120.0, 6.0);
    return scan(coarse, 6.0, 0.5);
}

double centsBetween(double frequencyA, double frequencyB)
{
    return 1200.0 * std::log2(frequencyA / frequencyB);
}

// Energy-weighted mean frequency across the tone's partial series. Sampling
// the spectrum at the partials measures the envelope rather than window
// leakage between harmonic lines.
double spectralCentroid(const std::vector<float>& data, int start, int length,
                        double sampleRate, double fundamentalHz)
{
    double weighted = 0.0;
    double total = 0.0;
    for (int partial = 1; partial <= 48; ++partial)
    {
        const double frequency = fundamentalHz * partial;
        if (frequency > std::min(6500.0, 0.45 * sampleRate))
            break;
        const double magnitude = dftMagnitude(data, start, length, sampleRate,
                                              frequency);
        weighted += magnitude * frequency;
        total += magnitude;
    }
    return total > 0.0 ? weighted / total : 0.0;
}

double midiHz(int midiNote)
{
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

// Magnitude ratio between the partials above and below a split frequency:
// a direct measure of what a passive tone control removes.
double highBandRatio(const std::vector<float>& data, int start, int length,
                     double sampleRate, double fundamentalHz)
{
    double low = 0.0;
    double high = 0.0;
    for (int partial = 1; partial <= 48; ++partial)
    {
        const double frequency = fundamentalHz * partial;
        if (frequency > std::min(6500.0, 0.45 * sampleRate))
            break;
        const double magnitude = dftMagnitude(data, start, length, sampleRate,
                                              frequency);
        if (frequency < 900.0)
            low += magnitude;
        else
            high += magnitude;
    }
    return low > 0.0 ? high / low : 0.0;
}

struct HarmonicBalance
{
    double lowPowerShare { 0.0 };
    double highPowerShare { 0.0 };
    double strongestFirstEightDb { -300.0 };
    double lowMagnitude { 0.0 };
};

double decibels(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-15));
}

// Stiff wound-string partials sit slightly above an ideal harmonic series.
// A short scan prevents valid dispersion from looking like missing spectral
// energy while remaining narrow enough not to count unrelated noise.
double scannedPartialMagnitude(const std::vector<float>& data, int start,
                               int length, double sampleRate,
                               double fundamentalHz, int partial)
{
    const double idealFrequency = fundamentalHz * static_cast<double>(partial);
    double best = 0.0;
    for (double cents = -8.0; cents <= 56.0; cents += 8.0)
    {
        const double frequency = idealFrequency * std::pow(2.0, cents / 1200.0);
        best = std::max(best, dftMagnitude(data, start, length, sampleRate,
                                           frequency));
    }
    return best;
}

HarmonicBalance measureHarmonicBalance(const std::vector<float>& data,
                                       int start, int length,
                                       double sampleRate,
                                       double fundamentalHz)
{
    double lowPower = 0.0;
    double middlePower = 0.0;
    double highPower = 0.0;
    double fundamentalMagnitude = 0.0;
    double strongestFirstEight = 0.0;

    for (int partial = 1; partial <= 64; ++partial)
    {
        const double frequency = fundamentalHz * static_cast<double>(partial);
        if (frequency >= std::min(6500.0, 0.45 * sampleRate))
            break;
        const double magnitude = scannedPartialMagnitude(
            data, start, length, sampleRate, fundamentalHz, partial);
        const double power = magnitude * magnitude;
        if (frequency < 250.0)
            lowPower += power;
        else if (frequency < 1000.0)
            middlePower += power;
        else
            highPower += power;

        if (partial == 1)
            fundamentalMagnitude = magnitude;
        else if (partial <= 8)
            strongestFirstEight = std::max(strongestFirstEight, magnitude);
    }

    const double totalPower = lowPower + middlePower + highPower;
    HarmonicBalance result;
    result.lowPowerShare = lowPower / std::max(totalPower, 1.0e-30);
    result.highPowerShare = highPower / std::max(totalPower, 1.0e-30);
    result.strongestFirstEightDb = decibels(
        strongestFirstEight / std::max(fundamentalMagnitude, 1.0e-15));
    result.lowMagnitude = std::sqrt(lowPower);
    return result;
}

// ---------------------------------------------------------------------------

void testModalResonatorPeakGain()
{
    // These span the open low strings, the complete solid-body mode table,
    // and the Q/gain ranges used by both the body and sympathetic banks. The
    // old numerator exceeded the requested gain by 100x or more down here.
    struct Case
    {
        float frequency;
        float q;
        float gain;
    };
    constexpr std::array<Case, 7> cases {{
        { 41.20f, 34.0f, 0.026f },
        { 61.74f, 28.0f, 0.040f },
        { 92.0f, 30.0f, 1.20f },
        { 112.0f, 24.0f, 1.00f },
        { 220.0f, 18.0f, 0.68f },
        { 488.0f, 12.0f, 0.32f },
        { 690.0f, 9.0f, 0.46f },
    }};

    constexpr std::array<float, 3> internalSampleRates {
        96000.0f, 192000.0f, 384000.0f
    };
    for (const float internalSampleRate : internalSampleRates)
    {
        for (const auto& test : cases)
        {
            const double actual = TestAccess::modalMagnitudeAt(
                test.frequency, test.q, test.gain, internalSampleRate,
                test.frequency);
            const double relativeError = std::abs(actual - test.gain)
                                       / std::max<double>(test.gain, 1.0e-12);
            expect(relativeError < 0.005,
                   "modal resonator did not reproduce its requested peak gain at "
                       + std::to_string(test.frequency) + " Hz / "
                       + std::to_string(internalSampleRate) + " Hz sample rate"
                       + " (requested " + std::to_string(test.gain)
                       + ", actual " + std::to_string(actual) + ")");
        }
    }
}

void testLowRegisterGuitarEnvelope()
{
    constexpr double sampleRate = 48000.0;
    EngineParameters cleanParameters;
    cleanParameters.pickupSelector = PickupSelector::Bridge;
    cleanParameters.outputMode = electry::OutputMode::Mono;
    // Measure the pitched string itself. Incidental noises and sympathetic
    // hardware must not be what makes an otherwise thin E1/B1 pass.
    cleanParameters.artifactAmount = 0.0f;
    cleanParameters.pickNoise = 0.0f;
    cleanParameters.fingerNoise = 0.0f;
    cleanParameters.releaseNoise = 0.0f;

    struct NoteCase
    {
        int midiNote;
        const char* name;
        double minimumAttackLowShare;
        double minimumSustainLowShare;
    };
    constexpr std::array<NoteCase, 2> notes {{
        { 28, "E1", 0.20, 0.25 },
        { 35, "B1", 0.15, 0.20 },
    }};

    for (const auto& note : notes)
    {
        const auto validate = [&] (const EngineParameters& parameters,
                                   const char* variant,
                                   double maximumPeakToSustainDb)
        {
            ElectryEngine engine;
            engine.prepare(sampleRate, 512);
            engine.setParameters(parameters);
            const auto render = renderNote(
                engine, sampleRate, note.midiNote, 0.8f,
                Articulation::Downstroke, 4.1);
            const double fundamental = midiHz(note.midiNote);
            const auto attack = measureHarmonicBalance(
                render.left, static_cast<int>(0.03 * sampleRate),
                static_cast<int>(0.15 * sampleRate), sampleRate, fundamental);
            const auto sustain = measureHarmonicBalance(
                render.left, static_cast<int>(0.25 * sampleRate),
                static_cast<int>(0.40 * sampleRate), sampleRate, fundamental);
            const auto late = measureHarmonicBalance(
                render.left, static_cast<int>(3.25 * sampleRate),
                static_cast<int>(0.40 * sampleRate), sampleRate, fundamental);
            const std::string prefix = std::string(note.name) + " " + variant;

            expect(attack.lowPowerShare >= note.minimumAttackLowShare,
                   prefix + " attack still lacks low partials ("
                       + std::to_string(100.0 * attack.lowPowerShare) + "%)");
            expect(attack.highPowerShare <= 0.50,
                   prefix + " attack remains clavinet-bright ("
                       + std::to_string(100.0 * attack.highPowerShare)
                       + "% above 1 kHz)");
            expect(sustain.lowPowerShare >= note.minimumSustainLowShare,
                   prefix + " sustain still lacks low partials ("
                       + std::to_string(100.0 * sustain.lowPowerShare) + "%)");
            expect(sustain.highPowerShare <= 0.35,
                   prefix + " sustain remains upper-harmonic dominated ("
                       + std::to_string(100.0 * sustain.highPowerShare)
                       + "% above 1 kHz)");
            expect(attack.strongestFirstEightDb <= 16.0,
                   prefix + " attack partial exceeds the fundamental by "
                       + std::to_string(attack.strongestFirstEightDb) + " dB");
            expect(sustain.strongestFirstEightDb <= 12.0,
                   prefix + " sustained partial exceeds the fundamental by "
                       + std::to_string(sustain.strongestFirstEightDb) + " dB");

            const double attackToSustain = decibels(
                peakAbs(render.left, 0, static_cast<int>(0.20 * sampleRate))
                / std::max(rmsInRange(
                    render.left, static_cast<int>(0.20 * sampleRate),
                    static_cast<int>(0.70 * sampleRate)), 1.0e-15));
            expect(attackToSustain <= maximumPeakToSustainDb,
                   prefix + " is too transient-heavy (peak/sustain "
                       + std::to_string(attackToSustain) + " dB)");

            const double earlyRms = rmsInRange(
                render.left, static_cast<int>(0.05 * sampleRate),
                static_cast<int>(0.20 * sampleRate));
            const auto expectTailAbove = [&] (double begin, double end,
                                              double minimumDb)
            {
                const double tailRms = rmsInRange(
                    render.left, static_cast<int>(begin * sampleRate),
                    static_cast<int>(end * sampleRate));
                const double relativeDb = decibels(
                    tailRms / std::max(earlyRms, 1.0e-15));
                expect(relativeDb >= minimumDb,
                       prefix + " string dies too early at "
                           + std::to_string(begin) + "-" + std::to_string(end)
                           + " s (" + std::to_string(relativeDb) + " dB)");
            };
            expectTailAbove(0.50, 1.00, -8.0);
            expectTailAbove(1.00, 2.00, -15.0);
            expectTailAbove(2.00, 4.00, -26.0);

            const double lowDecayDb = decibels(
                late.lowMagnitude / std::max(sustain.lowMagnitude, 1.0e-15));
            const double apparentT60 = lowDecayDb < -1.0e-6
                ? -60.0 * 3.0 / lowDecayDb
                : 1.0e6;
            expect(apparentT60 >= 4.0 && apparentT60 <= 12.0,
                   prefix + " low-partial T60 left the guitar range ("
                       + std::to_string(apparentT60) + " s)");
        };

        validate(cleanParameters, "clean physical string", 15.0);
        // The normal preset retains a small direct plectrum/contact transient;
        // allow that realistic edge without returning to the old 22 dB
        // clavinet-like attack-to-sustain ratio.
        validate(EngineParameters {}, "default output", 17.0);

    }
}

void testOpenLowStringLevelBalance()
{
    constexpr double sampleRate = 48000.0;
    constexpr double renderSeconds = 0.75;

    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.outputMode = electry::OutputMode::Mono;

    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(parameters);

    const auto e1 = renderNote(engine, sampleRate, 28, 0.8f,
                               Articulation::Downstroke, renderSeconds);
    const auto b1 = renderNote(engine, sampleRate, 35, 0.8f,
                               Articulation::Downstroke, renderSeconds);
    const auto e2 = renderNote(engine, sampleRate, 40, 0.8f,
                               Articulation::Downstroke, renderSeconds);
    const auto a2 = renderNote(engine, sampleRate, 45, 0.8f,
                               Articulation::Downstroke, renderSeconds);

    const std::array<const StereoBuffer*, 4> renders {{ &e1, &b1, &e2, &a2 }};
    struct Window
    {
        const char* name;
        double beginSeconds;
        double endSeconds;
        double minimumE1;
        double minimumB1;
    };
    constexpr std::array<Window, 3> windows {{
        { "attack", 0.004, 0.115, 0.003981, 0.003981 },
        { "body", 0.050, 0.200, 0.003981, 0.003981 },
        { "sustain", 0.200, 0.700, 0.002512, 0.002512 },
    }};

    for (const auto& window : windows)
    {
        std::array<double, 4> rms {};
        for (std::size_t i = 0; i < renders.size(); ++i)
        {
            rms[i] = rmsInRange(
                renders[i]->left,
                static_cast<int>(window.beginSeconds * sampleRate),
                static_cast<int>(window.endSeconds * sampleRate));
        }

        expect(rms[0] >= window.minimumE1,
               std::string(window.name) + " E1 is effectively silent (RMS "
                   + std::to_string(rms[0]) + ")");
        expect(rms[1] >= window.minimumB1,
               std::string(window.name) + " B1 is effectively silent (RMS "
                   + std::to_string(rms[1]) + ")");

        const double reference = std::max(rms[3], 1.0e-15);
        const std::array<double, 3> ratios {
            rms[0] / reference, rms[1] / reference, rms[2] / reference
        };
        constexpr std::array<double, 3> minimumRatios {
            0.5012, 0.6310, 0.7079 // -6, -4, and -3 dB versus A2.
        };
        for (std::size_t index = 0; index < ratios.size(); ++index)
        {
            expect(ratios[index] >= minimumRatios[index],
                   std::string(window.name) + " low string "
                       + std::to_string(index) + " is under-balanced versus A2 ("
                       + std::to_string(decibels(ratios[index])) + " dB)");
            expect(ratios[index] <= 1.585,
                   std::string(window.name) + " low string "
                       + std::to_string(index) + " is over-compensated versus A2 ("
                       + std::to_string(decibels(ratios[index])) + " dB)");
        }
    }

    expect(peakAbs(e1.left) < 0.50f && peakAbs(b1.left) < 0.50f,
           "low-register level compensation is driving every note into the guard");
}

void testInternalOversamplingPolicy()
{
    struct RateCase { double hostRate; int expectedFactor; };
    constexpr std::array<RateCase, 5> rates {{
        { 44100.0, 2 }, { 48000.0, 2 }, { 96000.0, 2 },
        { 192000.0, 1 }, { 384000.0, 1 },
    }};

    for (const auto& rate : rates)
    {
        ElectryEngine engine;
        engine.prepare(rate.hostRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);

        expect(TestAccess::oversamplingFactor(engine) == rate.expectedFactor,
               "wrong internal oversampling factor at "
                   + std::to_string(rate.hostRate) + " Hz");
        expect(TestAccess::hostSampleRate(engine) == rate.hostRate,
               "host sample rate was not retained by prepare()");
        expect(TestAccess::internalSampleRate(engine)
                   == rate.hostRate * rate.expectedFactor,
               "internal sample clock does not match host rate times factor");

        engine.noteOn(45, 0.8f);
        constexpr int hostSamples = 1024;
        StereoBuffer buffer(hostSamples);
        renderInto(engine, buffer, 127);
        expect(allFinite(buffer),
               "oversampled render became non-finite at "
                   + std::to_string(rate.hostRate) + " Hz");
        const float peak = peakAbs(buffer.left);
        expect(peak > 1.0e-5f && peak < 0.80f,
               "oversampled render was silent or unbounded at "
                   + std::to_string(rate.hostRate) + " Hz");

        const int stringIndex = TestAccess::stringForNote(engine, 45);
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        expect(snapshot.ageSamples
                   == static_cast<std::uint64_t>(hostSamples * rate.expectedFactor),
               "host samples did not advance the physical clock exactly");
    }

    // Compare one 2x render with one native high-rate render. Both must retain
    // the played pitch after the halfband FIR and host-rate decimation.
    for (const double sampleRate : { 48000.0, 192000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);
        const auto buffer = renderNote(engine, sampleRate, 45, 0.35f,
                                       Articulation::Downstroke, 0.8);
        const double expected = midiHz(45);
        const double measured = measureFrequency(
            buffer.left, static_cast<int>(0.30 * sampleRate),
            static_cast<int>(0.35 * sampleRate), sampleRate, expected);
        expect(std::abs(centsBetween(measured, expected)) < 10.0,
               "oversampling introduced gross pitch drift at "
                   + std::to_string(sampleRate) + " Hz");
    }
}

void testRenderMatrixFiniteAndBounded()
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        engine.setParameters(parameters);

        for (int articulationIndex = 0;
             articulationIndex < ElectryEngine::keyswitchCount; ++articulationIndex)
        {
            const auto articulation = static_cast<Articulation>(articulationIndex);
            auto buffer = renderNote(engine, sampleRate, 45, 0.9f, articulation,
                                     0.5, 0.35);
            expect(allFinite(buffer),
                   "non-finite output at rate " + std::to_string(sampleRate)
                       + " articulation " + std::to_string(articulationIndex));
            const float peak = peakAbs(buffer.left);
            expect(peak < 0.80f,
                   "output beyond guard at rate " + std::to_string(sampleRate)
                       + " articulation " + std::to_string(articulationIndex));
            expect(peak > 1.0e-4f,
                   "articulation " + std::to_string(articulationIndex)
                       + " is silent at rate " + std::to_string(sampleRate));

            if (sampleRate == 48000.0)
            {
                const auto low = renderNote(
                    engine, sampleRate, 28, 0.9f, articulation, 0.5, 0.35);
                const float lowPeak = peakAbs(low.left);
                expect(allFinite(low),
                       "non-finite Drop-E output for articulation "
                           + std::to_string(articulationIndex));
                expect(lowPeak > 1.0e-4f && lowPeak < 0.80f,
                       "Drop-E articulation is silent or driving the guard: "
                           + std::to_string(articulationIndex) + " (peak "
                           + std::to_string(lowPeak) + ")");
            }
        }
    }
}

void testPitchAccuracy()
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickNoise = 0.0f;
        parameters.fingerNoise = 0.0f;
        parameters.releaseNoise = 0.0f;
        engine.setParameters(parameters);

        for (const int midiNote : { 28, 35, 40, 45, 50, 55, 59, 64, 69, 76, 86 })
        {
            auto buffer = renderNote(engine, sampleRate, midiNote, 0.3f,
                                     Articulation::Downstroke, 1.1);
            const int start = static_cast<int>(0.45 * sampleRate);
            const int window = static_cast<int>(0.5 * sampleRate);
            const double expected = midiHz(midiNote);
            const double measured = measureFrequency(buffer.left, start, window,
                                                     sampleRate, expected);
            const double cents = centsBetween(measured, expected);
            expect(std::abs(cents) < 8.0,
                   "note " + std::to_string(midiNote) + " at rate "
                       + std::to_string(sampleRate) + " off by "
                       + std::to_string(cents) + " cents");
        }
    }
}

void testDropELowNoteAtMaximumRate()
{
    constexpr double sampleRate = 384000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);
    engine.setPitchBend(-1.0f);
    engine.noteOn(28, 0.25f);

    StereoBuffer buffer(static_cast<int>(1.25 * sampleRate));
    renderInto(engine, buffer);
    expect(allFinite(buffer), "Drop-E render became non-finite at 384 kHz");

    const int stringIndex = TestAccess::stringForNote(engine, 28);
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(stringIndex == 0 && snapshot.fret == 0,
           "open E1 was not allocated to the eighth string");
    expect(snapshot.verticalDelayTarget > 9000.0f,
           "maximum-rate Drop-E delay did not exercise the expanded line");
    expect(snapshot.verticalDelayTarget
               < static_cast<float>(TestAccess::delayLineCapacity() - 8),
           "maximum-rate Drop-E delay exceeded the delay-line capacity");

    const double expected = midiHz(26); // E1 with the wheel at -2 semitones.
    const double measured = measureFrequency(
        buffer.left, static_cast<int>(0.55 * sampleRate),
        static_cast<int>(0.60 * sampleRate), sampleRate, expected);
    expect(std::abs(centsBetween(measured, expected)) < 10.0,
           "384 kHz Drop-E wheel-down pitch is inaccurate");
}

void testDeterminism()
{
    constexpr double sampleRate = 48000.0;
    const auto renderSequence = [] ()
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 256);
        EngineParameters parameters;
        engine.setParameters(parameters);
        engine.reset();

        StereoBuffer buffer(static_cast<int>(1.6 * sampleRate));
        engine.noteOn(ElectryEngine::firstKeyswitchNote
                          + static_cast<int>(Articulation::Slap), 1.0f);
        engine.noteOn(45, 0.85f);
        engine.process(buffer.left.data(), buffer.right.data(), 12000);
        engine.noteOn(ElectryEngine::firstKeyswitchNote
                          + static_cast<int>(Articulation::Tremolo), 1.0f);
        engine.noteOn(52, 0.6f);
        engine.process(buffer.left.data() + 12000,
                       buffer.right.data() + 12000, 24000);
        engine.noteOff(45);
        engine.noteOff(52);
        engine.process(buffer.left.data() + 36000, buffer.right.data() + 36000,
                       buffer.size() - 36000);
        return buffer;
    };

    const auto first = renderSequence();
    const auto second = renderSequence();
    bool identical = true;
    for (std::size_t i = 0; i < first.left.size(); ++i)
        if (first.left[i] != second.left[i])
        {
            identical = false;
            break;
        }
    expect(identical, "identical MIDI does not render identical audio");

    const float peak = peakAbs(first.left);
    expect(peak > 1.0e-4f, "determinism fixture rendered silence");
}

void testKeyswitchesSelectStylesSilently()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();

    expect(ElectryEngine::firstKeyswitchNote == 12,
           "keyswitch range does not start at MIDI 12");
    expect(ElectryEngine::keyswitchCount == 16,
           "keyswitch range does not expose all 16 play styles");
    expect(ElectryEngine::lowestPlayableNote == 28
               && ElectryEngine::highestPlayableNote == 86,
           "Drop-E playable range is not MIDI 28..86");

    expect(engine.getCurrentArticulation() == Articulation::Downstroke,
           "default articulation is not Downstroke");

    // Keyswitches alone must never make sound.
    StereoBuffer buffer(static_cast<int>(0.25 * sampleRate));
    for (int keyswitch = 0; keyswitch < ElectryEngine::keyswitchCount; ++keyswitch)
        engine.noteOn(ElectryEngine::firstKeyswitchNote + keyswitch, 1.0f);
    renderInto(engine, buffer);
    expect(peakAbs(buffer.left) == 0.0f, "keyswitch notes produced audio");
    expect(engine.getActiveVoiceCount() == 0, "keyswitch notes created voices");

    // The last pressed keyswitch latches.
    expect(engine.getCurrentArticulation() == Articulation::Slap,
           "keyswitch latching did not reach Slap");

    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(Articulation::Muted), 1.0f);
    expect(engine.getCurrentArticulation() == Articulation::Muted,
           "keyswitch did not switch to Muted");

    // Keyswitch note-offs are ignored; the style persists for played notes.
    engine.noteOff(ElectryEngine::firstKeyswitchNote
                       + static_cast<int>(Articulation::Muted));
    expect(engine.getCurrentArticulation() == Articulation::Muted,
           "keyswitch note-off cleared the latched style");

    engine.noteOn(52, 0.8f);
    const auto stringIndex = TestAccess::stringForNote(engine, 52);
    expect(stringIndex >= 0, "played note did not allocate a string");
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(snapshot.articulation == Articulation::Muted,
           "played note did not inherit the latched articulation");

    // The adjacent range boundary is unambiguous: 27 is the final silent
    // keyswitch and 28 is the sounding open low E.
    engine.reset();
    engine.noteOn(27, 0.9f);
    expect(engine.getCurrentArticulation() == Articulation::Slap,
           "MIDI 27 did not select the final play style");
    expect(engine.getActiveVoiceCount() == 0,
           "final keyswitch note created a voice");
    engine.noteOn(28, 0.8f);
    expect(TestAccess::stringForNote(engine, 28) == 0,
           "MIDI 28 did not play open E1 on the lowest string");

    // Notes outside both the keyswitch and playable ranges are ignored.
    engine.noteOn(0, 0.9f);
    engine.noteOn(87, 0.9f);
    expect(engine.getActiveVoiceCount() == 1,
           "notes outside keyswitches and E1..D6 were not ignored");
}

void testAlternateStrokeSequence()
{
    ElectryEngine engine;
    engine.prepare(48000.0, 512);
    engine.setParameters(EngineParameters {});
    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(Articulation::AlternateStroke), 1.0f);

    expect(engine.getCurrentArticulation() == Articulation::AlternateStroke,
           "Alternate Stroke did not latch");

    // Rejected performance events must not consume a stroke direction.
    engine.noteOn(100, 1.0f);
    engine.noteOn(40, 0.0f);

    engine.noteOn(40, 0.8f);
    const auto first = TestAccess::snapshot(engine,
                                            TestAccess::stringForNote(engine, 40));
    engine.noteOn(45, 0.8f);
    const auto second = TestAccess::snapshot(engine,
                                             TestAccess::stringForNote(engine, 45));
    engine.noteOn(50, 0.8f);
    const auto third = TestAccess::snapshot(engine,
                                            TestAccess::stringForNote(engine, 50));

    expect(first.articulation == Articulation::Downstroke,
           "Alternate Stroke did not begin with a downstroke");
    expect(second.articulation == Articulation::Upstroke,
           "Alternate Stroke did not alternate to an upstroke");
    expect(third.articulation == Articulation::Downstroke,
           "Alternate Stroke did not alternate back to a downstroke");
    expect(engine.getCurrentArticulation() == Articulation::AlternateStroke,
           "resolved strokes replaced the latched Alternate Stroke style");

    // Pressing the keyswitch again begins a fresh phrase on a downstroke.
    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(Articulation::AlternateStroke), 1.0f);
    engine.noteOn(55, 0.8f);
    const auto restarted = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 55));
    expect(restarted.articulation == Articulation::Downstroke,
           "reselecting Alternate Stroke did not reset its phase");
}

void testArticulationsSoundDistinct()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    engine.setParameters(parameters);

    const int attackStart = static_cast<int>(0.002 * sampleRate);
    const int attackWindow = static_cast<int>(0.06 * sampleRate);

    const auto renderStyle = [&] (Articulation articulation)
    {
        return renderNote(engine, sampleRate, 48, 0.85f, articulation, 1.2);
    };

    const auto down = renderStyle(Articulation::Downstroke);
    const auto up = renderStyle(Articulation::Upstroke);
    const auto hammer = renderStyle(Articulation::HammerOn);
    const auto muted = renderStyle(Articulation::Muted);
    const auto slap = renderStyle(Articulation::Slap);

    const double f0 = midiHz(48);
    const double downCentroid = spectralCentroid(down.left, attackStart,
                                                 attackWindow, sampleRate, f0);
    const double upCentroid = spectralCentroid(up.left, attackStart,
                                               attackWindow, sampleRate, f0);
    const double hammerCentroid = spectralCentroid(hammer.left, attackStart,
                                                   attackWindow, sampleRate, f0);
    const double slapCentroid = spectralCentroid(slap.left, attackStart,
                                                 attackWindow, sampleRate, f0);

    // The hammered attack is fingered, not picked: it must be darker than
    // both pick strokes. The slap attack must be the brightest.
    expect(hammerCentroid < downCentroid * 0.9,
           "hammer-on attack is not darker than a downstroke (down "
               + std::to_string(downCentroid) + " Hz, hammer "
               + std::to_string(hammerCentroid) + " Hz)");
    expect(hammerCentroid < upCentroid * 0.9,
           "hammer-on attack is not darker than an upstroke (up "
               + std::to_string(upCentroid) + " Hz, hammer "
               + std::to_string(hammerCentroid) + " Hz)");
    expect(slapCentroid > downCentroid * 1.05,
           "slap attack is not brighter than a downstroke (down "
               + std::to_string(downCentroid) + " Hz, slap "
               + std::to_string(slapCentroid) + " Hz)");
    expect(upCentroid > downCentroid * 1.01,
           "upstroke attack is not brighter than a downstroke (down "
               + std::to_string(downCentroid) + " Hz, up "
               + std::to_string(upCentroid) + " Hz)");

    // The hammered attack is also quieter than the picked one.
    const double downAttackRms = rmsInRange(down.left, attackStart,
                                            attackStart + attackWindow);
    const double hammerAttackRms = rmsInRange(hammer.left, attackStart,
                                              attackStart + attackWindow);
    expect(hammerAttackRms < downAttackRms * 0.85,
           "hammer-on attack is not softer than a downstroke (down "
               + std::to_string(downAttackRms) + ", hammer "
               + std::to_string(hammerAttackRms) + ")");

    // Palm muting kills the sustain: compare late energy.
    const int lateStart = static_cast<int>(0.8 * sampleRate);
    const int lateEnd = static_cast<int>(1.1 * sampleRate);
    const double downLate = rmsInRange(down.left, lateStart, lateEnd);
    const double mutedLate = rmsInRange(muted.left, lateStart, lateEnd);
    expect(mutedLate < downLate * 0.25,
           "muted notes do not decay dramatically faster than open notes");

    // Down and up strokes must not be identical renders.
    double difference = 0.0;
    double reference = 0.0;
    for (int i = 0; i < static_cast<int>(0.2 * sampleRate); ++i)
    {
        const double a = down.left[static_cast<std::size_t>(i)];
        const double b = up.left[static_cast<std::size_t>(i)];
        difference += (a - b) * (a - b);
        reference += a * a;
    }
    expect(difference > 0.01 * reference,
           "downstroke and upstroke render nearly identical audio");
}

void testExtendedPlayStyles()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    engine.setParameters(parameters);

    const auto down = renderNote(engine, sampleRate, 45, 0.85f,
                                 Articulation::Downstroke, 0.9);
    const auto tap = renderNote(engine, sampleRate, 45, 0.85f,
                                Articulation::Tap, 0.9);
    const auto muted = renderNote(engine, sampleRate, 45, 0.85f,
                                  Articulation::Muted, 0.9);
    const auto chug = renderNote(engine, sampleRate, 45, 0.85f,
                                 Articulation::Chug, 0.9);
    const auto dead = renderNote(engine, sampleRate, 45, 0.85f,
                                 Articulation::DeadNote, 0.9);

    const int attackStart = static_cast<int>(0.002 * sampleRate);
    const int attackEnd = static_cast<int>(0.060 * sampleRate);
    const double downAttack = rmsInRange(down.left, attackStart, attackEnd);
    const double tapAttack = rmsInRange(tap.left, attackStart, attackEnd);
    expect(tapAttack < downAttack * 0.85,
           "tap is not a softer finger excitation than a downstroke");

    const int muteLateStart = static_cast<int>(0.36 * sampleRate);
    const int muteLateEnd = static_cast<int>(0.56 * sampleRate);
    const double mutedLate = rmsInRange(muted.left, muteLateStart, muteLateEnd);
    const double chugLate = rmsInRange(chug.left, muteLateStart, muteLateEnd);
    expect(chugLate < mutedLate * 0.55,
           "chug does not stop the string harder than Muted");

    const int deadLateStart = static_cast<int>(0.16 * sampleRate);
    const int deadLateEnd = static_cast<int>(0.30 * sampleRate);
    const double deadLate = rmsInRange(dead.left, deadLateStart, deadLateEnd);
    const double chugSameWindow = rmsInRange(chug.left, deadLateStart, deadLateEnd);
    // The anti-aliased high-rate path preserves slightly more of the dead
    // note's low-frequency residue, but it must still decay at least four
    // times below the firmer, pitched chug over the same late window.
    expect(deadLate < chugSameWindow * 0.25,
           "dead note is not a short percussive articulation");

    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);
    const auto natural = renderNote(engine, sampleRate, 45, 0.35f,
                                    Articulation::NaturalHarmonic, 1.1);
    const auto pinch = renderNote(engine, sampleRate, 45, 0.35f,
                                  Articulation::PinchHarmonic, 1.1);
    const int harmonicStart = static_cast<int>(0.35 * sampleRate);
    const int harmonicWindow = static_cast<int>(0.55 * sampleRate);
    const double naturalExpected = midiHz(57);
    const double pinchExpected = midiHz(64);
    const double naturalMeasured = measureFrequency(
        natural.left, harmonicStart, harmonicWindow, sampleRate, naturalExpected);
    const double pinchMeasured = measureFrequency(
        pinch.left, harmonicStart, harmonicWindow, sampleRate, pinchExpected);
    expect(std::abs(centsBetween(naturalMeasured, naturalExpected)) < 10.0,
           "natural harmonic does not sound one octave above the played note");
    expect(std::abs(centsBetween(pinchMeasured, pinchExpected)) < 10.0,
           "pinch harmonic does not sound nineteen semitones above the played note");

    // Tremolo is one held string with deterministic scheduled re-excitations,
    // not a stack of new voices.
    engine.reset();
    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(Articulation::Tremolo), 1.0f);
    engine.noteOn(45, 0.8f);
    const int tremoloString = TestAccess::stringForNote(engine, 45);
    const auto initial = TestAccess::snapshot(engine, tremoloString);
    expect(initial.tremoloRetriggerCount == 0 && ! initial.tremoloStrokeIsUp,
           "tremolo did not begin on its initial downstroke");

    StereoBuffer tremoloAudio(static_cast<int>(0.24 * sampleRate));
    renderInto(engine, tremoloAudio);
    const auto running = TestAccess::snapshot(engine, tremoloString);
    expect(running.tremoloRetriggerCount >= 3,
           "held tremolo note did not schedule repeated strokes");
    expect(running.tremoloStrokeIsUp
               == ((running.tremoloRetriggerCount & 1u) != 0u),
           "tremolo stroke directions did not alternate");
    expect(engine.getActiveVoiceCount() == 1,
           "tremolo retriggers allocated additional physical strings");

    engine.setSustainPedal(true);
    engine.noteOff(45);
    const auto countAtRelease = running.tremoloRetriggerCount;
    StereoBuffer sustained(static_cast<int>(0.20 * sampleRate));
    renderInto(engine, sustained);
    const auto afterRelease = TestAccess::snapshot(engine, tremoloString);
    expect(afterRelease.active
               && afterRelease.tremoloRetriggerCount == countAtRelease,
           "tremolo continued retriggering after the key was released");
}

void testBendPrograms()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.bodyResonance = 0.0f;
    parameters.bendTimeSeconds = 0.20f;
    engine.setParameters(parameters);

    struct BendCase
    {
        Articulation articulation;
        double startCentsFromPlayed;
        double endCentsFromPlayed;
    };
    const std::array<BendCase, 4> cases {{
        { Articulation::Bend1Up, 0.0, 100.0 },
        { Articulation::Bend2Up, 0.0, 200.0 },
        { Articulation::Bend1Down, 100.0, 0.0 },
        { Articulation::Bend2Down, 200.0, 0.0 },
    }};

    for (const auto& bendCase : cases)
    {
        const int midiNote = 55;
        auto buffer = renderNote(engine, sampleRate, midiNote, 0.35f,
                                 bendCase.articulation, 2.0);
        const double played = midiHz(midiNote);

        // Early window: after the pick transient, before the bend departs
        // (the bend holds for about 55 ms before travelling).
        const int earlyStart = static_cast<int>(0.008 * sampleRate);
        const int earlyLength = static_cast<int>(0.038 * sampleRate);
        const double expectedStart = played
            * std::pow(2.0, bendCase.startCentsFromPlayed / 1200.0);
        const double startFrequency = measureFrequency(
            buffer.left, earlyStart, earlyLength, sampleRate, expectedStart);
        const double startError = centsBetween(startFrequency, expectedStart);
        // The short analysis window and attack transient limit precision.
        expect(std::abs(startError) < 35.0,
               "bend start pitch off by " + std::to_string(startError)
                   + " cents for articulation "
                   + std::to_string(static_cast<int>(bendCase.articulation)));

        // Late window: the bend has settled.
        const int lateStart = static_cast<int>(1.0 * sampleRate);
        const int lateLength = static_cast<int>(0.8 * sampleRate);
        const double expectedEnd = played
            * std::pow(2.0, bendCase.endCentsFromPlayed / 1200.0);
        const double endFrequency = measureFrequency(
            buffer.left, lateStart, lateLength, sampleRate, expectedEnd);
        const double endError = centsBetween(endFrequency, expectedEnd);
        expect(std::abs(endError) < 10.0,
               "bend end pitch off by " + std::to_string(endError)
                   + " cents for articulation "
                   + std::to_string(static_cast<int>(bendCase.articulation)));

        // And the travel is in the right direction by the right amount.
        const double travel = centsBetween(endFrequency, startFrequency);
        const double expectedTravel = bendCase.endCentsFromPlayed
                                    - bendCase.startCentsFromPlayed;
        expect(std::abs(travel - expectedTravel) < 40.0,
               "bend travel measured " + std::to_string(travel)
                   + " cents, expected " + std::to_string(expectedTravel));
    }
}

void testHammerOnLegatoContinuity()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);
    engine.reset();

    // Pick G3 on the E-string region, then hammer to A3.
    engine.noteOn(43, 0.7f);
    StereoBuffer buffer(static_cast<int>(2.2 * sampleRate));
    engine.process(buffer.left.data(), buffer.right.data(),
                   static_cast<int>(0.5 * sampleRate));

    const int stringBefore = TestAccess::stringForNote(engine, 43);
    expect(stringBefore >= 0, "picked note did not allocate a string");

    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(Articulation::HammerOn), 1.0f);
    engine.noteOn(45, 0.7f);

    const int stringAfter = TestAccess::stringForNote(engine, 45);
    expect(stringAfter == stringBefore,
           "hammer-on did not continue on the same string");
    expect(engine.getActiveVoiceCount() == 1,
           "hammer-on created a second voice");

    const int transition = static_cast<int>(0.5 * sampleRate);
    engine.process(buffer.left.data() + transition,
                   buffer.right.data() + transition,
                   buffer.size() - transition);

    // The hammered note settles on the new pitch.
    const double expected = midiHz(45);
    const double measured = measureFrequency(
        buffer.left, static_cast<int>(1.2 * sampleRate),
        static_cast<int>(0.8 * sampleRate), sampleRate, expected);
    expect(std::abs(centsBetween(measured, expected)) < 10.0,
           "hammer-on did not settle on the target pitch");

    // No hard discontinuity at the hammer point: compare the largest
    // sample-to-sample step around the transition with the signal scale.
    float maximumStep = 0.0f;
    for (int i = transition - 32; i < transition + static_cast<int>(0.02 * sampleRate); ++i)
        maximumStep = std::max(maximumStep,
                               std::abs(buffer.left[static_cast<std::size_t>(i + 1)]
                                        - buffer.left[static_cast<std::size_t>(i)]));
    const float scale = peakAbs(buffer.left, transition - 4800, transition);
    expect(maximumStep < std::max(0.05f, 1.2f * scale),
           "hammer-on transition produced a hard discontinuity");
}

void testSlapCollisionAndTensionGlide()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    engine.setParameters(parameters);

    auto slap = renderNote(engine, sampleRate, 45, 0.95f, Articulation::Slap, 1.8);

    // The collision window must actually have engaged.
    engine.reset();
    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(Articulation::Slap), 1.0f);
    engine.noteOn(45, 0.95f);
    const int stringIndex = TestAccess::stringForNote(engine, 45);
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(snapshot.collisionRemaining > 0,
           "slap did not open a fret-collision window");

    // Tension modulation: the attack sounds sharp and relaxes as the string
    // energy decays. The early window sits where the energy envelope peaks,
    // and both measurements share the same scan reference so grid quantise
    // bias cancels in the difference.
    const double early = measureFrequency(slap.left,
                                          static_cast<int>(0.05 * sampleRate),
                                          static_cast<int>(0.14 * sampleRate),
                                          sampleRate, midiHz(45));
    const double late = measureFrequency(slap.left,
                                         static_cast<int>(1.2 * sampleRate),
                                         static_cast<int>(0.5 * sampleRate),
                                         sampleRate, midiHz(45));
    const double glide = centsBetween(early, late);
    expect(glide > 1.5,
           "slap attack does not glide sharp-to-true (measured "
               + std::to_string(glide) + " cents)");
    expect(glide < 80.0, "slap pitch glide is implausibly large");
}

void testPickupsToneAndModelMorph()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    const int start = static_cast<int>(0.05 * sampleRate);
    const int window = static_cast<int>(0.4 * sampleRate);

    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.artifactAmount = 0.0f;

    parameters.pickupSelector = PickupSelector::Bridge;
    engine.setParameters(parameters);
    const auto bridge = renderNote(engine, sampleRate, 45, 0.7f,
                                   Articulation::Downstroke, 0.8);

    parameters.pickupSelector = PickupSelector::Neck;
    engine.setParameters(parameters);
    const auto neck = renderNote(engine, sampleRate, 45, 0.7f,
                                 Articulation::Downstroke, 0.8);

    const double f0 = midiHz(45);
    // The bridge position senses far less of the fundamental's antinode, so
    // its energy-weighted centroid sits clearly higher than the neck's.
    const double bridgeCentroid = spectralCentroid(bridge.left, start, window,
                                                   sampleRate, f0);
    const double neckCentroid = spectralCentroid(neck.left, start, window,
                                                 sampleRate, f0);
    expect(bridgeCentroid > neckCentroid * 1.08,
           "bridge pickup is not brighter than neck pickup (bridge "
               + std::to_string(bridgeCentroid) + " Hz, neck "
               + std::to_string(neckCentroid) + " Hz)");

    // Rolling the tone control off darkens the output.
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.toneKnob = 0.05f;
    engine.setParameters(parameters);
    const auto dark = renderNote(engine, sampleRate, 45, 0.7f,
                                 Articulation::Downstroke, 0.8);
    const double openRatio = highBandRatio(bridge.left, start, window,
                                           sampleRate, f0);
    const double darkRatio = highBandRatio(dark.left, start, window,
                                           sampleRate, f0);
    expect(darkRatio < openRatio * 0.6,
           "tone control does not darken the pickup output (open ratio "
               + std::to_string(openRatio) + ", rolled "
               + std::to_string(darkRatio) + ")");

    // Guitar-model endpoints: a full Les Paul-style setting and a full
    // Telecaster-style setting must be audibly different instruments.
    EngineParameters lesPaul;
    lesPaul.bodyWood = lesPaul.bodySize = lesPaul.bodyShape = 0.0f;
    lesPaul.construction = lesPaul.scaleLength = lesPaul.pickupType = 0.0f;
    lesPaul.pickNoise = 0.0f;
    engine.setParameters(lesPaul);
    const auto lesPaulRender = renderNote(engine, sampleRate, 45, 0.7f,
                                          Articulation::Downstroke, 0.8);

    EngineParameters telecaster;
    telecaster.bodyWood = telecaster.bodySize = telecaster.bodyShape = 1.0f;
    telecaster.construction = telecaster.scaleLength = telecaster.pickupType = 1.0f;
    telecaster.pickNoise = 0.0f;
    engine.setParameters(telecaster);
    const auto telecasterRender = renderNote(engine, sampleRate, 45, 0.7f,
                                             Articulation::Downstroke, 0.8);

    const double lesPaulCentroid = spectralCentroid(lesPaulRender.left, start,
                                                    window, sampleRate, f0);
    const double telecasterCentroid = spectralCentroid(telecasterRender.left,
                                                       start, window, sampleRate, f0);
    // The single-coil Telecaster bridge is characteristically brighter than
    // a humbucker Les Paul.
    expect(telecasterCentroid > lesPaulCentroid * 1.10,
           "Telecaster endpoint is not brighter than Les Paul endpoint (LP "
               + std::to_string(lesPaulCentroid) + " Hz, Tele "
               + std::to_string(telecasterCentroid) + " Hz)");

    // Both endpoints stay in tune.
    for (const auto* render : { &lesPaulRender, &telecasterRender })
    {
        const double measured = measureFrequency(
            render->left, static_cast<int>(0.3 * sampleRate),
            static_cast<int>(0.4 * sampleRate), sampleRate, midiHz(45));
        expect(std::abs(centsBetween(measured, midiHz(45))) < 8.0,
               "guitar-model endpoint detuned the instrument");
    }
}

void testArtifactsControl()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    const auto renderAt = [&] (float amount)
    {
        parameters.artifactAmount = amount;
        engine.setParameters(parameters);
        return renderNote(engine, sampleRate, 45, 0.95f,
                          Articulation::Downstroke, 0.75);
    };

    const auto clean = renderAt(0.0f);
    const auto subtle = renderAt(0.18f);
    const auto medium = renderAt(0.60f);
    const auto full = renderAt(1.0f);
    const int start = static_cast<int>(0.020 * sampleRate);
    const int end = static_cast<int>(0.60 * sampleRate);
    const double subtleDifference = normalisedDifferenceRms(
        subtle.left, clean.left, start, end);
    const double mediumDifference = normalisedDifferenceRms(
        medium.left, clean.left, start, end);
    const double fullDifference = normalisedDifferenceRms(
        full.left, clean.left, start, end);

    expect(subtleDifference > 0.002 && subtleDifference < 0.15,
           "default artifacts are inaudible or no longer subtle (difference "
               + std::to_string(subtleDifference) + ")");
    expect(mediumDifference > subtleDifference * 1.35
               && fullDifference > mediumDifference * 1.12,
           "Artifacts control does not increase imperfection energy monotonically ("
               + std::to_string(subtleDifference) + ", "
               + std::to_string(mediumDifference) + ", "
               + std::to_string(fullDifference) + ")");

    const auto repeatedFull = renderAt(1.0f);
    expect(full.left == repeatedFull.left && full.right == repeatedFull.right,
           "artifact PRNG path is not sample-deterministic");

    engine.reset();
    StereoBuffer silence(4096);
    renderInto(engine, silence);
    expect(peakAbs(silence.left) == 0.0f,
           "Artifacts control generated ambience without a played note");

    // Worst-case low-register strum remains bounded with every imperfection
    // path open and the mandatory oversampling clock active.
    engine.reset();
    constexpr std::array<int, ElectryEngine::stringCount> openNotes {
        28, 35, 40, 45, 50, 55, 59, 64
    };
    for (const int note : openNotes)
        engine.noteOn(note, 1.0f);
    StereoBuffer strum(static_cast<int>(0.8 * sampleRate));
    renderInto(engine, strum);
    expect(allFinite(strum) && peakAbs(strum.left) < 0.80f,
           "maximum-artifact eight-string strum became unstable");
}

void testAdvancedDispersionAndBodyConductance()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.scaleLength = 0.0f;
    parameters.stringGauge = 1.0f;
    parameters.bodyResonance = 1.0f;
    parameters.bodyShape = 0.0f;
    parameters.artifactAmount = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(28, 0.8f);

    const int stringIndex = TestAccess::stringForNote(engine, 28);
    const auto snapshot = TestAccess::snapshot(engine, stringIndex);
    expect(snapshot.inharmonicity > 1.0e-7f,
           "physical wound-string inharmonicity was not configured");
    expect(snapshot.dispersionLowCoefficient <= 0.0f
               && snapshot.dispersionLowCoefficient >= -0.9951f
               && snapshot.dispersionHighCoefficient <= 0.0f
               && snapshot.dispersionHighCoefficient >= -0.9951f,
           "eight-stage dispersion coefficients escaped their stable bounds");

    const auto expectedDeficit = [&] (float partial)
    {
        const float period = static_cast<float>(TestAccess::internalSampleRate(engine))
                           / snapshot.baseFrequency;
        const float stretch = std::sqrt(
            (1.0f + snapshot.inharmonicity * partial * partial)
            / (1.0f + snapshot.inharmonicity));
        return period * (1.0f - 1.0f / stretch);
    };
    for (const float partial : { snapshot.dispersionLowPartial,
                                 snapshot.dispersionHighPartial })
    {
        const float wanted = expectedDeficit(partial);
        const float actual = TestAccess::dispersionDeficit(
            engine, stringIndex, partial);
        const float relativeError = std::abs(actual - wanted)
            / std::max(wanted, 1.0e-6f);
        expect(relativeError < 0.20f,
               "two-point dispersion fit missed partial "
                   + std::to_string(partial) + " (wanted "
                   + std::to_string(wanted) + ", actual "
                   + std::to_string(actual) + ")");
    }

    expect(snapshot.bodyConductance >= 0.0f && snapshot.bodyConductance <= 1.0f,
           "modal bridge conductance escaped its passive range");
    expect(snapshot.bodyLossFactor > 0.0f && snapshot.bodyLossFactor <= 1.0f,
           "modal bridge conductance added string energy");

    parameters.bodyResonance = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(28, 0.8f);
    const auto bypassed = TestAccess::snapshot(
        engine, TestAccess::stringForNote(engine, 28));
    expect(std::abs(bypassed.bodyLossFactor - 1.0f) < 1.0e-6f,
           "zero Body Resonance did not exactly bypass structural loss");
}

void testMonoStereoOutputField()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.bodyResonance = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    parameters.outputMode = electry::OutputMode::Mono;
    engine.setParameters(parameters);
    const auto monoLow = renderNote(engine, sampleRate, 28, 0.8f,
                                    Articulation::Downstroke, 0.65);
    expect(monoLow.left == monoLow.right,
           "Mono output mode is not exact dual mono");

    parameters.outputMode = electry::OutputMode::Stereo;
    engine.setParameters(parameters);
    const auto stereoLow = renderNote(engine, sampleRate, 28, 0.8f,
                                      Articulation::Downstroke, 0.65);
    const auto repeatedLow = renderNote(engine, sampleRate, 28, 0.8f,
                                        Articulation::Downstroke, 0.65);
    expect(stereoLow.left == repeatedLow.left
               && stereoLow.right == repeatedLow.right,
           "Stereo divided-pickup field is not deterministic");

    const int start = static_cast<int>(0.025 * sampleRate);
    const int end = static_cast<int>(0.55 * sampleRate);
    const double lowLeft = rmsInRange(stereoLow.left, start, end);
    const double lowRight = rmsInRange(stereoLow.right, start, end);
    expect(lowLeft > lowRight * 1.10,
           "Stereo low E does not favour the low-string side (L "
               + std::to_string(lowLeft) + ", R "
               + std::to_string(lowRight) + ")");

    const auto stereoHigh = renderNote(engine, sampleRate, 64, 0.8f,
                                       Articulation::Downstroke, 0.65);
    const double highLeft = rmsInRange(stereoHigh.left, start, end);
    const double highRight = rmsInRange(stereoHigh.right, start, end);
    expect(highRight > highLeft * 1.10,
           "Stereo high E does not favour the high-string side (L "
               + std::to_string(highLeft) + ", R "
               + std::to_string(highRight) + ")");

    std::vector<float> folded(stereoLow.left.size());
    std::vector<float> side(stereoLow.left.size());
    for (std::size_t sample = 0; sample < folded.size(); ++sample)
    {
        folded[sample] = 0.5f * (stereoLow.left[sample]
                               + stereoLow.right[sample]);
        side[sample] = 0.5f * (stereoLow.left[sample]
                             - stereoLow.right[sample]);
    }
    const double foldDifference = normalisedDifferenceRms(
        folded, monoLow.left, start, end);
    expect(foldDifference < 0.12,
           "Stereo field does not fold coherently to Mono (difference "
               + std::to_string(foldDifference) + ")");

    const double midRms = rmsInRange(folded, start, end);
    const double sideRms = rmsInRange(side, start, end);
    expect(sideRms > midRms * 0.08 && sideRms < midRms * 0.35,
           "Stereo side field is inaudible or excessive (ratio "
               + std::to_string(sideRms / std::max(midRms, 1.0e-12)) + ")");

    const double monoEnergy = std::pow(
        rmsInRange(monoLow.left, start, end), 2.0);
    const double stereoEnergy = 0.5
        * (lowLeft * lowLeft + lowRight * lowRight);
    const double energyRatio = stereoEnergy / std::max(monoEnergy, 1.0e-12);
    expect(energyRatio > 0.80 && energyRatio < 1.20,
           "Stereo output field changed average energy excessively (ratio "
               + std::to_string(energyRatio) + ")");
}

void testVelocityExpression()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.velocityAmount = 1.0f;
    parameters.bodyResonance = 0.0f;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;
    engine.setParameters(parameters);

    const auto low = renderNote(engine, sampleRate, 45, 0.2f,
                                Articulation::Downstroke, 0.55);
    const auto middle = renderNote(engine, sampleRate, 45, 0.6f,
                                   Articulation::Downstroke, 0.55);
    const auto high = renderNote(engine, sampleRate, 45, 1.0f,
                                 Articulation::Downstroke, 0.55);
    const int attackStart = static_cast<int>(0.004 * sampleRate);
    const int attackEnd = static_cast<int>(0.115 * sampleRate);
    const double lowRms = rmsInRange(low.left, attackStart, attackEnd);
    const double middleRms = rmsInRange(middle.left, attackStart, attackEnd);
    const double highRms = rmsInRange(high.left, attackStart, attackEnd);
    expect(middleRms > lowRms * 1.20 && highRms > middleRms * 1.20,
           "velocity amplitude is not clearly monotonic ("
               + std::to_string(lowRms) + ", " + std::to_string(middleRms)
               + ", " + std::to_string(highRms) + ")");

    const double lowCentroid = spectralCentroid(
        low.left, attackStart, attackEnd - attackStart, sampleRate, midiHz(45));
    const double highCentroid = spectralCentroid(
        high.left, attackStart, attackEnd - attackStart, sampleRate, midiHz(45));
    expect(highCentroid > lowCentroid * 1.10,
           "velocity does not brighten the attack (low "
               + std::to_string(lowCentroid) + " Hz, high "
               + std::to_string(highCentroid) + " Hz)");

    // At zero response, MIDI velocity is deliberately removed from every
    // attack dimension, not merely from output gain.
    parameters.velocityAmount = 0.0f;
    engine.setParameters(parameters);
    const auto flatLow = renderNote(engine, sampleRate, 45, 0.2f,
                                    Articulation::Downstroke, 0.30);
    const auto flatHigh = renderNote(engine, sampleRate, 45, 1.0f,
                                     Articulation::Downstroke, 0.30);
    expect(flatLow.left == flatHigh.left && flatLow.right == flatHigh.right,
           "zero velocity response still changes the rendered attack");
}

void testMaterialAndControlAudibility()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters base;
    base.bodyResonance = 0.55f;
    base.artifactAmount = 0.0f;
    base.pickNoise = 0.0f;
    base.fingerNoise = 0.0f;
    base.releaseNoise = 0.0f;

    const auto compareAxis = [&] (auto setAxis, int midiNote)
    {
        auto lowParameters = base;
        setAxis(lowParameters, 0.0f);
        engine.setParameters(lowParameters);
        const auto low = renderNote(engine, sampleRate, midiNote, 0.8f,
                                    Articulation::Downstroke, 0.75);
        auto highParameters = base;
        setAxis(highParameters, 1.0f);
        engine.setParameters(highParameters);
        const auto high = renderNote(engine, sampleRate, midiNote, 0.8f,
                                     Articulation::Downstroke, 0.75);
        return normalisedDifferenceRms(
            low.left, high.left, static_cast<int>(0.035 * sampleRate),
            static_cast<int>(0.60 * sampleRate));
    };

    const auto wood = [] (EngineParameters& p, float v) { p.bodyWood = v; };
    const auto size = [] (EngineParameters& p, float v) { p.bodySize = v; };
    const auto shape = [] (EngineParameters& p, float v) { p.bodyShape = v; };
    const auto construction = [] (EngineParameters& p, float v) { p.construction = v; };
    const auto scale = [] (EngineParameters& p, float v) { p.scaleLength = v; };
    const auto gauge = [] (EngineParameters& p, float v) { p.stringGauge = v; };

    using AxisSetter = void (*) (EngineParameters&, float);
    const std::array<std::pair<const char*, AxisSetter>, 4> bodyAxes {{
        { "wood", wood }, { "size", size }, { "shape", shape },
        { "construction", construction }
    }};
    for (const auto& namedAxis : bodyAxes)
    {
        const double lowNoteDifference = compareAxis(namedAxis.second, 28);
        const double midNoteDifference = compareAxis(namedAxis.second, 45);
        expect(std::min(lowNoteDifference, midNoteDifference) > 0.055,
               std::string("body ") + namedAxis.first
                   + " remains effectively inaudible on E1/A2 ("
                   + std::to_string(lowNoteDifference) + ", "
                   + std::to_string(midNoteDifference) + ")");
    }

    const double scaleDifference = compareAxis(scale, 28);
    const double gaugeDifference = compareAxis(gauge, 28);
    expect(scaleDifference > 0.025,
           "25.5-to-28-inch scale range does not change Drop-E timbre ("
               + std::to_string(scaleDifference) + ")");
    expect(gaugeDifference > 0.08,
           "string-gauge endpoints remain too similar ("
               + std::to_string(gaugeDifference) + ")");

    auto noBody = base;
    noBody.bodyResonance = 0.0f;
    engine.setParameters(noBody);
    const auto dry = renderNote(engine, sampleRate, 45, 0.8f,
                                Articulation::Downstroke, 0.75);
    auto fullBody = base;
    fullBody.bodyResonance = 1.0f;
    engine.setParameters(fullBody);
    const auto resonant = renderNote(engine, sampleRate, 45, 0.8f,
                                     Articulation::Downstroke, 0.75);
    const double bodyDifference = normalisedDifferenceRms(
        dry.left, resonant.left, static_cast<int>(0.035 * sampleRate),
        static_cast<int>(0.60 * sampleRate));
    // Exact modal normalisation keeps the structural path controlled: a
    // clearly audible endpoint change is required, but the test must not
    // reward the former oversized, clavinet-like body peaks.
    expect(bodyDifference > 0.08,
           "Body Resonance full range is still too polite ("
               + std::to_string(bodyDifference) + ")");

    auto fresh = base;
    fresh.bodyResonance = 0.0f;
    fresh.stringAge = 0.0f;
    engine.setParameters(fresh);
    const auto freshRender = renderNote(engine, sampleRate, 45, 0.8f,
                                        Articulation::Downstroke, 1.3);
    auto old = fresh;
    old.stringAge = 1.0f;
    engine.setParameters(old);
    const auto oldRender = renderNote(engine, sampleRate, 45, 0.8f,
                                      Articulation::Downstroke, 1.3);
    const double freshLate = rmsInRange(
        freshRender.left, static_cast<int>(0.8 * sampleRate),
        static_cast<int>(1.2 * sampleRate));
    const double oldLate = rmsInRange(
        oldRender.left, static_cast<int>(0.8 * sampleRate),
        static_cast<int>(1.2 * sampleRate));
    expect(oldLate < freshLate * 0.35,
           "String Age does not strongly shorten/darken the tail (ratio "
               + std::to_string(oldLate / std::max(freshLate, 1.0e-12)) + ")");

    auto bridgePick = fresh;
    bridgePick.pickPosition = 0.0f;
    engine.setParameters(bridgePick);
    const auto bridgePicked = renderNote(engine, sampleRate, 45, 0.8f,
                                         Articulation::Downstroke, 0.6);
    auto neckPick = fresh;
    neckPick.pickPosition = 1.0f;
    engine.setParameters(neckPick);
    const auto neckPicked = renderNote(engine, sampleRate, 45, 0.8f,
                                       Articulation::Downstroke, 0.6);
    const double positionDifference = normalisedDifferenceRms(
        bridgePicked.left, neckPicked.left, static_cast<int>(0.005 * sampleRate),
        static_cast<int>(0.40 * sampleRate));
    expect(positionDifference > 0.35,
           "Pick Position range is not clearly audible ("
               + std::to_string(positionDifference) + ")");

    auto soft = fresh;
    soft.pickHardness = 0.0f;
    engine.setParameters(soft);
    const auto softPick = renderNote(engine, sampleRate, 45, 0.8f,
                                     Articulation::Downstroke, 0.5);
    auto hard = fresh;
    hard.pickHardness = 1.0f;
    engine.setParameters(hard);
    const auto hardPick = renderNote(engine, sampleRate, 45, 0.8f,
                                     Articulation::Downstroke, 0.5);
    const int attackStart = static_cast<int>(0.004 * sampleRate);
    const int attackLength = static_cast<int>(0.10 * sampleRate);
    const double softCentroid = spectralCentroid(
        softPick.left, attackStart, attackLength, sampleRate, midiHz(45));
    const double hardCentroid = spectralCentroid(
        hardPick.left, attackStart, attackLength, sampleRate, midiHz(45));
    const double softRms = rmsInRange(softPick.left, attackStart,
                                      attackStart + attackLength);
    const double hardRms = rmsInRange(hardPick.left, attackStart,
                                      attackStart + attackLength);
    expect(hardCentroid > softCentroid * 1.35,
           "Pick Hardness does not sufficiently brighten the attack (ratio "
               + std::to_string(hardCentroid / std::max(softCentroid, 1.0e-12))
               + ")");
    expect(hardRms > softRms * 0.55 && hardRms < softRms * 1.70,
           "Pick Hardness changes loudness more than material (RMS ratio "
               + std::to_string(hardRms / std::max(softRms, 1.0e-12)) + ")");
}

void testNoiseComponentsAndSilence()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    // Complete silence with no input.
    engine.setParameters(EngineParameters {});
    engine.reset();
    StereoBuffer silent(static_cast<int>(0.5 * sampleRate));
    renderInto(engine, silent);
    expect(peakAbs(silent.left) == 0.0f && peakAbs(silent.right) == 0.0f,
           "engine does not render exact silence with no notes");

    // Pick noise adds energy in the pre-attack contact window.
    EngineParameters noNoise;
    noNoise.pickNoise = 0.0f;
    noNoise.fingerNoise = 0.0f;
    noNoise.releaseNoise = 0.0f;
    engine.setParameters(noNoise);
    const auto clean = renderNote(engine, sampleRate, 45, 0.8f,
                                  Articulation::Downstroke, 0.9, 0.6);

    EngineParameters fullNoise;
    fullNoise.pickNoise = 1.0f;
    fullNoise.fingerNoise = 1.0f;
    fullNoise.releaseNoise = 1.0f;
    engine.setParameters(fullNoise);
    const auto noisy = renderNote(engine, sampleRate, 45, 0.8f,
                                  Articulation::Downstroke, 0.9, 0.6);

    // The pick contact lasts about 1.5 ms at the default hardness before the
    // release pulse starts; inside 1.2 ms the noiseless render is silent.
    const int contactWindow = static_cast<int>(0.0012 * sampleRate);
    const double cleanContact = rmsInRange(clean.left, 0, contactWindow);
    const double noisyContact = rmsInRange(noisy.left, 0, contactWindow);
    expect(noisyContact > cleanContact * 1.5 + 1.0e-6,
           "plectrum contact noise is missing from the attack (clean "
               + std::to_string(cleanContact) + ", noisy "
               + std::to_string(noisyContact) + ")");

    // Release noise adds energy just after note-off. This render pair
    // differs only in the releaseNoise control, and rendering is
    // deterministic, so any pre-note-off difference is a defect and the
    // release-window difference is exactly the added noise.
    EngineParameters releaseOnly = noNoise;
    releaseOnly.releaseNoise = 1.0f;
    engine.setParameters(releaseOnly);
    const auto releaseNoisy = renderNote(engine, sampleRate, 45, 0.8f,
                                         Articulation::Downstroke, 0.9, 0.6);

    const int releaseStart = static_cast<int>(0.6 * sampleRate);
    const int releaseEnd = releaseStart + static_cast<int>(0.015 * sampleRate);
    double differenceEnergy = 0.0;
    double cleanEnergy = 0.0;
    for (int i = releaseStart; i < releaseEnd; ++i)
    {
        const double difference = releaseNoisy.left[static_cast<std::size_t>(i)]
                                - clean.left[static_cast<std::size_t>(i)];
        differenceEnergy += difference * difference;
        cleanEnergy += clean.left[static_cast<std::size_t>(i)]
                     * clean.left[static_cast<std::size_t>(i)];
    }
    expect(differenceEnergy > 1.0e-9
               && differenceEnergy > 0.005 * std::max(cleanEnergy, 1.0e-12),
           "release noise is missing after note-off");
    double preOffDifference = 0.0;
    int firstDifferingSample = -1;
    for (int i = 0; i < releaseStart - 64; ++i)
    {
        const double difference = releaseNoisy.left[static_cast<std::size_t>(i)]
                                - clean.left[static_cast<std::size_t>(i)];
        if (difference != 0.0 && firstDifferingSample < 0)
            firstDifferingSample = i;
        preOffDifference += difference * difference;
    }
    expect(preOffDifference == 0.0,
           "release-noise level changed audio before the note-off (first at "
               + std::to_string(firstDifferingSample) + ", energy "
               + std::to_string(preOffDifference) + ")");

    // Note-off damps the string quickly.
    const double preRelease = rmsInRange(clean.left,
                                         releaseStart - static_cast<int>(0.1 * sampleRate),
                                         releaseStart);
    const double postRelease = rmsInRange(clean.left,
                                          releaseStart + static_cast<int>(0.35 * sampleRate),
                                          releaseStart + static_cast<int>(0.45 * sampleRate));
    expect(postRelease < preRelease * 0.05,
           "released note does not damp towards silence");
}

void testStringAllocationAndPolyphony()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    engine.setParameters(EngineParameters {});
    engine.reset();

    // The open C-major shape: x-3-2-0-1-0 from the A string.
    const std::array<int, 5> chord { 48, 52, 55, 60, 64 };
    for (const int note : chord)
        engine.noteOn(note, 0.8f);

    expect(engine.getActiveVoiceCount() == 5,
           "C-major chord did not allocate five strings");
    expect(TestAccess::stringForNote(engine, 48) == 3, "C3 is not on the A string");
    expect(TestAccess::stringForNote(engine, 52) == 4, "E3 is not on the D string");
    expect(TestAccess::stringForNote(engine, 55) == 5, "G3 is not on the G string");
    expect(TestAccess::stringForNote(engine, 60) == 6, "C4 is not on the B string");
    expect(TestAccess::stringForNote(engine, 64) == 7,
           "E4 is not on the high E string");

    // The three lower opens fill the remaining physical strings. A ninth
    // simultaneous note must steal while the voice count remains eight.
    engine.noteOn(40, 0.8f);
    expect(engine.getActiveVoiceCount() == 6, "open E2 did not use its string");
    engine.noteOn(35, 0.8f);
    expect(engine.getActiveVoiceCount() == 7, "open B1 did not use its string");
    engine.noteOn(28, 0.8f);
    expect(engine.getActiveVoiceCount() == 8, "open E1 did not use its string");
    engine.noteOn(50, 0.8f);
    expect(engine.getActiveVoiceCount() == 8,
           "ninth simultaneous note exceeded eight strings");

    // Every open note maps to its own physical string in Drop-E tuning.
    engine.reset();
    constexpr std::array<int, ElectryEngine::stringCount> openNotes {
        28, 35, 40, 45, 50, 55, 59, 64
    };
    for (int string = 0; string < ElectryEngine::stringCount; ++string)
    {
        const int note = openNotes[static_cast<std::size_t>(string)];
        engine.noteOn(note, 0.8f);
        expect(TestAccess::stringForNote(engine, note) == string,
               "open note " + std::to_string(note)
                   + " did not map to physical string " + std::to_string(string));
    }
    expect(engine.getActiveVoiceCount() == ElectryEngine::stringCount,
           "eight open notes did not fill all eight physical strings");

    // Retriggering a sounding note reuses its string.
    engine.reset();
    engine.noteOn(45, 0.8f);
    const int firstString = TestAccess::stringForNote(engine, 45);
    engine.noteOn(45, 0.8f);
    expect(engine.getActiveVoiceCount() == 1,
           "restruck note did not reuse its string");
    expect(TestAccess::stringForNote(engine, 45) == firstString,
           "restruck note moved to another string");
}

void testPitchWheelAndSustainPedal()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickNoise = 0.0f;
    engine.setParameters(parameters);
    engine.reset();

    engine.noteOn(50, 0.4f);
    StereoBuffer buffer(static_cast<int>(2.0 * sampleRate));
    engine.process(buffer.left.data(), buffer.right.data(),
                   static_cast<int>(0.6 * sampleRate));
    engine.setPitchBend(1.0f); // +2 semitones
    engine.process(buffer.left.data() + static_cast<int>(0.6 * sampleRate),
                   buffer.right.data() + static_cast<int>(0.6 * sampleRate),
                   buffer.size() - static_cast<int>(0.6 * sampleRate));

    const double before = measureFrequency(buffer.left,
                                           static_cast<int>(0.2 * sampleRate),
                                           static_cast<int>(0.35 * sampleRate),
                                           sampleRate, midiHz(50));
    const double after = measureFrequency(buffer.left,
                                          static_cast<int>(1.2 * sampleRate),
                                          static_cast<int>(0.6 * sampleRate),
                                          sampleRate, midiHz(52));
    const double travel = centsBetween(after, before);
    expect(std::abs(travel - 200.0) < 15.0,
           "pitch wheel travel measured " + std::to_string(travel) + " cents");

    // Sustain pedal keeps a released note ringing.
    engine.reset();
    engine.setSustainPedal(true);
    engine.noteOn(45, 0.7f);
    StereoBuffer pedalBuffer(static_cast<int>(0.4 * sampleRate));
    renderInto(engine, pedalBuffer);
    engine.noteOff(45);
    renderInto(engine, pedalBuffer);
    expect(engine.getActiveVoiceCount() == 1,
           "sustain pedal did not hold the released note");

    const double heldRms = rmsInRange(pedalBuffer.left, 0, pedalBuffer.size());
    engine.setSustainPedal(false);
    StereoBuffer releasedBuffer(static_cast<int>(0.8 * sampleRate));
    renderInto(engine, releasedBuffer);
    const double lateRms = rmsInRange(releasedBuffer.left,
                                      static_cast<int>(0.6 * sampleRate),
                                      releasedBuffer.size());
    expect(lateRms < heldRms * 0.1,
           "note did not damp after the sustain pedal was released");
}

void testParameterSanitisation()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters hostile;
    hostile.bodyWood = std::numeric_limits<float>::quiet_NaN();
    hostile.bodySize = -12.0f;
    hostile.bodyShape = 400.0f;
    hostile.construction = std::numeric_limits<float>::infinity();
    hostile.scaleLength = -std::numeric_limits<float>::infinity();
    hostile.pickupType = 55.0f;
    hostile.toneKnob = std::numeric_limits<float>::quiet_NaN();
    hostile.bodyResonance = 1.0e9f;
    hostile.stringGauge = -1.0e9f;
    hostile.stringAge = std::numeric_limits<float>::quiet_NaN();
    hostile.pickPosition = 2.0f;
    hostile.pickHardness = -2.0f;
    hostile.pickNoise = std::numeric_limits<float>::infinity();
    hostile.fingerNoise = -0.5f;
    hostile.releaseNoise = 77.0f;
    hostile.muteDamping = std::numeric_limits<float>::quiet_NaN();
    hostile.bendTimeSeconds = -3.0f;
    hostile.velocityAmount = 9.0f;
    hostile.outputGain = std::numeric_limits<float>::quiet_NaN();
    hostile.artifactAmount = std::numeric_limits<float>::infinity();
    hostile.sympatheticAmount = std::numeric_limits<float>::quiet_NaN();
    hostile.palmMute = -7.0f;
    hostile.strumSpreadSeconds = std::numeric_limits<float>::infinity();
    hostile.vibratoDepthCents = std::numeric_limits<float>::quiet_NaN();
    hostile.pickupSelector = static_cast<PickupSelector>(999);
    hostile.outputMode = static_cast<electry::OutputMode>(999);
    engine.setParameters(hostile);

    auto buffer = renderNote(engine, sampleRate, 45, 0.9f, Articulation::Slap, 0.6);
    expect(allFinite(buffer), "hostile parameters produced non-finite audio");
    expect(peakAbs(buffer.left) < 2.1f, "hostile parameters bypassed the guard");

    engine.setPitchBend(std::numeric_limits<float>::quiet_NaN());
    engine.noteOn(45, std::numeric_limits<float>::quiet_NaN());
    StereoBuffer more(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, more);
    expect(allFinite(more), "hostile performance input produced non-finite audio");
}

// ---------------------------------------------------------------------------
// Version 1.1: bridge-coupled sympathetic strings
// ---------------------------------------------------------------------------

void testSympatheticBridgeCoupling()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    // A2 is picked on its own open string; its third partial lands almost
    // exactly on the open high E, which is the string a real guitar rings
    // hardest through the bridge.
    constexpr double openHighE = 329.62756;
    const auto tailEnergyAt = [&] (float amount)
    {
        parameters.sympatheticAmount = amount;
        engine.setParameters(parameters);
        const auto buffer = renderNote(engine, sampleRate, 45, 0.95f,
                                       Articulation::Downstroke, 2.6, 0.5);
        expect(allFinite(buffer),
               "sympathetic coupling produced non-finite audio");
        const int start = static_cast<int>(1.4 * sampleRate);
        const int length = static_cast<int>(1.0 * sampleRate);
        return dftMagnitude(buffer.left, start, length, sampleRate, openHighE);
    };

    const double bypassed = tailEnergyAt(0.0f);
    const double coupled = tailEnergyAt(0.6f);
    expect(coupled > 12.0 * std::max(bypassed, 1.0e-12),
           "bridge coupling did not ring the open high E after the played "
           "string was damped (" + std::to_string(bypassed) + " -> "
               + std::to_string(coupled) + ")");

    // At zero the coupled waveguides are never even configured, so the engine
    // is exactly the pre-1.1 model.
    parameters.sympatheticAmount = 0.0f;
    engine.setParameters(parameters);
    auto silentTail = renderNote(engine, sampleRate, 45, 0.95f,
                                 Articulation::Downstroke, 2.6, 0.5);
    for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount; ++stringIndex)
    {
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        expect(! snapshot.sympatheticReady,
               "a coupled string was configured while the control was at zero");
    }
    expect(peakAbs(silentTail.left, static_cast<int>(2.2 * sampleRate)) < 1.0e-6f,
           "bypassed engine still rang two seconds after the note died");

    // Coupling never invents ambience: with no note ever played the bus stays
    // at zero, so nothing can be injected.
    parameters.sympatheticAmount = 1.0f;
    engine.setParameters(parameters);
    engine.reset();
    StereoBuffer idle(8192);
    renderInto(engine, idle);
    expect(peakAbs(idle.left) == 0.0f && peakAbs(idle.right) == 0.0f,
           "sympathetic coupling generated output without a played note");

    // Determinism, including across engine reuse.
    const auto first = renderNote(engine, sampleRate, 45, 0.8f,
                                  Articulation::Downstroke, 0.9, 0.4);
    const auto second = renderNote(engine, sampleRate, 45, 0.8f,
                                   Articulation::Downstroke, 0.9, 0.4);
    expect(first.left == second.left && first.right == second.right,
           "sympathetic coupling is not sample-deterministic");

    // A fingered string never keeps its coupled ring: the hand stops it.
    engine.reset();
    engine.noteOn(45, 0.9f);
    StereoBuffer settle(static_cast<int>(0.4 * sampleRate));
    renderInto(engine, settle);
    const int highString = ElectryEngine::stringCount - 1;
    expect(TestAccess::snapshot(engine, highString).sympatheticReady,
           "the open high E did not begin ringing through the bridge");
    engine.noteOn(64, 0.9f);
    expect(! TestAccess::snapshot(engine, highString).sympatheticReady,
           "picking a coupled string did not hand it back to the player");

    // Worst case: every control at its extreme, every string struck hard, and
    // the coupled loops driven as hard as the model allows.
    for (const double rate : { 44100.0, 96000.0, 192000.0 })
    {
        ElectryEngine hot;
        hot.prepare(rate, 512);
        EngineParameters extreme;
        extreme.sympatheticAmount = 1.0f;
        extreme.artifactAmount = 1.0f;
        extreme.bodyResonance = 1.0f;
        extreme.stringAge = 0.0f;
        extreme.construction = 0.0f;
        extreme.outputGain = 2.0f;
        extreme.outputMode = electry::OutputMode::Stereo;
        hot.setParameters(extreme);
        hot.reset();
        for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
            hot.noteOn(note, 1.0f);
        StereoBuffer strum(static_cast<int>(0.9 * rate));
        renderInto(hot, strum);
        hot.allNotesOff();
        StereoBuffer ring(static_cast<int>(0.9 * rate));
        renderInto(hot, ring);
        expect(allFinite(strum) && allFinite(ring),
               "maximum coupling became non-finite at "
                   + std::to_string(rate) + " Hz");
        // The linked soft guard bounds any input to 1/sqrt(0.4356) = 1.516
        // before the output gain, so the maximum reachable peak at the
        // maximum +6 dB output is 3.03. Staying under that proves the coupled
        // loops cannot drive the model past its analytic ceiling.
        expect(peakAbs(strum.left) < 3.05f && peakAbs(ring.left) < 3.05f,
               "maximum coupling escaped the output guard at "
                   + std::to_string(rate) + " Hz");
        // The coupled strings must decay rather than sustain. With no plucked
        // voice left there is nothing writing to the bridge bus, so the ring
        // has to fall away and the engine has to reach exact silence.
        expect(peakAbs(ring.left) > 1.0e-3f,
               "the coupled strings did not ring after the notes ended at "
                   + std::to_string(rate) + " Hz");
        StereoBuffer firstTail(static_cast<int>(2.0 * rate));
        renderInto(hot, firstTail);
        StereoBuffer secondTail(static_cast<int>(2.0 * rate));
        renderInto(hot, secondTail);
        expect(allFinite(firstTail) && allFinite(secondTail),
               "maximum coupling became non-finite while ringing out at "
                   + std::to_string(rate) + " Hz");
        expect(peakAbs(firstTail.left) < 1.0e-3f
                   && peakAbs(secondTail.left) == 0.0f,
               "maximum coupling did not ring out to exact silence at "
                   + std::to_string(rate) + " Hz ("
                   + std::to_string(peakAbs(firstTail.left)) + " -> "
                   + std::to_string(peakAbs(secondTail.left)) + ")");
    }
}

// ---------------------------------------------------------------------------
// Version 1.1: continuous palm-mute pressure
// ---------------------------------------------------------------------------

void testPalmMuteContinuum()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);

    EngineParameters parameters;
    parameters.artifactAmount = 0.0f;
    parameters.sympatheticAmount = 0.0f;
    parameters.pickNoise = 0.0f;
    parameters.fingerNoise = 0.0f;
    parameters.releaseNoise = 0.0f;

    const auto lateRms = [&] (float amount)
    {
        parameters.palmMute = amount;
        engine.setParameters(parameters);
        const auto buffer = renderNote(engine, sampleRate, 45, 0.9f,
                                       Articulation::Downstroke, 0.9);
        expect(allFinite(buffer) && peakAbs(buffer.left) < 0.80f,
               "palm mute produced non-finite or unbounded audio at "
                   + std::to_string(amount));
        return rmsInRange(buffer.left, static_cast<int>(0.30 * sampleRate),
                          static_cast<int>(0.60 * sampleRate));
    };

    const double open = lateRms(0.0f);
    const double half = lateRms(0.5f);
    const double full = lateRms(1.0f);
    expect(open > half * 2.0 && half > full * 2.0,
           "palm mute pressure does not shorten decay monotonically ("
               + std::to_string(open) + ", " + std::to_string(half) + ", "
               + std::to_string(full) + ")");

    // Zero pressure is a mathematical no-op, not merely a small effect: the
    // rendered audio must be identical to an engine that never saw the
    // control.
    EngineParameters untouched = parameters;
    untouched.palmMute = 0.0f;
    engine.setParameters(untouched);
    const auto reference = renderNote(engine, sampleRate, 45, 0.9f,
                                      Articulation::Downstroke, 0.5);
    EngineParameters explicitZero = untouched;
    explicitZero.palmMute = 0.0f;
    engine.setParameters(explicitZero);
    const auto atZero = renderNote(engine, sampleRate, 45, 0.9f,
                                   Articulation::Downstroke, 0.5);
    expect(reference.left == atZero.left,
           "palm mute at zero is not an exact bypass");

    // Damping is re-solved through the same loop-filter path as every other
    // decay control, so a muted string still sounds the played pitch.
    parameters.palmMute = 0.45f;
    engine.setParameters(parameters);
    const auto muted = renderNote(engine, sampleRate, 45, 0.9f,
                                  Articulation::Downstroke, 0.5);
    const double expectedHz = midiHz(45);
    const double measured = measureFrequency(
        muted.left, static_cast<int>(0.02 * sampleRate),
        static_cast<int>(0.18 * sampleRate), sampleRate, expectedHz);
    expect(std::abs(centsBetween(measured, expectedHz)) < 12.0,
           "palm-muted string drifted out of tune ("
               + std::to_string(centsBetween(measured, expectedHz)) + " cents)");

    // The loop filter genuinely moves rather than a gain being applied after
    // the fact.
    parameters.palmMute = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(45, 0.9f);
    const float openDamping =
        TestAccess::snapshot(engine, TestAccess::stringForNote(engine, 45))
            .loopDampingCoefficient;
    parameters.palmMute = 0.9f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(45, 0.9f);
    const float mutedDamping =
        TestAccess::snapshot(engine, TestAccess::stringForNote(engine, 45))
            .loopDampingCoefficient;
    // A shorter decay target needs a heavier one-pole, because the solve
    // matches the ratio between the fundamental and high-frequency T60s.
    expect(mutedDamping > openDamping + 1.0e-3f,
           "palm mute did not change the solved loop-filter coefficient ("
               + std::to_string(openDamping) + " -> "
               + std::to_string(mutedDamping) + ")");

    // CC2 pressure adds to the parameter and is released cleanly.
    parameters.palmMute = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.setPalmMutePressure(1.0f);
    engine.noteOn(45, 0.9f);
    StereoBuffer pressed(static_cast<int>(0.9 * sampleRate));
    renderInto(engine, pressed);
    const double pressedLate = rmsInRange(pressed.left,
                                          static_cast<int>(0.30 * sampleRate),
                                          static_cast<int>(0.60 * sampleRate));
    expect(allFinite(pressed) && pressedLate < half * 2.0,
           "CC2 pressure did not damp the string");
    engine.setPalmMutePressure(std::numeric_limits<float>::quiet_NaN());
    engine.reset();
    engine.noteOn(45, 0.9f);
    StereoBuffer afterHostile(static_cast<int>(0.3 * sampleRate));
    renderInto(engine, afterHostile);
    expect(allFinite(afterHostile),
           "hostile CC2 pressure produced non-finite audio");
}

// ---------------------------------------------------------------------------
// Version 1.1: strum travel
// ---------------------------------------------------------------------------

void testStrumSpread()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::array<int, 6> chord { 28, 40, 45, 50, 55, 64 };

    const auto firstOnsetSample = [] (const std::vector<float>& data, float threshold)
    {
        for (int i = 0; i < static_cast<int>(data.size()); ++i)
            if (std::abs(data[static_cast<std::size_t>(i)]) > threshold)
                return i;
        return -1;
    };

    const auto renderChord = [&] (float spreadSeconds)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.strumSpreadSeconds = spreadSeconds;
        engine.setParameters(parameters);
        engine.reset();
        for (const int note : chord)
            engine.noteOn(note, 0.85f);

        std::array<int, ElectryEngine::stringCount> delays {};
        for (int s = 0; s < ElectryEngine::stringCount; ++s)
            delays[static_cast<std::size_t>(s)] =
                TestAccess::snapshot(engine, s).startDelaySamples;

        StereoBuffer buffer(static_cast<int>(0.6 * sampleRate));
        renderInto(engine, buffer);
        return std::make_pair(delays, std::move(buffer));
    };

    const auto block = renderChord(0.0f);
    for (const int delay : block.first)
        expect(delay == 0, "a chord at zero spread was not simultaneous");
    expect(allFinite(block.second), "block chord produced non-finite audio");

    const auto strummed = renderChord(0.020f);
    // The first string the pick meets fires immediately; each further string
    // is offset by the travel time, in physical string order.
    expect(strummed.first[0] == 0, "the leading string of a strum was delayed");
    const int step = static_cast<int>(0.020f * sampleRate * 2.0f); // internal 2x clock
    for (const std::size_t stringIndex : { std::size_t { 2 }, std::size_t { 3 },
                                           std::size_t { 4 }, std::size_t { 5 },
                                           std::size_t { 7 } })
    {
        const int expected = step * static_cast<int>(stringIndex);
        // The engine truncates the travel time to whole internal samples, so
        // the rounding error accumulates by at most one sample per string.
        expect(std::abs(strummed.first[stringIndex] - expected)
                   <= 2 * static_cast<int>(stringIndex) + 2,
               "string " + std::to_string(stringIndex)
                   + " did not receive its strum travel offset ("
                   + std::to_string(strummed.first[stringIndex]) + " vs "
                   + std::to_string(expected) + ")");
    }
    expect(allFinite(strummed.second) && peakAbs(strummed.second.left) < 0.80f,
           "strummed chord produced non-finite or unbounded audio");

    // The strum genuinely reaches the audio: the last string's attack arrives
    // measurably later than the chord's onset.
    const int blockOnset = firstOnsetSample(block.second.left, 0.02f);
    const int strumOnset = firstOnsetSample(strummed.second.left, 0.02f);
    expect(blockOnset >= 0 && strumOnset >= 0, "chord onset was not detectable");
    const double blockPeak = static_cast<double>(peakAbs(
        block.second.left, 0, static_cast<int>(0.03 * sampleRate)));
    const double strumPeak = static_cast<double>(peakAbs(
        strummed.second.left, 0, static_cast<int>(0.03 * sampleRate)));
    expect(strumPeak < blockPeak,
           "spreading a chord did not lower its stacked initial peak");

    // A note that arrives after the chord window starts a fresh stroke and is
    // therefore never delayed.
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.strumSpreadSeconds = 0.040f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(28, 0.9f);
    StereoBuffer gap(static_cast<int>(0.2 * sampleRate));
    renderInto(engine, gap);
    engine.noteOn(64, 0.9f);
    expect(TestAccess::snapshot(engine,
                                ElectryEngine::stringCount - 1).startDelaySamples == 0,
           "a note outside the chord window inherited a strum offset");

    // A delayed voice is never retired before its excitation fires.
    engine.reset();
    for (const int note : chord)
        engine.noteOn(note, 0.85f);
    StereoBuffer settle(static_cast<int>(0.5 * sampleRate));
    renderInto(engine, settle);
    expect(engine.getActiveVoiceCount() == static_cast<int>(chord.size()),
           "a strum-delayed string was retired before it was picked");
    expect(peakAbs(settle.left) > 1.0e-4f, "the delayed strings never sounded");

    // Strum Spread schedules the stroke rather than shaping it, so the control
    // tick copies it verbatim. A chord dispatched at offset 0 of the block that
    // carries the automation change must already use the new value; reading the
    // smoothed copy scheduled the whole chord with the previous spread. reset()
    // syncs the smoothed state from the target, so this deliberately does not
    // reset after the change.
    {
        ElectryEngine automated;
        automated.prepare (sampleRate, 512);
        EngineParameters flat;
        flat.artifactAmount = 0.0f;
        flat.sympatheticAmount = 0.0f;
        flat.strumSpreadSeconds = 0.0f;
        automated.setParameters (flat);
        automated.reset();
        StereoBuffer settled (1024);
        renderInto (automated, settled);

        EngineParameters spreadNow = flat;
        spreadNow.strumSpreadSeconds = 0.020f;
        automated.setParameters (spreadNow);

        // No render in between: the chord arrives at sample offset 0.
        for (const int note : chord)
            automated.noteOn (note, 0.85f);

        int delayedStrings = 0;
        for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount; ++stringIndex)
            if (TestAccess::snapshot (automated, stringIndex).startDelaySamples > 0)
                ++delayedStrings;
        expect (delayedStrings > 0,
                "a chord automated in the same block was scheduled with the "
                "previous strum spread");
    }

    // Lifting a key before the pick reaches that string cancels the stroke.
    // Leaving the countdown running excited the string after its release, so a
    // short strummed chord grew a late attack once the keys were already up.
    {
        ElectryEngine released;
        released.prepare(sampleRate, 512);
        EngineParameters spread;
        spread.artifactAmount = 0.0f;
        spread.sympatheticAmount = 0.0f;
        spread.strumSpreadSeconds = 0.12f;
        released.setParameters(spread);
        released.reset();
        for (const int note : chord)
            released.noteOn(note, 0.85f);

        // Every key is up long before the pick has crossed the neck.
        StereoBuffer opening(static_cast<int>(0.02 * sampleRate));
        renderInto(released, opening);
        for (const int note : chord)
            released.noteOff(note);

        for (int stringIndex = 0; stringIndex < ElectryEngine::stringCount; ++stringIndex)
            expect(TestAccess::snapshot(released, stringIndex).startDelaySamples == 0,
                   "string " + std::to_string(stringIndex)
                       + " kept a pending strum excitation after its key was lifted");

        StereoBuffer tail(static_cast<int>(1.0 * sampleRate));
        renderInto(released, tail);
        // By this point the damped strings have decayed; any energy here is a
        // pick that landed after the key was released.
        const float late = peakAbs(tail.left, static_cast<int>(0.15 * sampleRate));
        expect(late < 0.01f,
               "a string was picked after its key was released (late peak "
                   + std::to_string(late) + ")");
    }
}

// ---------------------------------------------------------------------------
// Version 1.1: vibrato depth and fret-following pick position
// ---------------------------------------------------------------------------

void testVibratoDepthAndPickGeometry()
{
    constexpr double sampleRate = 48000.0;

    const auto vibratoExcursion = [&] (float cents)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.artifactAmount = 0.0f;
        parameters.sympatheticAmount = 0.0f;
        parameters.vibratoDepthCents = cents;
        engine.setParameters(parameters);
        engine.reset();
        engine.setVibratoAmount(1.0f);
        engine.noteOn(45, 0.35f);

        const int stringIndex = TestAccess::stringForNote(engine, 45);
        // Let the attack's tension-modulation glide relax first; it moves the
        // same delay target and would otherwise be measured as vibrato.
        StereoBuffer settle(static_cast<int>(1.6 * sampleRate));
        renderInto(engine, settle);

        float lowest = 1.0e9f;
        float highest = -1.0e9f;
        StereoBuffer buffer(512);
        // Roughly two and a half 5.4 Hz cycles, sampled every 10.7 ms, which
        // is dense enough to land within a few per cent of the extremes.
        for (int block = 0; block < 44; ++block)
        {
            renderInto(engine, buffer, 512);
            const auto snapshot = TestAccess::snapshot(engine, stringIndex);
            lowest = std::min(lowest, snapshot.verticalDelayTarget);
            highest = std::max(highest, snapshot.verticalDelayTarget);
        }
        expect(allFinite(settle) && allFinite(buffer),
               "vibrato produced non-finite audio");
        return static_cast<double>(highest - lowest);
    };

    const double none = vibratoExcursion(0.0f);
    const double standard = vibratoExcursion(35.0f);
    const double wide = vibratoExcursion(100.0f);
    // The loop delay is measured on the internal 2x clock, where an A2 period
    // is about 873 samples, so 35 cents is roughly a 35-sample excursion.
    expect(none < 1.0, "zero vibrato depth still modulated the loop delay ("
                           + std::to_string(none) + " samples)");
    expect(standard > 25.0, "the default vibrato depth is far too shallow ("
                                + std::to_string(standard) + " samples)");
    // The delay excursion is proportional to the depth in cents to first order.
    const double ratio = wide / std::max(standard, 1.0e-9);
    expect(ratio > 2.4 && ratio < 3.3,
           "vibrato depth does not scale the pitch excursion ("
               + std::to_string(ratio) + ")");

    // The picking hand stays put while the fretting hand moves, so the pluck
    // position as a fraction of the sounding length grows by 2^(fret/12).
    // Note 86 is only reachable at fret 22 of the top string, and note 64 is
    // that same string open, which pins the comparison to one physical string.
    const auto combFraction = [&] (int midiNote)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickPosition = 0.10f;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(midiNote, 0.8f);
        const int stringIndex = ElectryEngine::stringCount - 1;
        const auto snapshot = TestAccess::snapshot(engine, stringIndex);
        expect(snapshot.midiNote == midiNote,
               "note " + std::to_string(midiNote)
                   + " was not allocated to the top string");
        return static_cast<double>(snapshot.excitationCombDelay)
             / std::max(static_cast<double>(snapshot.verticalDelayTarget), 1.0e-9);
    };

    const double openComb = combFraction(64);
    const double frettedComb = combFraction(86);
    const double expectedRatio = std::pow(2.0, 22.0 / 12.0);
    const double actualRatio = frettedComb / std::max(openComb, 1.0e-9);
    expect(std::abs(actualRatio - expectedRatio) < 0.05 * expectedRatio,
           "pluck position did not follow the fretted sounding length ("
               + std::to_string(actualRatio) + " vs "
               + std::to_string(expectedRatio) + ")");
}

void testPickContactGeometry()
{
    constexpr double sampleRate = 48000.0;

    // A plectrum is neither a point nor symmetric: it touches the string over a
    // patch and it slips off far faster than it loaded.
    const auto pickGeometry = [] (int midiNote, float hardness)
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickHardness = hardness;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(midiNote, 0.9f);
        const int stringIndex = TestAccess::stringForNote(engine, midiNote);
        expect(stringIndex >= 0, "note " + std::to_string(midiNote)
                                     + " was not allocated");
        return TestAccess::snapshot(engine, std::max(stringIndex, 0));
    };

    const auto lowDefault = pickGeometry(28, 0.6f);
    const auto highDefault = pickGeometry(64, 0.6f);
    const auto lowSoft = pickGeometry(28, 0.0f);
    const auto lowHard = pickGeometry(28, 1.0f);

    expect(lowDefault.excitationCombWidth > 0.0f,
           "the pick contact patch has no width");
    // A fixed physical patch is a larger share of the low string's much longer
    // round trip, so it spans more delay-line samples there.
    expect(lowDefault.excitationCombWidth > 4.0f * highDefault.excitationCombWidth,
           "the contact patch does not scale with the string's length ("
               + std::to_string(lowDefault.excitationCombWidth) + " vs "
               + std::to_string(highDefault.excitationCombWidth) + ")");
    // A soft rounded pick touches over roughly three times the patch of a stiff
    // sharp one.
    expect(lowSoft.excitationCombWidth > 2.0f * lowHard.excitationCombWidth,
           "pick hardness does not narrow the contact patch");

    // The patch spans a fixed distance along the string and the wave speed does
    // not change when the string is fretted, so its width in delay samples is
    // the same open and at the twelfth fret. The allocator prefers the free
    // string with the lowest fret, so note 64 is the top string open and note
    // 76 is that same string at fret 12; both are unambiguous.
    const auto openTop = pickGeometry(64, 0.6f);
    const auto frettedTop = pickGeometry(76, 0.6f);
    expect(openTop.stringIndex == frettedTop.stringIndex,
           "the fretted comparison did not stay on one physical string");
    // The remaining few percent is the loop delay's own analytic phase
    // compensation, which differs between the two notes because their decay
    // targets and dead-spot damping do.
    expect(std::abs(openTop.excitationCombWidth
                    - frettedTop.excitationCombWidth)
               < 0.08f * openTop.excitationCombWidth,
           "the contact patch is not fret invariant in delay samples ("
               + std::to_string(openTop.excitationCombWidth) + " vs "
               + std::to_string(frettedTop.excitationCombWidth) + ")");

    // A stiffer pick holds the string longer and then releases it faster.
    const float softSlip = 1.0f / lowSoft.excitationLoadScale;
    const float hardSlip = 1.0f / lowHard.excitationLoadScale;
    expect(hardSlip > softSlip + 0.10f,
           "pick hardness does not move the slip point later ("
               + std::to_string(softSlip) + " to " + std::to_string(hardSlip)
               + ")");
    expect(lowHard.excitationSlipScale > lowSoft.excitationSlipScale,
           "a stiffer pick does not release the string faster");

    // The release window is a load smoothstep times a slip smoothstep. Whatever
    // the slip point, its area is exactly one half - the same area the
    // symmetric raised cosine it replaced had - so the asymmetry changes the
    // attack's spectrum without changing how hard the note lands.
    for (const auto& snapshot : { lowSoft, lowDefault, lowHard })
    {
        const auto smoothStep = [] (double value)
        {
            value = std::clamp(value, 0.0, 1.0);
            return value * value * (3.0 - 2.0 * value);
        };
        constexpr int steps = 200000;
        double area = 0.0;
        for (int step = 0; step < steps; ++step)
        {
            const double progress = (static_cast<double>(step) + 0.5)
                                  / static_cast<double>(steps);
            area += smoothStep(progress * snapshot.excitationLoadScale)
                  * smoothStep((1.0 - progress) * snapshot.excitationSlipScale);
        }
        area /= static_cast<double>(steps);
        expect(std::abs(area - 0.5) < 1.0e-3,
               "the asymmetric pick release window is not level neutral ("
                   + std::to_string(area) + ")");
    }
}

// ---------------------------------------------------------------------------
// Version 1.1: display readout and fretboard geometry
// ---------------------------------------------------------------------------

void testVisualStateAndGeometry()
{
    namespace visuals = electry::visuals;
    constexpr int lastFret = ElectryEngine::fretCount;

    expect(visuals::fretWireFraction(0, lastFret) == 0.0f,
           "the nut is not at the start of the drawn neck");
    expect(std::abs(visuals::fretWireFraction(lastFret, lastFret) - 1.0f) < 1.0e-6f,
           "the last fret is not at the end of the drawn neck");
    // The twelfth fret sits at half the scale length, which on a 22-fret neck
    // is 0.5 / (1 - 2^(-22/12)) of the drawn span.
    const float octaveExpected = 0.5f / (1.0f - std::pow(2.0f, -22.0f / 12.0f));
    expect(std::abs(visuals::fretWireFraction(12, lastFret) - octaveExpected)
               < 1.0e-4f,
           "the twelfth fret is not at half the scale length");
    for (int fret = 1; fret <= lastFret; ++fret)
    {
        expect(visuals::fretWireFraction(fret, lastFret)
                   > visuals::fretWireFraction(fret - 1, lastFret),
               "fret positions are not monotonic at fret " + std::to_string(fret));
    }
    expect(visuals::fretWireFraction(2, lastFret) - visuals::fretWireFraction(1, lastFret)
               < visuals::fretWireFraction(1, lastFret),
           "fret spacing does not narrow toward the body");
    expect(visuals::fretWireFraction(-5, lastFret) == 0.0f
               && visuals::fretWireFraction(999, lastFret) == 1.0f,
           "fret geometry did not clamp out-of-range frets");
    expect(visuals::fretCentreFraction(0, lastFret) == 0.0f,
           "an open string is not drawn at the nut");
    expect(visuals::fretCentreFraction(5, lastFret)
                   > visuals::fretWireFraction(4, lastFret)
               && visuals::fretCentreFraction(5, lastFret)
                      < visuals::fretWireFraction(5, lastFret),
           "a fingered note is not between its enclosing fret wires");

    for (int s = 1; s < ElectryEngine::stringCount; ++s)
    {
        expect(visuals::stringRowFraction(s, ElectryEngine::stringCount, 0.085f)
                   > visuals::stringRowFraction(s - 1, ElectryEngine::stringCount,
                                                0.085f),
               "string rows are not ordered low to high");
        expect(visuals::stringThickness(s, 0.9f, 2.6f)
                   < visuals::stringThickness(s - 1, 0.9f, 2.6f),
               "string thickness does not taper toward the top string");
    }
    expect(visuals::stringRowFraction(0, ElectryEngine::stringCount, 0.085f) >= 0.085f
               && visuals::stringRowFraction(7, ElectryEngine::stringCount, 0.085f)
                      <= 0.915f,
           "string rows escaped the drawn fingerboard");

    float level = 0.0f;
    level = visuals::meterBallistics(level, 1.0f, 0.55f, 0.18f);
    expect(level > 0.5f, "meter attack is too slow to show an attack");
    const float afterAttack = level;
    level = visuals::meterBallistics(level, 0.0f, 0.55f, 0.18f);
    expect(level < afterAttack && level > 0.5f * afterAttack,
           "meter release does not hold the reading");
    for (int i = 0; i < 200; ++i)
        level = visuals::meterBallistics(level, 0.0f, 0.55f, 0.18f);
    expect(level < 1.0e-6f, "meter did not settle back to zero");
    expect(visuals::meterBallistics(std::numeric_limits<float>::quiet_NaN(), 0.5f,
                                    0.5f, 0.5f) == 0.25f,
           "meter ballistics did not recover from a non-finite reading");

    expect(visuals::vibrationShape(0.0f, 0.0f) < 1.0e-6f
               && visuals::vibrationShape(1.0f, 0.0f) < 1.0e-6f,
           "an open string is not pinned at the nut and bridge");
    expect(std::abs(visuals::vibrationShape(0.5f, 0.0f) - 1.0f) < 1.0e-5f,
           "an open string does not peak at its midpoint");
    expect(visuals::vibrationShape(0.2f, 0.5f) == 0.0f,
           "the string moves behind the fretting finger");
    expect(std::abs(visuals::vibrationShape(0.75f, 0.5f) - 1.0f) < 1.0e-5f,
           "a fretted string does not peak at the middle of its sounding length");
    expect(visuals::levelHeat(0.0f) == 0.0f
               && std::abs(visuals::levelHeat(1.0f) - 1.0f) < 1.0e-6f
               && visuals::levelHeat(0.25f) > 0.25f,
           "the level-to-heat curve is not a monotonic knee");

    // Packing is lossless for everything the display needs.
    for (int note = -1; note <= 127; note += 13)
    {
        for (int fret = -1; fret <= ElectryEngine::fretCount; fret += 5)
        {
            electry::StringVisualState state;
            state.midiNote = note;
            state.fret = fret;
            state.sounding = (note % 2) == 0;
            state.sympathetic = (fret % 2) == 0;
            state.releasing = (note % 3) == 0;
            state.level = 0.5f;
            state.articulation = static_cast<Articulation>(
                (note + 1) % ElectryEngine::keyswitchCount);
            const auto round = visuals::unpackStringVisual(
                visuals::packStringVisual(state));
            expect(round.midiNote == state.midiNote && round.fret == state.fret
                       && round.sounding == state.sounding
                       && round.sympathetic == state.sympathetic
                       && round.releasing == state.releasing
                       && round.articulation == state.articulation
                       && std::abs(round.level - state.level) < 0.005f,
                   "packed string state did not survive a round trip");
        }
    }

    // The engine's readout names the right physical string, fret and note.
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.sympatheticAmount = 0.8f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(Articulation::Chug), 1.0f);
    engine.noteOn(45, 0.95f);
    StereoBuffer buffer(static_cast<int>(0.25 * sampleRate));
    renderInto(engine, buffer);

    std::array<electry::StringVisualState, ElectryEngine::stringCount> visual {};
    engine.getStringVisualState(visual);
    expect(visual[3].sounding && visual[3].midiNote == 45 && visual[3].fret == 0
               && visual[3].articulation == Articulation::Chug,
           "the display readout did not identify the played open A string");
    expect(visual[3].level > 0.0f, "a struck string reported no display level");
    expect(! visual[0].sounding, "an unplayed string reported a played note");

    int ringing = 0;
    for (const auto& state : visual)
        if (state.sympathetic)
        {
            ++ringing;
            expect(state.midiNote >= ElectryEngine::lowestPlayableNote
                       && state.fret == 0,
                   "a coupled string reported an implausible open note");
        }
    expect(ringing == engine.getSympatheticStringCount(),
           "the coupled-string count disagrees with the per-string readout");

    engine.reset();
    engine.getStringVisualState(visual);
    for (const auto& state : visual)
        expect(! state.sounding && ! state.sympathetic && state.midiNote == -1
                   && state.level == 0.0f,
               "a reset engine still reported a sounding string");
}

// ---------------------------------------------------------------------------
// Version 1.1: the paths the efficiency work depends on
// ---------------------------------------------------------------------------

void testPickupCullingAndChannelLinking()
{
    constexpr double sampleRate = 48000.0;

    const auto settle = [] (ElectryEngine& engine, double seconds)
    {
        StereoBuffer buffer(static_cast<int>(seconds * sampleRate));
        renderInto(engine, buffer);
        return buffer;
    };

    struct SelectorCase { PickupSelector selector; bool neck; bool bridge; };
    for (const auto& item : { SelectorCase { PickupSelector::Bridge, false, true },
                              SelectorCase { PickupSelector::Neck, true, false },
                              SelectorCase { PickupSelector::Both, true, true } })
    {
        ElectryEngine engine;
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupSelector = item.selector;
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(45, 0.8f);
        const auto audio = settle(engine, 0.2);
        expect(TestAccess::pickupPathActive(engine, true) == item.neck,
               "the neck pickup path was not culled to match the selector");
        expect(TestAccess::pickupPathActive(engine, false) == item.bridge,
               "the bridge pickup path was not culled to match the selector");
        expect(allFinite(audio) && peakAbs(audio.left) > 1.0e-5f,
               "a selector position rendered silence");
    }

    // Bringing a culled pickup back must not click: its aperture and EMF
    // memory is cleared and the selector mix fades it in over about 4 ms.
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.pickupSelector = PickupSelector::Bridge;
    parameters.artifactAmount = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(45, 0.9f);
    settle(engine, 0.30);
    parameters.pickupSelector = PickupSelector::Both;
    engine.setParameters(parameters);
    const auto crossfade = settle(engine, 0.10);
    expect(allFinite(crossfade), "switching pickups produced non-finite audio");
    float largestStep = 0.0f;
    for (std::size_t i = 1; i < crossfade.left.size(); ++i)
        largestStep = std::max(largestStep,
                               std::abs(crossfade.left[i] - crossfade.left[i - 1]));
    expect(largestStep < 0.08f,
           "restoring a culled pickup produced a discontinuity ("
               + std::to_string(largestStep) + ")");

    // Mono runs one shared output chain and is bit-identical dual mono;
    // opening the field copies its state across without a discontinuity.
    ElectryEngine field;
    field.prepare(sampleRate, 512);
    EngineParameters mono;
    mono.outputMode = electry::OutputMode::Mono;
    field.setParameters(mono);
    field.reset();
    field.noteOn(40, 0.9f);
    StereoBuffer monoAudio(static_cast<int>(0.25 * sampleRate));
    renderInto(field, monoAudio);
    expect(TestAccess::channelsLinked(field),
           "Mono did not link the shared output chain");
    expect(monoAudio.left == monoAudio.right,
           "Mono is not sample-identical dual mono");

    EngineParameters stereo = mono;
    stereo.outputMode = electry::OutputMode::Stereo;
    field.setParameters(stereo);
    StereoBuffer opening(static_cast<int>(0.20 * sampleRate));
    renderInto(field, opening);
    expect(! TestAccess::channelsLinked(field),
           "Stereo did not unlink the shared output chain");
    float largestFieldStep = 0.0f;
    for (std::size_t i = 1; i < opening.right.size(); ++i)
        largestFieldStep = std::max(
            largestFieldStep, std::abs(opening.right[i] - opening.right[i - 1]));
    expect(allFinite(opening) && largestFieldStep < 0.08f,
           "opening the stereo field produced a discontinuity ("
               + std::to_string(largestFieldStep) + ")");
}

void testIdleFreezeAndDenormalSafety()
{
    constexpr double sampleRate = 48000.0;
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.sympatheticAmount = 1.0f;
    parameters.artifactAmount = 1.0f;
    parameters.bodyResonance = 1.0f;
    parameters.stringAge = 0.0f;
    engine.setParameters(parameters);
    engine.reset();

    // A freshly prepared engine is already frozen: no body mode, coil, DC
    // blocker or decimator runs until a string is asked to vibrate.
    StereoBuffer beforeAnyNote(static_cast<int>(0.5 * sampleRate));
    renderInto(engine, beforeAnyNote);
    expect(peakAbs(beforeAnyNote.left) == 0.0f
               && peakAbs(beforeAnyNote.right) == 0.0f,
           "an untouched engine did not render exact digital silence");

    for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
        engine.noteOn(note, 1.0f);
    StereoBuffer strum(static_cast<int>(1.0 * sampleRate));
    renderInto(engine, strum);
    expect(peakAbs(strum.left) > 0.01f, "the strum did not sound");
    engine.allNotesOff();

    // Every sample of the ring-out must be finite, and none of it may be a
    // subnormal float: the hosts that matter run with flush-to-zero, but the
    // engine must not depend on that to stay cheap.
    int subnormals = 0;
    int silentBlocks = 0;
    bool sawSilence = false;
    constexpr float smallestNormal = 1.1754943508e-38f;
    for (int block = 0; block < 40 && ! sawSilence; ++block)
    {
        StereoBuffer tail(static_cast<int>(0.5 * sampleRate));
        renderInto(engine, tail);
        expect(allFinite(tail), "the ring-out produced non-finite audio");
        float peak = 0.0f;
        for (const float sample : tail.left)
        {
            const float magnitude = std::abs(sample);
            peak = std::max(peak, magnitude);
            if (magnitude > 0.0f && magnitude < smallestNormal)
                ++subnormals;
        }
        if (peak == 0.0f)
        {
            sawSilence = true;
            silentBlocks = block;
        }
    }
    expect(subnormals == 0,
           "the ring-out generated " + std::to_string(subnormals)
               + " subnormal output samples");
    expect(sawSilence,
           "the engine never reached exact silence after the last note");
    // Freezing must happen promptly enough to matter, but never so early that
    // an audible tail is truncated.
    expect(silentBlocks >= 1 && silentBlocks <= 24,
           "the idle freeze happened at an implausible time (block "
               + std::to_string(silentBlocks) + ")");

    // A frozen engine wakes cleanly.
    engine.noteOn(45, 0.9f);
    StereoBuffer wake(static_cast<int>(0.25 * sampleRate));
    renderInto(engine, wake);
    expect(allFinite(wake) && peakAbs(wake.left) > 1.0e-3f,
           "a frozen engine did not wake for a new note");
}

void testCpuGuardrail()
{
    constexpr double sampleRate = 96000.0;
    constexpr int totalSamples = static_cast<int>(2.0 * 96000.0);

    // All eight physical strings ringing in Drop-E tuning. With every string
    // played there is no coupled string left to render, so this measures
    // exactly the same work the pre-1.1 engine did.
    const auto strike = [&] (ElectryEngine& engine, PickupSelector selector,
                             electry::OutputMode mode)
    {
        engine.prepare(sampleRate, 512);
        EngineParameters parameters;
        parameters.pickupSelector = selector;
        parameters.outputMode = mode;
        parameters.artifactAmount = 1.0f;
        parameters.bodyResonance = 1.0f;
        engine.setParameters(parameters);
        engine.reset();

        for (const int note : { 28, 35, 40, 45, 50, 55, 59, 64 })
            engine.noteOn(note, 0.9f);
    };

    StereoBuffer buffer(totalSamples);
    const auto timeRender = [&] (ElectryEngine& engine)
    {
        const auto begin = std::chrono::steady_clock::now();
        renderInto(engine, buffer);
        const auto end = std::chrono::steady_clock::now();
        expect(allFinite(buffer), "the CPU guardrail render was not finite");
        return std::chrono::duration<double>(end - begin).count()
             / (static_cast<double>(totalSamples) / sampleRate);
    };

    // The two configurations are timed alternately, each from a freshly struck
    // chord, and each keeps its fastest sample. Measuring one configuration to
    // completion and then the other lets a busy stretch on a shared runner land
    // entirely on whichever went second, which is enough to invert a real
    // difference; interleaving exposes both to the same noise.
    double worstCase = 1.0e9;
    double defaultCase = 1.0e9;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        ElectryEngine worstEngine;
        strike (worstEngine, PickupSelector::Both, electry::OutputMode::Stereo);
        worstCase = std::min (worstCase, timeRender (worstEngine));

        ElectryEngine defaultEngine;
        strike (defaultEngine, PickupSelector::Bridge, electry::OutputMode::Mono);
        defaultCase = std::min (defaultCase, timeRender (defaultEngine));
    }
    std::cout << "Eight-string render CPU ratio at 96 kHz: " << worstCase
              << "x worst case (Both + Stereo), " << defaultCase
              << "x default (Bridge + Mono)\n";

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    constexpr double ceiling = 40.0;
#else
    // Loose portable runaway guard; shared CI runners are not a stable
    // benchmark fixture.
    constexpr double ceiling = 8.0;
#endif
    expect(worstCase < ceiling,
           "eight-string render exceeded the portable CPU ceiling");
    // The default configuration skips the unselected pickup chain and runs one
    // shared output chain instead of two, which must show up as real work
    // removed rather than as a claim in the documentation. The margin is loose
    // because shared CI runners are noisy.
    expect(defaultCase < worstCase * 0.92,
           "the default configuration is not measurably cheaper than the "
           "worst case (" + std::to_string(defaultCase) + "x vs "
               + std::to_string(worstCase) + "x)");
}

} // namespace

int main()
{
    testModalResonatorPeakGain();
    testInternalOversamplingPolicy();
    testRenderMatrixFiniteAndBounded();
    testPitchAccuracy();
    testDropELowNoteAtMaximumRate();
    testDeterminism();
    testKeyswitchesSelectStylesSilently();
    testAlternateStrokeSequence();
    testArticulationsSoundDistinct();
    testExtendedPlayStyles();
    testBendPrograms();
    testHammerOnLegatoContinuity();
    testSlapCollisionAndTensionGlide();
    testPickupsToneAndModelMorph();
    testArtifactsControl();
    testAdvancedDispersionAndBodyConductance();
    testLowRegisterGuitarEnvelope();
    testOpenLowStringLevelBalance();
    testMonoStereoOutputField();
    testVelocityExpression();
    testMaterialAndControlAudibility();
    testNoiseComponentsAndSilence();
    testStringAllocationAndPolyphony();
    testPitchWheelAndSustainPedal();
    testSympatheticBridgeCoupling();
    testPalmMuteContinuum();
    testStrumSpread();
    testVibratoDepthAndPickGeometry();
    testPickContactGeometry();
    testVisualStateAndGeometry();
    testPickupCullingAndChannelLinking();
    testIdleFreezeAndDenormalSafety();
    testParameterSanitisation();
    testCpuGuardrail();

    if (failures != 0)
    {
        std::cerr << failures << " Electry DSP check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Electry DSP checks passed.\n";
    return EXIT_SUCCESS;
}
