#include "SampleLearner.h"

#include "AirFilterbank.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace neuramar
{
struct NeuralModelTrainingAccess
{
    static auto& outputMeans(NeuralModel& model) noexcept { return model.outputMeans_; }
    static auto& outputScales(NeuralModel& model) noexcept { return model.outputScales_; }
    static auto& inputWeights(NeuralModel& model) noexcept { return model.inputWeights_; }
    static const auto& inputWeights(const NeuralModel& model) noexcept
    {
        return model.inputWeights_;
    }
    static auto& hiddenBiases(NeuralModel& model) noexcept { return model.hiddenBiases_; }
    static const auto& hiddenBiases(const NeuralModel& model) noexcept
    {
        return model.hiddenBiases_;
    }
    static auto& outputWeights(NeuralModel& model) noexcept { return model.outputWeights_; }
    static const auto& outputWeights(const NeuralModel& model) noexcept
    {
        return model.outputWeights_;
    }
    static auto& outputBiases(NeuralModel& model) noexcept { return model.outputBiases_; }
    static const auto& outputBiases(const NeuralModel& model) noexcept
    {
        return model.outputBiases_;
    }
};

namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;
constexpr float amplitudeFloor = 1.12535175e-7f; // exp(-16)
constexpr int analysisFrameCount = 128;
constexpr int onsetAnalysisFrameCount = 48;
constexpr float denseOnsetSeconds = 0.120f;
// The dense onset region reserves 48 frames for the first 120 ms, 2.5 ms apart.
// The attack itself occupies the first third of that, and only there is the
// halved analysis aperture worth its lost frequency resolution: measured over
// the whole 120 ms it makes a noisy source's mid-band residual worse without
// improving the attack any further.
constexpr float shortApertureSeconds = 0.040f;
constexpr std::size_t spectrumSize = 4096;
constexpr int trainingEpochCount = 180;
constexpr std::size_t airFitCellCount = 48;
constexpr double maximumSampleSeconds = 20.0;
constexpr double fixedAnalysisSampleRate = 48000.0;
constexpr float airLowerFrequencyHz = 80.0f;
constexpr float airUpperFrequencyHz = 16000.0f;

using Complex = std::complex<float>;
using TargetFrame = std::array<float, NeuralModel::outputSize>;

[[nodiscard]] std::array<float, analysisFrameCount> makeAnalysisTimes(
    float durationSeconds) noexcept
{
    std::array<float, analysisFrameCount> times {};
    const float safeDuration = std::max(durationSeconds, 1.0e-6f);
    const float onsetEnd = std::min(1.0f, denseOnsetSeconds / safeDuration);

    // For normal musical samples, 48 physical-time keyframes describe the
    // first 120 ms and the remaining 80 cover sustain and decay. Extremely
    // short samples use the whole bounded grid because their complete gesture
    // is effectively an onset and there is no distinct sustain region.
    if (onsetEnd < 0.999f)
    {
        for (int frame = 0; frame < onsetAnalysisFrameCount; ++frame)
        {
            const float position = static_cast<float>(frame)
                / static_cast<float>(onsetAnalysisFrameCount - 1);
            times[static_cast<std::size_t>(frame)] = onsetEnd
                * std::pow(position, 1.18f);
        }

        constexpr int remainingFrameCount = analysisFrameCount
            - onsetAnalysisFrameCount;
        for (int frame = 0; frame < remainingFrameCount; ++frame)
        {
            const float position = static_cast<float>(frame + 1)
                / static_cast<float>(remainingFrameCount);
            times[static_cast<std::size_t>(onsetAnalysisFrameCount + frame)]
                = onsetEnd + (1.0f - onsetEnd)
                    * std::pow(position, 1.24f);
        }
    }
    else
    {
        for (int frame = 0; frame < analysisFrameCount; ++frame)
        {
            const float position = static_cast<float>(frame)
                / static_cast<float>(analysisFrameCount - 1);
            times[static_cast<std::size_t>(frame)] = std::pow(position, 1.35f);
        }
    }

    times.front() = 0.0f;
    times.back() = 1.0f;
    for (std::size_t frame = 1; frame < times.size(); ++frame)
    {
        if (times[frame] <= times[frame - 1])
            times[frame] = std::nextafter(times[frame - 1], 1.0f);
    }
    return times;
}

struct PitchEstimate
{
    float frequencyHz { 0.0f };
    float confidence { 0.0f };
};

struct SpectrumFrame
{
    float normalisedTime { 0.0f };
    float timeSeconds { 0.0f };
    float normalisation { 1.0f };
    float noisePowerCorrection { 1.0f };
    std::size_t analysisWindowSize { spectrumSize };
    std::vector<Complex> bins;
};

struct HarmonicAnalysisFrame
{
    SpectrumFrame residualSpectrum;
    std::array<float, NeuralModel::harmonicCount> amplitudes {};
    std::array<float, NeuralModel::harmonicCount> sinePhases {};
};

struct BoneSelection
{
    std::array<float, NeuralModel::boneModeCount> ratios {};
    std::array<float, NeuralModel::boneModeCount> reliabilities {};
};

using AirResponseMatrix = std::array<
    std::array<double, NeuralModel::airBandCount>, airFitCellCount>;

struct AirFitDesign
{
    AirResponseMatrix response {};
    // How many spectrum bins actually contributed to each cell. The cells are
    // log-spaced over a linear FFT grid, so the lowest hold a single bin while
    // the highest hold well over a hundred.
    std::array<int, airFitCellCount> evidence {};
    float lowerFrequencyHz { airLowerFrequencyHz };
    float upperFrequencyHz { airUpperFrequencyHz };
    // log(upperFrequencyHz / lowerFrequencyHz), resolved once by
    // makeAirFitDesign(). airCellForFrequency() is called once per FFT bin -
    // hundreds to thousands of times per design - and this ratio is fixed for
    // the design's whole lifetime, so recomputing it per call was pure waste.
    float logFrequencyRangeSpan { 1.0f };
};

struct AirFitResult
{
    std::array<float, NeuralModel::airBandCount> amplitudes {};
    std::array<double, NeuralModel::airBandCount> powers {};
};

[[nodiscard]] bool cancellationRequested(
    const SampleLearner::CancelPredicate& predicate)
{
    return predicate && predicate();
}

struct MidiPitchEstimate
{
    int note { 60 };
    float cents { 0.0f };
};

// Converts a frequency in Hz to the nearest MIDI note and the signed cents
// offset from it. Both the progress callback and the published model
// metadata report the same root frequency this way, so the rounding and
// the cents reference are computed in exactly one place.
[[nodiscard]] MidiPitchEstimate midiPitchFromFrequency(float frequencyHz) noexcept
{
    const float midi = 69.0f + 12.0f * std::log2(frequencyHz / 440.0f);
    const int note = std::clamp(static_cast<int>(std::lround(midi)), 0, 127);
    return { note, 100.0f * (midi - static_cast<float>(note)) };
}

void report(const SampleLearner::ProgressCallback& callback,
            SampleLearner::Stage stage, float overall,
            const PitchEstimate& pitch, const char* message)
{
    if (!callback)
        return;

    SampleLearner::Progress progress;
    progress.stage = stage;
    progress.overallProgress = std::clamp(overall, 0.0f, 1.0f);
    progress.rootFrequencyHz = pitch.frequencyHz;
    progress.pitchConfidence = pitch.confidence;
    if (pitch.frequencyHz > 0.0f)
    {
        const auto midiPitch = midiPitchFromFrequency(pitch.frequencyHz);
        progress.rootMidiNote = midiPitch.note;
        progress.rootCents = midiPitch.cents;
    }
    progress.message = message;
    callback(progress);
}

void fft(std::vector<Complex>& values)
{
    const std::size_t size = values.size();
    for (std::size_t index = 1, reversed = 0; index < size; ++index)
    {
        std::size_t bit = size >> 1;
        while ((reversed & bit) != 0)
        {
            reversed ^= bit;
            bit >>= 1;
        }
        reversed ^= bit;
        if (index < reversed)
            std::swap(values[index], values[reversed]);
    }

    for (std::size_t length = 2; length <= size; length <<= 1)
    {
        const float angle = -twoPi / static_cast<float>(length);
        const Complex root(std::cos(angle), std::sin(angle));
        for (std::size_t offset = 0; offset < size; offset += length)
        {
            Complex twiddle(1.0f, 0.0f);
            const std::size_t half = length >> 1;
            for (std::size_t index = 0; index < half; ++index)
            {
                const Complex even = values[offset + index];
                const Complex odd = values[offset + index + half] * twiddle;
                values[offset + index] = even + odd;
                values[offset + index + half] = even - odd;
                twiddle *= root;
            }
        }
    }
}

[[nodiscard]] float hannWindow(std::size_t index,
                               std::size_t windowSize) noexcept
{
    if (windowSize < 2 || index >= windowSize)
        return 0.0f;
    return 0.5f - 0.5f * std::cos(
        twoPi * static_cast<float>(index)
        / static_cast<float>(windowSize - 1));
}

// makeSpectrumFrame() always windows a full spectrumSize aperture, so its
// Hann coefficients depend only on that fixed length, never on which frame,
// probe, or sample is being analysed. A learn() pass calls it well over a
// hundred times - 128 timeline frames plus a handful of root and stretch
// probes - and every call used to re-evaluate the same 4096 std::cos()
// values from scratch. Building the table once, the same way
// ModelVisualisation.cpp's binOctaves table hoists its own per-index
// constant out of a per-call loop, removes that repetition; the values are
// unchanged, since both routes evaluate the identical hannWindow() formula.
[[nodiscard]] const std::array<float, spectrumSize>& spectrumHannWindow() noexcept
{
    static const std::array<float, spectrumSize> table = [] {
        std::array<float, spectrumSize> window {};
        for (std::size_t index = 0; index < spectrumSize; ++index)
            window[index] = hannWindow(index, spectrumSize);
        return window;
    }();
    return table;
}

[[nodiscard]] std::size_t transientAnalysisWindowSize(
    float fundamentalHz, double sampleRate, double periods = 4.0) noexcept
{
    if (!(fundamentalHz > 0.0f) || !std::isfinite(fundamentalHz))
        return spectrumSize;

    // Four fundamental periods give the weighted sinusoidal solve enough
    // information while avoiding the fixed 85 ms aperture that used to blur
    // every attack. Power-of-two apertures keep the deterministic analysis
    // bank bounded to 10.7, 21.3, 42.7, or 85.3 ms at 48 kHz. The four-period
    // rule exists to keep adjacent partials' Hann main lobes apart, which is a
    // constraint on a sequential projection and not on the joint solve; onset
    // frames therefore ask for two, which is the shortest aperture the shared
    // 512-sample floor and the Air fit design bank both support.
    const auto desired = static_cast<std::size_t>(std::ceil(
        periods * sampleRate / static_cast<double>(fundamentalHz)));
    std::size_t windowSize = 512;
    while (windowSize < desired && windowSize < spectrumSize)
        windowSize *= 2;
    return std::clamp(windowSize, std::size_t { 512 }, spectrumSize);
}

[[nodiscard]] float spectrumMagnitude(const SpectrumFrame& frame,
                                      float frequencyHz,
                                      double sampleRate) noexcept
{
    if (!(frequencyHz > 0.0f)
        || frequencyHz >= 0.5f * static_cast<float>(sampleRate))
        return 0.0f;

    const float bin = frequencyHz * static_cast<float>(frame.bins.size())
        / static_cast<float>(sampleRate);
    const auto lower = static_cast<std::size_t>(bin);
    if (lower + 1 >= frame.bins.size() / 2)
        return 0.0f;
    const float fraction = bin - static_cast<float>(lower);
    const float first = std::abs(frame.bins[lower]);
    const float second = std::abs(frame.bins[lower + 1]);
    return 2.0f * (first + fraction * (second - first))
        / std::max(frame.normalisation, 1.0f);
}

[[nodiscard]] float spectrumPhase(const SpectrumFrame& frame,
                                  float frequencyHz,
                                  double sampleRate) noexcept
{
    const float bin = frequencyHz * static_cast<float>(frame.bins.size())
        / static_cast<float>(sampleRate);
    const auto nearest = static_cast<std::size_t>(std::max(0.0f, std::round(bin)));
    if (nearest >= frame.bins.size() / 2)
        return 0.0f;
    float phase = std::arg(frame.bins[nearest]) / twoPi;
    phase -= std::floor(phase);
    return phase;
}

[[nodiscard]] SpectrumFrame makeSpectrumFrame(const std::vector<float>& sample,
                                               double sampleRate,
                                               float normalisedTime)
{
    SpectrumFrame frame;
    frame.normalisedTime = std::clamp(normalisedTime, 0.0f, 1.0f);
    frame.analysisWindowSize = spectrumSize;
    frame.timeSeconds = frame.normalisedTime
        * static_cast<float>(sample.size() - 1) / static_cast<float>(sampleRate);
    frame.bins.assign(spectrumSize, Complex {});

    const auto centre = static_cast<std::ptrdiff_t>(std::llround(
        frame.normalisedTime * static_cast<float>(sample.size() - 1)));
    const auto start = centre - static_cast<std::ptrdiff_t>(spectrumSize / 2);
    float windowSum = 0.0f;
    float windowSquareSum = 0.0f;
    for (std::size_t index = 0; index < spectrumSize; ++index)
    {
        const auto source = start + static_cast<std::ptrdiff_t>(index);
        if (source < 0 || source >= static_cast<std::ptrdiff_t>(sample.size()))
            continue;
        const float window = spectrumHannWindow()[index];
        frame.bins[index] = Complex(sample[static_cast<std::size_t>(source)] * window,
                                    0.0f);
        windowSum += window;
        windowSquareSum += window * window;
    }
    frame.normalisation = std::max(windowSum, 1.0f);
    frame.noisePowerCorrection = windowSum * windowSum
        / (static_cast<float>(spectrumSize)
           * std::max(windowSquareSum, 1.0e-8f));
    fft(frame.bins);
    return frame;
}

[[nodiscard]] HarmonicAnalysisFrame analyseHarmonicResidual(
    const std::vector<float>& sample, double sampleRate,
    float normalisedTime, float fundamentalHz, float inharmonicity,
    std::size_t windowSize = spectrumSize)
{
    windowSize = std::clamp(windowSize, std::size_t { 512 }, spectrumSize);
    HarmonicAnalysisFrame result;
    auto& residualSpectrum = result.residualSpectrum;
    residualSpectrum.normalisedTime = std::clamp(normalisedTime, 0.0f, 1.0f);
    residualSpectrum.analysisWindowSize = windowSize;
    residualSpectrum.timeSeconds = residualSpectrum.normalisedTime
        * static_cast<float>(sample.size() - 1) / static_cast<float>(sampleRate);
    residualSpectrum.bins.assign(spectrumSize, Complex {});

    const auto centre = static_cast<std::ptrdiff_t>(std::llround(
        residualSpectrum.normalisedTime
        * static_cast<float>(sample.size() - 1)));
    const auto start = centre - static_cast<std::ptrdiff_t>(windowSize / 2);
    std::array<float, spectrumSize> residual {};
    std::array<float, spectrumSize> windows {};
    float windowSum = 0.0f;
    float windowSquareSum = 0.0f;
    // The long-aperture case - every one of a learn() pass's 128 timeline
    // frames, since only the onset region ever asks for a shorter one - has
    // windowSize == spectrumSize, which is exactly the window
    // spectrumHannWindow() already caches for makeSpectrumFrame(). Reading it
    // here instead of re-deriving the same 4096 std::cos() values through
    // hannWindow() removes that repetition the same way spectrumHannWindow()
    // itself removed it from makeSpectrumFrame(); the shorter transient
    // apertures still build their own window, since a per-size cache is not
    // worth the complexity for the handful of onset frames that use it.
    const bool useCachedWindow = windowSize == spectrumSize;
    const auto& cachedWindow = spectrumHannWindow();
    for (std::size_t index = 0; index < windowSize; ++index)
    {
        const auto source = start + static_cast<std::ptrdiff_t>(index);
        if (source < 0 || source >= static_cast<std::ptrdiff_t>(sample.size()))
            continue;
        windows[index] = useCachedWindow
            ? cachedWindow[index] : hannWindow(index, windowSize);
        residual[index] = sample[static_cast<std::size_t>(source)];
        windowSum += windows[index];
        windowSquareSum += windows[index] * windows[index];
    }
    residualSpectrum.normalisation = std::max(windowSum, 1.0f);
    residualSpectrum.noisePowerCorrection = windowSum * windowSum
        / (static_cast<float>(spectrumSize)
           * std::max(windowSquareSum, 1.0e-8f));

    // Projecting at the stretched partial position is what lets a stiff source
    // keep clean amplitudes; a harmonic source is unaffected because the ratio
    // is then exactly the harmonic number.
    std::array<double, NeuralModel::harmonicCount> partialAngles {};
    std::size_t partialCount = 0;
    for (std::size_t harmonic = 0;
         harmonic < NeuralModel::harmonicCount; ++harmonic)
    {
        const double frequency = static_cast<double>(fundamentalHz)
            * static_cast<double>(stretchedHarmonicRatio(
                static_cast<float>(harmonic + 1), inharmonicity));
        if (!(frequency > 0.0) || frequency >= 0.48 * sampleRate)
            break;
        partialAngles[harmonic] = twoPi * frequency / sampleRate;
        ++partialCount;
    }

    // Weighted least-squares projections remove each partial from the actual
    // waveform frame. The 2x2 solve handles the finite/windowed cosine/sine
    // basis without assuming perfect orthogonality, but a single ordered pass
    // is only correct when the partials are orthogonal to each other as well.
    // They are not: the aperture is a few fundamental periods, so adjacent
    // partials' Hann main lobes touch and every subtraction leaks into its
    // neighbours - worst exactly where it is most audible, on a quiet partial
    // beside a loud one. Repeating the pass, adding each partial's current
    // estimate back before re-solving it against the others' residual, is
    // Gauss-Seidel on the joint normal equations. It converges to the joint
    // least-squares solution without forming or factorising the full
    // 2N x 2N system, and the first sweep is bit-identical to the previous
    // sequential behaviour because every estimate it adds back is still zero.
    //
    // Refinement is only bought where it is needed. A Hann main lobe is four
    // bins wide, so an aperture of P fundamental periods separates adjacent
    // partials by P/2 main-lobe half-widths: past about eight periods the bases
    // are orthogonal to working precision and every extra sweep reproduces the
    // first one at full cost. The parallel 4096-sample modal aperture is
    // eighteen periods at a mid pitch, and it is also the most expensive frame
    // in the pass, so it keeps the single sweep it always had.
    const double windowPeriods = static_cast<double>(windowSize)
        * static_cast<double>(fundamentalHz) / sampleRate;
    const int refinementSweeps = windowPeriods < 8.0 ? 3 : 1;
    std::array<double, NeuralModel::harmonicCount> cosineCoefficients {};
    std::array<double, NeuralModel::harmonicCount> sineCoefficients {};
    // Neither a partial's per-sample rotation nor its window-start phasor
    // depends on the sweep index - both are fixed by partialAngles[harmonic]
    // and start alone - so the refinement loop below used to rebuild the same
    // rotation and initial phasor from scratch on every one of its (up to
    // three) sweeps. Resolving them once per harmonic here and reusing the
    // stored values across every sweep removes that repeated std::cos()/
    // std::sin() pair without changing which values are computed.
    std::array<Complex, NeuralModel::harmonicCount> partialRotations {};
    std::array<Complex, NeuralModel::harmonicCount> partialInitialOscillators {};
    for (std::size_t harmonic = 0; harmonic < partialCount; ++harmonic)
    {
        const double angle = partialAngles[harmonic];
        partialRotations[harmonic] = Complex(static_cast<float>(std::cos(angle)),
                                             static_cast<float>(std::sin(angle)));
        partialInitialOscillators[harmonic] = std::polar(
            1.0f, static_cast<float>(angle * static_cast<double>(start)));
    }
    for (int sweep = 0; sweep < refinementSweeps; ++sweep)
    {
        for (std::size_t harmonic = 0; harmonic < partialCount; ++harmonic)
        {
            const Complex& rotation = partialRotations[harmonic];
            const Complex& initialOscillator = partialInitialOscillators[harmonic];
            Complex oscillator = initialOscillator;
            const double previousCosine = cosineCoefficients[harmonic];
            const double previousSine = sineCoefficients[harmonic];
            double cosineCosine = 0.0;
            double sineSine = 0.0;
            double cosineSine = 0.0;
            double signalCosine = 0.0;
            double signalSine = 0.0;
            for (std::size_t index = 0; index < windowSize; ++index)
            {
                const float weight = windows[index];
                if (weight > 0.0f)
                {
                    const double cosine = oscillator.real();
                    const double sine = oscillator.imag();
                    // Add this partial's own current estimate back without
                    // writing it: the residual then holds every other partial's
                    // leftovers, which is what this partial must be solved
                    // against. Only the change is written back below, so the
                    // stored residual never accumulates an add/subtract pair.
                    const double value = static_cast<double>(residual[index])
                        + previousCosine * cosine + previousSine * sine;
                    cosineCosine += weight * cosine * cosine;
                    sineSine += weight * sine * sine;
                    cosineSine += weight * cosine * sine;
                    signalCosine += weight * value * cosine;
                    signalSine += weight * value * sine;
                }
                oscillator *= rotation;
            }

            const double determinant = cosineCosine * sineSine
                - cosineSine * cosineSine;
            if (determinant <= 1.0e-9)
                continue;
            double cosineCoefficient = (signalCosine * sineSine
                - signalSine * cosineSine) / determinant;
            double sineCoefficient = (signalSine * cosineCosine
                - signalCosine * cosineSine) / determinant;
            const double magnitude = std::hypot(cosineCoefficient,
                                                sineCoefficient);
            if (!std::isfinite(magnitude))
                continue;
            if (magnitude > 2.0)
            {
                const double scale = 2.0 / magnitude;
                cosineCoefficient *= scale;
                sineCoefficient *= scale;
            }

            cosineCoefficients[harmonic] = cosineCoefficient;
            sineCoefficients[harmonic] = sineCoefficient;
            const double deltaCosine = cosineCoefficient - previousCosine;
            const double deltaSine = sineCoefficient - previousSine;
            oscillator = initialOscillator;
            for (std::size_t index = 0; index < windowSize; ++index)
            {
                if (windows[index] > 0.0f)
                {
                    residual[index] -= static_cast<float>(
                        deltaCosine * oscillator.real()
                        + deltaSine * oscillator.imag());
                }
                oscillator *= rotation;
            }
        }
    }

    for (std::size_t harmonic = 0; harmonic < partialCount; ++harmonic)
    {
        const double cosineCoefficient = cosineCoefficients[harmonic];
        const double sineCoefficient = sineCoefficients[harmonic];
        result.amplitudes[harmonic] = static_cast<float>(std::hypot(
            cosineCoefficient, sineCoefficient));
        float sinePhase = static_cast<float>(std::atan2(
            cosineCoefficient, sineCoefficient) / twoPi);
        sinePhase -= std::floor(sinePhase);
        result.sinePhases[harmonic] = sinePhase;
    }

    for (std::size_t index = 0; index < windowSize; ++index)
        residualSpectrum.bins[index] = Complex(
            residual[index] * windows[index], 0.0f);
    fft(residualSpectrum.bins);
    return result;
}

[[nodiscard]] double sinc(double value) noexcept
{
    if (std::abs(value) < 1.0e-12)
        return 1.0;
    const double angle = static_cast<double>(pi) * value;
    return std::sin(angle) / angle;
}

constexpr int resamplerHalfTaps = 24;
constexpr int resamplerTaps = 2 * resamplerHalfTaps;
// The kernel only depends on the fractional distance between the requested
// output position and the sample grid, so it is evaluated once into a
// polyphase table instead of calling sin() and cos() 48 times for every one
// of the hundreds of thousands of output samples.
constexpr int resamplerPhases = 1024;

struct ResamplerKernel
{
    // Row `phase` holds the 48 taps for a fractional offset of
    // phase / resamplerPhases, followed by that row's weight sum.
    std::vector<double> weights;
    std::vector<double> weightSums;

    explicit ResamplerKernel(double cutoff)
        : weights(static_cast<std::size_t>(resamplerPhases) * resamplerTaps,
                  0.0),
          weightSums(static_cast<std::size_t>(resamplerPhases), 0.0)
    {
        for (int phase = 0; phase < resamplerPhases; ++phase)
        {
            const double fraction = static_cast<double>(phase)
                / static_cast<double>(resamplerPhases);
            double sum = 0.0;
            auto* row = weights.data()
                + static_cast<std::size_t>(phase) * resamplerTaps;
            for (int tap = 0; tap < resamplerTaps; ++tap)
            {
                const double distance = fraction
                    + static_cast<double>(resamplerHalfTaps - 1 - tap);
                const double window = 0.5 + 0.5 * std::cos(
                    static_cast<double>(pi) * distance / resamplerHalfTaps);
                row[tap] = cutoff * sinc(cutoff * distance) * window;
                sum += row[tap];
            }
            weightSums[static_cast<std::size_t>(phase)] = sum;
        }
    }
};

[[nodiscard]] std::vector<float> resampleWindowedSinc(
    const std::vector<float>& source, double sourceRate, double destinationRate,
    const SampleLearner::CancelPredicate& cancel = {})
{
    if (source.empty() || !std::isfinite(sourceRate)
        || !std::isfinite(destinationRate)
        || sourceRate <= 0.0 || destinationRate <= 0.0)
        return {};
    if (std::abs(sourceRate - destinationRate) < 1.0e-6)
        return source;

    const double sourcePerOutput = sourceRate / destinationRate;
    const double cutoff = 0.94 * std::min(1.0, destinationRate / sourceRate);
    const auto destinationSize = static_cast<std::size_t>(
        std::max(1.0, std::round(static_cast<double>(source.size())
                                * destinationRate / sourceRate)));
    const ResamplerKernel kernel(cutoff);
    const auto sourceSize = static_cast<std::ptrdiff_t>(source.size());
    std::vector<float> destination(destinationSize, 0.0f);
    for (std::size_t output = 0; output < destinationSize; ++output)
    {
        if ((output & 1023u) == 0u && cancellationRequested(cancel))
            return {};
        const double centre = static_cast<double>(output) * sourcePerOutput;
        const double centreFloor = std::floor(centre);
        const auto first = static_cast<std::ptrdiff_t>(centreFloor)
                         - resamplerHalfTaps + 1;
        auto phase = static_cast<int>((centre - centreFloor)
            * static_cast<double>(resamplerPhases));
        phase = std::clamp(phase, 0, resamplerPhases - 1);
        const auto* row = kernel.weights.data()
            + static_cast<std::size_t>(phase) * resamplerTaps;

        double weighted = 0.0;
        if (first >= 0 && first + resamplerTaps <= sourceSize)
        {
            const auto* input = source.data() + first;
            for (int tap = 0; tap < resamplerTaps; ++tap)
                weighted += static_cast<double>(input[tap]) * row[tap];
            const double weightSum
                = kernel.weightSums[static_cast<std::size_t>(phase)];
            destination[output] = std::abs(weightSum) > 1.0e-12
                ? static_cast<float>(weighted / weightSum) : 0.0f;
            continue;
        }

        double weightSum = 0.0;
        for (int tap = 0; tap < resamplerTaps; ++tap)
        {
            const auto input = first + tap;
            if (input < 0 || input >= sourceSize)
                continue;
            weighted += static_cast<double>(source[static_cast<std::size_t>(input)])
                      * row[tap];
            weightSum += row[tap];
        }
        destination[output] = std::abs(weightSum) > 1.0e-12
            ? static_cast<float>(weighted / weightSum) : 0.0f;
    }
    return destination;
}

[[nodiscard]] std::vector<float> downsampleForPitch(
    const std::vector<float>& source, double sourceRate, double& destinationRate,
    const SampleLearner::CancelPredicate& cancel)
{
    destinationRate = std::min(sourceRate, 12000.0);
    return resampleWindowedSinc(source, sourceRate, destinationRate, cancel);
}

[[nodiscard]] PitchEstimate estimateYinWindow(const std::vector<float>& sample,
                                               double sampleRate,
                                               std::size_t start,
                                               std::size_t length,
                                               const SampleLearner::CancelPredicate& cancel)
{
    PitchEstimate result;
    if (length < 384 || start + length > sample.size())
        return result;

    const std::size_t minimumLag = std::max<std::size_t>(2,
        static_cast<std::size_t>(std::floor(sampleRate / 2000.0)));
    const std::size_t maximumLag = std::min<std::size_t>(length / 3,
        static_cast<std::size_t>(std::ceil(sampleRate / 35.0)));
    if (maximumLag <= minimumLag + 2)
        return result;

    const std::size_t comparisonLength = length - maximumLag;
    std::vector<double> difference(maximumLag + 1, 0.0);
    for (std::size_t lag = 1; lag <= maximumLag; ++lag)
    {
        if ((lag & 7u) == 0u && cancellationRequested(cancel))
            return {};
        double value = 0.0;
        for (std::size_t index = 0; index < comparisonLength; ++index)
        {
            const double delta = static_cast<double>(sample[start + index])
                - static_cast<double>(sample[start + index + lag]);
            value += delta * delta;
        }
        difference[lag] = value;
    }

    std::vector<double> cumulative(maximumLag + 1, 1.0);
    double running = 0.0;
    for (std::size_t lag = 1; lag <= maximumLag; ++lag)
    {
        running += difference[lag];
        cumulative[lag] = running > 1.0e-20
            ? difference[lag] * static_cast<double>(lag) / running
            : 1.0;
    }

    const auto firstMinimumBelow = [&cumulative, minimumLag, maximumLag](
        double threshold) noexcept
    {
        for (std::size_t lag = minimumLag; lag < maximumLag; ++lag)
        {
            if (cumulative[lag] >= threshold)
                continue;
            std::size_t selected = lag;
            while (selected + 1 < maximumLag
                   && cumulative[selected + 1] < cumulative[selected])
                ++selected;
            return selected;
        }
        return std::size_t { 0 };
    };

    // A moderately periodic upper partial can cross YIN's conventional loose
    // threshold before the much deeper true-period minimum. Prefer a strong
    // periodic trough across the complete range, then retain the former
    // threshold as a fallback for noisy or breathy material.
    std::size_t selected = firstMinimumBelow(0.10);
    if (selected == 0)
        selected = firstMinimumBelow(0.16);
    if (selected == 0)
    {
        selected = minimumLag;
        for (std::size_t lag = minimumLag + 1; lag <= maximumLag; ++lag)
            if (cumulative[lag] < cumulative[selected])
                selected = lag;
    }

    double refinedLag = static_cast<double>(selected);
    if (selected > minimumLag && selected < maximumLag)
    {
        const double before = cumulative[selected - 1];
        const double centre = cumulative[selected];
        const double after = cumulative[selected + 1];
        const double denominator = before - 2.0 * centre + after;
        if (std::abs(denominator) > 1.0e-12)
            refinedLag += 0.5 * (before - after) / denominator;
    }

    result.frequencyHz = static_cast<float>(sampleRate / refinedLag);
    result.confidence = static_cast<float>(
        std::clamp(1.0 - cumulative[selected], 0.0, 1.0));
    if (result.frequencyHz < 35.0f || result.frequencyHz > 2000.0f)
        return {};
    return result;
}

[[nodiscard]] PitchEstimate estimatePitch(const std::vector<float>& sample,
                                          double sampleRate,
                                          const std::vector<float>& pitchSample,
                                          double pitchRate,
                                          const SampleLearner::CancelPredicate& cancel)
{
    if (pitchSample.empty() || !(pitchRate > 0.0))
        return {};
    std::vector<PitchEstimate> candidates;
    constexpr std::array<std::size_t, 3> resolutions { 2048, 4096, 8192 };
    constexpr std::array<float, 3> centres { 0.22f, 0.50f, 0.76f };

    for (const auto requestedLength : resolutions)
    {
        const std::size_t length = std::min(requestedLength, pitchSample.size());
        if (length < 384)
            continue;
        for (const float centre : centres)
        {
            if (cancellationRequested(cancel))
                return {};
            const auto centreSample = static_cast<std::size_t>(centre
                * static_cast<float>(pitchSample.size() - 1));
            const std::size_t start = centreSample > length / 2
                ? std::min(centreSample - length / 2, pitchSample.size() - length)
                : 0;
            const auto estimate = estimateYinWindow(pitchSample, pitchRate,
                                                    start, length, cancel);
            if (estimate.confidence >= 0.20f)
                candidates.push_back(estimate);
        }
    }

    if (candidates.empty())
        return {};

    const auto strongest = std::max_element(candidates.begin(), candidates.end(),
        [](const PitchEstimate& left, const PitchEstimate& right)
        {
            return left.confidence < right.confidence;
        });
    const float reference = strongest->frequencyHz;
    double weightedLogFrequency = 0.0;
    double weightSum = 0.0;
    double confidenceSum = 0.0;
    for (auto candidate : candidates)
    {
        const float octaveShift = std::round(std::log2(reference / candidate.frequencyHz));
        candidate.frequencyHz *= std::exp2(octaveShift);
        if (candidate.frequencyHz < 35.0f || candidate.frequencyHz > 2000.0f)
            continue;
        const double weight = std::max(0.05f, candidate.confidence);
        weightedLogFrequency += weight * std::log(candidate.frequencyHz);
        weightSum += weight;
        confidenceSum += weight * candidate.confidence;
    }

    PitchEstimate combined;
    combined.frequencyHz = static_cast<float>(std::exp(
        weightedLogFrequency / std::max(weightSum, 1.0e-12)));
    combined.confidence = static_cast<float>(confidenceSum
        / std::max(weightSum, 1.0e-12));

    const auto spectrum = makeSpectrumFrame(sample, sampleRate, 0.42f);
    if (cancellationRequested(cancel))
        return {};
    const auto harmonicSupport = [&spectrum, sampleRate](float candidate)
    {
        if (candidate < 35.0f || candidate > 2000.0f)
            return -1.0f;
        float score = 0.0f;
        for (int harmonic = 1; harmonic <= 16; ++harmonic)
            score += std::sqrt(std::max(0.0f, spectrumMagnitude(
                         spectrum, candidate * static_cast<float>(harmonic),
                         sampleRate)))
                / std::sqrt(static_cast<float>(harmonic));
        return score;
    };

    float bestFrequency = combined.frequencyHz;
    float bestScore = harmonicSupport(bestFrequency);
    for (const float multiplier : { 0.5f, 2.0f })
    {
        const float candidate = combined.frequencyHz * multiplier;
        const float score = harmonicSupport(candidate);
        // Keep the coherent YIN result on near-ties. A hypothesis must carry
        // materially more distributed harmonic support before changing octave.
        if (score > 1.05f * bestScore)
        {
            bestScore = score;
            bestFrequency = candidate;
        }
    }
    combined.frequencyHz = bestFrequency;
    combined.confidence = std::clamp(combined.confidence, 0.0f, 1.0f);
    return combined;
}

struct SpectralPeak
{
    float frequencyHz { 0.0f };
    float magnitude { 0.0f };
};

// Locates the interpolated local maximum nearest a predicted partial. The
// parabolic vertex of three log-magnitude bins resolves the frequency far
// below the bin spacing, which is what makes a small stiffness coefficient
// measurable from a 4096-point aperture at all.
[[nodiscard]] SpectralPeak findSpectralPeak(const SpectrumFrame& frame,
                                            float targetHz, float searchHz,
                                            double sampleRate) noexcept
{
    SpectralPeak result;
    const auto binCount = static_cast<std::ptrdiff_t>(frame.bins.size());
    const float binWidth = static_cast<float>(sampleRate)
        / static_cast<float>(binCount);
    if (!(targetHz > 0.0f) || !(searchHz > 0.0f) || !(binWidth > 0.0f))
        return result;

    const auto firstBin = std::max<std::ptrdiff_t>(2,
        static_cast<std::ptrdiff_t>(std::floor((targetHz - searchHz) / binWidth)));
    const auto lastBin = std::min<std::ptrdiff_t>(binCount / 2 - 2,
        static_cast<std::ptrdiff_t>(std::ceil((targetHz + searchHz) / binWidth)));
    if (lastBin <= firstBin + 1)
        return result;

    auto best = firstBin;
    float bestMagnitude = 0.0f;
    for (auto bin = firstBin; bin <= lastBin; ++bin)
    {
        const float magnitude = std::abs(
            frame.bins[static_cast<std::size_t>(bin)]);
        if (magnitude > bestMagnitude)
        {
            bestMagnitude = magnitude;
            best = bin;
        }
    }
    // An edge maximum means the true partial is probably outside the window.
    if (!(bestMagnitude > 0.0f) || best <= firstBin || best >= lastBin)
        return result;

    const float left = std::log(std::max(std::abs(
        frame.bins[static_cast<std::size_t>(best - 1)]), 1.0e-20f));
    const float centre = std::log(std::max(bestMagnitude, 1.0e-20f));
    const float right = std::log(std::max(std::abs(
        frame.bins[static_cast<std::size_t>(best + 1)]), 1.0e-20f));
    const float denominator = left - 2.0f * centre + right;
    float offset = 0.0f;
    if (denominator < -1.0e-12f)
        offset = std::clamp(0.5f * (left - right) / denominator, -0.5f, 0.5f);
    result.frequencyHz = (static_cast<float>(best) + offset) * binWidth;
    result.magnitude = 2.0f * bestMagnitude
        / std::max(frame.normalisation, 1.0f);
    return result;
}

struct StretchEstimate
{
    float inharmonicity { 0.0f };
    float refinedRootHz { 0.0f };
    int supportingPartials { 0 };
    float explainedFraction { 0.0f };
};

// Fits the stiff-string law f(n) = n f0 sqrt(1 + B n^2) to the measured
// partial positions. Wound strings, tines, and struck bars all sharpen their
// upper partials this way; an ideal harmonic source returns exactly zero and
// therefore keeps the analysis and the renderer bit-identical to before.
[[nodiscard]] StretchEstimate estimateInharmonicity(
    const std::vector<float>& sample, double sampleRate, float rootFrequencyHz,
    const SampleLearner::CancelPredicate& cancel)
{
    StretchEstimate result;
    result.refinedRootHz = rootFrequencyHz;
    if (!(rootFrequencyHz > 0.0f) || sample.size() < spectrumSize / 2)
        return result;

    constexpr std::array<float, 3> probeTimes { 0.22f, 0.40f, 0.58f };
    std::vector<SpectrumFrame> probes;
    probes.reserve(probeTimes.size());
    for (const float time : probeTimes)
    {
        if (cancellationRequested(cancel))
            return result;
        probes.push_back(makeSpectrumFrame(sample, sampleRate, time));
    }

    float referenceMagnitude = 0.0f;
    for (const auto& probe : probes)
    {
        for (int harmonic = 1; harmonic <= 6; ++harmonic)
        {
            referenceMagnitude = std::max(referenceMagnitude,
                spectrumMagnitude(probe,
                    rootFrequencyHz * static_cast<float>(harmonic), sampleRate));
        }
    }
    if (!(referenceMagnitude > 1.0e-6f))
        return result;

    // Widening the partial set as the estimate converges prevents an early
    // mis-prediction from locking a high partial onto its neighbour.
    constexpr std::array<int, 4> passPartialLimits { 8, 16, 24, 32 };
    const float searchHz = 0.40f * rootFrequencyHz;
    float inharmonicity = 0.0f;
    float root = rootFrequencyHz;
    float explained = 0.0f;
    int supporting = 0;
    int runLength = 0;
    for (const int partialLimit : passPartialLimits)
    {
        runLength = 0;
        if (cancellationRequested(cancel))
            return result;

        // Squaring the stiff-string law linearises it exactly:
        //   (f(n) / n)^2 = f0^2 + (f0^2 B) n^2
        // so one weighted straight-line fit recovers the fundamental and the
        // coefficient together. Solving for B alone against an assumed
        // fundamental biases both, because a slightly sharp root absorbs part
        // of the stretch it is supposed to measure.
        double weightSum = 0.0;
        double sumX = 0.0;
        double sumY = 0.0;
        double sumXX = 0.0;
        double sumXY = 0.0;
        struct Observation
        {
            double x { 0.0 };
            double y { 0.0 };
            double weight { 0.0 };
        };
        std::vector<Observation> observations;
        observations.reserve(static_cast<std::size_t>(partialLimit)
                             * probes.size());
        for (int partial = 1; partial <= partialLimit; ++partial)
        {
            const auto number = static_cast<float>(partial);
            const float predicted = root
                * stretchedHarmonicRatio(number, inharmonicity);
            if (predicted >= 0.45f * static_cast<float>(sampleRate))
                break;

            bool found = false;
            for (const auto& probe : probes)
            {
                const auto peak = findSpectralPeak(
                    probe, predicted, searchHz, sampleRate);
                if (!(peak.frequencyHz > 0.0f)
                    || peak.magnitude < 0.004f * referenceMagnitude)
                    continue;

                const double partialRoot = static_cast<double>(peak.frequencyHz)
                    / number;
                const double x = static_cast<double>(number) * number;
                const double y = partialRoot * partialRoot;
                const double weight = peak.magnitude;
                observations.push_back({ x, y, weight });
                weightSum += weight;
                sumX += weight * x;
                sumY += weight * y;
                sumXX += weight * x * x;
                sumXY += weight * x * y;
                found = true;
            }
            // Stop at the first missing partial. A stiff string has a dense,
            // uninterrupted series; a sparse spectrum with a few strong
            // resonances does not, and following it past a gap is exactly how
            // a modal body would be mistaken for a stretched string.
            if (!found)
                break;
            runLength = partial;
        }

        supporting = static_cast<int>(observations.size());
        const double determinant = weightSum * sumXX - sumX * sumX;
        if (supporting < 6 || !(weightSum > 0.0)
            || !(std::abs(determinant) > 1.0e-9))
            return result;

        const double slope = (weightSum * sumXY - sumX * sumY) / determinant;
        const double intercept = (sumY - slope * sumX) / weightSum;
        if (!(intercept > 0.0) || !std::isfinite(slope))
            return result;

        double residualSquares = 0.0;
        double totalSquares = 0.0;
        const double meanY = sumY / weightSum;
        for (const auto& observation : observations)
        {
            const double modelled = intercept + slope * observation.x;
            const double residual = observation.y - modelled;
            const double centred = observation.y - meanY;
            residualSquares += observation.weight * residual * residual;
            totalSquares += observation.weight * centred * centred;
        }
        explained = totalSquares > 1.0e-12
            ? static_cast<float>(std::clamp(
                  1.0 - residualSquares / totalSquares, 0.0, 1.0))
            : 0.0f;

        inharmonicity = std::clamp(static_cast<float>(slope / intercept),
                                   0.0f, NeuralModel::maximumInharmonicity);
        // The regression's own fundamental replaces the period-based one. A
        // stiff string always fools a period detector slightly sharp; the
        // correction stays bounded so an ordinary source cannot drift.
        const auto refined = static_cast<float>(std::sqrt(intercept));
        root = std::clamp(refined, 0.94f * rootFrequencyHz,
                          1.06f * rootFrequencyHz);
    }

    result.supportingPartials = supporting;
    result.explainedFraction = explained;

    // Below this the stretch is inaudible and within measurement noise, so it
    // is reported as an exactly harmonic series instead. A short partial run
    // cannot distinguish stiffness from a handful of modal resonances, so it
    // is reported as harmonic too.
    constexpr float minimumUsefulInharmonicity = 1.5e-5f;
    constexpr int minimumPartialRun = 10;
    if (inharmonicity < minimumUsefulInharmonicity || explained < 0.55f
        || runLength < minimumPartialRun)
    {
        result.inharmonicity = 0.0f;
        result.refinedRootHz = rootFrequencyHz;
        return result;
    }

    result.inharmonicity = inharmonicity;
    result.refinedRootHz = root;
    return result;
}

[[nodiscard]] std::vector<float> estimatePitchContour(
    const std::vector<float>& pitchSample, double pitchRate,
    const std::vector<SpectrumFrame>& spectra, float rootFrequencyHz,
    const SampleLearner::CancelPredicate& cancel)
{
    std::vector<float> semitoneOffsets(spectra.size(), 0.0f);
    if (spectra.empty() || !(rootFrequencyHz > 0.0f)
        || pitchSample.empty() || !(pitchRate > 0.0))
        return semitoneOffsets;

    const std::size_t windowLength = std::min<std::size_t>(4096,
                                                           pitchSample.size());
    const double expectedLag = pitchRate / rootFrequencyHz;
    constexpr float maximumContourSemitones = 4.0f;
    const auto minimumLag = static_cast<std::size_t>(std::max(2.0,
        std::floor(expectedLag / std::exp2(maximumContourSemitones / 12.0f))));
    const auto maximumLag = static_cast<std::size_t>(std::ceil(
        expectedLag * std::exp2(maximumContourSemitones / 12.0f)));
    if (maximumLag <= minimumLag + 1 || windowLength <= maximumLag + 192)
        return semitoneOffsets;

    std::vector<float> correlations(maximumLag - minimumLag + 1, -1.0f);
    std::vector<float> rawOffsets(spectra.size(), 0.0f);
    std::vector<bool> measured(spectra.size(), false);
    for (std::size_t frame = 0; frame < spectra.size(); ++frame)
    {
        if (cancellationRequested(cancel))
            return semitoneOffsets;
        const auto centre = static_cast<std::size_t>(std::llround(
            spectra[frame].normalisedTime
            * static_cast<float>(pitchSample.size() - 1)));
        const std::size_t start = centre > windowLength / 2
            ? std::min(centre - windowLength / 2,
                       pitchSample.size() - windowLength)
            : 0;
        const std::size_t comparisonLength = windowLength - maximumLag;

        float bestCorrelation = -1.0f;
        std::size_t bestLag = minimumLag;
        for (std::size_t lag = minimumLag; lag <= maximumLag; ++lag)
        {
            if ((lag & 7u) == 0u && cancellationRequested(cancel))
                return semitoneOffsets;
            double cross = 0.0;
            double firstEnergy = 0.0;
            double secondEnergy = 0.0;
            for (std::size_t index = 0; index < comparisonLength; ++index)
            {
                const double first = pitchSample[start + index];
                const double second = pitchSample[start + index + lag];
                cross += first * second;
                firstEnergy += first * first;
                secondEnergy += second * second;
            }
            const double denominator = std::sqrt(firstEnergy * secondEnergy);
            const float correlation = denominator > 1.0e-18
                ? static_cast<float>(cross / denominator) : -1.0f;
            correlations[lag - minimumLag] = correlation;
            if (correlation > bestCorrelation)
            {
                bestCorrelation = correlation;
                bestLag = lag;
            }
        }

        if (bestCorrelation < 0.25f)
            continue;

        double refinedLag = static_cast<double>(bestLag);
        if (bestLag > minimumLag && bestLag < maximumLag)
        {
            const float before = correlations[bestLag - minimumLag - 1];
            const float centreValue = correlations[bestLag - minimumLag];
            const float after = correlations[bestLag - minimumLag + 1];
            const float denominator = before - 2.0f * centreValue + after;
            if (std::abs(denominator) > 1.0e-7f)
                refinedLag += std::clamp(
                    0.5 * static_cast<double>(before - after) / denominator,
                    -1.0, 1.0);
        }

        const float frequency = static_cast<float>(pitchRate / refinedLag);
        rawOffsets[frame] = std::clamp(
            12.0f * std::log2(frequency / rootFrequencyHz),
            -maximumContourSemitones, maximumContourSemitones);
        measured[frame] = true;
    }

    // Frames whose correlation was too weak to trust carry no measurement.
    // Leaving them at zero snaps the contour back to the root pitch and out
    // again, and because the residual trajectory reproduces this signal
    // exactly, that snap is rendered as an audible warble - worst on low notes,
    // where unvoiced frames are most common. Bridge the gaps instead, so an
    // unmeasured stretch glides between the pitches on either side of it.
    if (std::find(measured.begin(), measured.end(), true) != measured.end())
    {
        std::size_t previous = rawOffsets.size();
        for (std::size_t frame = 0; frame < rawOffsets.size(); ++frame)
        {
            if (!measured[frame])
                continue;
            if (previous == rawOffsets.size())
            {
                for (std::size_t gap = 0; gap < frame; ++gap)
                    rawOffsets[gap] = rawOffsets[frame];
            }
            else if (frame > previous + 1)
            {
                const float span = static_cast<float>(frame - previous);
                for (std::size_t gap = previous + 1; gap < frame; ++gap)
                {
                    const float position = static_cast<float>(gap - previous)
                        / span;
                    rawOffsets[gap] = rawOffsets[previous]
                        + position * (rawOffsets[frame] - rawOffsets[previous]);
                }
            }
            previous = frame;
        }
        for (std::size_t gap = previous + 1; gap < rawOffsets.size(); ++gap)
            rawOffsets[gap] = rawOffsets[previous];
    }

    // A three-frame median rejects isolated lag choices while retaining the
    // source's slower vibrato, bends, and glide trajectory.
    for (std::size_t frame = 0; frame < rawOffsets.size(); ++frame)
    {
        const std::size_t before = frame > 0 ? frame - 1 : frame;
        const std::size_t after = std::min(frame + 1, rawOffsets.size() - 1);
        std::array<float, 3> neighbourhood {
            rawOffsets[before], rawOffsets[frame], rawOffsets[after]
        };
        std::sort(neighbourhood.begin(), neighbourhood.end());
        semitoneOffsets[frame] = neighbourhood[1];
    }
    return semitoneOffsets;
}

[[nodiscard]] bool conditionSample(const std::vector<float>& input,
                                   double sampleRate,
                                   std::vector<float>& output,
                                   float& sourcePeak,
                                   float& sourceRms,
                                   std::string& error,
                                   const SampleLearner::CancelPredicate& cancel)
{
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 768000.0)
    {
        error = "Sample rate must be between 8 kHz and 768 kHz";
        return false;
    }
    if (input.size() < 256)
    {
        error = "The sample is too short to learn";
        return false;
    }

    const std::size_t maximumSamples = static_cast<std::size_t>(
        maximumSampleSeconds * sampleRate);
    const std::size_t usedSamples = std::min(input.size(), maximumSamples);
    double mean = 0.0;
    std::size_t finiteCount = 0;
    for (std::size_t index = 0; index < usedSamples; ++index)
    {
        if ((index & 65535u) == 0u && cancellationRequested(cancel))
            return false;
        if (std::isfinite(input[index]))
        {
            mean += input[index];
            ++finiteCount;
        }
    }
    if (finiteCount < 256)
    {
        error = "The sample does not contain enough finite audio";
        return false;
    }
    mean /= static_cast<double>(finiteCount);

    std::vector<float> centred(usedSamples, 0.0f);
    sourcePeak = 0.0f;
    double squareSum = 0.0;
    for (std::size_t index = 0; index < usedSamples; ++index)
    {
        if ((index & 65535u) == 0u && cancellationRequested(cancel))
            return false;
        const float value = std::isfinite(input[index])
            ? input[index] - static_cast<float>(mean) : 0.0f;
        centred[index] = value;
        sourcePeak = std::max(sourcePeak, std::abs(value));
        squareSum += static_cast<double>(value) * static_cast<double>(value);
    }
    sourceRms = static_cast<float>(std::sqrt(squareSum
        / static_cast<double>(usedSamples)));
    if (sourcePeak < 1.0e-6f || sourceRms < 1.0e-7f)
    {
        error = "The sample is silent or too quiet to analyse";
        return false;
    }

    const float trimThreshold = sourcePeak * 0.0015f;
    std::size_t first = 0;
    while (first + 1 < centred.size()
           && std::abs(centred[first]) < trimThreshold)
    {
        if ((first & 65535u) == 0u && cancellationRequested(cancel))
            return false;
        ++first;
    }
    std::size_t last = centred.size();
    while (last > first + 1 && std::abs(centred[last - 1]) < trimThreshold)
    {
        if ((last & 65535u) == 0u && cancellationRequested(cancel))
            return false;
        --last;
    }
    const std::size_t padding = static_cast<std::size_t>(0.008 * sampleRate);
    first = first > padding ? first - padding : 0;
    last = std::min(centred.size(), last + padding);
    if (last - first < 256)
    {
        error = "The non-silent part of the sample is too short";
        return false;
    }

    const float gain = 0.90f / sourcePeak;
    output.resize(last - first);
    for (std::size_t index = 0; index < output.size(); ++index)
    {
        if ((index & 65535u) == 0u && cancellationRequested(cancel))
            return false;
        output[index] = std::clamp(centred[first + index] * gain, -1.0f, 1.0f);
    }
    // Metadata describes the conditioned signal that was actually analysed.
    // Keeping it in the same bounded domain also guarantees that every model
    // produced by learn() satisfies the serializer's validation contract.
    sourceRms = std::clamp(sourceRms * gain, 0.0f, 0.90f);
    sourcePeak = 0.90f;
    return true;
}

void makePreview(const std::vector<float>& sample,
                 std::array<float, NeuralModel::previewSize>& preview)
{
    for (std::size_t point = 0; point < preview.size(); ++point)
    {
        const std::size_t begin = point * sample.size() / preview.size();
        const std::size_t end = std::max(begin + 1,
            (point + 1) * sample.size() / preview.size());
        float selected = 0.0f;
        for (std::size_t index = begin; index < std::min(end, sample.size()); ++index)
            if (std::abs(sample[index]) > std::abs(selected))
                selected = sample[index];
        preview[point] = selected;
    }
}

[[nodiscard]] BoneSelection findPersistentBoneModes(
    const std::vector<SpectrumFrame>& residualSpectra,
    float rootFrequencyHz, float inharmonicity, double sampleRate)
{
    struct Peak
    {
        float magnitude { 0.0f };
        float ratio { 0.0f };
        float score { 0.0f };
        float reliability { 0.0f };
    };
    std::vector<Peak> peaks;
    if (residualSpectra.empty())
        return {};

    const auto& reference = residualSpectra.front();
    const float binWidth = static_cast<float>(sampleRate)
        / static_cast<float>(reference.bins.size());
    const std::size_t firstBin = static_cast<std::size_t>(
        std::ceil(1.15f * rootFrequencyHz / binWidth));
    const std::size_t lastBin = std::min(reference.bins.size() / 2 - 2,
        static_cast<std::size_t>(std::floor(
            std::min(0.45f * static_cast<float>(sampleRate),
                     24.0f * rootFrequencyHz) / binWidth)));

    // The stretched-partial series below depends only on inharmonicity, which
    // this function receives as a single fixed value, never on the bin or
    // frame being scanned. Building it once here replaces a from-scratch
    // rebuild on every one of the up to three earlyFrameIndices * (lastBin -
    // firstBin) candidate bins with one table lookup per harmonic tried.
    std::array<float, NeuralModel::harmonicCount> stretchedPartialRatios {};
    if (inharmonicity > 0.0f)
    {
        for (std::size_t harmonic = 0;
             harmonic < NeuralModel::harmonicCount; ++harmonic)
            stretchedPartialRatios[harmonic] = stretchedHarmonicRatio(
                static_cast<float>(harmonic + 1), inharmonicity);
    }

    constexpr std::array<std::size_t, 3> earlyFrameIndices { 2, 8, 18 };
    for (const auto requestedFrame : earlyFrameIndices)
    {
        const auto& spectrum = residualSpectra[std::min(
            requestedFrame, residualSpectra.size() - 1)];
        for (std::size_t bin = std::max<std::size_t>(2, firstBin);
             bin <= lastBin; ++bin)
        {
            const float magnitude = spectrumMagnitude(
                spectrum, static_cast<float>(bin) * binWidth, sampleRate);
            if (magnitude <= spectrumMagnitude(
                    spectrum, static_cast<float>(bin - 1) * binWidth, sampleRate)
                || magnitude < spectrumMagnitude(
                    spectrum, static_cast<float>(bin + 1) * binWidth, sampleRate))
                continue;
            const float ratio = static_cast<float>(bin) * binWidth
                / rootFrequencyHz;
            // Reject anything sitting on a Core partial. With a stretched
            // series those no longer land on integer ratios, so the guard has
            // to measure the distance to the nearest stiff-string partial.
            float partialDistance = std::abs(ratio - std::round(ratio));
            if (inharmonicity > 0.0f)
            {
                partialDistance = std::numeric_limits<float>::max();
                for (std::size_t harmonic = 0;
                     harmonic < NeuralModel::harmonicCount; ++harmonic)
                {
                    const float partialRatio = stretchedPartialRatios[harmonic];
                    partialDistance = std::min(partialDistance,
                                               std::abs(ratio - partialRatio));
                    if (partialRatio > ratio)
                        break;
                }
            }
            if (partialDistance < 0.13f)
                continue;

            const auto existing = std::find_if(peaks.begin(), peaks.end(),
                [ratio](const Peak& peak)
                {
                    return std::abs(peak.ratio - ratio) < 0.12f;
                });
            if (existing == peaks.end())
                peaks.push_back({ magnitude, ratio, 0.0f, 0.0f });
            else if (magnitude > existing->magnitude)
            {
                existing->magnitude = magnitude;
                existing->ratio = ratio;
            }
        }
    }

    for (auto& peak : peaks)
    {
        const float frequency = peak.ratio * rootFrequencyHz;
        double totalWeight = 0.0;
        double supportWeight = 0.0;
        double prominenceSum = 0.0;
        for (std::size_t frameIndex = 0;
             frameIndex < residualSpectra.size(); ++frameIndex)
        {
            const auto& spectrum = residualSpectra[frameIndex];
            if (spectrum.normalisedTime > 0.72f)
                break;
            const float centre = std::max({
                spectrumMagnitude(spectrum, frequency - binWidth, sampleRate),
                spectrumMagnitude(spectrum, frequency, sampleRate),
                spectrumMagnitude(spectrum, frequency + binWidth, sampleRate)
            });
            const float shoulder = 0.25f * (
                spectrumMagnitude(spectrum, frequency - 5.0f * binWidth, sampleRate)
                + spectrumMagnitude(spectrum, frequency - 3.0f * binWidth, sampleRate)
                + spectrumMagnitude(spectrum, frequency + 3.0f * binWidth, sampleRate)
                + spectrumMagnitude(spectrum, frequency + 5.0f * binWidth, sampleRate));
            const float prominence = centre / std::max(shoulder, 1.0e-7f);
            const float relativeLevel = centre / std::max(peak.magnitude, 1.0e-7f);
            const float nextTime = frameIndex + 1 < residualSpectra.size()
                ? residualSpectra[frameIndex + 1].normalisedTime : 1.0f;
            const double weight = std::max(
                static_cast<double>(nextTime - spectrum.normalisedTime), 1.0e-5)
                * std::exp(-1.2 * spectrum.normalisedTime);
            totalWeight += weight;
            if (prominence >= 1.25f && relativeLevel >= 0.012f)
                supportWeight += weight;
            prominenceSum += weight * std::clamp(
                static_cast<double>(prominence - 1.0f), 0.0, 4.0);
        }

        const float support = static_cast<float>(supportWeight
            / std::max(totalWeight, 1.0e-12));
        const float prominence = static_cast<float>(prominenceSum
            / std::max(totalWeight, 1.0e-12));
        if (support < 0.10f || prominence < 0.08f)
            continue;
        peak.reliability = std::clamp(
            0.75f * support + 0.15f * prominence, 0.0f, 1.0f);
        peak.score = peak.magnitude * (0.15f + 0.85f * support)
            * (1.0f + 0.20f * prominence);
    }

    std::erase_if(peaks, [](const Peak& peak) { return peak.score <= 0.0f; });
    std::sort(peaks.begin(), peaks.end(),
        [](const Peak& left, const Peak& right)
        {
            return left.score > right.score;
        });

    constexpr std::array<float, NeuralModel::boneModeCount> defaults {
        1.47f, 2.63f, 3.82f, 5.21f, 7.13f, 9.37f,
        11.42f, 13.68f, 15.91f, 18.24f, 20.63f, 23.11f
    };
    struct Selected
    {
        float ratio { 1.0f };
        float reliability { 0.0f };
    };
    std::array<Selected, NeuralModel::boneModeCount> selected {};
    std::size_t count = 0;
    for (const auto& peak : peaks)
    {
        bool separated = true;
        for (std::size_t existing = 0; existing < count; ++existing)
            separated = separated
                && std::abs(selected[existing].ratio - peak.ratio) > 0.22f;
        if (separated)
            selected[count++] = { peak.ratio, peak.reliability };
        if (count == selected.size())
            break;
    }
    for (; count < selected.size(); ++count)
        selected[count] = { defaults[count], 0.0f };
    std::sort(selected.begin(), selected.end(),
        [](const Selected& left, const Selected& right)
        {
            return left.ratio < right.ratio;
        });

    BoneSelection result;
    for (std::size_t mode = 0; mode < selected.size(); ++mode)
    {
        result.ratios[mode] = selected[mode].ratio;
        result.reliabilities[mode] = selected[mode].reliability;
    }
    return result;
}

[[nodiscard]] bool belongsToActiveBone(float frequencyHz,
                                       float analysisBinWidth,
                                       const BoneSelection& selection,
                                       float rootFrequencyHz) noexcept
{
    for (std::size_t mode = 0; mode < NeuralModel::boneModeCount; ++mode)
    {
        if (selection.reliabilities[mode] <= 0.0f)
            continue;
        const float modeFrequency = rootFrequencyHz * selection.ratios[mode];
        if (std::abs(frequencyHz - modeFrequency)
            < 2.5f * analysisBinWidth)
            return true;
    }
    return false;
}

// The sinusoidal subtraction leaves a narrowband residue at each fitted
// partial - one only 0.5 % off its assumed frequency keeps several dB of its
// energy there. Counting that residue as noise inflates the Air layer and makes
// a tonal source come back breathier than it was, so the neighbourhood
// exclusion already used for Bone modes is applied to the partials that were
// actually subtracted. Removing a bin from the measured power also removes it
// from the filterbank's modelled response, so the fit stays unbiased; it simply
// measures the noise between the partials.
[[nodiscard]] bool belongsToSubtractedHarmonic(float frequencyHz,
                                               float analysisBinWidth,
                                               float rootFrequencyHz,
                                               float inharmonicity) noexcept
{
    if (!(rootFrequencyHz > 0.0f) || !(analysisBinWidth > 0.0f))
        return false;
    // Once the partials are closer together than the analysis can resolve there
    // is no gap left to measure, and excluding would discard the band rather
    // than its contaminated neighbourhoods.
    if (rootFrequencyHz < 3.0f * analysisBinWidth)
        return false;

    // Invert f = f0 * n * sqrt(1 + B n^2) exactly: with u = n^2 and
    // L = f / f0 this is B u^2 + u - L^2 = 0.
    const float ratio = frequencyHz / rootFrequencyHz;
    float index = ratio;
    if (inharmonicity > 0.0f && std::isfinite(inharmonicity))
    {
        const float squared = (std::sqrt(
            1.0f + 4.0f * inharmonicity * ratio * ratio) - 1.0f)
            / (2.0f * inharmonicity);
        index = std::sqrt(std::max(squared, 0.0f));
    }

    const float rounded = std::round(index);
    if (!(rounded >= 1.0f)
        || rounded > static_cast<float>(NeuralModel::harmonicCount))
        return false;
    const float partial = rootFrequencyHz
        * stretchedHarmonicRatio(rounded, inharmonicity);
    return std::abs(frequencyHz - partial) < 1.25f * analysisBinWidth;
}

[[nodiscard]] std::size_t airCellForFrequency(float frequencyHz,
                                              const AirFitDesign& design) noexcept
{
    const float position = std::log(frequencyHz / design.lowerFrequencyHz)
        / design.logFrequencyRangeSpan;
    return std::min<std::size_t>(airFitCellCount - 1,
        static_cast<std::size_t>(std::max(0.0f,
            std::floor(position * static_cast<float>(airFitCellCount)))));
}

[[nodiscard]] AirFitDesign makeAirFitDesign(
    const std::array<float, NeuralModel::airBandCount>& centreFrequencies,
    const std::array<float, NeuralModel::airBandCount>& bandwidthOctaves,
    const BoneSelection& boneSelection, float rootFrequencyHz,
    float inharmonicity,
    double sampleRate, std::size_t analysisWindowSize)
{
    AirFitDesign design;
    design.upperFrequencyHz = std::min(
        airUpperFrequencyHz, 0.42f * static_cast<float>(sampleRate));
    design.logFrequencyRangeSpan = std::log(
        design.upperFrequencyHz / design.lowerFrequencyHz);
    std::array<airfilter::Coefficients, NeuralModel::airBandCount> coefficients {};
    for (std::size_t band = 0; band < coefficients.size(); ++band)
        coefficients[band] = airfilter::makeCoefficients(
            centreFrequencies[band], bandwidthOctaves[band],
            static_cast<float>(sampleRate));

    const float fftBinWidth = static_cast<float>(sampleRate)
        / static_cast<float>(spectrumSize);
    const float analysisBinWidth = static_cast<float>(sampleRate)
        / static_cast<float>(std::max<std::size_t>(
            analysisWindowSize, 1));
    const std::size_t firstBin = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::ceil(
            design.lowerFrequencyHz / fftBinWidth)));
    const std::size_t lastBin = std::min<std::size_t>(spectrumSize / 2 - 1,
        static_cast<std::size_t>(std::floor(
            design.upperFrequencyHz / fftBinWidth)));
    for (std::size_t bin = firstBin; bin <= lastBin; ++bin)
    {
        const float frequency = static_cast<float>(bin) * fftBinWidth;
        if (belongsToActiveBone(frequency, analysisBinWidth, boneSelection,
                                rootFrequencyHz)
            || belongsToSubtractedHarmonic(frequency, analysisBinWidth,
                                           rootFrequencyHz, inharmonicity))
            continue;
        const auto cell = airCellForFrequency(frequency, design);
        ++design.evidence[cell];
        for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
        {
            design.response[cell][band] += 2.0
                * static_cast<double>(airfilter::unitNoisePowerResponse(
                    coefficients[band], frequency,
                    static_cast<float>(sampleRate)))
                / static_cast<double>(spectrumSize);
        }
    }
    return design;
}

[[nodiscard]] AirFitResult fitAirFilterbank(
    const SpectrumFrame& spectrum, const AirFitDesign& design,
    const BoneSelection& boneSelection, float rootFrequencyHz,
    float inharmonicity, double sampleRate,
    const std::array<double, NeuralModel::airBandCount>& initialPowers)
{
    std::array<double, airFitCellCount> target {};
    const float fftBinWidth = static_cast<float>(sampleRate)
        / static_cast<float>(spectrum.bins.size());
    const float analysisBinWidth = static_cast<float>(sampleRate)
        / static_cast<float>(std::max<std::size_t>(
            spectrum.analysisWindowSize, 1));
    const std::size_t firstBin = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::ceil(
            design.lowerFrequencyHz / fftBinWidth)));
    const std::size_t lastBin = std::min(spectrum.bins.size() / 2 - 1,
        static_cast<std::size_t>(std::floor(
            design.upperFrequencyHz / fftBinWidth)));
    for (std::size_t bin = firstBin; bin <= lastBin; ++bin)
    {
        const float frequency = static_cast<float>(bin) * fftBinWidth;
        if (belongsToActiveBone(frequency, analysisBinWidth, boneSelection,
                                rootFrequencyHz)
            || belongsToSubtractedHarmonic(frequency, analysisBinWidth,
                                           rootFrequencyHz, inharmonicity))
            continue;
        const float magnitude = 2.0f * std::abs(spectrum.bins[bin])
            / std::max(spectrum.normalisation, 1.0f);
        target[airCellForFrequency(frequency, design)] += 0.5
            * static_cast<double>(magnitude) * magnitude
            * static_cast<double>(spectrum.noisePowerCorrection);
    }

    const double maximumTarget = *std::max_element(target.begin(), target.end());
    const double tau = std::max(1.0e-12, 0.01 * maximumTarget);
    std::array<double, airFitCellCount> weights {};
    for (std::size_t cell = 0; cell < airFitCellCount; ++cell)
    {
        // 1/target^2 turns this into a relative-error fit, which is what makes
        // quiet cells count at all. On its own it also gives a cell holding one
        // FFT bin the same authority as one holding a hundred and forty, so a
        // single noisy bin near 80 Hz could set a whole Air band. Scaling by the
        // evidence count restores the balance: the variance of a cell's relative
        // error falls with the number of bins averaged into it.
        const double denominator = target[cell] + tau;
        weights[cell] = static_cast<double>(std::max(design.evidence[cell], 0))
            / (denominator * denominator);
    }

    auto powers = initialPowers;
    for (auto& power : powers)
        power = std::clamp(power, 0.0, 4.0);
    std::array<double, airFitCellCount> prediction {};
    for (std::size_t cell = 0; cell < airFitCellCount; ++cell)
        for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
            prediction[cell] += design.response[cell][band] * powers[band];

    double meanDiagonal = 0.0;
    for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
        for (std::size_t cell = 0; cell < airFitCellCount; ++cell)
            meanDiagonal += weights[cell] * design.response[cell][band]
                * design.response[cell][band];
    meanDiagonal /= static_cast<double>(NeuralModel::airBandCount);
    const double smoothness = 0.0025 * meanDiagonal;

    // Deterministic projected coordinate descent solves for non-negative band
    // powers. A small adjacent-band penalty makes the deliberately overlapping
    // eight-filter representation stable without erasing broad spectral shape.
    for (int sweep = 0; sweep < 48; ++sweep)
    {
        for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
        {
            double numerator = 0.0;
            double denominator = 0.0;
            for (std::size_t cell = 0; cell < airFitCellCount; ++cell)
            {
                const double response = design.response[cell][band];
                const double withoutCurrent = prediction[cell]
                    - response * powers[band];
                numerator += weights[cell] * response
                    * (target[cell] - withoutCurrent);
                denominator += weights[cell] * response * response;
            }
            int neighbours = 0;
            if (band > 0)
            {
                numerator += smoothness * powers[band - 1];
                ++neighbours;
            }
            if (band + 1 < NeuralModel::airBandCount)
            {
                numerator += smoothness * powers[band + 1];
                ++neighbours;
            }
            denominator += smoothness * static_cast<double>(neighbours);
            const double next = denominator > 1.0e-20
                ? std::clamp(numerator / denominator, 0.0, 4.0) : 0.0;
            const double delta = next - powers[band];
            powers[band] = next;
            for (std::size_t cell = 0; cell < airFitCellCount; ++cell)
                prediction[cell] += design.response[cell][band] * delta;
        }
    }

    AirFitResult result;
    result.powers = powers;
    for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
        result.amplitudes[band] = static_cast<float>(std::sqrt(powers[band]));
    return result;
}

[[nodiscard]] float estimateDecay(const std::vector<SpectrumFrame>& spectra,
                                  float frequencyHz, double sampleRate,
                                  float durationSeconds)
{
    double sumTime = 0.0;
    double sumLog = 0.0;
    double sumTimeTime = 0.0;
    double sumTimeLog = 0.0;
    int count = 0;
    for (const auto& frame : spectra)
    {
        if (frame.normalisedTime > 0.72f)
            break;
        const double time = frame.timeSeconds;
        const double logAmplitude = std::log(std::max(
            spectrumMagnitude(frame, frequencyHz, sampleRate), amplitudeFloor));
        sumTime += time;
        sumLog += logAmplitude;
        sumTimeTime += time * time;
        sumTimeLog += time * logAmplitude;
        ++count;
    }
    const double denominator = static_cast<double>(count) * sumTimeTime
        - sumTime * sumTime;
    if (count < 3 || std::abs(denominator) < 1.0e-12)
        return std::max(0.05f, 0.7f * durationSeconds);
    const double slope = (static_cast<double>(count) * sumTimeLog
                          - sumTime * sumLog) / denominator;
    if (slope >= -0.01)
        return std::clamp(2.0f * durationSeconds, 0.05f, 60.0f);
    return std::clamp(static_cast<float>(-1.0 / slope), 0.02f, 60.0f);
}

// Orbit revisits this region for as long as a note is held, so it has to be a
// stable part of the sound. The candidate window is expressed in normalised
// TIME rather than frame indices: the analysis grid is deliberately warped so
// that 48 of its 128 frames describe the first 120 ms, and index-based bounds
// therefore put both loop points inside the attack. A decaying source was then
// re-attacked on every orbit.
void chooseLoop(const std::vector<TargetFrame>& targets,
                const std::vector<SpectrumFrame>& spectra,
                float duration, float& loopStart, float& loopEnd)
{
    constexpr float earliestStartTime = 0.22f;
    constexpr float latestStartTime = 0.62f;
    constexpr float minimumSpanTime = 0.16f;

    const auto frameAtOrAfter = [&spectra](float time) noexcept
    {
        for (std::size_t frame = 0; frame < spectra.size(); ++frame)
            if (spectra[frame].normalisedTime >= time)
                return frame;
        return spectra.size() - 1;
    };

    float bestDistance = std::numeric_limits<float>::max();
    std::size_t bestStart = frameAtOrAfter(0.30f);
    std::size_t bestEnd = frameAtOrAfter(0.86f);
    const std::size_t firstCandidate = frameAtOrAfter(earliestStartTime);
    const std::size_t lastCandidate = frameAtOrAfter(latestStartTime);
    for (std::size_t first = firstCandidate;
         first <= lastCandidate && first + 1 < targets.size(); ++first)
    {
        const float startTime = spectra[first].normalisedTime;
        for (std::size_t second = first + 1;
             second + 1 < targets.size(); ++second)
        {
            if (spectra[second].normalisedTime - startTime < minimumSpanTime)
                continue;
            float distance = 0.0f;
            for (std::size_t output = 0; output < NeuralModel::outputSize; ++output)
            {
                const float delta = targets[first][output] - targets[second][output];
                const float firstSlope = targets[first + 1][output]
                    - targets[first][output];
                const float secondSlope = targets[second + 1][output]
                    - targets[second][output];
                const float slopeDelta = firstSlope - secondSlope;
                distance += delta * delta + 0.35f * slopeDelta * slopeDelta;
            }
            distance /= static_cast<float>(NeuralModel::outputSize);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestStart = first;
                bestEnd = second;
            }
        }
    }
    loopStart = std::clamp(spectra[bestStart].timeSeconds, 0.0f, duration - 0.002f);
    loopEnd = std::clamp(spectra[bestEnd].timeSeconds,
                         loopStart + 0.001f, duration);
}

[[nodiscard]] std::array<float, NeuralModel::inputSize> networkInputs(
    float time, float durationSeconds)
{
    return networkInputsAt(std::clamp(time, 0.0f, 1.0f), durationSeconds);
}

using NetworkInputs = std::array<float, NeuralModel::inputSize>;

[[nodiscard]] float networkLoss(const NeuralModel& model,
                                const std::vector<NetworkInputs>& frameInputs,
                                const std::vector<TargetFrame>& normalisedTargets)
{
    const auto& hiddenBiases = NeuralModelTrainingAccess::hiddenBiases(model);
    const auto& inputWeights = NeuralModelTrainingAccess::inputWeights(model);
    const auto& outputBiases = NeuralModelTrainingAccess::outputBiases(model);
    const auto& outputWeights = NeuralModelTrainingAccess::outputWeights(model);
    double loss = 0.0;
    for (std::size_t frame = 0; frame < frameInputs.size(); ++frame)
    {
        const auto& inputs = frameInputs[frame];
        std::array<float, NeuralModel::hiddenSize> hidden {};
        for (std::size_t unit = 0; unit < NeuralModel::hiddenSize; ++unit)
        {
            float value = hiddenBiases[unit];
            for (std::size_t input = 0; input < NeuralModel::inputSize; ++input)
                value += inputWeights[unit * NeuralModel::inputSize + input]
                    * inputs[input];
            hidden[unit] = std::tanh(value);
        }
        for (std::size_t output = 0; output < NeuralModel::outputSize; ++output)
        {
            float prediction = outputBiases[output];
            for (std::size_t unit = 0; unit < NeuralModel::hiddenSize; ++unit)
                prediction += outputWeights[output * NeuralModel::hiddenSize + unit]
                    * hidden[unit];
            const double error = static_cast<double>(prediction)
                - static_cast<double>(normalisedTargets[frame][output]);
            loss += error * error;
        }
    }
    return static_cast<float>(loss
        / static_cast<double>(frameInputs.size() * NeuralModel::outputSize));
}

template <std::size_t Size>
struct AdamState
{
    std::array<float, Size> first {};
    std::array<float, Size> second {};

    void update(std::array<float, Size>& values,
                const std::array<float, Size>& gradients,
                float learningRate, float beta1Correction,
                float beta2Correction) noexcept
    {
        constexpr float beta1 = 0.9f;
        constexpr float beta2 = 0.999f;
        constexpr float epsilon = 1.0e-8f;
        for (std::size_t index = 0; index < Size; ++index)
        {
            first[index] = beta1 * first[index] + (1.0f - beta1) * gradients[index];
            second[index] = beta2 * second[index]
                + (1.0f - beta2) * gradients[index] * gradients[index];
            const float correctedFirst = first[index] / beta1Correction;
            const float correctedSecond = second[index] / beta2Correction;
            values[index] -= learningRate * correctedFirst
                / (std::sqrt(correctedSecond) + epsilon);
        }
    }
};

[[nodiscard]] bool train(NeuralModel& model,
                         const std::vector<SpectrumFrame>& spectra,
                         const std::vector<TargetFrame>& targets,
                         const SampleLearner::ProgressCallback& callback,
                         const SampleLearner::CancelPredicate& cancel,
                         const PitchEstimate& pitch,
                         float& initialLoss, float& finalLoss,
                         int& completedEpochs)
{
    auto& outputMeans = NeuralModelTrainingAccess::outputMeans(model);
    auto& outputScales = NeuralModelTrainingAccess::outputScales(model);
    auto& inputWeights = NeuralModelTrainingAccess::inputWeights(model);
    auto& hiddenBiases = NeuralModelTrainingAccess::hiddenBiases(model);
    auto& outputWeights = NeuralModelTrainingAccess::outputWeights(model);
    auto& outputBiases = NeuralModelTrainingAccess::outputBiases(model);
    std::vector<TargetFrame> normalisedTargets = targets;
    for (std::size_t output = 0; output < NeuralModel::outputSize; ++output)
    {
        double mean = 0.0;
        for (const auto& target : targets)
            mean += target[output];
        mean /= static_cast<double>(targets.size());
        double variance = 0.0;
        for (const auto& target : targets)
        {
            const double delta = target[output] - mean;
            variance += delta * delta;
        }
        variance /= static_cast<double>(targets.size());
        const float scale = std::max(0.18f, static_cast<float>(std::sqrt(variance)));
        outputMeans[output] = static_cast<float>(mean);
        outputScales[output] = scale;
        for (auto& target : normalisedTargets)
            target[output] = (target[output] - static_cast<float>(mean)) / scale;
    }

    std::uint32_t randomState = 0x4e657572u;
    const auto randomBipolar = [&randomState]()
    {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return 2.0f * static_cast<float>(randomState) / 4294967295.0f - 1.0f;
    };
    for (auto& weight : inputWeights)
        weight = 0.35f * randomBipolar();
    for (auto& weight : outputWeights)
        weight = 0.025f * randomBipolar();

    // The conditioning inputs depend only on the frame time, so they are built
    // once instead of being re-derived from eight transcendental calls on
    // every frame of every epoch.
    std::vector<NetworkInputs> frameInputs(spectra.size());
    for (std::size_t frame = 0; frame < spectra.size(); ++frame)
        frameInputs[frame] = networkInputs(spectra[frame].normalisedTime,
                                           model.durationSeconds());

    const double lossScale = 1.0 / static_cast<double>(
        spectra.size() * NeuralModel::outputSize);
    initialLoss = 0.0f;
    float bestLoss = std::numeric_limits<float>::max();
    auto bestInputWeights = inputWeights;
    auto bestHiddenBiases = hiddenBiases;
    auto bestOutputWeights = outputWeights;
    auto bestOutputBiases = outputBiases;

    AdamState<NeuralModel::hiddenSize * NeuralModel::inputSize> inputAdam;
    AdamState<NeuralModel::hiddenSize> hiddenAdam;
    AdamState<NeuralModel::outputSize * NeuralModel::hiddenSize> outputAdam;
    AdamState<NeuralModel::outputSize> biasAdam;
    float beta1Power = 1.0f;
    float beta2Power = 1.0f;

    for (int epoch = 0; epoch < trainingEpochCount; ++epoch)
    {
        if (cancellationRequested(cancel))
            return false;

        std::array<float, NeuralModel::hiddenSize * NeuralModel::inputSize> inputGradient {};
        std::array<float, NeuralModel::hiddenSize> hiddenGradient {};
        std::array<float, NeuralModel::outputSize * NeuralModel::hiddenSize> outputGradient {};
        std::array<float, NeuralModel::outputSize> biasGradient {};

        double epochLoss = 0.0;
        for (std::size_t frame = 0; frame < spectra.size(); ++frame)
        {
            const auto& inputs = frameInputs[frame];
            std::array<float, NeuralModel::hiddenSize> hidden {};
            for (std::size_t unit = 0; unit < NeuralModel::hiddenSize; ++unit)
            {
                float value = hiddenBiases[unit];
                for (std::size_t input = 0; input < NeuralModel::inputSize; ++input)
                    value += inputWeights[unit * NeuralModel::inputSize + input]
                        * inputs[input];
                hidden[unit] = std::tanh(value);
            }

            std::array<float, NeuralModel::outputSize> errors {};
            for (std::size_t output = 0; output < NeuralModel::outputSize; ++output)
            {
                float prediction = outputBiases[output];
                for (std::size_t unit = 0; unit < NeuralModel::hiddenSize; ++unit)
                    prediction += outputWeights[output * NeuralModel::hiddenSize + unit]
                        * hidden[unit];
                errors[output] = prediction - normalisedTargets[frame][output];
                // The gradient pass already evaluates the whole network, so
                // the loss of these weights is free. The separate full
                // forward pass that used to follow every update is gone; the
                // final weights are evaluated once after the loop instead, so
                // exactly the same set of candidate states is still scored.
                epochLoss += static_cast<double>(errors[output]) * errors[output];
                biasGradient[output] += errors[output];
                for (std::size_t unit = 0; unit < NeuralModel::hiddenSize; ++unit)
                    outputGradient[output * NeuralModel::hiddenSize + unit]
                        += errors[output] * hidden[unit];
            }

            for (std::size_t unit = 0; unit < NeuralModel::hiddenSize; ++unit)
            {
                float propagated = 0.0f;
                for (std::size_t output = 0; output < NeuralModel::outputSize; ++output)
                    propagated += errors[output]
                        * outputWeights[output * NeuralModel::hiddenSize + unit];
                propagated *= 1.0f - hidden[unit] * hidden[unit];
                hiddenGradient[unit] += propagated;
                for (std::size_t input = 0; input < NeuralModel::inputSize; ++input)
                    inputGradient[unit * NeuralModel::inputSize + input]
                        += propagated * inputs[input];
            }
        }

        const auto loss = static_cast<float>(epochLoss * lossScale);
        if (epoch == 0)
            initialLoss = loss;
        if (std::isfinite(loss) && loss < bestLoss)
        {
            bestLoss = loss;
            bestInputWeights = inputWeights;
            bestHiddenBiases = hiddenBiases;
            bestOutputWeights = outputWeights;
            bestOutputBiases = outputBiases;
        }

        const float gradientScale = 1.0f / static_cast<float>(spectra.size());
        double gradientSquareSum = 0.0;
        const auto accumulateGradient = [&gradientSquareSum, gradientScale](auto& values)
        {
            for (auto& value : values)
            {
                value *= gradientScale;
                gradientSquareSum += static_cast<double>(value) * value;
            }
        };
        accumulateGradient(inputGradient);
        accumulateGradient(hiddenGradient);
        accumulateGradient(outputGradient);
        accumulateGradient(biasGradient);
        const float norm = static_cast<float>(std::sqrt(gradientSquareSum));
        if (norm > 5.0f)
        {
            const float clip = 5.0f / norm;
            const auto clipGradient = [clip](auto& values)
            {
                for (auto& value : values)
                    value *= clip;
            };
            clipGradient(inputGradient);
            clipGradient(hiddenGradient);
            clipGradient(outputGradient);
            clipGradient(biasGradient);
        }

        beta1Power *= 0.9f;
        beta2Power *= 0.999f;
        const float progress = static_cast<float>(epoch)
            / static_cast<float>(trainingEpochCount - 1);
        const float learningRate = 0.012f * (1.0f - 0.82f * progress);
        const float beta1Correction = 1.0f - beta1Power;
        const float beta2Correction = 1.0f - beta2Power;
        inputAdam.update(inputWeights, inputGradient, learningRate,
                         beta1Correction, beta2Correction);
        hiddenAdam.update(hiddenBiases, hiddenGradient, learningRate,
                          beta1Correction, beta2Correction);
        outputAdam.update(outputWeights, outputGradient, learningRate,
                          beta1Correction, beta2Correction);
        biasAdam.update(outputBiases, biasGradient, learningRate,
                        beta1Correction, beta2Correction);
        completedEpochs = epoch + 1;

        if ((epoch & 3) == 0 || epoch + 1 == trainingEpochCount)
            report(callback, SampleLearner::Stage::Training,
                   0.55f + 0.44f * static_cast<float>(epoch + 1)
                       / static_cast<float>(trainingEpochCount),
                   pitch, "Teaching the neural timbre map");
    }

    // Score the state produced by the final update as well, so the candidate
    // set is exactly the one the previous per-epoch evaluation covered.
    const float lastLoss = networkLoss(model, frameInputs, normalisedTargets);
    if (!(std::isfinite(lastLoss) && lastLoss < bestLoss))
    {
        inputWeights = bestInputWeights;
        hiddenBiases = bestHiddenBiases;
        outputWeights = bestOutputWeights;
        outputBiases = bestOutputBiases;
        finalLoss = bestLoss;
    }
    else
    {
        finalLoss = lastLoss;
    }
    return true;
}
} // namespace

std::vector<float> SampleLearner::resampleForTests(
    const std::vector<float>& input, double sourceRate,
    double destinationRate)
{
    return resampleWindowedSinc(input, sourceRate, destinationRate);
}

SampleLearner::LearnResult SampleLearner::learn(
    const std::vector<float>& monoSample,
    double sampleRate,
    ProgressCallback progressCallback,
    CancelPredicate cancelPredicate)
{
    LearnResult result;
    PitchEstimate pitch;
    report(progressCallback, Stage::Conditioning, 0.0f, pitch,
           "Conditioning the dropped sample");

    std::vector<float> conditionedSample;
    float sourcePeak = 0.0f;
    float sourceRms = 0.0f;
    if (!conditionSample(monoSample, sampleRate, conditionedSample,
                         sourcePeak, sourceRms, result.error, cancelPredicate))
    {
        if (cancellationRequested(cancelPredicate))
            result.cancelled = true;
        return result;
    }
    if (cancellationRequested(cancelPredicate))
    {
        result.cancelled = true;
        return result;
    }

    auto sample = resampleWindowedSinc(conditionedSample, sampleRate,
                                       fixedAnalysisSampleRate, cancelPredicate);
    if (cancellationRequested(cancelPredicate))
    {
        result.cancelled = true;
        return result;
    }
    if (sample.size() < 256)
    {
        result.error = "The conditioned sample could not be resampled for analysis";
        return result;
    }
    constexpr double analysisSampleRate = fixedAnalysisSampleRate;

    report(progressCallback, Stage::Pitch, 0.10f, pitch,
           "Listening across several pitch resolutions");
    // One band-limited decimation feeds both the multi-window root search and
    // the local pitch contour instead of being computed twice.
    double pitchRate = 0.0;
    const auto pitchSample = downsampleForPitch(
        sample, analysisSampleRate, pitchRate, cancelPredicate);
    if (cancellationRequested(cancelPredicate))
    {
        result.cancelled = true;
        return result;
    }
    pitch = estimatePitch(sample, analysisSampleRate, pitchSample, pitchRate,
                          cancelPredicate);
    if (cancellationRequested(cancelPredicate))
    {
        result.cancelled = true;
        return result;
    }
    if (!(pitch.frequencyHz > 0.0f) || pitch.confidence < 0.12f)
    {
        result.error = "No stable root pitch could be identified";
        return result;
    }

    report(progressCallback, Stage::Pitch, 0.20f, pitch,
           "Measuring the partial series");
    const auto stretch = estimateInharmonicity(
        sample, analysisSampleRate, pitch.frequencyHz, cancelPredicate);
    if (cancellationRequested(cancelPredicate))
    {
        result.cancelled = true;
        return result;
    }
    if (stretch.inharmonicity > 0.0f)
        pitch.frequencyHz = stretch.refinedRootHz;
    report(progressCallback, Stage::Pitch, 0.25f, pitch,
           stretch.inharmonicity > 0.0f
               ? "Root note identified with a stretched partial series"
               : "Root note identified");
    if (cancellationRequested(cancelPredicate))
    {
        result.cancelled = true;
        return result;
    }

    auto model = std::unique_ptr<NeuralModel>(new NeuralModel());
    model->inharmonicity_ = stretch.inharmonicity;
    auto& metadata = model->metadata_;
    metadata.sourceSampleRate = sampleRate;
    metadata.rootFrequencyHz = pitch.frequencyHz;
    const auto midiPitch = midiPitchFromFrequency(pitch.frequencyHz);
    metadata.rootMidiNote = midiPitch.note;
    metadata.rootCents = midiPitch.cents;
    metadata.pitchConfidence = pitch.confidence;
    metadata.durationSeconds = static_cast<float>(sample.size() - 1)
        / static_cast<float>(analysisSampleRate);
    metadata.sourcePeak = sourcePeak;
    metadata.sourceRms = sourceRms;
    makePreview(sample, metadata.waveformPreview);

    report(progressCallback, Stage::Analysis, 0.28f, pitch,
           "Tracing Core, Air, and Bone through time");
    std::vector<SpectrumFrame> spectra;
    spectra.reserve(analysisFrameCount);
    const auto analysisTimes = makeAnalysisTimes(metadata.durationSeconds);
    for (int frame = 0; frame < analysisFrameCount; ++frame)
    {
        const float frameProgress = static_cast<float>(frame)
            / static_cast<float>(analysisFrameCount - 1);
        spectra.push_back(makeSpectrumFrame(
            sample, analysisSampleRate,
            analysisTimes[static_cast<std::size_t>(frame)]));
        if ((frame & 7) == 0)
        {
            if (cancellationRequested(cancelPredicate))
            {
                result.cancelled = true;
                return result;
            }
            report(progressCallback, Stage::Analysis,
                   0.28f + 0.15f * frameProgress, pitch,
                   "Tracing spectral evolution");
        }
    }
    const auto pitchContour = estimatePitchContour(
        pitchSample, pitchRate, spectra, pitch.frequencyHz,
        cancelPredicate);
    if (cancellationRequested(cancelPredicate))
    {
        result.cancelled = true;
        return result;
    }

    std::vector<std::array<float, NeuralModel::harmonicCount>> harmonicAmplitudes;
    harmonicAmplitudes.reserve(spectra.size());
    std::vector<std::array<float, NeuralModel::harmonicCount>> harmonicPhases;
    harmonicPhases.reserve(spectra.size());
    std::vector<SpectrumFrame> residualSpectra;
    residualSpectra.reserve(spectra.size());
    std::vector<SpectrumFrame> transientResidualSpectra;
    transientResidualSpectra.reserve(spectra.size());
    for (std::size_t frameIndex = 0; frameIndex < spectra.size(); ++frameIndex)
    {
        const float framePitchHz = pitch.frequencyHz
            * std::exp2(pitchContour[frameIndex] / 12.0f);
        auto longAnalysis = analyseHarmonicResidual(
            sample, analysisSampleRate, spectra[frameIndex].normalisedTime,
            framePitchHz, stretch.inharmonicity);
        // The onset region carries 48 of the 128 analysis frames, 2.5 ms apart,
        // and measuring each of them through a four-period aperture threw that
        // resolution away: the first frame's amplitudes described an average of
        // the first 21 ms at a mid pitch and the renderer applied them from
        // sample zero. Halving the aperture there is what the joint solve buys.
        const auto transientWindow = transientAnalysisWindowSize(
            framePitchHz, analysisSampleRate,
            spectra[frameIndex].timeSeconds < shortApertureSeconds
                ? 2.0 : 4.0);
        if (transientWindow < spectrumSize)
        {
            auto transientAnalysis = analyseHarmonicResidual(
                sample, analysisSampleRate,
                spectra[frameIndex].normalisedTime, framePitchHz,
                stretch.inharmonicity, transientWindow);
            harmonicAmplitudes.push_back(transientAnalysis.amplitudes);
            harmonicPhases.push_back(transientAnalysis.sinePhases);
            transientResidualSpectra.push_back(
                std::move(transientAnalysis.residualSpectrum));
        }
        else
        {
            harmonicAmplitudes.push_back(longAnalysis.amplitudes);
            harmonicPhases.push_back(longAnalysis.sinePhases);
            transientResidualSpectra.push_back(longAnalysis.residualSpectrum);
        }
        // Bone selection and decay keep the long aperture: persistent modal
        // identity needs frequency resolution more than onset precision.
        residualSpectra.push_back(std::move(longAnalysis.residualSpectrum));
        if ((frameIndex & 7u) == 0u && cancellationRequested(cancelPredicate))
        {
            result.cancelled = true;
            return result;
        }
    }

    const auto boneSelection = findPersistentBoneModes(
        residualSpectra, pitch.frequencyHz, stretch.inharmonicity,
        analysisSampleRate);
    model->boneFrequencyRatios_ = boneSelection.ratios;
    model->boneModeReliabilities_ = boneSelection.reliabilities;
    model->initialHarmonicPhases_ = harmonicPhases.front();
    for (std::size_t mode = 0; mode < NeuralModel::boneModeCount; ++mode)
    {
        const float frequency = pitch.frequencyHz * boneSelection.ratios[mode];
        const auto firstFrameStart = -static_cast<std::ptrdiff_t>(spectrumSize / 2);
        float phase = spectrumPhase(residualSpectra.front(), frequency,
                                    analysisSampleRate)
            - frequency * static_cast<float>(firstFrameStart)
                / static_cast<float>(analysisSampleRate)
            + 0.25f;
        phase -= std::floor(phase);
        model->initialBonePhases_[mode] = phase;
        model->boneDecaySeconds_[mode] = estimateDecay(
            residualSpectra, frequency, analysisSampleRate,
            metadata.durationSeconds);
    }

    const float lowerAir = airLowerFrequencyHz;
    const float upperAir = std::min(
        airUpperFrequencyHz,
        0.42f * static_cast<float>(analysisSampleRate));
    const float airEdgeRatio = std::pow(
        upperAir / lowerAir,
        1.0f / static_cast<float>(NeuralModel::airBandCount));
    const float airWidthOctaves = std::log2(airEdgeRatio);
    for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
    {
        model->airCentreFrequenciesHz_[band] = lowerAir * std::pow(
            airEdgeRatio, static_cast<float>(band) + 0.5f);
        model->airBandwidthOctaves_[band] = airWidthOctaves;
    }
    constexpr std::array<std::size_t, 4> airAnalysisWindowSizes {
        512, 1024, 2048, spectrumSize
    };
    std::array<AirFitDesign, airAnalysisWindowSizes.size()> airFitDesigns {};
    for (std::size_t index = 0; index < airFitDesigns.size(); ++index)
    {
        airFitDesigns[index] = makeAirFitDesign(
            model->airCentreFrequenciesHz_, model->airBandwidthOctaves_,
            boneSelection, pitch.frequencyHz, stretch.inharmonicity,
            analysisSampleRate, airAnalysisWindowSizes[index]);
    }
    const auto airDesignIndex = [&airAnalysisWindowSizes] (
        std::size_t windowSize) noexcept
    {
        std::size_t index = 0;
        while (index + 1 < airAnalysisWindowSizes.size()
               && airAnalysisWindowSizes[index] < windowSize)
            ++index;
        return index;
    };

    std::vector<TargetFrame> targets(spectra.size());
    std::array<double, NeuralModel::airBandCount> previousAirPowers {};
    float loudestAirAmplitude = 0.0f;
    for (std::size_t frameIndex = 0; frameIndex < spectra.size(); ++frameIndex)
    {
        const auto& boneSpectrum = residualSpectra[frameIndex];
        const auto& transientSpectrum = transientResidualSpectra[frameIndex];
        auto& target = targets[frameIndex];
        for (std::size_t harmonic = 0;
             harmonic < NeuralModel::harmonicCount; ++harmonic)
        {
            const float amplitude = harmonicAmplitudes[frameIndex][harmonic];
            target[harmonic] = std::log(std::max(amplitude, amplitudeFloor));
        }

        const auto airFit = fitAirFilterbank(
            transientSpectrum,
            airFitDesigns[airDesignIndex(
                transientSpectrum.analysisWindowSize)],
            boneSelection, pitch.frequencyHz, stretch.inharmonicity,
            analysisSampleRate, previousAirPowers);
        previousAirPowers = airFit.powers;
        for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
        {
            loudestAirAmplitude = std::max(loudestAirAmplitude,
                                           airFit.amplitudes[band]);
            target[NeuralModel::harmonicCount + band] = std::log(
                std::max(airFit.amplitudes[band], amplitudeFloor));
        }

        for (std::size_t mode = 0; mode < NeuralModel::boneModeCount; ++mode)
        {
            const float amplitude = boneSelection.reliabilities[mode] > 0.0f
                ? spectrumMagnitude(
                    boneSpectrum, pitch.frequencyHz * boneSelection.ratios[mode],
                    analysisSampleRate)
                : 0.0f;
            target[NeuralModel::harmonicCount + NeuralModel::airBandCount + mode]
                = std::log(std::max(amplitude, amplitudeFloor));
        }
        target[NeuralModel::pitchOutputIndex] = pitchContour[frameIndex];
    }

    // The non-negative solver returns exact zeros for bands it rejects, and
    // log(amplitudeFloor) is -139 dB. Left alone, a band that drops out for a
    // few frames makes its target jump by more than 100 dB and back, which
    // dominates that output's normalisation and gates the whole noise layer on
    // and off. Sixty dB below the loudest band this sound ever produced is
    // inaudible and leaves the trajectory continuous.
    if (loudestAirAmplitude > 0.0f)
    {
        const float airFloor = std::log(std::max(
            1.0e-3f * loudestAirAmplitude, amplitudeFloor));
        for (auto& target : targets)
            for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
            {
                auto& value = target[NeuralModel::harmonicCount + band];
                value = std::max(value, airFloor);
            }
    }

    chooseLoop(targets, spectra, metadata.durationSeconds,
               metadata.loopStartSeconds, metadata.loopEndSeconds);
    report(progressCallback, Stage::Analysis, 0.54f, pitch,
           "Spectral memory prepared");

    if (!train(*model, spectra, targets, progressCallback, cancelPredicate,
               pitch, result.initialLoss, result.finalLoss,
               result.trainingEpochs))
    {
        result.cancelled = true;
        return result;
    }

    report(progressCallback, Stage::Training, 0.995f, pitch,
           "Capturing fine temporal detail");
    const std::size_t residualFrameCount = std::min(
        spectra.size(), NeuralModel::maximumResidualKeyframes);
    model->residualKeyframeCount_ = static_cast<std::uint32_t>(
        residualFrameCount);
    for (std::size_t frame = 0; frame < residualFrameCount; ++frame)
        model->residualTimes_[frame] = spectra[frame].normalisedTime;

    std::array<float, NeuralModel::outputSize> basePrediction {};
    for (std::size_t frame = 0; frame < residualFrameCount; ++frame)
    {
        if ((frame & 15u) == 0u && cancellationRequested(cancelPredicate))
        {
            result.cancelled = true;
            return result;
        }
        model->evaluateBaseRaw(model->residualTimes_[frame], basePrediction);
        for (std::size_t output = 0; output < NeuralModel::outputSize; ++output)
        {
            const float correction = targets[frame][output]
                - basePrediction[output];
            if (std::isfinite(correction))
            {
                model->residualScales_[output] = std::min(
                    NeuralModel::maximumResidualScale,
                    std::max(model->residualScales_[output],
                             std::abs(correction)));
            }
        }
    }

    constexpr float quantisationMaximum = 32767.0f;
    for (std::size_t frame = 0; frame < residualFrameCount; ++frame)
    {
        model->evaluateBaseRaw(model->residualTimes_[frame], basePrediction);
        for (std::size_t output = 0; output < NeuralModel::outputSize; ++output)
        {
            const float scale = model->residualScales_[output];
            if (scale <= 1.0e-7f)
            {
                model->residualScales_[output] = 0.0f;
                model->residualValues_[frame * NeuralModel::outputSize + output]
                    = 0;
                continue;
            }
            const float correction = std::isfinite(targets[frame][output]
                                                    - basePrediction[output])
                ? targets[frame][output] - basePrediction[output] : 0.0f;
            const float normalised = std::clamp(correction / scale,
                                                -1.0f, 1.0f);
            model->residualValues_[frame * NeuralModel::outputSize + output]
                = static_cast<std::int16_t>(std::lround(
                    normalised * quantisationMaximum));
        }
    }

    metadata.initialLoss = result.initialLoss;
    metadata.finalLoss = result.finalLoss;
    metadata.trainingEpochs = result.trainingEpochs;
    result.model = std::move(model);
    report(progressCallback, Stage::Training, 1.0f, pitch,
           "Neural instrument ready to play");
    return result;
}

} // namespace neuramar
