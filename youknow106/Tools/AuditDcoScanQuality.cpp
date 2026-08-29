// Common-host DCO reconstruction and converter-scan qualification.
//
// This executable links the unmodified shipping DSP.  Its executable-local
// friend probe prepares an isolated candidate at q * host Hz with HQ disabled,
// recovers the pre-VCF WAVE node from the module-coupling capacitor's state
// transition, and applies the shipping half-band stages explicitly back to the
// declared host boundary.  A 96 kHz fixture is therefore used only as the
// 96 kHz internal grid of a 48 kHz / q=2 isolated-domain candidate; it is not
// presented as a 96 kHz host or as whole-engine parity.

#include "DSP/YouKnow106Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace youknow106
{
struct YouKnow106TestAccess
{
    enum class Waveform { Saw, Pulse, Sub };

    struct ScanCursor
    {
        double phase {};
        std::size_t nextWrite {};
    };

    struct HoldState
    {
        float dco {};
        double pwmFirst {};
        double pwmSecond {};
        double sub {};
    };

    static void configureDco(YouKnow106Engine& engine, double internalRate,
                             double frequency, DcoRange range,
                             Waveform waveform,
                             float duty) noexcept
    {
        engine.prepare(internalRate, 256, false);

        EngineParameters parameters;
        parameters.sawEnabled = waveform == Waveform::Saw;
        parameters.pulseEnabled = waveform == Waveform::Pulse;
        parameters.subLevel = waveform == Waveform::Sub ? 1.0f : 0.0f;
        parameters.noiseLevel = 0.0f;
        parameters.cutoff = 0.0f;
        parameters.resonance = 0.0f;
        parameters.envDepth = 0.0f;
        parameters.keyFollow = 0.0f;
        parameters.calibration = 0.0f;
        parameters.range = range;
        parameters.chorus = ChorusMode::Off;
        parameters.chorusNoise = 0.0f;
        engine.setParameters(parameters);

        auto& voice = engine.voices_[0];
        voice.active = false;
        voice.cardIndex = 0;
        voice.dco.reset();
        voice.dco.periodSamples = internalRate / frequency;
        const auto divider = static_cast<std::uint32_t>(std::llround(
            YouKnow106Engine::rangeClockHz(range) / frequency));
        voice.dco.divider = divider;
        voice.dco.pendingDivider = divider;
        voice.dco.pendingDividerValid = false;
        voice.dco.pitState = YouKnow106Engine::Dco::PitState::running;
        voice.dco.pitOutHigh = true;
        voice.dco.pitClocksToEvent = static_cast<double>(
            YouKnow106Engine::Dco::mode3HalfClocks(divider, true));
        voice.dcoCv = static_cast<float>(frequency);
        voice.dcoCvTarget = static_cast<float>(frequency);
        voice.dco.renderScale = 1.0f;
        const double periodSeconds = voice.dco.periodSamples / internalRate;
        const double resetSeconds = static_cast<double>(
            YouKnow106Engine::resetFraction(periodSeconds)) * periodSeconds;
        voice.dco.rampValue = -1.0;
        voice.dco.rampSlopePerSecond = 2.0 / std::max(
            periodSeconds - resetSeconds, periodSeconds * 1.0e-4);
        voice.dco.resetSecondsRemaining = 0.0;
        voice.dco.positiveRailHeld = false;
        voice.pulseDuty = duty;
        voice.pulseThresholdVolts = 12.0f * (1.0f - duty);
        voice.previousPulseThresholdVolts = voice.pulseThresholdVolts;
        voice.pulsePinnedHigh = duty >= 1.0f;
        voice.previousPulsePinnedHigh = voice.pulsePinnedHigh;
        voice.pulseThresholdPrimed = true;
        voice.feedback = 0.0f;
        voice.filterOmegaStep = 0.0f;
        voice.inputCompensation = 1.0f;
        voice.moduleCoupling.reset();
        engine.subCv_ = waveform == Waveform::Sub ? 1.0 : 0.0;
        engine.subCvTarget_ = engine.subCv_;
    }

    // HighPass::process has s1=s0+2g(x-s0)/(1+g).  Solving that state
    // transition for x recovers the exact input to the shipping module
    // coupling, without observing or modifying renderVoice's local `mixed`.
    static float renderRecoveredMixed(YouKnow106Engine& engine) noexcept
    {
        auto& voice = engine.voices_[0];
        const double before = voice.moduleCoupling.state;
        (void) engine.renderVoice(voice, engine.activeParameters_, 0.0f);
        const double after = voice.moduleCoupling.state;
        const double g = static_cast<double>(engine.moduleCouplingG_);
        const double recovered = before
            + (after - before) * (1.0 + g) / (2.0 * g);
        return static_cast<float>(recovered);
    }

    static std::vector<float> decimate(YouKnow106Engine& engine,
                                       const std::vector<float>& input,
                                       int factor)
    {
        if (factor == 1)
            return input;

        YouKnow106Engine::HalfbandDecimator first;
        YouKnow106Engine::HalfbandDecimator second;
        first.reset();
        second.reset();
        std::vector<float> firstStage;
        firstStage.reserve(input.size() / 2);
        for (std::size_t index = 0; index + 1 < input.size(); index += 2)
        {
            float left = 0.0f;
            float right = 0.0f;
            engine.downsamplePair(first, input[index], input[index],
                                  input[index + 1], input[index + 1],
                                  left, right);
            firstStage.push_back(left);
        }
        if (factor == 2)
            return firstStage;

        std::vector<float> output;
        output.reserve(firstStage.size() / 2);
        for (std::size_t index = 0; index + 1 < firstStage.size(); index += 2)
        {
            float left = 0.0f;
            float right = 0.0f;
            engine.downsamplePair(second, firstStage[index], firstStage[index],
                                  firstStage[index + 1], firstStage[index + 1],
                                  left, right);
            output.push_back(left);
        }
        return output;
    }

    static ScanCursor scanCursor(const YouKnow106Engine& engine) noexcept
    {
        return { engine.controlScanPhase_, engine.nextConverterWrite_ };
    }

    static void processOne(YouKnow106Engine& engine) noexcept
    {
        float left = 0.0f;
        float right = 0.0f;
        engine.process(&left, &right, 1);
    }

    static void prepareHoldStep(YouKnow106Engine& engine,
                                double internalRate) noexcept
    {
        engine.prepare(internalRate, 1, false);
        // Keep the converter dormant.  The qualification below owns a separate
        // scheduler trace; this fixture isolates only the production hold
        // recurrences that follow a write.
        engine.controlScanPhase_ = -100.0;
        engine.nextConverterWrite_ = YouKnow106Engine::converterWritesPerPass;
        auto& voice = engine.voices_[0];
        voice.dcoCv = 0.0f;
        voice.dcoCvTarget = 1.0f;
        engine.pwmVoltsTarget_ = 1.0f;
        engine.pwmVoltsFirstPole_ = 0.0f;
        engine.pwmVolts_ = 0.0f;
        engine.subCvTarget_ = 1.0f;
        engine.subCv_ = 0.0f;
    }

    static HoldState holdState(const YouKnow106Engine& engine) noexcept
    {
        return { engine.voices_[0].dcoCv,
                 engine.pwmVoltsFirstPole_, engine.pwmVolts_, engine.subCv_ };
    }

    static constexpr double dcoHoldSeconds() noexcept
    {
        return YouKnow106Engine::dcoHoldSlewSecondsVoiced;
    }

    static constexpr double pwmFirstSeconds() noexcept
    {
        return YouKnow106Engine::pwmHoldFirstPoleSeconds;
    }

    static constexpr double pwmSecondSeconds() noexcept
    {
        return YouKnow106Engine::pwmHoldSecondPoleSeconds;
    }

    static constexpr double subHoldSeconds() noexcept
    {
        return YouKnow106Engine::subHoldSlewSeconds;
    }

    static std::uint32_t dividerFor(double frequencyHz) noexcept
    {
        return YouKnow106Engine::dcoDivider(frequencyHz);
    }

    static double quantisedFrequency(std::uint32_t divider,
                                     DcoRange range) noexcept
    {
        return YouKnow106Engine::dcoQuantisedFrequency(divider, range);
    }
};
} // namespace youknow106

namespace
{
using youknow106::DcoRange;
using youknow106::YouKnow106Engine;
using Access = youknow106::YouKnow106TestAccess;

constexpr double pi = 3.141592653589793238462643383279502884;
constexpr double scanSeconds = 0.0042;
constexpr int analysisLength = 65536;
constexpr int settleHostFrames = 1024;
constexpr double analysisCeilingHz = 20000.0;
constexpr double strictBandHz = 15000.0;
constexpr int allowedMaskBins = 6;

struct PitchCase
{
    int midi {};
    DcoRange range {};
    const char* rangeName {};
};

struct DcoMetrics
{
    double worstAliasDb {-300.0};
    double worstControlSpurDb {-300.0};
    double worstControlGainAbsDb {};
    double worstStrictGainDb {};
    double worstStrictGainSignedDb {};
    double worstTopGainDb {};
    double worstTopGainSignedDb {};
    int controlTakes {};
    int invalidControlTakes {};
    int candidateTakes {};
    int validCandidateTakes {};
    int invalidCandidateTakes {};
    bool candidateFinite { true };
    std::string aliasCase;
    std::string controlSpurCase;
    std::string controlGainCase;

    struct GainWinner
    {
        Access::Waveform waveform {Access::Waveform::Saw};
        bool dutyApplies {};
        double duty {};
        int note {};
        const char* rangeName {""};
        double baseHz {};
        int harmonic {};
        double frequencyHz {};
    } strictWinner, topWinner;

    struct FoldCandidate
    {
        enum class Class { BoundaryStopband, PregridFold } candidateClass {};
        int harmonic {};
        double expectedPeak {};
        double sourceHz {};
        double internalFoldHz {};
        double hostFoldHz {};
        double signedErrorHz {};
    };
    int foldCandidateCount {};
    int boundaryStopbandCandidateCount {};
    int pregridFoldCandidateCount {};
    std::vector<FoldCandidate> foldCandidates;
};

struct ScanMetrics
{
    int mismatches {};
    int observedPasses {};
    int observedWrites {};
    double maximumQuantisationUs {};
};

struct HoldMetrics
{
    double dcoMaximumError {};
    double pwmFirstMaximumError {};
    double pwmMaximumError {};
    double subMaximumError {};
};

struct Cell
{
    int host {};
    int factor {};
    DcoMetrics dco;
    ScanMetrics scan;
    HoldMetrics hold;
    bool analysisPass {};
    bool dcoPass {};
    bool scanPass {};
    bool holdPass {};
    bool absolutePass {};
};

double quantisedFrequency(const PitchCase& item) noexcept
{
    const double wanted = 440.0 * std::pow(2.0, (item.midi - 69.0) / 12.0);
    const auto divider = Access::dividerFor(wanted);
    return Access::quantisedFrequency(divider, item.range);
}

void fft(std::vector<std::complex<double>>& values)
{
    const std::size_t size = values.size();
    for (std::size_t index = 1, reverse = 0; index < size; ++index)
    {
        std::size_t bit = size >> 1;
        for (; (reverse & bit) != 0; bit >>= 1)
            reverse ^= bit;
        reverse ^= bit;
        if (index < reverse)
            std::swap(values[index], values[reverse]);
    }
    for (std::size_t length = 2; length <= size; length <<= 1)
    {
        const auto step = std::polar(1.0, -2.0 * pi / static_cast<double>(length));
        for (std::size_t start = 0; start < size; start += length)
        {
            std::complex<double> rotation {1.0, 0.0};
            for (std::size_t offset = 0; offset < length / 2; ++offset)
            {
                const auto even = values[start + offset];
                const auto odd = values[start + offset + length / 2] * rotation;
                values[start + offset] = even + odd;
                values[start + offset + length / 2] = even - odd;
                rotation *= step;
            }
        }
    }
}

std::vector<double> blackmanHarrisWindow()
{
    std::vector<double> window(analysisLength);
    for (int index = 0; index < analysisLength; ++index)
    {
        const double phase = 2.0 * pi * index / (analysisLength - 1.0);
        window[static_cast<std::size_t>(index)] =
            0.35875 - 0.48829 * std::cos(phase)
          + 0.14128 * std::cos(2.0 * phase)
          - 0.01168 * std::cos(3.0 * phase);
    }
    return window;
}

std::complex<double> analyticCoefficient(Access::Waveform waveform,
                                         int harmonic, double duty,
                                         double reset)
{
    const double rise = 1.0 - reset;
    const double omega = 2.0 * pi * harmonic;
    const std::complex<double> jOmega {0.0, omega};
    switch (waveform)
    {
        case Access::Waveform::Saw:
        {
            const std::complex<double> edge =
                1.0 - std::polar(1.0, -omega * rise);
            // Complex Fourier coefficient of the two linear ramp segments,
            // including the shipping source's 6 V peak.
            return -12.0 * (1.0 / rise + 1.0 / reset)
                 * edge / (omega * omega);
        }
        case Access::Waveform::Pulse:
        {
            // The comparator is high between the rising-ramp and
            // falling-reset crossings.  Their separation is exactly `duty`.
            const double start = rise * (1.0 - duty);
            return 12.0
                 * (std::polar(1.0, -omega * start)
                    - std::polar(1.0, -omega * (start + duty))) / jOmega;
        }
        case Access::Waveform::Sub:
        {
            if (harmonic % 2 == 0)
                return {};
            // In sub-period coordinates, the divider is high from 0 to
            // rise/2 and from (1+rise)/2 to 1.  Its two levels are +/-5 V.
            return 10.0
                 * (1.0 - std::polar(1.0, -omega * rise * 0.5)
                    + std::polar(1.0, -omega * (1.0 + rise) * 0.5)
                    - std::polar(1.0, -omega)) / jOmega;
        }
    }
    return {};
}

double analyticDc(Access::Waveform waveform, double duty) noexcept
{
    return waveform == Access::Waveform::Pulse ? 6.0 * (2.0 * duty - 1.0)
                                                : 0.0;
}

double expectedPeak(Access::Waveform waveform, int harmonic,
                    double duty, double reset)
{
    return 2.0 * std::abs(
        analyticCoefficient(waveform, harmonic, duty, reset));
}

double waveformBaseFrequency(Access::Waveform waveform, double dcoFrequency)
{
    return waveform == Access::Waveform::Sub ? 0.5 * dcoFrequency : dcoFrequency;
}

const char* waveformName(Access::Waveform waveform) noexcept
{
    switch (waveform)
    {
        case Access::Waveform::Saw: return "saw";
        case Access::Waveform::Pulse: return "pulse";
        case Access::Waveform::Sub: return "sub";
    }
    return "unknown";
}

double measuredPeakAt(const std::vector<float>& samples,
                      const std::vector<double>& window,
                      double frequency, double sampleRate)
{
    const auto step = std::polar(1.0, -2.0 * pi * frequency / sampleRate);
    std::complex<double> rotation {1.0, 0.0};
    std::complex<double> sum {};
    double windowSum = 0.0;
    for (std::size_t index = 0; index < samples.size(); ++index)
    {
        sum += static_cast<double>(samples[index]) * window[index] * rotation;
        windowSum += window[index];
        rotation *= step;
    }
    return 2.0 * std::abs(sum) / windowSum;
}

double foldedFrequency(double frequency, double sampleRate) noexcept
{
    double wrapped = std::fmod(std::abs(frequency), sampleRate);
    if (wrapped < 0.0)
        wrapped += sampleRate;
    return std::min(wrapped, sampleRate - wrapped);
}

void markAllowedLine(std::vector<bool>& allowed, int centre)
{
    for (int offset = -allowedMaskBins; offset <= allowedMaskBins; ++offset)
        if (centre + offset >= 0
            && centre + offset < static_cast<int>(allowed.size()))
            allowed[static_cast<std::size_t>(centre + offset)] = true;
}

std::vector<bool> allowedSpectrumMask(double base, Access::Waveform waveform,
                                      double duty, double reset, int host)
{
    const double ceiling = std::min(analysisCeilingHz, 0.49 * host);
    const double fundamental = expectedPeak(waveform, 1, duty, reset);
    std::vector<bool> allowed(analysisLength / 2 + 1, false);
    if (std::abs(analyticDc(waveform, duty)) >= fundamental * 1.0e-5)
        markAllowedLine(allowed, 0);
    const int highestHarmonic = static_cast<int>(std::floor(ceiling / base));
    for (int harmonic = 1; harmonic <= highestHarmonic; ++harmonic)
    {
        const double expected = expectedPeak(waveform, harmonic, duty, reset);
        if (expected < fundamental * 1.0e-5)
            continue;
        const int bin = static_cast<int>(std::llround(
            harmonic * base * analysisLength / host));
        markAllowedLine(allowed, bin);
    }
    return allowed;
}

struct SpectrumWorst
{
    double peak {};
    int bin {};
};

SpectrumWorst worstOutsideMask(const std::vector<float>& samples,
                               const std::vector<double>& window,
                               const std::vector<bool>& allowed, int host)
{
    std::vector<std::complex<double>> spectrum(samples.size());
    double windowSum = 0.0;
    for (std::size_t index = 0; index < samples.size(); ++index)
    {
        spectrum[index] = static_cast<double>(samples[index]) * window[index];
        windowSum += window[index];
    }
    fft(spectrum);
    SpectrumWorst result;
    const int firstBin = static_cast<int>(std::ceil(
        20.0 * samples.size() / host));
    const int lastBin = static_cast<int>(std::floor(
        std::min(analysisCeilingHz, 0.49 * host) * samples.size() / host));
    for (int bin = firstBin; bin <= lastBin; ++bin)
    {
        if (allowed[static_cast<std::size_t>(bin)])
            continue;
        const double peak = 2.0
            * std::abs(spectrum[static_cast<std::size_t>(bin)]) / windowSum;
        if (peak > result.peak)
            result = { peak, bin };
    }
    return result;
}

std::vector<int> probeHarmonics(double baseFrequency, Access::Waveform waveform)
{
    std::set<int> probes { 1, 2, 3, 5, 7, 11, 17, 23, 31 };
    for (double boundary : { 5000.0, 10000.0, 15000.0, 20000.0 })
    {
        int harmonic = std::max(1, static_cast<int>(std::floor(boundary / baseFrequency)));
        if (waveform == Access::Waveform::Sub && harmonic % 2 == 0)
            --harmonic;
        if (harmonic > 0)
            probes.insert(harmonic);
    }
    return { probes.begin(), probes.end() };
}

struct ControlTakeMetrics
{
    double spurDb {-300.0};
    double gainAbsDb {};
    bool valid {};
};

using ControlKey = std::tuple<int, int, int, int, int>;

ControlTakeMetrics analyticControlForTake(int host, const PitchCase& pitch,
                                          Access::Waveform waveform,
                                          double duty)
{
    static std::map<ControlKey, ControlTakeMetrics> cache;
    const ControlKey key { host, pitch.midi, static_cast<int>(pitch.range),
                           static_cast<int>(waveform),
                           static_cast<int>(std::llround(100.0 * duty)) };
    if (const auto found = cache.find(key); found != cache.end())
        return found->second;

    static const std::vector<double> window = blackmanHarrisWindow();
    const double dcoFrequency = quantisedFrequency(pitch);
    const double base = waveformBaseFrequency(waveform, dcoFrequency);
    const double reset = std::clamp(2.2e-6 * dcoFrequency, 1.0e-6, 0.25);
    const double fundamental = expectedPeak(waveform, 1, duty, reset);
    const double ceiling = std::min(analysisCeilingHz, 0.49 * host);
    const auto allowed = allowedSpectrumMask(base, waveform, duty, reset, host);

    // Synthesize the ideal in-band multi-line reference from its complex
    // Fourier coefficients.  Starting a fresh recurrence every 1024 samples
    // bounds oscillator drift without changing the deterministic zero-phase
    // reference.  Accumulation remains double until the one float conversion
    // used by the candidate analyzer too.
    std::vector<double> accumulated(
        analysisLength, analyticDc(waveform, duty));
    const int highestHarmonic = static_cast<int>(std::floor(ceiling / base));
    constexpr int recurrenceBlock = 1024;
    for (int harmonic = 1; harmonic <= highestHarmonic; ++harmonic)
    {
        const auto coefficient =
            analyticCoefficient(waveform, harmonic, duty, reset);
        if (2.0 * std::abs(coefficient) < fundamental * 1.0e-5)
            continue;
        const double angle = 2.0 * pi * harmonic * base / host;
        const double stepReal = std::cos(angle);
        const double stepImag = std::sin(angle);
        for (int block = 0; block < analysisLength;
             block += recurrenceBlock)
        {
            const auto atBlock = coefficient
                * std::polar(1.0, angle * static_cast<double>(block));
            double real = atBlock.real();
            double imaginary = atBlock.imag();
            const int end = std::min(block + recurrenceBlock, analysisLength);
            for (int index = block; index < end; ++index)
            {
                accumulated[static_cast<std::size_t>(index)] += 2.0 * real;
                const double nextReal = real * stepReal - imaginary * stepImag;
                imaginary = real * stepImag + imaginary * stepReal;
                real = nextReal;
            }
        }
    }
    std::vector<float> reference(analysisLength);
    std::transform(accumulated.begin(), accumulated.end(), reference.begin(),
                   [](double sample) { return static_cast<float>(sample); });

    const auto offMask = worstOutsideMask(reference, window, allowed, host);
    ControlTakeMetrics result;
    result.spurDb = 20.0 * std::log10(
        std::max(offMask.peak, 1.0e-30) / std::max(fundamental, 1.0e-30));
    for (const int harmonic : probeHarmonics(base, waveform))
    {
        const double frequency = harmonic * base;
        const double expected = expectedPeak(waveform, harmonic, duty, reset);
        if (!(frequency > 0.0 && frequency <= ceiling)
            || expected < fundamental * 1.0e-2)
            continue;
        const double measured = measuredPeakAt(reference, window, frequency, host);
        const double errorDb = 20.0 * std::log10(
            std::max(measured, 1.0e-30) / expected);
        result.gainAbsDb = std::max(result.gainAbsDb, std::abs(errorDb));
    }
    result.valid = std::isfinite(result.spurDb)
                && std::isfinite(result.gainAbsDb)
                && result.spurDb <= -85.0
                && result.gainAbsDb <= 0.025;
    cache.emplace(key, result);
    return result;
}

std::vector<DcoMetrics::FoldCandidate> foldCandidateFamily(
    double base, double hostRate, int factor, double candidateFrequency,
    Access::Waveform waveform, double duty, double reset, int& total,
    int& boundaryStopbandTotal, int& pregridFoldTotal)
{
    const double internalRate = hostRate * factor;
    const int firstAboveHostNyquist =
        static_cast<int>(std::floor(0.5 * hostRate / base)) + 1;
    constexpr int searchLast = 4096;
    const double halfBin = 0.5 * hostRate / analysisLength;
    std::vector<DcoMetrics::FoldCandidate> candidates;
    boundaryStopbandTotal = 0;
    pregridFoldTotal = 0;
    for (int harmonic = firstAboveHostNyquist;
         harmonic <= searchLast; ++harmonic)
    {
        const double expected = expectedPeak(waveform, harmonic, duty, reset);
        if (expected <= 1.0e-12)
            continue;
        const double source = harmonic * base;
        const double internalFold = foldedFrequency(source, internalRate);
        const double hostFold = foldedFrequency(internalFold, hostRate);
        const double signedError = hostFold - candidateFrequency;
        if (std::abs(signedError) <= halfBin)
        {
            const auto candidateClass = source <= 0.5 * internalRate
                ? DcoMetrics::FoldCandidate::Class::BoundaryStopband
                : DcoMetrics::FoldCandidate::Class::PregridFold;
            if (candidateClass
                == DcoMetrics::FoldCandidate::Class::BoundaryStopband)
                ++boundaryStopbandTotal;
            else
                ++pregridFoldTotal;
            candidates.push_back({ candidateClass, harmonic, expected, source,
                                   internalFold, hostFold, signedError });
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& left,
                                                        const auto& right) {
        if (left.expectedPeak != right.expectedPeak)
            return left.expectedPeak > right.expectedPeak;
        return left.harmonic < right.harmonic;
    });
    total = static_cast<int>(candidates.size());
    if (candidates.size() > 3)
        candidates.resize(3);
    return candidates;
}

void analyseDcoTake(const std::vector<float>& samples, int host, int factor,
                    const PitchCase& pitch, Access::Waveform waveform,
                    double duty, DcoMetrics& aggregate)
{
    static const std::vector<double> window = blackmanHarrisWindow();
    const double dcoFrequency = quantisedFrequency(pitch);
    const double base = waveformBaseFrequency(waveform, dcoFrequency);
    const double reset = std::clamp(2.2e-6 * dcoFrequency, 1.0e-6, 0.25);
    const double fundamental = expectedPeak(waveform, 1, duty, reset);
    const double ceiling = std::min(analysisCeilingHz, 0.49 * host);

    const auto control = analyticControlForTake(host, pitch, waveform, duty);
    ++aggregate.controlTakes;
    if (!control.valid)
        ++aggregate.invalidControlTakes;
    const std::string takeCase = std::string(waveformName(waveform))
        + (waveform == Access::Waveform::Pulse
               ? ("-d" + std::to_string(duty)) : "")
        + "/note" + std::to_string(pitch.midi) + "/" + pitch.rangeName;
    if (control.spurDb > aggregate.worstControlSpurDb)
    {
        aggregate.worstControlSpurDb = control.spurDb;
        aggregate.controlSpurCase = takeCase;
    }
    if (control.gainAbsDb > aggregate.worstControlGainAbsDb)
    {
        aggregate.worstControlGainAbsDb = control.gainAbsDb;
        aggregate.controlGainCase = takeCase;
    }

    for (const int harmonic : probeHarmonics(base, waveform))
    {
        const double frequency = harmonic * base;
        const double expected = expectedPeak(waveform, harmonic, duty, reset);
        if (!(frequency > 0.0 && frequency <= ceiling)
            || expected < fundamental * 1.0e-2)
            continue;
        const double measured = measuredPeakAt(samples, window, frequency, host);
        const double errorDb = 20.0 * std::log10(
            std::max(measured, 1.0e-30) / expected);
        const double absolute = std::abs(errorDb);
        if (frequency <= strictBandHz && absolute > aggregate.worstStrictGainDb)
        {
            aggregate.worstStrictGainDb = absolute;
            aggregate.worstStrictGainSignedDb = errorDb;
            aggregate.strictWinner = { waveform,
                waveform == Access::Waveform::Pulse, duty, pitch.midi,
                pitch.rangeName, base, harmonic, frequency };
        }
        if (frequency > strictBandHz && absolute > aggregate.worstTopGainDb)
        {
            aggregate.worstTopGainDb = absolute;
            aggregate.worstTopGainSignedDb = errorDb;
            aggregate.topWinner = { waveform,
                waveform == Access::Waveform::Pulse, duty, pitch.midi,
                pitch.rangeName, base, harmonic, frequency };
        }
    }

    const auto allowed = allowedSpectrumMask(base, waveform, duty, reset, host);
    const auto offMask = worstOutsideMask(samples, window, allowed, host);
    const double aliasDb = 20.0 * std::log10(
        std::max(offMask.peak, 1.0e-30) / std::max(fundamental, 1.0e-30));
    if (aliasDb > aggregate.worstAliasDb)
    {
        const double candidateFrequency =
            offMask.bin * host / static_cast<double>(samples.size());
        aggregate.worstAliasDb = aliasDb;
        aggregate.foldCandidates = foldCandidateFamily(
            base, host, factor, candidateFrequency, waveform, duty, reset,
            aggregate.foldCandidateCount,
            aggregate.boundaryStopbandCandidateCount,
            aggregate.pregridFoldCandidateCount);
        aggregate.aliasCase = std::string(waveformName(waveform))
                            + (waveform == Access::Waveform::Pulse
                                   ? ("-d" + std::to_string(duty)) : "")
                            + "/note" + std::to_string(pitch.midi) + "/"
                            + pitch.rangeName + "/base"
                            + std::to_string(base) + "Hz@"
                            + std::to_string(candidateFrequency)
                            + "Hz";
    }
}

DcoMetrics runDcoMatrix(int host, int factor)
{
    constexpr std::array<int, 6> notes { 36, 48, 60, 72, 84, 96 };
    constexpr std::array<std::pair<DcoRange, const char*>, 3> ranges {{
        { DcoRange::Sixteen, "16" }, { DcoRange::Eight, "8" },
        { DcoRange::Four, "4" }
    }};
    constexpr std::array<double, 3> duties { 0.05, 0.50, 0.95 };
    DcoMetrics result;

    for (const int note : notes)
    {
        for (const auto& [range, rangeName] : ranges)
        {
            const PitchCase pitch { note, range, rangeName };
            const double frequency = quantisedFrequency(pitch);
            const auto run = [&](Access::Waveform waveform, double duty) {
                ++result.candidateTakes;
                YouKnow106Engine engine;
                Access::configureDco(engine, host * factor, frequency, range,
                                     waveform, static_cast<float>(duty));
                const int internalFrames = (settleHostFrames + analysisLength) * factor;
                std::vector<float> internal(static_cast<std::size_t>(internalFrames));
                for (auto& sample : internal)
                    sample = Access::renderRecoveredMixed(engine);
                auto boundary = Access::decimate(engine, internal, factor);
                const auto requiredBoundaryFrames = static_cast<std::size_t>(
                    settleHostFrames + analysisLength);
                if (boundary.size() < requiredBoundaryFrames)
                {
                    result.candidateFinite = false;
                    ++result.invalidCandidateTakes;
                    return;
                }
                std::vector<float> take(
                    boundary.begin() + settleHostFrames,
                    boundary.begin() + settleHostFrames + analysisLength);
                const auto allFinite = [](const auto& samples) {
                    return std::all_of(
                        samples.begin(), samples.end(), [](const auto sample) {
                            return std::isfinite(static_cast<double>(sample));
                        });
                };
                const bool finite = allFinite(internal)
                                 && allFinite(boundary)
                                 && allFinite(take);
                result.candidateFinite = result.candidateFinite && finite;
                if (!finite)
                {
                    ++result.invalidCandidateTakes;
                    return;
                }
                ++result.validCandidateTakes;
                analyseDcoTake(take, host, factor, pitch, waveform, duty, result);
            };
            run(Access::Waveform::Saw, 0.5);
            run(Access::Waveform::Sub, 0.5);
            for (const double duty : duties)
                run(Access::Waveform::Pulse, duty);
        }
    }
    return result;
}

ScanMetrics runScanMatrix(int host, int factor)
{
    const double internalRate = host * factor;
    const double passFrames = internalRate * scanSeconds;
    const int passes = host == 44100 ? 50 : 5;
    const std::uint64_t frameLimit = static_cast<std::uint64_t>(
        std::ceil(passes * passFrames)) + 2u;
    YouKnow106Engine engine;
    engine.prepare(internalRate, 1, false);

    ScanMetrics result;
    int pass = 0;
    for (std::uint64_t frame = 0; frame < frameLimit && pass < passes; ++frame)
    {
        const auto before = Access::scanCursor(engine);
        Access::processOne(engine);
        const auto after = Access::scanCursor(engine);
        std::size_t first = before.nextWrite;
        if (after.nextWrite < before.nextWrite)
        {
            ++pass;
            first = 0;
        }
        if (pass >= passes)
            break;
        for (std::size_t ordinal = first; ordinal < after.nextWrite; ++ordinal)
        {
            ++result.observedWrites;
            if (ordinal == 0)
                ++result.observedPasses;
            const double idealFrames =
                (pass + static_cast<double>(ordinal) / 23.0) * passFrames;
            const auto expected = static_cast<std::uint64_t>(
                std::ceil(idealFrames - 1.0e-10));
            if (frame != expected)
                ++result.mismatches;
            const double errorUs = (static_cast<double>(frame) - idealFrames)
                                 / internalRate * 1.0e6;
            result.maximumQuantisationUs = std::max(
                result.maximumQuantisationUs, std::abs(errorUs));
        }
        (void) before.phase;
    }
    return result;
}

HoldMetrics runHoldMatrix(int host, int factor)
{
    const double internalRate = host * factor;
    const double duration = 8.0 * Access::subHoldSeconds();
    const int frames = static_cast<int>(std::ceil(duration * internalRate));
    YouKnow106Engine engine;
    Access::prepareHoldStep(engine, internalRate);
    HoldMetrics result;
    const double first = Access::pwmFirstSeconds();
    const double second = Access::pwmSecondSeconds();
    for (int frame = 0; frame < frames; ++frame)
    {
        Access::processOne(engine);
        const auto state = Access::holdState(engine);
        const double time = (frame + 1.0) / internalRate;
        const double dcoReference = 1.0 - std::exp(-time / Access::dcoHoldSeconds());
        const double subReference = 1.0 - std::exp(-time / Access::subHoldSeconds());
        const double pwmFirstReference = 1.0 - std::exp(-time / first);
        const double pwmReference = 1.0
            - (first * std::exp(-time / first)
               - second * std::exp(-time / second)) / (first - second);
        result.dcoMaximumError = std::max(
            result.dcoMaximumError, std::abs(state.dco - dcoReference));
        result.pwmFirstMaximumError = std::max(
            result.pwmFirstMaximumError,
            std::abs(state.pwmFirst - pwmFirstReference));
        result.pwmMaximumError = std::max(
            result.pwmMaximumError, std::abs(state.pwmSecond - pwmReference));
        result.subMaximumError = std::max(
            result.subMaximumError, std::abs(state.sub - subReference));
    }
    return result;
}

Cell runCell(int host, int factor)
{
    Cell cell;
    cell.host = host;
    cell.factor = factor;
    cell.dco = runDcoMatrix(host, factor);
    cell.scan = runScanMatrix(host, factor);
    cell.hold = runHoldMatrix(host, factor);
    const double topGate = host == 44100 ? 0.75 : 0.25;
    cell.analysisPass = cell.dco.controlTakes == 90
                     && cell.dco.invalidControlTakes == 0
                     && cell.dco.candidateTakes == 90
                     && cell.dco.validCandidateTakes == 90
                     && cell.dco.invalidCandidateTakes == 0
                     && cell.dco.candidateFinite
                     && std::isfinite(cell.dco.worstControlSpurDb)
                     && std::isfinite(cell.dco.worstControlGainAbsDb)
                     && !cell.dco.controlSpurCase.empty()
                     && !cell.dco.controlGainCase.empty()
                     && cell.dco.worstControlSpurDb <= -85.0
                     && cell.dco.worstControlGainAbsDb <= 0.025;
    cell.dcoPass = std::isfinite(cell.dco.worstAliasDb)
                && std::isfinite(cell.dco.worstStrictGainDb)
                && std::isfinite(cell.dco.worstTopGainDb)
                && !cell.dco.aliasCase.empty()
                && cell.dco.strictWinner.harmonic > 0
                && cell.dco.topWinner.harmonic > 0
                && cell.dco.worstAliasDb <= -70.0
                && cell.dco.worstStrictGainDb <= 0.25
                && cell.dco.worstTopGainDb <= topGate;
    cell.scanPass = cell.scan.mismatches == 0
                 && cell.scan.observedPasses == (host == 44100 ? 50 : 5)
                 && cell.scan.observedWrites == 23 * cell.scan.observedPasses
                 && cell.scan.maximumQuantisationUs
                        < 1.0e6 / (host * factor) + 1.0e-6;
    cell.holdPass = cell.hold.dcoMaximumError <= 2.0e-5
                 && cell.hold.pwmFirstMaximumError <= 2.0e-11
                 && cell.hold.pwmMaximumError <= 2.0e-11
                 && cell.hold.subMaximumError <= 2.0e-11;
    cell.absolutePass = cell.analysisPass && cell.dcoPass
                     && cell.scanPass && cell.holdPass;
    return cell;
}

void printCell(const Cell& cell)
{
    std::cout << std::fixed << std::setprecision(6)
              << "cell host=" << cell.host
              << " factor=" << cell.factor
              << " alias_db=" << cell.dco.worstAliasDb
              << " alias_case=" << cell.dco.aliasCase
              << " control_takes=" << cell.dco.controlTakes
              << " control_invalid=" << cell.dco.invalidControlTakes
              << " candidate_takes=" << cell.dco.candidateTakes
              << " candidate_valid=" << cell.dco.validCandidateTakes
              << " candidate_invalid=" << cell.dco.invalidCandidateTakes
              << " candidate_finite="
              << (cell.dco.candidateFinite ? "PASS" : "FAIL")
              << " control_spur_db=" << cell.dco.worstControlSpurDb
              << " control_spur_case=" << cell.dco.controlSpurCase
              << " control_gain_abs_db=" << cell.dco.worstControlGainAbsDb
              << " control_gain_case=" << cell.dco.controlGainCase
              << " fold_attribution=UNATTRIBUTED"
              << " fold_candidate_family_count="
              << cell.dco.foldCandidateCount
              << " fold_boundary_stopband_count="
              << cell.dco.boundaryStopbandCandidateCount
              << " fold_pregrid_fold_count="
              << cell.dco.pregridFoldCandidateCount
              << " fold_candidate_reported=" << cell.dco.foldCandidates.size();
    for (std::size_t index = 0; index < cell.dco.foldCandidates.size(); ++index)
    {
        const auto& candidate = cell.dco.foldCandidates[index];
        const auto ordinal = index + 1;
        std::cout << " fold_candidate_" << ordinal << "_class="
                  << (candidate.candidateClass
                          == DcoMetrics::FoldCandidate::Class::BoundaryStopband
                      ? "boundary_stopband" : "pregrid_fold")
                  << " fold_candidate_" << ordinal << "_harmonic="
                  << candidate.harmonic
                  << " fold_candidate_" << ordinal << "_peak="
                  << candidate.expectedPeak
                  << " fold_candidate_" << ordinal << "_source_hz="
                  << candidate.sourceHz
                  << " fold_candidate_" << ordinal << "_internal_fold_hz="
                  << candidate.internalFoldHz
                  << " fold_candidate_" << ordinal << "_host_fold_hz="
                  << candidate.hostFoldHz
                  << " fold_candidate_" << ordinal << "_signed_error_hz="
                  << candidate.signedErrorHz;
    }
    const auto printGain = [](const char* prefix, double absolute,
                              double signedError,
                              const DcoMetrics::GainWinner& winner) {
        std::cout << ' ' << prefix << "_gain_abs_db=" << absolute
                  << ' ' << prefix << "_gain_signed_db=" << signedError
                  << ' ' << prefix << "_waveform="
                  << waveformName(winner.waveform)
                  << ' ' << prefix << "_duty=";
        if (winner.dutyApplies)
            std::cout << winner.duty;
        else
            std::cout << "NA";
        std::cout << ' ' << prefix << "_note=" << winner.note
                  << ' ' << prefix << "_range=" << winner.rangeName
                  << ' ' << prefix << "_base_hz=" << winner.baseHz
                  << ' ' << prefix << "_harmonic=" << winner.harmonic
                  << ' ' << prefix << "_frequency_hz=" << winner.frequencyHz;
    };
    printGain("strict", cell.dco.worstStrictGainDb,
              cell.dco.worstStrictGainSignedDb, cell.dco.strictWinner);
    printGain("top", cell.dco.worstTopGainDb,
              cell.dco.worstTopGainSignedDb, cell.dco.topWinner);
    std::cout
              << " scan_mismatches=" << cell.scan.mismatches
              << " scan_passes=" << cell.scan.observedPasses
              << " scan_writes=" << cell.scan.observedWrites
              << " scan_quantisation_max_us=" << cell.scan.maximumQuantisationUs
              << std::scientific << std::setprecision(9)
              << " dco_hold_max_abs=" << cell.hold.dcoMaximumError
              << " pwm_first_hold_max_abs=" << cell.hold.pwmFirstMaximumError
              << " pwm_hold_max_abs=" << cell.hold.pwmMaximumError
              << " sub_hold_max_abs=" << cell.hold.subMaximumError
              << std::fixed << std::setprecision(6)
              << " analysis=" << (cell.analysisPass ? "PASS" : "INVALID")
              << " dco="
              << (!cell.analysisPass ? "ANALYSIS_INVALID"
                                      : (cell.dcoPass ? "PASS" : "REJECT"))
              << " scan=" << (cell.scanPass ? "PASS" : "REJECT")
              << " holds=" << (cell.holdPass ? "PASS" : "REJECT")
              << " combined="
              << (!cell.analysisPass ? "ANALYSIS_INVALID"
                                      : (cell.absolutePass ? "PASS" : "REJECT"))
              << '\n';
}

int run(bool selfTest)
{
    std::cout << "schema dco_scan_quality 3\n"
              << "boundary isolated pre-VCF DCO/scan domain; internal=q*host, "
                 "shipping halfbands return q2/q4 to host; no whole-engine parity\n"
              << "reference finite-reset piecewise-linear saw Fourier integral, "
                 "rectangular pulse/sub Fourier series, ordinal/23 scheduler, "
                 "exact exponential one-poles and affine two-pole PWM cascade\n"
              << "frequency shipping dcoDivider+dcoQuantisedFrequency; fixture "
                 "directly injects periodSamples=internal_rate/quantised_frequency\n"
              << "grid host={44100,48000} factor={1,2,4} "
                 "notes={36,48,60,72,84,96} ranges={16,8,4} "
                 "pulse_duty={0.05,0.50,0.95}\n"
              << "analysis blackman_harris_4term length=" << analysisLength
              << " allowed_half_width_bins=" << allowedMaskBins
              << " (first-null half-width=4 bins) per_take_control="
                 "analytic_complex_multiline_zero_phase control_spur_gate_db=-85 "
                 "control_gain_gate_db=0.025 alias_gate_db=-70 "
                 "fold_candidates=amplitude_ranked_unattributed "
                 "fold_classes={boundary_stopband,pregrid_fold} "
                 "search_harmonic_max=4096\n";

    std::vector<Cell> cells;
    for (const int host : { 44100, 48000 })
        for (const int factor : { 1, 2, 4 })
        {
            cells.push_back(runCell(host, factor));
            printCell(cells.back());
        }

    if (!selfTest)
        return 0;

    // The expected classification is intentionally explicit.  Calibrate this
    // only from a reviewed full-matrix run: the self-test is a deterministic
    // regression on that evidence, not a mechanism that blesses every result.
    constexpr std::array<double, 6> expectedAliasDb {
        -83.476933, -82.436627, -82.432588,
        -84.879008, -92.976529, -92.978397
    };
    constexpr std::array<double, 6> expectedControlSpurDb {
        -92.954176, -92.954176, -92.954176,
        -92.958785, -92.958785, -92.958785
    };
    constexpr std::array<double, 6> expectedControlGainDb {
        0.000032, 0.000032, 0.000032,
        0.000034, 0.000034, 0.000034
    };
    constexpr std::array<double, 6> expectedStrictGainDb {
        0.002208, 0.000536, 0.000196,
        0.002242, 0.000448, 0.000179
    };
    constexpr std::array<double, 6> expectedStrictSignedDb {
        -0.002208, -0.000536, -0.000196,
        -0.002242, -0.000448, 0.000179
    };
    constexpr std::array<double, 6> expectedTopGainDb {
        0.035644, 0.021486, 0.020735,
        0.004093, 0.001001, 0.000528
    };
    constexpr std::array<double, 6> expectedTopSignedDb {
        -0.035644, -0.021486, -0.020735,
        -0.004093, -0.001001, 0.000528
    };
    constexpr std::array<int, 6> expectedFoldCandidates { 1, 1, 1, 5, 0, 0 };
    constexpr std::array<int, 6> expectedBoundaryCandidates { 0, 1, 1, 0, 0, 0 };
    constexpr std::array<int, 6> expectedPregridCandidates { 1, 0, 0, 5, 0, 0 };
    constexpr std::array<double, 6> expectedScanQuantisationUs {
        22.656019, 11.318150, 5.659075,
        20.652174, 10.326087, 5.163043
    };
    bool layoutClass = cells.size() == expectedAliasDb.size();
    bool analysisClass = layoutClass;
    bool aliasClass = layoutClass;
    bool strictClass = layoutClass;
    bool topClass = layoutClass;
    bool scanClass = layoutClass;
    bool holdClass = layoutClass;
    bool foldCandidateClass = layoutClass;
    bool classificationClass = layoutClass;
    for (std::size_t index = 0;
         index < std::min(cells.size(), expectedAliasDb.size()); ++index)
    {
        const auto& cell = cells[index];
        const int expectedHost = index < 3 ? 44100 : 48000;
        const int expectedFactor = std::array { 1, 2, 4 }[index % 3];
        const bool layout = cell.host == expectedHost
                         && cell.factor == expectedFactor;
        const bool analysisMetric =
            std::isfinite(cell.dco.worstControlSpurDb)
            && std::isfinite(cell.dco.worstControlGainAbsDb)
            && cell.dco.controlTakes == 90
            && cell.dco.invalidControlTakes == 0
            && cell.dco.candidateTakes == 90
            && cell.dco.validCandidateTakes == 90
            && cell.dco.invalidCandidateTakes == 0
            && cell.dco.candidateFinite
            && cell.dco.worstControlSpurDb <= -85.0
            && cell.dco.worstControlGainAbsDb <= 0.025
            && std::abs(cell.dco.worstControlSpurDb
                        - expectedControlSpurDb[index]) <= 0.10
            && std::abs(cell.dco.worstControlGainAbsDb
                        - expectedControlGainDb[index]) <= 0.001
            && !cell.dco.controlSpurCase.empty()
            && !cell.dco.controlGainCase.empty()
            && cell.analysisPass;
        const bool aliasMetric = std::isfinite(cell.dco.worstAliasDb)
            && cell.dco.worstAliasDb <= -70.0
            && std::abs(cell.dco.worstAliasDb
                        - expectedAliasDb[index]) <= 0.75
            && !cell.dco.aliasCase.empty();
        const bool strictMetric =
            std::isfinite(cell.dco.worstStrictGainDb)
            && std::isfinite(cell.dco.worstStrictGainSignedDb)
            && std::abs(cell.dco.worstStrictGainDb
                        - expectedStrictGainDb[index]) <= 0.10
            && std::abs(cell.dco.worstStrictGainSignedDb
                        - expectedStrictSignedDb[index]) <= 0.10
            && std::abs(std::abs(cell.dco.worstStrictGainSignedDb)
                        - cell.dco.worstStrictGainDb) <= 1.0e-9
            && cell.dco.worstStrictGainDb <= 0.25
            && cell.dco.strictWinner.note > 0
            && cell.dco.strictWinner.harmonic > 0
            && cell.dco.strictWinner.baseHz > 0.0
            && cell.dco.strictWinner.frequencyHz > 0.0
            && cell.dco.strictWinner.dutyApplies
                   == (cell.dco.strictWinner.waveform == Access::Waveform::Pulse)
            && std::abs(cell.dco.strictWinner.frequencyHz
                        - cell.dco.strictWinner.baseHz
                            * cell.dco.strictWinner.harmonic) <= 1.0e-6
            && cell.dco.strictWinner.rangeName[0] != '\0';
        const bool topMetric = std::isfinite(cell.dco.worstTopGainDb)
            && std::isfinite(cell.dco.worstTopGainSignedDb)
            && std::abs(cell.dco.worstTopGainDb
                        - expectedTopGainDb[index]) <= 0.10
            && std::abs(cell.dco.worstTopGainSignedDb
                        - expectedTopSignedDb[index]) <= 0.10
            && std::abs(std::abs(cell.dco.worstTopGainSignedDb)
                        - cell.dco.worstTopGainDb) <= 1.0e-9
            && cell.dco.worstTopGainDb
                   <= (expectedHost == 44100 ? 0.75 : 0.25)
            && cell.dco.topWinner.note > 0
            && cell.dco.topWinner.harmonic > 0
            && cell.dco.topWinner.baseHz > 0.0
            && cell.dco.topWinner.frequencyHz > strictBandHz
            && cell.dco.topWinner.dutyApplies
                   == (cell.dco.topWinner.waveform == Access::Waveform::Pulse)
            && std::abs(cell.dco.topWinner.frequencyHz
                        - cell.dco.topWinner.baseHz
                            * cell.dco.topWinner.harmonic) <= 1.0e-6
            && cell.dco.topWinner.rangeName[0] != '\0';
        const int expectedPasses = expectedHost == 44100 ? 50 : 5;
        const bool scanMetric = cell.scan.mismatches == 0
            && cell.scan.observedPasses == expectedPasses
            && cell.scan.observedWrites == 23 * expectedPasses
            && std::isfinite(cell.scan.maximumQuantisationUs)
            && std::abs(cell.scan.maximumQuantisationUs
                        - expectedScanQuantisationUs[index]) <= 0.10
            && cell.scanPass;
        const bool holdMetric =
            std::isfinite(cell.hold.dcoMaximumError)
            && std::isfinite(cell.hold.pwmFirstMaximumError)
            && std::isfinite(cell.hold.pwmMaximumError)
            && std::isfinite(cell.hold.subMaximumError)
            && cell.hold.pwmMaximumError <= 2.0e-11
            && cell.hold.pwmFirstMaximumError <= 2.0e-11
            && cell.hold.dcoMaximumError <= 1.0e-5
            && cell.hold.subMaximumError <= 2.0e-11
            && cell.holdPass;
        bool foldMetric = cell.dco.foldCandidates.size()
                              == static_cast<std::size_t>(
                                  std::min(expectedFoldCandidates[index], 3))
                       && cell.dco.foldCandidateCount
                              == expectedFoldCandidates[index]
                       && cell.dco.boundaryStopbandCandidateCount
                              == expectedBoundaryCandidates[index]
                       && cell.dco.pregridFoldCandidateCount
                              == expectedPregridCandidates[index]
                       && cell.dco.foldCandidateCount
                              == cell.dco.boundaryStopbandCandidateCount
                               + cell.dco.pregridFoldCandidateCount
                       && cell.dco.foldCandidateCount
                              >= static_cast<int>(cell.dco.foldCandidates.size());
        double previousAmplitude = std::numeric_limits<double>::infinity();
        for (const auto& candidate : cell.dco.foldCandidates)
        {
            foldMetric = foldMetric
                && candidate.harmonic > 0
                && std::isfinite(candidate.expectedPeak)
                && std::isfinite(candidate.sourceHz)
                && std::isfinite(candidate.internalFoldHz)
                && std::isfinite(candidate.hostFoldHz)
                && std::isfinite(candidate.signedErrorHz)
                && candidate.expectedPeak > 0.0
                && candidate.expectedPeak <= previousAmplitude
                && candidate.sourceHz > 0.5 * cell.host
                && candidate.internalFoldHz >= 0.0
                && candidate.internalFoldHz <= 0.5 * cell.host * cell.factor
                && candidate.hostFoldHz >= 0.0
                && candidate.hostFoldHz <= 0.5 * cell.host
                && std::abs(candidate.signedErrorHz)
                       <= 0.5 * cell.host / analysisLength + 1.0e-9;
            if (candidate.candidateClass
                == DcoMetrics::FoldCandidate::Class::BoundaryStopband)
                foldMetric = foldMetric
                    && candidate.sourceHz <= 0.5 * cell.host * cell.factor;
            else
                foldMetric = foldMetric
                    && candidate.sourceHz > 0.5 * cell.host * cell.factor;
            previousAmplitude = candidate.expectedPeak;
        }
        const bool classification = cell.analysisPass && cell.dcoPass
                                 && cell.scanPass && cell.holdPass
                                 && cell.absolutePass;
        layoutClass = layoutClass && layout;
        analysisClass = analysisClass && analysisMetric;
        aliasClass = aliasClass && aliasMetric;
        strictClass = strictClass && strictMetric;
        topClass = topClass && topMetric;
        scanClass = scanClass && scanMetric;
        holdClass = holdClass && holdMetric;
        foldCandidateClass = foldCandidateClass && foldMetric;
        classificationClass = classificationClass && classification;
        if (!(layout && analysisMetric && aliasMetric && strictMetric
              && topMetric && scanMetric && holdMetric && foldMetric
              && classification))
            std::cerr << "self-test row " << index
                      << " layout=" << layout
                      << " analysis=" << analysisMetric
                      << " alias=" << aliasMetric
                      << " strict=" << strictMetric
                      << " top=" << topMetric
                      << " scan=" << scanMetric
                      << " holds=" << holdMetric
                      << " fold_candidates=" << foldMetric
                      << " classification=" << classification << '\n';
    }
    const bool pass = layoutClass && analysisClass && aliasClass
                   && strictClass && topClass && scanClass && holdClass
                   && foldCandidateClass && classificationClass;
    std::cout << "self_test layout=" << (layoutClass ? "PASS" : "FAIL")
              << " analysis=" << (analysisClass ? "PASS" : "FAIL")
              << " alias=" << (aliasClass ? "PASS" : "FAIL")
              << " strict=" << (strictClass ? "PASS" : "FAIL")
              << " top=" << (topClass ? "PASS" : "FAIL")
              << " scan=" << (scanClass ? "PASS" : "FAIL")
              << " holds=" << (holdClass ? "PASS" : "FAIL")
              << " fold_candidates="
              << (foldCandidateClass ? "PASS" : "FAIL")
              << " classification="
              << (classificationClass ? "PASS" : "FAIL")
              << " expected=44.1:PASS,PASS,PASS;"
                 "48:PASS,PASS,PASS verdict="
              << (pass ? "PASS" : "FAIL") << '\n';
    return pass ? 0 : 1;
}
} // namespace

int main(int argc, char** argv)
{
    bool selfTest = false;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--self-test")
            selfTest = true;
        else if (argument == "--help")
        {
            std::cout << "usage: YouKnow106DcoScanQualityAudit [--self-test]\n";
            return 0;
        }
        else
        {
            std::cerr << "unknown argument: " << argument << '\n';
            return 2;
        }
    }
    return run(selfTest);
}
