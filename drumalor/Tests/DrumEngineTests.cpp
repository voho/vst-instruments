#include "DSP/DrumEngine.h"

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

    auto maximumBell = rideDefaults;
    maximumBell.characterA = 1.0f;
    const auto bellRide = analyseCymbalPreset (
        drumalor::Instrument::Ride, maximumBell);
    expect (bellRide.airShare >= 0.055,
            "Ride Bell removed the cymbal wash (air share "
                + std::to_string (bellRide.airShare) + ")");
    expect (bellRide.logBandEntropy >= 0.65
                && bellRide.activeBandFraction >= 0.55,
            "Ride Bell produced a sparse, clingy resonator tail");
    const double defaultBellBalance = ride.bodyShare
        / std::max (1.0e-12, ride.airShare);
    const double maximumBellBalance = bellRide.bodyShare
        / std::max (1.0e-12, bellRide.airShare);
    expect (maximumBellBalance >= 1.25 * defaultBellBalance,
            "Ride Bell did not emphasize body relative to wash (default/max "
                + std::to_string (defaultBellBalance) + "/"
                + std::to_string (maximumBellBalance) + ")");

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

    auto narrowCrashParameters = crashDefaults;
    auto wideCrashParameters = crashDefaults;
    narrowCrashParameters.characterA = 0.0f;
    wideCrashParameters.characterA = 1.0f;
    const auto narrowCrash = analyseCymbalPreset (
        drumalor::Instrument::Crash, narrowCrashParameters);
    const auto wideCrash = analyseCymbalPreset (
        drumalor::Instrument::Crash, wideCrashParameters);
    const double spreadLevelChangeDb = std::abs (20.0 * std::log10 (
        wideCrash.rms / std::max (1.0e-12, narrowCrash.rms)));
    expect (spreadLevelChangeDb <= 4.0,
            "Crash Spread changed perceived level by more than 4 dB (observed "
                + std::to_string (spreadLevelChangeDb) + " dB)");
    const bool spreadBroadenedBands = wideCrash.logBandEntropy
        >= narrowCrash.logBandEntropy + 0.015;
    const bool spreadReducedRinging = wideCrash.tonalPersistence
        <= 0.92 * narrowCrash.tonalPersistence + 0.002;
    expect (spreadBroadenedBands || spreadReducedRinging,
            "Crash Spread did not diffuse its spectrum or reduce tonal persistence");
    expect (wideCrash.activeBandFraction + 0.10 >= narrowCrash.activeBandFraction,
            "Crash Spread lost too much spectral coverage");
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

    engine.reset();
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
    testTailsTerminate();
    testHatChokeAndPanic();
    testDeterminismAndBlockPartitioning();
    testOrganicAnalogVariation();
    testDeepAnalogKickContract();
    testLowFrequencyTailAndVoiceStealing();
    testCymbalQualityContract();
    testParameterInfluence();
    testInvalidValuesAndStressPerformance();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Drumalor DSP test(s) failed\n";
        return 1;
    }
    std::cout << "All Drumalor DSP tests passed\n";
    return 0;
}
