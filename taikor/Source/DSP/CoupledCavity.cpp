#include "DSP/CoupledCavity.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace taikor::cavity
{
namespace
{
constexpr double pi = 3.1415926535897932384626433832795;

bool positiveFinite (double value) noexcept
{
    return value > 0.0 && std::isfinite (value);
}

bool assemble (const Input& input, Solution& result) noexcept
{
    if (! positiveFinite (input.radius) || ! positiveFinite (input.depth)
        || ! positiveFinite (input.airDensity) || ! positiveFinite (input.soundSpeed)
        || ! std::isfinite (input.coupling) || input.coupling < 0.0
        || input.coupling > 1.0 || input.axialModeCount > maximumAxialModes)
        return false;

    const double area = pi * input.radius * input.radius;
    const double transformer = std::sqrt (input.coupling);
    Vector left {}, right {};
    result.coordinateCount = headCoordinateCount + input.axialModeCount;
    for (std::size_t radial = 0; radial < radialModeCount; ++radial)
    {
        const double lambda = input.besselZeros[radial];
        const double j1 = input.besselJ1AtZeros[radial];
        if (! positiveFinite (lambda) || ! std::isfinite (j1) || j1 == 0.0)
            return false;
        const double meanShape = 2.0 * j1 / lambda;
        left[radial] = -transformer * meanShape;
        right[radialModeCount + radial] = transformer * meanShape;
        for (std::size_t head = 0; head < 2; ++head)
        {
            const double sigma = input.headArealDensity[head];
            const double omega = input.headAngularFrequencies[head][radial];
            if (! positiveFinite (sigma) || ! positiveFinite (omega))
                return false;
            const auto coordinate = head * radialModeCount + radial;
            const double mass = sigma * area * j1 * j1;
            result.mass[coordinate][coordinate] = mass;
            result.stiffness[coordinate][coordinate] = mass * omega * omega;
        }
    }

    // Ritz displacement: u(z)=(1-z/L)u0+(z/L)uL+sum a_j sin(j*pi*z/L),
    // with u0=-sqrt(eta)*sum g_n qBn, uL=+sqrt(eta)*sum g_n qRn.
    // Galerkin energies T=rho*A/2 int(udot^2 dz), V=rho*c^2*A/2 int(uz^2 dz)
    // give the following EXACT integrals for this truncated basis:
    // Mends=rho*A*L/6 [[2,1],[1,2]], Kends=rho*c^2*A/L [[1,-1],[-1,1]],
    // M0j=rho*A*L/(j*pi), MLj=(-1)^(j+1)*M0j, Mjj=rho*A*L/2,
    // Kjj=rho*c^2*A*(j*pi)^2/(2L); other sine/sine and end/sine K terms vanish.
    // These are a 1-D energy reduction, not formulas claimed from the barrel
    // paper. The head mass integral A*J1(lambda)^2 is its Eq. 12.
    const double airMass = input.airDensity * area * input.depth;
    const double airSpring = input.airDensity * input.soundSpeed * input.soundSpeed
                           * area / input.depth;
    for (std::size_t i = 0; i < headCoordinateCount; ++i)
        for (std::size_t j = 0; j <= i; ++j)
        {
            result.mass[i][j] += airMass / 6.0
                * (2.0 * left[i] * left[j] + left[i] * right[j]
                   + right[i] * left[j] + 2.0 * right[i] * right[j]);
            result.stiffness[i][j] += airSpring
                * (right[i] - left[i]) * (right[j] - left[j]);
            result.mass[j][i] = result.mass[i][j];
            result.stiffness[j][i] = result.stiffness[i][j];
        }
    for (std::size_t axial = 0; axial < input.axialModeCount; ++axial)
    {
        const auto coordinate = headCoordinateCount + axial;
        const double waveNumber = static_cast<double> (axial + 1) * pi;
        const double endSign = axial % 2 == 0 ? 1.0 : -1.0;
        result.mass[coordinate][coordinate] = airMass / 2.0;
        result.stiffness[coordinate][coordinate] = airSpring * waveNumber * waveNumber / 2.0;
        for (std::size_t head = 0; head < headCoordinateCount; ++head)
        {
            const double crossMass = airMass / waveNumber
                                   * (left[head] + endSign * right[head]);
            result.mass[head][coordinate] = result.mass[coordinate][head] = crossMass;
        }
    }
    for (std::size_t i = 0; i < result.coordinateCount; ++i)
        for (std::size_t j = 0; j < result.coordinateCount; ++j)
            if (! std::isfinite (result.mass[i][j])
                || ! std::isfinite (result.stiffness[i][j]))
                return false;
    return true;
}

bool eigensolve (Solution& result) noexcept
{
    const auto count = result.coordinateCount;
    Vector scale {};
    Matrix lower {}, inverseLower {}, transformed {}, symmetric {}, rotation {};
    // Equilibrate before Cholesky: head coordinates and acoustic coordinates
    // can carry very different masses, especially on small or deep drums.
    for (std::size_t i = 0; i < count; ++i)
    {
        if (! positiveFinite (result.mass[i][i]))
            return false;
        scale[i] = 1.0 / std::sqrt (result.mass[i][i]);
        rotation[i][i] = 1.0;
    }
    for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = 0; j <= i; ++j)
        {
            double value = result.mass[i][j] * scale[i] * scale[j];
            for (std::size_t k = 0; k < j; ++k)
                value -= lower[i][k] * lower[j][k];
            if (i == j)
            {
                if (! positiveFinite (value))
                    return false;
                lower[i][j] = std::sqrt (value);
            }
            else
                lower[i][j] = value / lower[j][j];
        }
    for (std::size_t column = 0; column < count; ++column)
        for (std::size_t i = column; i < count; ++i)
        {
            double value = i == column ? 1.0 : 0.0;
            for (std::size_t j = column; j < i; ++j)
                value -= lower[i][j] * inverseLower[j][column];
            inverseLower[i][column] = value / lower[i][i];
        }
    for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = 0; j < count; ++j)
            for (std::size_t k = 0; k < count; ++k)
                transformed[i][j] += inverseLower[i][k] * scale[k]
                                   * result.stiffness[k][j] * scale[j];
    for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = 0; j <= i; ++j)
        {
            double value = 0.0;
            for (std::size_t k = 0; k < count; ++k)
                value += transformed[i][k] * inverseLower[j][k];
            symmetric[i][j] = symmetric[j][i] = value;
        }

    // Cyclic symmetric Jacobi keeps orthogonal eigenvectors even at exact
    // head degeneracies. A fixed sweep cap bounds setup cost; failure is visible.
    bool converged = false;
    for (unsigned sweep = 0; sweep < 40; ++sweep)
    {
        bool changed = false;
        for (std::size_t p = 0; p < count; ++p)
            for (std::size_t q = p + 1; q < count; ++q)
            {
                const double offDiagonal = symmetric[p][q];
                const double tolerance = 2.0e-14 * std::sqrt (
                    std::abs (symmetric[p][p])) * std::sqrt (std::abs (symmetric[q][q]));
                if (std::abs (offDiagonal) <= tolerance)
                    continue;
                changed = true;
                const double tau = (symmetric[q][q] - symmetric[p][p])
                                 / (2.0 * offDiagonal);
                const double tangent = std::copysign (
                    1.0 / (std::abs (tau) + std::hypot (1.0, tau)), tau);
                const double cosine = 1.0 / std::sqrt (1.0 + tangent * tangent);
                const double sine = tangent * cosine;
                symmetric[p][p] -= tangent * offDiagonal;
                symmetric[q][q] += tangent * offDiagonal;
                symmetric[p][q] = symmetric[q][p] = 0.0;
                for (std::size_t k = 0; k < count; ++k)
                {
                    if (k != p && k != q)
                    {
                        const double kp = symmetric[k][p], kq = symmetric[k][q];
                        symmetric[k][p] = symmetric[p][k] = cosine * kp - sine * kq;
                        symmetric[k][q] = symmetric[q][k] = sine * kp + cosine * kq;
                    }
                    const double rp = rotation[k][p], rq = rotation[k][q];
                    rotation[k][p] = cosine * rp - sine * rq;
                    rotation[k][q] = sine * rp + cosine * rq;
                }
            }
        result.jacobiSweeps = sweep + 1;
        if (! changed)
        {
            converged = true;
            break;
        }
    }
    if (! converged)
        return false;

    std::array<std::size_t, maximumCoordinates> order {};
    std::iota (order.begin(), order.end(), std::size_t { 0 });
    std::sort (order.begin(), order.begin() + static_cast<std::ptrdiff_t> (count),
               [&symmetric] (std::size_t a, std::size_t b)
               { return symmetric[a][a] < symmetric[b][b]; });
    for (std::size_t mode = 0; mode < count; ++mode)
    {
        const auto column = order[mode];
        if (! positiveFinite (symmetric[column][column]))
            return false;
        result.angularFrequencies[mode] = std::sqrt (symmetric[column][column]);
        std::size_t largest = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            double value = 0.0;
            for (std::size_t k = 0; k < count; ++k)
                value += inverseLower[k][i] * rotation[k][column];
            result.eigenvectors[i][mode] = scale[i] * value;
            if (std::abs (result.eigenvectors[i][mode])
                > std::abs (result.eigenvectors[largest][mode]))
                largest = i;
        }
        // Deterministic sign for serialization/debug comparisons. A repeated
        // eigenvalue still admits rotations; state transfer must use M projection.
        if (result.eigenvectors[largest][mode] < 0.0)
            for (std::size_t i = 0; i < count; ++i)
                result.eigenvectors[i][mode] = -result.eigenvectors[i][mode];
    }
    return true;
}
} // namespace

bool solve (const Input& input, Solution& result) noexcept
{
    result = {};
    return assemble (input, result) && eigensolve (result);
}
} // namespace taikor::cavity
