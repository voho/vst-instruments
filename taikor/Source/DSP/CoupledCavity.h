#pragma once

#include <array>
#include <cstddef>

namespace taikor::cavity
{
inline constexpr std::size_t radialModeCount = 4;
inline constexpr std::size_t headCoordinateCount = 2 * radialModeCount;
inline constexpr std::size_t maximumAxialModes = 4;
inline constexpr std::size_t maximumCoordinates = headCoordinateCount + maximumAxialModes;
using Vector = std::array<double, maximumCoordinates>;
using Matrix = std::array<Vector, maximumCoordinates>;

struct Input
{
    double radius = 0.2;
    double depth = 0.5;
    double coupling = 1.0;
    double airDensity = 1.2041;
    double soundSpeed = 343.0;
    std::array<double, radialModeCount> besselZeros {
        2.4048255576957728, 5.5200781102863106,
        8.6537279129110122, 11.791534439014281
    };
    // Supplied alongside the zeros so the engine can reuse its Bessel helper.
    // Signs matter: a higher radial mode can displace net volume backwards.
    std::array<double, radialModeCount> besselJ1AtZeros {
        0.5191474972894668, -0.3402648065583682,
        0.2714522999283819, -0.2324598313647248
    };
    std::array<double, 2> headArealDensity { 4.0, 4.0 };
    // Frequencies before INTERNAL air coupling, in radians/second. This solver
    // adds no exterior-air estimate, bending law, damping, or calibrated gain.
    std::array<std::array<double, radialModeCount>, 2> headAngularFrequencies {};
    std::size_t axialModeCount = 2;
};

struct Solution
{
    std::size_t coordinateCount = 0;
    Matrix mass {};
    Matrix stiffness {};
    Vector angularFrequencies {}; // ascending; only coordinateCount entries valid
    // Physical coordinates = eigenvectors * modal coordinates. Each column has
    // unit modal mass: Phi^T M Phi = I, Phi^T K Phi = diag(omega^2).
    // Coordinates: four batter shapes, four outward rear shapes, axial sines.
    Matrix eigenvectors {};
    unsigned jacobiSweeps = 0;
};

// Passive lossless cylinder approximation, not Suzuki-Hwang's full barrel
// pressure expansion: https://doi.org/10.1250/ast.29.215, Eqs. 8, 12-15.
// All retained radial head shapes share ONE plane-wave axial pressure field;
// transverse air modes, barrel flare, viscothermal losses and leakage are absent.
// Coupling is a reciprocal sqrt(eta) boundary transformer, not a porosity law.
// At eta=0 the acoustic modes remain finite, but the heads cannot excite them.
// No allocation. Returns false for invalid input or numerical non-convergence.
[[nodiscard]] bool solve (const Input&, Solution&) noexcept;
} // namespace taikor::cavity
