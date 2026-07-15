#include "MarsEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace mars
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;
constexpr float envelopeRange = 6.90775527898f; // -60 dB in the requested time.
constexpr float thermalVoltage = 0.026f;
constexpr float twiceThermalVoltage = 2.0f * thermalVoltage;
constexpr float bbdMeasuredGain = 1.30316678f; // +2.3 dB, Holters/Parker 2018.
// A subtle, provisionally fitted -0.45 dB of charge-transfer loss is present at the
// 50 kHz reference clock. The analogue gain calibration below compensates it,
// so the complete line still measures the published +2.3 dB at that clock.
// Clock-history variation remains audible instead of being double-counted as
// a second fixed insertion loss.
constexpr float bbdReferenceChargeGain = 0.949511f;
constexpr float bbdCompensatedGain = bbdMeasuredGain / bbdReferenceChargeGain;
constexpr float bbdReferenceClockHz = 50000.0f;
constexpr float decibelsToNaturalLog = 0.11512925465f;
constexpr std::size_t transistorPairTableIntervals = 1024;
constexpr float transistorPairTableLimit = 7.0f;
// 137-tap equal-ripple half-band decimator. The omitted even taps are exactly
// zero and the centre tap is exactly 0.5, leaving 34 unique side products.
// Final float response: < 0.00017 dB ripple through 0.455 of the 2x Nyquist
// and > 100 dB rejection from 0.545 upward.
constexpr std::array<float, 34> halfbandSideCoefficients {{
    -1.0571764506e-05f,  1.7359345293e-05f, -3.1163111998e-05f,
     5.1753715525e-05f, -8.1313795818e-05f,  1.2246193364e-04f,
    -1.7829195713e-04f,  2.5241030380e-04f, -3.4897186561e-04f,
     4.7271532821e-04f, -6.2900054036e-04f,  8.2385097630e-04f,
    -1.0640077526e-03f,  1.3570019510e-03f, -1.7112577334e-03f,
     2.1362432744e-03f, -2.6426916011e-03f,  3.2429292332e-03f,
    -3.9513632655e-03f,  4.7852084972e-03f, -5.7655782439e-03f,
     6.9191521034e-03f, -8.2807587460e-03f,  9.8974872380e-03f,
    -1.1835452169e-02f,  1.4191368595e-02f, -1.7113441601e-02f,
     2.0841548219e-02f, -2.5791287422e-02f,  3.2750040293e-02f,
    -4.3409109116e-02f,  6.2172815204e-02f, -1.0520371050e-01f,
     3.1800898910e-01f,
}};
const std::array<float, transistorPairTableIntervals + 1> transistorPairTable = []
{
    std::array<float, transistorPairTableIntervals + 1> values {};
    for (std::size_t index = 0; index < values.size(); ++index)
        values[index] = std::tanh(
            transistorPairTableLimit * static_cast<float>(index)
            / static_cast<float>(transistorPairTableIntervals));
    return values;
}();

float transistorPairTanh(float value) noexcept
{
    // The ladder evaluates transistor differential pairs throughout its bounded
    // nonlinear solve. Linear interpolation through a dense 4 KiB table of
    // exact tanh points keeps that physical transfer practical at full
    // polyphony. An exhaustive float-domain check measured less than 4.6e-6
    // absolute error (the analytic interpolation bound is 4.7e-6);
    // outside +/-7 the reference is already within 1.7e-6 of its asymptote.
    const float magnitude = std::abs(value);
    if (magnitude < 0.000244140625f)
        return value;
    if (magnitude >= transistorPairTableLimit)
        return std::copysign(1.0f, value);

    constexpr float tableScale = static_cast<float>(transistorPairTableIntervals)
                               / transistorPairTableLimit;
    const float position = magnitude * tableScale;
    const auto index = std::min(static_cast<std::size_t>(position),
                                transistorPairTableIntervals - 1);
    const float t = position - static_cast<float>(index);
    const float y0 = transistorPairTable[index];
    const float y1 = transistorPairTable[index + 1];
    const float interpolated = y0 + t * (y1 - y0);
    return std::copysign(interpolated, value);
}

float finiteOr(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float triangleAtPhase(float phase) noexcept
{
    phase -= std::floor(phase);
    return phase < 0.5f ? -1.0f + 4.0f * phase : 3.0f - 4.0f * phase;
}

float safetyLimit(float value) noexcept
{
    if (!std::isfinite(value))
        return 0.0f;

    const float magnitude = std::abs(value);
    if (magnitude <= 0.98f)
        return value;

    const float excess = magnitude - 0.98f;
    const float limited = 0.98f + excess / (1.0f + excess / 0.22f);
    return std::copysign(std::min(limited, 1.2f), value);
}
} // namespace

void MarsEngine::HalfbandDecimator::reset() noexcept
{
    left.fill(0.0f);
    right.fill(0.0f);
    writeIndex = 0;
}

void MarsEngine::ParallelAnalogFilter::configure(float sampleRate,
                                                  bool inputFilter,
                                                  float frequencyScale) noexcept
{
    // Partial-fraction coefficients obtained by numerical circuit analysis of
    // the Juno-60's fifth-order BBD input/output filters (Holters and Parker,
    // DAFx-18, Table 1). Conjugate pairs are kept explicitly; this parallel
    // impulse-invariant form remains stable when the ensemble runs at 1x, 2x,
    // or 4x and preserves the measured analogue roll-off inside the HQ island.
    constexpr std::array<double, 5> inputResidueReal {
        251589.0, -130428.0, -130428.0, 4634.0, 4634.0
    };
    constexpr std::array<double, 5> inputResidueImag {
        0.0, -4165.0, 4165.0, -22873.0, 22873.0
    };
    constexpr std::array<double, 5> inputPoleReal {
        -46580.0, -55482.0, -55482.0, -26292.0, -26292.0
    };
    constexpr std::array<double, 5> inputPoleImag {
        0.0, 25082.0, -25082.0, -59437.0, 59437.0
    };
    constexpr std::array<double, 5> outputResidueReal {
        5092.0, 11256.0, 11256.0, 13802.0, 13802.0
    };
    constexpr std::array<double, 5> outputResidueImag {
        0.0, -99566.0, 99566.0, -24606.0, 24606.0
    };
    constexpr std::array<double, 5> outputPoleReal {
        -176261.0, -51468.0, -51468.0, -26276.0, -26276.0
    };
    constexpr std::array<double, 5> outputPoleImag {
        0.0, 21437.0, -21437.0, -59699.0, 59699.0
    };

    sampleRate = std::max(sampleRate, 8000.0f);
    frequencyScale = std::clamp(frequencyScale, 0.96f, 1.04f);
    const double interval = 1.0 / static_cast<double>(sampleRate);
    double dcReal = 0.0;
    double dcImag = 0.0;
    for (std::size_t index = 0; index < stateReal.size(); ++index)
    {
        const double residueReal = (inputFilter ? inputResidueReal[index]
                                                 : outputResidueReal[index])
                                 * frequencyScale;
        const double residueImag = (inputFilter ? inputResidueImag[index]
                                                 : outputResidueImag[index])
                                 * frequencyScale;
        const double analoguePoleReal = (inputFilter ? inputPoleReal[index]
                                                       : outputPoleReal[index])
                                        * frequencyScale;
        const double analoguePoleImag = (inputFilter ? inputPoleImag[index]
                                                       : outputPoleImag[index])
                                        * frequencyScale;
        const double radius = std::exp(analoguePoleReal * interval);
        const double angle = analoguePoleImag * interval;
        const double digitalPoleReal = radius * std::cos(angle);
        const double digitalPoleImag = radius * std::sin(angle);
        const double digitalGainReal = interval * residueReal;
        const double digitalGainImag = interval * residueImag;
        poleReal[index] = static_cast<float>(digitalPoleReal);
        poleImag[index] = static_cast<float>(digitalPoleImag);
        gainReal[index] = static_cast<float>(digitalGainReal);
        gainImag[index] = static_cast<float>(digitalGainImag);

        const double denominatorReal = 1.0 - digitalPoleReal;
        const double denominatorImag = -digitalPoleImag;
        const double denominatorSquared = denominatorReal * denominatorReal
                                        + denominatorImag * denominatorImag;
        dcReal += (digitalGainReal * denominatorReal
                 + digitalGainImag * denominatorImag) / denominatorSquared;
        dcImag += (digitalGainImag * denominatorReal
                 - digitalGainReal * denominatorImag) / denominatorSquared;
    }
    (void) dcImag;
    normalisation = std::abs(dcReal) > 1.0e-8
        ? static_cast<float>(1.0 / dcReal) : 1.0f;
    reset();
}

void MarsEngine::ParallelAnalogFilter::reset() noexcept
{
    stateReal.fill(0.0f);
    stateImag.fill(0.0f);
}

float MarsEngine::ParallelAnalogFilter::process(float input) noexcept
{
    input = std::clamp(finiteOr(input, 0.0f), -8.0f, 8.0f);
    float output = 0.0f;
    for (std::size_t index = 0; index < stateReal.size(); ++index)
    {
        const float previousReal = stateReal[index];
        const float previousImag = stateImag[index];
        stateReal[index] = poleReal[index] * previousReal
                         - poleImag[index] * previousImag + input;
        stateImag[index] = poleImag[index] * previousReal
                         + poleReal[index] * previousImag;
        output += gainReal[index] * stateReal[index]
                - gainImag[index] * stateImag[index];
    }
    output *= normalisation;
    return std::isfinite(output) ? output : 0.0f;
}

void MarsEngine::BbdLine::configure(float sampleRate, float frequencyScale) noexcept
{
    inputFilter.configure(sampleRate, true, frequencyScale);
    outputFilter.configure(sampleRate, false, frequencyScale);
}

void MarsEngine::BbdLine::reset(double initialClockPhase) noexcept
{
    cells.fill(0.0f);
    transferLogHistory.fill(0.0f);
    inputFilter.reset();
    outputFilter.reset();
    clockPhase = initialClockPhase - std::floor(initialClockPhase);
    heldOutput = 0.0f;
    transferMemory = 0.0f;
    rollingTransferLog = 0.0f;
    feedthroughCorrection.fill(0.0f);
    previousFilteredInput = 0.0f;
    const auto phaseSeed = static_cast<std::uint32_t>(std::llround(clockPhase * 4294967295.0));
    noiseState = MarsEngine::hash32(phaseSeed ^ 0x6d2b79f5u);
    writeIndex = 0;
    filteredInputInitialised = false;
}

float MarsEngine::BbdLine::process(float input,
                                   float clockFrequency,
                                   float sampleRate,
                                   float parasiticGain) noexcept
{
    const float filteredInput = inputFilter.process(input);
    if (!filteredInputInitialised)
    {
        previousFilteredInput = filteredInput;
        filteredInputInitialised = true;
    }
    clockFrequency = std::clamp(clockFrequency, 18000.0f, 90000.0f);
    sampleRate = std::max(sampleRate, 8000.0f);
    parasiticGain = std::clamp(finiteOr(parasiticGain, 0.0f), 0.0f, 1.0f);

    // Retain the measured +2.3 dB complete-line gain at the 50 kHz reference
    // while putting a small, clock-history-dependent charge loss inside the
    // device model. A restrained bias-asymmetric transfer follows it before
    // the measured reconstruction network.
    constexpr float drive = 1.045f;
    constexpr float bias = 0.009f;
    // Normalise by the local transfer slope, not its full-scale value: the
    // small-signal gain then remains the measured +2.3 dB instead of the
    // colour stage accidentally contributing another roughly +2.4 dB.
    const float normalisation = drive * MarsEngine::softSaturateDerivative(bias);
    const auto colour = [normalisation](float held) noexcept
    {
        return (MarsEngine::softSaturate(drive * bbdCompensatedGain * held + bias)
                - MarsEngine::softSaturate(bias))
               / std::max(normalisation, 0.1f);
    };

    // Resolve every complementary half-clock edge at its fractional position.
    // Full-clock boundaries transfer charge; both phases inject a tiny,
    // opposing cancellation residue. A compact fractional cubic kernel places
    // each residual between audio samples before the physical output filter;
    // this is both cheaper and safer than emitting a digital clock waveform,
    // particularly when multiple edges fall inside one sample.
    const double advance = static_cast<double>(clockFrequency / sampleRate);
    const double startPhase = clockPhase;
    std::int64_t halfEdgeOrdinal = static_cast<std::int64_t>(std::floor(2.0 * startPhase)) + 1;
    double nextEventDistance = 0.5 * static_cast<double>(halfEdgeOrdinal) - startPhase;
    double segmentStart = 0.0;
    double integratedOutput = feedthroughCorrection[0];
    for (std::size_t index = 1; index < feedthroughCorrection.size(); ++index)
        feedthroughCorrection[index - 1] = feedthroughCorrection[index];
    feedthroughCorrection.back() = 0.0f;
    float colouredHold = colour(heldOutput);

    const auto integrateSegment =
        [&integratedOutput, &segmentStart, &colouredHold](double segmentEnd) noexcept
    {
        const double segmentLength = std::max(0.0, segmentEnd - segmentStart);
        integratedOutput += segmentLength * static_cast<double>(colouredHold);
        segmentStart = segmentEnd;
    };
    const auto scheduleFeedthrough = [this](float amplitude, float eventTime) noexcept
    {
        const float t = std::clamp(eventTime, 0.0f, 1.0f);
        const float oneMinusT = 1.0f - t;
        const float t2 = t * t;
        const float t3 = t2 * t;
        const float oneMinusT2 = oneMinusT * oneMinusT;
        const float oneMinusT3 = oneMinusT2 * oneMinusT;
        const std::array<float, 4> weights{{
            oneMinusT3 / 6.0f,
            2.0f / 3.0f - t2 + 0.5f * t3,
            2.0f / 3.0f - oneMinusT2 + 0.5f * oneMinusT3,
            t3 / 6.0f,
        }};
        for (std::size_t index = 0; index < feedthroughCorrection.size(); ++index)
            feedthroughCorrection[index] += amplitude * weights[index];
    };

    // Fade the ultrasonic carrier before it can fold around a low host
    // Nyquist frequency. With default HQ processing the whole range remains
    // represented at 176.4/192 kHz and is subsequently removed by the measured
    // analogue network and the half-band return filter.
    const float clockRatio = clockFrequency / sampleRate;
    const float feedthroughHeadroom = std::clamp((0.48f - clockRatio) / 0.10f, 0.0f, 1.0f);
    const float antialiasGain =
        feedthroughHeadroom * feedthroughHeadroom * (3.0f - 2.0f * feedthroughHeadroom);
    const float rootClockRatio = std::sqrt(clockFrequency / bbdReferenceClockHz);
    const float eventLogLoss =
        -0.45f / rootClockRatio * decibelsToNaturalLog / static_cast<float>(bbdStagePairs);
    const float normalisedClock = clockFrequency / bbdReferenceClockHz;
    const float transferCoefficient =
        std::clamp(0.90f + 0.035f * (normalisedClock - 1.0f), 0.84f, 0.95f);
    // Provisional cancellation-mismatch voicing, deliberately isolated for
    // later fitting to multi-unit clock-residue captures.
    constexpr float feedthroughChargeArea = 9.4e-9f;
    const float feedthroughScale =
        parasiticGain * antialiasGain * feedthroughChargeArea * sampleRate * rootClockRatio;

    while (nextEventDistance <= advance)
    {
        const double eventTime = std::clamp(nextEventDistance / advance, segmentStart, 1.0);
        integrateSegment(eventTime);

        const bool transferEdge = (halfEdgeOrdinal & 1) == 0;
        if (transferEdge)
        {
            const float capture =
                previousFilteredInput
                + static_cast<float>(eventTime) * (filteredInput - previousFilteredInput);

            // One log-efficiency value per transfer and a rolling 128-event
            // sum reproduce the complete packet history in O(1), even while
            // the clock is being modulated. The modest clock curve is a
            // conservative datasheet-informed calibration target pending
            // multi-unit hardware captures.
            const auto cellIndex = static_cast<std::size_t>(writeIndex);
            rollingTransferLog -= transferLogHistory[cellIndex];
            transferLogHistory[cellIndex] = eventLogLoss;
            rollingTransferLog += eventLogLoss;
            const float chargeGain = std::exp(std::clamp(rollingTransferLog, -1.0f, 0.0f));
            const float chargedOutput = cells[cellIndex] * chargeGain;

            // A single BBD-rate pole captures incomplete charge transfer. It
            // has unity DC gain and becomes slightly slower at the bottom of
            // the modulated clock range, avoiding a 256-device runtime loop.
            transferMemory += transferCoefficient * (chargedOutput - transferMemory);

            noiseState = MarsEngine::hash32(noiseState + 0x9e3779b9u);
            const float firstUniform = static_cast<float>(noiseState & 0x00ffffffu) / 16777216.0f;
            noiseState = MarsEngine::hash32(noiseState + 0x9e3779b9u);
            const float secondUniform = static_cast<float>(noiseState & 0x00ffffffu) / 16777216.0f;
            constexpr float unitRmsTpdf = 2.449489743f;
            // Compensates the following small-signal colour/output-filter gain
            // for an approximately -88 dBFS output-referred provisional target.
            constexpr float eventNoiseRms = 5.27e-5f;
            const float eventNoise =
                eventNoiseRms * unitRmsTpdf * (firstUniform + secondUniform - 1.0f);
            heldOutput = transferMemory + parasiticGain * eventNoise;
            cells[cellIndex] = capture;
            writeIndex = (writeIndex + 1) & (bbdStagePairs - 1);
            colouredHold = colour(heldOutput);
        }

        const float polarity = (halfEdgeOrdinal & 1) != 0 ? 1.0f : -1.0f;
        const float injection = polarity * feedthroughScale * (1.0f + 0.06f * std::abs(heldOutput));
        scheduleFeedthrough(injection, static_cast<float>(eventTime));

        ++halfEdgeOrdinal;
        nextEventDistance += 0.5;
    }
    integrateSegment(1.0);
    clockPhase = startPhase + advance;
    clockPhase -= std::floor(clockPhase);
    previousFilteredInput = filteredInput;
    return outputFilter.process(static_cast<float>(integratedOutput));
}

void MarsEngine::BbdCompander::configure(float sampleRate) noexcept
{
    sampleRate = std::max(sampleRate, 8000.0f);
    detectorCoefficient = 1.0f - std::exp(-1.0f / (0.0047f * sampleRate));
}

void MarsEngine::BbdCompander::reset() noexcept
{
    compressorEnvelope = 0.125f;
    expanderEnvelope = 0.125f * nominalLineGain;
}

float MarsEngine::BbdCompander::compress(float input) noexcept
{
    constexpr float reference = 0.125f;
    constexpr float detectorFloor = 1.0e-4f;
    compressorEnvelope += detectorCoefficient * (std::abs(input) - compressorEnvelope);
    if (std::abs(input) < 1.0e-12f && compressorEnvelope < 1.0e-10f)
        compressorEnvelope = 0.0f;
    const float gain =
        std::clamp(std::sqrt(reference / std::max(compressorEnvelope, detectorFloor)), 0.25f, 4.0f);
    const float output = input * gain;
    return std::isfinite(output) ? output : 0.0f;
}

void MarsEngine::BbdCompander::expand(float &left, float &right) noexcept
{
    constexpr float reference = 0.125f;
    const float detectorInput = 0.5f * (std::abs(left) + std::abs(right));
    expanderEnvelope += detectorCoefficient * (detectorInput - expanderEnvelope);
    if (detectorInput < 1.0e-12f && expanderEnvelope < 1.0e-10f)
        expanderEnvelope = 0.0f;
    // The complete MN3009 line is calibrated to +2.3 dB. Companding around a
    // fixed-gain channel otherwise squares that gain, so reference the
    // expander to the actual nominal line and preserve level when COMP toggles.
    const float gain =
        std::clamp(expanderEnvelope / (reference * nominalLineGain), 0.05f, 4.0f);
    left = std::isfinite(left * gain) ? left * gain : 0.0f;
    right = std::isfinite(right * gain) ? right * gain : 0.0f;
}

void MarsEngine::Envelope::reset() noexcept
{
    stage = EnvelopeStage::Idle;
    value = 0.0f;
}

void MarsEngine::Envelope::noteOn() noexcept
{
    value = 0.0f;
    stage = EnvelopeStage::Attack;
}

void MarsEngine::Envelope::noteOff() noexcept
{
    if (stage != EnvelopeStage::Idle)
        stage = EnvelopeStage::Release;
}

float MarsEngine::Envelope::tick(float attackCoefficient, float decayCoefficient,
                                 float sustain, float releaseCoefficient) noexcept
{
    sustain = std::clamp(sustain, 0.0f, 1.0f);

    switch (stage)
    {
        case EnvelopeStage::Idle:
            value = 0.0f;
            break;

        case EnvelopeStage::Attack:
            value += (1.001f - value) * attackCoefficient;
            if (value >= 0.999f)
            {
                value = 1.0f;
                stage = EnvelopeStage::Decay;
            }
            break;

        case EnvelopeStage::Decay:
            value += (sustain - value) * decayCoefficient;
            if (std::abs(value - sustain) < 0.0005f)
            {
                value = sustain;
                stage = EnvelopeStage::Sustain;
            }
            break;

        case EnvelopeStage::Sustain:
            value += (sustain - value) * decayCoefficient;
            break;

        case EnvelopeStage::Release:
            value += (0.0f - value) * releaseCoefficient;
            if (value < 0.00001f)
                reset();
            break;
    }

    return value;
}

void MarsEngine::LadderFilter::reset() noexcept
{
    stageVoltage.fill(0.0f);
    stageTanh.fill(0.0f);
    previousInputVoltage = 0.0f;
    previousFeedbackTanh = 0.0f;
    cachedFeedbackGain = 0.0f;
    stageTanhValid = true;
    feedbackTanhValid = false;
}

/*
    The four-stage nonlinear ladder below solves the bilinear-discretised
    implicit circuit equations from:

    S. D'Angelo and V. Valimaki, "Generalized Moog Ladder Filter: Part II--
    Explicit Nonlinear Model through a Novel Delay-Free Loop Implementation
    Method," IEEE/ACM TASLP, vol. 22, no. 12, 2014.

    The authors' non-iterative reference implementation is excellent inside its
    documented range, but its finite-precision evaluation is reported coherent
    only through fc <= fs/8. Mars exposes a 20 kHz cutoff even in native-rate
    mode, so it solves the original implicit equations with a residual-decreasing
    damped Newton method instead. Its small cyclic-bidiagonal Jacobian is solved
    analytically. Both the Newton and backtracking counts have fixed maxima, so
    no allocation, unbounded iteration, or artificial delay is introduced.
*/
float MarsEngine::LadderFilter::process(float inputVoltage,
                                        float frequencyTangent,
                                        float feedbackGain,
                                        float frequencyScale) noexcept
{
    inputVoltage = std::clamp(inputVoltage, -0.8f, 0.8f);
    frequencyTangent = std::clamp(frequencyTangent, 0.00001f, 8.0f);
    const float k = std::clamp(feedbackGain, 0.0f, 3.98f);
    const float g = frequencyTangent * std::clamp(frequencyScale, 0.5f, 2.0f);

    const auto previous = stageVoltage;
    std::array<float, 4> estimate = previous;
    std::array<float, 4> previousTanh = stageTanh;
    if (!stageTanhValid)
        for (std::size_t stage = 0; stage < previous.size(); ++stage)
            previousTanh[stage] = transistorPairTanh(
                previous[stage] / twiceThermalVoltage);
    const float historyFeedbackTanh = feedbackTanhValid && cachedFeedbackGain == k
        ? previousFeedbackTanh
        : transistorPairTanh(
            (previousInputVoltage + k * previous.back()) / twiceThermalVoltage);

    struct Evaluation
    {
        std::array<float, 4> currentTanh {};
        std::array<float, 4> residual {};
        float feedbackTanh { 0.0f };
        float squaredResidual { std::numeric_limits<float>::infinity() };
        float maximumResidual { std::numeric_limits<float>::infinity() };
        bool finite { false };
    };

    const auto evaluate = [&] (const std::array<float, 4>& candidate,
                               const std::array<float, 4>* knownTanh = nullptr) noexcept
    {
        Evaluation result;
        result.finite = std::all_of(candidate.begin(), candidate.end(),
                                    [] (float value) noexcept
                                    {
                                        return std::isfinite(value);
                                    });
        if (!result.finite)
            return result;

        if (knownTanh != nullptr)
            result.currentTanh = *knownTanh;
        else
            for (std::size_t stage = 0; stage < candidate.size(); ++stage)
                result.currentTanh[stage] = transistorPairTanh(
                    candidate[stage] / twiceThermalVoltage);

        result.feedbackTanh = transistorPairTanh(
            (inputVoltage + k * candidate.back()) / twiceThermalVoltage);
        result.residual[0] = candidate[0] - previous[0]
                           + twiceThermalVoltage * g
                               * (result.currentTanh[0] + previousTanh[0]
                                  + result.feedbackTanh + historyFeedbackTanh);
        for (std::size_t stage = 1; stage < result.residual.size(); ++stage)
            result.residual[stage] = candidate[stage] - previous[stage]
                                   - twiceThermalVoltage * g
                                       * (result.currentTanh[stage - 1]
                                          + previousTanh[stage - 1]
                                          - result.currentTanh[stage]
                                          - previousTanh[stage]);

        result.squaredResidual = 0.0f;
        result.maximumResidual = 0.0f;
        for (const float value : result.residual)
        {
            result.squaredResidual += value * value;
            result.maximumResidual = std::max(result.maximumResidual,
                                              std::abs(value));
        }
        result.finite = std::isfinite(result.squaredResidual)
                     && std::isfinite(result.maximumResidual);
        return result;
    };

    // Starting at the previous state is a good predictor for ordinary audio,
    // but a high-cutoff full-scale discontinuity can make an unrestricted
    // Newton step cross several saturated tanh regions. Accepting only a step
    // that reduces ||residual||^2 prevents that divergence. Typical musical
    // samples finish in the first bounded pass. A second pass with deeper
    // backtracking is reserved for hostile input/control jumps, so its stronger
    // global-convergence protection adds no cost to the normal path.
    constexpr int normalNewtonUpdates = 16;
    constexpr int normalBacktrackingSteps = 8;
    constexpr int rescueNewtonUpdates = 16;
    constexpr int rescueBacktrackingSteps = 20;
    // The 1024-segment tanh interpolator itself contributes at most about
    // 1.7e-4 of normalized equation residual at the reachable maximum g. A
    // target of the same order keeps the combined exact-tanh residual beneath
    // the independently tested 6e-4 bound without wasting another update.
    constexpr float residualTolerance = twiceThermalVoltage * 3.0e-4f;
    Evaluation evaluation = evaluate(estimate, &previousTanh);
    const auto improve = [&] (int maximumUpdates,
                              int maximumBacktrackingSteps) noexcept
    {
        for (int iteration = 0;
             iteration < maximumUpdates
                 && evaluation.finite
                 && evaluation.maximumResidual > residualTolerance;
             ++iteration)
        {
            const auto& residual = evaluation.residual;
            std::array<float, 4> derivative {};
            for (std::size_t stage = 0; stage < derivative.size(); ++stage)
                derivative[stage] = 1.0f
                                  - evaluation.currentTanh[stage]
                                      * evaluation.currentTanh[stage];
            const float feedbackDerivative = 1.0f
                                           - evaluation.feedbackTanh
                                               * evaluation.feedbackTanh;

            // Solve J * delta = -residual. Rows 1..3 are lower-bidiagonal;
            // row zero adds only the cyclic feedback term in column three.
            const float diagonal0 = 1.0f + g * derivative[0];
            const float feedbackCoupling = g * k * feedbackDerivative;
            std::array<float, 4> slope {};
            std::array<float, 4> offset {};
            for (std::size_t stage = 1; stage < slope.size(); ++stage)
            {
                const float lower = -g * derivative[stage - 1];
                const float diagonal = 1.0f + g * derivative[stage];
                const float previousSlope = stage == 1 ? 1.0f : slope[stage - 1];
                const float previousOffset = stage == 1 ? 0.0f : offset[stage - 1];
                slope[stage] = -lower * previousSlope / diagonal;
                offset[stage] = (-residual[stage] - lower * previousOffset) / diagonal;
            }

            const float denominator = diagonal0 + feedbackCoupling * slope[3];
            const float delta0 = (-residual[0] - feedbackCoupling * offset[3])
                               / std::max(denominator, 1.0e-8f);
            std::array<float, 4> delta {};
            delta[0] = delta0;
            for (std::size_t stage = 1; stage < estimate.size(); ++stage)
                delta[stage] = slope[stage] * delta0 + offset[stage];

            float stepScale = 1.0f;
            bool accepted = false;
            for (int backtrack = 0;
                 backtrack < maximumBacktrackingSteps;
                 ++backtrack)
            {
                std::array<float, 4> candidate {};
                for (std::size_t stage = 0; stage < candidate.size(); ++stage)
                    candidate[stage] = estimate[stage] + stepScale * delta[stage];

                auto trial = evaluate(candidate);
                if (trial.finite
                    && trial.squaredResidual < evaluation.squaredResidual)
                {
                    estimate = candidate;
                    evaluation = trial;
                    accepted = true;
                    break;
                }
                stepScale *= 0.5f;
            }

            if (!accepted)
                break;
        }
    };

    improve(normalNewtonUpdates, normalBacktrackingSteps);
    if (evaluation.finite && evaluation.maximumResidual > residualTolerance)
    {
        improve(rescueNewtonUpdates, rescueBacktrackingSteps);
    }

    if (!evaluation.finite)
    {
        reset();
        return 0.0f;
    }

    // Never publish a state that failed the documented LUT-equation residual
    // ceiling. The previous committed state/input/cache remain a coherent,
    // finite one-sample hold; the next sample gets a fresh bounded solve. This
    // is an emergency guard after both fixed-cost solve passes, not a normal
    // approximation path.
    if (evaluation.maximumResidual > residualTolerance)
    {
        return (previous.back() / twiceThermalVoltage) * (1.0f + k);
    }

    stageVoltage = estimate;
    stageTanh = evaluation.currentTanh;
    stageTanhValid = true;
    previousInputVoltage = inputVoltage;
    previousFeedbackTanh = evaluation.feedbackTanh;
    cachedFeedbackGain = k;
    feedbackTanhValid = true;

    constexpr float silenceVoltage = twiceThermalVoltage * 1.0e-4f;
    if (std::abs(inputVoltage) < 1.0e-12f
        && std::all_of(stageVoltage.begin(), stageVoltage.end(),
                       [] (float value) noexcept
                       {
                           return std::abs(value) < silenceVoltage;
                       }))
    {
        reset();
        return 0.0f;
    }

    // The circuit state is expressed in volts. The reference implementation's
    // optional DC compensation multiplies by (1 + k), cancelling the ladder's
    // otherwise severe passband loss as resonance rises. The raw loss is a
    // genuine ladder property, but at Mars' default resonance it removes about
    // 6.5 dB from bass fundamentals and sounds more broken than characterful.
    // Nonlinear resonance and self-oscillation remain; only the static low-band
    // attenuation is restored before the bounded VCA stage.
    return (stageVoltage.back() / twiceThermalVoltage) * (1.0f + k);
}

void MarsEngine::StateVariableFilter::reset() noexcept
{
    ic1eq = 0.0f;
    ic2eq = 0.0f;
}

void MarsEngine::StateVariableFilter::process(float input, float g,
                                              float resonance,
                                              float& low, float& band,
                                              float& high) noexcept
{
    g = std::clamp(g, 0.00001f, 8.0f);
    resonance = std::clamp(resonance, 0.0f, 0.995f);

    // This remains a stable TPT state-variable approximation rather than a
    // component-level SEM clone. Nonlinearity is applied once, at the shared
    // physical-voltage filter input, before this method is called.
    const float driven = input;
    const float damping = 2.0f - 1.94f * resonance;
    const float a1 = 1.0f / (1.0f + g * (g + damping));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = driven - ic2eq;
    const float v1 = a1 * ic1eq + a2 * v3;
    const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
    ic1eq = 2.0f * v1 - ic1eq;
    ic2eq = 2.0f * v2 - ic2eq;

    band = v1;
    low = v2;
    const float highRaw = driven - damping * v1 - v2;
    high = highRaw;
}

MarsEngine::MarsEngine() noexcept
{
    targetParameters_ = sanitise(EngineParameters {});
    smoothedParameters_ = targetParameters_;
}

EngineParameters MarsEngine::sanitise(const EngineParameters& source) noexcept
{
    EngineParameters p = source;
    if (p.voiceMode != VoiceMode::Poly && p.voiceMode != VoiceMode::Unison
        && p.voiceMode != VoiceMode::Fifth && p.voiceMode != VoiceMode::Mono)
        p.voiceMode = VoiceMode::Poly;
    const auto validOscillatorModel = [](OscillatorModel model)
    {
        return model == OscillatorModel::Vco || model == OscillatorModel::Dco;
    };
    if (!validOscillatorModel(p.osc1Model))
        p.osc1Model = OscillatorModel::Vco;
    if (!validOscillatorModel(p.osc2Model))
        p.osc2Model = OscillatorModel::Vco;
    const auto validWave = [](OscillatorWave wave)
    {
        return wave == OscillatorWave::Saw || wave == OscillatorWave::Pulse || wave == OscillatorWave::Triangle;
    };
    if (!validWave(p.osc1Wave))
        p.osc1Wave = OscillatorWave::Saw;
    if (!validWave(p.osc2Wave))
        p.osc2Wave = OscillatorWave::Pulse;
    if (p.filterModel != FilterModel::Ladder && p.filterModel != FilterModel::Sem)
        p.filterModel = FilterModel::Ladder;
    if (p.lfoWave != LfoWaveform::Triangle && p.lfoWave != LfoWaveform::Sine
        && p.lfoWave != LfoWaveform::SampleHold)
        p.lfoWave = LfoWaveform::Triangle;
    p.unisonVoices = std::clamp(p.unisonVoices, 2, 8);
    p.polyphonyLimit = std::clamp(p.polyphonyLimit, 1, maxVoices);
    p.osc1Octave = std::clamp(p.osc1Octave, -2, 2);
    p.osc2Octave = std::clamp(p.osc2Octave, -2, 2);
    p.osc2Semitones = std::clamp(p.osc2Semitones, -12, 12);
    p.osc2FineCents = std::clamp(finiteOr(p.osc2FineCents, 0.0f), -100.0f, 100.0f);
    p.pulseWidth = std::clamp(finiteOr(p.pulseWidth, 0.48f), 0.05f, 0.95f);
    p.crossMod = std::clamp(finiteOr(p.crossMod, 0.08f), 0.0f, 1.0f);
    p.oscMix = std::clamp(finiteOr(p.oscMix, 0.42f), 0.0f, 1.0f);
    p.subLevel = std::clamp(finiteOr(p.subLevel, 0.18f), 0.0f, 1.0f);
    p.noiseLevel = std::clamp(finiteOr(p.noiseLevel, 0.04f), 0.0f, 1.0f);
    p.cutoffHz = std::clamp(finiteOr(p.cutoffHz, 4200.0f), 18.0f, 22000.0f);
    p.resonance = std::clamp(finiteOr(p.resonance, 0.28f), 0.0f, 0.995f);
    p.filterDrive = std::clamp(finiteOr(p.filterDrive, 0.24f), 0.0f, 1.0f);
    p.filterShape = std::clamp(finiteOr(p.filterShape, 0.35f), 0.0f, 1.0f);
    p.filterEnvAmount = std::clamp(finiteOr(p.filterEnvAmount, 0.42f), -1.0f, 1.0f);
    p.lfoFilterOctaves = std::clamp(finiteOr(p.lfoFilterOctaves, 0.15f), -8.0f, 8.0f);
    p.filterKeyTrack = std::clamp(finiteOr(p.filterKeyTrack, 0.45f), 0.0f, 1.0f);
    p.ampAttack = std::clamp(finiteOr(p.ampAttack, 0.008f), 0.0005f, 20.0f);
    p.ampDecay = std::clamp(finiteOr(p.ampDecay, 0.38f), 0.0005f, 20.0f);
    p.ampSustain = std::clamp(finiteOr(p.ampSustain, 0.78f), 0.0f, 1.0f);
    p.ampRelease = std::clamp(finiteOr(p.ampRelease, 0.55f), 0.0005f, 20.0f);
    p.filterAttack = std::clamp(finiteOr(p.filterAttack, 0.012f), 0.0005f, 20.0f);
    p.filterDecay = std::clamp(finiteOr(p.filterDecay, 0.45f), 0.0005f, 20.0f);
    p.filterSustain = std::clamp(finiteOr(p.filterSustain, 0.34f), 0.0f, 1.0f);
    p.filterRelease = std::clamp(finiteOr(p.filterRelease, 0.62f), 0.0005f, 20.0f);
    p.lfoRateHz = std::clamp(finiteOr(p.lfoRateHz, 4.8f), 0.02f, 30.0f);
    p.lfoPitchCents = std::clamp(finiteOr(p.lfoPitchCents, 8.0f), -200.0f, 200.0f);
    p.lfoPwm = std::clamp(finiteOr(p.lfoPwm, 0.18f), 0.0f, 1.0f);
    p.drift = std::clamp(finiteOr(p.drift, 0.28f), 0.0f, 1.0f);
    p.spread = std::clamp(finiteOr(p.spread, 0.58f), 0.0f, 1.0f);
    p.glideSeconds = std::clamp(finiteOr(p.glideSeconds, 0.0f), 0.0f, 5.0f);
    p.velocityAmount = std::clamp(finiteOr(p.velocityAmount, 0.72f), 0.0f, 1.0f);
    p.chorusMix = std::clamp(finiteOr(p.chorusMix, 0.32f), 0.0f, 1.0f);
    p.chorusRateHz = std::clamp(finiteOr(p.chorusRateHz, 0.56f), 0.03f, 8.0f);
    p.outputGain = std::clamp(finiteOr(p.outputGain, 0.72f), 0.0f, 2.0f);
    return p;
}

void MarsEngine::updateProcessingRate() noexcept
{
    // HQ keeps the complete nonlinear voice and BBD ensemble island at or
    // above 176.4 kHz: 4x at 44.1/48, 2x at 88.2/96, then native at high-rate
    // sessions. Power-of-two staging lets the same measured half-band return
    // filter serve every host rate without an ad-hoc wide transition band.
    oversampling_ = 1;
    while (oversamplingEnabled_ && oversampling_ < maximumOversampleFactor
           && sampleRate_ * static_cast<double>(oversampling_) < minimumHqProcessingRate)
        oversampling_ *= 2;
    oversampledRate_ = static_cast<float>(sampleRate_ * static_cast<double>(oversampling_));
    filterCrossfadeSamples_ =
        std::max(1, static_cast<int>(std::lround(0.003 * static_cast<double>(oversampledRate_))));
    oscillatorModelCrossfadeSamples_ =
        std::max(1, static_cast<int>(std::lround(0.002 * static_cast<double>(oversampledRate_))));
    waveformCrossfadeSamples_ =
        std::max(1, static_cast<int>(std::lround(0.003 * static_cast<double>(oversampledRate_))));
    const float dcoBandwidth = std::min(18500.0f, 0.42f * oversampledRate_);
    const float prewarped = std::tan(pi * dcoBandwidth / oversampledRate_);
    dcoReconstructionGain_ = std::clamp(prewarped / (1.0f + prewarped), 0.01f, 0.99f);
    dcoPulseRise_ = 1.0f - std::exp(-1.0f / (2.4e-6f * oversampledRate_));
    dcoPulseFall_ = 1.0f - std::exp(-1.0f / (3.1e-6f * oversampledRate_));
    dcoControlPeriodSamples_ =
        std::max(1, static_cast<int>(std::lround(0.0042 * static_cast<double>(oversampledRate_))));
    velocitySmoothing_ = 1.0f - std::exp(-1.0f / (0.005f * oversampledRate_));
    outputEnergySmoothing_ = 1.0f - std::exp(-1.0f / (0.008f * oversampledRate_));
    chorusLeft_.configure(oversampledRate_, 0.994f);
    chorusRight_.configure(oversampledRate_, 1.006f);
    chorusCompander_.configure(oversampledRate_);
    chorusActivityAttack_ = 1.0f - std::exp(-1.0f / (0.002f * oversampledRate_));
    chorusActivityRelease_ = std::exp(-9.210340372f / (0.020f * oversampledRate_));
    companderBlendCoefficient_ = 1.0f - std::exp(-1.0f / (0.020f * oversampledRate_));
    // A stolen voice contributes its last sample to this -60 dB / 2 ms tail.
    // It is short enough to remain a transient treatment rather than another
    // voice, and fixed state keeps the note-on path allocation-free.
    stealTailCoefficient_ = std::exp(-envelopeRange / (0.002f * std::max(oversampledRate_, 1.0f)));
}

int MarsEngine::getProcessingLatencySamples() const noexcept
{
    // Latency is fixed for the prepared host rate, even while HQ is Off. This
    // lets a deferred quality change remain real-time safe: the processor does
    // not need to notify the host from its audio callback. The native path is
    // delayed below to match the exact even-phase FIR latency.
    constexpr int halfbandGroupDelay = (halfbandTaps - 1) / 2;
    int maximumFactor = 1;
    while (maximumFactor < maximumOversampleFactor
           && sampleRate_ * static_cast<double>(maximumFactor) < minimumHqProcessingRate)
        maximumFactor *= 2;
    int latency = 0;
    for (int rateFactor = maximumFactor; rateFactor > 1; rateFactor /= 2)
        latency += halfbandGroupDelay / rateFactor;
    return latency;
}

void MarsEngine::prepare(double sampleRate, int maxBlockSize, bool oversamplingEnabled)
{
    (void) maxBlockSize;
    sampleRate = std::isfinite(sampleRate) ? sampleRate : 48000.0;
    sampleRate_ = std::clamp(sampleRate, 8000.0, maximumSupportedSampleRate);
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    oversamplingEnabled_ = oversamplingEnabled;
    oversamplingRequested_ = oversamplingEnabled;
    updateProcessingRate();
    // A 1.5 Hz servo still removes accumulated DC while retaining sub-octave
    // fundamentals on the bottom MIDI octave. The former 8 Hz corner was
    // already 3 dB down at the sub oscillator's lowest musically useful notes.
    dcCoefficient_ = std::exp(-twoPi * 1.5f * inverseSampleRate_);

    buildVoiceCards();
    prepared_ = true;
    reset();
}

bool MarsEngine::setOversamplingEnabled(bool enabled) noexcept
{
    if (oversamplingRequested_ == enabled)
        return false;

    oversamplingRequested_ = enabled;
    return applyPendingOversamplingIfIdle();
}

bool MarsEngine::applyPendingOversamplingIfIdle() noexcept
{
    if (oversamplingEnabled_ == oversamplingRequested_)
        return false;

    const int quietSamplesRequired = std::max(
        1, static_cast<int>(std::lround(0.025 * sampleRate_)));
    if (activeVoiceCount_ != 0 || oversamplingIdleSamples_ < quietSamplesRequired)
        return false;

    oversamplingEnabled_ = oversamplingRequested_;
    const int previousFactor = oversampling_;
    updateProcessingRate();
    if (oversampling_ == previousFactor)
        return false;

    // Quality changes are non-automatable and deferred until the engine and
    // short ensemble tail have been idle for 25 ms. Reinitialising only then
    // avoids interpreting oscillator, envelope, and nonlinear-filter states at
    // a different timebase without cutting a held note or producing a click.
    reset();
    return true;
}

void MarsEngine::reset()
{
    for (auto& voice : voices_)
        voice = Voice {};

    firstDecimator_.reset();
    secondDecimator_.reset();
    latencyDelayLeft_.fill(0.0f);
    latencyDelayRight_.fill(0.0f);
    latencyDelayWriteIndex_ = 0;
    chorusLeft_.reset(0.17);
    chorusRight_.reset(0.63);
    chorusCompander_.reset();
    chorusPhase_ = 0.173f;
    chorusActivity_ = 0.0f;
    dcInputLeft_ = dcInputRight_ = 0.0f;
    dcOutputLeft_ = dcOutputRight_ = 0.0f;
    lfoPhase_ = 0.0f;
    randomLfoValue_ = -0.27f;
    globalNoiseState_ = 0x6d2b79f5u;
    pitchBendTarget_ = 0.0f;
    pitchBend_ = 0.0f;
    modWheelTarget_ = 0.0f;
    modWheel_ = 0.0f;
    sustainPedalDown_ = false;
    smoothedParameters_ = targetParameters_;
    companderBlend_ = smoothedParameters_.chorusCompander ? 1.0f : 0.0f;
    if (smoothedParameters_.osc1Enabled && smoothedParameters_.osc2Enabled)
    {
        oscillator1MixGain_ = std::sqrt(std::max(0.0f, 1.0f - smoothedParameters_.oscMix));
        oscillator2MixGain_ = std::sqrt(std::max(0.0f, smoothedParameters_.oscMix));
    }
    else
    {
        oscillator1MixGain_ = smoothedParameters_.osc1Enabled ? 1.0f : 0.0f;
        oscillator2MixGain_ = smoothedParameters_.osc2Enabled ? 1.0f : 0.0f;
    }
    stealTailLeft_ = 0.0f;
    stealTailRight_ = 0.0f;
    generation_ = 0;
    activeVoiceCount_ = 0;
    driftControlCountdown_ = 0;
    oversamplingIdleSamples_ = std::max(
        1, static_cast<int>(std::lround(0.025 * sampleRate_)));
    lastPlayedMidi_ = 60.0f;
    hasLastPlayedMidi_ = false;
    clearHeldNotes();
}

void MarsEngine::setParameters(const EngineParameters& parameters)
{
    const auto previousMode = targetParameters_.voiceMode;
    targetParameters_ = sanitise(parameters);

    if (previousMode != VoiceMode::Mono && targetParameters_.voiceMode == VoiceMode::Mono)
    {
        // Reconstruct last-note priority from the physically held polyphonic
        // groups before reducing the render budget. Group-atomic stealing
        // would otherwise retire every layer of a held Unison/Fifth note and
        // leave Mono silent until the next note-on.
        clearHeldNotes();
        std::array<std::uint64_t, maxVoices> heldGenerations {};
        int heldGenerationCount = 0;
        for (const auto& voice : voices_)
        {
            if (!voice.active || !voice.keyDown)
                continue;

            const bool alreadyRemembered = std::find(
                heldGenerations.begin(),
                heldGenerations.begin() + heldGenerationCount,
                voice.generation) != heldGenerations.begin() + heldGenerationCount;
            if (!alreadyRemembered)
                heldGenerations[static_cast<std::size_t>(heldGenerationCount++)]
                    = voice.generation;
        }
        std::sort(heldGenerations.begin(),
                  heldGenerations.begin() + heldGenerationCount);
        for (int index = 0; index < heldGenerationCount; ++index)
        {
            const auto generation = heldGenerations[static_cast<std::size_t>(index)];
            const auto voice = std::find_if(
                voices_.begin(), voices_.end(),
                [generation](const Voice& candidate)
                {
                    return candidate.active && candidate.keyDown
                        && candidate.generation == generation
                        && candidate.layer == 0;
                });
            if (voice != voices_.end())
                rememberHeldNote(voice->rootMidi, voice->velocity);
        }

        int survivor = -1;
        std::uint64_t survivorGeneration = 0;
        const bool haveHeldNotes = heldNoteCount_ > 0;
        for (int index = 0; index < maxVoices; ++index)
        {
            const auto& voice = voices_[static_cast<std::size_t>(index)];
            const bool eligible = voice.active && voice.layer == 0
                && (!haveHeldNotes || voice.keyDown);
            if (eligible && (survivor < 0 || voice.generation > survivorGeneration))
            {
                survivor = index;
                survivorGeneration = voice.generation;
            }
        }

        for (int index = 0; index < maxVoices; ++index)
        {
            if (index != survivor && voices_[static_cast<std::size_t>(index)].active)
                silenceVoice(voices_[static_cast<std::size_t>(index)], true);
        }

        if (survivor >= 0)
        {
            auto& voice = voices_[static_cast<std::size_t>(survivor)];
            voice.layer = 0;
            voice.unisonCents = 0.0f;
            voice.panBase = 0.0f;
            voice.groupGain = 0.82f;
            if (haveHeldNotes)
            {
                const int note = heldNoteOrder_[static_cast<std::size_t>(heldNoteCount_ - 1)];
                retargetMonoVoice(voice, note,
                                  heldNoteVelocities_[static_cast<std::size_t>(note)]);
            }
        }
        updateActiveVoiceCount();
    }
    else if (previousMode == VoiceMode::Mono
             && targetParameters_.voiceMode != VoiceMode::Mono)
    {
        clearHeldNotes();
    }

    const int effectiveLimit = targetParameters_.voiceMode == VoiceMode::Mono
        ? 1 : targetParameters_.polyphonyLimit;
    enforcePolyphonyLimit(effectiveLimit);
}

std::uint32_t MarsEngine::hash32(std::uint32_t value) noexcept
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value == 0u ? 1u : value;
}

float MarsEngine::hashBipolar(std::uint32_t value) noexcept
{
    return static_cast<float>(hash32(value) & 0x00ffffffu) / 8388607.5f - 1.0f;
}

void MarsEngine::buildVoiceCards() noexcept
{
    for (int i = 0; i < maxVoices; ++i)
    {
        auto& card = cards_[static_cast<std::size_t>(i)];
        const std::uint32_t seed = 0x9e3779b9u * static_cast<std::uint32_t>(i + 1);
        card.osc1Cents = 3.2f * hashBipolar(seed + 1u);
        card.osc2Cents = 4.1f * hashBipolar(seed + 2u);
        card.cutoffError = hashBipolar(seed + 3u);
        card.resonanceError = hashBipolar(seed + 4u);
        card.envelopeError = hashBipolar(seed + 5u);
        card.driveError = hashBipolar(seed + 6u);
        card.panError = hashBipolar(seed + 7u);
        card.pulseSkew = hashBipolar(seed + 8u);
        card.driftRate = 0.031f + 0.091f * (0.5f + 0.5f * hashBipolar(seed + 9u));
        card.driftDepth = 0.72f + 0.56f * (0.5f + 0.5f * hashBipolar(seed + 10u));
        card.driftPhase = 0.5f + 0.5f * hashBipolar(seed + 11u);
        card.driftSlow = 0.18f * hashBipolar(seed + 12u);
        card.driftFast = 0.10f * hashBipolar(seed + 13u);
        card.driftNoiseState = hash32(seed + 14u);
    }
}

float MarsEngine::midiToHz(float midiNote) noexcept
{
    return 440.0f * std::exp2((midiNote - 69.0f) / 12.0f);
}

float MarsEngine::wrapPhase(float phase) noexcept
{
    return phase - std::floor(phase);
}

float MarsEngine::smoothStep(float value) noexcept
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

float MarsEngine::envelopeCoefficient(float seconds, float sampleRate) noexcept
{
    seconds = std::max(seconds, 0.0005f);
    return 1.0f - std::exp(-envelopeRange / (seconds * sampleRate));
}

float MarsEngine::polyBlep(float phase, float increment) noexcept
{
    increment = std::clamp(increment, 0.0000001f, 0.499f);
    if (phase < increment)
    {
        const float x = phase / increment;
        return x + x - x * x - 1.0f;
    }
    if (phase > 1.0f - increment)
    {
        const float x = (phase - 1.0f) / increment;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}

void MarsEngine::addFourthOrderBlep(std::array<float, 4>& correction,
                                    float step, float samplesToNext) noexcept
{
    const float d = std::clamp(samplesToNext, 0.0f, 1.0f);
    const float d2 = d * d;
    const float d3 = d2 * d;
    const float d4 = d2 * d2;
    const std::array<float, 4> residual {{
        d4 / 24.0f,
        -d4 / 8.0f + d3 / 6.0f + d2 / 4.0f + d / 6.0f + 1.0f / 24.0f,
        d4 / 8.0f - d3 / 3.0f + 2.0f * d / 3.0f - 0.5f,
        -d4 / 24.0f + d3 / 6.0f - d2 / 4.0f + d / 6.0f - 1.0f / 24.0f,
    }};
    for (std::size_t index = 0; index < correction.size(); ++index)
        correction[index] += step * residual[index];
}

float MarsEngine::processFourthOrderBlep(float input,
                                         std::array<float, 2>& delay,
                                         std::array<float, 4>& correction,
                                         bool& initialised) noexcept
{
    if (!initialised)
    {
        delay.fill(input);
        correction.fill(0.0f);
        initialised = true;
    }

    const float output = delay[0] + correction[0];
    delay[0] = delay[1];
    delay[1] = input;
    for (std::size_t index = 1; index < correction.size(); ++index)
        correction[index - 1] = correction[index];
    correction.back() = 0.0f;
    return std::isfinite(output) ? output : 0.0f;
}

int MarsEngine::waveformIndex(OscillatorWave waveform) noexcept
{
    switch (waveform)
    {
        case OscillatorWave::Saw:      return 0;
        case OscillatorWave::Pulse:    return 1;
        case OscillatorWave::Triangle: return 2;
    }
    return 0;
}

float MarsEngine::halfbandCoefficient(int tap) noexcept
{
    if (tap < 0 || tap >= halfbandTaps)
        return 0.0f;
    constexpr int centre = (halfbandTaps - 1) / 2;
    if (tap == centre)
        return 0.5f;
    if ((tap & 1) == 0)
        return 0.0f;
    const int mirrored = std::min(tap, halfbandTaps - 1 - tap);
    return halfbandSideCoefficients[static_cast<std::size_t>((mirrored - 1) / 2)];
}

float MarsEngine::softSaturate(float value) noexcept
{
    value = std::clamp(value, -3.0f, 3.0f);
    const float square = value * value;
    return value * (27.0f + square) / (27.0f + 9.0f * square);
}

float MarsEngine::softSaturateDerivative(float value) noexcept
{
    if (value <= -3.0f || value >= 3.0f)
        return 0.0f;
    const float square = value * value;
    const float numerator = 27.0f * value + value * square;
    const float denominator = 27.0f + 9.0f * square;
    const float numeratorDerivative = 27.0f + 3.0f * square;
    const float denominatorDerivative = 18.0f * value;
    return (numeratorDerivative * denominator - numerator * denominatorDerivative)
         / (denominator * denominator);
}

float MarsEngine::adaaShape(float value) noexcept
{
    // A smooth diode-like transfer whose antiderivative is cheap and stable.
    // The bounded input also protects the divided difference under hostile
    // automation or corrupt host state.
    value = std::clamp(value, -12.0f, 12.0f);
    return value / std::sqrt(1.0f + value * value);
}

float MarsEngine::adaaAntiderivative(float value) noexcept
{
    value = std::clamp(value, -12.0f, 12.0f);
    return std::sqrt(1.0f + value * value);
}

float MarsEngine::processAdaaMixer(float value, Voice& voice) noexcept
{
    value = std::clamp(finiteOr(value, 0.0f), -12.0f, 12.0f);
    if (!voice.mixerInitialised)
    {
        voice.previousMixerInput = value;
        voice.mixerInitialised = true;
        return adaaShape(value);
    }

    const float previous = voice.previousMixerInput;
    const float difference = value - previous;
    voice.previousMixerInput = value;

    // First-order ADAA. Around coincident samples the quotient loses precision,
    // so evaluate the underlying nonlinearity at the midpoint instead.
    float output = std::abs(difference) < 1.0e-4f * (1.0f + std::abs(value))
        ? adaaShape(0.5f * (value + previous))
        : (adaaAntiderivative(value) - adaaAntiderivative(previous)) / difference;
    if (!std::isfinite(output))
        output = adaaShape(value);
    return output;
}

float MarsEngine::filterInputVoltage(float value, float drive) noexcept
{
    value = std::clamp(finiteOr(value, 0.0f), -4.0f, 4.0f);
    drive = std::clamp(finiteOr(drive, 0.0f), 0.0f, 1.0f);

    // The D'Angelo-Valimaki circuit equations operate in volts and use a
    // transistor thermal voltage of 26 mV. This maps the panel's 0..1 drive
    // range to roughly -5 dB .. +13 dB around 2*VT, once and only once at the
    // filter input. Mixer and VCA transfer functions do not follow this knob.
    const float levelInThermalPairs = 0.55f * std::exp2(3.0f * drive);
    return value * twiceThermalVoltage * levelInThermalPairs;
}

int MarsEngine::layersForMode(const EngineParameters& parameters) const noexcept
{
    if (parameters.voiceMode == VoiceMode::Unison)
        return std::clamp(parameters.unisonVoices, 1, parameters.polyphonyLimit);
    if (parameters.voiceMode == VoiceMode::Fifth)
        return std::min(2, parameters.polyphonyLimit);
    return 1;
}

int MarsEngine::fifthIntervalForLayer(int layer) const noexcept
{
    return layer == 0 ? 0 : 7;
}

int MarsEngine::findFreeVoice() const noexcept
{
    for (int i = 0; i < maxVoices; ++i)
        if (!voices_[static_cast<std::size_t>(i)].active)
            return i;
    return -1;
}

std::uint64_t MarsEngine::selectStealGeneration(bool releasingOnly) const noexcept
{
    constexpr auto none = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t selected = none;
    double selectedScore = std::numeric_limits<double>::infinity();
    const auto attackProtectionSamples = static_cast<std::uint64_t>(
        std::max(1.0f, 0.015f * oversampledRate_));

    for (const auto& seedVoice : voices_)
    {
        if (!seedVoice.active)
            continue;
        const auto generation = seedVoice.generation;
        bool alreadyVisited = false;
        for (const auto& earlier : voices_)
        {
            if (&earlier == &seedVoice)
                break;
            if (earlier.active && earlier.generation == generation)
            {
                alreadyVisited = true;
                break;
            }
        }
        if (alreadyVisited)
            continue;

        bool groupReleasing = true;
        bool attackProtected = false;
        double energy = 0.0;
        for (const auto& voice : voices_)
        {
            if (!voice.active || voice.generation != generation)
                continue;
            groupReleasing = groupReleasing && voice.releasing;
            attackProtected = attackProtected || voice.ageSamples < attackProtectionSamples;
            energy += std::sqrt(std::max(0.0,
                                         static_cast<double>(voice.outputEnergy)))
                    + 0.12 * static_cast<double>(voice.ampEnvelope.value);
        }
        if (releasingOnly && !groupReleasing)
            continue;

        // A just-triggered articulation is protected from being immediately
        // erased before it becomes audible. Otherwise choose the quietest
        // complete group, with the oldest generation as a deterministic tie
        // breaker. Released tails are searched in a separate first pass.
        const double score = energy + (!groupReleasing && attackProtected ? 100.0 : 0.0);
        if (score < selectedScore - 1.0e-12
            || (std::abs(score - selectedScore) <= 1.0e-12
                && (selected == none || generation < selected)))
        {
            selected = generation;
            selectedScore = score;
        }
    }
    return selected;
}

void MarsEngine::makeRoomFor(int required, int voiceLimit) noexcept
{
    required = std::clamp(required, 1, maxVoices);
    voiceLimit = std::clamp(voiceLimit, required, maxVoices);
    updateActiveVoiceCount();

    while (activeVoiceCount_ + required > voiceLimit)
    {
        constexpr auto none = std::numeric_limits<std::uint64_t>::max();
        auto generation = selectStealGeneration(true);
        if (generation == none)
            generation = selectStealGeneration(false);
        if (generation == none)
            break;
        for (auto& voice : voices_)
        {
            if (voice.active && voice.generation == generation)
            {
                silenceVoice(voice, true);
                --activeVoiceCount_;
            }
        }
    }
}

void MarsEngine::enforcePolyphonyLimit(int voiceLimit) noexcept
{
    voiceLimit = std::clamp(voiceLimit, 1, maxVoices);
    updateActiveVoiceCount();
    while (activeVoiceCount_ > voiceLimit)
    {
        constexpr auto none = std::numeric_limits<std::uint64_t>::max();
        auto generation = selectStealGeneration(true);
        if (generation == none)
            generation = selectStealGeneration(false);
        if (generation == none)
            break;
        for (auto& voice : voices_)
        {
            if (voice.active && voice.generation == generation)
            {
                silenceVoice(voice, true);
                --activeVoiceCount_;
            }
        }
    }
    updateActiveVoiceCount();
}

int MarsEngine::findMonoVoice() const noexcept
{
    int candidate = -1;
    std::uint64_t newestGeneration = 0;
    for (int i = 0; i < maxVoices; ++i)
    {
        const auto& voice = voices_[static_cast<std::size_t>(i)];
        if (voice.active && (candidate < 0 || voice.generation > newestGeneration))
        {
            candidate = i;
            newestGeneration = voice.generation;
        }
    }
    return candidate;
}

void MarsEngine::rememberHeldNote(int midiNote, float velocity) noexcept
{
    midiNote = std::clamp(midiNote, 0, 127);
    const auto noteIndex = static_cast<std::size_t>(midiNote);

    // The engine is MIDI-omni, so overlapping note-ons of one pitch can come
    // from multiple channels or controllers. Keep a saturating per-pitch hold
    // count while storing each pitch only once in the priority stack.
    if (heldNoteCounts_[noteIndex] == 0)
    {
        heldNoteOrder_[static_cast<std::size_t>(heldNoteCount_++)] = midiNote;
    }
    else
    {
        for (int index = 0; index < heldNoteCount_; ++index)
        {
            if (heldNoteOrder_[static_cast<std::size_t>(index)] != midiNote)
                continue;
            for (int next = index + 1; next < heldNoteCount_; ++next)
                heldNoteOrder_[static_cast<std::size_t>(next - 1)]
                    = heldNoteOrder_[static_cast<std::size_t>(next)];
            heldNoteOrder_[static_cast<std::size_t>(heldNoteCount_ - 1)] = midiNote;
            break;
        }
    }

    if (heldNoteCounts_[noteIndex] < std::numeric_limits<std::uint16_t>::max())
        ++heldNoteCounts_[noteIndex];
    heldNoteVelocities_[noteIndex] = std::clamp(velocity, 0.0f, 1.0f);
}

void MarsEngine::forgetHeldNote(int midiNote) noexcept
{
    midiNote = std::clamp(midiNote, 0, 127);
    const auto noteIndex = static_cast<std::size_t>(midiNote);
    if (heldNoteCounts_[noteIndex] == 0)
        return;
    if (--heldNoteCounts_[noteIndex] != 0)
        return;

    for (int index = 0; index < heldNoteCount_; ++index)
    {
        if (heldNoteOrder_[static_cast<std::size_t>(index)] != midiNote)
            continue;
        for (int next = index + 1; next < heldNoteCount_; ++next)
            heldNoteOrder_[static_cast<std::size_t>(next - 1)]
                = heldNoteOrder_[static_cast<std::size_t>(next)];
        --heldNoteCount_;
        break;
    }
}

void MarsEngine::clearHeldNotes() noexcept
{
    heldNoteCounts_.fill(0);
    heldNoteVelocities_.fill(0.0f);
    heldNoteOrder_.fill(0);
    heldNoteCount_ = 0;
}

void MarsEngine::retargetMonoVoice(Voice& voice, int midiNote, float velocity) noexcept
{
    midiNote = std::clamp(midiNote, 0, 127);
    voice.rootMidi = midiNote;
    voice.soundingMidi = midiNote;
    voice.targetMidi = static_cast<float>(midiNote);
    // Preserve legato continuity: pitch retargets immediately according to the
    // glide law, while a differently struck key moves the VCA control over a
    // short analogue-style slew instead of amplitude-stepping.
    voice.targetVelocity = std::clamp(velocity, 0.0f, 1.0f);
    voice.keyDown = true;
    voice.sustained = false;
    voice.releasing = false;
}

void MarsEngine::initialiseVoice(Voice& voice, int slot, int rootMidi,
                                 int soundingMidi, int layer, int layerCount,
                                 float velocity,
                                 const EngineParameters& parameters) noexcept
{
    voice = Voice {};
    voice.active = true;
    voice.keyDown = true;
    voice.rootMidi = rootMidi;
    voice.soundingMidi = soundingMidi;
    voice.layer = layer;
    voice.cardIndex = slot;
    voice.generation = generation_;
    voice.velocity = velocity;
    voice.targetVelocity = velocity;
    voice.targetMidi = static_cast<float>(soundingMidi);
    const float interval = static_cast<float>(soundingMidi - rootMidi);
    voice.currentMidi = parameters.glideSeconds > 0.0f && hasLastPlayedMidi_
        ? std::clamp(lastPlayedMidi_ + interval, 0.0f, 127.0f)
        : voice.targetMidi;

    const float layerPosition = layerCount > 1
        ? 2.0f * static_cast<float>(layer) / static_cast<float>(layerCount - 1) - 1.0f
        : 0.0f;
    if (parameters.voiceMode == VoiceMode::Unison)
    {
        voice.unisonCents = layerPosition * (4.0f + 20.0f * parameters.drift);
        voice.panBase = 0.86f * layerPosition;
        voice.groupGain = 0.72f / std::sqrt(static_cast<float>(layerCount));
    }
    else if (parameters.voiceMode == VoiceMode::Fifth)
    {
        voice.unisonCents = 0.35f * layerPosition;
        voice.panBase = 0.76f * layerPosition;
        voice.groupGain = 0.57f / std::sqrt(static_cast<float>(layerCount));
    }
    else
    {
        voice.groupGain = 0.82f;
        voice.panBase = 0.0f;
    }

    const std::uint32_t seed = hash32(static_cast<std::uint32_t>(generation_)
                                    ^ static_cast<std::uint32_t>(rootMidi * 977 + layer * 131));
    voice.noiseState = seed;
    voice.oscillator1.phase = wrapPhase(0.17f + 0.37f * (0.5f + 0.5f * hashBipolar(seed + 1u)));
    voice.oscillator2.phase = wrapPhase(0.53f + 0.41f * (0.5f + 0.5f * hashBipolar(seed + 2u)));
    voice.oscillator1.dcoPhase = voice.oscillator1.phase;
    voice.oscillator2.dcoPhase = voice.oscillator2.phase;
    voice.subOscillator.phase = wrapPhase(voice.oscillator1.phase * 0.5f);
    // The leaky polyBLEP triangle stores the low-pass integrator state, whose
    // steady-state amplitude is approximately one quarter of the output.
    voice.oscillator1.triangle = 0.25f * triangleAtPhase(voice.oscillator1.phase);
    voice.oscillator2.triangle = 0.25f * triangleAtPhase(voice.oscillator2.phase);
    voice.subOscillator.triangle = 0.25f * triangleAtPhase(voice.subOscillator.phase);
    voice.driftPhase = wrapPhase(cards_[static_cast<std::size_t>(slot)].driftPhase
                               + 0.173f * static_cast<float>(layer));
    voice.ampEnvelope.noteOn();
    voice.filterEnvelope.noteOn();
    voice.controlCountdown = 0;
    updateVoiceControl(voice, parameters, 0.0f);
}

void MarsEngine::noteOn(int midiNote, float velocity)
{
    if (velocity <= 0.0f || !std::isfinite(velocity))
    {
        noteOff(midiNote);
        return;
    }
    if (!prepared_)
        prepare(sampleRate_, 512);

    midiNote = std::clamp(midiNote, 0, 127);
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    const EngineParameters parameters = targetParameters_;
    if (parameters.voiceMode == VoiceMode::Mono)
    {
        const bool wasLegato = heldNoteCount_ > 0;
        rememberHeldNote(midiNote, velocity);
        enforcePolyphonyLimit(1);
        const int monoSlot = findMonoVoice();
        if (wasLegato && monoSlot >= 0)
        {
            retargetMonoVoice(voices_[static_cast<std::size_t>(monoSlot)], midiNote, velocity);
        }
        else
        {
            if (monoSlot >= 0)
                silenceVoice(voices_[static_cast<std::size_t>(monoSlot)], true);
            ++generation_;
            const int slot = findFreeVoice();
            if (slot >= 0)
                initialiseVoice(voices_[static_cast<std::size_t>(slot)], slot, midiNote,
                                midiNote, 0, 1, velocity, parameters);
        }
        lastPlayedMidi_ = static_cast<float>(midiNote);
        hasLastPlayedMidi_ = true;
        updateActiveVoiceCount();
        return;
    }

    const int layerCount = layersForMode(parameters);

    // Repeated MIDI notes retrigger as one physical key action.
    for (auto& voice : voices_)
        if (voice.active && voice.rootMidi == midiNote)
            silenceVoice(voice, true);

    makeRoomFor(layerCount, parameters.polyphonyLimit);
    ++generation_;
    for (int layer = 0; layer < layerCount; ++layer)
    {
        const int slot = findFreeVoice();
        if (slot < 0)
            break;
        const int interval = parameters.voiceMode == VoiceMode::Fifth
            ? fifthIntervalForLayer(layer) : 0;
        const int soundingMidi = std::clamp(midiNote + interval, 0, 127);
        initialiseVoice(voices_[static_cast<std::size_t>(slot)], slot, midiNote,
                        soundingMidi, layer, layerCount, velocity, parameters);
    }
    lastPlayedMidi_ = static_cast<float>(midiNote);
    hasLastPlayedMidi_ = true;
    updateActiveVoiceCount();
}

void MarsEngine::noteOff(int midiNote)
{
    midiNote = std::clamp(midiNote, 0, 127);
    if (targetParameters_.voiceMode == VoiceMode::Mono)
    {
        forgetHeldNote(midiNote);
        const int monoSlot = findMonoVoice();
        if (monoSlot < 0)
            return;

        auto& voice = voices_[static_cast<std::size_t>(monoSlot)];
        if (voice.rootMidi != midiNote)
            return;

        if (heldNoteCount_ > 0)
        {
            const int fallback = heldNoteOrder_[static_cast<std::size_t>(heldNoteCount_ - 1)];
            retargetMonoVoice(voice, fallback,
                              heldNoteVelocities_[static_cast<std::size_t>(fallback)]);
        }
        else
        {
            voice.keyDown = false;
            if (sustainPedalDown_)
                voice.sustained = true;
            else
            {
                voice.releasing = true;
                voice.ampEnvelope.noteOff();
                voice.filterEnvelope.noteOff();
            }
        }
        return;
    }

    for (auto& voice : voices_)
    {
        if (voice.active && voice.rootMidi == midiNote)
        {
            voice.keyDown = false;
            if (sustainPedalDown_)
            {
                voice.sustained = true;
            }
            else
            {
                voice.releasing = true;
                voice.ampEnvelope.noteOff();
                voice.filterEnvelope.noteOff();
            }
        }
    }
}

void MarsEngine::allNotesOff()
{
    clearHeldNotes();
    for (auto& voice : voices_)
    {
        if (!voice.active)
            continue;

        const bool wasHeld = voice.keyDown;
        voice.keyDown = false;
        if (sustainPedalDown_ && wasHeld && !voice.releasing)
        {
            voice.sustained = true;
            continue;
        }

        if (!sustainPedalDown_)
        {
            voice.sustained = false;
            voice.releasing = true;
            voice.ampEnvelope.noteOff();
            voice.filterEnvelope.noteOff();
        }
    }
}

void MarsEngine::setPitchBend(float normalisedBipolar) noexcept
{
    pitchBendTarget_ = std::clamp(finiteOr(normalisedBipolar, 0.0f), -1.0f, 1.0f);
}

void MarsEngine::setModWheel(float amount) noexcept
{
    modWheelTarget_ = std::clamp(finiteOr(amount, 0.0f), 0.0f, 1.0f);
}

void MarsEngine::setSustainPedal(bool down) noexcept
{
    if (sustainPedalDown_ == down)
        return;

    sustainPedalDown_ = down;
    if (down)
        return;

    for (auto& voice : voices_)
    {
        if (voice.active && voice.sustained && !voice.keyDown)
        {
            voice.sustained = false;
            voice.releasing = true;
            voice.ampEnvelope.noteOff();
            voice.filterEnvelope.noteOff();
        }
    }
}

void MarsEngine::addVoiceToStealTail(const Voice& voice) noexcept
{
    if (!voice.active || !std::isfinite(voice.lastOutput))
        return;

    constexpr float maximumTail = 8.0f;
    stealTailLeft_ = std::clamp(stealTailLeft_ + voice.lastOutput * voice.panLeft,
                                -maximumTail, maximumTail);
    stealTailRight_ = std::clamp(stealTailRight_ + voice.lastOutput * voice.panRight,
                                 -maximumTail, maximumTail);
}

void MarsEngine::renderStealTail(float& left, float& right) noexcept
{
    left = stealTailLeft_;
    right = stealTailRight_;
    stealTailLeft_ *= stealTailCoefficient_;
    stealTailRight_ *= stealTailCoefficient_;
    if (std::abs(stealTailLeft_) < 1.0e-7f)
        stealTailLeft_ = 0.0f;
    if (std::abs(stealTailRight_) < 1.0e-7f)
        stealTailRight_ = 0.0f;
}

void MarsEngine::silenceVoice(Voice& voice, bool preserveTail) noexcept
{
    if (preserveTail)
        addVoiceToStealTail(voice);
    voice.active = false;
    voice.releasing = false;
    voice.keyDown = false;
    voice.sustained = false;
    voice.ampEnvelope.reset();
    voice.filterEnvelope.reset();
    voice.ladder.reset();
    voice.stateVariable.reset();
}

void MarsEngine::updateActiveVoiceCount() noexcept
{
    int count = 0;
    for (const auto& voice : voices_)
        count += voice.active ? 1 : 0;
    activeVoiceCount_ = count;
}

float MarsEngine::nextNoise(Voice& voice) noexcept
{
    std::uint32_t value = voice.noiseState;
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    voice.noiseState = value == 0u ? 1u : value;
    return static_cast<float>(voice.noiseState & 0x00ffffffu) / 8388607.5f - 1.0f;
}

float MarsEngine::nextLfoValue(const EngineParameters& parameters) noexcept
{
    // The LFO lives inside the HQ island so PWM and pitch modulation do not
    // become a host-rate staircase before a 2x/4x oscillator render.
    lfoPhase_ += parameters.lfoRateHz / oversampledRate_;
    if (lfoPhase_ >= 1.0f)
    {
        lfoPhase_ -= std::floor(lfoPhase_);
        globalNoiseState_ = hash32(globalNoiseState_ + 0x9e3779b9u);
        randomLfoValue_ = hashBipolar(globalNoiseState_);
    }

    if (parameters.lfoWave == LfoWaveform::Sine)
        return std::sin(twoPi * lfoPhase_);
    if (parameters.lfoWave == LfoWaveform::SampleHold)
        return randomLfoValue_;
    return 1.0f - 4.0f * std::abs(lfoPhase_ - 0.5f);
}

void MarsEngine::updateVoiceCardDrift(VoiceCard& card) noexcept
{
    // Physical card temperature and oscillator noise keep evolving between
    // notes. Updating every card from one global control clock prevents drift
    // from freezing during silence or advancing at note-allocation time.
    const auto nextDriftNoise = [&card]() noexcept
    {
        std::uint32_t value = card.driftNoiseState;
        value ^= value << 13u;
        value ^= value >> 17u;
        value ^= value << 5u;
        card.driftNoiseState = value == 0u ? 1u : value;
        return static_cast<float>(card.driftNoiseState & 0x00ffffffu)
             / 8388607.5f - 1.0f;
    };
    const float controlInterval = static_cast<float>(controlPeriod) / oversampledRate_;
    const float slowRho = std::exp(-controlInterval / 2.8f);
    const float fastRho = std::exp(-controlInterval / 0.075f);
    const float slowExcitation = std::sqrt(std::max(
        0.0f, 3.0f * (1.0f - slowRho * slowRho)));
    const float fastExcitation = std::sqrt(std::max(
        0.0f, 3.0f * (1.0f - fastRho * fastRho)));
    card.driftSlow = slowRho * card.driftSlow
                   + slowExcitation * nextDriftNoise();
    card.driftFast = fastRho * card.driftFast
                   + fastExcitation * nextDriftNoise();
    card.driftSlow = std::clamp(card.driftSlow, -3.5f, 3.5f);
    card.driftFast = std::clamp(card.driftFast, -3.5f, 3.5f);
}

void MarsEngine::updateVoiceControl(Voice& voice,
                                    const EngineParameters& parameters,
                                    float lfoValue) noexcept
{
    auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float ageScale = 0.25f + 0.75f * parameters.drift;
    const float voiceAge = static_cast<float>(voice.ageSamples) / oversampledRate_;
    const float lfoFade = smoothStep(voiceAge / 0.14f);

    // Two bounded Ornstein-Uhlenbeck states model slow thermal motion and a
    // lower-amplitude fast component. They are advanced globally above, so
    // this note reads a continuously free-running card history.
    const float driftValue = 0.84f * card.driftSlow + 0.16f * card.driftFast;
    voice.driftPhase = wrapPhase(
        voice.driftPhase + card.driftRate * static_cast<float>(controlPeriod) / oversampledRate_);

    if (parameters.glideSeconds <= 0.0005f)
    {
        voice.currentMidi = voice.targetMidi;
    }
    else
    {
        const float glideCoefficient = 1.0f
                                       - std::exp(-envelopeRange * static_cast<float>(controlPeriod)
                                                  / (parameters.glideSeconds * oversampledRate_));
        voice.currentMidi += glideCoefficient * (voice.targetMidi - voice.currentMidi);
    }

    const float wanderCents = parameters.drift * card.driftDepth * (2.25f * driftValue);
    const float lfoCents = (parameters.lfoPitchCents + 40.0f * modWheel_) * lfoFade * lfoValue;
    // A free-running VCO inherits component calibration error and slow thermal
    // wander. A DCO still has an analogue waveform path, but its fundamental is
    // timed by a shared digital clock, so per-card analogue pitch error does
    // not enter the timer count. Intentional unison detune, panel tuning, bend,
    // and LFO modulation remain identical between models.
    const float oscillator1AnalogueCents = parameters.osc1Model == OscillatorModel::Vco
                                               ? ageScale * card.osc1Cents + wanderCents
                                               : 0.0f;
    const float oscillator2AnalogueCents = parameters.osc2Model == OscillatorModel::Vco
                                               ? ageScale * card.osc2Cents - 0.72f * wanderCents
                                               : 0.0f;
    const float oscillator1Cents = voice.unisonCents + oscillator1AnalogueCents + lfoCents;
    const float oscillator2Cents =
        -0.37f * voice.unisonCents + parameters.osc2FineCents + oscillator2AnalogueCents + lfoCents;
    const float bendSemitones = 2.0f * pitchBend_;
    const float oscillator1Note = voice.currentMidi + static_cast<float>(12 * parameters.osc1Octave)
                                  + bendSemitones + oscillator1Cents / 100.0f;
    const float oscillator2Note =
        voice.currentMidi
        + static_cast<float>(12 * parameters.osc2Octave + parameters.osc2Semitones) + bendSemitones
        + oscillator2Cents / 100.0f;
    const float oscillator1Target =
        std::clamp(midiToHz(oscillator1Note) / oversampledRate_, 0.0000001f, 0.45f);
    const float oscillator2Target =
        std::clamp(midiToHz(oscillator2Note) / oversampledRate_, 0.0000001f, 0.45f);
    const float subTarget = std::max(0.0000001f, 0.5f * oscillator1Target);

    const float keyOctaves = parameters.filterKeyTrack * (voice.currentMidi - 60.0f) / 12.0f;
    const float envelopeOctaves = 5.4f * parameters.filterEnvAmount * voice.filterEnvelope.value;
    const float lfoOctaves = (parameters.lfoFilterOctaves + 0.5f * modWheel_) * lfoFade * lfoValue;
    const float componentOctaves =
        ageScale * (0.075f * card.cutoffError) + parameters.drift * 0.028f * driftValue;
    const float maximumCutoff = 0.45f * static_cast<float>(sampleRate_);
    const float cutoff =
        std::clamp(parameters.cutoffHz
                       * std::exp2(keyOctaves + envelopeOctaves + lfoOctaves + componentOctaves),
                   18.0f,
                   maximumCutoff);
    const float stateTarget = std::tan(pi * cutoff / oversampledRate_);
    const float ladderTarget = stateTarget;

    const float panMotion =
        parameters.voiceMode == VoiceMode::Fifth
            ? 0.16f * std::sin(twoPi * (voice.driftPhase + 0.19f * static_cast<float>(voice.layer)))
            : 0.0f;
    const float pan = std::clamp(
        parameters.spread * (voice.panBase + panMotion + 0.12f * ageScale * card.panError),
        -1.0f,
        1.0f);
    const float targetLeft = std::sqrt(0.5f * (1.0f - pan));
    const float targetRight = std::sqrt(0.5f * (1.0f + pan));

    if (!voice.controlInitialised)
    {
        voice.oscillator1Increment = oscillator1Target;
        voice.oscillator2Increment = oscillator2Target;
        voice.subIncrement = subTarget;
        voice.ladderGain = ladderTarget;
        voice.stateGain = stateTarget;
        voice.panLeft = targetLeft;
        voice.panRight = targetRight;
        voice.controlInitialised = true;
    }

    const float period = static_cast<float>(controlPeriod);
    voice.oscillator1Step = (oscillator1Target - voice.oscillator1Increment) / period;
    voice.oscillator2Step = (oscillator2Target - voice.oscillator2Increment) / period;
    voice.subStep = (subTarget - voice.subIncrement) / period;
    voice.ladderGainStep = (ladderTarget - voice.ladderGain) / period;
    voice.stateGainStep = (stateTarget - voice.stateGain) / period;
    voice.panLeftStep = (targetLeft - voice.panLeft) / period;
    voice.panRightStep = (targetRight - voice.panRight) / period;
    voice.resonance =
        std::clamp(parameters.resonance + 0.045f * ageScale * card.resonanceError, 0.0f, 0.995f);
    // D'Angelo-Valimaki resonance compensation is constant between control
    // updates. Hoisting its square roots out of every oversampled ladder step
    // materially reduces the 16-voice worst-case cost without changing the
    // circuit equations.
    constexpr float cosPiOverFour = 0.7071067811865475f;
    voice.ladderFeedbackGain = 4.0f * voice.resonance;
    const float fourthRootK = std::sqrt(std::sqrt(voice.ladderFeedbackGain));
    const float alphaSquared =
        1.0f + std::sqrt(voice.ladderFeedbackGain) - 2.0f * fourthRootK * cosPiOverFour;
    voice.ladderFrequencyScale = 1.0f / std::sqrt(std::max(alphaSquared, 1.0e-8f));
    voice.drive = std::clamp(
        parameters.filterDrive * (1.0f + 0.16f * ageScale * card.driveError), 0.0f, 1.0f);

    const float envelopeScale =
        std::clamp(1.0f + 0.14f * ageScale * card.envelopeError, 0.72f, 1.28f);
    voice.ampAttackCoefficient =
        envelopeCoefficient(parameters.ampAttack * envelopeScale, oversampledRate_);
    voice.ampDecayCoefficient =
        envelopeCoefficient(parameters.ampDecay * envelopeScale, oversampledRate_);
    voice.ampReleaseCoefficient =
        envelopeCoefficient(parameters.ampRelease * envelopeScale, oversampledRate_);
    voice.filterAttackCoefficient =
        envelopeCoefficient(parameters.filterAttack * envelopeScale, oversampledRate_);
    voice.filterDecayCoefficient =
        envelopeCoefficient(parameters.filterDecay * envelopeScale, oversampledRate_);
    voice.filterReleaseCoefficient =
        envelopeCoefficient(parameters.filterRelease * envelopeScale, oversampledRate_);
}

float MarsEngine::renderOscillator(Oscillator &oscillator,
                                   OscillatorWave waveform,
                                   OscillatorModel model,
                                   float increment,
                                   float pulseWidth,
                                   float dcoRangeClockHz,
                                   bool &wrapped) noexcept
{
    increment = std::clamp(increment, 0.0000001f, 0.45f);
    pulseWidth = std::clamp(pulseWidth, 0.05f, 0.95f);
    dcoRangeClockHz = std::clamp(finiteOr(dcoRangeClockHz, 2000000.0f), 500000.0f, 8000000.0f);

    const int requestedWave = waveformIndex(waveform);
    if (!oscillator.waveformInitialised)
    {
        oscillator.activeWave = waveform;
        oscillator.waveformBlend.fill(0.0f);
        oscillator.waveformBlend[static_cast<std::size_t>(requestedWave)] = 1.0f;
        oscillator.waveformInitialised = true;
    }
    else if (oscillator.activeWave != waveform)
    {
        oscillator.activeWave = waveform;
        oscillator.waveformCrossfadeRemaining = waveformCrossfadeSamples_;
        for (std::size_t index = 0; index < oscillator.waveformBlend.size(); ++index)
        {
            const float target = static_cast<int>(index) == requestedWave ? 1.0f : 0.0f;
            oscillator.waveformBlendStep[index] = (target - oscillator.waveformBlend[index])
                                                  / static_cast<float>(waveformCrossfadeSamples_);
        }
    }

    if (!oscillator.modelInitialised)
    {
        oscillator.activeModel = model;
        oscillator.modelBlend = model == OscillatorModel::Dco ? 1.0f : 0.0f;
        if (model == OscillatorModel::Dco)
            oscillator.dcoPhase = oscillator.phase;
        oscillator.modelInitialised = true;
    }
    else if (oscillator.activeModel != model)
    {
        const bool wasCrossfading = oscillator.modelCrossfadeRemaining > 0;
        oscillator.activeModel = model;
        oscillator.modelCrossfadeRemaining = oscillatorModelCrossfadeSamples_;
        const float targetBlend = model == OscillatorModel::Dco ? 1.0f : 0.0f;
        oscillator.modelBlendStep = (targetBlend - oscillator.modelBlend)
                                    / static_cast<float>(oscillatorModelCrossfadeSamples_);
        if (!wasCrossfading && model == OscillatorModel::Dco)
        {
            // The inactive analogue endpoint is frozen at the settled VCO
            // endpoint. Seed it analytically from the live cycle when the
            // crossfade begins instead of paying its full cost continuously.
            oscillator.dcoPhase = oscillator.phase;
            oscillator.dcoClockInitialised = false;
            oscillator.dcoAnalogueInitialised = false;
            oscillator.dcoControlCountdown = 0;
        }
        else if (!wasCrossfading && model == OscillatorModel::Vco)
        {
            oscillator.phase = oscillator.dcoPhase;
            oscillator.triangle = 0.25f * triangleAtPhase(oscillator.phase);
            oscillator.sawContourInitialised = false;
            oscillator.vcoSawBlepInitialised = false;
            oscillator.pulseBlepInitialised = false;
            oscillator.expectedPulseInitialised = false;
            oscillator.triangleSquareBlepInitialised = false;
        }
    }

    // Roland's pitch path is an 8 MHz master divided to a range clock, an
    // 8253-style integer interval timer, then the MC5534A analogue wave
    // generator. Control writes update a pending count at the documented
    // multiplexed-control cadence; the active count remains untouched until
    // terminal count. There is deliberately no adjacent-count dithering.
    const auto writeDcoControl =
        [this, &oscillator, dcoRangeClockHz, pulseWidth](float desiredIncrement) noexcept
    {
        const double desiredFrequency =
            std::max(0.001, static_cast<double>(desiredIncrement) * oversampledRate_);
        // Counts above the physical 16-bit envelope are a documented Mars
        // prescaler extension for MIDI notes below the original keyboard
        // range; the hardware-accurate region still uses ordinary 2..65536
        // counts and the exact 1/2/4 MHz range clocks.
        constexpr double maximumExtendedCount = 1048576.0;
        const double idealTicks = std::clamp(
            static_cast<double>(dcoRangeClockHz) / desiredFrequency, 2.0, maximumExtendedCount);
        oscillator.dcoPendingDivisor = static_cast<std::uint32_t>(std::llround(idealTicks));
        oscillator.dcoPendingRangeClockHz = dcoRangeClockHz;
        oscillator.dcoHeldSlopeVoltsPerSecond = 12.0f * static_cast<float>(desiredFrequency);
        oscillator.dcoHeldPulseWidth = pulseWidth;
        oscillator.dcoControlCountdown = dcoControlPeriodSamples_;
    };

    const auto latchDcoTimer = [this, &oscillator]() noexcept
    {
        const bool rangeChanged = !oscillator.dcoClockInitialised
                                  || std::abs(oscillator.dcoRangeClockHz
                                              - oscillator.dcoPendingRangeClockHz) > 0.5f;
        oscillator.dcoTimerDivisor = std::max(2u, oscillator.dcoPendingDivisor);
        oscillator.dcoRangeClockHz = oscillator.dcoPendingRangeClockHz;
        oscillator.dcoIncrement =
            std::clamp(oscillator.dcoRangeClockHz
                           / (static_cast<float>(oscillator.dcoTimerDivisor) * oversampledRate_),
                       0.0000001f,
                       0.45f);
        if (rangeChanged)
        {
            constexpr float resetTimeConstantSeconds = 45.0e-9f;
            // The 8 MHz outer octave is a Mars timer extension; keep the
            // proprietary analogue reset drive within the documented 4 MHz
            // MC5534 range so every legal PWM threshold still resets high.
            const float analogueResetClock =
                std::clamp(oscillator.dcoRangeClockHz, 1.0f, 4000000.0f);
            const float resetDuration = 1.0f / analogueResetClock;
            oscillator.dcoResetResidue =
                std::exp(-resetDuration / resetTimeConstantSeconds);
            oscillator.dcoChargeInjectionVolts =
                0.012f
                + 0.004f
                      * std::clamp(oscillator.dcoRangeClockHz / 4000000.0f, 0.0f, 1.0f);
        }
        oscillator.dcoClockInitialised = true;
    };

    const bool needsDco = model == OscillatorModel::Dco || oscillator.modelBlend > 0.0f
                          || oscillator.modelCrossfadeRemaining > 0;
    if (needsDco)
    {
        // Range, pitch, and PWM are sampled together by the multiplexed
        // control scan. Do not combine a just-automated range clock with the
        // previous control interval's frequency target.
        if (!oscillator.dcoClockInitialised || --oscillator.dcoControlCountdown <= 0)
            writeDcoControl(increment);
        if (!oscillator.dcoClockInitialised)
            latchDcoTimer();
    }

    const bool needsVco = model == OscillatorModel::Vco || oscillator.modelBlend < 1.0f
                          || oscillator.modelCrossfadeRemaining > 0;
    const float vcoPhase = oscillator.phase;
    const float dcoPhase = oscillator.dcoPhase;
    std::array<float, 3> vcoOutputs{};
    if (needsVco)
    {
        // Keep every VCO waveform generator warm while the VCO endpoint is
        // audible. The same causal integrated
        // third-order B-spline correction now bandlimits the saw reset, both pulse
        // edges, and the square driver integrated into the triangle. This matters
        // most under PWM and cross-mod, where the former compact two-sample BLEP
        // was the weakest oscillator path. Warm parallel states also make preset
        // randomisation and host waveform automation crossfade without a click.
        const float rawSaw = 2.0f * vcoPhase - 1.0f;
        const float rawPulse = vcoPhase < pulseWidth ? 1.0f : -1.0f;
        const float rawTriangleSquare = vcoPhase < 0.5f ? 1.0f : -1.0f;
        if (!oscillator.expectedPulseInitialised)
        {
            oscillator.expectedPulseAtNextSample = rawPulse;
            oscillator.expectedPulseInitialised = true;
        }
        else if (rawPulse != oscillator.expectedPulseAtNextSample)
        {
            // A moving PWM comparator can cross the already-advanced phase even
            // when the previous sample's constant-width prediction saw no edge.
            // Schedule that threshold-motion step at the sample boundary; the
            // two-sample causal B-spline delay leaves the correction on time.
            addFourthOrderBlep(
                oscillator.pulseCorrection, rawPulse - oscillator.expectedPulseAtNextSample, 0.0f);
        }
        const float bandlimitedSaw = processFourthOrderBlep(rawSaw,
                                                            oscillator.vcoSawDelay,
                                                            oscillator.vcoSawCorrection,
                                                            oscillator.vcoSawBlepInitialised);
        const float bandlimitedPulse = processFourthOrderBlep(rawPulse,
                                                              oscillator.pulseDelay,
                                                              oscillator.pulseCorrection,
                                                              oscillator.pulseBlepInitialised);
        const float bandlimitedTriangleSquare =
            processFourthOrderBlep(rawTriangleSquare,
                                   oscillator.triangleSquareDelay,
                                   oscillator.triangleSquareCorrection,
                                   oscillator.triangleSquareBlepInitialised);

        oscillator.triangle = increment * bandlimitedTriangleSquare
                              + (1.0f - increment) * oscillator.triangle;
        oscillator.triangle = std::clamp(finiteOr(oscillator.triangle, 0.0f), -0.35f, 0.35f);
        vcoOutputs = {{
            bandlimitedSaw,
            bandlimitedPulse,
            4.0f * oscillator.triangle,
        }};

        // Pekonen et al.'s source-specific Table 4(b) fit matches this exact
        // fourth-order B-spline BLEP to recorded Minimoog Voyager spectra. Keep it
        // warm even while another waveform is selected, so a switch has no stale
        // post-EQ state.
        const float oscillatorFrequency = increment * oversampledRate_;
        const float fittedFrequency = std::clamp(oscillatorFrequency, 86.0f, 8300.0f);
        const float frequencySquared = fittedFrequency * fittedFrequency;
        const float referenceGain = 0.7105f + 3.380e-5f * fittedFrequency;
        const float referenceZero =
            1.0161f - 5.850e-4f * fittedFrequency + 5.220e-8f * frequencySquared;
        const float referencePole =
            1.0294f - 4.8921e-4f * fittedFrequency + 3.974e-8f * frequencySquared;
        constexpr float referenceRate = 44100.0f;
        const float rateRatio = referenceRate / oversampledRate_;
        const auto remapCoefficient = [rateRatio](float coefficient) noexcept
        {
            return ((1.0f + rateRatio) * coefficient + (1.0f - rateRatio))
                   / ((1.0f - rateRatio) * coefficient + (1.0f + rateRatio));
        };
        const float zero = remapCoefficient(referenceZero);
        const float pole = remapCoefficient(referencePole);
        const float referenceDcGain =
            referenceGain * (1.0f - referenceZero) / (1.0f - referencePole);
        const float gain = referenceDcGain * (1.0f - pole) / (1.0f - zero);
        if (!oscillator.sawContourInitialised)
        {
            oscillator.previousSawInput = bandlimitedSaw;
            oscillator.previousSawOutput = bandlimitedSaw;
            oscillator.sawContourInitialised = true;
        }
        else
        {
            const float contoured = gain * (bandlimitedSaw - zero * oscillator.previousSawInput)
                                    + pole * oscillator.previousSawOutput;
            oscillator.previousSawInput = bandlimitedSaw;
            oscillator.previousSawOutput = std::isfinite(contoured) ? contoured : bandlimitedSaw;
            const float contourBlend = smoothStep((oscillatorFrequency - 43.0f) / 43.0f);
            vcoOutputs[0] =
                bandlimitedSaw + contourBlend * (oscillator.previousSawOutput - bandlimitedSaw);
        }
    }

    const auto normaliseDcoSaw = [](float rampVolts) noexcept
    {
        return std::clamp(rampVolts / 6.0f - 1.0f, -1.12f, 1.12f);
    };
    std::array<float, 3> dcoOutputs{};
    if (needsDco)
    {
        // MC5534A: a resettable Miller integrator makes the saw and a
        // comparator derives PWM from the held control voltage. Work in the
        // documented 12 V ramp domain, then normalise only at the output.
        // Reset events are still corrected at their exact fractional time by
        // the existing causal integrated B-spline kernel.
        if (!oscillator.dcoAnalogueInitialised)
        {
            oscillator.dcoRampVolts = 12.0f * dcoPhase;
            const float thresholdVolts = 12.0f * oscillator.dcoHeldPulseWidth;
            oscillator.dcoPulseSlew = oscillator.dcoRampVolts < thresholdVolts ? 1.0f : -1.0f;
            oscillator.dcoSawDelay.fill(0.0f);
            oscillator.dcoSawCorrection.fill(0.0f);
            oscillator.dcoPulseDelay.fill(0.0f);
            oscillator.dcoPulseCorrection.fill(0.0f);
            oscillator.dcoTriangleSquareDelay.fill(0.0f);
            oscillator.dcoTriangleSquareCorrection.fill(0.0f);
            oscillator.dcoTriangle = 0.25f * triangleAtPhase(dcoPhase);
            oscillator.dcoSawBlepInitialised = false;
            oscillator.dcoPulseBlepInitialised = false;
            oscillator.dcoTriangleSquareBlepInitialised = false;
            oscillator.dcoExpectedPulseInitialised = false;
            oscillator.dcoReconstructionInitialised.fill(false);
            oscillator.dcoAnalogueInitialised = true;
        }

        const float rawDcoSaw = normaliseDcoSaw(oscillator.dcoRampVolts);
        const float dcoThresholdVolts = 12.0f * oscillator.dcoHeldPulseWidth;
        const float rawDcoPulse = oscillator.dcoRampVolts < dcoThresholdVolts ? 1.0f : -1.0f;
        if (!oscillator.dcoExpectedPulseInitialised)
        {
            oscillator.dcoExpectedPulseAtNextSample = rawDcoPulse;
            oscillator.dcoExpectedPulseInitialised = true;
        }
        else if (rawDcoPulse != oscillator.dcoExpectedPulseAtNextSample)
        {
            // The PWM CV is sample-and-held at the control scan. If its new
            // threshold crosses the already-running ramp, the comparator step
            // occurs at that control boundary rather than being missed.
            addFourthOrderBlep(oscillator.dcoPulseCorrection,
                               rawDcoPulse - oscillator.dcoExpectedPulseAtNextSample,
                               0.0f);
        }

        const float dcoSaw = processFourthOrderBlep(rawDcoSaw,
                                                    oscillator.dcoSawDelay,
                                                    oscillator.dcoSawCorrection,
                                                    oscillator.dcoSawBlepInitialised);
        const float dcoPulse = processFourthOrderBlep(rawDcoPulse,
                                                      oscillator.dcoPulseDelay,
                                                      oscillator.dcoPulseCorrection,
                                                      oscillator.dcoPulseBlepInitialised);
        const float rawDcoTriangleSquare = dcoPhase < 0.5f ? 1.0f : -1.0f;
        const float dcoTriangleSquare =
            processFourthOrderBlep(rawDcoTriangleSquare,
                                   oscillator.dcoTriangleSquareDelay,
                                   oscillator.dcoTriangleSquareCorrection,
                                   oscillator.dcoTriangleSquareBlepInitialised);
        oscillator.dcoTriangle = oscillator.dcoIncrement * dcoTriangleSquare
                                 + (1.0f - oscillator.dcoIncrement) * oscillator.dcoTriangle;
        oscillator.dcoTriangle =
            std::clamp(finiteOr(oscillator.dcoTriangle, 0.0f), -0.35f, 0.35f);
        const float pulseSlew = dcoPulse > oscillator.dcoPulseSlew ? dcoPulseRise_ : dcoPulseFall_;
        oscillator.dcoPulseSlew += pulseSlew * (dcoPulse - oscillator.dcoPulseSlew);

        const std::array<float, 3> dcoInputs{{
            dcoSaw,
            oscillator.dcoPulseSlew,
            // The MC5534A has no triangle output. Retaining it here is an
            // explicitly documented Mars extension through the same output
            // bandwidth, not a hardware claim.
            4.0f * oscillator.dcoTriangle,
        }};
        for (std::size_t index = 0; index < dcoOutputs.size(); ++index)
        {
            if (!oscillator.dcoReconstructionInitialised[index])
            {
                oscillator.dcoReconstruction[index] = dcoInputs[index];
                oscillator.dcoReconstructionInitialised[index] = true;
                dcoOutputs[index] = dcoInputs[index];
            }
            else
            {
                const float v = dcoReconstructionGain_
                                * (dcoInputs[index] - oscillator.dcoReconstruction[index]);
                dcoOutputs[index] = v + oscillator.dcoReconstruction[index];
                oscillator.dcoReconstruction[index] = dcoOutputs[index] + v;
            }
        }
    }

    float output = 0.0f;
    for (std::size_t index = 0; index < oscillator.waveformBlend.size(); ++index)
    {
        const float modelOutput =
            vcoOutputs[index] + oscillator.modelBlend * (dcoOutputs[index] - vcoOutputs[index]);
        output += oscillator.waveformBlend[index] * modelOutput;
    }
    if (!std::isfinite(output))
        output = 0.0f;

    // Each endpoint owns its timing. During a model change both continue at
    // their physical rates while only audio is crossfaded; once settled, the
    // inactive endpoint and its correction/filter state freeze.
    bool vcoWrapped = false;
    if (needsVco)
    {
        const float unwrappedVcoPhase = vcoPhase + increment;
        oscillator.phase = unwrappedVcoPhase;
        vcoWrapped = unwrappedVcoPhase >= 1.0f;

        // With increment < 0.5 there can be one cycle reset plus, for a
        // narrow pulse, its second edge in the new cycle. Corrections add.
        if (vcoPhase < pulseWidth && unwrappedVcoPhase >= pulseWidth)
            addFourthOrderBlep(
                oscillator.pulseCorrection, -2.0f, (unwrappedVcoPhase - pulseWidth) / increment);
        if (unwrappedVcoPhase >= 1.0f + pulseWidth)
            addFourthOrderBlep(oscillator.pulseCorrection,
                               -2.0f,
                               (unwrappedVcoPhase - (1.0f + pulseWidth)) / increment);
        if (vcoPhase < 0.5f && unwrappedVcoPhase >= 0.5f)
            addFourthOrderBlep(oscillator.triangleSquareCorrection,
                               -2.0f,
                               (unwrappedVcoPhase - 0.5f) / increment);

        if (vcoWrapped)
        {
            oscillator.phase -= std::floor(oscillator.phase);
            const float samplesToNext = oscillator.phase / increment;
            addFourthOrderBlep(oscillator.vcoSawCorrection, -2.0f, samplesToNext);
            addFourthOrderBlep(oscillator.pulseCorrection, 2.0f, samplesToNext);
            addFourthOrderBlep(oscillator.triangleSquareCorrection, 2.0f, samplesToNext);
        }
        oscillator.expectedPulseAtNextSample = oscillator.phase < pulseWidth ? 1.0f : -1.0f;
    }

    bool dcoWrapped = false;
    if (needsDco)
    {
        const float oldDcoIncrement = oscillator.dcoIncrement;
        const float unwrappedDcoPhase = dcoPhase + oldDcoIncrement;
        dcoWrapped = unwrappedDcoPhase >= 1.0f;
        if (dcoPhase < 0.5f && unwrappedDcoPhase >= 0.5f)
            addFourthOrderBlep(oscillator.dcoTriangleSquareCorrection,
                               -2.0f,
                               (unwrappedDcoPhase - 0.5f) / oldDcoIncrement);
        const float thresholdVolts = 12.0f * oscillator.dcoHeldPulseWidth;
        constexpr float integratorLeakSeconds = 8.0f;

        const auto comparatorOutput = [thresholdVolts](float rampVolts) noexcept
        {
            return rampVolts < thresholdVolts ? 1.0f : -1.0f;
        };
        const auto advanceRampSegment = [this, &oscillator, comparatorOutput, thresholdVolts](
                                            float startFraction,
                                            float endFraction) noexcept
        {
            const float startRamp = oscillator.dcoRampVolts;
            const float seconds = std::max(0.0f, endFraction - startFraction) / oversampledRate_;
            const float endRamp = startRamp
                                  + seconds
                                        * (oscillator.dcoHeldSlopeVoltsPerSecond
                                           - startRamp / integratorLeakSeconds);
            const float startPulse = comparatorOutput(startRamp);
            const float endPulse = comparatorOutput(endRamp);
            if (startPulse != endPulse && std::abs(endRamp - startRamp) > 1.0e-12f)
            {
                const float localCrossing =
                    std::clamp((thresholdVolts - startRamp) / (endRamp - startRamp), 0.0f, 1.0f);
                const float eventFraction =
                    startFraction + localCrossing * (endFraction - startFraction);
                addFourthOrderBlep(oscillator.dcoPulseCorrection,
                                   endPulse - startPulse,
                                   1.0f - eventFraction);
            }
            oscillator.dcoRampVolts = endRamp;
        };

        if (dcoWrapped)
        {
            const float eventFraction =
                std::clamp((1.0f - dcoPhase) / oldDcoIncrement, 0.0f, 1.0f);
            const float samplesToNext = 1.0f - eventFraction;
            advanceRampSegment(0.0f, eventFraction);
            const float beforeReset = oscillator.dcoRampVolts;
            const float pulseBeforeReset = comparatorOutput(beforeReset);

            // The reset switch is active for approximately one range-clock
            // period. These low-order constants are fitted voicing targets,
            // not a claimed transistor netlist of the proprietary IC.
            oscillator.dcoRampVolts = oscillator.dcoResetResidue * beforeReset
                                    + oscillator.dcoChargeInjectionVolts;
            const float pulseAfterReset = comparatorOutput(oscillator.dcoRampVolts);
            const float normalisedResetStep = normaliseDcoSaw(oscillator.dcoRampVolts)
                                            - normaliseDcoSaw(beforeReset);
            addFourthOrderBlep(oscillator.dcoSawCorrection, normalisedResetStep, samplesToNext);
            addFourthOrderBlep(oscillator.dcoTriangleSquareCorrection, 2.0f, samplesToNext);
            if (pulseAfterReset != pulseBeforeReset)
                addFourthOrderBlep(oscillator.dcoPulseCorrection,
                                   pulseAfterReset - pulseBeforeReset,
                                   samplesToNext);
            oscillator.dcoLastResetSamplesToNext = samplesToNext;

            // Intel Mode 2 reload semantics: the pending control write becomes
            // active only after the current terminal-count event. Preserve the
            // elapsed post-event time in the newly loaded count's units.
            latchDcoTimer();
            oscillator.dcoPhase = samplesToNext * oscillator.dcoIncrement;
            advanceRampSegment(eventFraction, 1.0f);
        }
        else
        {
            oscillator.dcoPhase = unwrappedDcoPhase;
            advanceRampSegment(0.0f, 1.0f);
        }
        oscillator.dcoExpectedPulseAtNextSample = comparatorOutput(oscillator.dcoRampVolts);
    }

    oscillator.dcoWrappedThisSample = dcoWrapped;
    wrapped = model == OscillatorModel::Dco ? dcoWrapped : vcoWrapped;

    if (oscillator.waveformCrossfadeRemaining > 0)
    {
        --oscillator.waveformCrossfadeRemaining;
        if (oscillator.waveformCrossfadeRemaining == 0)
        {
            oscillator.waveformBlend.fill(0.0f);
            oscillator
                .waveformBlend[static_cast<std::size_t>(waveformIndex(oscillator.activeWave))] =
                1.0f;
            oscillator.waveformBlendStep.fill(0.0f);
        }
        else
        {
            for (std::size_t index = 0; index < oscillator.waveformBlend.size(); ++index)
                oscillator.waveformBlend[index] = std::clamp(
                    oscillator.waveformBlend[index] + oscillator.waveformBlendStep[index],
                    0.0f,
                    1.0f);
        }
    }

    if (oscillator.modelCrossfadeRemaining > 0)
    {
        --oscillator.modelCrossfadeRemaining;
        if (oscillator.modelCrossfadeRemaining == 0)
        {
            oscillator.modelBlend = oscillator.activeModel == OscillatorModel::Dco ? 1.0f : 0.0f;
            oscillator.modelBlendStep = 0.0f;
        }
        else
        {
            oscillator.modelBlend =
                std::clamp(oscillator.modelBlend + oscillator.modelBlendStep, 0.0f, 1.0f);
        }
    }
    return output;
}

float MarsEngine::renderVoiceOversample(Voice &voice,
                                        const EngineParameters &parameters,
                                        float lfoValue) noexcept
{
    if (!voice.active)
        return 0.0f;

    if (--voice.controlCountdown <= 0)
    {
        updateVoiceControl(voice, parameters, lfoValue);
        voice.controlCountdown = controlPeriod;
    }

    voice.oscillator1Increment += voice.oscillator1Step;
    voice.oscillator2Increment += voice.oscillator2Step;
    voice.subIncrement += voice.subStep;
    voice.ladderGain += voice.ladderGainStep;
    voice.stateGain += voice.stateGainStep;
    voice.panLeft += voice.panLeftStep;
    voice.panRight += voice.panRightStep;
    voice.velocity += velocitySmoothing_ * (voice.targetVelocity - voice.velocity);

    const float ampEnvelope = voice.ampEnvelope.tick(voice.ampAttackCoefficient,
                                                     voice.ampDecayCoefficient,
                                                     parameters.ampSustain,
                                                     voice.ampReleaseCoefficient);
    voice.filterEnvelope.tick(voice.filterAttackCoefficient,
                              voice.filterDecayCoefficient,
                              parameters.filterSustain,
                              voice.filterReleaseCoefficient);

    const float voiceAge = static_cast<float>(voice.ageSamples) / oversampledRate_;
    const float lfoFade = smoothStep(voiceAge / 0.14f);
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float ageScale = 0.25f + 0.75f * parameters.drift;
    const float pulseWidth = std::clamp(parameters.pulseWidth
                                        + 0.42f * parameters.lfoPwm * lfoFade * lfoValue
                                        + 0.035f * ageScale * card.pulseSkew,
                                        0.05f, 0.95f);
    const auto dcoRangeClock = [] (int octave) noexcept
    {
        // -1/0/+1 are the JUNO 16'/8'/4' 1/2/4 MHz clocks. Mars keeps its
        // existing outer octave positions as explicit 500 kHz/8 MHz range
        // divider extensions.
        return std::ldexp(2000000.0f, std::clamp(octave, -2, 2));
    };

    bool oscillator2Wrapped = false;
    float oscillator2 = renderOscillator(voice.oscillator2, parameters.osc2Wave,
                                         parameters.osc2Model,
                                         voice.oscillator2Increment, pulseWidth,
                                         dcoRangeClock(parameters.osc2Octave),
                                         oscillator2Wrapped);
    (void) oscillator2Wrapped;

    // Cross-mod changes VCO I's instantaneous frequency while VCO II keeps
    // running independently of its audio mixer gate. The lower bound prevents
    // phase reversal and the effective increment is also the value seen by the
    // BLEP event correction.
    // A variable-width pulse has mean 2*PW-1. Keep that physical DC in the
    // audio mixer/filter path, but remove it from the exponential-frequency
    // modulation source so PWM changes timbre rather than average VCO I pitch.
    const float modulationOscillator = parameters.osc2Wave == OscillatorWave::Pulse
        ? oscillator2 - (2.0f * pulseWidth - 1.0f)
        : oscillator2;
    const float frequencyModulation = std::clamp(
        1.0f + 0.72f * parameters.crossMod * modulationOscillator, 0.20f, 1.80f);
    const float oscillator1Increment = voice.oscillator1Increment
                                     * frequencyModulation;

    // The DCO sub below is derived directly from oscillator I's cycle state;
    // it never owns a second timer. Run the separate VCO sub only while that
    // endpoint is audible or crossfading, then freeze it at settled DCO.
    bool vcoSubWrapped = false;
    float vcoSub = 0.0f;
    const bool needsVcoSub = parameters.osc1Model == OscillatorModel::Vco
                          || voice.oscillator1.modelBlend < 1.0f
                          || voice.oscillator1.modelCrossfadeRemaining > 0;
    if (needsVcoSub)
        vcoSub = renderOscillator(
            voice.subOscillator, OscillatorWave::Pulse,
            OscillatorModel::Vco, voice.subIncrement, 0.5f,
            2000000.0f, vcoSubWrapped);
    (void) vcoSubWrapped;

    bool oscillator1Wrapped = false;
    float oscillator1 = renderOscillator(voice.oscillator1, parameters.osc1Wave,
                                         parameters.osc1Model,
                                         oscillator1Increment, pulseWidth,
                                         dcoRangeClock(parameters.osc1Octave),
                                         oscillator1Wrapped);
    (void) oscillator1Wrapped;

    // A Juno-family sub is a divider transition, not a separately tuned DCO.
    // Its D flip-flop toggles only on the MC5534A reset pulse. Keep parity
    // cheap, but freeze the inactive sub audio endpoint with the rest of the
    // DCO path once a VCO selection has settled.
    const bool needsDcoSub = parameters.osc1Model == OscillatorModel::Dco
                             || voice.oscillator1.modelBlend > 0.0f
                             || voice.oscillator1.modelCrossfadeRemaining > 0;
    float dcoSub = 0.0f;
    const bool dcoWrapped = voice.oscillator1.dcoWrappedThisSample;
    if (needsDcoSub)
    {
        if (!voice.dcoSubActive)
        {
            voice.dcoSubDelay.fill(0.0f);
            voice.dcoSubCorrection.fill(0.0f);
            voice.dcoSubBlepInitialised = false;
            voice.dcoSubReconstructionInitialised = false;
            voice.dcoSubActive = true;
        }
        const float rawDcoSub = voice.oscillator1CycleOdd ? -1.0f : 1.0f;
        dcoSub = processFourthOrderBlep(rawDcoSub,
                                       voice.dcoSubDelay,
                                       voice.dcoSubCorrection,
                                       voice.dcoSubBlepInitialised);
        if (dcoWrapped)
            addFourthOrderBlep(voice.dcoSubCorrection,
                               -2.0f * rawDcoSub,
                               voice.oscillator1.dcoLastResetSamplesToNext);
        if (!voice.dcoSubReconstructionInitialised)
        {
            voice.dcoSubReconstruction = dcoSub;
            voice.dcoSubReconstructionInitialised = true;
        }
        else
        {
            const float v = dcoReconstructionGain_ * (dcoSub - voice.dcoSubReconstruction);
            dcoSub = v + voice.dcoSubReconstruction;
            voice.dcoSubReconstruction = dcoSub + v;
        }
        if (!std::isfinite(dcoSub))
            dcoSub = 0.0f;
    }
    else
    {
        voice.dcoSubActive = false;
    }
    const float sub = vcoSub + voice.oscillator1.modelBlend * (dcoSub - vcoSub);

    if (dcoWrapped)
        voice.oscillator1CycleOdd = !voice.oscillator1CycleOdd;

    const float shaping = 1.05f + 0.28f * parameters.drift;
    const float normalisation = std::max(0.2f, softSaturate(shaping));
    oscillator1 = softSaturate(oscillator1 * shaping) / normalisation;
    oscillator2 = softSaturate(oscillator2 * shaping) / normalisation;

    const float whiteNoise = nextNoise(voice);
    const float noiseCoefficient = std::clamp(twoPi * 5200.0f / oversampledRate_, 0.001f, 0.42f);
    voice.noiseColour += noiseCoefficient * (whiteNoise - voice.noiseColour);
    const float filteredNoise = 0.62f * whiteNoise + 0.38f * voice.noiseColour;
    const float rawMix = oscillator1MixGain_ * oscillator1
                       + oscillator2MixGain_ * oscillator2
                       + parameters.subLevel * 0.72f * sub
                       + parameters.noiseLevel * filteredNoise;
    const float mixed = processAdaaMixer(rawMix, voice);

    if (!voice.filterModelInitialised)
    {
        voice.activeFilterModel = parameters.filterModel;
        voice.filterBlend = parameters.filterModel == FilterModel::Sem ? 1.0f : 0.0f;
        voice.filterBlendStep = 0.0f;
        voice.filterCrossfadeRemaining = 0;
        voice.filterModelInitialised = true;
    }
    else if (voice.activeFilterModel != parameters.filterModel)
    {
        // At a steady endpoint the inactive model has stale state, so restart
        // it silently under a zero-gain side of the crossfade. If automation
        // reverses an in-flight transition both models are already current and
        // the blend simply changes direction without a discontinuity.
        if (voice.filterCrossfadeRemaining == 0)
        {
            if (parameters.filterModel == FilterModel::Ladder)
                voice.ladder.reset();
            else
                voice.stateVariable.reset();
        }

        voice.activeFilterModel = parameters.filterModel;
        voice.filterCrossfadeRemaining = filterCrossfadeSamples_;
        const float targetBlend = parameters.filterModel == FilterModel::Sem ? 1.0f : 0.0f;
        voice.filterBlendStep = (targetBlend - voice.filterBlend)
                              / static_cast<float>(filterCrossfadeSamples_);
    }

    float filtered = 0.0f;
    const float inputVoltage = filterInputVoltage(mixed, voice.drive);
    const auto processLadder = [&voice, inputVoltage]() noexcept
    {
        return voice.ladder.process(inputVoltage, voice.ladderGain,
                                    voice.ladderFeedbackGain,
                                    voice.ladderFrequencyScale);
    };
    const auto processSem = [&voice, &parameters, inputVoltage]() noexcept
    {
        float low = 0.0f;
        float band = 0.0f;
        float high = 0.0f;
        const float semInput = softSaturate(inputVoltage / twiceThermalVoltage);
        voice.stateVariable.process(semInput, voice.stateGain, voice.resonance,
                                    low, band, high);

        // The original SEM's 50 kOhm mode pot is a linear crossfade between
        // simultaneous low- and high-pass outputs. Its centre is therefore a
        // half-level notch sum, 0.5 * (low + high), rather than band-pass.
        // The filter core remains SEM-inspired rather than component-level.
        (void) band;
        const float shape = std::clamp(parameters.filterShape, 0.0f, 1.0f);
        return (1.0f - shape) * low + shape * high;
    };

    if (voice.filterCrossfadeRemaining > 0)
    {
        const float ladderOutput = processLadder();
        const float semOutput = processSem();
        filtered = (1.0f - voice.filterBlend) * ladderOutput
                 + voice.filterBlend * semOutput;

        --voice.filterCrossfadeRemaining;
        if (voice.filterCrossfadeRemaining == 0)
        {
            voice.filterBlend = voice.activeFilterModel == FilterModel::Sem ? 1.0f : 0.0f;
            voice.filterBlendStep = 0.0f;
        }
        else
        {
            voice.filterBlend = std::clamp(voice.filterBlend + voice.filterBlendStep,
                                           0.0f, 1.0f);
        }
    }
    else if (voice.activeFilterModel == FilterModel::Ladder)
        filtered = processLadder();
    else
        filtered = processSem();

    const float velocityCurve = std::sqrt(voice.velocity);
    const float velocityGain = 1.0f + parameters.velocityAmount * (velocityCurve - 1.0f);
    // An OTA/diode VCA's transfer is controlled by gain; it does not stop
    // having a transfer characteristic merely because the envelope is low.
    // Colour the signal first, then apply the smoothly moving control gain.
    // This keeps the harmonic balance stable through a decay instead of the
    // old f(g*x) topology becoming progressively cleaner as the note closed.
    const float vcaDrive = 1.05f + 0.14f * ageScale * std::abs(card.driveError);
    const float vcaNormalisation = std::max(0.2f, softSaturate(vcaDrive));
    const float vcaColour = softSaturate(filtered * vcaDrive) / vcaNormalisation;
    const float output = vcaColour * ampEnvelope * velocityGain * voice.groupGain;
    voice.lastOutput = output;
    voice.outputEnergy += outputEnergySmoothing_
                        * (output * output - voice.outputEnergy);
    ++voice.ageSamples;

    if (voice.ampEnvelope.stage == EnvelopeStage::Idle)
        silenceVoice(voice);
    return output;
}

void MarsEngine::downsamplePair(HalfbandDecimator& decimator,
                                float firstLeft, float firstRight,
                                float secondLeft, float secondRight,
                                float& outputLeft, float& outputRight) noexcept
{
    const auto push = [&decimator](float left, float right)
    {
        decimator.left[static_cast<std::size_t>(decimator.writeIndex)] = left;
        decimator.right[static_cast<std::size_t>(decimator.writeIndex)] = right;
        decimator.writeIndex = (decimator.writeIndex + 1) & (halfbandRingSize - 1);
    };
    // Evaluate on the even (first-sample) decimation phase. The 68-sample FIR
    // centre then maps to exactly 34 output samples for one stage; a 4x
    // cascade is exactly 17 + 34 = 51 host samples.
    push(firstLeft, firstRight);

    const auto filter = [&decimator](
        const std::array<float, halfbandRingSize>& history) noexcept
    {
        const auto delayed = [&decimator, &history](int samples) noexcept
        {
            const int index = (decimator.writeIndex - 1 - samples)
                            & (halfbandRingSize - 1);
            return history[static_cast<std::size_t>(index)];
        };
        float output = 0.5f * delayed((halfbandTaps - 1) / 2);
        for (std::size_t index = 0; index < halfbandSideCoefficients.size(); ++index)
        {
            const int nearDelay = 1 + 2 * static_cast<int>(index);
            const int farDelay = (halfbandTaps - 1) - nearDelay;
            output += halfbandSideCoefficients[index]
                    * (delayed(nearDelay) + delayed(farDelay));
        }
        return output;
    };

    outputLeft = filter(decimator.left);
    outputRight = filter(decimator.right);
    push(secondLeft, secondRight);
}

void MarsEngine::processChorus(float inputLeft,
                               float inputRight,
                               const EngineParameters &parameters,
                               float &outputLeft,
                               float &outputRight) noexcept
{
    // A Juno-60 line contains 256 physical buckets clocked in two phases, so
    // the 128 stored signal-bearing stage pairs below produce N/(2*fClock)
    // delay exactly as the device does. Clock rate, rather than a fractional
    // read pointer, is modulated by an antiphase triangle LFO. This preserves
    // the BBD's characteristic piecewise pitch motion and sample-and-hold
    // spectrum; the measured fifth-order networks suppress its images.
    chorusPhase_ = wrapPhase(chorusPhase_ + parameters.chorusRateHz / oversampledRate_);
    const float triangle = 1.0f - 4.0f * std::abs(chorusPhase_ - 0.5f);
    constexpr float centreClock = 50000.0f;
    constexpr float clockDepth = 24000.0f;
    const float leftClock = centreClock + clockDepth * triangle;
    const float rightClock = centreClock - clockDepth * triangle;
    const float monoFeed = 0.70710678f * (inputLeft + inputRight);

    // Real BBD clock residue and device noise persist at idle. A plug-in must
    // nevertheless reach exact digital silence so hosts can suspend it and HQ
    // changes can remain click-free. Open the parasitic path quickly whenever
    // signal is present, then retain it well past the complete BBD delay before
    // fading and snapping to zero.
    if (std::abs(monoFeed) > 1.0e-8f)
        chorusActivity_ += chorusActivityAttack_ * (1.0f - chorusActivity_);
    else
    {
        chorusActivity_ *= chorusActivityRelease_;
        if (chorusActivity_ < 1.0e-6f)
            chorusActivity_ = 0.0f;
    }

    const float companderTarget = parameters.chorusCompander ? 1.0f : 0.0f;
    companderBlend_ += companderBlendCoefficient_ * (companderTarget - companderBlend_);
    if (std::abs(companderBlend_ - companderTarget) < 1.0e-6f)
        companderBlend_ = companderTarget;
    const float compressedFeed = chorusCompander_.compress(monoFeed);
    const float bbdFeed = monoFeed + companderBlend_ * (compressedFeed - monoFeed);

    float wetLeft =
        chorusLeft_.process(0.997f * bbdFeed, leftClock, oversampledRate_, chorusActivity_);
    float wetRight =
        chorusRight_.process(1.003f * bbdFeed, rightClock, oversampledRate_, chorusActivity_);
    float expandedLeft = wetLeft;
    float expandedRight = wetRight;
    chorusCompander_.expand(expandedLeft, expandedRight);
    wetLeft += companderBlend_ * (expandedLeft - wetLeft);
    wetRight += companderBlend_ * (expandedRight - wetRight);

    // The panel Mix remains a continuous modern control while following the
    // hardware convention that chorus adds a wet path instead of replacing
    // the dry one. A quarter-circle law closely matches the old preset gain at
    // normal settings and stays bounded at 100%.
    const float mix = std::clamp(parameters.chorusMix, 0.0f, 1.0f);
    const float angle = 0.25f * pi * mix;
    const float dryGain = std::cos(angle);
    const float wetGain = std::sin(angle);
    outputLeft = dryGain * inputLeft + wetGain * wetLeft;
    outputRight = dryGain * inputRight + wetGain * wetRight;
}

float MarsEngine::processDcBlocker(float input,
                                   float &previousInput,
                                   float &previousOutput) const noexcept
{
    const float output = input - previousInput + dcCoefficient_ * previousOutput;
    if (std::abs(input) < 1.0e-10f && std::abs(output) < 1.0e-4f)
    {
        previousInput = 0.0f;
        previousOutput = 0.0f;
        return 0.0f;
    }
    previousInput = input;
    previousOutput = output;
    return output;
}

void MarsEngine::process(float *left, float *right, int numSamples)
{
    if (numSamples <= 0)
        return;
    if (!prepared_)
        prepare(sampleRate_, numSamples);

    applyPendingOversamplingIfIdle();

    const EngineParameters targets = targetParameters_;
    smoothedParameters_.voiceMode = targets.voiceMode;
    smoothedParameters_.osc1Wave = targets.osc1Wave;
    smoothedParameters_.osc2Wave = targets.osc2Wave;
    smoothedParameters_.osc1Model = targets.osc1Model;
    smoothedParameters_.osc2Model = targets.osc2Model;
    smoothedParameters_.chorusCompander = targets.chorusCompander;
    smoothedParameters_.osc1Enabled = targets.osc1Enabled;
    smoothedParameters_.osc2Enabled = targets.osc2Enabled;
    smoothedParameters_.filterModel = targets.filterModel;
    smoothedParameters_.lfoWave = targets.lfoWave;
    smoothedParameters_.unisonVoices = targets.unisonVoices;
    smoothedParameters_.polyphonyLimit = targets.polyphonyLimit;
    smoothedParameters_.osc1Octave = targets.osc1Octave;
    smoothedParameters_.osc2Octave = targets.osc2Octave;
    smoothedParameters_.osc2Semitones = targets.osc2Semitones;

    const float smoothing = 1.0f - std::exp(-inverseSampleRate_ / 0.014f);
    const float performanceSmoothing = 1.0f - std::exp(-inverseSampleRate_ / 0.005f);
    const float mixerGateSmoothing = 1.0f - std::exp(-inverseSampleRate_ / 0.004f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto smooth = [smoothing](float& current, float target)
        {
            current += smoothing * (target - current);
        };
        const auto smoothLog = [smoothing](float& current, float target)
        {
            current = std::max(current, 1.0e-6f);
            target = std::max(target, 1.0e-6f);
            current *= std::exp(smoothing * std::log(target / current));
        };
        smooth(smoothedParameters_.osc2FineCents, targets.osc2FineCents);
        smooth(smoothedParameters_.pulseWidth, targets.pulseWidth);
        smooth(smoothedParameters_.crossMod, targets.crossMod);
        smooth(smoothedParameters_.oscMix, targets.oscMix);
        smooth(smoothedParameters_.subLevel, targets.subLevel);
        smooth(smoothedParameters_.noiseLevel, targets.noiseLevel);
        smoothLog(smoothedParameters_.cutoffHz, targets.cutoffHz);
        smooth(smoothedParameters_.resonance, targets.resonance);
        smooth(smoothedParameters_.filterDrive, targets.filterDrive);
        smooth(smoothedParameters_.filterShape, targets.filterShape);
        smooth(smoothedParameters_.filterEnvAmount, targets.filterEnvAmount);
        smooth(smoothedParameters_.filterKeyTrack, targets.filterKeyTrack);
        smoothLog(smoothedParameters_.ampAttack, targets.ampAttack);
        smoothLog(smoothedParameters_.ampDecay, targets.ampDecay);
        smooth(smoothedParameters_.ampSustain, targets.ampSustain);
        smoothLog(smoothedParameters_.ampRelease, targets.ampRelease);
        smoothLog(smoothedParameters_.filterAttack, targets.filterAttack);
        smoothLog(smoothedParameters_.filterDecay, targets.filterDecay);
        smooth(smoothedParameters_.filterSustain, targets.filterSustain);
        smoothLog(smoothedParameters_.filterRelease, targets.filterRelease);
        smoothLog(smoothedParameters_.lfoRateHz, targets.lfoRateHz);
        smooth(smoothedParameters_.lfoPitchCents, targets.lfoPitchCents);
        smooth(smoothedParameters_.lfoPwm, targets.lfoPwm);
        smooth(smoothedParameters_.lfoFilterOctaves, targets.lfoFilterOctaves);
        smooth(smoothedParameters_.drift, targets.drift);
        smooth(smoothedParameters_.spread, targets.spread);
        smooth(smoothedParameters_.glideSeconds, targets.glideSeconds);
        smooth(smoothedParameters_.velocityAmount, targets.velocityAmount);
        smooth(smoothedParameters_.chorusMix, targets.chorusMix);
        smoothLog(smoothedParameters_.chorusRateHz, targets.chorusRateHz);
        smooth(smoothedParameters_.outputGain, targets.outputGain);
        pitchBend_ += performanceSmoothing * (pitchBendTarget_ - pitchBend_);
        modWheel_ += performanceSmoothing * (modWheelTarget_ - modWheel_);
        // Balance is equal-power only while both mixer inputs are enabled.
        // A lone VCO is unity-gain at every Balance setting, matching a
        // hardware mixer switch. The VCOs themselves keep advancing and the
        // roughly 4 ms gain transition makes host automation click-resistant.
        float oscillator1TargetGain = 0.0f;
        float oscillator2TargetGain = 0.0f;
        if (smoothedParameters_.osc1Enabled && smoothedParameters_.osc2Enabled)
        {
            oscillator1TargetGain = std::sqrt(
                std::max(0.0f, 1.0f - smoothedParameters_.oscMix));
            oscillator2TargetGain = std::sqrt(
                std::max(0.0f, smoothedParameters_.oscMix));
        }
        else
        {
            oscillator1TargetGain = smoothedParameters_.osc1Enabled ? 1.0f : 0.0f;
            oscillator2TargetGain = smoothedParameters_.osc2Enabled ? 1.0f : 0.0f;
        }
        oscillator1MixGain_ += mixerGateSmoothing
                             * (oscillator1TargetGain - oscillator1MixGain_);
        oscillator2MixGain_ += mixerGateSmoothing
                             * (oscillator2TargetGain - oscillator2MixGain_);
        if (std::abs(oscillator1MixGain_ - oscillator1TargetGain) < 1.0e-5f)
            oscillator1MixGain_ = oscillator1TargetGain;
        if (std::abs(oscillator2MixGain_ - oscillator2TargetGain) < 1.0e-5f)
            oscillator2MixGain_ = oscillator2TargetGain;

        const auto renderInternalSample = [this](float& renderedLeft,
                                                 float& renderedRight) noexcept
        {
            if (--driftControlCountdown_ <= 0)
            {
                for (auto& card : cards_)
                    updateVoiceCardDrift(card);
                driftControlCountdown_ = controlPeriod;
            }
            const float lfoValue = nextLfoValue(smoothedParameters_);
            float voiceLeft = 0.0f;
            float voiceRight = 0.0f;
            for (auto& voice : voices_)
            {
                if (!voice.active)
                    continue;
                const float rendered = renderVoiceOversample(
                    voice, smoothedParameters_, lfoValue);
                voiceLeft += rendered * voice.panLeft;
                voiceRight += rendered * voice.panRight;
            }
            float tailLeft = 0.0f;
            float tailRight = 0.0f;
            renderStealTail(tailLeft, tailRight);
            voiceLeft += tailLeft;
            voiceRight += tailRight;

            float ensembleLeft = 0.0f;
            float ensembleRight = 0.0f;
            processChorus(voiceLeft, voiceRight, smoothedParameters_,
                           ensembleLeft, ensembleRight);
            renderedLeft = safetyLimit(ensembleLeft * smoothedParameters_.outputGain);
            renderedRight = safetyLimit(ensembleRight * smoothedParameters_.outputGain);
        };

        float dryLeft = 0.0f;
        float dryRight = 0.0f;
        if (oversampling_ == 4)
        {
            std::array<float, 4> internalLeft {};
            std::array<float, 4> internalRight {};
            for (std::size_t index = 0; index < internalLeft.size(); ++index)
                renderInternalSample(internalLeft[index], internalRight[index]);

            float firstIntermediateLeft = 0.0f;
            float firstIntermediateRight = 0.0f;
            float secondIntermediateLeft = 0.0f;
            float secondIntermediateRight = 0.0f;
            downsamplePair(firstDecimator_, internalLeft[0], internalRight[0],
                           internalLeft[1], internalRight[1],
                           firstIntermediateLeft, firstIntermediateRight);
            downsamplePair(firstDecimator_, internalLeft[2], internalRight[2],
                           internalLeft[3], internalRight[3],
                           secondIntermediateLeft, secondIntermediateRight);
            downsamplePair(secondDecimator_, firstIntermediateLeft,
                           firstIntermediateRight, secondIntermediateLeft,
                           secondIntermediateRight, dryLeft, dryRight);
        }
        else if (oversampling_ == 2)
        {
            float firstLeft = 0.0f;
            float firstRight = 0.0f;
            float secondLeft = 0.0f;
            float secondRight = 0.0f;
            renderInternalSample(firstLeft, firstRight);
            renderInternalSample(secondLeft, secondRight);
            downsamplePair(secondDecimator_, firstLeft, firstRight,
                           secondLeft, secondRight, dryLeft, dryRight);
        }
        else
        {
            renderInternalSample(dryLeft, dryRight);
        }

        float outputLeft = processDcBlocker(dryLeft, dcInputLeft_, dcOutputLeft_);
        float outputRight = processDcBlocker(dryRight, dcInputRight_, dcOutputRight_);
        // FIR ringing can exceed the internally antialiased guard by a tiny
        // amount. This is an emergency invariant, not an audible bus shaper.
        outputLeft = std::clamp(finiteOr(outputLeft, 0.0f), -1.25f, 1.25f);
        outputRight = std::clamp(finiteOr(outputRight, 0.0f), -1.25f, 1.25f);

        // When HQ is Off, compensate the native path to the fixed latency
        // reported for this prepared host rate. The ring is bounded and reset
        // during the already-idle quality transition, so no host call, lock,
        // allocation, or boundary click is introduced on the audio thread.
        const int compensation = oversampling_ == 1
            ? getProcessingLatencySamples() : 0;
        if (compensation > 0)
        {
            const auto write = static_cast<std::size_t>(latencyDelayWriteIndex_);
            const float delayedLeft = latencyDelayLeft_[write];
            const float delayedRight = latencyDelayRight_[write];
            latencyDelayLeft_[write] = outputLeft;
            latencyDelayRight_[write] = outputRight;
            latencyDelayWriteIndex_ = (latencyDelayWriteIndex_ + 1) % compensation;
            outputLeft = delayedLeft;
            outputRight = delayedRight;
        }

        // Count actual voice-free host samples, not the size of a block that
        // merely happens to end with no active voice. The 25 ms guard is much
        // longer than this short BBD/filter memory, while deliberately not
        // waiting on the 1.5 Hz DC servo's inaudible residual.
        bool anyVoiceActive = false;
        for (const auto& voice : voices_)
            anyVoiceActive = anyVoiceActive || voice.active;
        if (!anyVoiceActive)
        {
            const int quietSamplesRequired = std::max(
                1, static_cast<int>(std::lround(0.025 * sampleRate_)));
            oversamplingIdleSamples_ = std::min(
                oversamplingIdleSamples_ + 1, quietSamplesRequired);
        }
        else
        {
            oversamplingIdleSamples_ = 0;
        }

        if (left != nullptr)
            left[sample] = outputLeft;
        if (right != nullptr && right != left)
            right[sample] = outputRight;
        else if (right != nullptr)
            right[sample] = 0.5f * (outputLeft + outputRight);
    }

    updateActiveVoiceCount();
}

int MarsEngine::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount_;
}

} // namespace mars
