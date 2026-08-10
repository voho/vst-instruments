// Numerical qualification for the shared main-noise support circuit.
//
// C42/R81 and C41/R79 define physical analogue corners.  At ordinary
// processing rates the shipping TPT filters use those corners directly.  The
// supported 8 kHz HQ-off endpoint is different: C41/R79's 4.82 kHz pole lies
// above Nyquist, where an unclamped bilinear prewarp becomes negative and its
// state recursion is unstable.  This audit derives the component law again,
// independently of the production helpers, and qualifies the declared
// min(physical corner, 0.45 * internal rate) numerical boundary.

#include "DSP/YouKnow106Engine.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace youknow106
{
// Executable-local access to the actual updater, filter state and public-path
// aftermath.  Reference component arithmetic and transfer equations remain
// outside this friend.
struct YouKnow106TestAccess
{
    struct Wiring
    {
        double internalRate {};
        int factor {};
        float lowPassG {};
    };

    struct Impulse
    {
        std::vector<float> output;
        double worstState {};
        double finalState {};
        bool finite { true };
    };

    struct PublicRun
    {
        std::uint64_t hash { 1469598103934665603ull };
        std::array<double, 2> state {};
        double worstHighState {};
        double worstLowState {};
        double drivenEnergy {};
        bool finite { true };
    };

    static Wiring wiring(double hostRate, bool hq)
    {
        YouKnow106Engine engine;
        engine.prepare(hostRate, 64, hq);
        return { engine.oversampledRate_, engine.oversampling_,
                 engine.noiseSourceLowPassG_ };
    }

    static Wiring q1Wiring(YouKnow106Engine& engine, double hostRate)
    {
        engine.sampleRate_ = std::clamp(
            hostRate, 8000.0, YouKnow106Engine::maximumSupportedSampleRate);
        engine.inverseSampleRate_ = static_cast<float>(1.0 / engine.sampleRate_);
        engine.oversamplingRequested_ = false;
        engine.oversamplingEnabled_ = false;
        engine.updateProcessingRate();
        return { engine.oversampledRate_, engine.oversampling_,
                 engine.noiseSourceLowPassG_ };
    }

    static Impulse impulse(double hostRate, bool hq, int frames)
    {
        YouKnow106Engine engine;
        engine.prepare(hostRate, 64, hq);
        engine.noiseSourceLowPass_.reset();

        Impulse result;
        result.output.resize(static_cast<std::size_t>(frames));
        for (int frame = 0; frame < frames; ++frame)
        {
            const float input = frame == 0 ? 1.0f : 0.0f;
            const float output = engine.noiseSourceLowPass_.process(
                input, engine.noiseSourceLowPassG_, 1.0f, 0.0f);
            result.output[static_cast<std::size_t>(frame)] = output;
            result.worstState = std::max(
                result.worstState, std::abs(engine.noiseSourceLowPass_.state));
            result.finite = result.finite && std::isfinite(output)
                && std::isfinite(engine.noiseSourceLowPass_.state);
        }
        result.finalState = engine.noiseSourceLowPass_.state;
        return result;
    }

    static PublicRun publicRun(double hostRate, bool hq, int blockSize,
                               bool legacyUnclamped = false)
    {
        constexpr int idleFrames = 2048;
        constexpr int drivenFrames = 4096;
        YouKnow106Engine engine;
        engine.prepare(hostRate, 256, hq);
        if (legacyUnclamped)
        {
            // Exact former coefficient expression.  It is safe only on the
            // cap-inactive identity rows that call this mode.
            constexpr float productionPi = 3.14159265358979323846f;
            engine.noiseSourceLowPassG_ = std::tan(
                productionPi * YouKnow106Engine::noiseSourceLowPassHz()
                * engine.inverseOversampledRate_);
        }

        EngineParameters parameters;
        parameters.sawEnabled = false;
        parameters.pulseEnabled = false;
        parameters.subLevel = 0.0f;
        parameters.noiseLevel = 0.0f;
        parameters.cutoff = 1.0f;
        parameters.resonance = 0.0f;
        parameters.envDepth = 0.0f;
        parameters.vcaMode = VcaMode::Gate;
        parameters.vcaLevel = 1.0f;
        parameters.attack = 0.0f;
        parameters.decay = 0.0f;
        parameters.sustain = 1.0f;
        parameters.release = 0.0f;
        parameters.chorus = ChorusMode::Off;
        parameters.volume = 1.0f;
        parameters.calibration = 0.0f;
        engine.setParameters(parameters);

        PublicRun result;
        std::vector<float> left(static_cast<std::size_t>(blockSize));
        std::vector<float> right(static_cast<std::size_t>(blockSize));
        const auto hashFloat = [&](float value) {
            const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
            for (int byte = 0; byte < 4; ++byte)
            {
                result.hash ^= (bits >> (byte * 8)) & 0xffu;
                result.hash *= 1099511628211ull;
            }
        };
        const auto render = [&](int frames, bool driven) {
            int rendered = 0;
            while (rendered < frames)
            {
                const int count = std::min(blockSize, frames - rendered);
                engine.process(left.data(), right.data(), count);
                result.worstHighState = std::max(
                    result.worstHighState,
                    std::abs(engine.noiseSourceHighPass_.state));
                result.worstLowState = std::max(
                    result.worstLowState,
                    std::abs(engine.noiseSourceLowPass_.state));
                result.finite = result.finite
                    && std::isfinite(engine.noiseSourceHighPass_.state)
                    && std::isfinite(engine.noiseSourceLowPass_.state);
                for (int frame = 0; frame < count; ++frame)
                {
                    const float l = left[static_cast<std::size_t>(frame)];
                    const float r = right[static_cast<std::size_t>(frame)];
                    result.finite = result.finite
                        && std::isfinite(l) && std::isfinite(r);
                    hashFloat(l);
                    hashFloat(r);
                    if (driven)
                        result.drivenEnergy += static_cast<double>(l) * l
                                             + static_cast<double>(r) * r;
                }
                rendered += count;
            }
        };

        // The source advances unconditionally.  Exercise the hidden idle state
        // before making it audible, without a reset in between.
        render(idleFrames, false);
        parameters.noiseLevel = 1.0f;
        engine.setParameters(parameters);
        engine.noteOn(60, 1.0f);
        render(drivenFrames, true);
        result.state = { engine.noiseSourceHighPass_.state,
                         engine.noiseSourceLowPass_.state };
        return result;
    }
};
} // namespace youknow106

namespace
{
constexpr long double pi = std::numbers::pi_v<long double>;
constexpr long double c41Farads = 100.0e-12L;
constexpr long double r79Ohms = 330000.0L;
constexpr long double physicalCornerHz =
    1.0L / (2.0L * pi * c41Farads * r79Ohms);
constexpr long double policyFraction = 0.45L;
constexpr long double releaseRateHz = physicalCornerHz / policyFraction;
constexpr long double instabilityEndRateHz = 2.0L * physicalCornerHz;
constexpr long double coefficientTolerance = 1.0e-5L;
constexpr long double physicalResponseErrorGateDb = 1.70L;

struct Result
{
    int failures {};

    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
};

long double expectedDesignCorner(long double internalRate)
{
    return std::min(physicalCornerHz, policyFraction * internalRate);
}

long double expectedCoefficient(long double internalRate)
{
    return std::tan(pi * expectedDesignCorner(internalRate) / internalRate);
}

long double poleFor(long double g)
{
    return (1.0L - g) / (1.0L + g);
}

long double lowPassMagnitude(long double g, long double frequency,
                             long double sampleRate)
{
    const long double omega = 2.0L * pi * frequency / sampleRate;
    const std::complex<long double> delay(
        std::cos(omega), -std::sin(omega));
    const std::complex<long double> numerator = g * (1.0L + delay);
    const std::complex<long double> denominator =
        (1.0L + g) - (1.0L - g) * delay;
    return std::abs(numerator / denominator);
}

long double analogMagnitude(long double frequency)
{
    const long double ratio = frequency / physicalCornerHz;
    return 1.0L / std::sqrt(1.0L + ratio * ratio);
}

long double worstPhysicalResponseErrorDb(long double internalRate,
                                         long double g)
{
    long double worst = 0.0L;
    constexpr int points = 4096;
    for (int point = 0; point <= points; ++point)
    {
        const long double frequency = policyFraction * internalRate
            * static_cast<long double>(point) / points;
        const long double ratio = lowPassMagnitude(g, frequency, internalRate)
                                / analogMagnitude(frequency);
        worst = std::max(worst, std::abs(20.0L * std::log10(ratio)));
    }
    return worst;
}

bool policyCellPasses(long double hostRate, long double internalRate,
                      long double g)
{
    if (!std::isfinite(g) || g <= 0.0L || std::abs(poleFor(g)) >= 1.0L)
        return false;
    if (std::abs(g - expectedCoefficient(internalRate))
        > coefficientTolerance)
        return false;
    if (internalRate < releaseRateHz
        && worstPhysicalResponseErrorDb(internalRate, g)
               > physicalResponseErrorGateDb)
        return false;
    (void) hostRate;
    return true;
}

enum class Mutation
{
    None,
    NoCap,
    HostRateCap,
    Cap044,
    Cap046,
    Cap049,
    Cap055,
    AbsTan,
    PostTanClamp,
    SanitizeOnly
};

long double mutatedCoefficient(Mutation mutation, long double hostRate,
                               long double internalRate)
{
    long double design = expectedDesignCorner(internalRate);
    switch (mutation)
    {
        case Mutation::None:
            break;
        case Mutation::NoCap:
        case Mutation::SanitizeOnly:
            design = physicalCornerHz;
            break;
        case Mutation::HostRateCap:
            design = std::min(physicalCornerHz, policyFraction * hostRate);
            break;
        case Mutation::Cap044:
            design = std::min(physicalCornerHz, 0.44L * internalRate);
            break;
        case Mutation::Cap046:
            design = std::min(physicalCornerHz, 0.46L * internalRate);
            break;
        case Mutation::Cap049:
            design = std::min(physicalCornerHz, 0.49L * internalRate);
            break;
        case Mutation::Cap055:
            design = std::min(physicalCornerHz, 0.55L * internalRate);
            break;
        case Mutation::AbsTan:
            return std::abs(std::tan(pi * physicalCornerHz / internalRate));
        case Mutation::PostTanClamp:
            return std::min(std::tan(pi * physicalCornerHz / internalRate),
                            std::tan(pi * policyFraction));
    }
    return std::tan(pi * design / internalRate);
}

bool mutationRejected(Mutation mutation)
{
    constexpr std::array<long double, 8> q1Rates {
        8000.0L, 9000.0L, 9600.0L, 9645.0L,
        9646.0L, 10000.0L, 10717.0L, 10718.0L
    };
    for (const long double rate : q1Rates)
    {
        if (!policyCellPasses(
                rate, rate, mutatedCoefficient(mutation, rate, rate)))
            return true;
    }
    // A host-grid cap is only distinguishable when the public selector runs a
    // higher internal grid, as 8 kHz HQ does at q4.
    const long double hqG = mutatedCoefficient(mutation, 8000.0L, 32000.0L);
    return !policyCellPasses(8000.0L, 32000.0L, hqG);
}

int selfTest()
{
    using youknow106::YouKnow106TestAccess;
    Result result;
    std::cout << std::fixed << std::setprecision(9);
    std::cout << "noise component corner_hz="
              << static_cast<double>(physicalCornerHz)
              << " instability_end_hz="
              << static_cast<double>(instabilityEndRateHz)
              << " cap_release_hz=" << static_cast<double>(releaseRateHz)
              << '\n';

    struct Row
    {
        double hostRate;
        bool hq;
        int factor;
        double internalRate;
        const char* name;
    };
    constexpr std::array<Row, 21> rows {{
        { 8000.0, false, 1, 8000.0, "8k-q1" },
        { 9000.0, false, 1, 9000.0, "9k-q1" },
        { 9600.0, false, 1, 9600.0, "9.6k-q1" },
        { 9645.0, false, 1, 9645.0, "9.645k-q1" },
        { 9646.0, false, 1, 9646.0, "9.646k-q1" },
        { 10000.0, false, 1, 10000.0, "10k-q1" },
        { 10717.0, false, 1, 10717.0, "10.717k-q1" },
        { 10718.0, false, 1, 10718.0, "10.718k-q1" },
        { 11025.0, false, 1, 11025.0, "11.025k-q1" },
        { 8000.0, true, 4, 32000.0, "8k-q4" },
        { 44100.0, false, 1, 44100.0, "44.1k-q1" },
        { 44100.0, true, 4, 176400.0, "44.1k-q4" },
        { 48000.0, false, 1, 48000.0, "48k-q1" },
        { 48000.0, true, 4, 192000.0, "48k-q4" },
        { 88200.0, false, 1, 88200.0, "88.2k-q1" },
        { 88200.0, true, 2, 176400.0, "88.2k-q2" },
        { 96000.0, false, 1, 96000.0, "96k-q1" },
        { 96000.0, true, 2, 192000.0, "96k-q2" },
        { 176400.0, true, 1, 176400.0, "176.4k-q1" },
        { 192000.0, true, 1, 192000.0, "192k-q1" },
        { 768000.0, true, 1, 768000.0, "768k-q1" }
    }};

    long double worstCoefficientError = 0.0L;
    long double worstAnalogError = 0.0L;
    for (const auto& row : rows)
    {
        const auto wiring = YouKnow106TestAccess::wiring(row.hostRate, row.hq);
        const long double expectedG = expectedCoefficient(row.internalRate);
        const long double coefficientError = std::abs(
            static_cast<long double>(wiring.lowPassG) - expectedG);
        const long double pole = poleFor(wiring.lowPassG);
        const long double analogError = row.internalRate < releaseRateHz
            ? worstPhysicalResponseErrorDb(row.internalRate, wiring.lowPassG)
            : 0.0L;
        worstCoefficientError = std::max(worstCoefficientError,
                                         coefficientError);
        worstAnalogError = std::max(worstAnalogError, analogError);
        const bool pass = wiring.factor == row.factor
            && wiring.internalRate == row.internalRate
            && std::isfinite(wiring.lowPassG) && wiring.lowPassG > 0.0f
            && std::abs(pole) < 1.0L
            && coefficientError <= coefficientTolerance
            && analogError <= physicalResponseErrorGateDb;
        result.require(pass, std::string("shipping row ") + row.name);
        std::cout << "row " << row.name << " internal=" << wiring.internalRate
                  << " factor=" << wiring.factor << " g=" << wiring.lowPassG
                  << " pole=" << static_cast<double>(pole)
                  << " analog_error_db=" << static_cast<double>(analogError)
                  << " " << (pass ? "PASS" : "FAIL") << '\n';
    }

    // Dense actual updater sweep, plus floating-point neighbours around the
    // two analytically important seams.
    youknow106::YouKnow106Engine denseEngine;
    denseEngine.prepare(8000.0, 64, false);
    int denseCells = 0;
    for (int rate = 8000; rate <= 12000; ++rate)
    {
        const auto wiring = YouKnow106TestAccess::q1Wiring(denseEngine, rate);
        result.require(policyCellPasses(rate, wiring.internalRate,
                                        wiring.lowPassG),
                       "dense q1 coefficient/response cell");
        ++denseCells;
    }
    // The updater stores its rate and inverse rate as float. Double neighbours
    // would collapse to one cell there, so bracket both seams with adjacent
    // representable float host rates and then promote those exact cells.
    constexpr float floatPi = 3.14159265358979323846f;
    const float floatPhysicalCorner =
        1.0f / (2.0f * floatPi * 100.0e-12f * 330000.0f);
    const float floatInstabilityEnd = 2.0f * floatPhysicalCorner;
    const float floatReleaseRate = floatPhysicalCorner / 0.45f;
    const std::array<float, 6> seamRates {
        std::nextafter(floatInstabilityEnd, 0.0f),
        floatInstabilityEnd,
        std::nextafter(floatInstabilityEnd,
                       std::numeric_limits<float>::infinity()),
        std::nextafter(floatReleaseRate, 0.0f),
        floatReleaseRate,
        std::nextafter(floatReleaseRate,
                       std::numeric_limits<float>::infinity())
    };
    int releaseCapCells = 0;
    int releasePhysicalCells = 0;
    for (std::size_t index = 0; index < seamRates.size(); ++index)
    {
        const float floatRate = seamRates[index];
        const double rate = static_cast<double>(floatRate);
        const auto wiring = YouKnow106TestAccess::q1Wiring(denseEngine, rate);
        result.require(policyCellPasses(rate, wiring.internalRate,
                                        wiring.lowPassG),
                       "adjacent-float seam coefficient/response cell");
        if (index >= 3)
        {
            if (0.45f * floatRate < floatPhysicalCorner)
                ++releaseCapCells;
            else
                ++releasePhysicalCells;
        }
        ++denseCells;
    }
    result.require(seamRates[0] < seamRates[1]
                       && seamRates[1] < seamRates[2]
                       && seamRates[3] < seamRates[4]
                       && seamRates[4] < seamRates[5],
                   "float seam neighbours collapsed");
    result.require(releaseCapCells > 0 && releasePhysicalCells > 0,
                   "release seam did not straddle cap and physical branches");
    std::cout << "float seams instability=" << floatInstabilityEnd
              << " release=" << floatReleaseRate
              << " release_cap_cells=" << releaseCapCells
              << " release_physical_cells=" << releasePhysicalCells << '\n';

    // Candidate filter implementation versus a separately written long-double
    // TPT recurrence.  The coefficient itself was already independently gated.
    for (const double rate : { 8000.0, 9000.0, 9645.0, 9646.0,
                               10000.0, 10717.0, 10718.0 })
    {
        const auto wiring = YouKnow106TestAccess::wiring(rate, false);
        const auto actual = YouKnow106TestAccess::impulse(rate, false, 4096);
        long double state = 0.0L;
        bool responsePass = actual.finite && actual.worstState < 8.0;
        for (std::size_t frame = 0; frame < actual.output.size(); ++frame)
        {
            const long double input = frame == 0 ? 1.0L : 0.0L;
            const long double g = wiring.lowPassG;
            const long double v = (input - state) * g / (1.0L + g);
            const long double expected = v + state;
            state = expected + v;
            responsePass = responsePass
                && std::abs(static_cast<long double>(actual.output[frame])
                            - expected) <= 2.0e-7L;
        }
        responsePass = responsePass
            && std::abs(actual.finalState) < 1.0e-12
            && std::abs(state) < 1.0e-12L;
        result.require(responsePass, "independent endpoint TPT impulse");
    }

    // Public idle-to-driven source trajectory.  The final audio alone cannot
    // decide this contract because downstream filters and clips can hide an
    // exploding finite support state.
    for (const double rate : { 8000.0, 9000.0, 9645.0 })
    {
        const auto blockOne = YouKnow106TestAccess::publicRun(rate, false, 1);
        const auto blockPrime = YouKnow106TestAccess::publicRun(rate, false, 37);
        const bool pass = blockOne.finite && blockPrime.finite
            && blockOne.worstHighState < 8.0 && blockOne.worstLowState < 8.0
            && blockPrime.worstHighState < 8.0
            && blockPrime.worstLowState < 8.0
            && blockOne.drivenEnergy > 1.0e-8
            && blockOne.hash == blockPrime.hash
            && blockOne.state == blockPrime.state;
        result.require(pass, "public idle/driven block-partition trajectory");
        std::cout << "public " << rate << " hash=" << std::hex
                  << blockOne.hash << std::dec
                  << " hp_state_max=" << blockOne.worstHighState
                  << " lp_state_max=" << blockOne.worstLowState
                  << " energy=" << blockOne.drivenEnergy
                  << " " << (pass ? "PASS" : "FAIL") << '\n';
    }

    struct IdentityRow
    {
        double hostRate;
        bool hq;
        const char* name;
    };
    // These cells never enter the endpoint cap. Compare the current updater
    // with the exact former expression inside the same executable, avoiding a
    // machine-libm-dependent cross-architecture hash lock.
    constexpr std::array<IdentityRow, 12> identityRows {{
        { 8000.0, true, "8k-q4" },
        { 32000.0, false, "32k-q1" },
        { 44100.0, false, "44.1k-q1" },
        { 44100.0, true, "44.1k-q4" },
        { 48000.0, false, "48k-q1" },
        { 48000.0, true, "48k-q4" },
        { 88200.0, false, "88.2k-q1" },
        { 88200.0, true, "88.2k-q2" },
        { 96000.0, false, "96k-q1" },
        { 96000.0, true, "96k-q2" },
        { 192000.0, true, "192k-q1" },
        { 768000.0, true, "768k-q1" }
    }};
    for (const auto& row : identityRows)
    {
        const auto run = YouKnow106TestAccess::publicRun(
            row.hostRate, row.hq, 37);
        const auto legacy = YouKnow106TestAccess::publicRun(
            row.hostRate, row.hq, 37, true);
        const bool pass = run.finite && legacy.finite
            && run.hash == legacy.hash && run.state == legacy.state
            && run.drivenEnergy == legacy.drivenEnergy;
        result.require(pass, std::string("inactive-grid identity ") + row.name);
        std::cout << "identity " << row.name << " hash=" << std::hex
                  << run.hash << std::dec << " finite=" << run.finite << ' '
                  << (pass ? "PASS" : "FAIL") << '\n';
    }

    constexpr std::array<std::pair<Mutation, const char*>, 9> mutations {{
        { Mutation::NoCap, "no_cap" },
        { Mutation::HostRateCap, "host_rate_cap" },
        { Mutation::Cap044, "cap_044" },
        { Mutation::Cap046, "cap_046" },
        { Mutation::Cap049, "cap_049" },
        { Mutation::Cap055, "cap_055" },
        { Mutation::AbsTan, "abs_tan" },
        { Mutation::PostTanClamp, "post_tan_clamp" },
        { Mutation::SanitizeOnly, "sanitize_only" }
    }};
    for (const auto& [mutation, name] : mutations)
    {
        const bool rejected = mutationRejected(mutation);
        result.require(rejected, std::string("mutation control ") + name);
        std::cout << "mutation " << name << " "
                  << (rejected ? "REJECT" : "FALSE_PASS") << '\n';
    }

    std::cout << "dense_cells=" << denseCells
              << " worst_coefficient_error="
              << static_cast<double>(worstCoefficientError)
              << " worst_physical_response_error_db="
              << static_cast<double>(worstAnalogError) << '\n';
    std::cout << "Noise source quality self-test: "
              << (result.failures == 0 ? "PASS" : "FAIL") << '\n';
    return result.failures == 0 ? 0 : 1;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--self-test")
        return selfTest();
    std::cerr << "usage: YouKnow106NoiseSourceQualityAudit --self-test\n";
    return 1;
}
