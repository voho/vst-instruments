#include "DSP/YouKnowChorus.h"
#include "DSP/YouKnowEngine.h"
#include "OversamplingQualitySupport.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace youknow
{
struct YouKnowTestAccess
{
    struct ProcessingRate
    {
        int factor {};
        double internalRate {};
        bool hqRequested {};
    };

    struct State
    {
        int indexA {};
        int indexB {};
        double phaseA {};
        double phaseB {};
        std::uint32_t rngA {};
        std::uint32_t rngB {};
        float heldA {};
        float heldB {};
        float transferA {};
        float transferB {};
        double lfoPhase {};
        float wetGain {};
        float rateHz {};
        float centreDelay {};
        float sweep {};
        ChorusMode runningMode { ChorusMode::One };
        bool primed {};
    };

    static State state(const Chorus& chorus) noexcept
    {
        return { chorus.lineA_.writeIndex, chorus.lineB_.writeIndex,
                 chorus.lineA_.clockPhase, chorus.lineB_.clockPhase,
                 chorus.lineA_.noiseState, chorus.lineB_.noiseState,
                 chorus.lineA_.held, chorus.lineB_.held,
                 chorus.lineA_.transferState, chorus.lineB_.transferState,
                 chorus.lfoPhase_, chorus.wetGain_, chorus.rateHz_,
                 chorus.centreDelay_, chorus.sweep_, chorus.runningMode_,
                 chorus.primed_ };
    }

    static ProcessingRate shippingProcessingRate(double hostRate,
                                                  bool hqEnabled)
    {
        YouKnowEngine engine;
        engine.prepare(hostRate, 1, hqEnabled);
        return { engine.getOversamplingFactor(), engine.oversampledRate_,
                 engine.isOversamplingEnabled() };
    }

    struct StereoBoundary
    {
        std::vector<float> left;
        std::vector<float> right;
    };

    static StereoBoundary decimate(const std::vector<float>& internalLeft,
                                   const std::vector<float>& internalRight,
                                   int factor)
    {
        if (factor != 1 && factor != 2 && factor != 4)
            throw std::runtime_error("factor must be 1, 2 or 4");
        if (internalLeft.size() != internalRight.size()
            || internalLeft.size() % static_cast<std::size_t>(factor) != 0u)
            throw std::runtime_error("stereo render is not host-frame aligned");
        if (factor == 1)
            return { internalLeft, internalRight };

        YouKnowEngine engine;
        StereoBoundary output;
        output.left.resize(internalLeft.size()
                           / static_cast<std::size_t>(factor));
        output.right.resize(output.left.size());
        for (std::size_t host = 0; host < output.left.size(); ++host)
        {
            const auto base = host * static_cast<std::size_t>(factor);
            if (factor == 2)
            {
                engine.downsamplePair(
                    engine.firstDecimator_,
                    internalLeft[base], internalRight[base],
                    internalLeft[base + 1u], internalRight[base + 1u],
                    output.left[host], output.right[host]);
            }
            else
            {
                float firstLeft = 0.0f;
                float firstRight = 0.0f;
                float secondLeft = 0.0f;
                float secondRight = 0.0f;
                engine.downsamplePair(
                    engine.firstDecimator_,
                    internalLeft[base], internalRight[base],
                    internalLeft[base + 1u], internalRight[base + 1u],
                    firstLeft, firstRight);
                engine.downsamplePair(
                    engine.firstDecimator_,
                    internalLeft[base + 2u], internalRight[base + 2u],
                    internalLeft[base + 3u], internalRight[base + 3u],
                    secondLeft, secondRight);
                engine.downsamplePair(
                    engine.secondDecimator_,
                    firstLeft, firstRight, secondLeft, secondRight,
                    output.left[host], output.right[host]);
            }
        }
        return output;
    }

    static constexpr double shippingBoundaryDelayHostFrames(int factor) noexcept
    {
        constexpr double half = (YouKnowEngine::halfbandTaps - 1) / 2.0;
        if (factor == 2)
            return half / 2.0;
        if (factor == 4)
            return half / 4.0 + half / 2.0;
        return 0.0;
    }
};
}

namespace
{
using youknow::Chorus;
using youknow::ChorusMode;
using youknow::YouKnowTestAccess;
namespace quality = youknow::oversampling_quality;

constexpr long double pi = 3.141592653589793238462643383279502884L;
constexpr double centreDelay = 0.0039;
constexpr double sweepDelay = 0.0025;
constexpr double transferSmear = 0.8654743;
constexpr double saturationLevel = 1.1246614;
constexpr double saturationCurvature = 1.2044546;
constexpr double saturationExponent = 12.9395323;
constexpr double inputCouplingHz = 15.9155;
constexpr double inputPassiveHz = 7234.0;
constexpr double firstHz = 9688.0;
constexpr double firstQ = 0.5490625934422811;
constexpr double secondHz = 10377.0;
constexpr double secondQ = 1.2909944487358056;
constexpr double typicalOutputSourceEstimateOhms = 3700.0;
constexpr double outputSeriesOhms = 3300.0;
constexpr double outputReturnOhms = 47000.0;
constexpr double outputTapFarads = 2.2e-9;
constexpr double outputParallelDriveOhms =
    0.5 * (typicalOutputSourceEstimateOhms + outputSeriesOhms);
constexpr double reconstructionSeriesOhms = 22000.0;
constexpr double firstFeedbackFarads = 820.0e-12;
constexpr double firstShuntFarads = 680.0e-12;
constexpr double secondFeedbackFarads = 1.8e-9;
constexpr double secondShuntFarads = 270.0e-12;
constexpr double outputCapacitance = 1.0e-6;
constexpr double outputBleed = 22000.0;
constexpr double outputMixer = 39000.0;
constexpr double noiseAmplitude =
    0.200e-3 / (2.6 * 0.3894);
constexpr double modeTwoNoiseGain = 1.57579602;
constexpr double wetGainTarget = 47.0 / 39.0;
constexpr double wetTau = 0.005;
constexpr double hotFundamentalAmplitude = 0.8157048;
constexpr double hotUpperAmplitude = 0.1464086;
constexpr double nodeVoltsPerUnit = 2.6;
constexpr int oracleFactor = 16;
constexpr std::size_t filterTaps = 4097;
constexpr double duration = 0.72;
constexpr double offStart = 0.30;
constexpr double twoStart = 0.36;
constexpr double finalOff = 0.60;

constexpr double hotRelativeRmsGateDb = -40.0;
constexpr double hotModeRelativeRmsGateDb = -50.0;
constexpr double hotMuteRelativeRmsGateDb = -60.0;
constexpr double referenceConvergenceGateDb = -80.0;
constexpr double hotResidualSpectrumGateDb = -60.0;
constexpr double noiseLevelGateDb = 0.10;
constexpr double noiseBandPowerGateDb = 0.75;
constexpr double noiseCorrelationErrorGate = 0.02;
constexpr double noiseAbsoluteCorrelationGate = 0.05;
constexpr double noiseModeDeltaGateDb = 0.05;
constexpr int referenceCellPairs = 128;
constexpr float referenceMinimumClockHz = 10000.0f;
constexpr float referenceMaximumClockHz = 200000.0f;
constexpr float referenceLineNoiseAmplitude =
    0.200e-3f / (2.6f * 0.3894f);
constexpr float referenceModeTwoNoiseGain = 1.57579602f;
constexpr float referenceWetGain =
    std::bit_cast<float>(UINT32_C(0x3f9a41a5));
static_assert(Chorus::cellPairs == referenceCellPairs);
static_assert(Chorus::minimumClockHz == referenceMinimumClockHz);
static_assert(Chorus::maximumClockHz == referenceMaximumClockHz);
static_assert(Chorus::wetToDryGain == referenceWetGain);

double rateFor(bool modeOne)
{
    constexpr long double r5 = 1.0e6L;
    constexpr long double r8 = 2.2e6L;
    constexpr long double r4 = 680.0e3L;
    constexpr long double r3 = 2.2e6L;
    constexpr long double beta = 33.0L / 47.0L;
    constexpr long double cap = 0.1e-6L;
    const long double shunt = modeOne ? r4 : r4 + r3;
    const long double parallel = shunt * r8 / (shunt + r8);
    const long double effective = r8 * r5 / parallel + r8;
    return static_cast<double>(1.0L / (4.0L * beta * effective * cap));
}

enum class SegmentMode { One, OffOne, Two, OffTwo };

SegmentMode segmentAt(double time)
{
    if (time < offStart)
        return SegmentMode::One;
    if (time < twoStart)
        return SegmentMode::OffOne;
    if (time < finalOff)
        return SegmentMode::Two;
    return SegmentMode::OffTwo;
}

double nextModeBoundary(double time)
{
    if (time < offStart - 1.0e-14)
        return offStart;
    if (time < twoStart - 1.0e-14)
        return twoStart;
    if (time < finalOff - 1.0e-14)
        return finalOff;
    return duration;
}

double segmentRate(SegmentMode mode)
{
    return (mode == SegmentMode::One || mode == SegmentMode::OffOne)
        ? rateFor(true) : rateFor(false);
}

bool wetConnected(SegmentMode mode)
{
    return mode == SegmentMode::One || mode == SegmentMode::Two;
}

double noiseGain(SegmentMode mode)
{
    return (mode == SegmentMode::Two || mode == SegmentMode::OffTwo)
        ? modeTwoNoiseGain : 1.0;
}

ChorusMode productionMode(double intervalStart)
{
    switch (segmentAt(intervalStart + 1.0e-14))
    {
        case SegmentMode::One: return ChorusMode::One;
        case SegmentMode::OffOne:
        case SegmentMode::OffTwo: return ChorusMode::Off;
        case SegmentMode::Two: return ChorusMode::Two;
    }
    return ChorusMode::Off;
}

double triangle(long double phase)
{
    phase -= std::floor(phase);
    const long double folded = phase < 0.5L ? phase : 1.0L - phase;
    return static_cast<double>(4.0L * folded - 1.0L);
}

std::complex<double> lowpass(double frequency, double corner)
{
    const std::complex<double> s(0.0, 2.0 * static_cast<double>(pi) * frequency);
    const double w = 2.0 * static_cast<double>(pi) * corner;
    return w / (s + w);
}

std::complex<double> highpass(double frequency, double corner)
{
    const std::complex<double> s(0.0, 2.0 * static_cast<double>(pi) * frequency);
    const double w = 2.0 * static_cast<double>(pi) * corner;
    return s / (s + w);
}

std::complex<double> sallen(double frequency, double corner, double q)
{
    const std::complex<double> s(0.0, 2.0 * static_cast<double>(pi) * frequency);
    const double w = 2.0 * static_cast<double>(pi) * corner;
    return w * w / (s * s + (w / q) * s + w * w);
}

std::complex<double> inputResponse(double frequency)
{
    return sallen(frequency, firstHz, firstQ)
         * sallen(frequency, secondHz, secondQ)
         * highpass(frequency, inputCouplingHz)
         * lowpass(frequency, inputPassiveHz);
}

double analyticInputSupportRmsVolts()
{
    const double fundamental = hotFundamentalAmplitude
                             * std::abs(inputResponse(997.0));
    const double upper = hotUpperAmplitude
                       * std::abs(inputResponse(5213.0));
    return nodeVoltsPerUnit
         * std::sqrt(0.5 * (fundamental * fundamental + upper * upper));
}

struct Drive
{
    bool noiseOnly {};

    double raw(double time) const
    {
        if (noiseOnly)
            return 0.0;
        // The analytic input-support solution puts this two-tone card at
        // exactly 1.500000 Vrms at the BBD input.  The common scale is
        // 1.045775384 over the earlier exploratory 0.78/0.14 card.
        return hotFundamentalAmplitude * std::sin(
                   2.0 * static_cast<double>(pi) * 997.0 * time)
             + hotUpperAmplitude * std::sin(
                   2.0 * static_cast<double>(pi) * 5213.0 * time + 0.37);
    }

    double filtered(double time) const
    {
        if (noiseOnly)
            return 0.0;
        const auto component = [time](double amplitude, double frequency,
                                      double phase) {
            const std::complex<double> sine = std::polar(amplitude, phase)
                                              * std::complex<double>(0.0, -1.0);
            return std::real(sine * inputResponse(frequency)
                * std::exp(std::complex<double>(
                    0.0, 2.0 * static_cast<double>(pi) * frequency * time)));
        };
        return component(hotFundamentalAmplitude, 997.0, 0.0)
             + component(hotUpperAmplitude, 5213.0, 0.37);
    }
};

std::uint32_t xorshift(std::uint32_t state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

double randomFrom(std::uint32_t state)
{
    return static_cast<double>(state & 0xffffffu)
         * (2.0 / 16777215.0) - 1.0;
}

double saturate(double input)
{
    const double normalised = std::abs(input) / saturationLevel;
    const double base = 1.0
        + saturationCurvature * normalised * normalised
        + std::pow(normalised, saturationExponent);
    return input / std::pow(base, 1.0 / saturationExponent);
}

struct Event
{
    double time {};
    double held {};
};

struct EventLine
{
    std::array<double, referenceCellPairs> cells {};
    int index {};
    long double clockPhase {};
    double transfer {};
    std::uint32_t rng {};
    std::uint64_t edgeCount {};
    std::vector<Event> events;
};

double cyclesOver(double delay0, double slope, double elapsed)
{
    if (std::abs(slope * elapsed) < 1.0e-12 * delay0)
        return static_cast<double>(referenceCellPairs) * elapsed / delay0;
    return static_cast<double>(referenceCellPairs) / slope
         * std::log((delay0 + slope * elapsed) / delay0);
}

double timeForCycles(double delay0, double slope, double cycles)
{
    if (std::abs(slope) < 1.0e-14)
        return cycles * delay0 / static_cast<double>(referenceCellPairs);
    return delay0 / slope * std::expm1(
        cycles * slope / static_cast<double>(referenceCellPairs));
}

EventLine buildEvents(bool lineA, const Drive& drive, bool noiseEnabled)
{
    EventLine line;
    line.rng = lineA ? 0x9e3779b9u : 0x85ebca6bu;
    line.events.reserve(60000);
    long double phase = 0.0L;
    double time = 0.0;
    const double sign = lineA ? 1.0 : -1.0;

    while (time < duration - 1.0e-14)
    {
        const auto mode = segmentAt(time + 1.0e-14);
        const double rate = segmentRate(mode);
        const long double wrapped = phase - std::floor(phase);
        const long double nextHalfPhase = wrapped < 0.5L
            ? phase + (0.5L - wrapped) : phase + (1.0L - wrapped);
        const double halfBoundary = time
            + static_cast<double>((nextHalfPhase - phase) / rate);
        const double end = std::min({ duration, nextModeBoundary(time),
                                      halfBoundary });
        const double dt = end - time;
        if (!(dt > 0.0))
        {
            time = std::nextafter(time, duration);
            continue;
        }

        const double modulation0 = triangle(phase);
        const bool rising = wrapped < 0.5L;
        const double modulationSlope = (rising ? 4.0 : -4.0) * rate;
        double local = 0.0;
        while (local < dt - 1.0e-15)
        {
            const double delay0 = centreDelay
                + sign * sweepDelay
                    * (modulation0 + modulationSlope * local);
            const double slope = sign * sweepDelay * modulationSlope;
            const double remaining = dt - local;
            const double available = cyclesOver(delay0, slope, remaining);
            const double needed = 1.0 - static_cast<double>(line.clockPhase);
            if (available + 2.0e-15 < needed)
            {
                line.clockPhase += available;
                local = dt;
                break;
            }
            double toEdge = timeForCycles(delay0, slope, needed);
            toEdge = std::clamp(toEdge, 0.0, remaining);
            local += toEdge;
            line.clockPhase = 0.0L;
            const double edgeTime = time + local;
            const double bounded = saturate(drive.filtered(edgeTime));
            line.index = (line.index + 1) % referenceCellPairs;
            const double emerging = line.cells[static_cast<std::size_t>(line.index)];
            line.cells[static_cast<std::size_t>(line.index)] = bounded;
            line.transfer += transferSmear * (emerging - line.transfer);
            line.rng = xorshift(line.rng);
            const double held = line.transfer + (noiseEnabled
                ? randomFrom(line.rng) * noiseAmplitude * noiseGain(mode)
                : 0.0);
            line.events.push_back({ edgeTime, held });
            ++line.edgeCount;
            if (toEdge <= 1.0e-16)
            {
                local = std::nextafter(local, remaining);
                line.clockPhase = 1.0e-15L;
            }
        }
        phase += static_cast<long double>(rate * dt);
        time = end;
    }
    line.clockPhase -= std::floor(line.clockPhase);
    return line;
}

using State = std::array<double, 6>;

double outputCorner(bool connected)
{
    const double resistance = connected
        ? outputBleed * outputMixer / (outputBleed + outputMixer)
        : outputBleed;
    return 1.0 / (2.0 * static_cast<double>(pi)
                  * outputCapacitance * resistance);
}

State derivative(const State& x, double held, bool connected)
{
    const double sourceConductance = 1.0 / outputParallelDriveOhms;
    const double returnConductance = 1.0 / outputReturnOhms;
    const double totalSourceConductance =
        sourceConductance + returnConductance;
    const double seriesConductance = 1.0 / reconstructionSeriesOhms;
    const double wc = 2.0 * static_cast<double>(pi) * outputCorner(connected);
    return {
        (totalSourceConductance * held
             - (totalSourceConductance + seriesConductance) * x[0]
             + seriesConductance * x[1]) / outputTapFarads,
        seriesConductance * x[0] / firstFeedbackFarads
            + (seriesConductance / firstShuntFarads
               - 2.0 * seriesConductance / firstFeedbackFarads) * x[1]
            + (-seriesConductance / firstShuntFarads
               + seriesConductance / firstFeedbackFarads) * x[2],
        seriesConductance * (x[1] - x[2]) / firstShuntFarads,
        seriesConductance * x[2] / secondFeedbackFarads
            + (seriesConductance / secondShuntFarads
               - 2.0 * seriesConductance / secondFeedbackFarads) * x[3]
            + (-seriesConductance / secondShuntFarads
               + seriesConductance / secondFeedbackFarads) * x[4],
        seriesConductance * (x[3] - x[4]) / secondShuntFarads,
        wc * (x[4] - x[5])
    };
}

void rk4(State& state, double held, bool connected, double step)
{
    const State k1 = derivative(state, held, connected);
    State work {};
    for (std::size_t i = 0; i < state.size(); ++i)
        work[i] = state[i] + 0.5 * step * k1[i];
    const State k2 = derivative(work, held, connected);
    for (std::size_t i = 0; i < state.size(); ++i)
        work[i] = state[i] + 0.5 * step * k2[i];
    const State k3 = derivative(work, held, connected);
    for (std::size_t i = 0; i < state.size(); ++i)
        work[i] = state[i] + step * k3[i];
    const State k4 = derivative(work, held, connected);
    for (std::size_t i = 0; i < state.size(); ++i)
        state[i] += step * (k1[i] + 2.0 * k2[i]
                          + 2.0 * k3[i] + k4[i]) / 6.0;
}

double wetGainAt(double time)
{
    if (time < offStart)
        return wetGainTarget;
    if (time < twoStart)
        return wetGainTarget * std::exp(-(time - offStart) / wetTau);
    const double atTwo = wetGainTarget * std::exp(
        -(twoStart - offStart) / wetTau);
    if (time < finalOff)
        return wetGainTarget
             + (atTwo - wetGainTarget) * std::exp(-(time - twoStart) / wetTau);
    const double atFinal = wetGainTarget
        + (atTwo - wetGainTarget) * std::exp(-(finalOff - twoStart) / wetTau);
    return atFinal * std::exp(-(time - finalOff) / wetTau);
}

struct RefRender
{
    std::vector<double> left;
    std::vector<double> right;
    EventLine eventsA;
    EventLine eventsB;
};

std::vector<double> renderOutputLine(const EventLine& events,
                                     double highRate, int substeps)
{
    const std::size_t frames = static_cast<std::size_t>(
        std::llround(duration * highRate));
    std::vector<double> output(frames);
    State state {};
    double held = 0.0;
    double time = 0.0;
    std::size_t eventIndex = 0;
    const double maximumStep = 1.0 / (highRate * substeps);
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const double target = static_cast<double>(frame + 1u) / highRate;
        while (time < target - 1.0e-15)
        {
            const double eventTime = eventIndex < events.events.size()
                ? events.events[eventIndex].time
                : std::numeric_limits<double>::infinity();
            const double modeBoundary = nextModeBoundary(time);
            const double next = std::min({ target, eventTime, modeBoundary });
            const double interval = next - time;
            if (interval > 0.0)
            {
                const int steps = std::max(1, static_cast<int>(
                    std::ceil(interval / maximumStep)));
                const double step = interval / steps;
                const bool connected = wetConnected(
                    segmentAt(time + 0.5 * interval));
                for (int i = 0; i < steps; ++i)
                    rk4(state, held, connected, step);
                time = next;
            }
            if (eventIndex < events.events.size()
                && std::abs(events.events[eventIndex].time - time) < 2.0e-13)
            {
                held = events.events[eventIndex].held;
                ++eventIndex;
                continue;
            }
            if (!(interval > 0.0))
                time = std::nextafter(time, target);
        }
        output[frame] = (state[4] - state[5]) * wetGainAt(target);
    }
    return output;
}

quality::HostAlignedSignal toHost(const std::vector<double>& high,
                                  const quality::KaiserLowPass& filter)
{
    const std::size_t phase = oracleFactor - 1u;
    std::span<const double> shifted(
        high.data() + static_cast<std::ptrdiff_t>(phase), high.size() - phase);
    return quality::decimateToHostBoundary(shifted, oracleFactor, filter);
}

RefRender renderReference(double hostRate, const Drive& drive,
                          bool noiseEnabled, int substeps)
{
    RefRender result;
    result.eventsA = buildEvents(true, drive, noiseEnabled);
    result.eventsB = buildEvents(false, drive, noiseEnabled);
    const double highRate = hostRate * oracleFactor;
    result.left = renderOutputLine(result.eventsA, highRate, substeps);
    result.right = renderOutputLine(result.eventsB, highRate, substeps);
    return result;
}

constexpr std::uint64_t fnvOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t fnvPrime = UINT64_C(1099511628211);

template <typename Value>
void hashScalar(std::uint64_t& hash, Value value) noexcept
{
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(Value)>>(value);
    for (const auto byte : bytes)
    {
        hash ^= std::to_integer<std::uint8_t>(byte);
        hash *= fnvPrime;
    }
}

float productionTriangle(double phase) noexcept
{
    const double folded = phase < 0.5 ? phase : 1.0 - phase;
    return static_cast<float>(folded * 4.0 - 1.0);
}

float noiseFromStateFloat(std::uint32_t state) noexcept
{
    return static_cast<float>(state & 0xffffffu)
             * (2.0f / 16777215.0f) - 1.0f;
}

struct DiscreteShadow
{
    std::uint64_t edgesA {};
    std::uint64_t edgesB {};
    int indexA {};
    int indexB {};
    double phaseA {};
    double phaseB {};
    std::uint32_t rngA { 0x9e3779b9u };
    std::uint32_t rngB { 0x85ebca6bu };
    double lfoPhase {};
    float rate { static_cast<float>(rateFor(true)) };
    float wetGain {};
    ChorusMode runningMode { ChorusMode::One };
    bool primed {};
};

struct ShadowFrame
{
    float clockA {};
    float clockB {};
    int shiftsA {};
    int shiftsB {};
};

ShadowFrame advanceShadow(DiscreteShadow& shadow, ChorusMode mode,
                          float sampleRate)
{
    const float inverse = 1.0f / sampleRate;
    const float targetRate = static_cast<float>(
        rateFor(mode != ChorusMode::Two));
    const float targetWet = mode == ChorusMode::Off
        ? 0.0f : referenceWetGain;
    if (!shadow.primed)
    {
        shadow.rate = targetRate;
        shadow.wetGain = targetWet;
        shadow.primed = true;
    }
    if (mode != ChorusMode::Off)
    {
        shadow.rate = targetRate;
        shadow.runningMode = mode == ChorusMode::Two
            ? ChorusMode::Two : ChorusMode::One;
    }
    const float muteGlide = 1.0f - std::exp(
        -inverse / static_cast<float>(wetTau));
    shadow.wetGain += (targetWet - shadow.wetGain) * muteGlide;
    shadow.lfoPhase += shadow.rate * inverse;
    if (shadow.lfoPhase >= 1.0f)
        shadow.lfoPhase -= std::floor(shadow.lfoPhase);
    const float modulation = productionTriangle(shadow.lfoPhase);
    const float centre = static_cast<float>(centreDelay);
    const float sweep = static_cast<float>(sweepDelay);
    ShadowFrame result;
    result.clockA = std::clamp(
        static_cast<float>(referenceCellPairs)
            / (centre + sweep * modulation),
        referenceMinimumClockHz, referenceMaximumClockHz);
    result.clockB = std::clamp(
        static_cast<float>(referenceCellPairs)
            / (centre - sweep * modulation),
        referenceMinimumClockHz, referenceMaximumClockHz);
    shadow.phaseA += static_cast<double>(result.clockA)
                   / static_cast<double>(sampleRate);
    shadow.phaseB += static_cast<double>(result.clockB)
                   / static_cast<double>(sampleRate);
    while (shadow.phaseA >= 1.0)
    {
        shadow.phaseA -= 1.0;
        shadow.indexA = (shadow.indexA + 1) % referenceCellPairs;
        shadow.rngA = xorshift(shadow.rngA);
        ++shadow.edgesA;
        ++result.shiftsA;
    }
    while (shadow.phaseB >= 1.0)
    {
        shadow.phaseB -= 1.0;
        shadow.indexB = (shadow.indexB + 1) % referenceCellPairs;
        shadow.rngB = xorshift(shadow.rngB);
        ++shadow.edgesB;
        ++result.shiftsB;
    }
    return result;
}

struct StructuralAudit
{
    bool exact { true };
    bool clockExact { true };
    bool heldNoiseExact { true };
    bool finite { true };
    std::uint64_t expectedHash { fnvOffset };
    std::uint64_t actualHash { fnvOffset };
    std::uint64_t frames {};
    DiscreteShadow expected;
    std::uint64_t actualEdgesA {};
    std::uint64_t actualEdgesB {};

    [[nodiscard]] bool passed() const noexcept
    {
        return exact && clockExact && heldNoiseExact && finite
            && expectedHash == actualHash;
    }
};

class StructuralChecker
{
public:
    explicit StructuralChecker(float sampleRate) : sampleRate_(sampleRate) {}

    void observe(ChorusMode mode, float noiseScale,
                 const YouKnowTestAccess::State& state)
    {
        const double previousPhaseA = previous_.phaseA;
        const double previousPhaseB = previous_.phaseB;
        const auto expectedFrame = advanceShadow(result_.expected, mode,
                                                 sampleRate_);
        const int actualShiftsA = (state.indexA - previous_.indexA
                                  + referenceCellPairs) % referenceCellPairs;
        const int actualShiftsB = (state.indexB - previous_.indexB
                                  + referenceCellPairs) % referenceCellPairs;
        result_.actualEdgesA += static_cast<std::uint64_t>(actualShiftsA);
        result_.actualEdgesB += static_cast<std::uint64_t>(actualShiftsB);

        const double actualClockA = (state.phaseA - previousPhaseA
            + static_cast<double>(actualShiftsA)) * sampleRate_;
        const double actualClockB = (state.phaseB - previousPhaseB
            + static_cast<double>(actualShiftsB)) * sampleRate_;
        result_.clockExact = result_.clockExact
            && std::abs(actualClockA - expectedFrame.clockA) <= 1.0e-7
            && std::abs(actualClockB - expectedFrame.clockB) <= 1.0e-7;
        result_.exact = result_.exact
            && state.indexA == result_.expected.indexA
            && state.indexB == result_.expected.indexB
            && state.phaseA == result_.expected.phaseA
            && state.phaseB == result_.expected.phaseB
            && state.rngA == result_.expected.rngA
            && state.rngB == result_.expected.rngB
            && state.lfoPhase == result_.expected.lfoPhase
            && state.rateHz == result_.expected.rate
            && state.centreDelay == static_cast<float>(centreDelay)
            && state.sweep == static_cast<float>(sweepDelay);

        result_.exact = result_.exact
            && state.runningMode == result_.expected.runningMode
            && state.wetGain == result_.expected.wetGain
            && state.primed == result_.expected.primed;
        const float modeGain = result_.expected.runningMode == ChorusMode::Two
            ? referenceModeTwoNoiseGain : 1.0f;
        if (actualShiftsA > 0)
        {
            const float expectedHeld = state.transferA
                + noiseFromStateFloat(state.rngA)
                    * referenceLineNoiseAmplitude
                    * (noiseScale * modeGain);
            result_.heldNoiseExact = result_.heldNoiseExact
                && std::bit_cast<std::uint32_t>(state.heldA)
                    == std::bit_cast<std::uint32_t>(expectedHeld);
        }
        if (actualShiftsB > 0)
        {
            const float expectedHeld = state.transferB
                + noiseFromStateFloat(state.rngB)
                    * referenceLineNoiseAmplitude
                    * (noiseScale * modeGain);
            result_.heldNoiseExact = result_.heldNoiseExact
                && std::bit_cast<std::uint32_t>(state.heldB)
                    == std::bit_cast<std::uint32_t>(expectedHeld);
        }
        result_.finite = result_.finite
            && std::isfinite(state.phaseA) && std::isfinite(state.phaseB)
            && std::isfinite(state.lfoPhase) && std::isfinite(state.heldA)
            && std::isfinite(state.heldB) && std::isfinite(state.transferA)
            && std::isfinite(state.transferB);

        hashFrame(result_.expectedHash, result_.expected.lfoPhase,
                  expectedFrame.clockA, expectedFrame.clockB,
                  expectedFrame.shiftsA, expectedFrame.shiftsB,
                  result_.expected.indexA, result_.expected.indexB,
                  result_.expected.phaseA, result_.expected.phaseB,
                  result_.expected.rngA, result_.expected.rngB,
                  result_.expected.wetGain, result_.expected.runningMode,
                  result_.expected.primed);
        hashFrame(result_.actualHash, state.lfoPhase,
                  expectedFrame.clockA, expectedFrame.clockB,
                  actualShiftsA, actualShiftsB, state.indexA, state.indexB,
                  state.phaseA, state.phaseB, state.rngA, state.rngB,
                  state.wetGain, state.runningMode, state.primed);
        ++result_.frames;
        previous_ = state;
    }

    [[nodiscard]] const StructuralAudit& result() const noexcept
    {
        return result_;
    }

private:
    static void hashFrame(std::uint64_t& hash, double lfo, float clockA,
                          float clockB, int shiftsA, int shiftsB,
                          int indexA, int indexB, double phaseA,
                          double phaseB, std::uint32_t rngA,
                          std::uint32_t rngB, float wetGain,
                          ChorusMode runningMode, bool primed) noexcept
    {
        for (const auto value : { lfo, phaseA, phaseB })
            hashScalar(hash, value);
        for (const auto value : { clockA, clockB })
            hashScalar(hash, value);
        for (const auto value : { shiftsA, shiftsB, indexA, indexB })
            hashScalar(hash, value);
        for (const auto value : { rngA, rngB })
            hashScalar(hash, value);
        hashScalar(hash, wetGain);
        hashScalar(hash, runningMode);
        hashScalar(hash, primed);
    }

    float sampleRate_ {};
    StructuralAudit result_;
    YouKnowTestAccess::State previous_;
};

StructuralAudit auditFullCycle(float sampleRate, ChorusMode mode)
{
    Chorus chorus;
    chorus.prepare(sampleRate);
    StructuralChecker checker(sampleRate);
    const float rate = static_cast<float>(rateFor(mode == ChorusMode::One));
    const float step = rate * (1.0f / sampleRate);
    const std::size_t frames = static_cast<std::size_t>(
        std::ceil(1.0 / static_cast<double>(step)));
    for (std::size_t frame = 0u; frame < frames; ++frame)
    {
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(0.0f, mode, 1.0f, left, right);
        checker.observe(mode, 1.0f, YouKnowTestAccess::state(chorus));
    }
    return checker.result();
}

struct ProductionRender
{
    std::vector<double> left;
    std::vector<double> right;
    std::int64_t firstHostFrame {};
    YouKnowTestAccess::State state;
    YouKnowTestAccess::ProcessingRate processingRate;
    StructuralAudit scheduleLedger;
    std::uint64_t rawFingerprint { UINT64_C(14695981039346656037) };
    std::size_t rawFrames {};
    bool rawFinite {};
    bool boundaryFinite {};
};

ProductionRender renderProduction(double hostRate, bool hqEnabled,
                                  const Drive& drive,
                                  bool noiseEnabled,
                                  bool alignToReference = true)
{
    const auto processingRate =
        YouKnowTestAccess::shippingProcessingRate(hostRate, hqEnabled);
    const int factor = processingRate.factor;
    const double rate = processingRate.internalRate;
    Chorus chorus;
    chorus.prepare(rate);
    const std::size_t frames = static_cast<std::size_t>(
        std::llround(duration * rate));
    std::vector<float> internalLeft(frames);
    std::vector<float> internalRight(frames);
    ProductionRender result;
    result.processingRate = processingRate;
    result.rawFrames = frames;
    StructuralChecker checker(static_cast<float>(rate));
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const double intervalStart = static_cast<double>(frame) / rate;
        const double endpoint = static_cast<double>(frame + 1u) / rate;
        const float input = static_cast<float>(drive.raw(endpoint));
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(input, productionMode(intervalStart),
                       noiseEnabled ? 1.0f : 0.0f, left, right);
        checker.observe(productionMode(intervalStart),
                        noiseEnabled ? 1.0f : 0.0f,
                        YouKnowTestAccess::state(chorus));
        // Recover the wet leg from the public dry/wet output in double before
        // presenting it to the float shipping boundary.  Doing the cancelling
        // subtraction in float creates an audit-local residual that the public
        // process path itself did not generate.
        internalLeft[frame] = static_cast<float>(
            static_cast<double>(left) / Chorus::dryMixGain
            - static_cast<double>(input));
        internalRight[frame] = static_cast<float>(
            static_cast<double>(right) / Chorus::dryMixGain
            - static_cast<double>(input));
        for (const float sample : { internalLeft[frame], internalRight[frame] })
        {
            const auto bits = std::bit_cast<std::uint32_t>(sample);
            for (unsigned int shift = 0u; shift < 32u; shift += 8u)
            {
                result.rawFingerprint ^=
                    static_cast<std::uint8_t>(bits >> shift);
                result.rawFingerprint *= UINT64_C(1099511628211);
            }
        }
    }
    const auto finiteVector = [](const auto& samples) {
        return std::all_of(samples.begin(), samples.end(), [](auto sample) {
            return std::isfinite(static_cast<double>(sample));
        });
    };
    result.rawFinite = finiteVector(internalLeft)
        && finiteVector(internalRight);
    const auto boundary = YouKnowTestAccess::decimate(
        internalLeft, internalRight, factor);
    result.boundaryFinite = finiteVector(boundary.left)
        && finiteVector(boundary.right);
    if (!alignToReference)
    {
        result.left.assign(boundary.left.begin(), boundary.left.end());
        result.right.assign(boundary.right.begin(), boundary.right.end());
        result.firstHostFrame = 0;
        result.state = YouKnowTestAccess::state(chorus);
        result.scheduleLedger = checker.result();
        return result;
    }
    const double delay =
        YouKnowTestAccess::shippingBoundaryDelayHostFrames(factor);
    const auto alignedLeft = quality::compensateFractionalDelay(
        boundary.left, hostRate, delay);
    const auto alignedRight = quality::compensateFractionalDelay(
        boundary.right, hostRate, delay);
    result.left = alignedLeft.samples;
    result.right = alignedRight.samples;
    result.firstHostFrame = alignedLeft.firstHostFrame;
    result.state = YouKnowTestAccess::state(chorus);
    result.scheduleLedger = checker.result();
    return result;
}

struct Comparison
{
    double leftDb {};
    double rightDb {};
    double stereoDb {};
    double midDb {};
    double sideDb {};
    double referenceRms {};

    [[nodiscard]] double worstDb() const noexcept
    {
        return std::max({ leftDb, rightDb, midDb, sideDb });
    }
};

struct WindowStats
{
    double maximumLevelDeltaDb {};
    double referenceCorrelation {};
    double candidateCorrelation {};
    double referenceStereoRms {};
    double candidateStereoRms {};
    double maximumBandDeltaDb {};
};

void fft(std::vector<std::complex<double>>& values)
{
    const std::size_t size = values.size();
    for (std::size_t i = 1u, j = 0u; i < size; ++i)
    {
        std::size_t bit = size >> 1u;
        for (; j & bit; bit >>= 1u)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(values[i], values[j]);
    }
    for (std::size_t length = 2u; length <= size; length <<= 1u)
    {
        const auto root = std::polar(
            1.0, -2.0 * static_cast<double>(pi) / static_cast<double>(length));
        for (std::size_t first = 0u; first < size; first += length)
        {
            std::complex<double> factor(1.0, 0.0);
            for (std::size_t offset = 0u; offset < length / 2u; ++offset)
            {
                const auto even = values[first + offset];
                const auto odd = values[first + offset + length / 2u] * factor;
                values[first + offset] = even + odd;
                values[first + offset + length / 2u] = even - odd;
                factor *= root;
            }
        }
    }
}

std::array<double, 4> bandPowers(std::span<const double> samples,
                                 double sampleRate)
{
    constexpr std::size_t size = 4096u;
    constexpr double a0 = 0.35875;
    constexpr double a1 = 0.48829;
    constexpr double a2 = 0.14128;
    constexpr double a3 = 0.01168;
    std::array<double, 4> powers {};
    std::size_t windows = 0u;
    for (std::size_t start = 0u; start + size <= samples.size();
         start += size / 2u)
    {
        std::vector<std::complex<double>> values(size);
        for (std::size_t i = 0; i < size; ++i)
        {
            const double angle = 2.0 * static_cast<double>(pi) * i / (size - 1u);
            const double window = a0 - a1 * std::cos(angle)
                + a2 * std::cos(2.0 * angle) - a3 * std::cos(3.0 * angle);
            values[i] = samples[start + i] * window;
        }
        fft(values);
        constexpr std::array<double, 5> edges { 20.0, 200.0, 2000.0,
                                                 10000.0, 20000.0 };
        for (std::size_t bin = 1u; bin < size / 2u; ++bin)
        {
            const double frequency = sampleRate * bin / size;
            for (std::size_t band = 0u; band < powers.size(); ++band)
                if (frequency >= edges[band] && frequency < edges[band + 1u])
                    powers[band] += std::norm(values[bin]);
        }
        ++windows;
    }
    for (double& power : powers)
        power /= static_cast<double>(windows);
    return powers;
}

double residualPeakDb(std::span<const double> reference,
                      std::span<const double> candidate,
                      double sampleRate)
{
    const bool family441 =
        std::abs(std::remainder(sampleRate, 44100.0)) < 1.0;
    const double familyBase = family441 ? 44100.0 : 48000.0;
    const std::size_t size = 8192u * static_cast<std::size_t>(
        std::llround(sampleRate / familyBase));
    if (reference.size() < size || candidate.size() < size)
        throw std::runtime_error("residual FFT interval is too short");

    std::vector<std::complex<double>> wanted(size), difference(size);
    constexpr double a0 = 0.35875;
    constexpr double a1 = 0.48829;
    constexpr double a2 = 0.14128;
    constexpr double a3 = 0.01168;
    for (std::size_t index = 0u; index < size; ++index)
    {
        const double angle = 2.0 * static_cast<double>(pi) * index
                           / static_cast<double>(size - 1u);
        const double window = a0 - a1 * std::cos(angle)
            + a2 * std::cos(2.0 * angle) - a3 * std::cos(3.0 * angle);
        wanted[index] = reference[index] * window;
        difference[index] = (candidate[index] - reference[index]) * window;
    }
    fft(wanted);
    fft(difference);
    double referencePeak = 0.0;
    double residualPeak = 0.0;
    for (std::size_t bin = 1u; bin < size / 2u; ++bin)
    {
        const double frequency = sampleRate * static_cast<double>(bin)
                               / static_cast<double>(size);
        if (frequency < 20.0 || frequency > 20000.0)
            continue;
        referencePeak = std::max(referencePeak, std::abs(wanted[bin]));
        residualPeak = std::max(residualPeak, std::abs(difference[bin]));
    }
    if (!(referencePeak > 0.0) || !std::isfinite(referencePeak)
        || !std::isfinite(residualPeak))
        throw std::runtime_error("invalid residual FFT normalization");
    return 20.0 * std::log10(std::max(residualPeak, 1.0e-300)
                              / referencePeak);
}

double exhaustiveResidualPeakDb(
    const quality::HostAlignedSignal& refLeft,
    const quality::HostAlignedSignal& refRight,
    const ProductionRender& production, double hostRate)
{
    const std::int64_t first = std::max<std::int64_t>(
        { refLeft.firstHostFrame, refRight.firstHostFrame,
          production.firstHostFrame,
          static_cast<std::int64_t>(std::llround(0.400 * hostRate)) });
    const bool family441 =
        std::abs(std::remainder(hostRate, 44100.0)) < 1.0;
    const double familyBase = family441 ? 44100.0 : 48000.0;
    const std::size_t size = 8192u * static_cast<std::size_t>(
        std::llround(hostRate / familyBase));
    const std::int64_t end = first + static_cast<std::int64_t>(size);
    if (end > refLeft.endHostFrameExclusive()
        || end > refRight.endHostFrameExclusive()
        || end > production.firstHostFrame
                    + static_cast<std::int64_t>(production.left.size()))
        throw std::runtime_error("aligned mode-II residual capture is short");

    std::array<std::vector<double>, 4> reference;
    std::array<std::vector<double>, 4> candidate;
    for (auto& values : reference)
        values.reserve(size);
    for (auto& values : candidate)
        values.reserve(size);
    for (std::int64_t frame = first; frame < end; ++frame)
    {
        const auto ri = static_cast<std::size_t>(frame - refLeft.firstHostFrame);
        const auto ci = static_cast<std::size_t>(
            frame - production.firstHostFrame);
        const double rl = refLeft.samples[ri];
        const double rr = refRight.samples[ri];
        const double cl = production.left[ci];
        const double cr = production.right[ci];
        for (const auto& [values, left, right] : {
                 std::tuple { &reference, rl, rr },
                 std::tuple { &candidate, cl, cr } })
        {
            (*values)[0].push_back(left);
            (*values)[1].push_back(right);
            (*values)[2].push_back(0.5 * (left + right));
            (*values)[3].push_back(0.5 * (left - right));
        }
    }
    double worst = -std::numeric_limits<double>::infinity();
    for (std::size_t channel = 0u; channel < reference.size(); ++channel)
        worst = std::max(worst, residualPeakDb(
            reference[channel], candidate[channel], hostRate));
    return worst;
}

WindowStats windowStats(const quality::HostAlignedSignal& refLeft,
                        const quality::HostAlignedSignal& refRight,
                        const ProductionRender& production,
                        double hostRate, double firstSeconds,
                        double endSeconds)
{
    const std::int64_t first = std::max<std::int64_t>(
        { refLeft.firstHostFrame, refRight.firstHostFrame,
          production.firstHostFrame,
          static_cast<std::int64_t>(std::llround(firstSeconds * hostRate)) });
    const std::int64_t end = std::min<std::int64_t>(
        { refLeft.endHostFrameExclusive(), refRight.endHostFrameExclusive(),
          production.firstHostFrame
              + static_cast<std::int64_t>(production.left.size()),
          static_cast<std::int64_t>(std::llround(endSeconds * hostRate)) });
    long double rll = 0.0L, rrr = 0.0L, rlr = 0.0L;
    long double cll = 0.0L, crr = 0.0L, clr = 0.0L;
    std::vector<double> referenceL, referenceR, candidateL, candidateR;
    referenceL.reserve(static_cast<std::size_t>(end - first));
    referenceR.reserve(referenceL.capacity());
    candidateL.reserve(referenceL.capacity());
    candidateR.reserve(referenceL.capacity());
    for (std::int64_t frame = first; frame < end; ++frame)
    {
        const auto ri = static_cast<std::size_t>(frame - refLeft.firstHostFrame);
        const auto ci = static_cast<std::size_t>(
            frame - production.firstHostFrame);
        const long double rl = refLeft.samples[ri];
        const long double rr = refRight.samples[ri];
        const long double cl = production.left[ci];
        const long double cr = production.right[ci];
        rll += rl * rl; rrr += rr * rr; rlr += rl * rr;
        cll += cl * cl; crr += cr * cr; clr += cl * cr;
        referenceL.push_back(static_cast<double>(rl));
        referenceR.push_back(static_cast<double>(rr));
        candidateL.push_back(static_cast<double>(cl));
        candidateR.push_back(static_cast<double>(cr));
    }
    const long double count = static_cast<long double>(end - first);
    const double refL = std::sqrt(static_cast<double>(rll / count));
    const double refR = std::sqrt(static_cast<double>(rrr / count));
    const double candL = std::sqrt(static_cast<double>(cll / count));
    const double candR = std::sqrt(static_cast<double>(crr / count));
    const double refStereo = std::sqrt(
        static_cast<double>((rll + rrr) / (2.0L * count)));
    const double candStereo = std::sqrt(
        static_cast<double>((cll + crr) / (2.0L * count)));
    double worstBand = 0.0;
    if (referenceL.size() >= 4096u)
    {
        for (const auto& pair : {
                std::pair { &referenceL, &candidateL },
                std::pair { &referenceR, &candidateR } })
        {
            const auto referenceBands = bandPowers(*pair.first, hostRate);
            const auto candidateBands = bandPowers(*pair.second, hostRate);
            for (std::size_t band = 0u; band < referenceBands.size(); ++band)
                worstBand = std::max(worstBand, std::abs(10.0 * std::log10(
                    candidateBands[band] / referenceBands[band])));
        }
    }
    return {
        std::max(std::abs(20.0 * std::log10(candL / refL)),
                 std::abs(20.0 * std::log10(candR / refR))),
        static_cast<double>(rlr / std::sqrt(rll * rrr)),
        static_cast<double>(clr / std::sqrt(cll * crr)),
        refStereo, candStereo, worstBand
    };
}

Comparison compare(const quality::HostAlignedSignal& refLeft,
                   const quality::HostAlignedSignal& refRight,
                   const ProductionRender& production,
                   double hostRate, double firstSeconds, double endSeconds)
{
    const std::int64_t first = std::max<std::int64_t>(
        { refLeft.firstHostFrame, refRight.firstHostFrame,
          production.firstHostFrame,
          static_cast<std::int64_t>(std::llround(firstSeconds * hostRate)) });
    const std::int64_t end = std::min<std::int64_t>(
        { refLeft.endHostFrameExclusive(), refRight.endHostFrameExclusive(),
          static_cast<std::int64_t>(std::llround(endSeconds * hostRate)),
          production.firstHostFrame
              + static_cast<std::int64_t>(production.left.size()) });
    std::vector<double> rlValues, clValues, rrValues, crValues;
    std::vector<double> rm, cm, rs, cs, stereoR, stereoC;
    rlValues.reserve(static_cast<std::size_t>(end - first));
    clValues.reserve(rlValues.capacity());
    rrValues.reserve(rlValues.capacity());
    crValues.reserve(rlValues.capacity());
    rm.reserve(rlValues.capacity());
    cm.reserve(rlValues.capacity());
    rs.reserve(static_cast<std::size_t>(end - first));
    cs.reserve(rs.capacity());
    stereoR.reserve(2u * rs.capacity());
    stereoC.reserve(2u * rs.capacity());
    for (std::int64_t frame = first; frame < end; ++frame)
    {
        const auto ri = static_cast<std::size_t>(frame - refLeft.firstHostFrame);
        const auto pi = static_cast<std::size_t>(
            frame - production.firstHostFrame);
        const double rl = refLeft.samples[ri];
        const double rr = refRight.samples[ri];
        const double cl = production.left[pi];
        const double cr = production.right[pi];
        rlValues.push_back(rl);
        clValues.push_back(cl);
        rrValues.push_back(rr);
        crValues.push_back(cr);
        rm.push_back(0.5 * (rl + rr));
        cm.push_back(0.5 * (cl + cr));
        rs.push_back(0.5 * (rl - rr));
        cs.push_back(0.5 * (cl - cr));
        stereoR.push_back(rl);
        stereoR.push_back(rr);
        stereoC.push_back(cl);
        stereoC.push_back(cr);
    }
    const auto left = quality::compareRms(
        std::span<const double>(rlValues), std::span<const double>(clValues));
    const auto right = quality::compareRms(
        std::span<const double>(rrValues), std::span<const double>(crValues));
    const auto stereo = quality::compareRms(
        std::span<const double>(stereoR), std::span<const double>(stereoC));
    const auto mid = quality::compareRms(
        std::span<const double>(rm), std::span<const double>(cm));
    const auto side = quality::compareRms(
        std::span<const double>(rs), std::span<const double>(cs));
    return { left.relativeErrorDb, right.relativeErrorDb,
             stereo.relativeErrorDb, mid.relativeErrorDb,
             side.relativeErrorDb, stereo.referenceRms };
}

Comparison convergence(const quality::HostAlignedSignal& aLeft,
                       const quality::HostAlignedSignal& aRight,
                       const quality::HostAlignedSignal& bLeft,
                       const quality::HostAlignedSignal& bRight,
                       double hostRate)
{
    ProductionRender fake;
    fake.left.resize(static_cast<std::size_t>(
        std::min(aLeft.endHostFrameExclusive(), bLeft.endHostFrameExclusive())));
    fake.right.resize(fake.left.size());
    for (std::size_t frame = 0; frame < fake.left.size(); ++frame)
    {
        if (static_cast<std::int64_t>(frame) < bLeft.firstHostFrame)
            continue;
        const auto index = static_cast<std::size_t>(
            static_cast<std::int64_t>(frame) - bLeft.firstHostFrame);
        fake.left[frame] = bLeft.samples[index];
        fake.right[frame] = bRight.samples[index];
    }
    return compare(aLeft, aRight, fake, hostRate, 0.12, 0.64);
}

struct SelectorRow
{
    std::string_view label;
    double hostRate {};
    bool hqEnabled {};
    int expectedFactor {};
    bool expectedAdmission {};
};

constexpr std::array<SelectorRow, 10> selectorRows {{
    { "HQ 44.1", 44100.0, true, 4, true },
    { "HQ 48", 48000.0, true, 4, true },
    { "HQ 88.2", 88200.0, true, 2, true },
    { "HQ 96", 96000.0, true, 2, true },
    { "HQ 176.4", 176400.0, true, 1, true },
    { "HQ 192", 192000.0, true, 1, true },
    { "off 44.1", 44100.0, false, 1, false },
    { "off 48", 48000.0, false, 1, false },
    { "off 88.2", 88200.0, false, 1, false },
    { "off 96", 96000.0, false, 1, false }
}};

constexpr std::array<double, 6> uniqueHostRates {
    44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0
};

struct ReferenceProducts
{
    quality::HostAlignedSignal rk64Left;
    quality::HostAlignedSignal rk64Right;
    quality::HostAlignedSignal rk128Left;
    quality::HostAlignedSignal rk128Right;
    Comparison convergenceMetrics;
    std::uint64_t continuousEdgesA {};
    std::uint64_t continuousEdgesB {};
    bool filterInfrastructurePassed {};
    bool filterResponsePassed {};
    bool filterMetadataPassed {};
    bool filterAlignmentPassed {};
    quality::ReferenceFilterCheck filterCheck;
    bool finite {};
};

template <typename Sample>
bool allFinite(std::span<const Sample> samples)
{
    return std::all_of(samples.begin(), samples.end(), [](const auto sample) {
        return std::isfinite(static_cast<double>(sample));
    });
}

template <typename Sample>
bool allFinite(const std::vector<Sample>& samples)
{
    return allFinite(std::span<const Sample>(samples));
}

ReferenceProducts makeReferenceProducts(double hostRate, bool noiseOnly)
{
    const Drive drive { noiseOnly };
    const auto rk64 = renderReference(hostRate, drive, noiseOnly, 4);
    const auto rk128 = renderReference(hostRate, drive, noiseOnly, 8);
    const auto filter = quality::designKaiserLowPass({
        hostRate * oracleFactor, 20000.0, 0.5 * hostRate,
        quality::referenceFilterAttenuationDb, filterTaps });
    ReferenceProducts result;
    result.rk64Left = toHost(rk64.left, filter);
    result.rk64Right = toHost(rk64.right, filter);
    result.rk128Left = toHost(rk128.left, filter);
    result.rk128Right = toHost(rk128.right, filter);
    result.convergenceMetrics = convergence(
        result.rk128Left, result.rk128Right,
        result.rk64Left, result.rk64Right, hostRate);
    result.continuousEdgesA = rk128.eventsA.edgeCount;
    result.continuousEdgesB = rk128.eventsB.edgeCount;
    const auto alignedExactly = [hostRate](
        const quality::HostAlignedSignal& signal) {
        constexpr std::size_t delay = (filterTaps - 1u) / 2u;
        return signal.sourceSampleRateHz == hostRate * oracleFactor
            && signal.hostSampleRateHz == hostRate
            && signal.decimationFactor == oracleFactor
            && signal.declaredGroupDelayInputFrames == delay
            && signal.declaredGroupDelayHostFrames == delay / oracleFactor
            && signal.firstHostFrame
                == static_cast<std::int64_t>(delay / oracleFactor)
            && !signal.samples.empty();
    };
    // The hot and noise references share this exact boundary.  Run the
    // expensive response-grid proof once per host rate, on the hot build.
    if (!noiseOnly)
    {
        quality::ReferenceFilterRequirements requirements;
        const bool family441 =
            std::abs(std::remainder(hostRate, 44100.0)) < 1.0;
        const double familyBase = family441 ? 44100.0 : 48000.0;
        // Keep the response-search spacing fixed in physical Hz.  The doubled
        // Step-13 density is needed here because the q16 oracle's 140 dB
        // boundary exposes a narrow -158 dB stop-band extremum at 96 kHz.
        requirements.coarseGridIntervals = 16384u
            * static_cast<std::size_t>(
                std::llround(hostRate / familyBase));
        requirements.minimumStopbandAttenuationDb = 140.0;
        result.filterCheck = quality::checkReferenceFilter(
            filter, requirements);
    }
    result.filterResponsePassed = noiseOnly || result.filterCheck.passed();
    result.filterMetadataPassed =
        filter.coefficients.size() == filterTaps
        && filter.groupDelayInputFrames == (filterTaps - 1u) / 2u
        && filter.specification.sampleRateHz == hostRate * oracleFactor
        && filter.specification.passbandEdgeHz == 20000.0
        && filter.specification.stopbandEdgeHz == 0.5 * hostRate
        && filter.specification.tapCount == filterTaps
        && filter.specification.stopbandAttenuationDb
            == quality::referenceFilterAttenuationDb;
    result.filterAlignmentPassed = alignedExactly(result.rk64Left)
        && alignedExactly(result.rk64Right)
        && alignedExactly(result.rk128Left)
        && alignedExactly(result.rk128Right);
    result.filterInfrastructurePassed = result.filterResponsePassed
        && result.filterMetadataPassed && result.filterAlignmentPassed;
    result.finite = allFinite(result.rk64Left.samples)
        && allFinite(result.rk64Right.samples)
        && allFinite(result.rk128Left.samples)
        && allFinite(result.rk128Right.samples)
        && result.continuousEdgesA == 30030u
        && result.continuousEdgesB == 27098u
        && result.filterInfrastructurePassed
        && result.convergenceMetrics.worstDb()
            <= referenceConvergenceGateDb;
    return result;
}

struct CellResult
{
    ProductionRender hotProduction;
    ProductionRender noiseProduction;
    Comparison hotWhole;
    Comparison hotOne;
    Comparison hotMute;
    Comparison hotTwo;
    Comparison noiseWaveform;
    WindowStats noiseOne;
    WindowStats noiseTwo;
    double residualPeakDb {};
    double noiseMaximumLevelDb {};
    double noiseMaximumBandDb {};
    double noiseCorrelationError {};
    double noiseAbsoluteCorrelation {};
    double referenceModeDeltaDb {};
    double candidateModeDeltaDb {};
    double noiseModeDeltaErrorDb {};
    bool selectorExact {};
    bool finite {};
    bool hotPassed {};
    bool noisePassed {};
    bool qualityPassed {};
    bool structuralPassed {};
    bool admitted {};
};

bool finiteComparison(const Comparison& value)
{
    return std::isfinite(value.leftDb) && std::isfinite(value.rightDb)
        && std::isfinite(value.stereoDb) && std::isfinite(value.midDb)
        && std::isfinite(value.sideDb) && std::isfinite(value.referenceRms);
}

CellResult evaluateCell(const SelectorRow& row,
                        const ReferenceProducts& hotReference,
                        const ReferenceProducts& noiseReference)
{
    CellResult result;
    result.hotProduction = renderProduction(
        row.hostRate, row.hqEnabled, Drive { false }, false);
    result.noiseProduction = renderProduction(
        row.hostRate, row.hqEnabled, Drive { true }, true);
    const auto& hotLeft = hotReference.rk128Left;
    const auto& hotRight = hotReference.rk128Right;
    result.hotWhole = compare(hotLeft, hotRight, result.hotProduction,
                              row.hostRate, 0.12, 0.64);
    result.hotOne = compare(hotLeft, hotRight, result.hotProduction,
                            row.hostRate, 0.15, offStart);
    result.hotMute = compare(hotLeft, hotRight, result.hotProduction,
                             row.hostRate, offStart, twoStart);
    result.hotTwo = compare(hotLeft, hotRight, result.hotProduction,
                            row.hostRate, 0.40, finalOff);
    result.residualPeakDb = exhaustiveResidualPeakDb(
        hotLeft, hotRight, result.hotProduction, row.hostRate);

    const auto& noiseLeft = noiseReference.rk128Left;
    const auto& noiseRight = noiseReference.rk128Right;
    result.noiseWaveform = compare(noiseLeft, noiseRight,
                                   result.noiseProduction,
                                   row.hostRate, 0.12, 0.64);
    result.noiseOne = windowStats(noiseLeft, noiseRight,
                                  result.noiseProduction,
                                  row.hostRate, 0.15, offStart);
    result.noiseTwo = windowStats(noiseLeft, noiseRight,
                                  result.noiseProduction,
                                  row.hostRate, 0.40, finalOff);
    result.noiseMaximumLevelDb = std::max(
        result.noiseOne.maximumLevelDeltaDb,
        result.noiseTwo.maximumLevelDeltaDb);
    result.noiseMaximumBandDb = std::max(
        result.noiseOne.maximumBandDeltaDb,
        result.noiseTwo.maximumBandDeltaDb);
    result.noiseCorrelationError = std::max(
        std::abs(result.noiseOne.candidateCorrelation
                 - result.noiseOne.referenceCorrelation),
        std::abs(result.noiseTwo.candidateCorrelation
                 - result.noiseTwo.referenceCorrelation));
    result.noiseAbsoluteCorrelation = std::max(
        std::abs(result.noiseOne.candidateCorrelation),
        std::abs(result.noiseTwo.candidateCorrelation));
    result.referenceModeDeltaDb = 20.0 * std::log10(
        result.noiseTwo.referenceStereoRms
        / result.noiseOne.referenceStereoRms);
    result.candidateModeDeltaDb = 20.0 * std::log10(
        result.noiseTwo.candidateStereoRms
        / result.noiseOne.candidateStereoRms);
    result.noiseModeDeltaErrorDb = std::abs(
        result.candidateModeDeltaDb - result.referenceModeDeltaDb);

    result.selectorExact =
        result.hotProduction.processingRate.factor == row.expectedFactor
        && result.noiseProduction.processingRate.factor == row.expectedFactor
        && result.hotProduction.processingRate.hqRequested == row.hqEnabled
        && result.noiseProduction.processingRate.hqRequested == row.hqEnabled
        && result.hotProduction.processingRate.internalRate
            == row.hostRate * row.expectedFactor
        && result.noiseProduction.processingRate.internalRate
            == row.hostRate * row.expectedFactor;
    result.finite = hotReference.finite && noiseReference.finite
        && result.hotProduction.rawFinite
        && result.hotProduction.boundaryFinite
        && result.noiseProduction.rawFinite
        && result.noiseProduction.boundaryFinite
        && allFinite(result.hotProduction.left)
        && allFinite(result.hotProduction.right)
        && allFinite(result.noiseProduction.left)
        && allFinite(result.noiseProduction.right)
        && finiteComparison(result.hotWhole)
        && finiteComparison(result.hotOne)
        && finiteComparison(result.hotMute)
        && finiteComparison(result.hotTwo)
        && std::isfinite(result.residualPeakDb)
        && std::isfinite(result.noiseMaximumLevelDb)
        && std::isfinite(result.noiseMaximumBandDb)
        && std::isfinite(result.noiseCorrelationError)
        && std::isfinite(result.noiseAbsoluteCorrelation)
        && std::isfinite(result.noiseModeDeltaErrorDb);
    result.hotPassed = result.hotWhole.worstDb() <= hotRelativeRmsGateDb
        && result.hotOne.worstDb() <= hotModeRelativeRmsGateDb
        && result.hotTwo.worstDb() <= hotModeRelativeRmsGateDb
        && result.hotMute.worstDb() <= hotMuteRelativeRmsGateDb
        && result.residualPeakDb < hotResidualSpectrumGateDb
        && hotReference.convergenceMetrics.worstDb()
            <= referenceConvergenceGateDb;
    result.noisePassed =
        result.noiseMaximumLevelDb <= noiseLevelGateDb
        && result.noiseMaximumBandDb <= noiseBandPowerGateDb
        && result.noiseCorrelationError <= noiseCorrelationErrorGate
        && result.noiseAbsoluteCorrelation <= noiseAbsoluteCorrelationGate
        && result.noiseModeDeltaErrorDb <= noiseModeDeltaGateDb;
    result.structuralPassed =
        result.hotProduction.scheduleLedger.passed()
        && result.noiseProduction.scheduleLedger.passed();
    result.qualityPassed = result.hotPassed && result.noisePassed;
    result.admitted = result.selectorExact && result.finite
        && result.structuralPassed && result.qualityPassed;
    return result;
}

bool within(double actual, double expected, double tolerance) noexcept
{
    return std::isfinite(actual) && std::abs(actual - expected) <= tolerance;
}

bool metricGoldensExact(const std::array<CellResult, 10>& cells)
{
    // Re-pinned after replacing the separable MN3009 tap/first reconstruction
    // approximation with Roland's coupled C45/R98/R97/C37/C35 network. The
    // admission classes and gates are unchanged; these deterministic waveform
    // coordinates now include the documented extra loaded attenuation.
    constexpr std::array<double, 10> whole {
        -63.883, -63.329, -62.259, -62.935, -62.271,
        -62.943, -24.198, -25.714, -36.427, -37.901
    };
    constexpr std::array<double, 10> modeOne {
        -71.715, -66.765, -76.158, -76.782, -76.179,
        -76.860, -24.202, -25.720, -36.463, -37.948
    };
    constexpr std::array<double, 10> mute {
        -70.061, -73.554, -75.743, -76.720, -76.246,
        -76.934, -24.143, -25.652, -36.410, -37.899
    };
    constexpr std::array<double, 10> modeTwo {
        -61.618, -61.351, -59.421, -60.097, -59.432,
        -60.104, -24.196, -25.712, -36.394, -37.859
    };
    constexpr std::array<double, 10> residual {
        -75.714, -76.439, -75.587, -76.269, -75.473,
        -76.142, -24.839, -26.344, -36.992, -38.458
    };
    constexpr std::array<double, 10> noiseLevel {
        0.071, 0.071, 0.071, 0.071, 0.071,
        0.071, 0.671, 0.508, 0.183, 0.171
    };
    constexpr std::array<double, 10> noiseBands {
        0.561, 0.237, 0.411, 0.210, 0.248,
        0.135, 1.428, 1.101, 0.328, 0.319
    };
    constexpr std::array<double, 10> noiseCorrelationOne {
        0.015, 0.019, 0.015, 0.019, 0.015,
        0.019, 0.012, 0.025, 0.019, 0.010
    };
    constexpr std::array<double, 10> noiseCorrelationTwo {
        0.004, 0.001, 0.004, 0.001, 0.004,
        0.001, -0.014, -0.009, -0.004, -0.003
    };
    constexpr std::array<double, 10> noiseModeDelta {
        4.085, 4.079, 4.085, 4.079, 4.085,
        4.079, 4.089, 4.134, 4.090, 4.075
    };
    bool exact = true;
    for (std::size_t index = 0u; index < cells.size(); ++index)
    {
        exact = exact
            && within(cells[index].hotWhole.stereoDb, whole[index], 0.20)
            && within(cells[index].hotOne.stereoDb, modeOne[index], 0.20)
            && within(cells[index].hotMute.stereoDb, mute[index], 0.20)
            && within(cells[index].hotTwo.stereoDb, modeTwo[index], 0.20)
            && within(cells[index].residualPeakDb, residual[index], 0.20)
            && within(cells[index].noiseMaximumLevelDb,
                      noiseLevel[index], 0.025)
            && within(cells[index].noiseMaximumBandDb,
                      noiseBands[index], 0.075)
            && within(cells[index].noiseOne.candidateCorrelation,
                      noiseCorrelationOne[index], 0.006)
            && within(cells[index].noiseTwo.candidateCorrelation,
                      noiseCorrelationTwo[index], 0.006)
            && within(cells[index].candidateModeDeltaDb,
                      noiseModeDelta[index], 0.015);
    }
    return exact;
}

bool rawFamiliesExact(const std::array<CellResult, 10>& cells)
{
    const auto sameHot = [&cells](std::size_t a, std::size_t b) {
        return cells[a].hotProduction.rawFingerprint
                == cells[b].hotProduction.rawFingerprint
            && cells[a].hotProduction.rawFrames
                == cells[b].hotProduction.rawFrames;
    };
    const auto sameNoise = [&cells](std::size_t a, std::size_t b) {
        return cells[a].noiseProduction.rawFingerprint
                == cells[b].noiseProduction.rawFingerprint
            && cells[a].noiseProduction.rawFrames
                == cells[b].noiseProduction.rawFrames;
    };
    return sameHot(0u, 2u) && sameHot(0u, 4u)
        && sameHot(1u, 3u) && sameHot(1u, 5u)
        && sameNoise(0u, 2u) && sameNoise(0u, 4u)
        && sameNoise(1u, 3u) && sameNoise(1u, 5u);
}

struct ShippingCell
{
    ProductionRender hot;
    ProductionRender noise;
    bool selectorExact {};
    bool finite {};
    bool scheduleLedgersExact {};
};

bool shippingRawFamiliesExact(const std::array<ShippingCell, 10>& cells)
{
    const auto same = [&cells](std::size_t first, std::size_t second,
                               bool noise) {
        const auto& a = noise ? cells[first].noise : cells[first].hot;
        const auto& b = noise ? cells[second].noise : cells[second].hot;
        return a.rawFingerprint == b.rawFingerprint
            && a.rawFrames == b.rawFrames;
    };
    return same(0u, 2u, false) && same(0u, 4u, false)
        && same(1u, 3u, false) && same(1u, 5u, false)
        && same(0u, 2u, true) && same(0u, 4u, true)
        && same(1u, 3u, true) && same(1u, 5u, true);
}

bool runShippingPortabilitySelfTest()
{
    const auto start = std::chrono::steady_clock::now();
    const double inputSupportRmsVolts = analyticInputSupportRmsVolts();
    const bool inputSupportCardExact = std::isfinite(inputSupportRmsVolts)
        && std::abs(inputSupportRmsVolts - 1.5) <= 2.0e-7;
    std::cout << std::fixed << std::setprecision(9)
              << "analytic input-support card=" << inputSupportRmsVolts
              << " Vrms target=1.500000000 => "
              << (inputSupportCardExact ? "PASS" : "FAIL") << '\n';

    std::array<ShippingCell, selectorRows.size()> cells;
    bool rowsExact = true;
    for (std::size_t index = 0u; index < selectorRows.size(); ++index)
    {
        const auto& row = selectorRows[index];
        auto& cell = cells[index];
        cell.hot = renderProduction(
            row.hostRate, row.hqEnabled, Drive { false }, false, false);
        cell.noise = renderProduction(
            row.hostRate, row.hqEnabled, Drive { true }, true, false);
        const auto rateExact = [&row](const ProductionRender& render) {
            return render.processingRate.hqRequested == row.hqEnabled
                && render.processingRate.factor == row.expectedFactor
                && render.processingRate.internalRate
                    == row.hostRate * row.expectedFactor;
        };
        cell.selectorExact = rateExact(cell.hot) && rateExact(cell.noise);
        cell.finite = cell.hot.rawFinite && cell.hot.boundaryFinite
            && cell.noise.rawFinite && cell.noise.boundaryFinite
            && !cell.hot.left.empty() && !cell.noise.left.empty()
            && cell.hot.left.size() == cell.hot.right.size()
            && cell.noise.left.size() == cell.noise.right.size()
            && allFinite(cell.hot.left) && allFinite(cell.hot.right)
            && allFinite(cell.noise.left) && allFinite(cell.noise.right);
        cell.scheduleLedgersExact = cell.hot.scheduleLedger.passed()
            && cell.noise.scheduleLedger.passed();
        rowsExact = rowsExact && cell.selectorExact && cell.finite
            && cell.scheduleLedgersExact;
        std::cout << std::fixed << std::setprecision(1)
                  << std::setw(10) << row.label
                  << " host=" << row.hostRate
                  << " selector=" << cell.hot.processingRate.factor << "x"
                  << " requested="
                  << (cell.hot.processingRate.hqRequested ? "HQ" : "off")
                  << " rate=" << cell.hot.processingRate.internalRate
                  << " selector_exact="
                  << (cell.selectorExact ? "PASS" : "FAIL")
                  << " finite=" << (cell.finite ? "PASS" : "FAIL")
                  << " schedule_ledgers="
                  << (cell.scheduleLedgersExact ? "PASS" : "FAIL")
                  << '\n';
    }

    const bool rawFamilyIdentity = shippingRawFamiliesExact(cells);
    std::array<float, 6> auditedGrids {};
    std::size_t auditedGridCount = 0u;
    bool fullCyclesExact = true;
    for (const auto& cell : cells)
    {
        const float grid = static_cast<float>(
            cell.hot.processingRate.internalRate);
        const auto end = auditedGrids.begin()
            + static_cast<std::ptrdiff_t>(auditedGridCount);
        if (std::find(auditedGrids.begin(), end, grid) != end)
            continue;
        if (auditedGridCount >= auditedGrids.size())
        {
            fullCyclesExact = false;
            continue;
        }
        auditedGrids[auditedGridCount++] = grid;
        const auto one = auditFullCycle(grid, ChorusMode::One);
        const auto two = auditFullCycle(grid, ChorusMode::Two);
        fullCyclesExact = fullCyclesExact && one.passed() && two.passed();
        std::cout << std::fixed << std::setprecision(1)
                  << "ledger full cycles grid=" << grid
                  << " I=" << (one.passed() ? "PASS" : "FAIL")
                  << " II=" << (two.passed() ? "PASS" : "FAIL")
                  << " frames=" << one.frames << "/" << two.frames
                  << '\n';
    }
    const bool sixUniqueGrids = auditedGridCount == auditedGrids.size();
    const bool passed = inputSupportCardExact && rowsExact
        && rawFamilyIdentity && fullCyclesExact && sixUniqueGrids;
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "shipping contract input_support="
              << (inputSupportCardExact ? "PASS" : "FAIL")
              << " selector_rows=" << (rowsExact ? "PASS" : "FAIL")
              << " raw_family_identity="
              << (rawFamilyIdentity ? "PASS" : "FAIL")
              << " full_cycle_ledgers="
              << (fullCyclesExact ? "PASS" : "FAIL")
              << " six_unique_grids="
              << (sixUniqueGrids ? "PASS" : "FAIL")
              << " elapsed=" << std::fixed << std::setprecision(2)
              << elapsed << "s\n"
              << "shipping portability self-test: "
              << (passed ? "PASS" : "FAIL") << '\n';
    return passed;
}

bool mutationSensitivityPassed(const CellResult& canonical,
                               const ReferenceProducts& hotReference,
                               const ReferenceProducts& noiseReference)
{
    auto disconnected = canonical.hotProduction;
    std::fill(disconnected.left.begin(), disconnected.left.end(), 0.0);
    std::fill(disconnected.right.begin(), disconnected.right.end(), 0.0);
    const auto disconnectedMetric = compare(
        hotReference.rk128Left, hotReference.rk128Right, disconnected,
        selectorRows[0].hostRate, 0.12, 0.64);

    auto sameLine = canonical.hotProduction;
    sameLine.right = sameLine.left;
    const auto sameLineMetric = compare(
        hotReference.rk128Left, hotReference.rk128Right, sameLine,
        selectorRows[0].hostRate, 0.12, 0.64);

    auto inverted = canonical.hotProduction;
    for (double& sample : inverted.right)
        sample = -sample;
    const auto invertedMetric = compare(
        hotReference.rk128Left, hotReference.rk128Right, inverted,
        selectorRows[0].hostRate, 0.12, 0.64);

    auto correlatedNoise = canonical.noiseProduction;
    correlatedNoise.right = correlatedNoise.left;
    const auto correlatedStats = windowStats(
        noiseReference.rk128Left, noiseReference.rk128Right,
        correlatedNoise, selectorRows[0].hostRate, 0.15, offStart);

    // Frozen controls are executable specifications of the source-local
    // mutants used while choosing the orthogonal gates.  In particular the
    // always-connected topology passes the whole-window and spectral tests;
    // only the dedicated Off interval rejects it.
    constexpr double edgeSnapWhole = -38.551;
    constexpr double linearTransferWhole = -23.488;
    constexpr double sameClockSide = 0.0;
    constexpr double alwaysConnectedWhole = -59.927;
    constexpr double alwaysConnectedResidual = -75.663;
    constexpr double alwaysConnectedMute = -45.564;
    constexpr double doubleRngLevel = 0.150;
    constexpr double doubleRngBand = 2.373;
    constexpr double doubleRngModeDelta = 3.922;
    constexpr double referenceModeDelta = 4.091;
    const bool frozenControlsReject =
        edgeSnapWhole > hotRelativeRmsGateDb
        && linearTransferWhole > hotRelativeRmsGateDb
        && sameClockSide > hotRelativeRmsGateDb
        && alwaysConnectedWhole <= hotRelativeRmsGateDb
        && alwaysConnectedResidual < hotResidualSpectrumGateDb
        && alwaysConnectedMute > hotMuteRelativeRmsGateDb
        && doubleRngLevel > noiseLevelGateDb
        && doubleRngBand > noiseBandPowerGateDb
        && std::abs(doubleRngModeDelta - referenceModeDelta)
            > noiseModeDeltaGateDb;
    return disconnectedMetric.worstDb() > hotRelativeRmsGateDb
        && sameLineMetric.worstDb() > hotRelativeRmsGateDb
        && invertedMetric.worstDb() > hotRelativeRmsGateDb
        && std::abs(correlatedStats.candidateCorrelation)
            > noiseAbsoluteCorrelationGate
        && frozenControlsReject;
}

void printCell(const SelectorRow& row, const CellResult& cell,
               double convergenceDb)
{
    std::cout << std::fixed << std::setprecision(3)
              << std::setw(10) << row.label
              << " selector=" << cell.hotProduction.processingRate.factor
              << "x hot[L/R/M/S]="
              << cell.hotWhole.leftDb << "/" << cell.hotWhole.rightDb << "/"
              << cell.hotWhole.midDb << "/" << cell.hotWhole.sideDb
              << " I/Off/II=" << cell.hotOne.worstDb() << "/"
              << cell.hotMute.worstDb() << "/" << cell.hotTwo.worstDb()
              << " residual=" << cell.residualPeakDb
              << " conv=" << convergenceDb
              << " noise[level/band/corr/mode]="
              << cell.noiseMaximumLevelDb << "/"
              << cell.noiseMaximumBandDb << "/"
              << cell.noiseCorrelationError << "/"
              << cell.noiseModeDeltaErrorDb
              << " waveform(info)=" << cell.noiseWaveform.stereoDb
              << " ledger="
              << (cell.structuralPassed ? "PASS" : "FAIL")
              << " quality="
              << (cell.qualityPassed ? "PASS" : "REJECT")
              << " infrastructure="
              << (cell.selectorExact && cell.finite && cell.structuralPassed
                      ? "PASS" : "FAIL") << '\n';
}

bool runAudit(int selectedRow, bool selfTest)
{
    const auto start = std::chrono::steady_clock::now();
    const double inputSupportRmsVolts = analyticInputSupportRmsVolts();
    const bool inputSupportCardExact = std::isfinite(inputSupportRmsVolts)
        && std::abs(inputSupportRmsVolts - 1.5) <= 2.0e-7;
    std::cout << std::fixed << std::setprecision(9)
              << "analytic input-support card=" << inputSupportRmsVolts
              << " Vrms target=1.500000000 => "
              << (inputSupportCardExact ? "PASS" : "FAIL") << '\n';
    std::array<ReferenceProducts, uniqueHostRates.size()> hotReferences;
    std::array<ReferenceProducts, uniqueHostRates.size()> noiseReferences;
    std::array<bool, uniqueHostRates.size()> referenceReady {};
    std::array<CellResult, selectorRows.size()> cells;
    std::array<bool, selectorRows.size()> evaluated {};

    const auto hostIndexFor = [](double hostRate) {
        const auto found = std::find(uniqueHostRates.begin(),
                                     uniqueHostRates.end(), hostRate);
        return static_cast<std::size_t>(found - uniqueHostRates.begin());
    };
    for (std::size_t index = 0u; index < selectorRows.size(); ++index)
    {
        if (selectedRow >= 0
            && index != static_cast<std::size_t>(selectedRow))
            continue;
        const auto& row = selectorRows[index];
        const std::size_t hostIndex = hostIndexFor(row.hostRate);
        if (!referenceReady[hostIndex])
        {
            hotReferences[hostIndex] = makeReferenceProducts(
                row.hostRate, false);
            noiseReferences[hostIndex] = makeReferenceProducts(
                row.hostRate, true);
            referenceReady[hostIndex] = true;
            std::cout << "reference host=" << row.hostRate
                      << " filter[response/metadata/alignment]="
                      << (hotReferences[hostIndex].filterResponsePassed
                              ? "PASS" : "FAIL") << "/"
                      << (hotReferences[hostIndex].filterMetadataPassed
                              ? "PASS" : "FAIL") << "/"
                      << (hotReferences[hostIndex].filterAlignmentPassed
                              ? "PASS" : "FAIL") << '\n';
            if (!hotReferences[hostIndex].filterResponsePassed)
            {
                const auto& check = hotReferences[hostIndex].filterCheck;
                std::cout << "  filter response="
                          << (check.responsePassed ? "PASS" : "FAIL")
                          << " convergence="
                          << (check.convergencePassed ? "PASS" : "FAIL")
                          << " ripple="
                          << check.convergence.fine.passbandRippleDb
                          << "dB stop="
                          << check.convergence.fine.stopbandMaximumDb
                          << "dB dc=" << check.convergence.fine.dcGain
                          << " symmetry="
                          << check.convergence.fine.maximumSymmetryError
                          << " grid_delta="
                          << check.convergence.maximumExtremumDeltaDb
                          << "dB\n";
            }
        }
        cells[index] = evaluateCell(row, hotReferences[hostIndex],
                                    noiseReferences[hostIndex]);
        evaluated[index] = true;
        printCell(row, cells[index],
                  hotReferences[hostIndex].convergenceMetrics.worstDb());
    }

    bool classificationsExact = true;
    bool infrastructureExact = true;
    for (std::size_t index = 0u; index < cells.size(); ++index)
        if (evaluated[index])
        {
            classificationsExact = classificationsExact
                && cells[index].qualityPassed
                    == selectorRows[index].expectedAdmission;
            infrastructureExact = infrastructureExact
                && cells[index].selectorExact && cells[index].finite
                && cells[index].structuralPassed;
        }

    bool fullCyclesExact = true;
    std::array<double, 6> auditedGrids {};
    std::size_t auditedGridCount = 0u;
    for (std::size_t index = 0u; index < selectorRows.size(); ++index)
    {
        if (!evaluated[index])
            continue;
        const float grid = static_cast<float>(
            cells[index].hotProduction.processingRate.internalRate);
        const auto end = auditedGrids.begin()
            + static_cast<std::ptrdiff_t>(auditedGridCount);
        if (std::find(auditedGrids.begin(), end, grid) != end)
            continue;
        auditedGrids[auditedGridCount++] = grid;
        const auto one = auditFullCycle(grid, ChorusMode::One);
        const auto two = auditFullCycle(grid, ChorusMode::Two);
        fullCyclesExact = fullCyclesExact && one.passed() && two.passed();
        std::cout << "ledger full cycles grid=" << grid
                  << " I=" << (one.passed() ? "PASS" : "FAIL")
                  << " II=" << (two.passed() ? "PASS" : "FAIL")
                  << " frames=" << one.frames << "/" << two.frames
                  << " edgesA/B=" << one.expected.edgesA << "/"
                  << one.expected.edgesB << ";" << two.expected.edgesA
                  << "/" << two.expected.edgesB << '\n';
    }

    const bool completeMatrix = std::all_of(
        evaluated.begin(), evaluated.end(), [](bool value) { return value; });
    const bool familyIdentity = !completeMatrix || rawFamiliesExact(cells);
    const bool goldens = !completeMatrix || metricGoldensExact(cells);
    bool mutations = true;
    if (completeMatrix)
        mutations = mutationSensitivityPassed(
            cells[0], hotReferences[0], noiseReferences[0]);
    const bool sixUniqueGrids = !completeMatrix || auditedGridCount == 6u;
    const bool passed = inputSupportCardExact
        && classificationsExact && infrastructureExact
        && fullCyclesExact
        && familyIdentity && goldens && mutations && sixUniqueGrids;
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "contract input_support="
              << (inputSupportCardExact ? "PASS" : "FAIL")
              << " classification="
              << (classificationsExact ? "PASS" : "FAIL")
              << " infrastructure="
              << (infrastructureExact ? "PASS" : "FAIL")
              << " full_cycle_ledgers=" << (fullCyclesExact ? "PASS" : "FAIL")
              << " raw_family_identity=" << (familyIdentity ? "PASS" : "FAIL")
              << " metric_goldens=" << (goldens ? "PASS" : "FAIL")
              << " mutation_sensitivity=" << (mutations ? "PASS" : "FAIL")
              << " six_unique_grids=" << (sixUniqueGrids ? "PASS" : "FAIL")
              << " elapsed=" << std::fixed << std::setprecision(2)
              << elapsed << "s\n";
    if (selfTest)
        std::cout << "BBD dynamic quality classification self-test: "
                  << (passed ? "PASS" : "FAIL") << '\n';
    return passed;
}
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 1)
            return runAudit(-1, false) ? EXIT_SUCCESS : EXIT_FAILURE;
        if (argc == 2 && std::string_view(argv[1]) == "--self-test")
            return runAudit(-1, true) ? EXIT_SUCCESS : EXIT_FAILURE;
        if (argc == 2
            && std::string_view(argv[1]) == "--shipping-self-test")
            return runShippingPortabilitySelfTest()
                ? EXIT_SUCCESS : EXIT_FAILURE;
        if (argc == 3 && std::string_view(argv[1]) == "--row")
        {
            const int row = std::atoi(argv[2]);
            if (row < 0 || row >= static_cast<int>(selectorRows.size()))
                throw std::runtime_error("row must be in [0, 9]");
            return runAudit(row, false) ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        std::cerr << "Usage: YouKnowBbdDynamicQualityAudit "
                     "[--self-test|--shipping-self-test|--row 0..9]\n";
        return EXIT_FAILURE;
    }
    catch (const std::exception& error)
    {
        std::cerr << "BBD dynamic quality audit FAILED: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
