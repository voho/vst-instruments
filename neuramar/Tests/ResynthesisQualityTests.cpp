#include "DSP/NeuramarEngine.h"
#include "DSP/SampleLearner.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double sampleRate = 48000.0;
constexpr double rootFrequencyHz = 220.0;
constexpr int rootMidiNote = 57;
int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] double gaussianLogFrequency(double frequencyHz,
                                          double centreHz,
                                          double widthOctaves) noexcept
{
    const double distance = std::log2(
        std::max(frequencyHz, 1.0) / centreHz) / widthOctaves;
    return std::exp(-0.5 * distance * distance);
}

[[nodiscard]] double physicalPartialAmplitude(int harmonic,
                                              double fundamentalHz) noexcept
{
    const double frequencyHz = fundamentalHz * static_cast<double>(harmonic);
    const double sourceEnvelope = 0.13
        + 1.55 * gaussianLogFrequency(frequencyHz, 720.0, 0.23)
        + 0.92 * gaussianLogFrequency(frequencyHz, 1810.0, 0.19)
        + 0.58 * gaussianLogFrequency(frequencyHz, 3470.0, 0.22);
    // Keep this renderer benchmark independently pitch-identifiable. A
    // separate hard-root fixture covers formant-dominant/missing-fundamental
    // material without confounding the source/filter reconstruction metric.
    const double excitation = harmonic == 1
        ? 5.0 : ((harmonic & 1) != 0 ? 1.0 : 0.055);
    return excitation * sourceEnvelope
        / std::pow(static_cast<double>(harmonic), 0.72);
}

[[nodiscard]] std::vector<float> makeSourceFilterNote(double fundamentalHz,
                                                       double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(durationSeconds * sampleRate));
    std::vector<float> signal(sampleCount, 0.0f);
    const int harmonicCount = std::min(128, static_cast<int>(std::floor(
        0.44 * sampleRate / fundamentalHz)));
    for (std::size_t index = 0; index < signal.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double envelope = (1.0 - std::exp(-time / 0.006))
            * (0.88 + 0.12 * std::exp(-time / 0.38));
        double value = 0.0;
        for (int harmonic = 1; harmonic <= harmonicCount; ++harmonic)
        {
            const double phase = 0.19 * static_cast<double>(harmonic * harmonic)
                + 0.07 * static_cast<double>(harmonic);
            value += physicalPartialAmplitude(harmonic, fundamentalHz)
                * std::sin(2.0 * pi * fundamentalHz
                           * static_cast<double>(harmonic) * time + phase);
        }
        signal[index] = static_cast<float>(0.16 * envelope * value);
    }
    return signal;
}

[[nodiscard]] std::vector<float> renderNote(const neuramar::NeuralModel& model,
                                            int midiNote, float bodyLock)
{
    neuramar::NeuramarEngine engine;
    engine.prepare(sampleRate, 128);
    engine.setModel(&model);
    neuramar::EngineParameters parameters;
    parameters.imprint = 1.0f;
    parameters.bodyLock = bodyLock;
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    parameters.brightness = 0.5f;
    parameters.evolutionRate = 1.0f;
    parameters.orbit = 0.0f;
    parameters.mutation = 0.0f;
    parameters.attackSeconds = 0.0f;
    parameters.spread = 0.0f;
    parameters.outputGain = 0.06f;
    engine.setParameters(parameters);

    constexpr int sampleCount = 36000;
    std::vector<float> left(sampleCount, 0.0f);
    std::vector<float> right(sampleCount, 0.0f);
    engine.noteOn(midiNote, 0.82f);
    for (int offset = 0; offset < sampleCount; offset += 128)
    {
        const int count = std::min(128, sampleCount - offset);
        engine.process(left.data() + offset, right.data() + offset, count);
    }
    for (std::size_t index = 0; index < left.size(); ++index)
        left[index] = 0.5f * (left[index] + right[index]);
    return left;
}

[[nodiscard]] double sinusoidAmplitude(const std::vector<float>& signal,
                                       double centreSeconds,
                                       double frequencyHz)
{
    const auto centre = static_cast<std::ptrdiff_t>(std::llround(
        centreSeconds * sampleRate));
    const auto halfWindow = static_cast<std::ptrdiff_t>(std::llround(
        0.050 * sampleRate));
    const auto first = std::max<std::ptrdiff_t>(0, centre - halfWindow);
    const auto last = std::min<std::ptrdiff_t>(
        static_cast<std::ptrdiff_t>(signal.size()), centre + halfWindow + 1);
    if (last - first < 32)
        return 0.0;

    double cosineCosine = 0.0;
    double sineSine = 0.0;
    double cosineSine = 0.0;
    double signalCosine = 0.0;
    double signalSine = 0.0;
    const double denominator = static_cast<double>(last - first - 1);
    for (auto index = first; index < last; ++index)
    {
        const double position = static_cast<double>(index - first) / denominator;
        const double window = 0.5 - 0.5 * std::cos(2.0 * pi * position);
        const double angle = 2.0 * pi * frequencyHz
            * static_cast<double>(index) / sampleRate;
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        const double value = signal[static_cast<std::size_t>(index)];
        cosineCosine += window * cosine * cosine;
        sineSine += window * sine * sine;
        cosineSine += window * cosine * sine;
        signalCosine += window * value * cosine;
        signalSine += window * value * sine;
    }
    const double determinant = cosineCosine * sineSine
        - cosineSine * cosineSine;
    if (determinant <= 1.0e-12)
        return 0.0;
    const double cosineCoefficient = (signalCosine * sineSine
        - signalSine * cosineSine) / determinant;
    const double sineCoefficient = (signalSine * cosineCosine
        - signalCosine * cosineSine) / determinant;
    return std::hypot(cosineCoefficient, sineCoefficient);
}

struct SpectralScore
{
    double shapeMaeDb { 0.0 };
    double parityErrorDb { 0.0 };
};

[[nodiscard]] SpectralScore scoreSpectrum(const std::vector<float>& rendered,
                                          double fundamentalHz)
{
    constexpr double analysisTime = 0.46;
    const double registerRatio = fundamentalHz / rootFrequencyHz;
    const int harmonicCount = std::min({
        36,
        static_cast<int>(std::floor(0.40 * sampleRate / fundamentalHz)),
        static_cast<int>(std::floor(64.0 / registerRatio))
    });
    std::vector<double> ratiosDb;
    std::vector<double> measuredOddDb;
    std::vector<double> measuredEvenDb;
    std::vector<double> expectedOddDb;
    std::vector<double> expectedEvenDb;
    for (int harmonic = 1; harmonic <= harmonicCount; ++harmonic)
    {
        const double expected = physicalPartialAmplitude(harmonic, fundamentalHz);
        const double measured = sinusoidAmplitude(
            rendered, analysisTime, fundamentalHz * harmonic);
        const double expectedDb = 20.0 * std::log10(std::max(expected, 1.0e-10));
        const double measuredDb = 20.0 * std::log10(std::max(measured, 1.0e-10));
        ratiosDb.push_back(measuredDb - expectedDb);
        auto& measuredParity = (harmonic & 1) != 0
            ? measuredOddDb : measuredEvenDb;
        auto& expectedParity = (harmonic & 1) != 0
            ? expectedOddDb : expectedEvenDb;
        measuredParity.push_back(measuredDb);
        expectedParity.push_back(expectedDb);
    }

    std::vector<double> sortedRatios = ratiosDb;
    std::sort(sortedRatios.begin(), sortedRatios.end());
    const double levelOffset = sortedRatios[sortedRatios.size() / 2];
    double shapeError = 0.0;
    for (const auto ratio : ratiosDb)
        shapeError += std::abs(ratio - levelOffset);
    shapeError /= static_cast<double>(ratiosDb.size());

    const auto mean = [](const std::vector<double>& values)
    {
        return std::accumulate(values.begin(), values.end(), 0.0)
            / std::max<std::size_t>(values.size(), 1);
    };
    const double measuredParity = mean(measuredOddDb) - mean(measuredEvenDb);
    const double expectedParity = mean(expectedOddDb) - mean(expectedEvenDb);
    return { shapeError, std::abs(measuredParity - expectedParity) };
}

void testHeldOutSourceFilterFamily()
{
    const auto source = makeSourceFilterNote(rootFrequencyHz, 0.82);
    const auto learned = neuramar::SampleLearner::learn(source, sampleRate);
    expect(static_cast<bool>(learned),
           "held-out source/filter fixture failed to learn: " + learned.error);
    if (!learned)
        return;
    std::cout << "Held-out detected root: "
              << learned.model->rootFrequencyHz() << " Hz\n";
    expect(std::abs(learned.model->rootFrequencyHz() / rootFrequencyHz - 1.0)
               < 0.012,
           "held-out fixture root was identified incorrectly");

    constexpr int offsets[] { -24, -19, -12, -7, -3, 3, 7, 12, 19, 24 };
    double lockedShapeSum = 0.0;
    double followingShapeSum = 0.0;
    double lockedParitySum = 0.0;
    for (const int offset : offsets)
    {
        const double targetFrequency = rootFrequencyHz * std::exp2(
            static_cast<double>(offset) / 12.0);
        const auto locked = scoreSpectrum(
            renderNote(*learned.model, rootMidiNote + offset, 1.0f),
            targetFrequency);
        const auto following = scoreSpectrum(
            renderNote(*learned.model, rootMidiNote + offset, 0.0f),
            targetFrequency);
        lockedShapeSum += locked.shapeMaeDb;
        followingShapeSum += following.shapeMaeDb;
        lockedParitySum += locked.parityErrorDb;
        std::cout << "Held-out " << (offset >= 0 ? "+" : "") << offset
                  << " st: locked shape " << locked.shapeMaeDb
                  << " dB, follow shape " << following.shapeMaeDb
                  << " dB, excitation parity error " << locked.parityErrorDb
                  << " dB\n";
    }

    const double count = static_cast<double>(std::size(offsets));
    const double lockedShape = lockedShapeSum / count;
    const double followingShape = followingShapeSum / count;
    const double lockedParity = lockedParitySum / count;
    std::cout << "Held-out aggregate: factorized shape " << lockedShape
              << " dB, pitch-follow shape " << followingShape
              << " dB, parity error " << lockedParity << " dB\n";
    expect(lockedShape < 5.0,
           "factorized Body Lock missed the held-out spectral family (MAE "
               + std::to_string(lockedShape) + " dB)");
    expect(lockedShape < 0.72 * followingShape,
           "source/filter factorization did not materially improve held-out "
           "register shape over pitch-following synthesis");
    expect(lockedParity < 3.5,
           "Body Lock did not preserve harmonic-index excitation character "
           "across held-out notes (parity error "
               + std::to_string(lockedParity) + " dB)");
}
} // namespace

int main()
{
    testHeldOutSourceFilterFamily();
    if (failures != 0)
    {
        std::cerr << failures << " Neuramar quality benchmark(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Neuramar held-out quality benchmarks passed\n";
    return EXIT_SUCCESS;
}
