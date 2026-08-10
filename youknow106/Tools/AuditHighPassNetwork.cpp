// Nominal C14/four-position-HPF network qualification.
//
// The production engine deliberately keeps a scalar C14 pole followed by one
// scalar selected-leg pole.  This audit does not use that cascade as its
// oracle: it stamps the Service Notes' ideal component network into independent
// long-double complex MNA systems, then compares the production constants and
// scalar transfer against those solves.  CMOS on/off parasitics, charge
// injection and switch-click amplitude are outside this nominal fixed-mode
// contract because no measurement anchors them.

#include "DSP/YouKnow106Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string_view>

namespace youknow106
{
// Keep production access narrow.  All reference component equations and MNA
// solves live outside this friend and share no production implementation.
struct YouKnow106TestAccess
{
    struct ContinuityProbe
    {
        double stateBefore {};
        double output {};
        double expectedOutput {};
        double resetOutput {};
    };

    struct WiringProbe
    {
        double processingRate {};
        int factor {};
        float couplingG {};
    };

    static float couplingCorner(HighPassMode mode) noexcept
    {
        return YouKnow106Engine::voiceBusCouplingCornerHz(mode);
    }

    static float legCorner(HighPassMode mode) noexcept
    {
        return YouKnow106Engine::highPassCornerHz(mode);
    }

    static float lowGain(HighPassMode mode) noexcept
    {
        return YouKnow106Engine::highPassShelfGain(mode);
    }

    static float highGain(HighPassMode mode) noexcept
    {
        return YouKnow106Engine::highPassHighGain(mode);
    }

    static WiringProbe wiringProbe(double hostRate, bool hqEnabled,
                                   HighPassMode mode)
    {
        YouKnow106Engine engine;
        engine.prepare(hostRate, 64, hqEnabled);
        EngineParameters parameters;
        parameters.highPass = mode;
        engine.updateSharedHighPass(parameters);
        return { engine.oversampledRate_, engine.oversampling_,
                 engine.voiceBusCouplingG_ };
    }

    static ContinuityProbe continuityProbe(double sampleRate)
    {
        YouKnow106Engine::HighPass preserved;
        preserved.reset();
        const double flatG = std::tan(
            std::numbers::pi * static_cast<double>(couplingCorner(
                HighPassMode::One)) / sampleRate);
        const double cutG = std::tan(
            std::numbers::pi * static_cast<double>(couplingCorner(
                HighPassMode::Two)) / sampleRate);

        // Reach a nontrivial, physically possible C14 voltage without writing
        // private state directly.  A coefficient change must retain it.
        const int primingSamples = static_cast<int>(sampleRate * 0.2);
        for (int sample = 0; sample < primingSamples; ++sample)
            (void) preserved.process(0.75f, static_cast<float>(flatG),
                                     0.0f, 1.0f);

        const double stateBefore = preserved.state;
        constexpr float nextInput = -0.25f;
        const double v = (static_cast<double>(nextInput) - stateBefore)
                       * cutG / (1.0 + cutG);
        const double expected = static_cast<double>(nextInput)
                              - (stateBefore + v);
        const double output = preserved.process(
            nextInput, static_cast<float>(cutG), 0.0f, 1.0f);

        YouKnow106Engine::HighPass resetMutation;
        resetMutation.reset();
        const double resetOutput = resetMutation.process(
            nextInput, static_cast<float>(cutG), 0.0f, 1.0f);
        return { stateBefore, output, expected, resetOutput };
    }
};
} // namespace youknow106

namespace
{
using Complex = std::complex<long double>;
using Mode = youknow106::HighPassMode;

constexpr long double pi = std::numbers::pi_v<long double>;
constexpr long double minimumFrequencyHz = 0.001L;
constexpr long double maximumFrequencyHz = 20000.0L;
constexpr long double nyquistSafetyFraction = 0.49L;
constexpr std::size_t pointsPerRate = 4097u;

// Keep the declared endpoint, common-host, and oversampled policy grids
// explicit.  At the supported 8 kHz host endpoint the two selectable paths
// run this filter at 8 kHz (HQ off) and 32 kHz (HQ on).
constexpr std::array<long double, 9> supportedRatesHz {
    8000.0L, 32000.0L, 44100.0L, 48000.0L, 88200.0L,
    96000.0L, 176400.0L, 192000.0L, 768000.0L
};
static_assert(nyquistSafetyFraction > 0.0L
              && nyquistSafetyFraction < 0.5L);
static_assert(supportedRatesHz.front() == 8000.0L);
static_assert(supportedRatesHz[1] == 32000.0L);

// Service Notes p. 15 nominal designators.  These deliberately do not refer
// to production constants: a shared transcription could otherwise make both
// sides agree while both were wrong.
constexpr long double c14 = 10.0e-6L;
constexpr long double r39 = 33.0e3L;
constexpr long double selectedCutBias = 1.0e6L; // R21 / R23
constexpr long double summerLeg = 47.0e3L;       // R25--R28
constexpr long double summerFeedback = 47.0e3L; // R29
constexpr long double c10 = 15.0e-9L;
constexpr long double c11 = 4.7e-9L;
constexpr long double r22 = 47.0e3L;
constexpr long double c9 = 47.0e-9L;
constexpr long double c8 = 10.0e-9L;
constexpr long double r18 = 100.0e3L;
constexpr long double r19 = 10.0e3L;
constexpr long double c6 = 22.0e-9L;
constexpr long double r24 = 220.0e3L;

constexpr long double parallel(long double first,
                               long double second) noexcept
{
    return first * second / (first + second);
}

constexpr long double selectedCutLoad = parallel(r39, selectedCutBias);
constexpr long double flatBoostLoad = parallel(r39, summerLeg);

constexpr std::array<Mode, 4> modes {
    Mode::Boost, Mode::One, Mode::Two, Mode::Three
};

constexpr std::string_view modeName(Mode mode) noexcept
{
    switch (mode)
    {
        case Mode::Boost: return "Boost";
        case Mode::One:   return "Flat";
        case Mode::Two:   return "CutII";
        case Mode::Three: return "CutIII";
    }
    return "Unknown";
}

template <std::size_t size>
using Matrix = std::array<std::array<Complex, size>, size>;

template <std::size_t size>
using Vector = std::array<Complex, size>;

template <std::size_t size>
Vector<size> solve(Matrix<size> matrix, Vector<size> right)
{
    // Independent small-system Gaussian elimination with partial pivoting.
    // The fixed-mode passive matrices are comfortably conditioned over the
    // audit band, but pivoting makes a transcription failure explicit rather
    // than allowing a non-finite result to limp through the extrema search.
    for (std::size_t column = 0; column < size; ++column)
    {
        std::size_t pivot = column;
        long double pivotMagnitude = std::abs(matrix[pivot][column]);
        for (std::size_t row = column + 1; row < size; ++row)
        {
            const long double candidate = std::abs(matrix[row][column]);
            if (candidate > pivotMagnitude)
            {
                pivot = row;
                pivotMagnitude = candidate;
            }
        }
        if (!(pivotMagnitude > std::numeric_limits<long double>::min()))
            throw std::runtime_error("singular HPF MNA matrix");
        if (pivot != column)
        {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(right[pivot], right[column]);
        }

        for (std::size_t row = column + 1; row < size; ++row)
        {
            const Complex scale = matrix[row][column]
                                / matrix[column][column];
            matrix[row][column] = {};
            for (std::size_t inner = column + 1; inner < size; ++inner)
                matrix[row][inner] -= scale * matrix[column][inner];
            right[row] -= scale * right[column];
        }
    }

    Vector<size> result {};
    for (std::size_t reverse = size; reverse-- > 0;)
    {
        Complex value = right[reverse];
        for (std::size_t column = reverse + 1; column < size; ++column)
            value -= matrix[reverse][column] * result[column];
        result[reverse] = value / matrix[reverse][reverse];
    }
    return result;
}

long double cutCapacitance(Mode mode)
{
    return mode == Mode::Two ? c10 : c11;
}

Complex exactFlat(Complex s)
{
    // One-node MNA: C14 feeds the parallel R39/R27 load.  IC4a inverts it;
    // the audit removes that common hardware polarity to match engine sign.
    return s * c14 / (s * c14 + 1.0L / r39 + 1.0L / summerLeg);
}

Complex exactCut(Complex s, Mode mode)
{
    const long double capacitor = cutCapacitance(mode);
    Matrix<2> matrix {{
        {{ s * (c14 + capacitor)
               + 1.0L / r39 + 1.0L / selectedCutBias,
           -s * capacitor }},
        {{ -s * capacitor,
           s * capacitor + 1.0L / summerLeg }}
    }};
    const Vector<2> right {{ s * c14, {} }}; // unit source
    const auto node = solve(matrix, right);
    return node[1]; // -IC4a output because R29 == selected 47 kOhm leg
}

Complex exactBoost(Complex s)
{
    // Nodes: x = C14/YCOM/Y3, p = C8/IC4b non-inverting input,
    // v6 = IC4b output minus p (the physical C6 voltage).  R20 drops no
    // voltage for an ideal op-amp input and therefore does not enter nominal
    // small-signal MNA.
    Matrix<3> matrix {{
        {{ s * (c14 + c9) + 1.0L / r39 + 1.0L / summerLeg
                                   + 1.0L / r22,
           -(s * c9 + 1.0L / r22),
           {} }},
        {{ -(s * c9 + 1.0L / r22),
           s * (c9 + c8) + 1.0L / r22,
           {} }},
        {{ {}, -1.0L / r19, s * c6 + 1.0L / r18 }}
    }};
    const Vector<3> right {{ s * c14, {}, {} }};
    const auto node = solve(matrix, right);
    const Complex ic4bOutput = node[1] + node[2];
    return (summerFeedback / summerLeg) * node[0]
         + (summerFeedback / r24) * ic4bOutput;
}

Complex exactTransfer(long double frequencyHz, Mode mode)
{
    const Complex s { 0.0L, 2.0L * pi * frequencyHz };
    switch (mode)
    {
        case Mode::Boost: return exactBoost(s);
        case Mode::One:   return exactFlat(s);
        case Mode::Two:
        case Mode::Three: return exactCut(s, mode);
    }
    return {};
}

enum class ScalarVariant
{
    Production,
    OldR39Only,
    BiasAcrossSummerLeg,
    SwappedCutCapacitors
};

struct ScalarParts
{
    long double couplingCornerHz {};
    long double legCornerHz {};
    long double lowGain {};
    long double highGain {};
};

ScalarParts scalarParts(Mode mode, ScalarVariant variant)
{
    using Access = youknow106::YouKnow106TestAccess;
    ScalarParts parts {
        static_cast<long double>(Access::couplingCorner(mode)),
        static_cast<long double>(Access::legCorner(mode)),
        static_cast<long double>(Access::lowGain(mode)),
        static_cast<long double>(Access::highGain(mode))
    };
    if (mode != Mode::Two && mode != Mode::Three)
        return parts;

    switch (variant)
    {
        case ScalarVariant::Production:
            break;
        case ScalarVariant::OldR39Only:
            parts.couplingCornerHz = 1.0L / (2.0L * pi * c14 * r39);
            break;
        case ScalarVariant::BiasAcrossSummerLeg:
        {
            const long double wrongTimingResistance =
                parallel(summerLeg, selectedCutBias);
            parts.legCornerHz = 1.0L
                / (2.0L * pi * wrongTimingResistance
                   * cutCapacitance(mode));
            break;
        }
        case ScalarVariant::SwappedCutCapacitors:
            parts.legCornerHz = 1.0L
                / (2.0L * pi * summerLeg
                   * (mode == Mode::Two ? c11 : c10));
            break;
    }
    return parts;
}

Complex analogOnePoleHigh(Complex s, long double cornerHz)
{
    const long double omega = 2.0L * pi * cornerHz;
    return s / (s + omega);
}

Complex scalarTransfer(long double frequencyHz, Mode mode,
                       ScalarVariant variant)
{
    const Complex s { 0.0L, 2.0L * pi * frequencyHz };
    const ScalarParts parts = scalarParts(mode, variant);
    const Complex coupling = analogOnePoleHigh(
        s, parts.couplingCornerHz);
    if (mode == Mode::One)
        return coupling;

    const long double omega = 2.0L * pi * parts.legCornerHz;
    if (mode == Mode::Two || mode == Mode::Three)
        return coupling * s / (s + omega);

    // The production TPT shelf is high*HP + low*LP.  Its continuous scalar
    // counterpart is algebraically the expression below.
    return coupling * (parts.highGain
        + (parts.lowGain - parts.highGain) / (1.0L + s / omega));
}

struct Metrics
{
    long double maximumMagnitudeDb {};
    long double magnitudeFrequencyHz {};
    long double maximumPhaseDegrees {};
    long double phaseFrequencyHz {};
    std::size_t evaluated {};
    std::size_t finite {};
};

void observe(Metrics& metrics, long double frequencyHz,
             Complex candidate, Complex reference)
{
    ++metrics.evaluated;
    const Complex ratio = candidate / reference;
    const long double magnitudeDb = std::abs(
        20.0L * std::log10(std::abs(ratio)));
    const long double phaseDegrees = std::abs(
        std::arg(ratio) * 180.0L / pi);
    if (!std::isfinite(magnitudeDb) || !std::isfinite(phaseDegrees)
        || !std::isfinite(candidate.real()) || !std::isfinite(candidate.imag())
        || !std::isfinite(reference.real()) || !std::isfinite(reference.imag()))
        return;
    ++metrics.finite;
    if (magnitudeDb > metrics.maximumMagnitudeDb)
    {
        metrics.maximumMagnitudeDb = magnitudeDb;
        metrics.magnitudeFrequencyHz = frequencyHz;
    }
    if (phaseDegrees > metrics.maximumPhaseDegrees)
    {
        metrics.maximumPhaseDegrees = phaseDegrees;
        metrics.phaseFrequencyHz = frequencyHz;
    }
}

Metrics logarithmicMetrics(Mode mode, ScalarVariant variant,
                           std::size_t points)
{
    Metrics metrics;
    const long double ratio = maximumFrequencyHz / minimumFrequencyHz;
    for (std::size_t index = 0; index < points; ++index)
    {
        const long double position = static_cast<long double>(index)
                                   / static_cast<long double>(points - 1u);
        const long double frequency = minimumFrequencyHz
            * std::pow(ratio, position);
        observe(metrics, frequency,
                scalarTransfer(frequency, mode, variant),
                exactTransfer(frequency, mode));
    }
    return metrics;
}

void mergeMetrics(Metrics& destination, const Metrics& source)
{
    destination.evaluated += source.evaluated;
    destination.finite += source.finite;
    if (source.maximumMagnitudeDb > destination.maximumMagnitudeDb)
    {
        destination.maximumMagnitudeDb = source.maximumMagnitudeDb;
        destination.magnitudeFrequencyHz = source.magnitudeFrequencyHz;
    }
    if (source.maximumPhaseDegrees > destination.maximumPhaseDegrees)
    {
        destination.maximumPhaseDegrees = source.maximumPhaseDegrees;
        destination.phaseFrequencyHz = source.phaseFrequencyHz;
    }
}

struct OnePolePair
{
    Complex low;
    Complex high;
};

OnePolePair digitalOnePole(long double frequencyHz, long double sampleRate,
                           long double cornerHz)
{
    const long double g = std::tan(pi * cornerHz / sampleRate);
    const Complex delay = std::exp(
        Complex { 0.0L, -2.0L * pi * frequencyHz / sampleRate });
    const Complex denominator = (1.0L + g) + (g - 1.0L) * delay;
    return { g * (1.0L + delay) / denominator,
             (1.0L - delay) / denominator };
}

Complex digitalProductionTransfer(long double frequencyHz,
                                  long double sampleRate, Mode mode)
{
    const ScalarParts parts = scalarParts(mode, ScalarVariant::Production);
    const auto coupling = digitalOnePole(
        frequencyHz, sampleRate, parts.couplingCornerHz);
    if (mode == Mode::One)
        return coupling.high;
    const auto leg = digitalOnePole(
        frequencyHz, sampleRate, parts.legCornerHz);
    if (mode == Mode::Two || mode == Mode::Three)
        return coupling.high * leg.high;
    return coupling.high
        * (parts.lowGain * leg.low + parts.highGain * leg.high);
}

Metrics rateGridMetrics(Mode mode, long double sampleRate,
                        std::size_t points)
{
    Metrics metrics;
    const long double upperFrequencyHz = std::min(
        maximumFrequencyHz, nyquistSafetyFraction * sampleRate);
    for (std::size_t index = 0; index < points; ++index)
    {
        const long double position = static_cast<long double>(index)
                                   / static_cast<long double>(points - 1u);
        const long double frequency = minimumFrequencyHz
            * std::pow(upperFrequencyHz / minimumFrequencyHz, position);
        observe(metrics, frequency,
                digitalProductionTransfer(frequency, sampleRate, mode),
                exactTransfer(frequency, mode));
    }
    return metrics;
}

struct Gate
{
    long double magnitudeDb;
    long double phaseDegrees;
};

// The 8 kHz endpoint cannot meet the common-rate envelope, chiefly because
// the 720.5 Hz section is no longer small relative to Fs.  This separate fence
// makes the limitation quantitative without weakening the analogue MNA gates
// or pretending that endpoint-rate discretization is topology error.
constexpr Gate endpointRateFence { 0.25L, 10.0L };
constexpr Gate endpointHqRateFence { 0.03L, 3.0L };

constexpr Gate gateFor(Mode mode) noexcept
{
    switch (mode)
    {
        case Mode::Boost: return { 0.0085L, 0.0570L };
        case Mode::One:   return { 0.00001L, 0.0001L };
        case Mode::Two:   return { 0.0131L, 0.0430L };
        case Mode::Three: return { 0.0042L, 0.0136L };
    }
    return {};
}

bool passes(const Metrics& metrics, Gate gate) noexcept
{
    return metrics.evaluated == metrics.finite
        && metrics.maximumMagnitudeDb <= gate.magnitudeDb
        && metrics.maximumPhaseDegrees <= gate.phaseDegrees;
}

class Check
{
public:
    void require(bool condition, std::string_view message)
    {
        if (!condition)
        {
            passed_ = false;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    [[nodiscard]] bool passed() const noexcept { return passed_; }

private:
    bool passed_ { true };
};

bool nearRelative(long double actual, long double expected,
                  long double tolerance) noexcept
{
    return std::abs(actual - expected)
        <= tolerance * std::max(1.0L, std::abs(expected));
}

int selfTest()
{
    using Access = youknow106::YouKnow106TestAccess;
    Check check;
    std::cout << std::fixed << std::setprecision(9);

    const long double expectedCutCorner =
        1.0L / (2.0L * pi * c14 * selectedCutLoad);
    const long double expectedCutTau = c14 * selectedCutLoad;
    for (const Mode mode : { Mode::Two, Mode::Three })
    {
        const long double corner = static_cast<long double>(
            Access::couplingCorner(mode));
        const long double tau = 1.0L / (2.0L * pi * corner);
        check.require(nearRelative(corner, expectedCutCorner, 2.0e-7L),
                      "selected Cut C14 corner is not R39 || 1 MOhm");
        check.require(nearRelative(tau, expectedCutTau, 2.0e-7L),
                      "selected Cut C14 time constant is not 319.4578896 ms");
    }
    check.require(nearRelative(
        static_cast<long double>(Access::couplingCorner(Mode::One)),
        1.0L / (2.0L * pi * c14 * flatBoostLoad), 2.0e-7L),
        "Flat C14 load is not R39 || R27");
    check.require(nearRelative(
        static_cast<long double>(Access::couplingCorner(Mode::Boost)),
        1.0L / (2.0L * pi * c14 * flatBoostLoad), 2.0e-7L),
        "Boost C14 load is not R39 || R25");

    struct PolicyGrid
    {
        double hostRate;
        bool hqEnabled;
        int factor;
        long double processingRate;
    };
    constexpr std::array<PolicyGrid, 9> policyGrids {{
        { 8000.0, false, 1, 8000.0L },
        { 8000.0, true, 4, 32000.0L },
        { 44100.0, false, 1, 44100.0L },
        { 48000.0, false, 1, 48000.0L },
        { 88200.0, false, 1, 88200.0L },
        { 96000.0, false, 1, 96000.0L },
        { 44100.0, true, 4, 176400.0L },
        { 48000.0, true, 4, 192000.0L },
        { 768000.0, true, 1, 768000.0L }
    }};
    std::size_t wiringProbeCount = 0u;
    for (const Mode mode : modes)
    {
        const long double expectedLoad =
            mode == Mode::Two || mode == Mode::Three
                ? selectedCutLoad : flatBoostLoad;
        const long double expectedCorner =
            1.0L / (2.0L * pi * c14 * expectedLoad);
        for (const PolicyGrid& grid : policyGrids)
        {
            const Access::WiringProbe probe = Access::wiringProbe(
                grid.hostRate, grid.hqEnabled, mode);
            const long double expectedG = std::tan(
                pi * expectedCorner / grid.processingRate);
            check.require(probe.factor == grid.factor
                              && nearRelative(probe.processingRate,
                                              grid.processingRate, 1.0e-12L),
                          "production HPF policy grid selected the wrong rate");
            check.require(std::isfinite(probe.couplingG)
                              && nearRelative(probe.couplingG,
                                              expectedG, 2.0e-6L),
                          "production shared-HPF updater did not realize the mode-specific C14 load");
            ++wiringProbeCount;
        }
    }
    check.require(wiringProbeCount == modes.size() * policyGrids.size(),
                  "production shared-HPF wiring probe count changed");

    constexpr std::size_t coarsePoints = 60001u;
    constexpr std::size_t finePoints = 240001u;
    for (std::size_t index = 0; index < modes.size(); ++index)
    {
        const Mode mode = modes[index];
        const Metrics coarse = logarithmicMetrics(
            mode, ScalarVariant::Production, coarsePoints);
        const Metrics fine = logarithmicMetrics(
            mode, ScalarVariant::Production, finePoints);
        check.require(fine.evaluated == finePoints && fine.finite == finePoints,
                      "fixed-mode sweep produced non-finite or missing filters");
        check.require(std::abs(fine.maximumMagnitudeDb
                               - coarse.maximumMagnitudeDb) < 2.0e-5L,
                      "magnitude extremum did not converge across grid density");
        check.require(std::abs(fine.maximumPhaseDegrees
                               - coarse.maximumPhaseDegrees) < 2.0e-5L,
                      "phase extremum did not converge across grid density");
        check.require(passes(fine, gateFor(mode)),
                      "production scalar exceeds nominal fixed-mode MNA gate");

        std::cout << modeName(mode)
                  << " analog max_mag_db=" << fine.maximumMagnitudeDb
                  << " at_hz=" << fine.magnitudeFrequencyHz
                  << " max_phase_deg=" << fine.maximumPhaseDegrees
                  << " at_hz=" << fine.phaseFrequencyHz
                  << " finite=" << fine.finite << '/' << fine.evaluated
                  << '\n';
    }

    // Realized TPT filters are sampled on the declared endpoint, common-host,
    // and oversampled policy grids.  The >=44.1 kHz grids retain their
    // established digital envelope.  The separately reported 8 kHz endpoint
    // and its 32 kHz HQ grid stop at 0.49*Fs; any wider high-corner prewarp or
    // near-Nyquist phase error is an explicit endpoint-rate numerical
    // limitation, not an analogue-topology admission and not evidence about
    // unmodelled switch transients.
    std::size_t digitalFilterCount = 0u;
    for (const Mode mode : modes)
    {
        Metrics standardGrid;
        for (std::size_t rateIndex = 2u;
             rateIndex < supportedRatesHz.size(); ++rateIndex)
        {
            mergeMetrics(standardGrid, rateGridMetrics(
                mode, supportedRatesHz[rateIndex], pointsPerRate));
        }
        check.require(standardGrid.evaluated == standardGrid.finite,
                      "standard-rate TPT grid produced a non-finite filter");
        check.require(standardGrid.maximumMagnitudeDb < 0.02L,
                      "standard-rate TPT magnitude escaped its scalar family");
        check.require(standardGrid.maximumPhaseDegrees < 1.65L,
                      "standard-rate TPT phase escaped its declared q1 bound");

        constexpr std::size_t endpointCoarsePoints = 1025u;
        const Metrics endpointCoarse = rateGridMetrics(
            mode, supportedRatesHz.front(), endpointCoarsePoints);
        const Metrics endpoint = rateGridMetrics(
            mode, supportedRatesHz.front(), pointsPerRate);
        check.require(endpoint.evaluated == pointsPerRate
                          && endpoint.finite == endpoint.evaluated,
                      "8 kHz endpoint produced a non-finite or missing filter");
        check.require(endpointCoarse.evaluated == endpointCoarsePoints
                          && endpointCoarse.finite == endpointCoarse.evaluated,
                      "8 kHz convergence grid produced a non-finite filter");
        check.require(std::abs(endpoint.maximumMagnitudeDb
                               - endpointCoarse.maximumMagnitudeDb) < 2.0e-5L,
                      "8 kHz magnitude extremum did not converge");
        check.require(std::abs(endpoint.maximumPhaseDegrees
                               - endpointCoarse.maximumPhaseDegrees) < 2.0e-5L,
                      "8 kHz phase extremum did not converge");
        check.require(endpoint.maximumMagnitudeDb
                          < endpointRateFence.magnitudeDb,
                      "8 kHz magnitude exceeded its endpoint-rate fence");
        check.require(endpoint.maximumPhaseDegrees
                          < endpointRateFence.phaseDegrees,
                      "8 kHz phase exceeded its endpoint-rate fence");
        const bool endpointWithinStandardEnvelope =
            endpoint.maximumMagnitudeDb < 0.02L
            && endpoint.maximumPhaseDegrees < 1.65L;

        const Metrics endpointHq = rateGridMetrics(
            mode, supportedRatesHz[1], pointsPerRate);
        check.require(endpointHq.evaluated == pointsPerRate
                          && endpointHq.finite == endpointHq.evaluated,
                      "32 kHz endpoint-HQ grid produced a non-finite filter");
        check.require(endpointHq.maximumMagnitudeDb
                          < endpointHqRateFence.magnitudeDb,
                      "32 kHz magnitude exceeded its endpoint-HQ fence");
        check.require(endpointHq.maximumPhaseDegrees
                          < endpointHqRateFence.phaseDegrees,
                      "32 kHz phase exceeded its endpoint-HQ fence");
        const bool endpointHqWithinStandardEnvelope =
            endpointHq.maximumMagnitudeDb < 0.02L
            && endpointHq.maximumPhaseDegrees < 1.65L;

        digitalFilterCount += standardGrid.evaluated + endpoint.evaluated
                            + endpointHq.evaluated;
        std::cout << modeName(mode)
                  << " standard_rate_grid_max_mag_db="
                  << standardGrid.maximumMagnitudeDb
                  << " standard_rate_grid_max_phase_deg="
                  << standardGrid.maximumPhaseDegrees
                  << " finite=" << standardGrid.finite << '/'
                  << standardGrid.evaluated << '\n';
        std::cout << modeName(mode)
                  << " endpoint_8khz_max_hz="
                  << nyquistSafetyFraction * supportedRatesHz.front()
                  << " max_mag_db=" << endpoint.maximumMagnitudeDb
                  << " at_hz=" << endpoint.magnitudeFrequencyHz
                  << " max_phase_deg=" << endpoint.maximumPhaseDegrees
                  << " at_hz=" << endpoint.phaseFrequencyHz
                  << " finite=" << endpoint.finite << '/'
                  << endpoint.evaluated
                  << " standard_envelope="
                  << (endpointWithinStandardEnvelope
                          ? "PASS" : "ENDPOINT_LIMITED")
                  << " endpoint_fence=PASS convergence=PASS"
                  << '\n';
        std::cout << modeName(mode)
                  << " endpoint_hq_32khz_max_hz="
                  << nyquistSafetyFraction * supportedRatesHz[1]
                  << " max_mag_db=" << endpointHq.maximumMagnitudeDb
                  << " at_hz=" << endpointHq.magnitudeFrequencyHz
                  << " max_phase_deg=" << endpointHq.maximumPhaseDegrees
                  << " at_hz=" << endpointHq.phaseFrequencyHz
                  << " finite=" << endpointHq.finite << '/'
                  << endpointHq.evaluated
                  << " standard_envelope="
                  << (endpointHqWithinStandardEnvelope
                          ? "PASS" : "ENDPOINT_HQ_LIMITED")
                  << " endpoint_hq_fence=PASS"
                  << '\n';
    }
    check.require(digitalFilterCount
                      == modes.size() * supportedRatesHz.size() * pointsPerRate,
                  "supported-rate filter count changed");

    for (const Mode mode : { Mode::Two, Mode::Three })
    {
        const Gate gate = gateFor(mode);
        const Metrics oldLoad = logarithmicMetrics(
            mode, ScalarVariant::OldR39Only, finePoints);
        const Metrics wrongSide = logarithmicMetrics(
            mode, ScalarVariant::BiasAcrossSummerLeg, finePoints);
        const Metrics swapped = logarithmicMetrics(
            mode, ScalarVariant::SwappedCutCapacitors, finePoints);
        check.require(!passes(oldLoad, gate),
                      "R39-only selected-Cut mutation was not rejected");
        check.require(!passes(wrongSide, gate),
                      "1 MOhm-across-47 kOhm mutation was not rejected");
        check.require(!passes(swapped, gate),
                      "C10/C11 swap mutation was not rejected");
        std::cout << modeName(mode)
                  << " mutations old_load_db=" << oldLoad.maximumMagnitudeDb
                  << " wrong_side_db=" << wrongSide.maximumMagnitudeDb
                  << " swapped_db=" << swapped.maximumMagnitudeDb
                  << " all_rejected=YES\n";
    }

    const auto continuity = Access::continuityProbe(192000.0);
    check.require(std::abs(continuity.output - continuity.expectedOutput)
                      < 2.0e-7,
                  "C14 state did not survive a mode-coefficient change");
    check.require(std::abs(continuity.output - continuity.resetOutput) > 0.1,
                  "reset-on-mode-change mutation was not observable");
    check.require(std::isfinite(continuity.stateBefore)
                      && std::abs(continuity.stateBefore) > 0.1,
                  "mode-continuity probe did not establish a charged C14");

    std::cout << "selected_cut_load_ohms=" << selectedCutLoad
              << " tau_seconds=" << expectedCutTau
              << " corner_hz=" << expectedCutCorner << '\n';
    std::cout << "mode_continuity state=" << continuity.stateBefore
              << " output=" << continuity.output
              << " reset_mutation_output=" << continuity.resetOutput << '\n';
    std::cout << "analog_fixed_modes=" << modes.size()
              << " production_wiring_probes=" << wiringProbeCount
              << " supported_rate_responses=" << digitalFilterCount
              << " convergence=PASS mutations=PASS"
              << " switch_parasitics=NOT_CLAIMED\n";

    if (!check.passed())
    {
        std::cerr << "High-pass network audit: FAIL\n";
        return 1;
    }
    std::cout << "High-pass network audit: PASS\n";
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--self-test")
        return selfTest();
    std::cerr << "usage: YouKnow106HighPassNetworkAudit --self-test\n";
    return 2;
}
