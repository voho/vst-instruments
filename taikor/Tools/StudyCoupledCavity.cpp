// Experimental 1-D cavity study using Taikor's actual resolved factory drums.
// Reference: Suzuki & Hwang (2008), https://doi.org/10.1250/ast.29.215.
// Their full barrel-pressure solution motivates the mechanism; this candidate
// uses the passive cylinder Ritz reduction documented in DSP/CoupledCavity.cpp.
//
// This tool compares physics, not audio. Existing octave/tuning geometry and
// exterior-air-loaded head frequencies are frozen. Khead=Mnominal*omega_loaded^2
// preserves the current exterior-air frequency approximation; it does NOT fix
// exterior-air mass in force/contact/observation coordinates. No fitted gains,
// loudness matching, decay model, radiation model or listening claim is added.
//
// Integration is NOT a defaults switch: each candidate mode spans multiple
// radial head shapes plus air coordinates, whereas the current engine stores
// independent pairs with scalar head participation. Contact force/sensing,
// radiation, palm damping, tension energy and automation state would all need
// the same full eigenbasis. Preserve physical x and velocity including the air,
// then project through Phi_new^T M_new; pending forces need reciprocal mapping.
// Merely copying frequencies into the old forty membrane IDs is incorrect.

#include "DSP/CoupledCavity.h"
#include "DSP/TaikoEngine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace taikor
{
struct CavityStudyAccess
{
    struct FactoryDrum
    {
        cavity::Input input;
        std::array<double, 2> legacyPairHz {};
    };

    static FactoryDrum factoryDrum (int octave)
    {
        const EngineParameters parameters;
        const auto drum = TaikoEngine::resolveDrumFor (parameters, 0.0f, octave);
        const auto pair = TaikoEngine::solveAxisymmetricPair (drum);
        FactoryDrum result;
        result.legacyPairHz = { pair.lowerHz, pair.upperHz };
        auto& input = result.input;
        input.radius = drum.radius;
        input.depth = drum.depth;
        input.coupling = parameters.cavityCoupling;
        input.headArealDensity = { drum.batterDensity, drum.resonantDensity };
        // Input's 20 C air constants match TaikoEngine.cpp (1.2041 kg/m^3,
        // 343 m/s). Do not replace these with the paper's 1.21 kg/m^3 fixture.
        const auto& modes = TaikoEngine::membraneModes();
        static_assert (TaikoEngine::axisymmetricEntryCount == cavity::radialModeCount);
        for (std::size_t radial = 0; radial < cavity::radialModeCount; ++radial)
        {
            const auto lambda = modes[radial].besselZero;
            input.besselZeros[radial] = lambda;
            input.besselJ1AtZeros[radial] = TaikoEngine::besselJ (1, lambda);
            const auto omegas = TaikoEngine::membraneModeOmegas (
                drum, drum.radius, static_cast<float> (lambda), 0.0f);
            input.headAngularFrequencies[0][radial] = omegas.batter;
            input.headAngularFrequencies[1][radial] = omegas.resonant;
        }
        return result;
    }
};
} // namespace taikor

namespace
{
using namespace taikor::cavity;
constexpr double pi = 3.1415926535897932384626433832795;

std::array<double, headCoordinateCount> nominalHeadMasses (const Input& input)
{
    std::array<double, headCoordinateCount> masses {};
    const double area = pi * input.radius * input.radius;
    for (std::size_t coordinate = 0; coordinate < headCoordinateCount; ++coordinate)
    {
        const auto head = coordinate / radialModeCount;
        const auto radial = coordinate % radialModeCount;
        const double j1 = input.besselJ1AtZeros[radial];
        masses[coordinate] = input.headArealDensity[head] * area * j1 * j1;
    }
    return masses;
}

struct Participation
{
    double fundamentalHeads = 0.0;
    double air = 0.0;
};

std::array<Participation, maximumCoordinates> participations (
    const Input& input, const Solution& solution)
{
    const auto masses = nominalHeadMasses (input);
    std::array<Participation, maximumCoordinates> result {};
    for (std::size_t mode = 0; mode < solution.coordinateCount; ++mode)
    {
        double allHeads = 0.0;
        for (std::size_t coordinate = 0; coordinate < headCoordinateCount; ++coordinate)
        {
            const double value = solution.eigenvectors[coordinate][mode];
            const double share = masses[coordinate] * value * value;
            allHeads += share;
            if (coordinate % radialModeCount == 0)
                result[mode].fundamentalHeads += share;
        }
        // Phi^T M Phi=1, and M=Mhead+Mair. This is kinetic-energy participation,
        // not pressure amplitude or audibility. Clamp only floating-point residue.
        result[mode].air = std::clamp (1.0 - allHeads, 0.0, 1.0);
    }
    return result;
}

std::array<std::size_t, maximumCoordinates> rankedModes (
    const Solution& solution,
    const std::array<Participation, maximumCoordinates>& participation,
    bool rankAir)
{
    std::array<std::size_t, maximumCoordinates> order {};
    std::iota (order.begin(), order.end(), std::size_t { 0 });
    const auto score = [&] (std::size_t mode)
    {
        return rankAir ? participation[mode].air : participation[mode].fundamentalHeads;
    };
    std::sort (order.begin(), order.begin() + static_cast<std::ptrdiff_t> (solution.coordinateCount),
               [&] (std::size_t a, std::size_t b)
               {
                   if (score (a) != score (b))
                       return score (a) > score (b);
                   return solution.angularFrequencies[a] < solution.angularFrequencies[b];
               });
    return order;
}

std::array<double, 2> printCandidate (const Input& input, const Solution& solution)
{
    const auto participation = participations (input, solution);
    const auto ranking = rankedModes (solution, participation, false);
    std::array<std::size_t, 2> selected { ranking[0], ranking[1] };
    std::sort (selected.begin(), selected.end()); // frequencies are ascending
    std::array<double, 2> frequencies {};
    std::cout << "  N=" << input.axialModeCount << " selected fundamental-head pair: ";
    for (std::size_t branch = 0; branch < selected.size(); ++branch)
    {
        const auto mode = selected[branch];
        frequencies[branch] = solution.angularFrequencies[mode] / (2.0 * pi);
        if (branch != 0)
            std::cout << " / ";
        std::cout << frequencies[branch] << " Hz ("
                  << 100.0 * participation[mode].fundamentalHeads << "% fundamental-head mass)";
    }
    std::cout << '\n';
    if (input.axialModeCount != 0)
    {
        auto acoustic = rankedModes (solution, participation, true);
        std::sort (acoustic.begin(), acoustic.begin()
                   + static_cast<std::ptrdiff_t> (input.axialModeCount));
        std::cout << "      " << input.axialModeCount << " modes with greatest air kinetic share: ";
        for (std::size_t i = 0; i < input.axialModeCount; ++i)
        {
            if (i != 0)
                std::cout << ", ";
            const auto mode = acoustic[i];
            std::cout << solution.angularFrequencies[mode] / (2.0 * pi)
                      << " Hz (" << 100.0 * participation[mode].air << "% air)";
        }
        std::cout << '\n';
    }
    return frequencies;
}

bool printCost (Input input)
{
    constexpr int iterations = 1000;
    constexpr std::size_t batches = 5;
    std::array<double, batches> microseconds {};
    Solution result;
    double checksum = 0.0;
    for (std::size_t batch = 0; batch < batches; ++batch)
    {
        const auto start = std::chrono::steady_clock::now();
        for (int iteration = 0; iteration < iterations; ++iteration)
        {
            if (! solve (input, result))
                return false;
            checksum += result.angularFrequencies[0];
        }
        microseconds[batch] = std::chrono::duration<double, std::micro> (
            std::chrono::steady_clock::now() - start).count() / iterations;
    }
    std::sort (microseconds.begin(), microseconds.end());
    std::cout << "  N=" << input.axialModeCount << " solve cost: "
              << microseconds[batches / 2] << " us median of 5 x 1000 solves"
              << " (geometry resolution excluded, checksum=" << checksum << ")\n";
    return true;
}
} // namespace

int main()
{
    std::cout << std::fixed << std::setprecision (3)
              << "Taikor experimental shared-cavity study; no audio rendered or cavity activation.\n"
              << "Reference: https://doi.org/10.1250/ast.29.215\n"
              << "Inputs: resolved factory geometry/tuning and existing exterior-air frequency shifts;\n"
              << "nominal head masses retained. This does not repair exterior-air mass coordinates.\n"
              << "N = retained axial sine coordinates in ONE shared 1-D cylindrical air field.\n"
              << "Candidate pair = two greatest (0,1) nominal head kinetic-mass participations,\n"
              << "sorted by frequency; selection is not an audibility or rendered-pitch estimate.\n"
              << "Higher air modes, transverse pressure, barrel flare, damping and radiation are absent.\n";

    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset; ++octave)
    {
        const auto drum = taikor::CavityStudyAccess::factoryDrum (octave);
        auto input = drum.input;
        std::cout << '\n' << taikor::getDrumDescription (octave).displayName
                  << ": radius=" << input.radius << " m, depth=" << input.depth
                  << " m, coupling=" << input.coupling << ", sigma="
                  << input.headArealDensity[0] << '/' << input.headArealDensity[1] << " kg/m^2\n"
                  << "  Current independent (0,1) pair: " << drum.legacyPairHz[0]
                  << " / " << drum.legacyPairHz[1] << " Hz\n"
                  << "  Rigid-end air reference poles c*j/(2L): ";
        for (std::size_t axial = 1; axial <= maximumAxialModes; ++axial)
            std::cout << (axial == 1 ? "" : ", ")
                      << input.soundSpeed * static_cast<double> (axial) / (2.0 * input.depth);
        std::cout << " Hz\n";

        std::array<std::array<double, 2>, 3> pairs {};
        for (std::size_t candidate = 0; candidate < pairs.size(); ++candidate)
        {
            input.axialModeCount = 2 * candidate;
            Solution solution;
            if (! solve (input, solution))
            {
                std::cerr << "Cavity solve failed for octave " << octave
                          << ", N=" << input.axialModeCount << '\n';
                return 1;
            }
            pairs[candidate] = printCandidate (input, solution);
        }
        std::cout << "  Selected-pair truncation change N=2 -> N=4: "
                  << 1200.0 * std::log2 (pairs[2][0] / pairs[1][0]) << " / "
                  << 1200.0 * std::log2 (pairs[2][1] / pairs[1][1])
                  << " cents (successive truncations, not a measured-error bound)\n";
        for (const std::size_t count : { 2u, 4u })
        {
            input.axialModeCount = count;
            if (! printCost (input))
                return 1;
        }
    }
    return 0;
}
