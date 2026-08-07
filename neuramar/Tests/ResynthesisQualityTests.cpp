#include "DSP/NeuramarEngine.h"
#include "DSP/SampleLearner.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
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
                                            int midiNote, float bodyLock,
                                            float stretch = 1.0f)
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
    parameters.stretch = stretch;
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
    // 1.0 dB is below what a single ordered projection pass can reach on this
    // fixture. Its even partials sit about 25 dB under their neighbours, so
    // they absorb whatever the sequential subtraction of the loud odd partial
    // leaves behind: that pass measured 1.043 dB here, and the joint
    // Gauss-Seidel refinement measures 0.923 dB. The guard exists to keep the
    // solve joint, not as an acceptance gate.
    expect(lockedParity < 1.0,
           "Body Lock did not preserve harmonic-index excitation character "
           "across held-out notes (parity error "
               + std::to_string(lockedParity) + " dB)");
}
// Analytic ground-truth item 7's stiff-string sibling: a known stiff-string
// coefficient, rendered across the same register matrix. The target partial
// positions are generated independently at every requested pitch.
constexpr double stiffInharmonicity = 4.0e-4;

[[nodiscard]] std::vector<float> makeStiffStringNote(double fundamentalHz,
                                                     double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(durationSeconds * sampleRate));
    std::vector<float> signal(sampleCount, 0.0f);
    constexpr int partialCount = 34;
    for (std::size_t index = 0; index < signal.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double attack = 1.0 - std::exp(-time / 0.0035);
        double value = 0.0;
        for (int partial = 1; partial <= partialCount; ++partial)
        {
            const double number = static_cast<double>(partial);
            const double frequency = fundamentalHz * number
                * std::sqrt(1.0 + stiffInharmonicity * number * number);
            if (frequency >= 0.44 * sampleRate)
                break;
            const double decay = std::exp(-time * (0.80 + 0.048 * number));
            value += std::pow(number, -1.02) * decay * std::sin(
                2.0 * pi * frequency * time
                + 0.29 * number + 0.13 * number * number);
        }
        signal[index] = static_cast<float>(0.40 * attack * value);
    }
    return signal;
}

[[nodiscard]] double partialFrequencyHz(const std::vector<float>& rendered,
                                        double expectedHz)
{
    // A narrow analytic sweep around the independently generated target,
    // followed by a parabolic vertex over the three strongest neighbouring
    // probes. Without that refinement the reported error would be quantised
    // by the sweep step rather than by the renderer.
    const auto first = rendered.size() / 6;
    const auto count = std::min<std::size_t>(16384, rendered.size() - first);
    const double step = std::max(0.25, expectedHz / 2000.0);
    const double lowest = 0.9553 * expectedHz;
    std::vector<double> energies;
    std::size_t best = 0;
    for (double frequency = lowest; frequency <= 1.045 * expectedHz;
         frequency += step)
    {
        std::complex<double> sum;
        const std::complex<double> rotation = std::polar(
            1.0, -2.0 * pi * frequency / sampleRate);
        std::complex<double> oscillator(1.0, 0.0);
        for (std::size_t index = 0; index < count; ++index)
        {
            const double window = 0.5 - 0.5 * std::cos(
                2.0 * pi * static_cast<double>(index)
                / static_cast<double>(std::max<std::size_t>(1, count - 1)));
            sum += static_cast<double>(rendered[first + index]) * window
                * oscillator;
            oscillator *= rotation;
        }
        energies.push_back(std::norm(sum));
        if (energies.back() > energies[best])
            best = energies.size() - 1;
    }
    if (energies.empty())
        return expectedHz;

    double offset = 0.0;
    if (best > 0 && best + 1 < energies.size())
    {
        const double left = std::log(std::max(energies[best - 1], 1.0e-300));
        const double centre = std::log(std::max(energies[best], 1.0e-300));
        const double right = std::log(std::max(energies[best + 1], 1.0e-300));
        const double denominator = left - 2.0 * centre + right;
        if (denominator < -1.0e-18)
            offset = std::clamp(0.5 * (left - right) / denominator, -0.5, 0.5);
    }
    return lowest + (static_cast<double>(best) + offset) * step;
}

void testStiffStringPartialPlacement()
{
    const auto source = makeStiffStringNote(rootFrequencyHz, 1.05);
    const auto learned = neuramar::SampleLearner::learn(source, sampleRate);
    expect(static_cast<bool>(learned),
           "stiff-string fixture failed to learn: " + learned.error);
    if (!learned)
        return;

    const double fitted = learned.model->inharmonicity();
    std::cout << "Stiff-string detected root: "
              << learned.model->rootFrequencyHz() << " Hz, fitted B "
              << fitted << " (true " << stiffInharmonicity << ")\n";
    expect(std::abs(learned.model->rootFrequencyHz() / rootFrequencyHz - 1.0)
               < 0.02,
           "stiff-string fixture root was identified incorrectly");
    expect(fitted > 0.0, "stiff-string fixture was fitted as fully harmonic");

    constexpr int offsets[] { -12, 0, 12 };
    double stretchedSum = 0.0;
    double flatSum = 0.0;
    int measured = 0;
    for (const int offset : offsets)
    {
        const double registerRatio = std::exp2(
            static_cast<double>(offset) / 12.0);
        const auto stretched = renderNote(
            *learned.model, rootMidiNote + offset, 0.0f, 1.0f);
        const auto flat = renderNote(
            *learned.model, rootMidiNote + offset, 0.0f, 0.0f);

        neuramar::SynthesisFrame frame;
        learned.model->evaluate(
            static_cast<float>(0.25 / learned.model->durationSeconds()), frame);
        const double renderedRoot = static_cast<double>(
            learned.model->rootFrequencyHz()) * frame.pitchRatio * registerRatio;
        double stretchedError = 0.0;
        double flatError = 0.0;
        int partials = 0;
        for (int partial = 6; partial <= 14; partial += 4)
        {
            const double number = static_cast<double>(partial);
            const double target = renderedRoot * number
                * std::sqrt(1.0 + stiffInharmonicity * number * number);
            if (target >= 0.40 * sampleRate)
                break;
            stretchedError += std::abs(1200.0 * std::log2(
                partialFrequencyHz(stretched, target) / target));
            flatError += std::abs(1200.0 * std::log2(
                partialFrequencyHz(flat, target) / target));
            ++partials;
        }
        if (partials == 0)
            continue;
        stretchedError /= static_cast<double>(partials);
        flatError /= static_cast<double>(partials);
        stretchedSum += stretchedError;
        flatSum += flatError;
        ++measured;
        std::cout << "Stiff-string " << (offset >= 0 ? "+" : "") << offset
                  << " st: partial placement " << stretchedError
                  << " cents fitted, " << flatError
                  << " cents as an ideal harmonic bank\n";
    }

    expect(measured > 0, "no stiff-string register was measurable");
    if (measured == 0)
        return;
    const double stretchedMean = stretchedSum / static_cast<double>(measured);
    const double flatMean = flatSum / static_cast<double>(measured);
    std::cout << "Stiff-string aggregate: " << stretchedMean
              << " cents fitted, " << flatMean << " cents ideal-harmonic\n";
    expect(stretchedMean < 12.0,
           "fitted stiff-string rendering missed its known partial series ("
               + std::to_string(stretchedMean) + " cents)");
    expect(stretchedMean < 0.35 * flatMean,
           "stiff-string modelling did not materially improve partial "
           "placement over an ideal harmonic bank");
}

// ---------------------------------------------------------------------------
// Root-note reconstruction harness
//
// The register metrics above measure extrapolation: how well a model fitted at
// one pitch describes a different pitch. They say nothing about the instrument's
// central claim, which is that the fitted model reproduces the sound that was
// dropped in. Everything below measures that, at the analysed pitch, against the
// generating fixture, using the metric definitions in
// Docs/resynthesis-quality-benchmark.md.
// ---------------------------------------------------------------------------

using Complex = std::complex<double>;

void fft(std::vector<Complex>& values)
{
    const std::size_t size = values.size();
    if (size < 2)
        return;
    for (std::size_t index = 1, target = 0; index < size; ++index)
    {
        std::size_t bit = size >> 1;
        for (; (target & bit) != 0; bit >>= 1)
            target &= ~bit;
        target |= bit;
        if (index < target)
            std::swap(values[index], values[target]);
    }
    for (std::size_t length = 2; length <= size; length <<= 1)
    {
        const double angle = -2.0 * pi / static_cast<double>(length);
        const Complex root(std::cos(angle), std::sin(angle));
        for (std::size_t offset = 0; offset < size; offset += length)
        {
            Complex twiddle(1.0, 0.0);
            for (std::size_t index = 0; index < length / 2; ++index)
            {
                const Complex even = values[offset + index];
                const Complex odd = values[offset + index + length / 2] * twiddle;
                values[offset + index] = even + odd;
                values[offset + index + length / 2] = even - odd;
                twiddle *= root;
            }
        }
    }
}

// One magnitude spectrogram: frames of `window` samples advanced by `hop`,
// Hann-windowed, keeping the positive half spectrum.
[[nodiscard]] std::vector<std::vector<double>> spectrogram(
    const std::vector<float>& signal, std::size_t window, std::size_t hop)
{
    std::vector<std::vector<double>> frames;
    if (signal.size() < window)
        return frames;
    std::vector<double> hann(window, 0.0);
    for (std::size_t index = 0; index < window; ++index)
        hann[index] = 0.5 - 0.5 * std::cos(
            2.0 * pi * static_cast<double>(index)
            / static_cast<double>(window - 1));
    for (std::size_t start = 0; start + window <= signal.size(); start += hop)
    {
        std::vector<Complex> bins(window);
        for (std::size_t index = 0; index < window; ++index)
            bins[index] = Complex(
                static_cast<double>(signal[start + index]) * hann[index], 0.0);
        fft(bins);
        std::vector<double> magnitudes(window / 2 + 1, 0.0);
        for (std::size_t bin = 0; bin < magnitudes.size(); ++bin)
            magnitudes[bin] = std::abs(bins[bin]);
        frames.push_back(std::move(magnitudes));
    }
    return frames;
}

struct SpectralPair
{
    double convergence { 0.0 };
    double logMagnitudeMaeDb { 0.0 };
};

// Spectral convergence and log-magnitude MAE exactly as specified in the
// benchmark: epsilon is -100 dB relative to the reference peak, and the log term
// is evaluated on the union of bins where either signal exceeds -80 dB relative
// to that peak, so energy added where the reference is silent is penalised.
[[nodiscard]] SpectralPair scoreResolution(const std::vector<float>& reference,
                                           const std::vector<float>& render,
                                           std::size_t window, std::size_t hop)
{
    const auto referenceFrames = spectrogram(reference, window, hop);
    const auto renderFrames = spectrogram(render, window, hop);
    const std::size_t frameCount = std::min(referenceFrames.size(),
                                            renderFrames.size());
    if (frameCount == 0)
        return {};

    double peak = 0.0;
    for (std::size_t frame = 0; frame < frameCount; ++frame)
        for (const double magnitude : referenceFrames[frame])
            peak = std::max(peak, magnitude);
    const double epsilon = peak * std::pow(10.0, -100.0 / 20.0);
    const double floorMagnitude = peak * std::pow(10.0, -80.0 / 20.0);

    double differenceSquared = 0.0;
    double referenceSquared = 0.0;
    double logSum = 0.0;
    std::size_t logCount = 0;
    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        const auto& referenceBins = referenceFrames[frame];
        const auto& renderBins = renderFrames[frame];
        for (std::size_t bin = 0; bin < referenceBins.size(); ++bin)
        {
            const double referenceMagnitude = referenceBins[bin];
            const double renderMagnitude = renderBins[bin];
            const double difference = referenceMagnitude - renderMagnitude;
            differenceSquared += difference * difference;
            referenceSquared += referenceMagnitude * referenceMagnitude;
            if (referenceMagnitude > floorMagnitude
                || renderMagnitude > floorMagnitude)
            {
                logSum += std::abs(
                    20.0 * std::log10(referenceMagnitude + epsilon)
                    - 20.0 * std::log10(renderMagnitude + epsilon));
                ++logCount;
            }
        }
    }
    SpectralPair result;
    result.convergence = std::sqrt(differenceSquared)
        / std::max(std::sqrt(referenceSquared), 1.0e-30);
    result.logMagnitudeMaeDb = logCount > 0
        ? logSum / static_cast<double>(logCount) : 0.0;
    return result;
}

struct ReconstructionScore
{
    double convergence { 0.0 };
    double logMagnitudeMaeDb { 0.0 };
    double residualErbMaeDb { 0.0 };
    double earlyEnergyMaeDb { 0.0 };
    double attackErrorMs { 0.0 };
};

// Glasberg and Moore's ERB-rate scale, used to place the residual bands.
[[nodiscard]] double erbRate(double frequencyHz) noexcept
{
    return 21.4 * std::log10(1.0 + 0.00437 * frequencyHz);
}

// Band power of everything that is not within two analysis bins of a harmonic
// of the known fundamental, accumulated into ERB-spaced bands and compared in
// dB. This is the "residual ERB-band power" row of the benchmark: it measures
// the noise between the partials, which is what the Air layer is fitted to.
[[nodiscard]] double residualErbPowerMaeDb(const std::vector<float>& reference,
                                           const std::vector<float>& render,
                                           double fundamentalHz)
{
    constexpr std::size_t window = 4096;
    constexpr std::size_t hop = 1024;
    constexpr std::size_t bandCount = 24;
    constexpr double lowestHz = 100.0;
    constexpr double highestHz = 16000.0;
    const auto referenceFrames = spectrogram(reference, window, hop);
    const auto renderFrames = spectrogram(render, window, hop);
    const std::size_t frameCount = std::min(referenceFrames.size(),
                                            renderFrames.size());
    if (frameCount == 0)
        return 0.0;

    const double binWidth = sampleRate / static_cast<double>(window);
    const double lowestRate = erbRate(lowestHz);
    const double rateSpan = erbRate(highestHz) - lowestRate;
    std::vector<int> bandOfBin(referenceFrames[0].size(), -1);
    for (std::size_t bin = 0; bin < bandOfBin.size(); ++bin)
    {
        const double frequency = static_cast<double>(bin) * binWidth;
        if (frequency < lowestHz || frequency > highestHz)
            continue;
        // Two bins on either side of a harmonic covers the Hann main lobe, so
        // what remains is genuinely between the partials rather than the
        // shoulder of one.
        const double nearest = std::round(frequency / fundamentalHz);
        if (nearest >= 1.0
            && std::abs(frequency - nearest * fundamentalHz) < 2.5 * binWidth)
            continue;
        const double position = (erbRate(frequency) - lowestRate) / rateSpan;
        bandOfBin[bin] = static_cast<int>(std::min(
            static_cast<double>(bandCount - 1),
            std::floor(position * static_cast<double>(bandCount))));
    }

    double totalReferencePower = 0.0;
    std::vector<std::array<double, bandCount>> referenceBands(frameCount);
    std::vector<std::array<double, bandCount>> renderBands(frameCount);
    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        referenceBands[frame].fill(0.0);
        renderBands[frame].fill(0.0);
        for (std::size_t bin = 0; bin < bandOfBin.size(); ++bin)
        {
            if (bandOfBin[bin] < 0)
                continue;
            const auto band = static_cast<std::size_t>(bandOfBin[bin]);
            const double referencePower = referenceFrames[frame][bin]
                * referenceFrames[frame][bin];
            referenceBands[frame][band] += referencePower;
            renderBands[frame][band] += renderFrames[frame][bin]
                * renderFrames[frame][bin];
            totalReferencePower = std::max(totalReferencePower, referencePower);
        }
    }

    // Only score cells the reference actually populates. A band 60 dB below the
    // loudest observed residual cell carries no evidence about the source's
    // noise floor and would otherwise dominate a dB-domain mean.
    const double floorPower = totalReferencePower * std::pow(10.0, -60.0 / 10.0);
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        for (std::size_t band = 0; band < bandCount; ++band)
        {
            if (referenceBands[frame][band] <= floorPower)
                continue;
            sum += std::abs(
                10.0 * std::log10(referenceBands[frame][band] + 1.0e-30)
                - 10.0 * std::log10(renderBands[frame][band] + 1.0e-30));
            ++count;
        }
    }
    return count > 0 ? sum / static_cast<double>(count) : 0.0;
}

[[nodiscard]] double cumulativeEnergyDb(const std::vector<float>& signal,
                                        double seconds)
{
    const auto last = std::min(signal.size(), static_cast<std::size_t>(
        std::llround(seconds * sampleRate)));
    double sum = 0.0;
    for (std::size_t index = 0; index < last; ++index)
        sum += static_cast<double>(signal[index]) * signal[index];
    return 10.0 * std::log10(sum + 1.0e-30);
}

// T10-T90 of a 1 ms RMS envelope over the first 200 ms, in milliseconds.
[[nodiscard]] double attackTimeMs(const std::vector<float>& signal)
{
    const auto span = static_cast<std::size_t>(std::llround(0.001 * sampleRate));
    const auto limit = std::min(signal.size(), static_cast<std::size_t>(
        std::llround(0.200 * sampleRate)));
    std::vector<double> envelope;
    for (std::size_t start = 0; start + span <= limit; start += span)
    {
        double sum = 0.0;
        for (std::size_t index = start; index < start + span; ++index)
            sum += static_cast<double>(signal[index]) * signal[index];
        envelope.push_back(std::sqrt(sum / static_cast<double>(span)));
    }
    if (envelope.size() < 3)
        return 0.0;
    const double peak = *std::max_element(envelope.begin(), envelope.end());
    if (!(peak > 0.0))
        return 0.0;
    const auto crossing = [&envelope, peak](double fraction)
    {
        for (std::size_t index = 0; index < envelope.size(); ++index)
            if (envelope[index] >= fraction * peak)
                return static_cast<double>(index);
        return static_cast<double>(envelope.size());
    };
    return crossing(0.90) - crossing(0.10);
}

// The frozen Match state from the benchmark: full learned Imprint, Core, Air,
// and Bone; Mutation, Orbit, spread, and Noise off; neutral tone controls; zero
// added attack; ordinary evolution rate; linear output.
[[nodiscard]] std::vector<float> renderMatchState(
    const neuramar::NeuralModel& model, int midiNote, std::size_t sampleCount)
{
    neuramar::NeuramarEngine engine;
    engine.prepare(sampleRate, 128);
    engine.setModel(&model);
    neuramar::EngineParameters parameters;
    parameters.imprint = 1.0f;
    parameters.bodyLock = 0.65f;
    parameters.air = 1.0f;
    parameters.bone = 1.0f;
    parameters.brightness = 0.5f;
    parameters.evolutionRate = 1.0f;
    parameters.orbit = 0.0f;
    parameters.mutation = 0.0f;
    parameters.noise = 0.0f;
    parameters.attackSeconds = 0.0f;
    parameters.releaseSeconds = 0.35f;
    parameters.spread = 0.0f;
    parameters.stretch = 1.0f;
    parameters.formantShiftSemitones = 0.0f;
    parameters.touch = 0.0f;
    parameters.registerTilt = 0.0f;
    parameters.outputGain = 0.5f;
    engine.setParameters(parameters);

    std::vector<float> left(sampleCount, 0.0f);
    std::vector<float> right(sampleCount, 0.0f);
    engine.noteOn(midiNote, 0.82f);
    for (std::size_t offset = 0; offset < sampleCount; offset += 128)
    {
        const auto count = static_cast<int>(
            std::min<std::size_t>(128, sampleCount - offset));
        engine.process(left.data() + offset, right.data() + offset, count);
    }
    for (std::size_t index = 0; index < left.size(); ++index)
        left[index] = 0.5f * (left[index] + right[index]);
    return left;
}

[[nodiscard]] double signalRms(const std::vector<float>& signal)
{
    double sum = 0.0;
    for (const float value : signal)
        sum += static_cast<double>(value) * value;
    return std::sqrt(sum / static_cast<double>(std::max<std::size_t>(
        signal.size(), 1)));
}

[[nodiscard]] ReconstructionScore scoreRootReconstruction(
    const std::vector<float>& reference, const neuramar::NeuralModel& model,
    double fundamentalHz, const std::string& label)
{
    // The conditioning pass removes DC and normalises the analysed signal to a
    // 0.90 peak. Applying the same normalisation to the reference here is what
    // makes the single RMS scalar below a level match rather than a level
    // correction that hides one.
    std::vector<float> target(reference);
    double mean = 0.0;
    for (const float value : target)
        mean += value;
    mean /= static_cast<double>(std::max<std::size_t>(target.size(), 1));
    float peak = 0.0f;
    for (float& value : target)
    {
        value -= static_cast<float>(mean);
        peak = std::max(peak, std::abs(value));
    }
    for (float& value : target)
        value *= 0.90f / std::max(peak, 1.0e-9f);

    auto render = renderMatchState(model, rootMidiNote, target.size());
    const double scalar = signalRms(target)
        / std::max(signalRms(render), 1.0e-12);
    for (float& value : render)
        value = static_cast<float>(value * scalar);

    ReconstructionScore score;
    constexpr std::array<std::pair<std::size_t, std::size_t>, 3> resolutions {
        std::pair<std::size_t, std::size_t> { 256, 64 },
        std::pair<std::size_t, std::size_t> { 1024, 256 },
        std::pair<std::size_t, std::size_t> { 4096, 1024 }
    };
    for (const auto& [window, hop] : resolutions)
    {
        const auto pair = scoreResolution(target, render, window, hop);
        score.convergence += pair.convergence;
        score.logMagnitudeMaeDb += pair.logMagnitudeMaeDb;
        std::cout << label << " (" << window << ", " << hop
                  << "): spectral convergence " << pair.convergence
                  << ", log-magnitude MAE " << pair.logMagnitudeMaeDb << " dB\n";
    }
    score.convergence /= static_cast<double>(resolutions.size());
    score.logMagnitudeMaeDb /= static_cast<double>(resolutions.size());
    score.residualErbMaeDb = residualErbPowerMaeDb(target, render,
                                                   fundamentalHz);

    constexpr std::array<double, 5> earlyTimes { 0.001, 0.005, 0.010, 0.020,
                                                 0.050 };
    for (const double seconds : earlyTimes)
    {
        const double difference = cumulativeEnergyDb(target, seconds)
            - cumulativeEnergyDb(render, seconds);
        score.earlyEnergyMaeDb += std::abs(difference);
        std::cout << label << " cumulative energy at "
                  << 1000.0 * seconds << " ms: " << difference << " dB\n";
    }
    score.earlyEnergyMaeDb /= static_cast<double>(earlyTimes.size());
    score.attackErrorMs = (attackTimeMs(render) - attackTimeMs(target));

    std::cout << label << " aggregate: convergence " << score.convergence
              << ", log-magnitude MAE " << score.logMagnitudeMaeDb
              << " dB, residual ERB MAE " << score.residualErbMaeDb
              << " dB, early-energy MAE " << score.earlyEnergyMaeDb
              << " dB, T10-T90 error " << score.attackErrorMs << " ms\n";
    return score;
}

// Gold-corpus items 5 and 6 in one fixture: harmonics with a time-varying
// shaped noise layer and a short broadband transient at the onset. The noise is
// deterministic so the fixture is reproducible, but the renderer draws its own
// independent realisation, which is exactly the condition the residual metric
// is designed for.
[[nodiscard]] std::vector<float> makeNoisyTransientNote(double fundamentalHz,
                                                        double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(durationSeconds * sampleRate));
    std::vector<float> signal(sampleCount, 0.0f);
    const int harmonicCount = std::min(48, static_cast<int>(std::floor(
        0.44 * sampleRate / fundamentalHz)));

    // Two fixed noise resonances plus a hiss shelf. Their levels move
    // independently over the note, so a filterbank that is too coarse cannot
    // track them even with a perfect amplitude trajectory.
    struct NoiseResonator
    {
        double centreHz;
        double quality;
        double startGain;
        double endGain;
        double z1;
        double z2;
    };
    std::array<NoiseResonator, 3> resonators {
        NoiseResonator { 420.0, 5.5, 1.00, 0.22, 0.0, 0.0 },
        NoiseResonator { 2300.0, 7.0, 0.35, 0.85, 0.0, 0.0 },
        NoiseResonator { 7400.0, 3.0, 0.80, 0.18, 0.0, 0.0 }
    };

    std::uint32_t noiseState = 0x1f2e3d4cu;
    const auto nextNoise = [&noiseState]() noexcept
    {
        noiseState = noiseState * 1664525u + 1013904223u;
        return static_cast<double>(noiseState >> 8) / 8388608.0 - 1.0;
    };

    for (std::size_t index = 0; index < sampleCount; ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double position = static_cast<double>(index)
            / static_cast<double>(std::max<std::size_t>(sampleCount - 1, 1));
        const double envelope = (1.0 - std::exp(-time / 0.004))
            * (0.72 + 0.28 * std::exp(-time / 0.30));
        double harmonic = 0.0;
        for (int partial = 1; partial <= harmonicCount; ++partial)
        {
            const double number = static_cast<double>(partial);
            // A slowly closing spectral tilt: the upper partials decay faster,
            // which is what the learned time trajectory has to reproduce.
            const double decay = std::exp(-time * (0.35 + 0.11 * number));
            harmonic += std::pow(number, -0.85) * decay * std::sin(
                2.0 * pi * fundamentalHz * number * time
                + 0.31 * number + 0.05 * number * number);
        }

        double noise = 0.0;
        const double excitation = nextNoise();
        for (auto& resonator : resonators)
        {
            const double omega = 2.0 * pi * resonator.centreHz / sampleRate;
            const double alpha = std::sin(omega) / (2.0 * resonator.quality);
            const double a0 = 1.0 + alpha;
            const double b0 = alpha / a0;
            const double a1 = -2.0 * std::cos(omega) / a0;
            const double a2 = (1.0 - alpha) / a0;
            const double input = excitation;
            const double output = b0 * input + resonator.z1;
            resonator.z1 = -a1 * output + resonator.z2;
            resonator.z2 = -b0 * input - a2 * output;
            noise += output * (resonator.startGain
                + position * (resonator.endGain - resonator.startGain));
        }

        // A 3 ms broadband transient. It carries roughly a quarter of the
        // note's peak amplitude, so an analysis aperture that smears it shows up
        // immediately in the early cumulative-energy rows.
        const double transient = 0.55 * std::exp(-time / 0.0030)
            * excitation;

        signal[index] = static_cast<float>(
            0.22 * (envelope * (harmonic + 0.30 * noise) + transient));
    }
    return signal;
}

void testRootNoteReconstruction()
{
    struct Fixture
    {
        const char* label;
        std::vector<float> audio;
    };
    const std::array<Fixture, 2> fixtures {
        Fixture { "Root reconstruction, source/filter",
                  makeSourceFilterNote(rootFrequencyHz, 0.82) },
        Fixture { "Root reconstruction, noise+transient",
                  makeNoisyTransientNote(rootFrequencyHz, 0.90) }
    };

    // Generous guards. These are regression bounds on numbers that had never
    // been measured before, not acceptance gates; the gates live in
    // Docs/resynthesis-quality-benchmark.md and remain targets.
    constexpr double convergenceGuard = 0.15;
    constexpr double logMagnitudeGuard = 13.0;
    constexpr double residualErbGuard = 7.0;
    // 2 dB is below what a four-period aperture can reach on the source/filter
    // fixture: it measured 3.45 dB, and halving the aperture over the first
    // 40 ms measures 1.32 dB. This guard keeps the onset aperture short.
    constexpr double earlyEnergyGuard = 2.0;
    constexpr double attackGuard = 8.0;

    for (const auto& fixture : fixtures)
    {
        const auto learned = neuramar::SampleLearner::learn(fixture.audio,
                                                            sampleRate);
        expect(static_cast<bool>(learned),
               std::string(fixture.label) + " failed to learn: "
                   + learned.error);
        if (!learned)
            continue;
        expect(std::abs(learned.model->rootFrequencyHz() / rootFrequencyHz
                        - 1.0) < 0.02,
               std::string(fixture.label) + " root was identified incorrectly");
        const auto score = scoreRootReconstruction(
            fixture.audio, *learned.model, rootFrequencyHz, fixture.label);
        expect(score.convergence < convergenceGuard,
               std::string(fixture.label) + " spectral convergence regressed ("
                   + std::to_string(score.convergence) + ")");
        expect(score.logMagnitudeMaeDb < logMagnitudeGuard,
               std::string(fixture.label) + " log-magnitude MAE regressed ("
                   + std::to_string(score.logMagnitudeMaeDb) + " dB)");
        expect(score.residualErbMaeDb < residualErbGuard,
               std::string(fixture.label) + " residual ERB MAE regressed ("
                   + std::to_string(score.residualErbMaeDb) + " dB)");
        expect(score.earlyEnergyMaeDb < earlyEnergyGuard,
               std::string(fixture.label) + " early cumulative energy regressed ("
                   + std::to_string(score.earlyEnergyMaeDb) + " dB)");
        expect(std::abs(score.attackErrorMs) < attackGuard,
               std::string(fixture.label) + " T10-T90 attack error regressed ("
                   + std::to_string(score.attackErrorMs) + " ms)");
    }
}

// Analytic ground-truth item 7: a struck body whose inharmonic modes are known
// exactly. The benchmark asks for active-mode precision and recall and for
// peak-frequency error in cents; this is that measurement, and it is also what
// bounds the modal branch's capacity, because a slot that does not exist cannot
// recall a mode.
constexpr std::array<double, 10> knownModeRatios {
    1.43, 2.37, 3.61, 4.55, 5.49, 6.72, 8.31, 9.58, 11.24, 13.47
};

[[nodiscard]] std::vector<float> makeStruckBodyNote(double fundamentalHz,
                                                    double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(durationSeconds * sampleRate));
    std::vector<float> signal(sampleCount, 0.0f);
    for (std::size_t index = 0; index < sampleCount; ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double attack = 1.0 - std::exp(-time / 0.0025);
        // A clear harmonic series so the root stays identifiable, and ten
        // long-ringing inharmonic modes on top of it.
        double value = 0.0;
        for (int harmonic = 1; harmonic <= 10; ++harmonic)
        {
            const double number = static_cast<double>(harmonic);
            const double weight = harmonic == 1 ? 2.4 : 1.0;
            value += weight * std::pow(number, -1.15)
                * std::exp(-time * (0.55 + 0.10 * number))
                * std::sin(2.0 * pi * fundamentalHz * number * time
                           + 0.23 * number);
        }
        for (std::size_t mode = 0; mode < knownModeRatios.size(); ++mode)
        {
            const double ratio = knownModeRatios[mode];
            const double frequency = fundamentalHz * ratio;
            if (frequency >= 0.42 * sampleRate)
                continue;
            value += 0.10 * std::exp(-time * (0.55 + 0.045 * ratio))
                * std::sin(2.0 * pi * frequency * time
                           + 0.61 * static_cast<double>(mode + 1));
        }
        signal[index] = static_cast<float>(0.22 * attack * value);
    }
    return signal;
}

void testStruckBodyModalRecall()
{
    const auto source = makeStruckBodyNote(rootFrequencyHz, 1.30);
    const auto learned = neuramar::SampleLearner::learn(source, sampleRate);
    expect(static_cast<bool>(learned),
           "struck-body fixture failed to learn: " + learned.error);
    if (!learned)
        return;

    const double detectedRoot = static_cast<double>(
        learned.model->rootFrequencyHz());
    std::cout << "Struck-body detected root: " << detectedRoot << " Hz\n";
    expect(std::abs(detectedRoot / rootFrequencyHz - 1.0) < 0.02,
           "struck-body fixture root was identified incorrectly");

    const auto ratios = learned.model->boneFrequencyRatios();
    const auto reliabilities = learned.model->boneModeReliabilities();
    std::size_t activeSlots = 0;
    for (std::size_t mode = 0; mode < ratios.size(); ++mode)
        if (reliabilities[mode] > 0.0f)
            ++activeSlots;

    std::size_t recalled = 0;
    std::size_t matchedSlots = 0;
    double centsErrorSum = 0.0;
    std::vector<bool> slotMatched(ratios.size(), false);
    for (const double expected : knownModeRatios)
    {
        double bestCents = 1.0e9;
        std::size_t bestSlot = ratios.size();
        for (std::size_t mode = 0; mode < ratios.size(); ++mode)
        {
            if (reliabilities[mode] <= 0.0f)
                continue;
            const double cents = std::abs(1200.0 * std::log2(
                static_cast<double>(ratios[mode]) / expected));
            if (cents < bestCents)
            {
                bestCents = cents;
                bestSlot = mode;
            }
        }
        // A quarter tone is generous as an identity test and still far tighter
        // than the 0.22-ratio spacing the selector enforces between slots.
        if (bestSlot < ratios.size() && bestCents < 50.0)
        {
            ++recalled;
            centsErrorSum += bestCents;
            if (!slotMatched[bestSlot])
            {
                slotMatched[bestSlot] = true;
                ++matchedSlots;
            }
        }
    }

    const double recall = static_cast<double>(recalled)
        / static_cast<double>(knownModeRatios.size());
    const double precision = activeSlots > 0
        ? static_cast<double>(matchedSlots) / static_cast<double>(activeSlots)
        : 0.0;
    const double meanCents = recalled > 0
        ? centsErrorSum / static_cast<double>(recalled) : 0.0;
    std::cout << "Struck-body modal result: " << activeSlots
              << " active slots, recall " << recall << ", precision "
              << precision << ", mean frequency error " << meanCents
              << " cents\n";

    // Six slots cap recall at 0.6 by construction, so this guard is what keeps
    // the modal branch wide enough to describe a struck body.
    expect(recall >= 0.70,
           "the modal branch recalled too few of the fixture's known modes ("
               + std::to_string(recall) + ")");
    expect(precision >= 0.70,
           "the modal branch spent too many slots on modes the fixture does "
           "not have (precision " + std::to_string(precision) + ")");
    expect(meanCents < 25.0,
           "recalled modal frequencies missed their known positions ("
               + std::to_string(meanCents) + " cents)");
}

// ---------------------------------------------------------------------------
// Automatic root detection across the analytic source classes
//
// The benchmark's one-click protocol treats automatic root error as a headline
// result and proposes a 98%-correct gate, and until now the suite tested four
// hand-picked cases. This is a corpus: one fixture per analytic ground-truth
// class, each with a known fundamental, reported as a correct-semitone rate and
// an octave-error rate. Alchemy's own documentation makes the stake explicit -
// additive import quality depends on identifying the root, and a resynthesis
// model built on the wrong octave is wrong in a way no later control fixes.
// ---------------------------------------------------------------------------

[[nodiscard]] int midiNoteFor(double frequencyHz) noexcept
{
    return static_cast<int>(std::lround(
        69.0 + 12.0 * std::log2(frequencyHz / 440.0)));
}

struct RootCase
{
    const char* name;
    double fundamentalHz;
    std::vector<float> audio;
};

[[nodiscard]] std::vector<float> makeRootFixture(const std::string& kind,
                                                 double fundamentalHz,
                                                 double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(durationSeconds * sampleRate));
    std::vector<float> signal(sampleCount, 0.0f);
    std::uint32_t noiseState = 0x2b7e1516u;
    const auto nextNoise = [&noiseState]() noexcept
    {
        noiseState = noiseState * 1664525u + 1013904223u;
        return static_cast<double>(noiseState >> 8) / 8388608.0 - 1.0;
    };
    const int partialCeiling = std::max(1, static_cast<int>(std::floor(
        0.44 * sampleRate / fundamentalHz)));

    for (std::size_t index = 0; index < sampleCount; ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double position = static_cast<double>(index)
            / static_cast<double>(std::max<std::size_t>(sampleCount - 1, 1));
        const double envelope = (1.0 - std::exp(-time / 0.006))
            * (0.85 + 0.15 * std::exp(-time / 0.40));
        double value = 0.0;

        if (kind == "rolloff")
        {
            for (int harmonic = 1; harmonic <= std::min(28, partialCeiling);
                 ++harmonic)
                value += std::exp(-0.22 * (harmonic - 1))
                    * std::sin(2.0 * pi * fundamentalHz * harmonic * time
                               + 0.19 * harmonic);
        }
        else if (kind == "missing-fundamental")
        {
            // Odd/even contrast with no energy at all at f0 or 2 f0.
            for (int harmonic = 3; harmonic <= std::min(20, partialCeiling);
                 ++harmonic)
            {
                const double weight = (harmonic & 1) != 0 ? 1.0 : 0.22;
                value += weight * std::pow(harmonic, -0.85)
                    * std::sin(2.0 * pi * fundamentalHz * harmonic * time
                               + 0.31 * harmonic);
            }
        }
        else if (kind == "formant")
        {
            for (int harmonic = 1; harmonic <= std::min(40, partialCeiling);
                 ++harmonic)
            {
                const double frequency = fundamentalHz * harmonic;
                const double shape =
                    0.06 + 1.60 * gaussianLogFrequency(frequency, 640.0, 0.19)
                    + 1.05 * gaussianLogFrequency(frequency, 1720.0, 0.17);
                value += shape * std::pow(harmonic, -0.55)
                    * std::sin(2.0 * pi * frequency * time + 0.11 * harmonic);
            }
        }
        else if (kind == "stiff")
        {
            for (int partial = 1; partial <= std::min(26, partialCeiling);
                 ++partial)
            {
                const double number = static_cast<double>(partial);
                const double frequency = fundamentalHz * number
                    * std::sqrt(1.0 + 4.0e-4 * number * number);
                if (frequency >= 0.44 * sampleRate)
                    break;
                value += std::pow(number, -1.05)
                    * std::exp(-time * (0.7 + 0.05 * number))
                    * std::sin(2.0 * pi * frequency * time + 0.23 * number);
            }
        }
        else if (kind == "noisy")
        {
            for (int harmonic = 1; harmonic <= std::min(24, partialCeiling);
                 ++harmonic)
                value += std::pow(harmonic, -0.95)
                    * std::sin(2.0 * pi * fundamentalHz * harmonic * time
                               + 0.17 * harmonic);
            value += 0.28 * nextNoise();
        }
        else if (kind == "vibrato")
        {
            // A stable 5 Hz, 35-cent vibrato: inside the four-semitone contour
            // the analyser is allowed to track.
            const double deviation = 0.35 / 12.0
                * std::sin(2.0 * pi * 5.0 * time);
            const double phaseHz = fundamentalHz * std::exp2(deviation);
            for (int harmonic = 1; harmonic <= std::min(20, partialCeiling);
                 ++harmonic)
                value += std::pow(harmonic, -1.0)
                    * std::sin(2.0 * pi * phaseHz * harmonic * time
                               + 0.13 * harmonic);
        }
        else if (kind == "moving-formant")
        {
            const double centreHz = 700.0 * std::exp2(1.1 * position);
            for (int harmonic = 1; harmonic <= std::min(36, partialCeiling);
                 ++harmonic)
            {
                const double frequency = fundamentalHz * harmonic;
                const double shape = 0.10
                    + 1.70 * gaussianLogFrequency(frequency, centreHz, 0.28);
                value += shape * std::pow(harmonic, -0.65)
                    * std::sin(2.0 * pi * frequency * time + 0.29 * harmonic);
            }
        }
        else if (kind == "delayed-onsets")
        {
            for (int harmonic = 1; harmonic <= std::min(24, partialCeiling);
                 ++harmonic)
            {
                const double delay = 0.0015 * (harmonic - 1);
                if (time < delay)
                    continue;
                value += std::pow(harmonic, -0.9)
                    * (1.0 - std::exp(-(time - delay) / 0.004))
                    * std::sin(2.0 * pi * fundamentalHz * harmonic * time
                               + 0.21 * harmonic);
            }
            value += 0.9 * std::exp(-time / 0.0020) * nextNoise();
        }
        else if (kind == "swept-saw")
        {
            // A band-limited saw through a moving resonant low-pass: every
            // partial is present and the spectral peak walks two octaves.
            const double cutoffHz = fundamentalHz * std::exp2(
                2.0 + 2.0 * position);
            for (int harmonic = 1; harmonic <= std::min(64, partialCeiling);
                 ++harmonic)
            {
                const double frequency = fundamentalHz * harmonic;
                const double excess = std::max(0.0,
                    std::log2(frequency / cutoffHz));
                const double resonance = 1.0
                    + 2.6 * gaussianLogFrequency(frequency, cutoffHz, 0.10);
                value += resonance * std::exp2(-3.0 * excess) / harmonic
                    * std::sin(2.0 * pi * frequency * time + 0.07 * harmonic);
            }
        }
        else if (kind == "phase-modulation")
        {
            // A 3:1 phase-modulation tone at a high index. The series stays
            // harmonic but the fundamental is far from the loudest partial.
            const double index2 = 5.5 * std::exp(-time / 0.28);
            value = std::sin(2.0 * pi * fundamentalHz * time
                             + index2 * std::sin(
                                 2.0 * pi * 3.0 * fundamentalHz * time));
        }

        signal[index] = static_cast<float>(0.20 * envelope * value);
    }
    return signal;
}

void testAutomaticRootCorpus()
{
    // One fixture per analytic ground-truth class, across the register range
    // the detector is bounded to. Durations are kept short deliberately: this
    // measures root identification, not fit quality, and the suite pays for
    // every learn.
    const std::vector<std::pair<std::string, double>> corpus {
        { "rolloff", 220.0 },
        { "rolloff", 61.735 },        // B1, low register
        { "rolloff", 987.767 },       // B5, high register
        { "missing-fundamental", 146.832 },
        { "formant", 110.0 },
        { "stiff", 329.628 },
        { "noisy", 196.0 },
        { "vibrato", 261.626 },
        { "moving-formant", 174.614 },
        { "delayed-onsets", 82.407 },
        { "swept-saw", 130.813 },
        { "phase-modulation", 233.082 }
    };

    std::size_t correct = 0;
    std::size_t octaveErrors = 0;
    for (const auto& [kind, fundamentalHz] : corpus)
    {
        const auto audio = makeRootFixture(kind, fundamentalHz, 0.62);
        const auto learned = neuramar::SampleLearner::learn(audio, sampleRate);
        if (!learned)
        {
            std::cout << "Root corpus " << kind << " @ " << fundamentalHz
                      << " Hz: failed to learn (" << learned.error << ")\n";
            expect(false, "root corpus fixture failed to learn: "
                       + learned.error);
            continue;
        }
        const int expectedNote = midiNoteFor(fundamentalHz);
        const int detectedNote = learned.model->rootMidiNote();
        const int error = detectedNote - expectedNote;
        if (error == 0)
            ++correct;
        else if (error % 12 == 0)
            ++octaveErrors;
        std::cout << "Root corpus " << kind << " @ " << fundamentalHz
                  << " Hz: detected " << learned.model->rootFrequencyHz()
                  << " Hz, MIDI " << detectedNote << " (expected "
                  << expectedNote << ", error " << error << " st)\n";
    }

    const double total = static_cast<double>(corpus.size());
    const double correctRate = static_cast<double>(correct) / total;
    const double octaveRate = static_cast<double>(octaveErrors) / total;
    std::cout << "Root corpus result: " << correctRate
              << " correct semitone, " << octaveRate << " octave errors over "
              << corpus.size() << " analytic classes\n";
    expect(correctRate >= 0.90,
           "automatic root detection fell below its corpus rate ("
               + std::to_string(correctRate) + ")");
    expect(octaveRate <= 0.10,
           "automatic root detection made too many octave errors ("
               + std::to_string(octaveRate) + ")");
}
} // namespace

int main()
{
    testHeldOutSourceFilterFamily();
    testStiffStringPartialPlacement();
    testRootNoteReconstruction();
    testStruckBodyModalRecall();
    testAutomaticRootCorpus();
    if (failures != 0)
    {
        std::cerr << failures << " Neuramar quality benchmark(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Neuramar held-out quality benchmarks passed\n";
    return EXIT_SUCCESS;
}
