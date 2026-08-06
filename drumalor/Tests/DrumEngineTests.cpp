#include "DSP/DrumEngine.h"
#include "DSP/UiMath.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <vector>

// Toggling flush-to-zero is how the denormal-cost contract below is measured.
// Only x86 exposes it portably here; elsewhere that one assertion is skipped.
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#define DRUMALOR_CAN_TOGGLE_FLUSH_TO_ZERO 1
#include <pmmintrin.h>
#include <xmmintrin.h>
#else
#define DRUMALOR_CAN_TOGGLE_FLUSH_TO_ZERO 0
#endif

namespace
{
constexpr int defaultBlockSize = 257;
constexpr double analysisPi = 3.1415926535897932384626433832795;
int failureCount = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct RenderMetrics
{
    double sumOfSquares { 0.0 };
    double peak { 0.0 };
    std::size_t sampleCount { 0 };
    std::size_t nonZeroCount { 0 };
    bool finite { true };

    [[nodiscard]] double rms() const noexcept
    {
        return sampleCount == 0 ? 0.0
                                : std::sqrt (sumOfSquares / static_cast<double> (sampleCount));
    }
};

RenderMetrics renderMetrics (drumalor::DrumEngine& engine, int numSamples,
                             int blockSize = defaultBlockSize)
{
    std::vector<float> left (static_cast<std::size_t> (blockSize));
    std::vector<float> right (static_cast<std::size_t> (blockSize));
    RenderMetrics metrics;
    for (int rendered = 0; rendered < numSamples;)
    {
        const int count = std::min (blockSize, numSamples - rendered);
        engine.process (left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            for (const float value : { left[static_cast<std::size_t> (sample)],
                                       right[static_cast<std::size_t> (sample)] })
            {
                metrics.finite = metrics.finite && std::isfinite (value);
                if (std::isfinite (value))
                {
                    const double magnitude = std::abs (static_cast<double> (value));
                    metrics.peak = std::max (metrics.peak, magnitude);
                    metrics.sumOfSquares += static_cast<double> (value) * value;
                    if (magnitude > 1.0e-9)
                        ++metrics.nonZeroCount;
                }
                ++metrics.sampleCount;
            }
        }
        rendered += count;
    }
    return metrics;
}

RenderMetrics metricsForInterleaved (const std::vector<float>& samples)
{
    RenderMetrics metrics;
    for (const float value : samples)
    {
        metrics.finite = metrics.finite && std::isfinite (value);
        if (std::isfinite (value))
        {
            const double magnitude = std::abs (static_cast<double> (value));
            metrics.peak = std::max (metrics.peak, magnitude);
            metrics.sumOfSquares += static_cast<double> (value) * value;
            if (magnitude > 1.0e-9)
                ++metrics.nonZeroCount;
        }
        ++metrics.sampleCount;
    }
    return metrics;
}

std::vector<float> renderInterleaved (drumalor::DrumEngine& engine, int numSamples,
                                      int blockSize)
{
    std::vector<float> result (static_cast<std::size_t> (numSamples) * 2u);
    std::vector<float> left (static_cast<std::size_t> (blockSize));
    std::vector<float> right (static_cast<std::size_t> (blockSize));
    for (int rendered = 0; rendered < numSamples;)
    {
        const int count = std::min (blockSize, numSamples - rendered);
        engine.process (left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto target = static_cast<std::size_t> (rendered + sample) * 2u;
            result[target] = left[static_cast<std::size_t> (sample)];
            result[target + 1u] = right[static_cast<std::size_t> (sample)];
        }
        rendered += count;
    }
    return result;
}

double meanAbsoluteDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    const auto count = std::min (a.size(), b.size());
    double difference = 0.0;
    for (std::size_t index = 0; index < count; ++index)
        difference += std::abs (static_cast<double> (a[index]) - b[index]);
    return count == 0 ? 0.0 : difference / static_cast<double> (count);
}

double meanAbsoluteMagnitude (const std::vector<float>& values)
{
    double magnitude = 0.0;
    for (const float value : values)
        magnitude += std::abs (static_cast<double> (value));
    return values.empty() ? 0.0 : magnitude / static_cast<double> (values.size());
}

struct AnalysisBiquad
{
    double b0 { 1.0 };
    double b1 { 0.0 };
    double b2 { 0.0 };
    double a1 { 0.0 };
    double a2 { 0.0 };
    double z1 { 0.0 };
    double z2 { 0.0 };

    [[nodiscard]] double tick (double input) noexcept
    {
        const double output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        return output;
    }
};

AnalysisBiquad makeAnalysisLowpass (double sampleRate, double frequency)
{
    const double omega = 2.0 * analysisPi * frequency / sampleRate;
    const double cosine = std::cos (omega);
    const double sine = std::sin (omega);
    const double alpha = sine / (2.0 * std::sqrt (0.5));
    const double inverseA0 = 1.0 / (1.0 + alpha);
    return { 0.5 * (1.0 - cosine) * inverseA0,
             (1.0 - cosine) * inverseA0,
             0.5 * (1.0 - cosine) * inverseA0,
             -2.0 * cosine * inverseA0,
             (1.0 - alpha) * inverseA0 };
}

AnalysisBiquad makeAnalysisHighpass (double sampleRate, double frequency)
{
    const double omega = 2.0 * analysisPi * frequency / sampleRate;
    const double cosine = std::cos (omega);
    const double sine = std::sin (omega);
    const double alpha = sine / (2.0 * std::sqrt (0.5));
    const double inverseA0 = 1.0 / (1.0 + alpha);
    return { 0.5 * (1.0 + cosine) * inverseA0,
             -(1.0 + cosine) * inverseA0,
             0.5 * (1.0 + cosine) * inverseA0,
             -2.0 * cosine * inverseA0,
             (1.0 - alpha) * inverseA0 };
}

std::vector<float> renderMonoHit (double sampleRate,
                                  drumalor::Instrument instrument,
                                  const drumalor::InstrumentParameters& parameters,
                                  double durationSeconds = 0.75,
                                  float velocity = 0.95f)
{
    drumalor::DrumEngine engine;
    engine.prepare (sampleRate, defaultBlockSize);
    engine.setInstrumentParameters (instrument, parameters);
    engine.trigger (instrument, velocity);
    const int sampleCount = static_cast<int> (std::ceil (durationSeconds * sampleRate));
    const auto interleaved = renderInterleaved (engine, sampleCount, defaultBlockSize);
    std::vector<float> mono (static_cast<std::size_t> (sampleCount));
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const auto index = static_cast<std::size_t> (sample) * 2u;
        mono[static_cast<std::size_t> (sample)] = 0.5f
            * (interleaved[index] + interleaved[index + 1u]);
    }
    return mono;
}

// Repeated strikes into one engine, averaged down to mono. The stochastic
// layers need several hits before a level measurement stops being dominated by
// which particular noise realisation a single strike happened to draw.
std::vector<float> renderMonoSequence (double sampleRate,
                                       drumalor::Instrument instrument,
                                       const drumalor::InstrumentParameters& parameters,
                                       int hitCount, double spacingSeconds,
                                       float velocity)
{
    drumalor::DrumEngine engine;
    engine.prepare (sampleRate, defaultBlockSize);
    engine.setInstrumentParameters (instrument, parameters);
    const auto spacing = static_cast<int> (std::ceil (spacingSeconds * sampleRate));
    std::vector<float> mono;
    mono.reserve (static_cast<std::size_t> (spacing * hitCount));
    for (int hit = 0; hit < hitCount; ++hit)
    {
        engine.trigger (instrument, velocity);
        const auto interleaved = renderInterleaved (engine, spacing, defaultBlockSize);
        for (int sample = 0; sample < spacing; ++sample)
        {
            const auto index = static_cast<std::size_t> (sample) * 2u;
            mono.push_back (0.5f * (interleaved[index] + interleaved[index + 1u]));
        }
    }
    return mono;
}

double rmsInRange (const std::vector<float>& samples, std::size_t begin, std::size_t end)
{
    begin = std::min (begin, samples.size());
    end = std::clamp (end, begin, samples.size());
    double sumOfSquares = 0.0;
    for (std::size_t sample = begin; sample < end; ++sample)
        sumOfSquares += static_cast<double> (samples[sample]) * samples[sample];
    return begin == end ? 0.0
                        : std::sqrt (sumOfSquares / static_cast<double> (end - begin));
}

double meanInRange (const std::vector<float>& samples, std::size_t begin, std::size_t end)
{
    begin = std::min (begin, samples.size());
    end = std::clamp (end, begin, samples.size());
    double sum = 0.0;
    for (std::size_t sample = begin; sample < end; ++sample)
        sum += samples[sample];
    return begin == end ? 0.0 : sum / static_cast<double> (end - begin);
}

double peakInRange (const std::vector<float>& samples, std::size_t begin, std::size_t end)
{
    begin = std::min (begin, samples.size());
    end = std::clamp (end, begin, samples.size());
    double peak = 0.0;
    for (std::size_t sample = begin; sample < end; ++sample)
        peak = std::max (peak, std::abs (static_cast<double> (samples[sample])));
    return peak;
}

std::vector<float> filterForAnalysis (const std::vector<float>& samples,
                                      double sampleRate,
                                      double highpassFrequency,
                                      double lowpassFrequency)
{
    auto highpass = makeAnalysisHighpass (sampleRate, std::max (1.0, highpassFrequency));
    auto lowpass = makeAnalysisLowpass (
        sampleRate, std::min (0.45 * sampleRate, std::max (1.0, lowpassFrequency)));
    std::vector<float> result (samples.size());
    for (std::size_t sample = 0; sample < samples.size(); ++sample)
    {
        double value = samples[sample];
        if (highpassFrequency > 0.0)
            value = highpass.tick (value);
        if (lowpassFrequency > 0.0)
            value = lowpass.tick (value);
        result[sample] = static_cast<float> (value);
    }
    return result;
}

double bandPowerInRange (const std::vector<float>& samples,
                         double sampleRate,
                         double highpassFrequency,
                         double lowpassFrequency,
                         double beginSeconds,
                         double endSeconds)
{
    const auto filtered = filterForAnalysis (
        samples, sampleRate, highpassFrequency, lowpassFrequency);
    const std::size_t begin = std::min (
        filtered.size(), static_cast<std::size_t> (std::ceil (beginSeconds * sampleRate)));
    const std::size_t end = std::min (
        filtered.size(), static_cast<std::size_t> (std::ceil (endSeconds * sampleRate)));
    const double rms = rmsInRange (filtered, begin, end);
    return rms * rms;
}

double maximumNormalizedAutocorrelation (const std::vector<float>& samples,
                                         double sampleRate,
                                         double beginSeconds,
                                         double endSeconds,
                                         double minimumFrequency,
                                         double maximumFrequency)
{
    const std::size_t begin = std::min (
        samples.size(), static_cast<std::size_t> (std::ceil (beginSeconds * sampleRate)));
    const std::size_t end = std::min (
        samples.size(), static_cast<std::size_t> (std::ceil (endSeconds * sampleRate)));
    if (end <= begin + 2u)
        return 0.0;

    const std::size_t minimumLag = std::max<std::size_t> (
        1u, static_cast<std::size_t> (std::floor (sampleRate / maximumFrequency)));
    const std::size_t maximumLag = std::min (
        end - begin - 1u,
        static_cast<std::size_t> (std::ceil (sampleRate / minimumFrequency)));
    const double mean = meanInRange (samples, begin, end);
    double maximum = 0.0;
    for (std::size_t lag = minimumLag; lag <= maximumLag; ++lag)
    {
        double product = 0.0;
        double firstPower = 0.0;
        double secondPower = 0.0;
        for (std::size_t sample = begin + lag; sample < end; ++sample)
        {
            const double first = static_cast<double> (samples[sample]) - mean;
            const double second = static_cast<double> (samples[sample - lag]) - mean;
            product += first * second;
            firstPower += first * first;
            secondPower += second * second;
        }
        const double denominator = std::sqrt (firstPower * secondPower);
        if (denominator > 1.0e-20)
            maximum = std::max (maximum, std::abs (product / denominator));
    }
    return maximum;
}

struct CymbalAnalysis
{
    double rms { 0.0 };
    double presenceShare { 0.0 };
    double lowShare { 0.0 };
    double airShare { 0.0 };
    double bodyShare { 0.0 };
    double logBandEntropy { 0.0 };
    double activeBandFraction { 0.0 };
    double tonalPersistence { 0.0 };
    bool finite { true };
};

CymbalAnalysis analyseCymbal (const std::vector<float>& samples, double sampleRate)
{
    constexpr double bodyBeginSeconds = 0.040;
    constexpr double bodyEndSeconds = 0.750;
    CymbalAnalysis result;
    const std::size_t bodyBegin = std::min (
        samples.size(), static_cast<std::size_t> (std::ceil (bodyBeginSeconds * sampleRate)));
    const std::size_t bodyEnd = std::min (
        samples.size(), static_cast<std::size_t> (std::ceil (bodyEndSeconds * sampleRate)));
    result.rms = rmsInRange (samples, bodyBegin, bodyEnd);
    result.finite = std::all_of (samples.begin(), samples.end(), [] (float value)
    {
        return std::isfinite (value);
    });

    // Broad, overlapping Butterworth regions intentionally describe perceived
    // weight rather than exact modal frequencies. This lets the implementation
    // evolve while guarding against a low/brittle Ride or all-air Crash.
    const double totalPower = bandPowerInRange (
        samples, sampleRate, 300.0, 16000.0, bodyBeginSeconds, bodyEndSeconds);
    const double safeTotalPower = std::max (1.0e-20, totalPower);
    result.presenceShare = bandPowerInRange (
        samples, sampleRate, 900.0, 5000.0, bodyBeginSeconds, bodyEndSeconds)
        / safeTotalPower;
    result.lowShare = bandPowerInRange (
        samples, sampleRate, 300.0, 1200.0, bodyBeginSeconds, bodyEndSeconds)
        / safeTotalPower;
    result.airShare = bandPowerInRange (
        samples, sampleRate, 5000.0, 14000.0, bodyBeginSeconds, bodyEndSeconds)
        / safeTotalPower;
    result.bodyShare = bandPowerInRange (
        samples, sampleRate, 500.0, 4000.0, bodyBeginSeconds, bodyEndSeconds)
        / safeTotalPower;

    // A third-octave bank detects a spectrum collapsing onto a few ringing
    // modes without freezing the test to particular oscillator ratios.
    constexpr std::size_t logBandCount = 15u;
    std::array<double, logBandCount> logBandPowers {};
    double logBandPowerSum = 0.0;
    double maximumLogBandPower = 0.0;
    for (std::size_t band = 0; band < logBandCount; ++band)
    {
        const double lower = 500.0 * std::exp2 (static_cast<double> (band) / 3.0);
        const double upper = 500.0 * std::exp2 (static_cast<double> (band + 1u) / 3.0);
        const double power = bandPowerInRange (
            samples, sampleRate, lower, upper, bodyBeginSeconds, bodyEndSeconds);
        logBandPowers[band] = power;
        logBandPowerSum += power;
        maximumLogBandPower = std::max (maximumLogBandPower, power);
    }
    if (logBandPowerSum > 1.0e-20)
    {
        double entropy = 0.0;
        std::size_t activeBands = 0u;
        constexpr double activeBandPowerRatio = 0.03162277660168379; // -15 dB
        for (const double power : logBandPowers)
        {
            const double probability = power / logBandPowerSum;
            if (probability > 0.0)
                entropy -= probability * std::log (probability);
            if (power >= maximumLogBandPower * activeBandPowerRatio)
                ++activeBands;
        }
        result.logBandEntropy = entropy / std::log (static_cast<double> (logBandCount));
        result.activeBandFraction = static_cast<double> (activeBands)
            / static_cast<double> (logBandCount);
    }

    // Narrow ringing remains strongly self-correlated in the tail. Spread may
    // improve either this measure or the coarse spectral distribution below.
    result.tonalPersistence = maximumNormalizedAutocorrelation (
        samples, sampleRate, 0.100, 0.600, 180.0, 2200.0);
    return result;
}

double medianOfThree (std::array<double, 3> values)
{
    std::sort (values.begin(), values.end());
    return values[1];
}

CymbalAnalysis analyseCymbalPreset (drumalor::Instrument instrument,
                                    const drumalor::InstrumentParameters& parameters)
{
    constexpr double sampleRate = 48000.0;
    constexpr int captureSamples = static_cast<int> (0.90 * sampleRate);
    constexpr std::size_t hitCount = 3u;
    drumalor::DrumEngine engine;
    engine.prepare (sampleRate, defaultBlockSize);
    engine.setInstrumentParameters (instrument, parameters);

    std::array<CymbalAnalysis, hitCount> hits {};
    for (std::size_t hit = 0; hit < hitCount; ++hit)
    {
        engine.trigger (instrument, 0.90f);
        const auto interleaved = renderInterleaved (
            engine, captureSamples, defaultBlockSize);
        std::vector<float> mono (static_cast<std::size_t> (captureSamples));
        for (int sample = 0; sample < captureSamples; ++sample)
        {
            const auto index = static_cast<std::size_t> (sample) * 2u;
            mono[static_cast<std::size_t> (sample)] = 0.5f
                * (interleaved[index] + interleaved[index + 1u]);
        }
        hits[hit] = analyseCymbal (mono, sampleRate);

        // Preserve the trigger counter/component drift for the next organic
        // hit, but clear the captured tail so the spectra never overlap.
        engine.allSoundsOff();
        renderMetrics (engine, 4096, defaultBlockSize);
    }

    CymbalAnalysis result;
    result.rms = medianOfThree ({ hits[0].rms, hits[1].rms, hits[2].rms });
    result.presenceShare = medianOfThree (
        { hits[0].presenceShare, hits[1].presenceShare, hits[2].presenceShare });
    result.lowShare = medianOfThree (
        { hits[0].lowShare, hits[1].lowShare, hits[2].lowShare });
    result.airShare = medianOfThree (
        { hits[0].airShare, hits[1].airShare, hits[2].airShare });
    result.bodyShare = medianOfThree (
        { hits[0].bodyShare, hits[1].bodyShare, hits[2].bodyShare });
    result.logBandEntropy = medianOfThree (
        { hits[0].logBandEntropy, hits[1].logBandEntropy, hits[2].logBandEntropy });
    result.activeBandFraction = medianOfThree (
        { hits[0].activeBandFraction, hits[1].activeBandFraction,
          hits[2].activeBandFraction });
    result.tonalPersistence = medianOfThree (
        { hits[0].tonalPersistence, hits[1].tonalPersistence,
          hits[2].tonalPersistence });
    result.finite = hits[0].finite && hits[1].finite && hits[2].finite;
    return result;
}

double estimateSettledFundamental (const std::vector<float>& samples,
                                   double sampleRate,
                                   double minimumFrequency,
                                   double maximumFrequency)
{
    // Removing upper harmonics before finding positive-going crossings keeps
    // the estimator stable when Drive intentionally changes the waveshape.
    const auto smoothed = filterForAnalysis (
        samples, sampleRate, 0.0, std::min (180.0, 0.42 * sampleRate));
    const std::size_t begin = std::min (
        smoothed.size(), static_cast<std::size_t> (std::ceil (0.080 * sampleRate)));
    const std::size_t end = std::min (
        smoothed.size(), static_cast<std::size_t> (std::ceil (0.32 * sampleRate)));
    std::vector<double> crossings;
    for (std::size_t sample = begin + 1u; sample < end; ++sample)
    {
        const double before = smoothed[sample - 1u];
        const double after = smoothed[sample];
        if (before <= 0.0 && after > 0.0)
        {
            const double denominator = after - before;
            const double fraction = denominator == 0.0 ? 0.0 : -before / denominator;
            const double crossing = static_cast<double> (sample - 1u) + fraction;
            if (crossings.empty()
                || crossing - crossings.back() >= 0.72 * sampleRate / maximumFrequency)
                crossings.push_back (crossing);
        }
    }
    if (crossings.size() < 3u)
        return 0.0;

    std::vector<double> periods;
    periods.reserve (crossings.size() - 1u);
    const double minimumPeriod = sampleRate / maximumFrequency;
    const double maximumPeriod = sampleRate / minimumFrequency;
    for (std::size_t crossing = 1; crossing < crossings.size(); ++crossing)
    {
        const double period = crossings[crossing] - crossings[crossing - 1u];
        if (period >= minimumPeriod && period <= maximumPeriod)
            periods.push_back (period);
    }
    if (periods.size() < 2u)
        return 0.0;
    std::sort (periods.begin(), periods.end());
    const double medianPeriod = periods[periods.size() / 2u];
    return sampleRate / medianPeriod;
}

double windowedMagnitude (const std::vector<float>& samples,
                          double sampleRate,
                          double frequency,
                          double beginSeconds = 0.10,
                          double endSeconds = 0.42)
{
    const std::size_t begin = std::min (
        samples.size(), static_cast<std::size_t> (std::ceil (beginSeconds * sampleRate)));
    const std::size_t end = std::min (
        samples.size(), static_cast<std::size_t> (std::ceil (endSeconds * sampleRate)));
    if (end <= begin + 1u)
        return 0.0;
    const double mean = meanInRange (samples, begin, end);
    double real = 0.0;
    double imaginary = 0.0;
    double windowSum = 0.0;
    for (std::size_t sample = begin; sample < end; ++sample)
    {
        const double normalized = static_cast<double> (sample - begin)
            / static_cast<double> (end - begin - 1u);
        const double window = 0.5 - 0.5 * std::cos (2.0 * analysisPi * normalized);
        const double phase = 2.0 * analysisPi * frequency
            * static_cast<double> (sample) / sampleRate;
        const double value = (static_cast<double> (samples[sample]) - mean) * window;
        real += value * std::cos (phase);
        imaginary -= value * std::sin (phase);
        windowSum += window;
    }
    return 2.0 * std::hypot (real, imaginary) / std::max (1.0, windowSum);
}

double harmonicRichness (const std::vector<float>& samples,
                         double sampleRate,
                         double fundamental,
                         int highestHarmonic = 6)
{
    const double fundamentalMagnitude = windowedMagnitude (
        samples, sampleRate, fundamental);
    double harmonicPower = 0.0;
    for (int harmonic = 2; harmonic <= highestHarmonic; ++harmonic)
    {
        const double magnitude = windowedMagnitude (
            samples, sampleRate, fundamental * static_cast<double> (harmonic));
        harmonicPower += magnitude * magnitude;
    }
    return std::sqrt (harmonicPower) / std::max (1.0e-12, fundamentalMagnitude);
}

struct KickAnalysis
{
    double fundamentalHz { 0.0 };
    double fullRms { 0.0 };
    double lowRms { 0.0 };
    double midRms { 0.0 };
    double transientHighRms { 0.0 };
    double transientRms { 0.0 };
    double peak { 0.0 };
    double mean { 0.0 };
    bool finite { true };
};

KickAnalysis analyseKick (const std::vector<float>& samples, double sampleRate,
                          double minimumFrequency = 35.0,
                          double maximumFrequency = 130.0)
{
    KickAnalysis result;
    const auto low = filterForAnalysis (samples, sampleRate, 20.0, 100.0);
    const auto mid = filterForAnalysis (samples, sampleRate, 130.0, 2000.0);
    const auto high = filterForAnalysis (samples, sampleRate, 2000.0, 0.0);
    const std::size_t bodyEnd = std::min (
        samples.size(), static_cast<std::size_t> (std::ceil (0.60 * sampleRate)));
    const std::size_t transientEnd = std::min (
        samples.size(), static_cast<std::size_t> (std::ceil (0.030 * sampleRate)));
    result.fundamentalHz = estimateSettledFundamental (
        samples, sampleRate, minimumFrequency, maximumFrequency);
    result.fullRms = rmsInRange (samples, 0, bodyEnd);
    result.lowRms = rmsInRange (low, 0, bodyEnd);
    result.midRms = rmsInRange (mid, 0, bodyEnd);
    result.transientHighRms = rmsInRange (high, 0, transientEnd);
    result.transientRms = rmsInRange (samples, 0, transientEnd);
    result.peak = peakInRange (samples, 0, samples.size());
    result.mean = meanInRange (samples, 0, samples.size());
    result.finite = std::all_of (samples.begin(), samples.end(), [] (float value)
    {
        return std::isfinite (value);
    });
    return result;
}

template <std::size_t size>
double decibelSpread (const std::array<double, size>& values)
{
    const auto [minimum, maximum] = std::minmax_element (values.begin(), values.end());
    return 20.0 * std::log10 (*maximum / std::max (1.0e-12, *minimum));
}

template <std::size_t size>
std::string describeValues (const std::array<double, size>& values)
{
    std::string result;
    for (const double value : values)
    {
        if (! result.empty())
            result += ", ";
        result += std::to_string (value);
    }
    return result;
}

int renderUntilInactive (drumalor::DrumEngine& engine, int maximumSamples,
                         int blockSize = defaultBlockSize)
{
    int rendered = 0;
    while (engine.getActiveVoiceCount() > 0 && rendered < maximumSamples)
    {
        const int count = std::min (blockSize, maximumSamples - rendered);
        renderMetrics (engine, count, blockSize);
        rendered += count;
    }
    return rendered;
}

struct IsolatedHit
{
    std::vector<float> samples;
    RenderMetrics metrics;
    int activeSamples { 0 };
    bool terminated { false };
};

IsolatedHit renderIsolatedHit (drumalor::DrumEngine& engine,
                               drumalor::Instrument instrument,
                               float velocity,
                               int captureSamples = 8192)
{
    constexpr int probeBlockSize = 64;
    constexpr int maximumSamples = static_cast<int> (
        48000.0 * drumalor::maximumTailSeconds);
    IsolatedHit result;
    result.samples.resize (static_cast<std::size_t> (captureSamples) * 2u);
    std::array<float, probeBlockSize> left {};
    std::array<float, probeBlockSize> right {};

    engine.trigger (instrument, velocity);
    while (engine.getActiveVoiceCount() > 0 && result.activeSamples < maximumSamples)
    {
        const int count = std::min (probeBlockSize, maximumSamples - result.activeSamples);
        engine.process (left.data(), right.data(), count);

        if (result.activeSamples < captureSamples)
        {
            const int copied = std::min (count, captureSamples - result.activeSamples);
            for (int sample = 0; sample < copied; ++sample)
            {
                const auto target = static_cast<std::size_t> (result.activeSamples + sample) * 2u;
                result.samples[target] = left[static_cast<std::size_t> (sample)];
                result.samples[target + 1u] = right[static_cast<std::size_t> (sample)];
            }
        }
        result.activeSamples += count;
    }

    result.terminated = engine.getActiveVoiceCount() == 0;
    result.metrics = metricsForInterleaved (result.samples);
    return result;
}

void testMetadataAndMidiMapping()
{
    struct MidiMapping
    {
        int note;
        drumalor::Instrument instrument;
    };
    constexpr std::array expectedMappings {
        MidiMapping { 35, drumalor::Instrument::Kick },
        MidiMapping { 36, drumalor::Instrument::Kick },
        MidiMapping { 37, drumalor::Instrument::Perc2 },
        MidiMapping { 38, drumalor::Instrument::Snare },
        MidiMapping { 39, drumalor::Instrument::Clap },
        MidiMapping { 40, drumalor::Instrument::Snare },
        MidiMapping { 41, drumalor::Instrument::LowTom },
        MidiMapping { 42, drumalor::Instrument::ClosedHat },
        MidiMapping { 43, drumalor::Instrument::LowTom },
        MidiMapping { 44, drumalor::Instrument::ClosedHat },
        MidiMapping { 45, drumalor::Instrument::LowTom },
        MidiMapping { 46, drumalor::Instrument::OpenHat },
        MidiMapping { 47, drumalor::Instrument::MidTom },
        MidiMapping { 48, drumalor::Instrument::MidTom },
        MidiMapping { 49, drumalor::Instrument::Crash },
        MidiMapping { 50, drumalor::Instrument::HighTom },
        MidiMapping { 51, drumalor::Instrument::Ride },
        MidiMapping { 53, drumalor::Instrument::Ride },
        MidiMapping { 56, drumalor::Instrument::Perc1 },
        MidiMapping { 57, drumalor::Instrument::Crash },
        MidiMapping { 59, drumalor::Instrument::Ride },
        MidiMapping { 70, drumalor::Instrument::Shaker },
        MidiMapping { 75, drumalor::Instrument::Perc2 },
        MidiMapping { 76, drumalor::Instrument::Perc2 },
        MidiMapping { 77, drumalor::Instrument::Perc2 },
        MidiMapping { 82, drumalor::Instrument::Shaker },
    };

    std::set<std::string> slugs;
    std::set<int> primaryNotes;
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto& item = drumalor::getInstrumentMetadata (instrument);
        expect (item.instrument == instrument, "metadata instrument order changed");
        expect (! drumalor::getInstrumentDisplayName (instrument).empty(), "empty display name");
        expect (! drumalor::getInstrumentSlug (instrument).empty(), "empty instrument slug");
        expect (! drumalor::getCharacterALabel (instrument).empty(), "empty character A label");
        expect (! drumalor::getCharacterBLabel (instrument).empty(), "empty character B label");
        expect (slugs.insert (std::string (item.slug)).second, "duplicate instrument slug");
        expect (primaryNotes.insert (item.standardMidiNote).second, "duplicate primary MIDI note");
        const auto mapped = drumalor::instrumentForMidiNote (item.standardMidiNote);
        expect (mapped.has_value() && *mapped == instrument,
                std::string (item.displayName) + " primary MIDI note maps incorrectly");
    }

    for (int midiNote = 0; midiNote <= 127; ++midiNote)
    {
        const auto expected = std::find_if (
            expectedMappings.begin(), expectedMappings.end(),
            [midiNote] (const MidiMapping& mapping) { return mapping.note == midiNote; });
        const auto actual = drumalor::instrumentForMidiNote (midiNote);
        if (expected == expectedMappings.end())
        {
            expect (! actual.has_value(),
                    "unexpected MIDI mapping for note " + std::to_string (midiNote));
        }
        else
        {
            expect (actual.has_value() && *actual == expected->instrument,
                    "incorrect MIDI mapping for note " + std::to_string (midiNote));
        }
    }
    expect (! drumalor::instrumentForMidiNote (-1).has_value(), "negative MIDI note was accepted");
    expect (! drumalor::instrumentForMidiNote (128).has_value(), "out-of-range MIDI note was accepted");
}

void testEveryInstrumentAndSampleRate()
{
    constexpr std::array sampleRates { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 };
    for (const double sampleRate : sampleRates)
    {
        for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
        {
            const auto instrument = static_cast<drumalor::Instrument> (index);
            drumalor::DrumEngine engine;
            engine.prepare (sampleRate, defaultBlockSize);
            engine.setInstrumentParameters (instrument,
                                             drumalor::getInstrumentMetadata (instrument).defaultParameters);
            engine.trigger (instrument, 0.82f);
            const auto metrics = renderMetrics (engine, static_cast<int> (0.16 * sampleRate));
            const std::string label = std::string (drumalor::getInstrumentDisplayName (instrument))
                + " at " + std::to_string (static_cast<int> (sampleRate)) + " Hz";
            expect (metrics.finite, label + " produced NaN or infinity");
            expect (metrics.nonZeroCount > 10, label + " was silent");
            expect (metrics.rms() > 1.0e-7, label + " RMS was effectively silent");
            expect (metrics.peak <= 1.001, label + " exceeded the output safety bound");
        }
    }
}

void testModalSampleRateConsistency()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };
    constexpr std::array modalInstruments {
        drumalor::Instrument::Ride,
        drumalor::Instrument::Crash,
        drumalor::Instrument::Perc2
    };

    for (const auto instrument : modalInstruments)
    {
        std::array<double, sampleRates.size()> levels {};
        for (std::size_t rateIndex = 0; rateIndex < sampleRates.size(); ++rateIndex)
        {
            const auto rate = sampleRates[rateIndex];
            drumalor::DrumEngine engine;
            engine.prepare (rate, defaultBlockSize);
            engine.setInstrumentParameters (
                instrument, drumalor::getInstrumentMetadata (instrument).defaultParameters);

            // Multiple deterministic strikes exercise several PRNG streams,
            // while the low velocity keeps the output stage effectively linear.
            for (int hit = 0; hit < 4; ++hit)
                engine.trigger (instrument, 0.20f);

            const auto metrics = renderMetrics (
                engine, static_cast<int> (0.5 * rate), defaultBlockSize);
            expect (metrics.finite && metrics.rms() > 1.0e-7,
                    std::string (drumalor::getInstrumentDisplayName (instrument))
                        + " modal consistency probe was invalid or silent");
            levels[rateIndex] = metrics.rms();
        }

        expect (decibelSpread (levels) < 1.25,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " modal level changed by more than 1.25 dB across sample rates");
    }

    // Perc 2 can excite its modes with a single impulse. With the stochastic
    // click removed, this directly guards the resonator normalization itself.
    std::array<double, sampleRates.size()> impulseLevels {};
    for (std::size_t rateIndex = 0; rateIndex < sampleRates.size(); ++rateIndex)
    {
        const auto rate = sampleRates[rateIndex];
        drumalor::DrumEngine engine;
        engine.prepare (rate, defaultBlockSize);
        auto parameters = drumalor::getInstrumentMetadata (
            drumalor::Instrument::Perc2).defaultParameters;
        parameters.characterB = 0.0f;
        engine.setInstrumentParameters (drumalor::Instrument::Perc2, parameters);
        engine.trigger (drumalor::Instrument::Perc2, 0.85f);
        impulseLevels[rateIndex] = renderMetrics (
            engine, static_cast<int> (0.5 * rate), defaultBlockSize).rms();
    }
    expect (decibelSpread (impulseLevels) < 0.25,
            "Perc 2 resonator impulse changed by more than 0.25 dB across sample rates");
}

// Every noise layer in the kit - the kick click, the snare wires and snap, the
// clap bursts, the stick skin on a tom, the shaker grain and the Perc 1 click -
// is heard through a filter whose bandwidth is fixed in hertz, so what reaches
// the listener is the noise's power *density*. A generator that spreads a fixed
// variance across the whole Nyquist band therefore loses 3 dB of that density
// per doubling of the host sample rate. Measured before the engine moved those
// layers onto a fixed 48 kHz grid, the clap lost 5.9 dB of level and the snare's
// spectral centroid fell from 1.9 kHz to 0.9 kHz between 44.1 and 192 kHz.
void testNoiseDensityAcrossSampleRates()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };
    struct NoiseLayer
    {
        drumalor::Instrument instrument;
        double lowFrequency;
        double highFrequency;
        double toleranceDecibels;
        // Character position that opens this voice's noise layer up furthest,
        // so the measurement is dominated by the layer under test.
        float characterA;
        float characterB;
    };
    // The bands isolate each voice's filtered noise layer from its tonal core.
    // These four cover the distinct ways the kit consumes noise - a bandpassed
    // click, wires plus snap, a twice-filtered burst and a grain train - and the
    // unnormalised generator failed every one of them by 2.9 to 6.4 dB. Voices
    // whose noise sits far under their tonal core (the toms, the hats, Perc 1)
    // share these code paths but cannot measure them, so they are not listed.
    constexpr std::array layers {
        NoiseLayer { drumalor::Instrument::Kick,      2000.0,  8000.0, 2.0, 1.0f, 0.42f },
        NoiseLayer { drumalor::Instrument::Snare,     2000.0,  9000.0, 2.0, 0.62f, 1.0f },
        NoiseLayer { drumalor::Instrument::Clap,      1000.0,  6000.0, 2.0, 0.48f, 0.62f },
        NoiseLayer { drumalor::Instrument::Shaker,    4000.0, 14000.0, 2.5, 0.62f, 0.62f },
    };

    for (const auto& layer : layers)
    {
        const auto name = std::string (
            drumalor::getInstrumentDisplayName (layer.instrument));
        std::array<double, sampleRates.size()> levels {};
        for (std::size_t rateIndex = 0; rateIndex < sampleRates.size(); ++rateIndex)
        {
            const auto rate = sampleRates[rateIndex];
            auto parameters = drumalor::getInstrumentMetadata (
                layer.instrument).defaultParameters;
            parameters.characterA = layer.characterA;
            parameters.characterB = layer.characterB;
            const auto mono = renderMonoSequence (
                rate, layer.instrument, parameters, 8, 0.20, 0.90f);
            levels[rateIndex] = std::sqrt (bandPowerInRange (
                mono, rate, layer.lowFrequency, layer.highFrequency, 0.0,
                static_cast<double> (mono.size()) / rate));
            expect (levels[rateIndex] > 1.0e-7,
                    name + " noise-density probe was silent at "
                        + std::to_string (static_cast<int> (rate)) + " Hz");
        }
        expect (decibelSpread (levels) < layer.toleranceDecibels,
                name + " filtered noise level moved by "
                    + std::to_string (decibelSpread (levels))
                    + " dB across sample rates (" + describeValues (levels) + ")");
    }
}

void testTailsTerminate()
{
    constexpr double sampleRate = 48000.0;
    const int limit = static_cast<int> (sampleRate * drumalor::maximumTailSeconds);
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        drumalor::DrumEngine engine;
        engine.prepare (sampleRate, defaultBlockSize);
        auto parameters = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        parameters.decay = 1.0f;
        engine.setInstrumentParameters (instrument, parameters);
        engine.trigger (instrument, 0.8f);
        const auto boundaryAudio = renderMetrics (engine, limit, 389);
        expect (engine.getActiveVoiceCount() == 0,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " remained active at the advertised tail boundary");
        expect (boundaryAudio.finite,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " produced invalid audio while approaching the tail boundary");

        const auto afterTail = renderMetrics (engine, 4096, 251);
        expect (afterTail.finite && afterTail.peak < 1.0e-7 && afterTail.rms() < 1.0e-8,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " emitted non-silent audio after the advertised tail boundary");
    }
}

void testHatChokeAndPanic()
{
    constexpr double sampleRate = 48000.0;
    drumalor::DrumEngine engine;
    engine.prepare (sampleRate, defaultBlockSize);
    auto openParameters = drumalor::getInstrumentMetadata (drumalor::Instrument::OpenHat).defaultParameters;
    openParameters.decay = 1.0f;
    engine.setInstrumentParameters (drumalor::Instrument::OpenHat, openParameters);
    engine.trigger (drumalor::Instrument::OpenHat, 1.0f);
    renderMetrics (engine, static_cast<int> (0.025 * sampleRate));
    expect (engine.getActiveVoiceCount() == 1, "open hat stopped before choke test");

    engine.trigger (drumalor::Instrument::ClosedHat, 0.8f);
    expect (engine.getActiveVoiceCount() == 2, "hat retrigger did not create the new voice");
    renderMetrics (engine, static_cast<int> (0.010 * sampleRate));
    expect (engine.getActiveVoiceCount() == 1, "closed hat did not choke open hat quickly");

    engine.trigger (drumalor::Instrument::Crash, 0.9f);
    engine.trigger (drumalor::Instrument::Ride, 0.9f);
    engine.allSoundsOff();
    const auto panicTail = renderMetrics (engine, static_cast<int> (0.015 * sampleRate));
    expect (panicTail.finite, "allSoundsOff produced invalid audio");
    expect (engine.getActiveVoiceCount() == 0, "allSoundsOff did not clear voices");
}

void testDeterminismAndBlockPartitioning()
{
    constexpr int samples = 24000;
    drumalor::DrumEngine first;
    drumalor::DrumEngine second;
    first.prepare (48000.0, 512);
    second.prepare (48000.0, 512);
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        first.trigger (instrument, 0.35f + 0.04f * static_cast<float> (index));
        second.trigger (instrument, 0.35f + 0.04f * static_cast<float> (index));
    }
    const auto oneSampleBlocks = renderInterleaved (first, samples, 1);
    const auto irregularBlocks = renderInterleaved (second, samples, 383);
    expect (oneSampleBlocks == irregularBlocks,
            "render changed when the same stream was partitioned into different blocks");

    const auto renderStaggeredSequence = [] (int blockSize)
    {
        drumalor::DrumEngine engine;
        engine.prepare (48000.0, 512);
        std::vector<float> sequence;
        constexpr int spacingSamples = 997;
        sequence.reserve (drumalor::instrumentCount
                          * static_cast<std::size_t> (spacingSamples) * 2u);
        for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
        {
            engine.trigger (static_cast<drumalor::Instrument> (index), 0.73f);
            auto segment = renderInterleaved (engine, spacingSamples, blockSize);
            sequence.insert (sequence.end(), segment.begin(), segment.end());
        }
        return sequence;
    };
    expect (renderStaggeredSequence (1) == renderStaggeredSequence (383),
            "staggered organic hit sequence changed with process block partitioning");
}

void testFreeRunningMetallicOscillators()
{
    constexpr int renderedSamples = 2048;
    constexpr std::array silentOffsets { 137, 173, 251 };
    constexpr std::array instruments {
        drumalor::Instrument::ClosedHat,
        drumalor::Instrument::OpenHat,
        drumalor::Instrument::Ride,
        drumalor::Instrument::Crash,
        drumalor::Instrument::Perc1
    };

    for (const auto instrument : instruments)
    {
        bool observedAdvancedPhase = false;
        for (const int silentOffset : silentOffsets)
        {
            drumalor::DrumEngine immediate;
            drumalor::DrumEngine delayedSingleBlock;
            drumalor::DrumEngine delayedIrregularBlocks;
            for (auto* engine : { &immediate, &delayedSingleBlock,
                                  &delayedIrregularBlocks })
                engine->prepare (48000.0, 512);

            const auto silent = renderInterleaved (
                delayedSingleBlock, silentOffset, silentOffset);
            const auto irregularSilence = renderInterleaved (
                delayedIrregularBlocks, silentOffset, 37);
            expect (meanAbsoluteMagnitude (silent) == 0.0
                        && meanAbsoluteMagnitude (irregularSilence) == 0.0,
                    "free-running metallic banks leaked through their closed VCAs");

            immediate.trigger (instrument, 0.8f);
            delayedSingleBlock.trigger (instrument, 0.8f);
            delayedIrregularBlocks.trigger (instrument, 0.8f);
            const auto immediateHit = renderInterleaved (
                immediate, renderedSamples, defaultBlockSize);
            const auto delayedHit = renderInterleaved (
                delayedSingleBlock, renderedSamples, defaultBlockSize);
            const auto delayedPartitionedHit = renderInterleaved (
                delayedIrregularBlocks, renderedSamples, 113);

            expect (delayedHit == delayedPartitionedHit,
                    "free-running metallic state changed with block partitioning");
            const double difference = meanAbsoluteDifference (immediateHit, delayedHit);
            const double reference = std::max (meanAbsoluteMagnitude (immediateHit),
                                               meanAbsoluteMagnitude (delayedHit));
            observedAdvancedPhase = observedAdvancedPhase
                || difference > std::max (1.0e-7, 0.01 * reference);
        }

        expect (observedAdvancedPhase,
                "metallic oscillator bank restarted at trigger instead of free-running");
    }
}

void testPersistentMetallicParameterUpdates()
{
    constexpr auto instrument = drumalor::Instrument::ClosedHat;
    constexpr int silentSamples = 997;
    constexpr int renderedSamples = 2048;
    auto values = drumalor::getInstrumentMetadata (instrument).defaultParameters;
    values.pitch = 11.0f;
    values.characterA = 0.91f;

    drumalor::DrumEngine automatedSingleBlock;
    drumalor::DrumEngine automatedPartitioned;
    drumalor::DrumEngine automatedOnlyAtTrigger;
    for (auto* engine : { &automatedSingleBlock, &automatedPartitioned,
                          &automatedOnlyAtTrigger })
        engine->prepare (48000.0, 512);

    automatedSingleBlock.setInstrumentParameters (instrument, values);
    automatedPartitioned.setInstrumentParameters (instrument, values);
    renderInterleaved (automatedSingleBlock, silentSamples, silentSamples);
    renderInterleaved (automatedPartitioned, silentSamples, 37);
    renderInterleaved (automatedOnlyAtTrigger, silentSamples, 113);
    automatedOnlyAtTrigger.setInstrumentParameters (instrument, values);

    automatedSingleBlock.trigger (instrument, 0.8f);
    automatedPartitioned.trigger (instrument, 0.8f);
    automatedOnlyAtTrigger.trigger (instrument, 0.8f);
    const auto singleBlockHit = renderInterleaved (
        automatedSingleBlock, renderedSamples, defaultBlockSize);
    const auto partitionedHit = renderInterleaved (
        automatedPartitioned, renderedSamples, 113);
    const auto lateAutomationHit = renderInterleaved (
        automatedOnlyAtTrigger, renderedSamples, defaultBlockSize);

    expect (singleBlockHit == partitionedHit,
            "silent metallic automation changed with block partitioning");
    const double automationDifference = meanAbsoluteDifference (
        singleBlockHit, lateAutomationHit);
    const double automationReference = std::max (
        meanAbsoluteMagnitude (singleBlockHit),
        meanAbsoluteMagnitude (lateAutomationHit));
    expect (automationDifference > std::max (1.0e-7, 0.01 * automationReference),
            "silent automation did not retune the persistent metallic history");

    drumalor::DrumEngine restoredBeforePrepare;
    drumalor::DrumEngine changedAfterPrepare;
    restoredBeforePrepare.setInstrumentParameters (instrument, values);
    restoredBeforePrepare.prepare (48000.0, 512);
    changedAfterPrepare.prepare (48000.0, 512);
    changedAfterPrepare.setInstrumentParameters (instrument, values);
    restoredBeforePrepare.trigger (instrument, 0.8f);
    changedAfterPrepare.trigger (instrument, 0.8f);
    const auto restoredHit = renderInterleaved (
        restoredBeforePrepare, renderedSamples, defaultBlockSize);
    const auto changedAfterPrepareHit = renderInterleaved (
        changedAfterPrepare, renderedSamples, defaultBlockSize);
    const double restoreDifference = meanAbsoluteDifference (
        restoredHit, changedAfterPrepareHit);
    const double restoreReference = std::max (
        meanAbsoluteMagnitude (restoredHit),
        meanAbsoluteMagnitude (changedAfterPrepareHit));
    expect (restoreDifference > std::max (1.0e-7, 0.01 * restoreReference),
            "prepare ignored restored metallic parameters during history prefill");
}

void testOrganicAnalogVariation()
{
    constexpr double sampleRate = 48000.0;
    constexpr std::size_t hitCount = 6;
    constexpr float velocity = 0.81f;

    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const std::string label (drumalor::getInstrumentDisplayName (instrument));
        drumalor::DrumEngine engine;
        engine.prepare (sampleRate, defaultBlockSize);
        auto parameters = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        parameters.decay = 0.35f;
        engine.setInstrumentParameters (instrument, parameters);

        std::array<IsolatedHit, hitCount> firstPass;
        std::array<double, hitCount> rmsValues {};
        std::array<double, hitCount> peakValues {};
        std::array<int, hitCount> activeSamples {};
        for (std::size_t hit = 0; hit < hitCount; ++hit)
        {
            firstPass[hit] = renderIsolatedHit (engine, instrument, velocity);
            const auto& result = firstPass[hit];
            rmsValues[hit] = result.metrics.rms();
            peakValues[hit] = result.metrics.peak;
            activeSamples[hit] = result.activeSamples;

            expect (result.terminated, label + " organic hit exceeded the maximum tail");
            expect (result.metrics.finite, label + " organic hit produced NaN or infinity");
            expect (result.metrics.nonZeroCount > 10 && result.metrics.rms() > 1.0e-7,
                    label + " organic hit was effectively silent");
            expect (result.metrics.peak <= 1.001,
                    label + " organic hit exceeded the output safety bound");

            if (hit != 0)
            {
                const auto& previous = firstPass[hit - 1u].samples;
                const double difference = meanAbsoluteDifference (previous, result.samples);
                const double reference = std::max (meanAbsoluteMagnitude (previous),
                                                    meanAbsoluteMagnitude (result.samples));
                expect (difference > std::max (1.0e-7, 0.002 * reference),
                        label + " repeated an effectively identical equal-velocity hit");
            }
        }

        const double rmsSpreadDb = decibelSpread (rmsValues);
        const double peakSpreadDb = decibelSpread (peakValues);
        expect (rmsSpreadDb < 4.0,
                label + " organic RMS variation exceeded 4 dB (observed "
                    + std::to_string (rmsSpreadDb) + " dB from "
                    + describeValues (rmsValues) + ")");
        expect (peakSpreadDb < 6.0,
                label + " organic peak variation exceeded 6 dB (observed "
                    + std::to_string (peakSpreadDb) + " dB from "
                    + describeValues (peakValues) + ")");
        const auto [shortest, longest] = std::minmax_element (
            activeSamples.begin(), activeSamples.end());
        expect (static_cast<double> (*longest)
                    <= 1.35 * static_cast<double> (std::max (1, *shortest)) + 128.0,
                label + " organic tail duration varied by more than 35 percent");

        engine.reset();
        for (std::size_t hit = 0; hit < hitCount; ++hit)
        {
            const auto replay = renderIsolatedHit (engine, instrument, velocity);
            expect (replay.samples == firstPass[hit].samples,
                    label + " reset did not reproduce its modeled hit sequence exactly");
            expect (replay.activeSamples == firstPass[hit].activeSamples,
                    label + " reset did not reproduce its modeled tail sequence exactly");
        }
    }
}

void testDeepAnalogKickContract()
{
    constexpr std::array sampleRates { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 };
    const auto defaults = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Kick).defaultParameters;
    std::array<double, sampleRates.size()> fundamentals {};
    std::array<double, sampleRates.size()> rmsLevels {};
    std::array<double, sampleRates.size()> peaks {};
    std::array<double, sampleRates.size()> strongDriveLevels {};

    for (std::size_t index = 0; index < sampleRates.size(); ++index)
    {
        const double sampleRate = sampleRates[index];
        const auto samples = renderMonoHit (
            sampleRate, drumalor::Instrument::Kick, defaults);
        const auto metrics = analyseKick (samples, sampleRate);
        fundamentals[index] = metrics.fundamentalHz;
        rmsLevels[index] = metrics.fullRms;
        peaks[index] = metrics.peak;
        const double subToMid = metrics.lowRms / std::max (1.0e-12, metrics.midRms);
        const double transientHighShare = metrics.transientHighRms
            / std::max (1.0e-12, metrics.transientRms);
        const double crest = metrics.peak / std::max (1.0e-12, metrics.fullRms);
        const std::string label = "default Kick at "
            + std::to_string (static_cast<int> (sampleRate)) + " Hz";

        expect (metrics.finite, label + " produced NaN or infinity");
        expect (metrics.fundamentalHz >= 43.0 && metrics.fundamentalHz <= 55.0,
                label + " settled outside the 43-55 Hz deep-kick range (observed "
                    + std::to_string (metrics.fundamentalHz) + " Hz)");
        expect (metrics.fullRms >= 0.040,
                label + " lacked sustained body (RMS "
                    + std::to_string (metrics.fullRms) + ")");
        expect (metrics.peak >= 0.25 && metrics.peak <= 1.001,
                label + " did not maintain a strong, safe peak (observed "
                    + std::to_string (metrics.peak) + ")");
        expect (subToMid >= 1.80,
                label + " lacked dominant sub-100 Hz energy (sub/mid RMS ratio "
                    + std::to_string (subToMid) + ")");
        expect (transientHighShare >= 0.003 && transientHighShare <= 0.35,
                label + " transient was missing or excessively bright (high-band share "
                    + std::to_string (transientHighShare) + ")");
        expect (crest >= 2.5 && crest <= 8.0,
                label + " had an implausible crest factor (observed "
                    + std::to_string (crest) + ")");
        expect (std::abs (metrics.mean) <= 0.002,
                label + " retained excessive DC (mean "
                    + std::to_string (metrics.mean) + ")");

        auto strongDrive = defaults;
        strongDrive.characterB = 1.0f;
        const auto strongDriveMetrics = analyseKick (renderMonoHit (
            sampleRate, drumalor::Instrument::Kick, strongDrive), sampleRate);
        strongDriveLevels[index] = strongDriveMetrics.fullRms;
        expect (strongDriveMetrics.finite && strongDriveMetrics.fullRms >= 0.035
                    && strongDriveMetrics.peak >= 0.25
                    && strongDriveMetrics.peak <= 1.001
                    && std::abs (strongDriveMetrics.mean) <= 0.002,
                "strong-drive " + label
                    + " was unstable, weak, clipped, or DC-biased (RMS/peak/mean "
                    + std::to_string (strongDriveMetrics.fullRms) + "/"
                    + std::to_string (strongDriveMetrics.peak) + "/"
                    + std::to_string (strongDriveMetrics.mean) + ")");
    }

    expect (decibelSpread (rmsLevels) < 0.75,
            "default Kick body varied by more than 0.75 dB across sample rates ("
                + describeValues (rmsLevels) + ")");
    expect (decibelSpread (peaks) < 0.75,
            "default Kick peak varied by more than 0.75 dB across sample rates ("
                + describeValues (peaks) + ")");
    expect (decibelSpread (strongDriveLevels) < 0.75,
            "strong-drive Kick body varied by more than 0.75 dB across sample rates ("
                + describeValues (strongDriveLevels) + ")");
    const auto [minimumFundamental, maximumFundamental] = std::minmax_element (
        fundamentals.begin(), fundamentals.end());
    expect (*maximumFundamental - *minimumFundamental < 0.25,
            "default Kick fundamental moved by 0.25 Hz or more across sample rates ("
                + describeValues (fundamentals) + ")");

    auto lowPunch = defaults;
    auto highPunch = defaults;
    lowPunch.characterA = 0.0f;
    highPunch.characterA = 1.0f;
    const auto lowPunchMetrics = analyseKick (
        renderMonoHit (48000.0, drumalor::Instrument::Kick, lowPunch), 48000.0);
    const auto highPunchMetrics = analyseKick (
        renderMonoHit (48000.0, drumalor::Instrument::Kick, highPunch), 48000.0);
    expect (highPunchMetrics.transientHighRms
                > 1.75 * lowPunchMetrics.transientHighRms,
            "Kick Punch did not materially strengthen the high-frequency attack (low/high "
                + std::to_string (lowPunchMetrics.transientHighRms) + "/"
                + std::to_string (highPunchMetrics.transientHighRms) + ")");

    auto lowDrive = defaults;
    auto highDrive = defaults;
    lowDrive.characterB = 0.0f;
    highDrive.characterB = 1.0f;
    const auto lowDriveSamples = renderMonoHit (
        48000.0, drumalor::Instrument::Kick, lowDrive);
    const auto highDriveSamples = renderMonoHit (
        48000.0, drumalor::Instrument::Kick, highDrive);
    const double lowDriveFundamental = estimateSettledFundamental (
        lowDriveSamples, 48000.0, 35.0, 70.0);
    const double highDriveFundamental = estimateSettledFundamental (
        highDriveSamples, 48000.0, 35.0, 70.0);
    const double lowDriveHarmonicRatio = windowedMagnitude (
        lowDriveSamples, 48000.0, 2.0 * lowDriveFundamental)
        / std::max (1.0e-12, windowedMagnitude (
            lowDriveSamples, 48000.0, lowDriveFundamental));
    const double highDriveHarmonicRatio = windowedMagnitude (
        highDriveSamples, 48000.0, 2.0 * highDriveFundamental)
        / std::max (1.0e-12, windowedMagnitude (
            highDriveSamples, 48000.0, highDriveFundamental));
    const double lowDriveRichness = harmonicRichness (
        lowDriveSamples, 48000.0, lowDriveFundamental);
    const double highDriveRichness = harmonicRichness (
        highDriveSamples, 48000.0, highDriveFundamental);
    const auto highDriveHighBand = filterForAnalysis (
        highDriveSamples, 48000.0, 8000.0, 0.0);
    constexpr std::size_t settledBegin = 4800u;
    constexpr std::size_t settledEnd = 20160u;
    const double settledHighBandShare = rmsInRange (
        highDriveHighBand, settledBegin, settledEnd)
        / std::max (1.0e-12, rmsInRange (
            highDriveSamples, settledBegin, settledEnd));
    expect (highDriveHarmonicRatio > lowDriveHarmonicRatio + 0.020
                && highDriveHarmonicRatio > 2.0 * lowDriveHarmonicRatio,
            "Kick Drive did not materially enrich its second harmonic (low/high ratio "
                + std::to_string (lowDriveHarmonicRatio) + "/"
                + std::to_string (highDriveHarmonicRatio) + ")");
    expect (highDriveRichness >= 0.030 && highDriveRichness <= 0.35
                && highDriveRichness > lowDriveRichness + 0.020,
            "Kick Drive did not provide controlled 2nd-6th harmonic translation (low/high "
                + std::to_string (lowDriveRichness) + "/"
                + std::to_string (highDriveRichness) + ")");
    expect (settledHighBandShare < 0.003,
            "strong-drive Kick produced excessive settled energy above 8 kHz (share "
                + std::to_string (settledHighBandShare) + ")");

    auto lowPitch = defaults;
    auto highPitch = defaults;
    lowPitch.pitch = -12.0f;
    highPitch.pitch = 12.0f;
    const double lowFrequency = estimateSettledFundamental (
        renderMonoHit (48000.0, drumalor::Instrument::Kick, lowPitch),
        48000.0, 20.0, 40.0);
    const double centreFrequency = fundamentals[2];
    const double highFrequency = estimateSettledFundamental (
        renderMonoHit (48000.0, drumalor::Instrument::Kick, highPitch),
        48000.0, 75.0, 130.0);
    const double lowerRatio = lowFrequency / std::max (1.0e-12, centreFrequency);
    const double upperRatio = highFrequency / std::max (1.0e-12, centreFrequency);
    expect (lowFrequency >= 21.5 && lowFrequency <= 29.0
                && highFrequency >= 90.0 && highFrequency <= 115.0,
            "Kick octave offsets left the useful bass range (low/centre/high "
                + std::to_string (lowFrequency) + "/"
                + std::to_string (centreFrequency) + "/"
                + std::to_string (highFrequency) + " Hz)");
    expect (lowerRatio >= 0.48 && lowerRatio <= 0.52
                && upperRatio >= 1.94 && upperRatio <= 2.06,
            "Kick pitch did not track semitone ratios accurately (ratios "
                + std::to_string (lowerRatio) + "/"
                + std::to_string (upperRatio) + ")");
}

void testLowFrequencyTailAndVoiceStealing()
{
    constexpr double sampleRate = 48000.0;
    constexpr int recentWindowSamples = 2400; // 50 ms, longer than a 12 Hz peak interval.
    drumalor::DrumEngine lowKick;
    lowKick.prepare (sampleRate, 1);
    auto kickParameters = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Kick).defaultParameters;
    kickParameters.pitch = -24.0f;
    kickParameters.decay = 1.0f;
    lowKick.setInstrumentParameters (drumalor::Instrument::Kick, kickParameters);
    lowKick.trigger (drumalor::Instrument::Kick, 1.0f);

    std::array<float, recentWindowSamples> recentMagnitudes {};
    std::size_t recentIndex = 0;
    float finalLeft = 0.0f;
    float finalRight = 0.0f;
    int rendered = 0;
    const int hardLimit = static_cast<int> (sampleRate * drumalor::maximumTailSeconds);
    while (lowKick.getActiveVoiceCount() > 0 && rendered < hardLimit)
    {
        lowKick.process (&finalLeft, &finalRight, 1);
        recentMagnitudes[recentIndex] = std::max (std::abs (finalLeft), std::abs (finalRight));
        recentIndex = (recentIndex + 1u) % recentMagnitudes.size();
        ++rendered;
    }

    const auto recentPeak = *std::max_element (
        recentMagnitudes.begin(), recentMagnitudes.end());
    float afterLeft = 0.0f;
    float afterRight = 0.0f;
    lowKick.process (&afterLeft, &afterRight, 1);
    expect (recentPeak < 1.2e-5f,
            "minimum-pitch Kick was cleared while an audible low-frequency peak remained");
    expect (std::max (std::abs (afterLeft - finalLeft), std::abs (afterRight - finalRight))
                < 1.0e-5f,
            "minimum-pitch Kick tail ended with a discontinuity");

    // Twelve same-sample replacements exceed the old four-slot retirement
    // edge case. At very low levels the output stage is effectively linear,
    // so old + new renders should equal the saturated render on its first sample.
    drumalor::DrumEngine oldOnly;
    drumalor::DrumEngine saturated;
    drumalor::DrumEngine newOnly;
    for (auto* engine : { &oldOnly, &saturated, &newOnly })
        engine->prepare (sampleRate, 256);

    auto crashParameters = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Crash).defaultParameters;
    crashParameters.decay = 1.0f;
    oldOnly.setInstrumentParameters (drumalor::Instrument::Crash, crashParameters);
    saturated.setInstrumentParameters (drumalor::Instrument::Crash, crashParameters);
    for (int voice = 0; voice < 64; ++voice)
    {
        oldOnly.trigger (drumalor::Instrument::Crash, 0.0005f);
        saturated.trigger (drumalor::Instrument::Crash, 0.0005f);
    }

    std::array<float, 256> scratchLeft {};
    std::array<float, 256> scratchRight {};
    oldOnly.process (scratchLeft.data(), scratchRight.data(), 256);
    saturated.process (scratchLeft.data(), scratchRight.data(), 256);
    newOnly.process (scratchLeft.data(), scratchRight.data(), 256);

    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        if (instrument == drumalor::Instrument::Crash)
            continue;
        saturated.trigger (instrument, 0.001f);
        newOnly.trigger (instrument, 0.001f);
    }

    constexpr int replacementCount = static_cast<int> (drumalor::instrumentCount) - 1;
    expect (saturated.getActiveVoiceCount() == 64 + replacementCount,
            "active voice count omitted audible retirement-fade tails after stealing");

    float oldLeft = 0.0f;
    float oldRight = 0.0f;
    float saturatedLeft = 0.0f;
    float saturatedRight = 0.0f;
    float newLeft = 0.0f;
    float newRight = 0.0f;
    oldOnly.process (&oldLeft, &oldRight, 1);
    saturated.process (&saturatedLeft, &saturatedRight, 1);
    newOnly.process (&newLeft, &newRight, 1);
    const float stealingDifference = std::max (
        std::abs (saturatedLeft - oldLeft - newLeft),
        std::abs (saturatedRight - oldRight - newRight));
    expect (stealingDifference < 1.0e-6f,
            "same-sample voice stealing dropped an audible tail without a fade (difference "
                + std::to_string (stealingDifference) + ")");

    for (int trigger = 0; trigger < 128; ++trigger)
        saturated.trigger (static_cast<drumalor::Instrument> (
                               static_cast<std::size_t> (trigger) % drumalor::instrumentCount),
                           0.5f);
    expect (saturated.getActiveVoiceCount() == 128,
            "same-sample trigger burst did not report both primary and fading voices");
    const auto burstMetrics = renderMetrics (saturated, 1024, 127);
    expect (burstMetrics.finite && burstMetrics.peak <= 1.001,
            "same-sample trigger burst produced unsafe audio");
}

void testCymbalQualityContract()
{
    const auto rideDefaults = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Ride).defaultParameters;
    const auto crashDefaults = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Crash).defaultParameters;
    const auto ride = analyseCymbalPreset (drumalor::Instrument::Ride, rideDefaults);
    const auto crash = analyseCymbalPreset (drumalor::Instrument::Crash, crashDefaults);

    expect (ride.finite && ride.rms > 1.0e-5,
            "Ride cymbal quality probe was invalid or silent");
    expect (crash.finite && crash.rms > 1.0e-5,
            "Crash cymbal quality probe was invalid or silent");

    // These are deliberately broad regions rather than a golden spectrum.
    // They reject the former split between low narrow modes and a detached
    // high noise peak, while leaving room for either 808-style oscillators,
    // 909-style noisy grain, or a musically useful blend of both.
    expect (ride.presenceShare >= 0.19,
            "Ride lacks 0.9-5 kHz presence (share "
                + std::to_string (ride.presenceShare) + ")");
    expect (ride.lowShare <= 0.62,
            "Ride is dominated by low ringing modes (share "
                + std::to_string (ride.lowShare) + ")");
    expect (ride.airShare >= 0.10,
            "Ride has no sustained metallic air (share "
                + std::to_string (ride.airShare) + ")");
    expect (crash.presenceShare >= 0.23,
            "Crash lacks 0.9-5 kHz body (share "
                + std::to_string (crash.presenceShare) + ")");
    expect (crash.airShare >= 0.30 && crash.airShare <= 0.72,
            "Crash air/body balance is hollow or dull (air share "
                + std::to_string (crash.airShare) + ")");
    expect (ride.logBandEntropy >= 0.70 && ride.activeBandFraction >= 0.60,
            "Ride spectrum collapsed onto too few persistent modes");
    expect (crash.logBandEntropy >= 0.70 && crash.activeBandFraction >= 0.60,
            "Crash spectrum collapsed onto too few persistent modes");

    // Machine chooses between the two circuits rather than mixing a third
    // sound out of them, so both ends have to stand on their own: a cymbal at
    // either extreme must be a complete, usable cymbal, not a half-empty
    // version of the middle.
    for (const auto instrument : { drumalor::Instrument::Ride,
                                   drumalor::Instrument::Crash })
    {
        const std::string label (drumalor::getInstrumentDisplayName (instrument));
        auto defaults = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        auto analogueOnly = defaults;
        auto digitalOnly = defaults;
        analogueOnly.characterA = 0.0f;
        digitalOnly.characterA = 1.0f;
        const auto analogue = analyseCymbalPreset (instrument, analogueOnly);
        const auto digital = analyseCymbalPreset (instrument, digitalOnly);

        expect (analogue.airShare >= 0.10 && digital.airShare >= 0.10,
                label + " Machine left one end without metallic air (analogue/digital "
                    + std::to_string (analogue.airShare) + "/"
                    + std::to_string (digital.airShare) + ")");
        expect (analogue.logBandEntropy >= 0.65 && digital.logBandEntropy >= 0.65,
                label + " Machine produced a sparse, clingy tail at one end");

        // Choosing a machine is not choosing a volume. Both were levelled at
        // their own output stages, and a control that changed loudness would
        // be useless for comparing them.
        const double levelChangeDb = std::abs (20.0 * std::log10 (
            digital.rms / std::max (1.0e-12, analogue.rms)));
        expect (levelChangeDb <= 2.0,
                label + " Machine changed level rather than character (observed "
                    + std::to_string (levelChangeDb) + " dB)");

        // ...but it must change something, or it is not selecting anything.
        // The two circuits share no source, so their spectra have no reason to
        // agree, and a control that left the spectrum alone would mean the
        // decorrelation had been lost again.
        const double balanceChangeDb = std::abs (10.0 * std::log10 (
            (digital.airShare / std::max (1.0e-12, digital.bodyShare))
            / std::max (1.0e-12,
                        analogue.airShare / std::max (1.0e-12, analogue.bodyShare))));
        const bool movedPersistence = std::abs (digital.tonalPersistence
                                                - analogue.tonalPersistence)
            >= 0.04 * std::max (digital.tonalPersistence, analogue.tonalPersistence);
        expect (balanceChangeDb >= 1.0 || movedPersistence,
                label + " Machine did not distinguish the two circuits (balance "
                    + std::to_string (balanceChangeDb) + " dB)");
    }

    // Tone/Brightness must move spectral balance, not merely produce a
    // numerically different waveform as the generic parameter test checks.
    auto darkRideParameters = rideDefaults;
    auto brightRideParameters = rideDefaults;
    darkRideParameters.characterB = 0.0f;
    brightRideParameters.characterB = 1.0f;
    const auto darkRide = analyseCymbalPreset (
        drumalor::Instrument::Ride, darkRideParameters);
    const auto brightRide = analyseCymbalPreset (
        drumalor::Instrument::Ride, brightRideParameters);
    const double rideToneChangeDb = 10.0 * std::log10 (
        (brightRide.airShare / std::max (1.0e-12, brightRide.bodyShare))
        / std::max (1.0e-12,
                    darkRide.airShare / std::max (1.0e-12, darkRide.bodyShare)));
    expect (rideToneChangeDb >= 3.0,
            "Ride Tone changed air/body balance by less than 3 dB (observed "
                + std::to_string (rideToneChangeDb) + " dB)");

    auto darkCrashParameters = crashDefaults;
    auto brightCrashParameters = crashDefaults;
    darkCrashParameters.characterB = 0.0f;
    brightCrashParameters.characterB = 1.0f;
    const auto darkCrash = analyseCymbalPreset (
        drumalor::Instrument::Crash, darkCrashParameters);
    const auto brightCrash = analyseCymbalPreset (
        drumalor::Instrument::Crash, brightCrashParameters);
    const double crashBrightnessChangeDb = 10.0 * std::log10 (
        (brightCrash.airShare / std::max (1.0e-12, brightCrash.bodyShare))
        / std::max (1.0e-12,
                    darkCrash.airShare / std::max (1.0e-12, darkCrash.bodyShare)));
    expect (crashBrightnessChangeDb >= 3.0,
            "Crash Brightness changed air/body balance by less than 3 dB (observed "
                + std::to_string (crashBrightnessChangeDb) + " dB)");

    // The level match has to hold across the whole sweep, not only at its two
    // ends: an equal-power crossfade between two uncorrelated sources should
    // not dip in the middle, and a dip there would be the audible sign that
    // the two channels had become correlated again.
    for (const auto instrument : { drumalor::Instrument::Ride,
                                   drumalor::Instrument::Crash })
    {
        const std::string label (drumalor::getInstrumentDisplayName (instrument));
        auto sweep = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        double quietest = std::numeric_limits<double>::max();
        double loudest = 0.0;
        for (int step = 0; step <= 8; ++step)
        {
            sweep.characterA = static_cast<float> (step) / 8.0f;
            const auto measured = analyseCymbalPreset (instrument, sweep);
            quietest = std::min (quietest, measured.rms);
            loudest = std::max (loudest, measured.rms);
        }
        const double sweepRangeDb = 20.0 * std::log10 (
            loudest / std::max (1.0e-12, quietest));
        expect (sweepRangeDb <= 2.5,
                label + " Machine sweep was not level (range "
                    + std::to_string (sweepRangeDb) + " dB)");
    }
}

std::vector<float> renderCymbalMono (drumalor::Instrument instrument,
                                     drumalor::InstrumentParameters parameters,
                                     double seconds, float velocity,
                                     double sampleRate)
{
    drumalor::DrumEngine engine;
    engine.prepare (sampleRate, defaultBlockSize);
    engine.setInstrumentParameters (instrument, parameters);
    engine.trigger (instrument, velocity);
    const int count = static_cast<int> (seconds * sampleRate);
    const auto interleaved = renderInterleaved (engine, count, defaultBlockSize);
    std::vector<float> mono (static_cast<std::size_t> (count));
    for (int sample = 0; sample < count; ++sample)
    {
        const auto index = static_cast<std::size_t> (sample) * 2u;
        mono[static_cast<std::size_t> (sample)] = 0.5f
            * (interleaved[index] + interleaved[index + 1u]);
    }
    return mono;
}

double rmsWindow (const std::vector<float>& samples, double sampleRate,
                  double beginSeconds, double endSeconds)
{
    return rmsInRange (
        samples,
        std::min (samples.size(),
                  static_cast<std::size_t> (beginSeconds * sampleRate)),
        std::min (samples.size(),
                  static_cast<std::size_t> (endSeconds * sampleRate)));
}

// Power-weighted third-octave centroid. A cymbal's is supposed to fall while it
// rings; a fader closing on a fixed spectrum keeps it where it started.
double spectralCentroid (const std::vector<float>& samples, double sampleRate,
                         double beginSeconds, double endSeconds)
{
    double weighted = 0.0;
    double total = 0.0;
    for (int band = 0; band < 18; ++band)
    {
        const double lower = 300.0 * std::exp2 (band / 3.0);
        const double upper = 300.0 * std::exp2 ((band + 1) / 3.0);
        if (upper >= 0.45 * sampleRate)
            break;
        const double power = bandPowerInRange (
            samples, sampleRate, lower, upper, beginSeconds, endSeconds);
        weighted += power * std::sqrt (lower * upper);
        total += power;
    }
    return total > 0.0 ? weighted / total : 0.0;
}

double largestStepInRange (const std::vector<float>& samples, double sampleRate,
                           double beginSeconds, double endSeconds)
{
    const auto begin = std::min (
        samples.size(), static_cast<std::size_t> (beginSeconds * sampleRate));
    const auto end = std::min (
        samples.size(), static_cast<std::size_t> (endSeconds * sampleRate));
    double largest = 0.0;
    for (std::size_t sample = begin + 1u; sample < end; ++sample)
        largest = std::max (largest,
                            std::abs (static_cast<double> (samples[sample])
                                      - samples[sample - 1u]));
    return largest;
}

// Contracts for the circuit behaviours the cymbal channels model, as opposed
// to the spectral shape the quality contract above already covers. Each one
// fails for a specific missing block rather than for a general loss of
// realism: the attack smoother, the swing VCA's control law, the sample clock
// driving the address envelope, the closing VCA's bandwidth, and the band
// retirement that lets the analogue channel stop paying for shut VCAs.
void testCymbalCircuitContract()
{
    constexpr double sampleRate = 48000.0;
    for (const auto instrument : { drumalor::Instrument::Ride,
                                   drumalor::Instrument::Crash })
    {
        const std::string name { drumalor::getInstrumentDisplayName (instrument) };
        const auto defaults = drumalor::getInstrumentMetadata (
            instrument).defaultParameters;
        const auto rendered = renderCymbalMono (
            instrument, defaults, 2.2, 0.9f, sampleRate);

        // The trigger pulse reaches the envelope capacitors through an RC, so
        // the channel opens over a ramp. Without the attack smoother the first
        // sample is a step into a resonant band-pass.
        double onsetPeak = 0.0;
        std::size_t onsetPeakIndex = 0;
        const auto onsetLength = static_cast<std::size_t> (0.005 * sampleRate);
        for (std::size_t sample = 0; sample < onsetLength; ++sample)
            if (std::abs (rendered[sample]) > onsetPeak)
            {
                onsetPeak = std::abs (rendered[sample]);
                onsetPeakIndex = sample;
            }
        expect (onsetPeak > 1.0e-4,
                name + " produced no measurable onset");
        expect (std::abs (rendered[0]) <= 0.35 * onsetPeak,
                name + " opened its VCAs as a step rather than through the "
                       "attack smoother (first sample "
                    + std::to_string (std::abs (rendered[0])) + " of peak "
                    + std::to_string (onsetPeak) + ")");
        expect (onsetPeakIndex >= static_cast<std::size_t> (0.0005 * sampleRate),
                name + " reached its onset peak before the smoother could ramp");

        // A cymbal darkens as it rings, and both channels are built to do it:
        // the analogue one gives its top band the short envelope, and the
        // digital one loses bandwidth as its VCA's control current falls.
        const double early = spectralCentroid (rendered, sampleRate, 0.02, 0.10);
        const double late = spectralCentroid (rendered, sampleRate, 0.80, 1.60);
        expect (early > 0.0 && late > 0.0 && late <= 0.90 * early,
                name + " did not darken as it rang (centroid "
                    + std::to_string (early) + " Hz to "
                    + std::to_string (late) + " Hz)");

        // The address counter, not a clock of its own, steps the 909 channel's
        // envelope, so retuning the sample clock moves pitch and tail length
        // together. Both renders keep the same Decay setting; only the clock
        // that walks the ROM differs.
        auto lowClock = defaults;
        auto highClock = defaults;
        lowClock.pitch = -12.0f;
        highClock.pitch = 12.0f;
        const auto slow = renderCymbalMono (
            instrument, lowClock, 1.7, 0.9f, sampleRate);
        const auto fast = renderCymbalMono (
            instrument, highClock, 1.7, 0.9f, sampleRate);
        const double slowPersistence = rmsWindow (slow, sampleRate, 1.0, 1.6)
            / std::max (1.0e-9, rmsWindow (slow, sampleRate, 0.05, 0.15));
        const double fastPersistence = rmsWindow (fast, sampleRate, 1.0, 1.6)
            / std::max (1.0e-9, rmsWindow (fast, sampleRate, 0.05, 0.15));
        expect (slowPersistence >= 2.0 * fastPersistence,
                name + " sample clock did not carry the address envelope with "
                       "it (slow/fast tail persistence "
                    + std::to_string (slowPersistence) + "/"
                    + std::to_string (fastPersistence) + ")");

        // Accent is a voltage on the trigger line and the swing VCAs are
        // superlinear in it, so a quiet hit does not open them as far and
        // leaves sooner. A plain output-gain VCA would leave this unchanged.
        const auto soft = renderCymbalMono (
            instrument, defaults, 1.7, 0.2f, sampleRate);
        const auto hard = renderCymbalMono (
            instrument, defaults, 1.7, 1.0f, sampleRate);
        const double softPersistence = rmsWindow (soft, sampleRate, 1.0, 1.6)
            / std::max (1.0e-9, rmsWindow (soft, sampleRate, 0.02, 0.10));
        const double hardPersistence = rmsWindow (hard, sampleRate, 1.0, 1.6)
            / std::max (1.0e-9, rmsWindow (hard, sampleRate, 0.02, 0.10));
        expect (softPersistence <= 0.85 * hardPersistence,
                name + " velocity behaved as a plain output gain rather than "
                       "as accent into the swing VCAs (soft/hard tail "
                       "persistence "
                    + std::to_string (softPersistence) + "/"
                    + std::to_string (hardPersistence) + ")");

        // The analogue channel stops running the sections behind a shut VCA
        // partway through the tail. That boundary has to be inaudible, so the
        // decaying tail must not contain a larger sample-to-sample step than
        // the loud part of the hit already did.
        const double headStep = largestStepInRange (rendered, sampleRate, 0.02, 0.40);
        const double tailStep = largestStepInRange (rendered, sampleRate, 0.40, 2.20);
        expect (headStep > 0.0 && tailStep <= 0.75 * headStep,
                name + " tail contains a step the hit itself never made, which "
                       "is what retiring a band audibly would look like (head "
                    + std::to_string (headStep) + ", tail "
                    + std::to_string (tailStep) + ")");

        // The sample clock is fixed in hertz and the ROM's length in seconds,
        // so neither may follow the host rate.
        const double referenceRms = rmsWindow (rendered, sampleRate, 0.04, 0.75);
        for (const double rate : { 44100.0, 96000.0, 192000.0 })
        {
            const auto resampled = renderCymbalMono (
                instrument, defaults, 0.85, 0.9f, rate);
            const double rms = rmsWindow (resampled, rate, 0.04, 0.75);
            const double decibels = 20.0 * std::log10 (
                std::max (1.0e-12, rms) / std::max (1.0e-12, referenceRms));
            expect (std::abs (decibels) <= 1.5,
                    name + " level moved " + std::to_string (decibels)
                        + " dB at " + std::to_string (rate) + " Hz");
        }
    }
}

std::vector<float> renderWithParameters (drumalor::Instrument instrument,
                                         drumalor::InstrumentParameters parameters,
                                         int samples = 7200)
{
    drumalor::DrumEngine engine;
    engine.prepare (48000.0, defaultBlockSize);
    engine.setInstrumentParameters (instrument, parameters);
    engine.trigger (instrument, 0.85f);
    return renderInterleaved (engine, samples, defaultBlockSize);
}

void testParameterInfluence()
{
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto defaults = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        auto lowA = defaults;
        auto highA = defaults;
        lowA.characterA = 0.0f;
        highA.characterA = 1.0f;
        const double characterADifference = meanAbsoluteDifference (
            renderWithParameters (instrument, lowA), renderWithParameters (instrument, highA));
        expect (characterADifference > 1.0e-7,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " character A did not affect its sound");

        auto lowB = defaults;
        auto highB = defaults;
        lowB.characterB = 0.0f;
        highB.characterB = 1.0f;
        const double characterBDifference = meanAbsoluteDifference (
            renderWithParameters (instrument, lowB), renderWithParameters (instrument, highB));
        expect (characterBDifference > 1.0e-7,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " character B did not affect its sound");

        auto lowPitch = defaults;
        auto highPitch = defaults;
        lowPitch.pitch = -12.0f;
        highPitch.pitch = 12.0f;
        const auto lowPitchRender = renderWithParameters (instrument, lowPitch, 14400);
        const auto highPitchRender = renderWithParameters (instrument, highPitch, 14400);
        const double pitchDifference = meanAbsoluteDifference (lowPitchRender, highPitchRender);
        const double pitchReference = std::max (meanAbsoluteMagnitude (lowPitchRender),
                                                meanAbsoluteMagnitude (highPitchRender));
        expect (pitchDifference > std::max (1.0e-7, 0.02 * pitchReference),
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " pitch endpoints produced indistinguishable renders");

        auto shortDecay = defaults;
        auto longDecay = defaults;
        shortDecay.decay = 0.0f;
        longDecay.decay = 1.0f;
        drumalor::DrumEngine shortEngine;
        drumalor::DrumEngine longEngine;
        shortEngine.prepare (48000.0, defaultBlockSize);
        longEngine.prepare (48000.0, defaultBlockSize);
        shortEngine.setInstrumentParameters (instrument, shortDecay);
        longEngine.setInstrumentParameters (instrument, longDecay);
        shortEngine.trigger (instrument, 0.9f);
        longEngine.trigger (instrument, 0.9f);
        constexpr int tailLimit = static_cast<int> (
            48000.0 * drumalor::maximumTailSeconds);
        const int shortTailSamples = renderUntilInactive (shortEngine, tailLimit, 127);
        const int longTailSamples = renderUntilInactive (longEngine, tailLimit, 127);
        expect (longTailSamples > shortTailSamples + 4800
                    && longTailSamples > shortTailSamples * 3 / 2,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " decay did not materially extend its active tail");
    }
}

// Perc 1's second control is labelled Drive, and the voice already reserved a
// wider circuit-drive span than its neighbours for it - but the output stage's
// exact 1/drive compensation cancelled nearly all of it. Over the whole travel
// of the knob the render changed by 6.9 %, against a 90 % average for the other
// twenty-five character controls, and what little arrived came as a 0.9 dB level
// drop rather than as saturation. A drive control has to add density: the peak
// comes down while the level holds. The kick's identically labelled control is
// deliberately not measured here - it retunes the resonator's loss and body gain
// as well, so it is not a pure output-stage drive.
void testPerc1DriveAddsDensity()
{
    const auto probe = [] (float drive)
    {
        auto parameters = drumalor::getInstrumentMetadata (
            drumalor::Instrument::Perc1).defaultParameters;
        parameters.characterB = drive;
        return renderMonoHit (48000.0, drumalor::Instrument::Perc1, parameters, 0.30, 0.95f);
    };
    const auto clean = probe (0.0f);
    const auto driven = probe (1.0f);
    const double cleanPeak = peakInRange (clean, 0u, clean.size());
    const double drivenPeak = peakInRange (driven, 0u, driven.size());
    const double cleanRms = rmsInRange (clean, 0u, clean.size());
    const double drivenRms = rmsInRange (driven, 0u, driven.size());
    expect (cleanPeak > 1.0e-3 && cleanRms > 1.0e-4,
            "the Perc 1 drive probe produced no signal");

    const double levelChangeDb = 20.0 * std::log10 (drivenRms / cleanRms);
    const double crestReductionDb = 20.0 * std::log10 (cleanPeak / drivenPeak)
        + levelChangeDb;
    expect (crestReductionDb > 1.4,
            "Perc 1 Drive barely saturates across its whole travel (crest factor "
            "fell by only " + std::to_string (crestReductionDb) + " dB)");
    expect (std::abs (levelChangeDb) < 0.5,
            "Perc 1 Drive behaves as a level control rather than a saturation "
            "control (" + std::to_string (levelChangeDb) + " dB across its travel)");
}

std::vector<float> renderLevelProbe (drumalor::Instrument instrument,
                                     const drumalor::InstrumentParameters& parameters)
{
    drumalor::DrumEngine engine;
    engine.prepare (48000.0, defaultBlockSize);
    engine.setInstrumentParameters (instrument, parameters);
    engine.trigger (instrument, 0.12f);
    return renderInterleaved (engine, 9600, defaultBlockSize);
}

std::vector<float> renderKitHits (drumalor::Instrument instrument,
                                  const drumalor::InstrumentParameters& parameters,
                                  const drumalor::KitParameters& kit,
                                  int hitCount, int samplesPerHit,
                                  float velocity = 0.85f)
{
    drumalor::DrumEngine engine;
    engine.prepare (48000.0, defaultBlockSize);
    engine.setInstrumentParameters (instrument, parameters);
    engine.setKitParameters (kit);
    std::vector<float> result;
    result.reserve (static_cast<std::size_t> (hitCount)
                    * static_cast<std::size_t> (samplesPerHit) * 2u);
    for (int hit = 0; hit < hitCount; ++hit)
    {
        engine.trigger (instrument, velocity);
        auto segment = renderInterleaved (engine, samplesPerHit, defaultBlockSize);
        result.insert (result.end(), segment.begin(), segment.end());
    }
    return result;
}

// "Same but different", for every voice on the board.
//
// An analogue drum machine never plays a note twice. Two strikes of the same
// drum at the same velocity land on components that have drifted, oscillators
// that were somewhere else in their cycle, and a supply that has sagged since
// the last one - so the pair is recognisably one drum and measurably not one
// recording. Both halves of that need asserting: a kit that repeats exactly is
// a sampler, and a kit whose repeats wander is broken.
//
// This runs over all thirteen voices rather than a sampled few, because the
// property is meant to hold for the instrument, not for the voices that
// happened to be checked.
void testEveryVoiceVariesBetweenHits()
{
    constexpr int hitCount = 6;
    constexpr int samplesPerHit = 12000;
    const drumalor::KitParameters factory {};

    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const std::string label (drumalor::getInstrumentDisplayName (instrument));
        const auto defaults = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        const auto render = renderKitHits (
            instrument, defaults, factory, hitCount, samplesPerHit);
        const auto stride = static_cast<std::size_t> (samplesPerHit) * 2u;

        const auto segment = [&render, stride] (std::size_t hit)
        {
            return std::vector<float> (
                render.begin() + static_cast<std::ptrdiff_t> (hit * stride),
                render.begin() + static_cast<std::ptrdiff_t> ((hit + 1u) * stride));
        };
        const auto peak = [] (const std::vector<float>& x)
        {
            double highest = 0.0;
            for (const float value : x)
                highest = std::max (highest, std::abs (static_cast<double> (value)));
            return highest;
        };

        const auto meanAbsolute = [] (const std::vector<float>& x)
        {
            double total = 0.0;
            for (const float value : x)
                total += std::abs (static_cast<double> (value));
            return total / static_cast<double> (std::max<std::size_t> (1, x.size()));
        };

        double quietest = std::numeric_limits<double>::max();
        double loudest = 0.0;
        bool everyPairDiffered = true;
        double smallestRelativeChange = std::numeric_limits<double>::max();
        for (std::size_t hit = 0; hit + 1u < static_cast<std::size_t> (hitCount); ++hit)
        {
            const auto previous = segment (hit);
            const auto current = segment (hit + 1u);
            everyPairDiffered = everyPairDiffered && previous != current;
            const double reference = std::max (meanAbsolute (previous),
                                               meanAbsolute (current));
            smallestRelativeChange = std::min (
                smallestRelativeChange,
                meanAbsoluteDifference (previous, current)
                    / std::max (1.0e-12, reference));
            quietest = std::min (quietest, peak (current));
            loudest = std::max (loudest, peak (current));
        }
        expect (everyPairDiffered,
                label + " played two consecutive hits identically, which no "
                        "analogue voice does");

        // Not merely unequal in the last bit - different enough to hear. A
        // voice that repeated to within a fraction of a percent would satisfy
        // an exact comparison and still sound like one recording retriggered.
        expect (smallestRelativeChange >= 0.02,
                label + " repeated too closely between hits to be heard as "
                        "variation (smallest change "
                    + std::to_string (smallestRelativeChange) + " of level)");

        // Different, but the same drum: the spread across six strikes has to
        // stay inside what a machine's tolerances would explain. This is the
        // half that catches variation turning into sloppiness.
        const double spreadDb = 20.0 * std::log10 (
            loudest / std::max (1.0e-12, quietest));
        expect (spreadDb <= 6.0,
                label + " varied its level between hits by more than a machine "
                        "would (" + std::to_string (spreadDb) + " dB)");
    }
}

void testPerVoiceMixer()
{
    constexpr double sampleRate = 48000.0;
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const std::string label (drumalor::getInstrumentDisplayName (instrument));
        const auto defaults = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        expect (defaults.level == 0.0f,
                label + " does not default to unity channel level");
        expect (defaults.pan >= -1.0f && defaults.pan <= 1.0f,
                label + " default pan is out of range");
        expect (defaults.chokeGroup >= 0
                    && defaults.chokeGroup <= drumalor::chokeGroupCount,
                label + " default choke group is out of range");

        // The mixer must be a clean gain law: -6 dB has to halve the render.
        // The probe velocity is deliberately low, because the per-voice analogue
        // output stage is intentionally nonlinear at high levels and would make
        // a quieter channel measure slightly louder than a linear law predicts.
        auto quieter = defaults;
        quieter.level = -6.0f;
        const auto unityRender = renderLevelProbe (instrument, defaults);
        const auto quietRender = renderLevelProbe (instrument, quieter);
        const double unityLevel = meanAbsoluteMagnitude (unityRender);
        const double quietLevel = meanAbsoluteMagnitude (quietRender);
        expect (unityLevel > 1.0e-8, label + " unity-level render was silent");
        const double ratioDb = 20.0 * std::log10 (
            quietLevel / std::max (1.0e-14, unityLevel));
        expect (ratioDb < -5.2 && ratioDb > -6.8,
                label + " -6 dB channel level produced " + std::to_string (ratioDb)
                    + " dB instead of roughly -6 dB");

        // Hard-left and hard-right must swap the channel balance symmetrically.
        auto leftOnly = defaults;
        auto rightOnly = defaults;
        leftOnly.pan = -1.0f;
        rightOnly.pan = 1.0f;
        const auto leftRender = renderWithParameters (instrument, leftOnly, 9600);
        const auto rightRender = renderWithParameters (instrument, rightOnly, 9600);
        double leftOnLeft = 0.0;
        double leftOnRight = 0.0;
        double rightOnRight = 0.0;
        for (std::size_t sample = 0; sample + 1u < leftRender.size(); sample += 2u)
        {
            leftOnLeft += std::abs (static_cast<double> (leftRender[sample]));
            leftOnRight += std::abs (static_cast<double> (leftRender[sample + 1u]));
            rightOnRight += std::abs (static_cast<double> (rightRender[sample + 1u]));
        }
        expect (leftOnLeft > 40.0 * std::max (1.0e-9, leftOnRight),
                label + " hard-left pan still fed the right channel");
        expect (std::abs (leftOnLeft - rightOnRight)
                    <= 0.02 * std::max (leftOnLeft, rightOnRight),
                label + " pan law is not symmetric about centre");
    }

    // The default kit must still image exactly as the hard-coded positions did.
    constexpr std::array pannedInstruments {
        std::pair { drumalor::Instrument::ClosedHat, 0.16f },
        std::pair { drumalor::Instrument::Ride, 0.27f },
        std::pair { drumalor::Instrument::Crash, -0.27f },
        std::pair { drumalor::Instrument::LowTom, -0.20f },
        std::pair { drumalor::Instrument::Kick, 0.0f }
    };
    for (const auto& [instrument, expectedPan] : pannedInstruments)
        expect (std::abs (drumalor::getInstrumentMetadata (instrument)
                              .defaultParameters.pan - expectedPan) < 1.0e-6f,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " default pan changed from its original kit position");

    // A level of exactly 0 dB must not perturb a single sample.
    drumalor::DrumEngine untouched;
    drumalor::DrumEngine explicitlyUnity;
    untouched.prepare (sampleRate, defaultBlockSize);
    explicitlyUnity.prepare (sampleRate, defaultBlockSize);
    auto unityParameters = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Snare).defaultParameters;
    unityParameters.level = 0.0f;
    explicitlyUnity.setInstrumentParameters (drumalor::Instrument::Snare, unityParameters);
    untouched.trigger (drumalor::Instrument::Snare, 0.9f);
    explicitlyUnity.trigger (drumalor::Instrument::Snare, 0.9f);
    expect (renderInterleaved (untouched, 8192, defaultBlockSize)
                == renderInterleaved (explicitlyUnity, 8192, defaultBlockSize),
            "an explicit 0 dB channel level changed the render");
}

void testChokeGroups()
{
    constexpr double sampleRate = 48000.0;
    // Group A is the factory hi-hat pair, and must behave exactly as the old
    // hard-coded pedal link did.
    expect (drumalor::getInstrumentMetadata (drumalor::Instrument::ClosedHat)
                .defaultParameters.chokeGroup == 1
                && drumalor::getInstrumentMetadata (drumalor::Instrument::OpenHat)
                       .defaultParameters.chokeGroup == 1,
            "the hi-hat pair no longer shares the default choke group");

    // Any pair of voices can be linked. Ride cut by Crash is the classic case.
    drumalor::DrumEngine engine;
    engine.prepare (sampleRate, defaultBlockSize);
    auto rideParameters = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Ride).defaultParameters;
    auto crashParameters = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Crash).defaultParameters;
    rideParameters.decay = 1.0f;
    rideParameters.chokeGroup = 2;
    crashParameters.chokeGroup = 2;
    engine.setInstrumentParameters (drumalor::Instrument::Ride, rideParameters);
    engine.setInstrumentParameters (drumalor::Instrument::Crash, crashParameters);

    engine.trigger (drumalor::Instrument::Ride, 1.0f);
    renderMetrics (engine, static_cast<int> (0.030 * sampleRate));
    expect (engine.getActiveVoiceCount() == 1, "ride stopped before the choke test");
    engine.trigger (drumalor::Instrument::Crash, 0.9f);
    expect (engine.getActiveVoiceCount() == 2,
            "grouped crash did not create its own voice");
    const auto chokeAudio = renderMetrics (engine, static_cast<int> (0.010 * sampleRate));
    expect (chokeAudio.finite, "group choke produced invalid audio");
    expect (engine.getActiveVoiceCount() == 1,
            "a shared choke group did not cut the ringing ride");

    // Voices in different groups, or in no group, must not interfere.
    drumalor::DrumEngine independent;
    independent.prepare (sampleRate, defaultBlockSize);
    auto lowTom = drumalor::getInstrumentMetadata (
        drumalor::Instrument::LowTom).defaultParameters;
    auto highTom = drumalor::getInstrumentMetadata (
        drumalor::Instrument::HighTom).defaultParameters;
    lowTom.decay = 1.0f;
    lowTom.chokeGroup = 1;
    highTom.chokeGroup = 3;
    independent.setInstrumentParameters (drumalor::Instrument::LowTom, lowTom);
    independent.setInstrumentParameters (drumalor::Instrument::HighTom, highTom);
    independent.trigger (drumalor::Instrument::LowTom, 1.0f);
    renderMetrics (independent, static_cast<int> (0.030 * sampleRate));
    independent.trigger (drumalor::Instrument::HighTom, 0.9f);
    renderMetrics (independent, static_cast<int> (0.010 * sampleRate));
    expect (independent.getActiveVoiceCount() == 2,
            "voices in different choke groups cut each other");

    // A voice keeps the group it was born into, so retuning cannot strand it.
    drumalor::DrumEngine retuned;
    retuned.prepare (sampleRate, defaultBlockSize);
    auto grouped = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Perc2).defaultParameters;
    grouped.decay = 1.0f;
    grouped.chokeGroup = 3;
    retuned.setInstrumentParameters (drumalor::Instrument::Perc2, grouped);
    retuned.trigger (drumalor::Instrument::Perc2, 1.0f);
    renderMetrics (retuned, static_cast<int> (0.020 * sampleRate));
    auto ungrouped = grouped;
    ungrouped.chokeGroup = 0;
    retuned.setInstrumentParameters (drumalor::Instrument::Perc2, ungrouped);
    retuned.trigger (drumalor::Instrument::Perc2, 0.9f);
    renderMetrics (retuned, static_cast<int> (0.010 * sampleRate));
    expect (retuned.getActiveVoiceCount() == 2,
            "an ungrouped retrigger still cut the previously grouped tail");
}

void testHumaniseDepth()
{
    constexpr int hitCount = 6;
    constexpr int samplesPerHit = 7200;
    // Voices whose character comes from the free-running metallic banks are
    // deliberately excluded: their hit-to-hit difference is dominated by the
    // circuit phase they happen to sample, which Humanise does not control.
    constexpr std::array probeInstruments {
        drumalor::Instrument::Kick, drumalor::Instrument::LowTom,
        drumalor::Instrument::MidTom, drumalor::Instrument::HighTom,
        drumalor::Instrument::Snare, drumalor::Instrument::Perc2
    };

    drumalor::KitParameters tight;
    drumalor::KitParameters factory;
    drumalor::KitParameters loose;
    tight.humanise = 0.0f;
    factory.humanise = 0.5f;
    loose.humanise = 1.0f;

    for (const auto instrument : probeInstruments)
    {
        const std::string label (drumalor::getInstrumentDisplayName (instrument));
        const auto defaults = drumalor::getInstrumentMetadata (instrument).defaultParameters;

        // The 0.5 default must reproduce the historical fixed variation depth
        // sample for sample, so restoring an old session cannot change a kit.
        drumalor::DrumEngine legacy;
        legacy.prepare (48000.0, defaultBlockSize);
        legacy.setInstrumentParameters (instrument, defaults);
        std::vector<float> legacyRender;
        for (int hit = 0; hit < hitCount; ++hit)
        {
            legacy.trigger (instrument, 0.85f);
            auto segment = renderInterleaved (legacy, samplesPerHit, defaultBlockSize);
            legacyRender.insert (legacyRender.end(), segment.begin(), segment.end());
        }
        expect (legacyRender
                    == renderKitHits (instrument, defaults, factory, hitCount, samplesPerHit),
                label + " default Humanise changed the untouched analogue variation");

        const auto tightRender = renderKitHits (
            instrument, defaults, tight, hitCount, samplesPerHit);
        const auto looseRender = renderKitHits (
            instrument, defaults, loose, hitCount, samplesPerHit);
        expect (std::all_of (tightRender.begin(), tightRender.end(),
                             [] (float value) { return std::isfinite (value); })
                    && std::all_of (looseRender.begin(), looseRender.end(),
                                    [] (float value) { return std::isfinite (value); }),
                label + " Humanise endpoints produced NaN or infinity");

        // Spread between consecutive equal-velocity strikes must grow with the
        // control and collapse at zero.
        const auto hitSpread = [samplesPerHit] (const std::vector<float>& render)
        {
            const auto stride = static_cast<std::size_t> (samplesPerHit) * 2u;
            double total = 0.0;
            for (std::size_t hit = 1; hit * stride < render.size(); ++hit)
            {
                const std::vector<float> previous (
                    render.begin() + static_cast<std::ptrdiff_t> ((hit - 1u) * stride),
                    render.begin() + static_cast<std::ptrdiff_t> (hit * stride));
                const std::vector<float> current (
                    render.begin() + static_cast<std::ptrdiff_t> (hit * stride),
                    render.begin() + static_cast<std::ptrdiff_t> ((hit + 1u) * stride));
                total += meanAbsoluteDifference (previous, current);
            }
            return total;
        };
        const double tightSpread = hitSpread (tightRender);
        const double factorySpread = hitSpread (legacyRender);
        const double looseSpread = hitSpread (looseRender);
        expect (looseSpread > factorySpread && factorySpread > tightSpread,
                label + " Humanise did not order hit-to-hit variation (tight/factory/loose "
                    + std::to_string (tightSpread) + "/" + std::to_string (factorySpread)
                    + "/" + std::to_string (looseSpread) + ")");

        // Every setting must stay reproducible after reset.
        drumalor::DrumEngine replay;
        replay.prepare (48000.0, defaultBlockSize);
        replay.setInstrumentParameters (instrument, defaults);
        replay.setKitParameters (loose);
        std::vector<float> firstPass;
        for (int hit = 0; hit < hitCount; ++hit)
        {
            replay.trigger (instrument, 0.85f);
            auto segment = renderInterleaved (replay, samplesPerHit, defaultBlockSize);
            firstPass.insert (firstPass.end(), segment.begin(), segment.end());
        }
        replay.reset();
        std::vector<float> secondPass;
        for (int hit = 0; hit < hitCount; ++hit)
        {
            replay.trigger (instrument, 0.85f);
            auto segment = renderInterleaved (replay, samplesPerHit, defaultBlockSize);
            secondPass.insert (secondPass.end(), segment.begin(), segment.end());
        }
        expect (firstPass == secondPass,
                label + " maximum Humanise was not reproducible after reset");
        expect (firstPass == looseRender,
                label + " maximum Humanise depended on engine history");
    }
}

void testKitBusStage()
{
    constexpr double sampleRate = 48000.0;
    constexpr int captureSamples = 24000;
    const auto renderKitAt = [] (const drumalor::KitParameters& kit, float velocity)
    {
        drumalor::DrumEngine engine;
        // Publish the kit before prepare(), exactly as the processor does, so
        // the bus smoothers adopt the restored setting instead of ramping up to
        // it across the probe's own opening transient.
        engine.setKitParameters (kit);
        engine.prepare (sampleRate, defaultBlockSize);
        for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
            engine.trigger (static_cast<drumalor::Instrument> (index), velocity);
        return renderInterleaved (engine, captureSamples, defaultBlockSize);
    };
    const auto renderKit = [&renderKitAt] (const drumalor::KitParameters& kit)
    {
        return renderKitAt (kit, 0.92f);
    };

    drumalor::KitParameters bypassed;
    const auto dry = renderKit (bypassed);
    expect (metricsForInterleaved (dry).peak > 0.05,
            "kit bus probe rendered an inaudible kit");

    // The default bus must be a true bypass, not an almost-bypass.
    drumalor::KitParameters explicitBypass;
    explicitBypass.busDrive = 0.0f;
    explicitBypass.busCompression = 0.0f;
    expect (dry == renderKit (explicitBypass),
            "an explicitly zeroed kit bus changed the render");

    drumalor::KitParameters driven;
    driven.busDrive = 1.0f;
    const auto drivenRender = renderKit (driven);
    const auto drivenMetrics = metricsForInterleaved (drivenRender);
    expect (drivenMetrics.finite && drivenMetrics.peak <= 1.001,
            "bus drive produced unsafe audio");
    expect (meanAbsoluteDifference (dry, drivenRender) > 1.0e-4,
            "bus drive did not change the kit");

    // The stage has to meet bypass continuously. A fixed curvature offset meant
    // the first fraction of a percent on the knob jumped straight to a third of
    // the full curve, so automation crossing zero clicked.
    drumalor::KitParameters barelyDriven;
    barelyDriven.busDrive = 0.001f;
    const auto barelyDrivenRender = renderKit (barelyDriven);
    const double bypassGap = meanAbsoluteDifference (dry, barelyDrivenRender);
    const double fullDriveGap = meanAbsoluteDifference (dry, drivenRender);
    expect (bypassGap < 0.02 * fullDriveGap,
            "the first step of bus drive is discontinuous with bypass ("
                + std::to_string (bypassGap) + " against "
                + std::to_string (fullDriveGap) + " at full drive)");
    const double driveLevelDb = 20.0 * std::log10 (
        metricsForInterleaved (drivenRender).rms()
        / std::max (1.0e-12, metricsForInterleaved (dry).rms()));
    expect (std::abs (driveLevelDb) <= 4.0,
            "bus drive is a level control rather than a saturation control ("
                + std::to_string (driveLevelDb) + " dB)");

    drumalor::KitParameters compressed;
    compressed.busCompression = 1.0f;
    const auto compressedRender = renderKit (compressed);
    const auto compressedMetrics = metricsForInterleaved (compressedRender);
    expect (compressedMetrics.finite && compressedMetrics.peak <= 1.001,
            "bus compression produced unsafe audio");

    // Glue means less distance between a hard and a soft kit: the defining
    // behaviour of a compressor, measured on the level law rather than on any
    // particular internal envelope.
    const auto softDry = renderKitAt (bypassed, 0.30f);
    const auto softCompressed = renderKitAt (compressed, 0.30f);
    const double dryRangeDb = 20.0 * std::log10 (
        metricsForInterleaved (dry).rms()
        / std::max (1.0e-12, metricsForInterleaved (softDry).rms()));
    const double compressedRangeDb = 20.0 * std::log10 (
        metricsForInterleaved (compressedRender).rms()
        / std::max (1.0e-12, metricsForInterleaved (softCompressed).rms()));
    expect (dryRangeDb > 4.0,
            "the kit bus probe has too little dynamic range to test compression ("
                + std::to_string (dryRangeDb) + " dB)");
    expect (compressedRangeDb < dryRangeDb - 1.5,
            "bus compression did not reduce the kit's dynamic range (dry/compressed "
                + std::to_string (dryRangeDb) + "/" + std::to_string (compressedRangeDb)
                + " dB)");

    // Gain reduction has to be reported for the editor's meter, and released.
    drumalor::DrumEngine metered;
    metered.prepare (sampleRate, defaultBlockSize);
    metered.setKitParameters (compressed);
    expect (metered.getBusGain() == 1.0f, "idle bus reported gain reduction");
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
        metered.trigger (static_cast<drumalor::Instrument> (index), 1.0f);
    renderMetrics (metered, 4800, defaultBlockSize);
    const float loudGain = metered.getBusGain();
    expect (loudGain < 0.90f && loudGain > 0.0f,
            "bus compressor reported no gain reduction on a loud kit (gain "
                + std::to_string (loudGain) + ")");
    metered.allSoundsOff();
    renderMetrics (metered, 48000, defaultBlockSize);
    expect (metered.getBusGain() > 0.97f,
            "bus compressor never released its gain reduction");

    // Level and Pan are channel-strip controls, so automating them has to be
    // audible on a voice that is already ringing. They used to be copied into
    // the voice at trigger time only, which left a long crash or ride tail
    // ignoring the mixer until the next hit.
    const auto crashTailRms = [] (bool automateLevel, float automatedDecibels)
    {
        drumalor::DrumEngine engine;
        engine.prepare (sampleRate, defaultBlockSize);
        auto values = drumalor::getInstrumentMetadata (drumalor::Instrument::Crash)
                          .defaultParameters;
        engine.setInstrumentParameters (drumalor::Instrument::Crash, values);
        engine.trigger (drumalor::Instrument::Crash, 1.0f);
        renderMetrics (engine, static_cast<int> (0.2 * sampleRate), defaultBlockSize);

        if (automateLevel)
        {
            values.level = automatedDecibels;
            engine.setInstrumentParameters (drumalor::Instrument::Crash, values);
        }
        return renderMetrics (engine, static_cast<int> (0.3 * sampleRate),
                              defaultBlockSize).rms();
    };

    const double ringingTail = crashTailRms (false, 0.0f);
    const double duckedTail = crashTailRms (true, -24.0f);
    expect (ringingTail > 1.0e-5, "the crash tail probe produced no signal");
    expect (duckedTail < 0.50 * ringingTail,
            "level automation never reached the ringing voice (tail rms "
                + std::to_string (duckedTail) + " against "
                + std::to_string (ringingTail) + " untouched)");

    // Bypassing the compressor must not freeze its detector. With Bus Drive
    // still on the bus stage keeps running, so automating Bus Compression back
    // on used to reapply whatever gain reduction was in flight when it was
    // switched off, however long ago that was.
    drumalor::DrumEngine automated;
    automated.prepare (sampleRate, defaultBlockSize);
    drumalor::KitParameters drivenCompressed;
    drivenCompressed.busDrive = 0.6f;
    drivenCompressed.busCompression = 1.0f;
    automated.setKitParameters (drivenCompressed);
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
        automated.trigger (static_cast<drumalor::Instrument> (index), 1.0f);
    renderMetrics (automated, 4800, defaultBlockSize);
    const float bypassEntryGain = automated.getBusGain();
    expect (bypassEntryGain < 0.90f,
            "the automation probe never built any gain reduction to go stale");

    // Compression off, Drive still on, and a quiet passage that keeps a voice
    // alive so the bus stage is not skipped as silence.
    drumalor::KitParameters compressionBypassed = drivenCompressed;
    compressionBypassed.busCompression = 0.0f;
    automated.setKitParameters (compressionBypassed);
    for (int repeat = 0; repeat < 5; ++repeat)
    {
        automated.trigger (drumalor::Instrument::Kick, 0.04f);
        renderMetrics (automated, static_cast<int> (0.1 * sampleRate), defaultBlockSize);
    }

    automated.setKitParameters (drivenCompressed);
    renderMetrics (automated, 64, defaultBlockSize);
    const float resumedGain = automated.getBusGain();
    expect (resumedGain > 0.97f,
            "re-enabling bus compression restored the gain reduction from "
            "before the bypass (resumed at " + std::to_string (resumedGain)
                + ", bypassed at " + std::to_string (bypassEntryGain) + ")");

    // Both stages stay stable and block-partition invariant together.
    drumalor::KitParameters both;
    both.busDrive = 0.8f;
    both.busCompression = 0.7f;
    const auto renderPartitioned = [&both] (int blockSize)
    {
        drumalor::DrumEngine engine;
        engine.setKitParameters (both);
        engine.prepare (sampleRate, 512);
        for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
            engine.trigger (static_cast<drumalor::Instrument> (index), 0.95f);
        return renderInterleaved (engine, 12000, blockSize);
    };
    expect (renderPartitioned (1) == renderPartitioned (383),
            "the kit bus stage changed with host block partitioning");
}

// Both bus controls used to be taken straight from the block's parameter value.
// Stepping one between two blocks put a hard discontinuity into the mix: on a
// sustained 24 Hz kick, switching Bus Compression on produced a sample-to-sample
// jump 143 times the largest step anywhere else in that waveform, and Bus Drive
// one 37 times as large. Both stages now follow the master gain's 20 ms law.
//
// Watching only the single sample at the block boundary, and only the OFF -> ON
// direction, is not enough. Bus Drive sets its saturator's curvature straight
// from the control, and the antiderivative that stage's antialiasing evaluates
// used to lose every significant digit once the curvature fell below about
// 1e-3 - a region the control reaches on its own smallest parameter step and
// dwells in for ~80 ms on every ramp toward bypass. The boundary sample was
// perfectly smooth while the next 3500 were a full-scale clipped square wave:
// peak 1.0, sample-to-sample jump 2.0, at every supported rate. So each sweep
// below runs both directions and scans the whole half-second that follows,
// bounding its peak and its largest jump against what the same tail was already
// doing before the control moved.
void testBusAutomationIsClickFree()
{
    constexpr int blockSize = 64;
    struct SweepResult
    {
        double boundaryRatio { 0.0 };
        double peakRatio { 0.0 };
        double jumpRatio { 0.0 };
    };

    const auto sweep = [] (double sampleRate, bool automateDrive,
                           float from, float to)
    {
        drumalor::DrumEngine engine;
        engine.prepare (sampleRate, blockSize);
        // A deep kick tail is the clearest probe available: it is smooth,
        // sustained and slow, so its own sample-to-sample motion is tiny and any
        // gain step at a block boundary stands out unmistakably.
        auto kick = drumalor::getInstrumentMetadata (
            drumalor::Instrument::Kick).defaultParameters;
        kick.decay = 1.0f;
        kick.pitch = -12.0f;
        engine.setInstrumentParameters (drumalor::Instrument::Kick, kick);

        drumalor::KitParameters start;
        (automateDrive ? start.busDrive : start.busCompression) = from;
        engine.setKitParameters (start);
        engine.trigger (drumalor::Instrument::Kick, 1.0f);

        std::vector<float> left (static_cast<std::size_t> (blockSize));
        std::vector<float> right (static_cast<std::size_t> (blockSize));
        for (int block = 0; block < 75; ++block)
            engine.process (left.data(), right.data(), blockSize);

        // A quarter second of the settled tail is the reference: whatever the
        // waveform's own peak and largest neighbour step are here, automating a
        // bus control must not multiply either of them.
        const auto measure = [&left, &right, &engine] (int blocks, double& peak,
                                                       double& step, float& previous,
                                                       double* firstJump)
        {
            for (int block = 0; block < blocks; ++block)
            {
                engine.process (left.data(), right.data(), blockSize);
                for (int sample = 0; sample < blockSize; ++sample)
                {
                    const double value = left[static_cast<std::size_t> (sample)];
                    const double jump = std::abs (value - previous);
                    if (firstJump != nullptr && block == 0 && sample == 0)
                        *firstJump = jump;
                    peak = std::max (peak, std::abs (value));
                    step = std::max (step, jump);
                    previous = left[static_cast<std::size_t> (sample)];
                }
            }
        };

        const int settledBlocks = static_cast<int> (0.25 * sampleRate) / blockSize;
        double settledPeak = 0.0;
        double settledStep = 0.0;
        float previous = left[static_cast<std::size_t> (blockSize - 1)];
        measure (settledBlocks, settledPeak, settledStep, previous, nullptr);

        drumalor::KitParameters changed;
        (automateDrive ? changed.busDrive : changed.busCompression) = to;
        engine.setKitParameters (changed);

        const int settlingBlocks = static_cast<int> (0.5 * sampleRate) / blockSize;
        double peak = 0.0;
        double step = 0.0;
        double boundaryJump = 0.0;
        measure (settlingBlocks, peak, step, previous, &boundaryJump);

        return SweepResult { boundaryJump / std::max (1.0e-9, settledStep),
                             peak / std::max (1.0e-9, settledPeak),
                             step / std::max (1.0e-9, settledStep) };
    };

    // 0.001 is the smallest step the host can send either control: it is one
    // increment of the 0.1 % parameter grid, and it is exactly where the old
    // antiderivative fell apart.
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    for (const bool automateDrive : { true, false })
    for (const float target : { 0.001f, 0.5f, 1.0f })
    for (const bool turningOn : { true, false })
    {
        const std::string what = std::string (automateDrive ? "Bus Drive" : "Bus Compression")
            + (turningOn ? " to " : " from ") + std::to_string (target)
            + " at " + std::to_string (static_cast<int> (sampleRate)) + " Hz";
        const auto result = turningOn ? sweep (sampleRate, automateDrive, 0.0f, target)
                                      : sweep (sampleRate, automateDrive, target, 0.0f);
        expect (result.boundaryRatio < 3.0,
                "automating " + what + " stepped the mix at the block boundary by "
                    + std::to_string (result.boundaryRatio)
                    + " times the waveform's own largest sample-to-sample motion");
        expect (result.peakRatio < 3.0,
                "automating " + what + " pushed the half second that followed to "
                    + std::to_string (result.peakRatio)
                    + " times the settled tail's own peak");
        expect (result.jumpRatio < 4.0,
                "automating " + what + " put a sample-to-sample jump of "
                    + std::to_string (result.jumpRatio)
                    + " times the settled tail's own largest step into the half"
                      " second that followed");
    }
}

void testMetering()
{
    constexpr double sampleRate = 48000.0;
    drumalor::DrumEngine engine;
    engine.prepare (sampleRate, defaultBlockSize);
    expect (engine.getOutputLevel (0) == 0.0f && engine.getOutputLevel (1) == 0.0f,
            "a prepared engine reported output level before any hit");
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
        expect (engine.getInstrumentLevel (static_cast<drumalor::Instrument> (index))
                    == 0.0f,
                "a prepared engine reported channel level before any hit");

    engine.trigger (drumalor::Instrument::Kick, 1.0f);
    renderMetrics (engine, 2400, defaultBlockSize);
    expect (engine.getOutputLevel (0) > 0.01f && engine.getOutputLevel (1) > 0.01f,
            "output metering stayed silent while the kick was sounding");
    expect (engine.getInstrumentLevel (drumalor::Instrument::Kick) > 0.01f,
            "kick channel metering stayed silent while it was sounding");
    expect (engine.getInstrumentLevel (drumalor::Instrument::Crash) == 0.0f,
            "an unplayed channel reported a level");

    // Panning must be visible in the stereo meter.
    drumalor::DrumEngine panned;
    panned.prepare (sampleRate, defaultBlockSize);
    auto hardLeft = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Snare).defaultParameters;
    hardLeft.pan = -1.0f;
    panned.setInstrumentParameters (drumalor::Instrument::Snare, hardLeft);
    panned.trigger (drumalor::Instrument::Snare, 1.0f);
    renderMetrics (panned, 2400, defaultBlockSize);
    expect (panned.getOutputLevel (0) > 10.0f * panned.getOutputLevel (1),
            "the stereo meter did not follow a hard-left channel");

    // Levels must release rather than latch, and clear on reset.
    engine.allSoundsOff();
    renderMetrics (engine, static_cast<int> (2.0 * sampleRate), defaultBlockSize);
    expect (engine.getOutputLevel (0) < 0.001f,
            "output metering never released after the kit stopped");
    expect (engine.getInstrumentLevel (drumalor::Instrument::Kick) == 0.0f,
            "channel metering never released after the kit stopped");
    engine.trigger (drumalor::Instrument::Ride, 0.9f);
    renderMetrics (engine, 2400, defaultBlockSize);
    engine.reset();
    expect (engine.getOutputLevel (0) == 0.0f && engine.getOutputLevel (1) == 0.0f
                && engine.getInstrumentLevel (drumalor::Instrument::Ride) == 0.0f
                && engine.getBusGain() == 1.0f,
            "reset did not clear the published metering state");
}

void testMembraneAndVelocityTimbre()
{
    constexpr double sampleRate = 48000.0;
    // Bands chosen around each drum's own fundamental: the body region covers
    // the swept fundamental, the head region the inharmonic membrane modes that
    // sit between roughly 1.5 and 3 times it.
    struct MembraneProbe
    {
        drumalor::Instrument instrument;
        double bodyLow;
        double bodyHigh;
        double headLow;
        double headHigh;
    };
    constexpr std::array membraneProbes {
        MembraneProbe { drumalor::Instrument::LowTom, 55.0, 130.0, 130.0, 420.0 },
        MembraneProbe { drumalor::Instrument::MidTom, 85.0, 190.0, 190.0, 620.0 },
        MembraneProbe { drumalor::Instrument::HighTom, 120.0, 270.0, 270.0, 880.0 },
        MembraneProbe { drumalor::Instrument::Snare, 130.0, 290.0, 290.0, 760.0 }
    };

    for (const auto& probe : membraneProbes)
    {
        const std::string label (drumalor::getInstrumentDisplayName (probe.instrument));
        const auto defaults = drumalor::getInstrumentMetadata (
            probe.instrument).defaultParameters;
        const auto samples = renderMonoHit (
            sampleRate, probe.instrument, defaults, 0.60, 0.95f);
        expect (std::all_of (samples.begin(), samples.end(), [] (float value)
                {
                    return std::isfinite (value);
                }),
                label + " membrane render produced NaN or infinity");
        expect (peakInRange (samples, 0, samples.size()) <= 1.001,
                label + " membrane render exceeded the output safety bound");

        const double bodyBegin = bandPowerInRange (
            samples, sampleRate, probe.bodyLow, probe.bodyHigh, 0.004, 0.060);
        const double headBegin = bandPowerInRange (
            samples, sampleRate, probe.headLow, probe.headHigh, 0.004, 0.060);
        const double earlyRatio = headBegin / std::max (1.0e-20, bodyBegin);
        expect (earlyRatio > 0.05,
                label + " has no audible inharmonic head content at the strike (ratio "
                    + std::to_string (earlyRatio) + ")");

        // A struck head rings out faster than the drum body. The snare is
        // excluded because its wire wash, not its head, dominates that region
        // later in the tail; its own nonlinearity is checked separately below.
        if (probe.instrument == drumalor::Instrument::Snare)
            continue;
        const double bodyLate = bandPowerInRange (
            samples, sampleRate, probe.bodyLow, probe.bodyHigh, 0.180, 0.320);
        const double headLate = bandPowerInRange (
            samples, sampleRate, probe.headLow, probe.headHigh, 0.180, 0.320);
        const double lateRatio = headLate / std::max (1.0e-20, bodyLate);
        expect (lateRatio < 0.85 * earlyRatio,
                label + " head modes did not decay faster than the body (early/late "
                    + std::to_string (earlyRatio) + "/" + std::to_string (lateRatio) + ")");
    }

    // Air loading is a real control, not a constant: Skin has to move where the
    // head modes sit relative to the fundamental.
    auto tightHead = drumalor::getInstrumentMetadata (
        drumalor::Instrument::MidTom).defaultParameters;
    auto looseHead = tightHead;
    tightHead.characterB = 1.0f;
    looseHead.characterB = 0.0f;
    const auto tightSamples = renderMonoHit (
        sampleRate, drumalor::Instrument::MidTom, tightHead, 0.30, 0.95f);
    const auto looseSamples = renderMonoHit (
        sampleRate, drumalor::Instrument::MidTom, looseHead, 0.30, 0.95f);
    const double tightUpper = bandPowerInRange (
        tightSamples, sampleRate, 300.0, 620.0, 0.004, 0.060)
        / std::max (1.0e-20, bandPowerInRange (
            tightSamples, sampleRate, 190.0, 300.0, 0.004, 0.060));
    const double looseUpper = bandPowerInRange (
        looseSamples, sampleRate, 300.0, 620.0, 0.004, 0.060)
        / std::max (1.0e-20, bandPowerInRange (
            looseSamples, sampleRate, 190.0, 300.0, 0.004, 0.060));
    expect (std::abs (20.0 * std::log10 (
                std::max (1.0e-12, tightUpper / std::max (1.0e-20, looseUpper)))) > 0.5,
            "Mid Tom Skin did not move the membrane's air loading (tight/loose "
                + std::to_string (tightUpper) + "/" + std::to_string (looseUpper) + ")");

    // The snare wires only rattle while the head displacement lifts them off
    // their rest contact, so their share of the sound grows faster than the
    // body does as the strike gets harder.
    const auto snareDefaults = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Snare).defaultParameters;
    const auto hardSnare = renderMonoHit (
        sampleRate, drumalor::Instrument::Snare, snareDefaults, 0.30, 1.0f);
    const auto softSnare = renderMonoHit (
        sampleRate, drumalor::Instrument::Snare, snareDefaults, 0.30, 0.22f);
    const double hardWireRatio = bandPowerInRange (
        hardSnare, sampleRate, 1500.0, 9000.0, 0.0, 0.120)
        / std::max (1.0e-20, bandPowerInRange (
            hardSnare, sampleRate, 130.0, 290.0, 0.0, 0.120));
    const double softWireRatio = bandPowerInRange (
        softSnare, sampleRate, 1500.0, 9000.0, 0.0, 0.120)
        / std::max (1.0e-20, bandPowerInRange (
            softSnare, sampleRate, 130.0, 290.0, 0.0, 0.120));
    expect (softWireRatio < 0.85 * hardWireRatio,
            "the snare wire rattle is not level dependent (hard/soft wire-to-body "
                + std::to_string (hardWireRatio) + "/" + std::to_string (softWireRatio)
                + ")");

    // Velocity has to change timbre, not only level. Comparing the high-band to
    // low-band ratio stays sensitive even for voices that are almost entirely
    // high frequency, where a share of the total would saturate near one.
    constexpr std::array timbreInstruments {
        drumalor::Instrument::Kick, drumalor::Instrument::Snare,
        drumalor::Instrument::Clap, drumalor::Instrument::ClosedHat,
        drumalor::Instrument::OpenHat, drumalor::Instrument::LowTom,
        drumalor::Instrument::MidTom, drumalor::Instrument::HighTom,
        drumalor::Instrument::Shaker, drumalor::Instrument::Perc2
    };
    for (const auto instrument : timbreInstruments)
    {
        const std::string label (drumalor::getInstrumentDisplayName (instrument));
        const auto defaults = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        const auto hard = renderMonoHit (sampleRate, instrument, defaults, 0.30, 1.0f);
        const auto soft = renderMonoHit (sampleRate, instrument, defaults, 0.30, 0.18f);

        const double hardRatio = bandPowerInRange (
            hard, sampleRate, 2500.0, 16000.0, 0.0, 0.060)
            / std::max (1.0e-20, bandPowerInRange (
                hard, sampleRate, 40.0, 2500.0, 0.0, 0.060));
        const double softRatio = bandPowerInRange (
            soft, sampleRate, 2500.0, 16000.0, 0.0, 0.060)
            / std::max (1.0e-20, bandPowerInRange (
                soft, sampleRate, 40.0, 2500.0, 0.0, 0.060));
        expect (softRatio < 0.90 * hardRatio,
                label + " soft hits are not spectrally darker than hard hits"
                    " (hard/soft high-to-low " + std::to_string (hardRatio) + "/"
                    + std::to_string (softRatio) + ")");
    }
}

void testUiPresentationMath()
{
    using namespace drumalor::ui;

    // Meter curve: monotone, anchored at both ends, and exactly invertible.
    expect (meterPositionForLinear (1.0f, -48.0f) == 1.0f,
            "full scale is not the top of the meter curve");
    expect (meterPositionForLinear (0.0f, -48.0f) == 0.0f,
            "silence is not the bottom of the meter curve");
    expect (std::abs (meterPositionForLinear (0.5f, -48.0f) - (1.0f - (-6.0206f) / -48.0f))
                < 0.002f,
            "half amplitude is not placed at -6 dB on the meter curve");
    float previousPosition = -1.0f;
    for (int step = 0; step <= 64; ++step)
    {
        const float position = static_cast<float> (step) / 64.0f;
        const float linear = linearForMeterPosition (position, -48.0f);
        const float roundTrip = meterPositionForLinear (linear, -48.0f);
        expect (std::abs (roundTrip - position) < 0.002f,
                "meter curve did not round-trip at position "
                    + std::to_string (position));
        expect (position > previousPosition, "meter curve is not strictly increasing");
        previousPosition = position;
    }
    expect (meterPositionForLinear (std::numeric_limits<float>::quiet_NaN(), -48.0f) == 0.0f
                && meterPositionForLinear (-1.0f, 0.0f) == 0.0f,
            "meter curve did not sanitize invalid input");

    // Ballistics: instant attack, gradual release, hold then fall on the peak.
    MeterBallistics ballistics;
    const float release = onePoleCoefficient (0.30f, 30.0f);
    const float fall = decayMultiplier (-12.0f, 1.0f, 30.0f);
    expect (release > 0.0f && release < 1.0f, "release coefficient is out of range");
    expect (fall > 0.0f && fall < 1.0f, "peak fall multiplier is out of range");
    expect (onePoleCoefficient (0.0f, 30.0f) == 1.0f,
            "a zero time constant is not an instant coefficient");

    ballistics.update (0.8f, 1.0f, release, fall, 3.0f);
    expect (std::abs (ballistics.level - 0.8f) < 1.0e-6f,
            "meter attack was not instant");
    expect (std::abs (ballistics.peak - 0.8f) < 1.0e-6f, "peak did not latch");
    for (int update = 0; update < 3; ++update)
        ballistics.update (0.0f, 1.0f, release, fall, 3.0f);
    expect (ballistics.level < 0.8f && ballistics.level > 0.0f,
            "meter release was instant instead of gradual");
    expect (std::abs (ballistics.peak - 0.8f) < 1.0e-6f,
            "peak marker did not hold for its configured time");
    const float heldPeak = ballistics.peak;
    for (int update = 0; update < 12; ++update)
        ballistics.update (0.0f, 1.0f, release, fall, 3.0f);
    expect (ballistics.peak < heldPeak, "peak marker never fell after its hold");
    expect (ballistics.peak >= ballistics.level,
            "peak marker fell below the level it marks");
    ballistics.update (std::numeric_limits<float>::infinity(), 1.0f, release, fall, 3.0f);
    expect (std::isfinite (ballistics.level) && std::isfinite (ballistics.peak),
            "meter ballistics did not sanitize invalid input");
    ballistics.reset();
    expect (ballistics.level == 0.0f && ballistics.peak == 0.0f,
            "meter ballistics did not reset");

    // Pad-grid geometry: equal cells, exact gaps, and short rows centred under
    // the long row they share a grid with.
    const auto fullRow = rowLayout (960, 7, 7, 7);
    expect (fullRow.cellSize == 131 && fullRow.origin == 0,
            "seven-column pad grid did not fill its row");
    expect (cellOffset (fullRow, 7, 0) == 0
                && cellOffset (fullRow, 7, 6) == 6 * (131 + 7),
            "pad grid cell offsets do not honour the gap");
    const auto shortRow = rowLayout (960, 7, 7, 6);
    expect (shortRow.cellSize == fullRow.cellSize,
            "short pad row used a different cell size");
    const int used = shortRow.cellSize * 6 + 7 * 5;
    expect (shortRow.origin == (960 - used) / 2,
            "short pad row was not centred under the full row");
    expect (cellOffset (shortRow, 7, 5) + shortRow.cellSize <= 960,
            "short pad row escaped its container");
    const auto degenerate = rowLayout (0, 0, -4, 99);
    expect (degenerate.cellSize >= 1 && degenerate.origin >= 0,
            "pad grid did not sanitize a degenerate request");

    // Colour/curve helpers used by the meter ramp.
    expect (mix (0.0f, 10.0f, 0.25f) == 2.5f, "mix did not interpolate");
    expect (mix (0.0f, 10.0f, -3.0f) == 0.0f && mix (0.0f, 10.0f, 4.0f) == 10.0f,
            "mix did not clamp its amount");
    expect (smoothStep (0.0f, 1.0f, 0.0f) == 0.0f
                && smoothStep (0.0f, 1.0f, 1.0f) == 1.0f
                && std::abs (smoothStep (0.0f, 1.0f, 0.5f) - 0.5f) < 1.0e-6f,
            "smoothStep endpoints or midpoint are wrong");
    expect (smoothStep (0.5f, 0.5f, 0.6f) == 1.0f
                && smoothStep (0.5f, 0.5f, 0.4f) == 0.0f,
            "smoothStep did not handle a zero-width edge");
}

void testIdleMetallicCostAndDenormalSafety()
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int samples = 48000 * 4;

    // A bank frozen while other voices were sounding must wake into exactly the
    // state it would have reached had it kept running behind their closed VCAs.
    // Two engines that reach the same absolute gap in different ways - one busy
    // with an unrelated drum, one completely idle - must therefore render the
    // same hi-hat, sample for sample.
    drumalor::DrumEngine busy;
    drumalor::DrumEngine idle;
    for (auto* engine : { &busy, &idle })
        engine->prepare (sampleRate, 512);
    auto shortKick = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Kick).defaultParameters;
    shortKick.decay = 0.0f;
    busy.setInstrumentParameters (drumalor::Instrument::Kick, shortKick);
    busy.trigger (drumalor::Instrument::Kick, 0.9f);
    constexpr int gapSamples = 48000;
    renderInterleaved (busy, gapSamples, 373);
    renderInterleaved (idle, gapSamples, 97);
    expect (busy.getActiveVoiceCount() == 0,
            "the frozen-bank probe still had a sounding kick at the hi-hat strike");
    busy.trigger (drumalor::Instrument::ClosedHat, 0.85f);
    idle.trigger (drumalor::Instrument::ClosedHat, 0.85f);
    expect (renderInterleaved (busy, 6000, defaultBlockSize)
                == renderInterleaved (idle, 6000, defaultBlockSize),
            "a metallic bank frozen behind an unrelated voice woke into a "
            "different state than one frozen during silence");

    // Cost side of the same contract. A bank is only advanced while some voice
    // can observe it, so adding one hi-hat to a single sustained kick has to
    // cost clearly more than that kick alone. When all five relaxation banks ran
    // unconditionally the two measurements were nearly identical, so this ratio
    // is what actually guards the optimisation. It compares two runs taken back
    // to back on the same machine rather than an absolute wall-clock budget.
    const auto timeSustainedKick = [sampleRate, samples, blockSize] (bool withHat)
    {
        drumalor::DrumEngine engine;
        engine.prepare (sampleRate, blockSize);
        auto deepKick = drumalor::getInstrumentMetadata (
            drumalor::Instrument::Kick).defaultParameters;
        deepKick.pitch = -24.0f;
        deepKick.decay = 1.0f;
        engine.setInstrumentParameters (drumalor::Instrument::Kick, deepKick);
        engine.trigger (drumalor::Instrument::Kick, 1.0f);
        std::array<float, blockSize> left {};
        std::array<float, blockSize> right {};
        bool finite = true;
        const auto start = std::chrono::steady_clock::now();
        for (int rendered = 0; rendered < samples; rendered += blockSize)
        {
            if (withHat && rendered % 4800 == 0)
                engine.trigger (drumalor::Instrument::ClosedHat, 0.8f);
            const int count = std::min (blockSize, samples - rendered);
            engine.process (left.data(), right.data(), count);
            for (int sample = 0; sample < count; ++sample)
                finite = finite
                    && std::isfinite (left[static_cast<std::size_t> (sample)])
                    && std::isfinite (right[static_cast<std::size_t> (sample)]);
        }
        const double elapsed = std::chrono::duration<double> (
            std::chrono::steady_clock::now() - start).count();
        expect (finite, "the timed metallic probe produced NaN or infinity");
        return elapsed;
    };

    double withoutHat = std::numeric_limits<double>::max();
    double withHat = std::numeric_limits<double>::max();
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        withoutHat = std::min (withoutHat, timeSustainedKick (false));
        withHat = std::min (withHat, timeSustainedKick (true));
    }
    expect (withHat > 1.30 * withoutHat,
            "adding a hi-hat did not measurably cost more than the kick alone, so "
            "the relaxation banks are running even when nothing can observe them "
            "(kick/kick+hat " + std::to_string (withoutHat) + "/"
                + std::to_string (withHat) + " s)");
    expect (withHat < 6.0,
            "four seconds of a sustained kick and hi-hat exceeded the offline "
            "guardrail (" + std::to_string (withHat) + " s)");

    // Every envelope, resonator, biquad and detector in the engine decays
    // geometrically for as long as its voice lives, so without an explicit floor
    // they all spend a stretch of every note in the subnormal range - where x86
    // traps into microcode. A host that sets flush-to-zero for us hides that,
    // but an offline renderer or a wrapper that does not leaves the plug-in
    // paying for it: measured on this workload the engine used to cost 3.1 times
    // as much with denormals enabled as with them flushed. The engine now snaps
    // its own recursive states to zero at -600 dBFS, so the two modes must cost
    // the same and the sound cannot depend on the host's FPU configuration.
#if DRUMALOR_CAN_TOGGLE_FLUSH_TO_ZERO
    const auto timeDecayingKit = [] (bool flushToZero)
    {
        _MM_SET_FLUSH_ZERO_MODE (flushToZero ? _MM_FLUSH_ZERO_ON : _MM_FLUSH_ZERO_OFF);
        _MM_SET_DENORMALS_ZERO_MODE (
            flushToZero ? _MM_DENORMALS_ZERO_ON : _MM_DENORMALS_ZERO_OFF);
        double best = std::numeric_limits<double>::max();
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            drumalor::DrumEngine engine;
            engine.prepare (sampleRate, blockSize);
            std::array<float, blockSize> left {};
            std::array<float, blockSize> right {};
            const auto start = std::chrono::steady_clock::now();
            for (int rendered = 0, step = 0; rendered < static_cast<int> (sampleRate) * 2;
                 rendered += blockSize, ++step)
            {
                if ((step % 6) == 0)
                    engine.trigger (
                        static_cast<drumalor::Instrument> (
                            static_cast<std::size_t> (step / 6) % drumalor::instrumentCount),
                        0.9f);
                engine.process (left.data(), right.data(), blockSize);
            }
            best = std::min (best, std::chrono::duration<double> (
                std::chrono::steady_clock::now() - start).count());
        }
        _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_OFF);
        _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_OFF);
        return best;
    };

    const double flushed = timeDecayingKit (true);
    const double unflushed = timeDecayingKit (false);
    // This is a wall-clock ratio, so the bound has to leave room for a busy CI
    // machine. It sits between the ~1.0 the floored engine actually measures and
    // the 3.1 to 3.2 the unfloored one does, which is the only distinction the
    // assertion needs to make.
    expect (unflushed < 2.0 * flushed,
            "the engine still falls into denormal arithmetic when the host does "
            "not set flush-to-zero (flushed/unflushed "
                + std::to_string (flushed) + "/" + std::to_string (unflushed) + " s)");
#endif

    // Long, very quiet tails must not leave residue behind. Render a full
    // maximum-decay kit down to silence and check that the deep tail is exactly
    // zero once every voice has retired.
    drumalor::DrumEngine tails;
    tails.prepare (sampleRate, 256);
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        auto parameters = drumalor::getInstrumentMetadata (
            static_cast<drumalor::Instrument> (index)).defaultParameters;
        parameters.decay = 1.0f;
        parameters.level = drumalor::minimumVoiceLevelDecibels;
        tails.setInstrumentParameters (static_cast<drumalor::Instrument> (index),
                                       parameters);
        tails.trigger (static_cast<drumalor::Instrument> (index), 0.02f);
    }
    const auto quietTail = renderMetrics (
        tails, static_cast<int> (sampleRate * drumalor::maximumTailSeconds), 256);
    expect (quietTail.finite, "a very quiet maximum-decay kit produced invalid audio");
    expect (tails.getActiveVoiceCount() == 0,
            "a very quiet maximum-decay kit never retired its voices");
    const auto afterTail = renderMetrics (tails, 8192, 251);
    expect (afterTail.peak == 0.0,
            "the engine emitted residue after every voice retired");
}

void testInvalidValuesAndStressPerformance()
{
    drumalor::DrumEngine engine;
    engine.prepare (96000.0, defaultBlockSize);
    drumalor::InstrumentParameters invalid;
    invalid.characterA = std::numeric_limits<float>::quiet_NaN();
    invalid.characterB = std::numeric_limits<float>::infinity();
    invalid.pitch = -std::numeric_limits<float>::infinity();
    invalid.decay = std::numeric_limits<float>::quiet_NaN();
    engine.setInstrumentParameters (drumalor::Instrument::Snare, invalid);
    engine.setOutputGain (std::numeric_limits<float>::quiet_NaN());
    expect (! engine.triggerMidi (127, 1.0f), "unmapped MIDI note reported as handled");
    expect (engine.triggerMidi (38, 1.0f), "mapped snare note was rejected");
    const auto invalidMetrics = renderMetrics (engine, 4096);
    expect (invalidMetrics.finite && invalidMetrics.nonZeroCount > 0,
            "invalid parameter sanitization did not produce safe audio");

    drumalor::KitParameters invalidKit;
    invalidKit.humanise = std::numeric_limits<float>::quiet_NaN();
    invalidKit.busDrive = std::numeric_limits<float>::infinity();
    invalidKit.busCompression = -std::numeric_limits<float>::infinity();
    engine.setKitParameters (invalidKit);
    drumalor::InstrumentParameters invalidMixer;
    invalidMixer.level = std::numeric_limits<float>::quiet_NaN();
    invalidMixer.pan = std::numeric_limits<float>::infinity();
    invalidMixer.chokeGroup = 9999;
    engine.setInstrumentParameters (drumalor::Instrument::LowTom, invalidMixer);
    engine.trigger (drumalor::Instrument::LowTom, 1.0f);
    const auto invalidKitMetrics = renderMetrics (engine, 8192);
    expect (invalidKitMetrics.finite && invalidKitMetrics.peak <= 1.001,
            "invalid kit/mixer sanitization did not produce safe audio");

    engine.reset();
    engine.setKitParameters (drumalor::KitParameters {});
    engine.setInstrumentParameters (
        drumalor::Instrument::LowTom,
        drumalor::getInstrumentMetadata (drumalor::Instrument::LowTom).defaultParameters);
    constexpr int samples = 96000;
    constexpr std::array instruments {
        drumalor::Instrument::Kick, drumalor::Instrument::Snare,
        drumalor::Instrument::Clap, drumalor::Instrument::ClosedHat,
        drumalor::Instrument::OpenHat, drumalor::Instrument::Ride,
        drumalor::Instrument::Crash, drumalor::Instrument::LowTom,
        drumalor::Instrument::MidTom, drumalor::Instrument::HighTom,
        drumalor::Instrument::Shaker, drumalor::Instrument::Perc1,
        drumalor::Instrument::Perc2
    };
    std::array<float, defaultBlockSize> left {};
    std::array<float, defaultBlockSize> right {};
    bool finite = true;
    double peak = 0.0;
    const auto start = std::chrono::steady_clock::now();
    for (int rendered = 0, triggerIndex = 0; rendered < samples;)
    {
        engine.trigger (instruments[static_cast<std::size_t> (triggerIndex) % instruments.size()],
                        0.35f + 0.05f * static_cast<float> (triggerIndex % 12));
        ++triggerIndex;
        const int count = std::min (defaultBlockSize, samples - rendered);
        engine.process (left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            finite = finite && std::isfinite (left[static_cast<std::size_t> (sample)])
                            && std::isfinite (right[static_cast<std::size_t> (sample)]);
            peak = std::max ({ peak,
                              std::abs (static_cast<double> (left[static_cast<std::size_t> (sample)])),
                              std::abs (static_cast<double> (right[static_cast<std::size_t> (sample)])) });
        }
        rendered += count;
    }
    const double elapsed = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - start).count();
    expect (finite && peak <= 1.001, "dense stress render produced unsafe audio");
    expect (engine.getActiveVoiceCount() <= 128,
            "voice allocation exceeded its primary and retirement-pool capacity");
    expect (elapsed < 20.0, "one-second stress render exceeded the generous performance guardrail");
}
} // namespace

int main()
{
    testMetadataAndMidiMapping();
    testEveryInstrumentAndSampleRate();
    testModalSampleRateConsistency();
    testNoiseDensityAcrossSampleRates();
    testTailsTerminate();
    testHatChokeAndPanic();
    testDeterminismAndBlockPartitioning();
    testFreeRunningMetallicOscillators();
    testPersistentMetallicParameterUpdates();
    testOrganicAnalogVariation();
    testDeepAnalogKickContract();
    testLowFrequencyTailAndVoiceStealing();
    testCymbalQualityContract();
    testCymbalCircuitContract();
    testParameterInfluence();
    testPerc1DriveAddsDensity();
    testEveryVoiceVariesBetweenHits();
    testPerVoiceMixer();
    testChokeGroups();
    testHumaniseDepth();
    testKitBusStage();
    testBusAutomationIsClickFree();
    testMetering();
    testMembraneAndVelocityTimbre();
    testUiPresentationMath();
    testIdleMetallicCostAndDenormalSafety();
    testInvalidValuesAndStressPerformance();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Drumalor DSP test(s) failed\n";
        return 1;
    }
    std::cout << "All Drumalor DSP tests passed\n";
    return 0;
}
