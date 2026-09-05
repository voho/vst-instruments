#include "DSP/CoupledCavity.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>

namespace
{
using namespace taikor::cavity;
constexpr double pi = 3.1415926535897932384626433832795;
int failures = 0;

void expect (bool condition, const char* message)
{
    if (! condition)
    {
        if (failures < 12)
            std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near (double actual, double expected, double tolerance = 1.0e-9)
{
    return std::abs (actual - expected)
        <= tolerance * std::max ({ 1.0, std::abs (actual), std::abs (expected) });
}

Input referenceInput()
{
    Input input;
    input.airDensity = 1.21;
    for (std::size_t head = 0; head < 2; ++head)
        for (std::size_t radial = 0; radial < radialModeCount; ++radial)
            input.headAngularFrequencies[head][radial] = 2.0 * pi * 113.3
                * input.besselZeros[radial] / input.besselZeros[0];
    return input;
}

double quadratic (const Matrix& matrix, const Vector& vector, std::size_t count)
{
    double result = 0.0;
    for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = 0; j < count; ++j)
            result += vector[i] * matrix[i][j] * vector[j];
    return result;
}

void checkEigenproblem (const Solution& solution)
{
    const auto n = solution.coordinateCount;
    for (std::size_t mode = 0; mode < n; ++mode)
    {
        expect (solution.angularFrequencies[mode] > 0.0, "all stiffness eigenvalues must be positive");
        if (mode != 0)
            expect (solution.angularFrequencies[mode] >= solution.angularFrequencies[mode - 1],
                    "eigenfrequencies must be sorted");
        double residual = 0.0, denominator = 0.0;
        const double eigenvalue = solution.angularFrequencies[mode]
                                * solution.angularFrequencies[mode];
        for (std::size_t i = 0; i < n; ++i)
        {
            double stiffness = 0.0, inertia = 0.0;
            for (std::size_t j = 0; j < n; ++j)
            {
                stiffness += solution.stiffness[i][j] * solution.eigenvectors[j][mode];
                inertia += solution.mass[i][j] * solution.eigenvectors[j][mode] * eigenvalue;
                expect (solution.mass[i][j] == solution.mass[j][i], "mass must be symmetric");
                expect (solution.stiffness[i][j] == solution.stiffness[j][i], "stiffness must be symmetric");
            }
            residual += (stiffness - inertia) * (stiffness - inertia);
            denominator += stiffness * stiffness + inertia * inertia;
        }
        expect (std::sqrt (residual / std::max (denominator, 1.0e-300)) < 2.0e-9,
                "generalized eigenpair residual must be small");
        for (std::size_t other = 0; other < n; ++other)
        {
            double inner = 0.0;
            for (std::size_t i = 0; i < n; ++i)
                for (std::size_t j = 0; j < n; ++j)
                    inner += solution.eigenvectors[i][mode] * solution.mass[i][j]
                           * solution.eigenvectors[j][other];
            expect (near (inner, mode == other ? 1.0 : 0.0, 2.0e-10),
                    "eigenvectors must be orthonormal in the full physical mass metric");
        }
    }
}

void testEnergyIntegrals()
{
    auto input = referenceInput();
    input.axialModeCount = 4;
    input.coupling = 0.73;
    Solution result;
    expect (solve (input, result), "energy model must solve");
    checkEigenproblem (result);
    std::minstd_rand random { 1948 };
    const double area = pi * input.radius * input.radius;
    for (int trial = 0; trial < 8; ++trial)
    {
        Vector vector {};
        for (double& value : vector)
            value = 2.0 * std::generate_canonical<double, 32> (random) - 1.0;
        double left = 0.0, right = 0.0, headMass = 0.0, headStiffness = 0.0;
        for (std::size_t radial = 0; radial < radialModeCount; ++radial)
        {
            const double mean = 2.0 * input.besselJ1AtZeros[radial] / input.besselZeros[radial];
            left -= std::sqrt (input.coupling) * mean * vector[radial];
            right += std::sqrt (input.coupling) * mean * vector[radialModeCount + radial];
            for (std::size_t head = 0; head < 2; ++head)
            {
                const auto i = head * radialModeCount + radial;
                const double mass = area * input.headArealDensity[head]
                                  * std::pow (input.besselJ1AtZeros[radial], 2);
                headMass += mass * vector[i] * vector[i];
                headStiffness += mass * vector[i] * vector[i]
                              * std::pow (input.headAngularFrequencies[head][radial], 2);
            }
        }
        // Independent numerical integral of the continuum displacement field,
        // rather than repeating the solver's analytic matrix coefficients.
        constexpr int steps = 2048;
        double displacementIntegral = 0.0, strainIntegral = 0.0;
        for (int step = 0; step <= steps; ++step)
        {
            const double z = static_cast<double> (step) / steps;
            double displacement = (1.0 - z) * left + z * right;
            double strain = (right - left) / input.depth;
            for (std::size_t j = 0; j < input.axialModeCount; ++j)
            {
                const double phase = static_cast<double> (j + 1) * pi;
                displacement += vector[headCoordinateCount + j] * std::sin (phase * z);
                strain += vector[headCoordinateCount + j] * phase / input.depth * std::cos (phase * z);
            }
            const double weight = step == 0 || step == steps ? 1.0 : (step % 2 == 0 ? 2.0 : 4.0);
            displacementIntegral += weight * displacement * displacement;
            strainIntegral += weight * strain * strain;
        }
        displacementIntegral *= input.depth / (3.0 * steps);
        strainIntegral *= input.depth / (3.0 * steps);
        const double mass = quadratic (result.mass, vector, result.coordinateCount);
        const double stiffness = quadratic (result.stiffness, vector, result.coordinateCount);
        expect (mass > 0.0 && stiffness > 0.0, "physical energies must be positive");
        expect (near (mass, headMass + input.airDensity * area * displacementIntegral, 1.0e-9),
                "mass matrix must equal the continuum kinetic energy integral");
        expect (near (stiffness, headStiffness + input.airDensity * input.soundSpeed
                               * input.soundSpeed * area * strainIntegral, 1.0e-9),
                "stiffness matrix must equal the continuum compression energy integral");
    }
}

void testUncoupledAndRigidLimits()
{
    auto input = referenceInput();
    input.axialModeCount = 4;
    input.coupling = 0.0;
    input.headAngularFrequencies[1][0] *= 1.11;
    Solution result;
    expect (solve (input, result), "zero coupling must remain nonsingular");
    Vector expected {};
    for (std::size_t i = 0; i < headCoordinateCount; ++i)
        expected[i] = input.headAngularFrequencies[i / radialModeCount][i % radialModeCount];
    for (std::size_t j = 0; j < input.axialModeCount; ++j)
        expected[headCoordinateCount + j] = static_cast<double> (j + 1) * pi
                                          * input.soundSpeed / input.depth;
    std::sort (expected.begin(), expected.end());
    for (std::size_t i = 0; i < maximumCoordinates; ++i)
    {
        expect (near (result.angularFrequencies[i], expected[i]),
                "zero coupling must recover every bare-head mode and exact rigid-end axial pole");
        for (std::size_t j = 0; j < maximumCoordinates; ++j)
            if (i != j)
                expect (result.mass[i][j] == 0.0 && result.stiffness[i][j] == 0.0,
                        "zero coupling must remove all head-head and head-air coupling");
    }
    input.coupling = 1.0;
    for (auto& head : input.headAngularFrequencies)
        head.fill (1.0e7);
    expect (solve (input, result), "rigid-head limit must solve");
    for (std::size_t j = 0; j < input.axialModeCount; ++j)
        expect (near (result.angularFrequencies[j], static_cast<double> (j + 1) * pi
                                                  * input.soundSpeed / input.depth, 1.0e-5),
                "stiff heads must approach actual axial air poles");
}

// Exact cylindrical plane-wave dynamic stiffness in outward coordinates is
// rho*A*c*omega/sin(kL) [[cos(kL),1],[1,cos(kL)]]. For identical heads it has
// breathing eigenvalue rho*A*c*omega*cot(kL/2), and translation eigenvalue
// -rho*A*c*omega*tan(kL/2). This independent frequency-domain oracle retains
// the acoustic poles; it does not use the Ritz matrices or their eigensolver.
double exactLowestBranch (const Input& input, bool breathing)
{
    const auto function = [&] (double omega)
    {
        const double area = pi * input.radius * input.radius;
        double compliance = 0.0;
        for (std::size_t radial = 0; radial < radialModeCount; ++radial)
        {
            const double j1 = input.besselJ1AtZeros[radial];
            const double shape = 2.0 * j1 / input.besselZeros[radial];
            const double mass = input.headArealDensity[0] * area * j1 * j1;
            const double bare = input.headAngularFrequencies[0][radial];
            compliance += shape * shape / (mass * (bare * bare - omega * omega));
        }
        const double x = omega * input.depth / (2.0 * input.soundSpeed);
        const double stiffness = input.airDensity * area * input.soundSpeed * omega
                               * (breathing ? 1.0 / std::tan (x) : -std::tan (x));
        return 1.0 + stiffness * compliance;
    };
    const double reference = input.headAngularFrequencies[0][0];
    double low = breathing ? reference * (1.0 + 1.0e-8) : 1.0;
    double high = breathing ? reference * 1.5 : reference * (1.0 - 1.0e-8);
    expect (function (low) * function (high) < 0.0, "analytic branch must have a root bracket");
    for (int step = 0; step < 80; ++step)
    {
        const double middle = (low + high) * 0.5;
        if ((function (middle) > 0.0) == (function (low) > 0.0))
            low = middle;
        else
            high = middle;
    }
    return (low + high) * 0.5;
}

void testInertanceAndConvergence()
{
    auto input = referenceInput();
    input.axialModeCount = 0;
    Solution result;
    expect (solve (input, result), "lumped-inertance model must solve");
    const double area = pi * input.radius * input.radius;
    const double g = 2.0 * input.besselJ1AtZeros[0] / input.besselZeros[0];
    const double headMass = input.headArealDensity[0] * area
                         * input.besselJ1AtZeros[0] * input.besselJ1AtZeros[0];
    const double airMass = input.airDensity * area * input.depth * g * g;
    expect (near (result.mass[0][0] - headMass, airMass / 3.0),
            "each head must see rho*A*L*g^2/3 diagonal air mass");
    expect (near (result.mass[0][4], -airMass / 6.0),
            "opposite outward coordinates must carry negative cross mass");
    expect (near (result.mass[0][0] - headMass + result.mass[0][4], airMass / 6.0),
            "breathing branch must recover its low-frequency rho*L/6 inertance");
    expect (near (result.mass[0][0] - headMass - result.mass[0][4], airMass / 2.0),
            "translation branch must recover its low-frequency rho*L/2 inertance");
    expect (result.stiffness[0][1] < 0.0,
            "one shared cavity must couple opposite-sign radial volume projections");

    const double lower = exactLowestBranch (input, false);
    const double upper = exactLowestBranch (input, true);
    double previousError = std::numeric_limits<double>::infinity();
    for (const std::size_t count : { 0u, 2u, 4u })
    {
        input.axialModeCount = count;
        expect (solve (input, result), "convergence candidate must solve");
        const double error = std::hypot (result.angularFrequencies[0] - lower,
                                         result.angularFrequencies[1] - upper);
        expect (error < previousError, "adding axial sine coordinates must converge toward exact 1-D coupling");
        previousError = error;
        std::cout << "axial=" << count << " lower=" << result.angularFrequencies[0] / (2.0 * pi)
                  << " upper=" << result.angularFrequencies[1] / (2.0 * pi) << " Hz\n";
    }
    expect (previousError / lower < 1.0e-5, "four axial coordinates must accurately reproduce low-branch dynamics");
}

void testRangeAndInvalidInput()
{
    std::minstd_rand random { 8173 };
    const auto unit = [&] { return std::generate_canonical<double, 32> (random); };
    for (int trial = 0; trial < 100; ++trial)
    {
        auto input = referenceInput();
        input.radius = 0.008 * std::pow (3.75 / 0.008, unit());
        input.depth = 0.02 * std::pow (12.0 / 0.02, unit());
        input.coupling = unit();
        input.axialModeCount = static_cast<std::size_t> (trial % 3) * 2;
        for (std::size_t head = 0; head < 2; ++head)
        {
            input.headArealDensity[head] = 0.25 + 4.0 * unit();
            const double speed = 20.0 + 120.0 * unit();
            for (std::size_t radial = 0; radial < radialModeCount; ++radial)
                input.headAngularFrequencies[head][radial] = speed
                    * input.besselZeros[radial] / input.radius;
        }
        Solution result;
        expect (solve (input, result), "geometry/tension sweep must stay stable");
        checkEigenproblem (result);
    }
    for (int failure = 0; failure < 8; ++failure)
    {
        auto input = referenceInput();
        switch (failure)
        {
            case 0: input.radius = 0.0; break;
            case 1: input.depth = std::numeric_limits<double>::infinity(); break;
            case 2: input.coupling = -0.1; break;
            case 3: input.coupling = std::numeric_limits<double>::quiet_NaN(); break;
            case 4: input.headArealDensity[0] = 0.0; break;
            case 5: input.headAngularFrequencies[1][2] = 0.0; break;
            case 6: input.axialModeCount = 5; break;
            case 7: input.besselJ1AtZeros[3] = 0.0; break;
        }
        Solution result;
        expect (! solve (input, result), "invalid physical input must fail explicitly");
    }
}
} // namespace

int main()
{
    testEnergyIntegrals();
    testUncoupledAndRigidLimits();
    testInertanceAndConvergence();
    testRangeAndInvalidInput();
    for (const std::size_t count : { 2u, 4u })
    {
        auto input = referenceInput();
        input.axialModeCount = count;
        Solution result;
        double checksum = 0.0;
        const auto start = std::chrono::steady_clock::now();
        constexpr int iterations = 1000;
        for (int iteration = 0; iteration < iterations; ++iteration)
        {
            input.coupling = 0.1 + 0.9 * static_cast<double> (iteration % 101) / 100.0;
            expect (solve (input, result), "benchmark candidate must solve");
            checksum += result.angularFrequencies[0];
        }
        const auto elapsed = std::chrono::duration<double, std::micro> (
            std::chrono::steady_clock::now() - start).count();
        std::cout << "axial=" << count << " mean solve=" << elapsed / iterations
                  << " us; checksum=" << checksum << '\n';
    }
    if (failures == 0)
        std::cout << "Coupled cavity tests passed\n";
    else
        std::cerr << failures << " checks failed\n";
    return failures == 0 ? 0 : 1;
}
