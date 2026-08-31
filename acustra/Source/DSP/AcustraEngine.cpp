#include "AcustraEngine.h"
#include "MeasuredBodyData.h"
#if defined(ACUSTRA_MEASURED_BRIDGE_DATA_HEADER)
#include ACUSTRA_MEASURED_BRIDGE_DATA_HEADER
#else
#include "MeasuredBridgeData.h"
#endif

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

namespace acustra
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;
constexpr float bridgePositionFraction = 0.995f;
constexpr int localMaximumDelaySamples = 8192;
// The g21 minimum-phase residues retain the calibrated H1 coefficient scale
// and the measured treble/bass microphone balance. The string drive remains a
// reference-scaled bridge-force proxy, so this is an output calibration rather
// than an absolute-SPL claim; it changes no spectrum, decay, or coupling.
// Overall level is excluded from the descriptor fit. This fixed calibration
// places the quietest public physical render just above -20 dBFS while keeping
// ordinary output well below the limiter's -1 dBFS knee.
constexpr float radiationReferenceGain = 18.0f;
static_assert(detail::measuredBodyModes.size() == 96);
static_assert(detail::measuredBridgeModes.size() == ACUSTRA_BRIDGE_MODE_COUNT);

struct ShapeSpec
{
    float airHz;
    float modeScale;
    float bass;
    float volume;
    float asymmetry;
};

constexpr std::array<ShapeSpec, 4> shapeSpecs {{
    { 112.0f, 1.045f, 0.90f, 1.11f, -0.012f }, // parlor
    { 107.0f, 1.000f, 0.98f, 1.00f,  0.000f }, // auditorium/reference
    { 101.0f, 0.972f, 1.08f, 0.97f,  0.009f }, // dreadnought
    {  96.0f, 0.948f, 1.14f, 0.90f,  0.015f }  // jumbo
}};

struct WoodSpec
{
    float frequencyScale;
    float qScale;
    float brightness;
    float radiation;
};

// Effective plate directions, not species-identification claims.  Density,
// stiffness and loss do not reduce to a single "tonewood" number; keeping four
// bounded directions is more honest than attaching exact woods to unsupported
// impulse responses.  The relationship f ~ sqrt(E/rho) and modal loss scaling
// are the only laws used here.
constexpr std::array<WoodSpec, 4> woodSpecs {{
    { 1.000f, 1.00f, 1.00f, 1.00f }, // spruce reference
    { 0.982f, 0.88f, 0.93f, 1.04f }, // cedar: softer/more damped direction
    { 0.991f, 0.82f, 0.87f, 1.02f }, // mahogany: lossier/warmer direction
    { 1.025f, 1.08f, 1.08f, 0.96f }  // maple: stiffer/brighter direction
}};

float safetyLimit(float sample) noexcept
{
    // Exactly linear through -1 dBFS, then C1-continuous into unit headroom.
    // The former zero-centred knee altered every ordinary guitar transient.
    constexpr float threshold = 0.89125094f;
    constexpr float headroom = 1.0f - threshold;
    const float magnitude = std::abs(sample);
    if (magnitude <= threshold)
        return sample;
    const float excess = magnitude - threshold;
    const float limited = threshold
        + excess / (1.0f + excess / headroom);
    return std::copysign(limited, sample);
}

constexpr std::array<float, AcustraEngine::stringCount> steelDiameterMetres {{
    1.346e-3f, 1.067e-3f, 0.813e-3f, 0.610e-3f, 0.406e-3f, 0.305e-3f
}};

// D'Addario EJ16 published tensions, low E to high E, converted from pounds
// force.  Wound-string mass cannot be inferred from its outside diameter as a
// solid steel cylinder; tension and scale length give the effective linear
// mass that the waveguide actually needs.
constexpr std::array<float, AcustraEngine::stringCount> steelTensionNewtons {{
    110.759f, 128.554f, 133.002f, 133.892f, 103.643f, 104.088f
}};

constexpr std::array<int, AcustraEngine::stringCount> standardOpenMidi {{
    40, 45, 50, 55, 59, 64
}};

// Wound-string bending is governed mostly by a smaller core than the outside
// diameter.  The low E/A/D effective diameters reproduce the measured open-
// string inharmonicities 1.08e-4, 6.6e-5 and 5.0e-5 reported by Jarvelainen
// and Karjalainen (Acta Acustica 92, 2006); shortening the same construction
// then predicts their seventh-fret values.  G remains an authored effective
// diameter because that paper does not publish a matching value for it.
constexpr std::array<float, AcustraEngine::stringCount> steelBendingDiameter {{
    0.477159e-3f, 0.437895e-3f, 0.412021e-3f,
    0.38e-3f, 0.406e-3f, 0.305e-3f
}};

constexpr std::array<float, AcustraEngine::stringCount> nylonDiameterMetres {{
    1.15e-3f, 0.97e-3f, 0.79e-3f, 1.00e-3f, 0.82e-3f, 0.67e-3f
}};

// DAFx-26 Table 1's EJ45 construction data, reversed from its high-E-to-low-E
// presentation into engine order. The three wound basses use effective
// densities rather than pretending their wrap/core composite is homogeneous
// plain nylon. https://dafx26.mit.edu/assets/papers/DAFx26_paper_40.pdf
constexpr std::array<float, AcustraEngine::stringCount> nylonDensity {{
    5900.0f, 5200.0f, 4500.0f, 1140.0f, 1140.0f, 1140.0f
}};

constexpr std::array<float, AcustraEngine::stringCount> nylonYoungsModulus {{
    2.5e9f, 2.5e9f, 2.5e9f, 2.7e9f, 2.7e9f, 2.7e9f
}};

constexpr float steelYoungsModulus = 2.0e11f;

float stringImpedance(bool steel, int stringIndex, int openMidi) noexcept
{
    const auto index = static_cast<std::size_t>(stringIndex);
    const float frequency = 440.0f
        * std::exp2((static_cast<float>(openMidi) - 69.0f) / 12.0f);
    const float length = steel ? 0.648f : 0.650f;
    const float waveSpeed = 2.0f * length * frequency;
    if (steel)
    {
        const float standardFrequency = 440.0f * std::exp2(
            (static_cast<float>(standardOpenMidi[index]) - 69.0f) / 12.0f);
        const float standardWaveSpeed = 2.0f * length * standardFrequency;
        if (openMidi == standardOpenMidi[index])
            return steelTensionNewtons[index] / standardWaveSpeed;
        const float linearMass = steelTensionNewtons[index]
                               / (standardWaveSpeed * standardWaveSpeed);
        return linearMass * waveSpeed;
    }
    const float diameter = nylonDiameterMetres[index];
    const float linearMass = nylonDensity[index] * pi * 0.25f
                           * diameter * diameter;
    return linearMass * waveSpeed;
}

bool includeMeasuredBridgeMode(const detail::MeasuredBridgeMode& mode) noexcept
{
#if defined(ACUSTRA_ANALYSIS_EXCLUDE_MEASURED_OPEN_STRINGS)
    // Analysis only: the g21 setup photographs show installed strings.  Of
    // the retained candidates only 82.764 Hz lies within 25 cents of a
    // standard open string (E2, 82.407 Hz).  Do not ship this de-embedding
    // surrogate: the measurement string's impedance metadata is unavailable.
    constexpr float openLowE = 82.406889f;
    const float cents = 1200.0f * std::log2(mode.frequency / openLowE);
    return std::abs(cents) >= 25.0f;
#else
    (void) mode;
    return true;
#endif
}

// The plate conductance floor is one over-damped positive-real section. An
// over-damped s/(s^2+2ds+w0^2) has real poles at w0^2/(2d) and 2d, so its
// conductance is flat between them; centre sqrt(f_low*f_high) with
// q = sqrt(f_low/f_high) places those poles at f_low and f_high, and
// weight = G*2*pi*f_high makes the plateau conductance equal G. The upper
// limit is fixed above the 10 kHz measurement band it extrapolates.
constexpr float plateConductanceUpperHz = 16000.0f;

struct PlateConductanceMode
{
    float frequency;
    float q;
    float weight;
};

PlateConductanceMode plateConductanceMode(
    const PhysicalCalibration& calibration) noexcept
{
    const float low = std::clamp(calibration.bridgeConductanceCornerHz,
                                 100.0f, 8000.0f);
    return { std::sqrt(low * plateConductanceUpperHz),
             std::sqrt(low / plateConductanceUpperHz),
             calibration.bridgeConductanceFloor * twoPi
                 * plateConductanceUpperHz };
}

int wrapDelayIndex(int index) noexcept
{
    while (index < 0)
        index += localMaximumDelaySamples;
    while (index >= localMaximumDelaySamples)
        index -= localMaximumDelaySamples;
    return index;
}

bool sameDiscreteConstruction(const EngineParameters& a,
                              const EngineParameters& b) noexcept
{
    return a.shape == b.shape
        && a.bodyMaterial == b.bodyMaterial
        && a.stringMaterial == b.stringMaterial
        && a.tuning == b.tuning;
}

struct DispersionCalibration
{
    double delay { 128.0 };
    double decayRatio { 10.0 };
    double poleRatio { 4.0 };
    double a1 { 0.0 };
    double a2 { 0.0 };
};

double mixedOnePolePhase(double coefficient, double mix,
                         double omega) noexcept
{
    const double cosine = std::cos(omega);
    const double sine = std::sin(omega);
    const double denominatorReal = 1.0 - coefficient * cosine;
    const double denominatorImaginary = coefficient * sine;
    const double denominatorNorm = denominatorReal * denominatorReal
                                 + denominatorImaginary * denominatorImaginary;
    const double lowReal = (1.0 - coefficient) * denominatorReal
                         / denominatorNorm;
    const double lowImaginary = -(1.0 - coefficient) * denominatorImaginary
                              / denominatorNorm;
    return -std::atan2(mix * lowImaginary,
                       (1.0 - mix) + mix * lowReal);
}

double catmullRomDelayPhase(double samples, double omega) noexcept
{
    const double bounded = std::clamp(
        samples, 3.0, static_cast<double>(localMaximumDelaySamples - 3));
    const int whole = static_cast<int>(bounded);
    const double fraction = bounded - static_cast<double>(whole);
    const double squared = fraction * fraction;
    const double cubed = squared * fraction;
    const double weights[] {
        -0.5 * fraction + squared - 0.5 * cubed,
         1.0 - 2.5 * squared + 1.5 * cubed,
         0.5 * fraction + 2.0 * squared - 1.5 * cubed,
        -0.5 * squared + 0.5 * cubed
    };
    const double kernelReal = weights[0] * std::cos(omega) + weights[1]
                            + weights[2] * std::cos(omega)
                            + weights[3] * std::cos(2.0 * omega);
    const double kernelImaginary = weights[0] * std::sin(omega)
                                 - weights[2] * std::sin(omega)
                                 - weights[3] * std::sin(2.0 * omega);
    return static_cast<double>(whole) * omega
         - std::atan2(kernelImaginary, kernelReal);
}

void secondOrderAllpassCoefficients(double omega, double decayRatio,
                                    double poleRatio,
                                    double& a1, double& a2) noexcept
{
    const double radius = std::exp(-omega * decayRatio);
    const double angle = omega * poleRatio;
    a1 = -2.0 * radius * std::cos(angle);
    a2 = radius * radius;
}

double secondOrderAllpassPhase(double a1, double a2,
                               double omega) noexcept
{
    const double cosine = std::cos(omega);
    const double sine = std::sin(omega);
    const double cosine2 = std::cos(2.0 * omega);
    const double sine2 = std::sin(2.0 * omega);
    const double numeratorPhase = std::atan2(
        -a1 * sine - sine2, a2 + a1 * cosine + cosine2);
    const double denominatorPhase = std::atan2(
        -a1 * sine - a2 * sine2, 1.0 + a1 * cosine + a2 * cosine2);
    double lag = denominatorPhase - numeratorPhase;
    constexpr double twoPiDouble = 2.0 * 3.14159265358979323846;
    while (lag < 0.0)
        lag += twoPiDouble;
    while (lag >= twoPiDouble)
        lag -= twoPiDouble;
    return lag;
}

double tunedCatmullDelay(double fundamental, double sampleRate,
                         double broadCoefficient, double broadMix,
                         double highCoefficient, double highMix,
                         double a1, double a2) noexcept
{
    constexpr double twoPiDouble = 2.0 * 3.14159265358979323846;
    const double omega = twoPiDouble * fundamental / sampleRate;
    const double fixedPhase = mixedOnePolePhase(
        broadCoefficient, broadMix, omega)
        + mixedOnePolePhase(highCoefficient, highMix, omega)
        + secondOrderAllpassPhase(a1, a2, omega);
    double delay = std::clamp(sampleRate / fundamental - fixedPhase / omega,
                              3.0,
                              static_cast<double>(localMaximumDelaySamples - 3));
    for (int iteration = 0; iteration < 6; ++iteration)
    {
        const double residual = catmullRomDelayPhase(delay, omega)
                              + fixedPhase - twoPiDouble;
        if (std::abs(residual) < 1.0e-11)
            break;
        constexpr double step = 0.01;
        const double slope = (catmullRomDelayPhase(delay + step, omega)
                            - catmullRomDelayPhase(delay - step, omega))
                           / (2.0 * step);
        if (std::abs(slope) < 1.0e-12)
            break;
        delay = std::clamp(delay - residual / slope, 3.0,
            static_cast<double>(localMaximumDelaySamples - 3));
    }
    return delay;
}

bool solveThreeByThree(double matrix[3][3], const double rhs[3],
                       double result[3]) noexcept
{
    double augmented[3][4] {};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
            augmented[row][column] = matrix[row][column];
        augmented[row][3] = rhs[row];
    }

    for (int column = 0; column < 3; ++column)
    {
        int pivot = column;
        for (int row = column + 1; row < 3; ++row)
            if (std::abs(augmented[row][column])
                > std::abs(augmented[pivot][column]))
                pivot = row;
        if (std::abs(augmented[pivot][column]) < 1.0e-13)
            return false;
        if (pivot != column)
            for (int item = column; item < 4; ++item)
                std::swap(augmented[column][item], augmented[pivot][item]);
        for (int row = column + 1; row < 3; ++row)
        {
            const double factor = augmented[row][column]
                                / augmented[column][column];
            for (int item = column; item < 4; ++item)
                augmented[row][item] -= factor * augmented[column][item];
        }
    }

    for (int row = 2; row >= 0; --row)
    {
        double value = augmented[row][3];
        for (int column = row + 1; column < 3; ++column)
            value -= augmented[row][column] * result[column];
        result[row] = value / augmented[row][row];
    }
    return true;
}

DispersionCalibration calibrateDispersion(
    double inharmonicity, double fundamental, double sampleRate,
    double broadCoefficient, double broadMix,
    double highCoefficient, double highMix,
    double initialDecayRatio, double initialPoleRatio) noexcept
{
    constexpr double piDouble = 3.14159265358979323846;
    constexpr double twoPiDouble = 2.0 * piDouble;
    const double omega0 = twoPiDouble * fundamental / sampleRate;
    DispersionCalibration calibration;
    calibration.decayRatio = std::clamp(initialDecayRatio, 0.1, 30.0);
    calibration.poleRatio = std::clamp(initialPoleRatio, 0.05, 15.0);

    if (!(inharmonicity > 1.0e-8) || !(omega0 > 1.0e-7))
    {
        calibration.a1 = 0.0;
        calibration.a2 = 0.0;
        calibration.delay = tunedCatmullDelay(
            fundamental, sampleRate, broadCoefficient, broadMix,
            highCoefficient, highMix, calibration.a1, calibration.a2);
        return calibration;
    }

    const auto stretchedOmega = [=] (double partial)
    {
        return omega0 * partial * std::sqrt(
            (1.0 + inharmonicity * partial * partial)
            / (1.0 + inharmonicity));
    };
    int highest = 12;
    while (highest > 3 && stretchedOmega(highest) > 0.84 * piDouble)
        --highest;
    // Three loop-phase collocation points determine delay plus the stable
    // complex pole pair. They are H1/H7/H11.5 throughout the normal 44.1
    // kHz-and-up guitar range; placing the upper point half a partial inward
    // distributes the approximation error over the integer partials instead
    // of spending exactness at H12. The upper two move down with Nyquist.
    const double anchors[] {
        1.0, static_cast<double>(1 + highest / 2),
        static_cast<double>(highest) - 0.5
    };

    double parameters[] {
        128.0, calibration.decayRatio, calibration.poleRatio
    };
    secondOrderAllpassCoefficients(
        omega0, parameters[1], parameters[2], calibration.a1, calibration.a2);
    parameters[0] = tunedCatmullDelay(
        fundamental, sampleRate, broadCoefficient, broadMix,
        highCoefficient, highMix, calibration.a1, calibration.a2);

    const auto evaluate = [&] (const double values[3], double residuals[3])
    {
        double a1 = 0.0;
        double a2 = 0.0;
        secondOrderAllpassCoefficients(
            omega0, values[1], values[2], a1, a2);
        for (int index = 0; index < 3; ++index)
        {
            const double partial = anchors[index];
            const double omega = stretchedOmega(partial);
            residuals[index] = catmullRomDelayPhase(values[0], omega)
                + mixedOnePolePhase(broadCoefficient, broadMix, omega)
                + mixedOnePolePhase(highCoefficient, highMix, omega)
                + secondOrderAllpassPhase(a1, a2, omega)
                - twoPiDouble * partial;
        }
    };
    const auto maximumResidual = [] (const double residuals[3])
    {
        return std::max({ std::abs(residuals[0]), std::abs(residuals[1]),
                          std::abs(residuals[2]) });
    };

    constexpr double steps[] { 0.02, 0.01, 0.01 };
    for (int iteration = 0; iteration < 9; ++iteration)
    {
        double residuals[3] {};
        evaluate(parameters, residuals);
        const double oldNorm = maximumResidual(residuals);
        if (oldNorm < 1.0e-10)
            break;

        double jacobian[3][3] {};
        for (int column = 0; column < 3; ++column)
        {
            double higher[] { parameters[0], parameters[1], parameters[2] };
            double lower[] { parameters[0], parameters[1], parameters[2] };
            higher[column] += steps[column];
            lower[column] -= steps[column];
            double higherResiduals[3] {};
            double lowerResiduals[3] {};
            evaluate(higher, higherResiduals);
            evaluate(lower, lowerResiduals);
            for (int row = 0; row < 3; ++row)
                jacobian[row][column] = (higherResiduals[row]
                    - lowerResiduals[row]) / (2.0 * steps[column]);
        }

        const double rhs[] { -residuals[0], -residuals[1], -residuals[2] };
        double update[3] {};
        if (!solveThreeByThree(jacobian, rhs, update))
            break;

        bool accepted = false;
        for (double amount = 1.0; amount >= 0.03125; amount *= 0.5)
        {
            double candidate[] {
                std::clamp(parameters[0] + amount * update[0], 3.0,
                    static_cast<double>(localMaximumDelaySamples - 3)),
                std::clamp(parameters[1] + amount * update[1], 0.1, 30.0),
                std::clamp(parameters[2] + amount * update[2], 0.05, 15.0)
            };
            double candidateResiduals[3] {};
            evaluate(candidate, candidateResiduals);
            if (maximumResidual(candidateResiduals) < oldNorm)
            {
                std::copy(std::begin(candidate), std::end(candidate), parameters);
                accepted = true;
                break;
            }
        }
        if (!accepted)
            break;
    }

    calibration.delay = parameters[0];
    calibration.decayRatio = parameters[1];
    calibration.poleRatio = parameters[2];
    secondOrderAllpassCoefficients(
        omega0, parameters[1], parameters[2], calibration.a1, calibration.a2);
    return calibration;
}
} // namespace

AcustraEngine::AcustraEngine() noexcept
{
    parameters_ = sanitise(parameters_);
    targetParameters_ = parameters_;
    const auto notes = openNotes(parameters_.tuning);
    for (int string = 0; string < stringCount; ++string)
    {
        auto& voice = voices_[static_cast<std::size_t>(string)];
        voice.openMidi = notes[static_cast<std::size_t>(string)];
        voice.midiNote = voice.openMidi;
        voice.randomState = 0x9e3779b9u
            ^ (0x85ebca6bu * static_cast<std::uint32_t>(string + 1));
    }
}

float AcustraEngine::clamp(float value, float low, float high) noexcept
{
    if (!std::isfinite(value))
        return low;
    return std::max(low, std::min(high, value));
}

EngineParameters AcustraEngine::sanitise(const EngineParameters& source) noexcept
{
    EngineParameters result = source;
    const auto enumOr = [] (int value, int maximum, int fallback)
    {
        return value >= 0 && value <= maximum ? value : fallback;
    };
    result.shape = static_cast<BodyShape>(enumOr(
        static_cast<int>(source.shape), 3,
        static_cast<int>(EngineParameters {}.shape)));
    result.bodyMaterial = static_cast<BodyMaterial>(enumOr(
        static_cast<int>(source.bodyMaterial), 3,
        static_cast<int>(EngineParameters {}.bodyMaterial)));
    result.stringMaterial = static_cast<StringMaterial>(enumOr(
        static_cast<int>(source.stringMaterial), 1,
        static_cast<int>(EngineParameters {}.stringMaterial)));
    result.tuning = static_cast<Tuning>(enumOr(
        static_cast<int>(source.tuning), 4,
        static_cast<int>(EngineParameters {}.tuning)));
    result.stringAge = clamp(source.stringAge, 0.0f, 1.0f);
    result.pluckPosition = clamp(source.pluckPosition, 0.0f, 1.0f);
    result.touch = clamp(source.touch, 0.0f, 1.0f);
    result.bodyAmount = clamp(source.bodyAmount, 0.0f, 1.0f);
    result.stereoWidth = clamp(source.stereoWidth, 0.0f, 1.0f);
    result.outputGain = clamp(source.outputGain, 0.0f, 4.0f);
    return result;
}

PhysicalCalibration AcustraEngine::sanitise(
    const PhysicalCalibration& source) noexcept
{
    const auto bounded = [] (float value, float low, float high,
                             float fallback) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    };
    const auto material = [&] (const MaterialCalibration& value,
                               const MaterialCalibration& fallback) noexcept
    {
        return MaterialCalibration {
            bounded(value.stiffnessScale, 0.25f, 4.0f,
                    fallback.stiffnessScale),
            bounded(value.fundamentalT60Scale, 0.4f, 2.0f,
                    fallback.fundamentalT60Scale),
            bounded(value.frequencyLossScale, 0.35f, 3.0f,
                    fallback.frequencyLossScale),
            bounded(value.apertureScale, 0.35f, 2.5f,
                    fallback.apertureScale),
            bounded(value.transientScale, 0.0f, 3.0f,
                    fallback.transientScale),
            bounded(value.pluckDistanceScale, 0.7f, 1.3f,
                    fallback.pluckDistanceScale),
            bounded(value.velocityBrightnessDepth, 0.0f, 1.2f,
                    fallback.velocityBrightnessDepth)
        };
    };

    return {
        bounded(source.bodyFrequencyScale, 0.96f, 1.04f,
                fittedPhysicalCalibration.bodyFrequencyScale),
        bounded(source.bodyQScale, 0.05f, 1.8f,
                fittedPhysicalCalibration.bodyQScale),
        bounded(source.bridgeMobilityScale, 0.25f, 4.0f,
                fittedPhysicalCalibration.bridgeMobilityScale),
        bounded(source.residueTiltDbPerOctave, -6.0f, 6.0f,
                fittedPhysicalCalibration.residueTiltDbPerOctave),
        bounded(source.directGain, 0.0f, 0.12f,
                fittedPhysicalCalibration.directGain),
        material(source.nylon, fittedPhysicalCalibration.nylon),
        material(source.steel, fittedPhysicalCalibration.steel),
        bounded(source.apertureRegisterExponent, -1.0f, 1.0f,
                fittedPhysicalCalibration.apertureRegisterExponent),
        bounded(source.lowBodyModeGain, 0.25f, 32.0f,
                fittedPhysicalCalibration.lowBodyModeGain),
        bounded(source.steelDisplacementScaleMetres, 0.0f, 0.04f,
                fittedPhysicalCalibration.steelDisplacementScaleMetres),
        bounded(source.steelFretT60Slope, -0.06f, 0.05f,
                fittedPhysicalCalibration.steelFretT60Slope),
        bounded(source.highLossCutoffScale, 0.5f, 4.0f,
                fittedPhysicalCalibration.highLossCutoffScale),
        bounded(source.bridgeConductanceFloor, 0.0f, 0.02f,
                fittedPhysicalCalibration.bridgeConductanceFloor),
        bounded(source.bridgeConductanceCornerHz, 100.0f, 8000.0f,
                fittedPhysicalCalibration.bridgeConductanceCornerHz)
    };
}

std::array<int, AcustraEngine::stringCount>
AcustraEngine::openNotes(Tuning tuning) noexcept
{
    switch (tuning)
    {
        case Tuning::DropD:        return { 38, 45, 50, 55, 59, 64 };
        case Tuning::Dadgad:       return { 38, 45, 50, 55, 57, 62 };
        case Tuning::OpenG:        return { 38, 43, 50, 55, 59, 62 };
        case Tuning::HalfStepDown: return { 39, 44, 49, 54, 58, 63 };
        case Tuning::Standard:     return { 40, 45, 50, 55, 59, 64 };
    }
    return { 40, 45, 50, 55, 59, 64 };
}

float AcustraEngine::midiFrequency(int midiNote) noexcept
{
    return 440.0f * std::exp2((static_cast<float>(midiNote) - 69.0f) / 12.0f);
}

float AcustraEngine::phaseDelayForOnePoleMix(float coefficient, float mix,
                                             float omega) noexcept
{
    if (!(omega > 1.0e-7f))
        return mix * coefficient / std::max(1.0f - coefficient, 1.0e-5f);

    // H = (1-m) + m(1-c)/(1-c z^-1), evaluated directly so the
    // fractional-delay target includes the loss filter's actual phase.
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    const float denominatorReal = 1.0f - coefficient * cosine;
    const float denominatorImag = coefficient * sine;
    const float denominatorNorm = denominatorReal * denominatorReal
                                + denominatorImag * denominatorImag;
    const float lowReal = (1.0f - coefficient) * denominatorReal
                        / denominatorNorm;
    const float lowImag = -(1.0f - coefficient) * denominatorImag
                        / denominatorNorm;
    const float real = (1.0f - mix) + mix * lowReal;
    const float imaginary = mix * lowImag;
    return -std::atan2(imaginary, real) / omega;
}

float AcustraEngine::magnitudeForOnePoleMix(float coefficient, float mix,
                                            float omega) noexcept
{
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    const float denominatorReal = 1.0f - coefficient * cosine;
    const float denominatorImag = coefficient * sine;
    const float denominatorNorm = denominatorReal * denominatorReal
                                + denominatorImag * denominatorImag;
    const float lowReal = (1.0f - coefficient) * denominatorReal
                        / denominatorNorm;
    const float lowImag = -(1.0f - coefficient) * denominatorImag
                        / denominatorNorm;
    return std::hypot((1.0f - mix) + mix * lowReal,
                      mix * lowImag);
}

float AcustraEngine::registeredPluckAperture(
    float apertureSamples, float apertureScale, float referenceDelay,
    float currentReferenceLength, float exponent) noexcept
{
    // Keep the promoted exponent-one model bit-for-bit on its original path.
    if (exponent == 1.0f)
        return apertureSamples * apertureScale
            / std::max(currentReferenceLength, 8.0f);

    const float boundedLength = std::max(currentReferenceLength, 8.0f);
    return apertureSamples * apertureScale / referenceDelay
        * std::pow(referenceDelay / boundedLength, exponent);
}

void AcustraEngine::StringLoop::reset() noexcept
{
    delay.fill(0.0f);
    writeIndex = 0;
    broadLossFilter.reset();
    lossFilter.reset();
    dispersion.reset();
    bridgeDerivative.reset();
}

float AcustraEngine::FixedDerivative::process(float input,
                                              float sampleRateRatio) noexcept
{
    history[static_cast<std::size_t>(index)] = input;
    // sampleRateRatio samples span exactly the 48 kHz reference period, so
    // this finite difference already has one host-rate-independent scale.
    const float historyDelay = AcustraEngine::clamp(
        sampleRateRatio, 0.1f, 8.0f);
    const int whole = static_cast<int>(historyDelay);
    const float fraction = historyDelay - static_cast<float>(whole);
    const auto at = [&] (int samplesAgo)
    {
        int readIndex = index - samplesAgo;
        while (readIndex < 0)
            readIndex += static_cast<int>(history.size());
        return history[static_cast<std::size_t>(readIndex)];
    };
    const float first = at(whole);
    const float delayed = first + fraction * (at(whole + 1) - first);
    index = (index + 1) % static_cast<int>(history.size());
    return input - delayed;
}

float AcustraEngine::StringLoop::readDelay(float samples) const noexcept
{
    const float bounded = AcustraEngine::clamp(
        samples, 3.0f, static_cast<float>(maximumDelaySamples - 3));
    const int whole = static_cast<int>(bounded);
    const float fraction = bounded - static_cast<float>(whole);
    const auto at = [&] (int samplesAgo)
    {
        return delay[static_cast<std::size_t>(
            wrapDelayIndex(writeIndex - samplesAgo))];
    };

    // Four-point Catmull-Rom interpolation.  Its small overshoot is bounded by
    // the loop's sub-unity T60 gain; it is substantially less dispersive than
    // linear interpolation while a changing pitch remains state-free.
    const float p0 = at(whole - 1);
    const float p1 = at(whole);
    const float p2 = at(whole + 1);
    const float p3 = at(whole + 2);
    const float f2 = fraction * fraction;
    const float f3 = f2 * fraction;
    return 0.5f * ((2.0f * p1)
        + (-p0 + p2) * fraction
        + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * f2
        + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * f3);
}

float AcustraEngine::StringLoop::advance(float delaySmoothing,
                                         float releaseGain) noexcept
{
    currentDelay += delaySmoothing * (targetDelay - currentDelay);
    const float delayed = readDelay(currentDelay);
    const float broad = broadLossFilter.process(
        delayed, broadLossCoefficient);
    float reflected = delayed + broadLossMix * (broad - delayed);
    const float low = lossFilter.process(reflected, lowpassCoefficient);
    reflected += highLossMix * (low - reflected);
    reflected = dispersion.process(reflected, dispersionA1, dispersionA2);
    return reflected * loopGain * releaseGain;
}

void AcustraEngine::StringLoop::write(float value) noexcept
{
    if (!std::isfinite(value) || std::abs(value) < 1.0e-30f)
        value = 0.0f;
    delay[static_cast<std::size_t>(writeIndex)] = value;
    writeIndex = wrapDelayIndex(writeIndex + 1);
}

float AcustraEngine::BridgeMode::processPast(float input) noexcept
{
    const float output = numerator1 * input + numerator2 * input1
                       - denominator1 * output1
                       - denominator2 * output2;
    input1 = input;
    output2 = output1;
    output1 = output;
    return output;
}

void AcustraEngine::BridgeLoad::reset() noexcept
{
    delayedPastResponse = 0.0f;
    tailIntegratedForce = 0.0f;
    previousDisplacement = 0.0f;
    mainIntegratedForce = 0.0f;
    bodyIntegratedForce = 0.0f;
    for (auto& mode : modes)
        mode.reset();
}

float AcustraEngine::BridgeLoad::process(
    float incident, float characteristicAdmittance,
    float tailStiffness, float samplePeriod) noexcept
{
    // DAFx-26 attaches the measured body at xi_b=0.995, leaving a short
    // fixed-end string tail.  Below its first resonance that segment is the
    // passive spring K=T/((1-xi_b)L).  Trapezoidal integration gives the
    // spring's current-step impedance c=K*dt/2; solving it together with the
    // immediate mobility keeps the scattering junction algebraic-loop-free.
    const float springImpedance = 0.5f * tailStiffness * samplePeriod;
    const float tailHistory = tailIntegratedForce
                            + springImpedance * previousDisplacement;
    const float springScale = 1.0f
                            + characteristicAdmittance * springImpedance;
    const float denominator = characteristicAdmittance
                            + immediateAdmittance * springScale;
    if (!(denominator > 1.0e-8f) || !std::isfinite(incident))
    {
        reset();
        return -incident;
    }

    const float displacement = (delayedPastResponse
        + 2.0f * immediateAdmittance * incident
        - immediateAdmittance * characteristicAdmittance * tailHistory)
        / denominator;
    const float reflected = displacement - incident;
    if (!std::isfinite(reflected))
    {
        reset();
        return -incident;
    }

    const float nextTailIntegratedForce = tailHistory
                                        + springImpedance * displacement;
    const float bodyForceWave = 2.0f * incident - displacement
        - characteristicAdmittance * nextTailIntegratedForce;
    float nextPastResponse = 0.0f;
    for (auto& mode : modes)
        nextPastResponse += mode.processPast(bodyForceWave);
    delayedPastResponse = std::isfinite(nextPastResponse)
        ? nextPastResponse : 0.0f;
    previousDisplacement = displacement;
    tailIntegratedForce = nextTailIntegratedForce;
    const float portImpedance = 1.0f / characteristicAdmittance;
    mainIntegratedForce = portImpedance
                        * (2.0f * incident - displacement);
    bodyIntegratedForce = portImpedance * bodyForceWave;
    return reflected;
}

void AcustraEngine::prepare(double sampleRate, int)
{
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0)
        sampleRate = 48000.0;
    sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    delaySmoothing_ = 1.0f - std::exp(-1.0f
        / (0.006f * static_cast<float>(sampleRate_)));
    parameterSmoothing_ = 1.0f - std::exp(-1.0f
        / (0.020f * static_cast<float>(sampleRate_)));
    // The bridge hand is reconfigured at control rate, so its follower runs
    // there too; 15 ms is a hand arriving, not a step.
    palmMuteSmoothing_ = 1.0f - std::exp(-static_cast<float>(controlPeriod)
        / (0.015f * static_cast<float>(sampleRate_)));
    // Preserve the former 48 kHz pole as an 8.323 ms physical-time follower.
    levelSmoothing_ = -std::expm1(std::log1p(-0.0025f)
        * 48000.0f / static_cast<float>(sampleRate_));
    bodyModelFadeStep_ = 1.0f
        / (0.040f * static_cast<float>(sampleRate_));
    configureBridge();
    prepared_ = true;
    reset();
}

void AcustraEngine::reset() noexcept
{
    resetSoundState();
    palmMute_ = targetPalmMute_;
    pitchBendSemitones_.fill(0.0f);
    sustainPedals_.fill(false);
    noteOrder_ = 0;
    controlCounter_ = 0;
    parameters_ = sanitise(targetParameters_);
    bodyAmount_ = parameters_.bodyAmount;
    width_ = parameters_.stereoWidth;
    outputGain_ = parameters_.outputGain;
    bodyConfigured_ = false;
    configureBody();
    for (auto& mode : bodyModes_)
        mode.reset();
    for (auto& mode : fadingBodyModes_)
        mode.reset();
    const auto notes = openNotes(parameters_.tuning);
    for (int string = 0; string < stringCount; ++string)
    {
        auto& voice = voices_[static_cast<std::size_t>(string)];
        voice.openMidi = notes[static_cast<std::size_t>(string)];
        voice.ownerCount = 0;
        voice.played = false;
        voice.keyDown = false;
        voice.pedalHeld = false;
        voice.level = 0.0f;
        voice.quietSamples = 0;
        returnToOpenString(voice, string);
    }
    // Initialise every open-string loop. Unplayed strings are driven one-way
    // by the shared bridge and therefore add no return-impedance phase here;
    // the repeated pass also preserves the normal post-pluck update order.
    for (int pass = 0; pass < 2; ++pass)
        for (int string = 0; string < stringCount; ++string)
            configureVoice(voices_[static_cast<std::size_t>(string)], string,
                           voices_[static_cast<std::size_t>(string)].midiNote,
                           false);
}

void AcustraEngine::resetSoundState() noexcept
{
    bridgeLoad_.reset();
    bridgeVelocityDerivative_.reset();
    bridgeForceDerivative_.reset();
    bridgeBodyForceDerivative_.reset();
    bridgeTailForceDerivative_.reset();
    lastBridgeVelocity_ = 0.0f;
    lastBridgeReactionForce_ = 0.0f;
    lastBridgeBodyForce_ = 0.0f;
    lastBridgeTailForce_ = 0.0f;
    lastSympatheticRadiationForce_ = 0.0f;
    lastBridgePower_ = 0.0f;
    lastBridgeBodyPower_ = 0.0f;
    lastBridgeTailPower_ = 0.0f;
    bridgeDerivativesNeedPriming_ = true;
    lastImpedanceSum_ = 0.0f;
    lastTailStiffnessSum_ = 0.0f;
    for (auto& mode : bodyModes_)
        mode.reset();
    for (auto& mode : fadingBodyModes_)
        mode.reset();
}

void AcustraEngine::setParameters(const EngineParameters& parameters) noexcept
{
    targetParameters_ = sanitise(parameters);
    if (prepared_)
        applyDiscreteParameters(false);
}

void AcustraEngine::setPhysicalCalibration(
    const PhysicalCalibration& calibration) noexcept
{
    physicalCalibration_ = sanitise(calibration);
    if (!prepared_)
        return;
    configureBridge();
    reset();
}

void AcustraEngine::applyDiscreteParameters(bool force) noexcept
{
    const auto next = sanitise(targetParameters_);
    const bool constructionChanged = force
        || !sameDiscreteConstruction(next, parameters_);
    const bool bodyChanged = force || next.shape != parameters_.shape
        || next.bodyMaterial != parameters_.bodyMaterial;
    const bool ageChanged = force
        || std::abs(next.stringAge - parameters_.stringAge) > 1.0e-5f;
    const bool stringChanged = force
        || next.stringMaterial != parameters_.stringMaterial;
    const bool tuningChanged = force || next.tuning != parameters_.tuning;
    parameters_ = next;

    if (bodyChanged)
        configureBody();

    const auto notes = openNotes(parameters_.tuning);
    for (int string = 0; string < stringCount; ++string)
    {
        auto& voice = voices_[static_cast<std::size_t>(string)];
        const int newOpen = notes[static_cast<std::size_t>(string)];
        voice.openMidi = newOpen;
        if (stringChanged)
        {
            voice.attackPitchCents = 0.0f;
            voice.attackPitchDecay = 1.0f;
            voice.attackSlopeEnergy = 0.0f;
            voice.observedSlopeEnergy = 0.0f;
        }
        if (!voice.played && tuningChanged)
            returnToOpenString(voice, string);
        else if (constructionChanged || ageChanged || stringChanged)
            configureVoice(voice, string, voice.midiNote, false);
    }
    // A tail belongs to the construction it was taken from, and its loop is
    // not redesigned by the passes below. Drop it rather than let a steel
    // string ring on into a nylon instrument.
    if (constructionChanged || ageChanged || stringChanged || tuningChanged)
        for (auto& voice : voices_)
        {
            if (!voice.tailActive)
                continue;
            voice.tailActive = false;
            voice.tailLevel = 0.0f;
            voice.tailQuietSamples = 0;
            voice.tailLoop.reset();
        }

    if (constructionChanged || ageChanged || stringChanged || tuningChanged)
        for (int pass = 0; pass < 2; ++pass)
            for (int string = 0; string < stringCount; ++string)
                configureVoice(voices_[static_cast<std::size_t>(string)], string,
                               voices_[static_cast<std::size_t>(string)].midiNote,
                               false);
}

void AcustraEngine::updateControlState() noexcept
{
    palmMute_ += palmMuteSmoothing_ * (targetPalmMute_ - palmMute_);
    if (std::abs(targetPalmMute_ - palmMute_) < 1.0e-6f)
        palmMute_ = targetPalmMute_;
    applyDiscreteParameters(false);
    for (int string = 0; string < stringCount; ++string)
    {
        auto& voice = voices_[static_cast<std::size_t>(string)];
        if (parameters_.stringMaterial == StringMaterial::Steel)
            updateAttackPitch(voice, string);
        else
        {
            voice.attackPitchCents *= voice.attackPitchDecay;
            if (voice.attackPitchCents < 1.0e-4f)
                voice.attackPitchCents = 0.0f;
        }
        configureVoice(voice, string, voice.midiNote, false);
    }
}

void AcustraEngine::configureBody() noexcept
{
    if (bodyConfigured_)
    {
        fadingBodyModes_ = bodyModes_;
        bodyModelFade_ = 0.0f;
    }
    else
    {
        bodyModelFade_ = 1.0f;
    }

    const auto shape = shapeSpecs[static_cast<std::size_t>(parameters_.shape)];
    const auto wood = woodSpecs[static_cast<std::size_t>(parameters_.bodyMaterial)];

    for (int index = 0; index < bodyModeCount; ++index)
    {
        auto& mode = bodyModes_[static_cast<std::size_t>(index)];
        const auto& measured = detail::measuredBodyModes[
            static_cast<std::size_t>(index)];
        const float alternating = (index & 1) == 0 ? 1.0f : -1.0f;
        const bool lowBodyMode = measured.frequency > 85.0f
            && measured.frequency < 145.0f;
        const float lowModeMorph = lowBodyMode
            ? shape.airHz / 107.0f : shape.modeScale;
        float frequency = measured.frequency * lowModeMorph
            * wood.frequencyScale
            * physicalCalibration_.bodyFrequencyScale
            * (1.0f + alternating * shape.asymmetry
               / std::sqrt(static_cast<float>(index + 1)));
        const float highestMode = 0.46f * static_cast<float>(sampleRate_);
        const bool audibleAtThisRate = frequency < highestMode;
        frequency = clamp(frequency, 45.0f, highestMode);

        const float upper = clamp(std::log2(std::max(frequency, 120.0f)
            / 120.0f) / 6.0f, 0.0f, 1.0f);
        const float q = clamp(measured.q * wood.qScale
            * physicalCalibration_.bodyQScale, 4.0f, 150.0f);
        const float radius = std::exp(-pi * frequency
                                      / (q * static_cast<float>(sampleRate_)));
        const std::complex<float> pole = std::polar(
            radius, twoPi * frequency * inverseSampleRate_);
        mode.poleReal = pole.real();
        mode.poleImaginary = audibleAtThisRate ? pole.imag() : 0.0f;

        const float bassTilt = 1.0f + (shape.bass - 1.0f)
            * std::exp(-frequency / 520.0f);
        const float brilliance = std::pow(wood.brightness, upper);
        const float residueTilt = std::exp2(
            physicalCalibration_.residueTiltDbPerOctave
            * std::log2(frequency / 1000.0f) / 6.02059991f);
        const float drive = audibleAtThisRate
            ? shape.volume * wood.radiation * bassTilt * brilliance
                * residueTilt
                * (lowBodyMode ? physicalCalibration_.lowBodyModeGain : 1.0f)
            : 0.0f;
        // The stored residues drive unit-input discrete states fitted at
        // 48 kHz. Convert that state as a zero-order-held continuous mode:
        // q=(p_new-1)/(p_48k-1). The former real 48k/rate approximation lost
        // q's phase and changed the summed response at higher host rates.
        const float referenceRate = 48000.0f;
        const std::complex<float> referencePole = std::polar(
            std::exp(-pi * frequency / (q * referenceRate)),
            twoPi * frequency / referenceRate);
        const std::complex<float> residueRateScale
            = (pole - 1.0f) / (referencePole - 1.0f);
        const auto scaledResidue = [drive, residueRateScale]
            (float real, float imaginary)
        {
            return drive * std::complex<float>(real, imaginary)
                * residueRateScale;
        };
        const auto left = scaledResidue(
            measured.leftReal, measured.leftImaginary);
        const auto right = scaledResidue(
            measured.rightReal, measured.rightImaginary);
        mode.leftReal = left.real();
        mode.leftImaginary = left.imag();
        mode.rightReal = right.real();
        mode.rightImaginary = right.imag();
        if (bodyConfigured_)
            mode.reset();
    }
    bodyConfigured_ = true;
}

void AcustraEngine::configureBridge() noexcept
{
    bridgeLoad_.immediateAdmittance = 0.0f;
    const float rate = static_cast<float>(sampleRate_);
    const float bilinear = 2.0f * rate;
    // Each stored positive weight multiplies the continuous mobility
    // s/(s^2 + 2 damping s + omega^2).  A per-mode prewarped bilinear
    // transform preserves its measured centre frequency and PR property.
    const auto configure = [&] (BridgeMode& mode, float frequency, float q,
                                float weight)
    {
        if (!(weight > 0.0f) || frequency >= 0.45f * rate)
        {
            mode.denominator1 = mode.denominator2 = 0.0f;
            mode.numerator1 = mode.numerator2 = 0.0f;
            mode.reset();
            return;
        }
        const float omega = bilinear * std::tan(pi * frequency / rate);
        const float damping = omega / (2.0f * q);
        const float denominator0 = bilinear * bilinear
            + 2.0f * damping * bilinear + omega * omega;
        mode.denominator1 = (-2.0f * bilinear * bilinear
            + 2.0f * omega * omega) / denominator0;
        mode.denominator2 = (bilinear * bilinear
            - 2.0f * damping * bilinear + omega * omega) / denominator0;
        const float immediate = weight * bilinear / denominator0;
        mode.numerator1 = -immediate * mode.denominator1;
        mode.numerator2 = -immediate * (1.0f + mode.denominator2);
        bridgeLoad_.immediateAdmittance += immediate;
        mode.reset();
    };

    for (std::size_t index = 0; index < detail::measuredBridgeModes.size();
         ++index)
    {
        const auto& measured = detail::measuredBridgeModes[index];
        configure(bridgeLoad_.modes[index], measured.frequency, measured.q,
                  includeMeasuredBridgeMode(measured)
                      ? measured.weight
                            * physicalCalibration_.bridgeMobilityScale
                      : 0.0f);
    }

    const auto plate = plateConductanceMode(physicalCalibration_);
    configure(bridgeLoad_.modes[detail::measuredBridgeModes.size()],
              plate.frequency, plate.q, plate.weight);
    bridgeLoad_.delayedPastResponse = 0.0f;
}

float AcustraEngine::bridgePhaseDelay(float frequency,
                                      int stringIndex) const noexcept
{
    if (!bridgeCouplingEnabled_ || !(frequency > 0.0f)
        || !voices_[static_cast<std::size_t>(stringIndex)].played)
        return 0.0f;

    const float rate = static_cast<float>(sampleRate_);
    const float bilinear = 2.0f * rate;
    const float digitalOmega = twoPi * frequency / rate;
    const std::complex<float> s(
        0.0f, bilinear * std::tan(0.5f * digitalOmega));
    std::complex<float> mobility {};
    for (const auto& measured : detail::measuredBridgeModes)
    {
        if (measured.frequency >= 0.45f * rate
            || !includeMeasuredBridgeMode(measured))
            continue;
        const float omega = bilinear * std::tan(
            pi * measured.frequency / rate);
        const float damping = omega / (2.0f * measured.q);
        mobility += measured.weight
            * physicalCalibration_.bridgeMobilityScale * s
            / (s * s + 2.0f * damping * s + omega * omega);
    }

    const auto plate = plateConductanceMode(physicalCalibration_);
    if (plate.weight > 0.0f && plate.frequency < 0.45f * rate)
    {
        const float omega = bilinear * std::tan(pi * plate.frequency / rate);
        const float damping = omega / (2.0f * plate.q);
        mobility += plate.weight * s
            / (s * s + 2.0f * damping * s + omega * omega);
    }

    const bool steel = parameters_.stringMaterial == StringMaterial::Steel;
    const auto notes = openNotes(parameters_.tuning);
    const float impedance = stringImpedance(
        steel, stringIndex, notes[static_cast<std::size_t>(stringIndex)]);
    // Evaluate this folded return phase independently for each played string.
    // Unplayed open-string voices are forced one-way and do not appear in the
    // return impedance, avoiding a second count of the strings already visible
    // in the Mores bridge measurement.
    const float characteristicAdmittance = 1.0f / impedance;
    const std::complex<float> bodyImpedance = 1.0f / mobility;
    const std::complex<float> tailImpedance
        = voices_[static_cast<std::size_t>(stringIndex)].bridgeTailStiffness / s;
    const std::complex<float> effectiveMobility
        = 1.0f / (bodyImpedance + tailImpedance);
    // This is the folded full-round-trip multiplier -b/a.  Its phase is the
    // phase contributed by both measured body motion and the xi_b tail; the
    // speaking-string delay is shortened by exactly that amount when tuned.
    const std::complex<float> selfReflection
        = (characteristicAdmittance - effectiveMobility)
        / (characteristicAdmittance + effectiveMobility);
    return -std::arg(selfReflection) / digitalOmega;
}

void AcustraEngine::configureVoice(Voice& voice, int stringIndex,
                                    int midiNote, bool clearDelay) noexcept
{
    const bool steel = parameters_.stringMaterial == StringMaterial::Steel;
    const auto& physical = steel ? physicalCalibration_.steel
                                 : physicalCalibration_.nylon;
    const auto index = static_cast<std::size_t>(stringIndex);
    const float scaleLength = steel ? 0.648f : 0.650f;
    // A natural harmonic is the open string vibrating in its nth mode, so the
    // waveguide runs at the open pitch and the mode number comes from the
    // released shape. voice.midiNote stays the requested note, which is what
    // note-off and ownership match on.
    const int stoppedMidi = voice.harmonic > 1 ? voice.openMidi : midiNote;
    const int fret = std::max(0, stoppedMidi - voice.openMidi);
    const float soundingLength = scaleLength * std::exp2(-static_cast<float>(fret) / 12.0f);
    const float unbentFrequency = midiFrequency(stoppedMidi);
    // The bounded Kirchhoff-Carrier surrogate follows the waveguide's inferred
    // slope energy; its delay target slews on the existing 6 ms time constant.
    const auto channel = static_cast<std::size_t>(voice.midiChannel - 1);
    float performedBend = voice.played ? pitchBendSemitones_[channel] : 0.0f;
    if (voice.played && voice.mpeMember)
    {
        const float memberBend = voice.memberPitchBendFrozen
            ? voice.frozenMemberPitchBendSemitones
            : pitchBendSemitones_[channel];
        performedBend = pitchBendSemitones_[0] + memberBend;
    }
    const float performedSemitones = clamp(performedBend, -192.0f, 192.0f)
        + 0.01f * voice.attackPitchCents;
    // An eight-octave RPN range is legal even when the requested pitch is
    // outside this finite waveguide's representable band. Keep the performed
    // interval, then bound the physical frequency at the existing delay-line
    // limits so hostile wheels remain finite.
    const float frequency = clamp(
        unbentFrequency * std::exp2(performedSemitones / 12.0f),
        static_cast<float>(sampleRate_) / (maximumDelaySamples - 3.0f),
        0.24f * static_cast<float>(sampleRate_));

    const float diameter = steel ? steelDiameterMetres[index]
                                 : nylonDiameterMetres[index];
    const float bendingDiameter = steel ? steelBendingDiameter[index]
                                        : diameter;
    const float youngsModulus = steel ? steelYoungsModulus
                                     : nylonYoungsModulus[index];
    const float openFrequency = midiFrequency(voice.openMidi);
    const float openWaveSpeed = 2.0f * scaleLength * openFrequency;
    const float standardWaveSpeed = 2.0f * scaleLength
        * midiFrequency(standardOpenMidi[index]);
    const float linearMass = steel
        ? steelTensionNewtons[index]
            / (standardWaveSpeed * standardWaveSpeed)
        : nylonDensity[index] * pi * 0.25f * diameter * diameter;
    const float tension = steel
        ? (voice.openMidi == standardOpenMidi[index]
            ? steelTensionNewtons[index]
            : std::max(linearMass * openWaveSpeed * openWaveSpeed, 1.0f))
        : std::max(linearMass * openWaveSpeed * openWaveSpeed, 1.0f);
    const float stiffness = pi * pi * pi * youngsModulus
        * bendingDiameter * bendingDiameter * bendingDiameter * bendingDiameter
        / 64.0f;
    const float inharmonicity = clamp(stiffness * physical.stiffnessScale
        / (tension * soundingLength * soundingLength), 0.0f, 0.004f);
    const float age = parameters_.stringAge;
    // Preserve the material/age law while allowing one shared fitted cutoff
    // scale to reduce excess upper-partial damping without changing the
    // fundamental T60 target below.
    const float cutoff = (steel
        ? 12500.0f * std::exp(-1.25f * age)
        :  8200.0f * std::exp(-0.78f * age))
        * physicalCalibration_.highLossCutoffScale;
    const float lowpassCoefficient = std::exp(
        -twoPi * clamp(cutoff, 1200.0f,
                       0.44f * static_cast<float>(sampleRate_))
        * inverseSampleRate_);
    const float highLoss = clamp(((steel ? 0.035f : 0.095f)
        + 0.42f * age
        + 0.018f * static_cast<float>(stringCount - 1 - stringIndex))
        * physical.frequencyLossScale, 0.0f, 0.95f);
    // The heel of the picking hand resting by the saddle is a soft lossy
    // absorber in parallel with the string's own loss, so the rates add:
    // 1/T60 = 1/T60_string + 1/T60_hand. The hand's mapped time and the 0.62
    // ratio by which it shortens the top relative to the fundamental are
    // Electry's calibrated bridge-hand endpoints in this repository. They
    // transfer because the absorber is the player's hand, not the instrument's
    // string set; they are not refitted here, since the reference corpus holds
    // no muted notes. Pressure scales the rate, so zero is an exact no-op
    // rather than a four-second floor.
    const float handRate = palmMute_ > 0.0f
        ? palmMute_ / std::exp(std::log(4.0f)
            + palmMute_ * (std::log(0.080f) - std::log(4.0f)))
        : 0.0f;
    // A soft contact damps the top faster than the fundamental. Adding exactly
    // the extra per-round-trip loss that a 0.62 high-to-fundamental T60 ratio
    // implies keeps the shelf's shape and leaves it untouched at zero pressure.
    const float mutedHighLoss = handRate > 0.0f
        ? clamp(1.0f - (1.0f - highLoss) * std::exp(
              -(1.0f / 0.62f - 1.0f) * handRate
              / std::max(unbentFrequency, 1.0f)), 0.0f, 0.95f)
        : highLoss;

    // Woodhouse's measured string-loss model contains a term linear in modal
    // angular frequency (DAFx-26 Eq. 25 reports eta_f=2.0--2.5e-4 s for the
    // EJ45 set). A second, low-gain pole supplies the part of that slope the
    // former fixed-kHz loss filter missed, so upper partials evolve during a
    // held note instead of repeating like a lossless plucked oscillator.
    const float viscousLoss = (steel ? 1.65e-4f : 2.25e-4f)
        * (1.0f + 1.35f * age);
    const float broadLoss = clamp(72.0f * viscousLoss
        * physical.frequencyLossScale, 0.0f, 0.95f);
    const float broadLossCutoff = 14.3f * frequency;
    const float broadLossCoefficient = std::exp(-twoPi
        * clamp(broadLossCutoff, 500.0f,
                0.44f * static_cast<float>(sampleRate_))
        * inverseSampleRate_);
    const float designBroadLossCoefficient = std::exp(-twoPi
        * clamp(14.3f * unbentFrequency, 500.0f,
                0.44f * static_cast<float>(sampleRate_))
        * inverseSampleRate_);
    // The dispersion allpass is designed for the open string. Its cached
    // design is invalidated by frequency, inharmonicity, age and loss scale,
    // and deliberately not by bridge-hand pressure: tracking a gliding hand
    // would rerun this iterative design every control period, and the hand
    // only adds loss on top of a shape this already fixes.
    const bool dispersionDesignChanged = clearDelay
        || std::abs(voice.dispersionDesignFrequency - unbentFrequency) > 1.0e-4f
        || std::abs(voice.dispersionDesignInharmonicity - inharmonicity) > 1.0e-9f
        || std::abs(voice.dispersionDesignAge - age) > 1.0e-5f
        || std::abs(voice.dispersionDesignFrequencyLossScale
                    - physical.frequencyLossScale) > 1.0e-5f;
    if (dispersionDesignChanged)
    {
        const auto calibration = calibrateDispersion(
            inharmonicity, unbentFrequency, sampleRate_,
            designBroadLossCoefficient, broadLoss,
            lowpassCoefficient, highLoss, 10.0, 4.0);
        voice.dispersionDecayRatio = static_cast<float>(calibration.decayRatio);
        voice.dispersionPoleRatio = static_cast<float>(calibration.poleRatio);
        voice.dispersionDesignFrequency = unbentFrequency;
        voice.dispersionDesignInharmonicity = inharmonicity;
        voice.dispersionDesignAge = age;
        voice.dispersionDesignFrequencyLossScale
            = physical.frequencyLossScale;
    }
    const float omega = twoPi * frequency * inverseSampleRate_;
    double dispersionA1 = 0.0;
    double dispersionA2 = 0.0;
    secondOrderAllpassCoefficients(
        static_cast<double>(omega), voice.dispersionDecayRatio,
        voice.dispersionPoleRatio, dispersionA1, dispersionA2);
    const float fretT60Factor = steel
        ? clamp(1.0f - physicalCalibration_.steelFretT60Slope
                            * static_cast<float>(fret), 0.10f, 2.0f)
        : clamp(1.0f - 0.018f * static_cast<float>(fret), 0.10f, 2.0f);
    float fundamentalT60 = (steel ? 5.4f : 4.1f)
        * (1.0f - 0.12f * age)
        * fretT60Factor
        * physical.fundamentalT60Scale;
    if (handRate > 0.0f)
        fundamentalT60 = 1.0f / (1.0f / fundamentalT60 + handRate);
    const float rawDelay = static_cast<float>(tunedCatmullDelay(
        frequency, sampleRate_, broadLossCoefficient, broadLoss,
        lowpassCoefficient, mutedHighLoss, dispersionA1, dispersionA2));
    voice.bridgeTailStiffness = tension / std::max(
        (1.0f - bridgePositionFraction) * soundingLength, 1.0e-5f);
    const float measuredBridgeDelay = bridgePhaseDelay(frequency, stringIndex);
    const float desiredPeriodGain = std::pow(0.001f,
        1.0f / std::max(fundamentalT60 * frequency, 1.0f));
    const float filterGain = magnitudeForOnePoleMix(
        broadLossCoefficient, broadLoss, omega)
        * magnitudeForOnePoleMix(lowpassCoefficient, mutedHighLoss, omega);
    const float loopGain = desiredPeriodGain / std::max(filterGain, 0.50f);
    const float touch = effectiveTouch(voice);
    const float contactCutoff = ((steel ? 900.0f : 1800.0f)
        + (steel ? 2000.0f : 3000.0f) * touch)
        * (0.90f + 0.03f * static_cast<float>(stringIndex));
    voice.bridgeContactCoefficient = 1.0f - std::exp(-twoPi
        * clamp(contactCutoff, 700.0f,
                 0.35f * static_cast<float>(sampleRate_))
        * inverseSampleRate_);

    for (int polarisation = 0; polarisation < 2; ++polarisation)
    {
        auto& loop = voice.loops[static_cast<std::size_t>(polarisation)];
        const float splitCents = polarisation == 0 ? -0.32f : 0.41f;
        const float split = std::exp2(splitCents / 1200.0f);
        const float polarisationDelay = rawDelay
            - (polarisation == 0 ? measuredBridgeDelay : 0.0f);
        loop.targetDelay = clamp(polarisationDelay / split, 3.0f,
                                 static_cast<float>(maximumDelaySamples - 3));
        if (clearDelay)
            loop.currentDelay = loop.targetDelay;
        loop.loopGain = clamp(loopGain
            * (polarisation == 0 ? 0.9995f : 0.9988f), 0.70f, 0.999995f);
        loop.broadLossMix = clamp(broadLoss
            * (polarisation == 0 ? 1.0f : 1.06f), 0.0f, 1.0f);
        loop.highLossMix = clamp(mutedHighLoss
            * (polarisation == 0 ? 1.0f : 1.08f), 0.0f, 1.0f);
        loop.broadLossCoefficient = broadLossCoefficient;
        loop.lowpassCoefficient = lowpassCoefficient;
        loop.dispersionA1 = static_cast<float>(dispersionA1);
        loop.dispersionA2 = static_cast<float>(dispersionA2);
        if (clearDelay)
            loop.reset();
    }

    voice.midiNote = midiNote;
    voice.fret = fret;
    voice.characteristicImpedance = stringImpedance(
        steel, stringIndex, voice.openMidi);
    if (voice.keyDown || voice.pedalHeld || !voice.played)
        voice.releaseDamping = 1.0f;
}

void AcustraEngine::updateAttackPitch(Voice& voice, int stringIndex) noexcept
{
    if (parameters_.stringMaterial != StringMaterial::Steel)
        return;
    if (!voice.played || !(voice.attackSlopeEnergy > 0.0f)
        || !std::isfinite(voice.attackSlopeEnergy))
    {
        voice.attackPitchCents = 0.0f;
        return;
    }

    const auto& physical = physicalCalibration_.steel;
    const auto index = static_cast<std::size_t>(stringIndex);
    constexpr float scaleLength = 0.648f;
    const float soundingLength = scaleLength
        * std::exp2(-static_cast<float>(voice.fret) / 12.0f);
    // Wound axial rigidity uses the same effective core proxy as bending;
    // treating its outside diameter as solid 200 GPa steel is less physical.
    const float diameter = steelBendingDiameter[index];
    constexpr float youngsModulus = steelYoungsModulus;
    const float area = 0.25f * pi * diameter * diameter;
    const float secondMoment = pi * diameter * diameter * diameter * diameter
                             / 64.0f;
    const float openWaveSpeed = 2.0f * scaleLength
                              * midiFrequency(voice.openMidi);
    const float tension = voice.characteristicImpedance * openWaveSpeed;
    const float lengthSquared = soundingLength * soundingLength;
    const float displacement = physicalCalibration_.steelDisplacementScaleMetres;
    const float tensionIncrease = youngsModulus * area
        * displacement * displacement * voice.attackSlopeEnergy
        / (2.0f * lengthSquared);
    const float bendingStiffness = pi * pi * youngsModulus * secondMoment
        * physical.stiffnessScale / lengthSquared;
    const float ratio = tensionIncrease / (tension + bendingStiffness);
    const float cents = 1200.0f * std::log2(std::sqrt(1.0f + ratio));
    voice.attackPitchCents = std::isfinite(cents)
        ? clamp(cents, 0.0f, 20.0f) : 0.0f;
}

float AcustraEngine::effectiveTouch(const Voice& voice) const noexcept
{
    const auto& physical = parameters_.stringMaterial == StringMaterial::Steel
        ? physicalCalibration_.steel : physicalCalibration_.nylon;
    return clamp(parameters_.touch + physical.velocityBrightnessDepth
        * (voice.velocity - 0.5f), 0.0f, 1.0f);
}

float AcustraEngine::nextNoise(Voice& voice) noexcept
{
    std::uint32_t state = voice.randomState;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    voice.randomState = state == 0 ? 0x6d2b79f5u : state;
    return static_cast<float>(static_cast<std::int32_t>(voice.randomState))
         / static_cast<float>(std::numeric_limits<std::int32_t>::max());
}

void AcustraEngine::initialisePluck(Voice& voice, int stringIndex,
                                    float velocity) noexcept
{
    const float v = clamp(velocity, 0.001f, 1.0f);
    const bool steel = parameters_.stringMaterial == StringMaterial::Steel;
    const auto& physical = steel ? physicalCalibration_.steel
                                 : physicalCalibration_.nylon;
    const float touch = effectiveTouch(voice);
    const float scaleLength = steel ? 0.648f : 0.650f;
    const float soundingLength = scaleLength
        * std::exp2(-static_cast<float>(voice.fret) / 12.0f);
    // A player's hand stays at an approximately fixed physical distance from
    // the bridge.  Express that distance relative to each fretted string only
    // when constructing its initial condition.
    const float distanceFromBridge = (0.045f
        + 0.135f * parameters_.pluckPosition) * physical.pluckDistanceScale;
    const float position = clamp(distanceFromBridge / soundingLength,
                                 0.05f, 0.46f);
    // Velocity response has two bounded parts: touch brightens with velocity,
    // while the displacement exponent moves from the legacy 1.32 toward the
    // reference-response 0.82 as the same fitted depth rises.
    const float velocityExponent = 1.32f
        - 0.50f * physical.velocityBrightnessDepth;
    const float amplitude = (steel ? 0.24f : 0.29f)
        * std::pow(v, velocityExponent) * (0.92f + 0.08f * touch);
    const float randomAngle = 0.025f * nextNoise(voice);
    voice.polarisationMix = clamp(0.91f - 0.08f * touch + randomAngle,
                                  0.78f, 0.96f);
    // The shared register law pivots at one fixed 48 kHz MIDI-61 period,
    // independent of material, string choice and host sample rate.
    const float apertureReferenceDelay = 48000.0f / midiFrequency(61);

    // A finger or plectrum that lands on a sounding string to repluck it holds
    // it at the contact point, and a held point leaves exactly the modes that
    // have a node there. Averaging the stored shape with itself shifted by the
    // contact position is that projection: a mode with a node at the contact
    // passes at unit gain, one with an antinode cancels, and nothing in between
    // is invented. Discarding the shape instead stepped the wave the bridge
    // reads, and since a step strikes every bridge and body mode at once it
    // arrived about 24 dB above the note. A silent string projects to silence,
    // so a first pluck is unchanged to the bit. The scratch is the tail loop,
    // which by construction holds nothing when a string is replucked rather
    // than taken.
    const bool keepContactResidual = voice.level > 2.0e-7f && !voice.tailActive;
    auto& scratch = voice.tailLoop.delay;

    for (int polarisation = 0; polarisation < 2; ++polarisation)
    {
        auto& loop = voice.loops[static_cast<std::size_t>(polarisation)];
        const int storedLength = std::clamp(
            static_cast<int>(std::round(loop.targetDelay)), 8,
            maximumDelaySamples - 3);
        if (keepContactResidual)
            for (int sample = 1; sample <= storedLength; ++sample)
                scratch[static_cast<std::size_t>(sample - 1)]
                    = loop.delay[static_cast<std::size_t>(wrapDelayIndex(
                        loop.writeIndex - sample))];
        loop.reset();
        loop.currentDelay = loop.targetDelay;
        const int length = std::clamp(
            static_cast<int>(std::round(loop.targetDelay)), 8,
            maximumDelaySamples - 3);
        const float localPosition = clamp(position
            + (polarisation == 0 ? -0.006f : 0.009f), 0.05f, 0.48f);
        const float polarisationGain = polarisation == 0
            ? std::sqrt(voice.polarisationMix)
            : std::sqrt(1.0f - voice.polarisationMix);
        const float currentReferenceLength = loop.targetDelay * 48000.0f
            / static_cast<float>(sampleRate_);
#if defined(ACUSTRA_ANALYSIS_APERTURE_MILLISECONDS)
        const float apertureSamples
            = ACUSTRA_ANALYSIS_APERTURE_MILLISECONDS * 48.0f;
#else
        const float apertureSamples = 0.70f + 3.60f * (1.0f - touch)
            + (stringIndex < 3 ? 1.0f : 0.0f)
            + (voice.fret >= 17 ? 1.5f : 0.0f);
#endif
        const int modes = std::max(voice.harmonic, 1);
        const float aperture = registeredPluckAperture(
            apertureSamples, physical.apertureScale, apertureReferenceDelay,
            currentReferenceLength,
            physicalCalibration_.apertureRegisterExponent);
        const auto triangleAt = [localPosition] (float phase)
        {
            phase -= std::floor(phase);
            return phase < localPosition
                ? phase / localPosition
                : (1.0f - phase) / (1.0f - localPosition);
        };
        const auto smoothedTriangleAt = [&] (float phase)
        {
            return (triangleAt(phase - 2.0f * aperture)
                + 4.0f * triangleAt(phase - aperture)
                + 6.0f * triangleAt(phase)
                + 4.0f * triangleAt(phase + aperture)
                + triangleAt(phase + 2.0f * aperture)) / 16.0f;
        };
        // A light touch at the nth node leaves exactly the modes that have a
        // node there. Averaging the released shape over its n cyclic shifts is
        // that projection exactly: every harmonic that is a multiple of n
        // passes at unit gain and every other one cancels. It needs no filter
        // and no free constant, and the surviving modes keep precisely the
        // amplitude the pluck gave them - which is why a harmonic comes out
        // quieter than the stopped note, as one does on a guitar.
        const auto releasedAt = [&] (float phase)
        {
            if (modes <= 1)
                return smoothedTriangleAt(phase);
            float sum = 0.0f;
            for (int shift = 0; shift < modes; ++shift)
                sum += smoothedTriangleAt(phase + static_cast<float>(shift)
                                                / static_cast<float>(modes));
            return sum / static_cast<float>(modes);
        };
        const float endpoint = releasedAt(0.0f);
        const int contactShift = std::clamp(
            static_cast<int>(std::lround(localPosition
                                         * static_cast<float>(length))),
            1, std::max(1, length - 1));
        for (int sample = 1; sample <= length; ++sample)
        {
            const float phase = static_cast<float>(sample - 1)
                              / static_cast<float>(length);
            const float triangle = std::max(
                releasedAt(phase) - endpoint, 0.0f);
            float residual = 0.0f;
            if (keepContactResidual)
            {
                const int shifted = (sample - 1 + contactShift) % length;
                residual = 0.5f * (scratch[static_cast<std::size_t>(sample - 1)]
                    + scratch[static_cast<std::size_t>(shifted)]);
            }
            loop.delay[static_cast<std::size_t>(wrapDelayIndex(
                loop.writeIndex - sample))] = residual
                    + amplitude * polarisationGain * triangle;
        }

        // A plucked string is released from rest. Seed the whole finite-
        // difference history from its initial displacement; reset-time zero
        // otherwise becomes an artificial harpsichord-like velocity impulse.
        loop.bridgeDerivative.reset(loop.readDelay(loop.currentDelay));
    }

    if (steel)
    {
        double slopeEnergy = 0.0;
        for (auto& loop : voice.loops)
        {
            const int length = std::clamp(
                static_cast<int>(std::round(loop.targetDelay)), 8,
                maximumDelaySamples - 3);
            const auto at = [&] (int sample)
            {
                return loop.delay[static_cast<std::size_t>(wrapDelayIndex(
                    loop.writeIndex - sample))];
            };
            float previous = at(length);
            double squaredDifferences = 0.0;
            for (int sample = 1; sample <= length; ++sample)
            {
                const float current = at(sample);
                const double difference
                    = static_cast<double>(current - previous);
                squaredDifferences += difference * difference;
                previous = current;
            }
            slopeEnergy += static_cast<double>(length) * squaredDifferences;
        }
        voice.attackSlopeEnergy = std::isfinite(slopeEnergy)
            ? static_cast<float>(std::max(slopeEnergy, 0.0)) : 0.0f;
        voice.observedSlopeEnergy = voice.attackSlopeEnergy;
        updateAttackPitch(voice, stringIndex);
    }
    else
    {
        voice.attackSlopeEnergy = 0.0f;
        voice.observedSlopeEnergy = 0.0f;
    }

    voice.velocity = v;
    voice.excitationEnvelope = amplitude * (0.003f + 0.014f * touch)
        * physical.transientScale;
    const float burstSeconds = 0.0046f - 0.0025f * touch;
    voice.excitationDecay = std::exp(-1.0f
        / (std::max(burstSeconds, 0.0004f) * static_cast<float>(sampleRate_)));
    voice.excitationColour = 0.10f + 0.62f * touch;
    voice.excitationLowpass = 0.0f;
    voice.bridgeContactState = 0.0f;
    voice.bridgeContactState2 = 0.0f;
    voice.bridgeContactBlend = std::min(physical.transientScale, 1.0f);
    const float contactSeconds = 0.0050f + 0.0030f * (1.0f - touch);
    voice.bridgeContactDecay = std::exp(-1.0f
        / (contactSeconds * static_cast<float>(sampleRate_)));
    voice.level = std::max(voice.level, 0.02f * v);
    voice.releaseDamping = 1.0f;
    voice.quietSamples = 0;
}

void AcustraEngine::returnToOpenString(Voice& voice, int stringIndex) noexcept
{
    voice.played = false;
    voice.keyDown = false;
    voice.pedalHeld = false;
    voice.mpeMember = false;
    voice.memberPitchBendFrozen = false;
    voice.ownerCount = 0;
    voice.midiNote = voice.openMidi;
    voice.midiChannel = 1;
    voice.fret = 0;
    voice.velocity = 0.0f;
    voice.excitationEnvelope = 0.0f;
    voice.bridgeContactState = 0.0f;
    voice.bridgeContactState2 = 0.0f;
    voice.bridgeContactBlend = 0.0f;
    voice.attackPitchCents = 0.0f;
    voice.attackPitchDecay = 1.0f;
    voice.frozenMemberPitchBendSemitones = 0.0f;
    voice.attackSlopeEnergy = 0.0f;
    voice.observedSlopeEnergy = 0.0f;
    voice.harmonic = 1;
    voice.releaseDamping = 1.0f;
    voice.quietSamples = 0;
    voice.tailActive = false;
    voice.tailLevel = 0.0f;
    voice.tailQuietSamples = 0;
    voice.tailLoop.reset();
    configureVoice(voice, stringIndex, voice.openMidi, true);
}

void AcustraEngine::captureTail(Voice& voice) noexcept
{
    // A string is only taken while it is being replucked, so the plucking hand
    // is on it whatever the outgoing note's fret was. That is the contact
    // beginRelease already models for a stopped fretted note, so the same
    // 0.16 s damping applies here; the open-string 1.25 s case is a lifted
    // fretting finger on a string nobody is touching, which this is not.
    if (!(voice.level > 2.0e-7f))
    {
        voice.tailActive = false;
        return;
    }
    voice.tailLoop = voice.loops[0];
    constexpr float contactSeconds = 0.16f;
    voice.tailDamping = std::pow(0.001f,
        1.0f / std::max(contactSeconds * midiFrequency(voice.midiNote), 1.0f));
    voice.tailLevel = voice.level;
    voice.tailQuietSamples = 0;
    voice.tailActive = true;
}

void AcustraEngine::beginRelease(Voice& voice) noexcept
{
    voice.pedalHeld = false;
    const float releaseSeconds = voice.fret == 0 ? 1.25f : 0.16f;
    voice.releaseDamping = std::pow(0.001f,
        1.0f / std::max(releaseSeconds * midiFrequency(voice.midiNote), 1.0f));
}

void AcustraEngine::freezeMemberPitchBend(Voice& voice) noexcept
{
    if (!voice.mpeMember || voice.memberPitchBendFrozen)
        return;
    voice.frozenMemberPitchBendSemitones = pitchBendSemitones_[
        static_cast<std::size_t>(voice.midiChannel - 1)];
    voice.memberPitchBendFrozen = true;
}

bool AcustraEngine::isLowerZoneMaster(int midiChannel) const noexcept
{
    return lowerZoneMemberCount_ > 0 && midiChannel == 1;
}

bool AcustraEngine::isLowerZoneMember(int midiChannel) const noexcept
{
    return lowerZoneMemberCount_ > 0 && midiChannel >= 2
        && midiChannel <= lowerZoneMemberCount_ + 1;
}

bool AcustraEngine::channelControlsVoice(int midiChannel,
                                         const Voice& voice) const noexcept
{
    if (isLowerZoneMaster(midiChannel))
        return voice.midiChannel == 1 || voice.mpeMember;
    return voice.midiChannel == midiChannel;
}

bool AcustraEngine::sustainIsDown(const Voice& voice) const noexcept
{
    if (voice.midiChannel < 1 || voice.midiChannel > midiChannelCount)
        return false;
    const bool own = sustainPedals_[static_cast<std::size_t>(
        voice.midiChannel - 1)];
    return voice.mpeMember ? sustainPedals_[0] || own : own;
}

int AcustraEngine::chooseString(int midiNote) const noexcept
{
    int best = -1;
    int bestFret = fretCount + 1;
    for (int string = stringCount - 1; string >= 0; --string)
    {
        const auto& voice = voices_[static_cast<std::size_t>(string)];
        const int fret = midiNote - voice.openMidi;
        if (fret < 0 || fret > fretCount)
            continue;
        if (!voice.played && fret < bestFret)
        {
            best = string;
            bestFret = fret;
        }
    }
    if (best >= 0)
        return best;

    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
    for (int string = stringCount - 1; string >= 0; --string)
    {
        const auto& voice = voices_[static_cast<std::size_t>(string)];
        const int fret = midiNote - voice.openMidi;
        if (fret < 0 || fret > fretCount)
            continue;
        if (!voice.keyDown && voice.startOrder < oldest)
        {
            best = string;
            oldest = voice.startOrder;
        }
    }
    if (best >= 0)
        return best;

    oldest = std::numeric_limits<std::uint64_t>::max();
    for (int string = stringCount - 1; string >= 0; --string)
    {
        const auto& voice = voices_[static_cast<std::size_t>(string)];
        const int fret = midiNote - voice.openMidi;
        if (fret >= 0 && fret <= fretCount && voice.startOrder < oldest)
        {
            best = string;
            oldest = voice.startOrder;
        }
    }
    return best;
}

AcustraEngine::HarmonicChoice
AcustraEngine::chooseHarmonic(int midiNote) const noexcept
{
    // A natural harmonic sounds at exactly n times a string's open pitch, so
    // the requested note itself says whether the guitar can produce it. No
    // keyswitch is needed and none is offered. Harmonics 2 to 6 and 8 fall
    // within 25 cents of equal temperament; the seventh is 31 cents flat,
    // which is why players avoid it against tempered material, and the same
    // tolerance excludes it here. Nodes above the eighth are impractical to
    // touch and very quiet, so the search stops there.
    constexpr int highestHarmonic = 8;
    constexpr float toleranceCents = 25.0f;
    const float wanted = midiFrequency(midiNote);
    HarmonicChoice best {};
    bool bestFree = false;
    for (int string = stringCount - 1; string >= 0; --string)
    {
        const auto& voice = voices_[static_cast<std::size_t>(string)];
        const float open = midiFrequency(voice.openMidi);
        for (int harmonic = 2; harmonic <= highestHarmonic; ++harmonic)
        {
            const float sounding = open * static_cast<float>(harmonic);
            if (std::abs(1200.0f * std::log2(sounding / wanted))
                > toleranceCents)
                continue;
            // Prefer a string nobody is using, then the lowest node, which is
            // the loudest and the one a player reaches for first.
            const bool free = !voice.played;
            if (best.string < 0 || (free && !bestFree)
                || (free == bestFree && harmonic < best.harmonic))
            {
                best = { string, harmonic };
                bestFree = free;
            }
            break;
        }
    }
    return best;
}

void AcustraEngine::noteOn(int midiNote, float velocity,
                           int midiChannel) noexcept
{
    if (!prepared_ || midiNote < 0 || midiNote > 127
        || !std::isfinite(velocity) || velocity <= 0.0f
        || midiChannel < 1 || midiChannel > midiChannelCount)
        return;

    for (int string = 0; string < stringCount; ++string)
    {
        auto& voice = voices_[static_cast<std::size_t>(string)];
        if (voice.played && voice.midiNote == midiNote
            && voice.midiChannel == midiChannel && voice.keyDown)
        {
            ++voice.ownerCount;
            voice.startOrder = ++noteOrder_;
            const float v = clamp(velocity, 0.001f, 1.0f);
            voice.velocity = v;
            if (parameters_.stringMaterial == StringMaterial::Steel)
            {
                voice.attackPitchCents = 0.0f;
                voice.attackPitchDecay = 1.0f;
            }
            else
            {
                voice.attackPitchCents = 3.0f * v * v
                    * (0.72f + 0.28f * effectiveTouch(voice));
                voice.attackPitchDecay = std::exp(
                    -static_cast<float>(controlPeriod)
                    / (0.075f * static_cast<float>(sampleRate_)));
            }
            configureVoice(voice, string, midiNote, false);
            initialisePluck(voice, string, velocity);
            configureVoice(voice, string, midiNote, false);
            return;
        }
    }

    int string = chooseString(midiNote);
    int harmonic = 1;
    if (string < 0)
    {
        // Above the fretted range the guitar still reaches, through the
        // natural harmonics of its open strings. Below it, it does not.
        const auto choice = chooseHarmonic(midiNote);
        if (choice.string < 0)
            return;
        string = choice.string;
        harmonic = choice.harmonic;
    }
    auto& voice = voices_[static_cast<std::size_t>(string)];
    // Taking a string that is still sounding for a different note is a refret
    // and a repluck, not a cut. Keep what it still holds. A repluck of the same
    // note keeps the previous behaviour: a tail at the identical delay length
    // would comb against the new pluck rather than model the superposition.
    if (voice.played && voice.midiNote != midiNote)
        captureTail(voice);
    voice.harmonic = harmonic;
    voice.played = true;
    voice.keyDown = true;
    voice.pedalHeld = false;
    voice.ownerCount = 1;
    voice.midiChannel = midiChannel;
    voice.mpeMember = isLowerZoneMember(midiChannel);
    voice.memberPitchBendFrozen = false;
    voice.frozenMemberPitchBendSemitones = 0.0f;
    voice.startOrder = ++noteOrder_;
    const float v = clamp(velocity, 0.001f, 1.0f);
    voice.velocity = v;
    if (parameters_.stringMaterial == StringMaterial::Steel)
    {
        voice.attackPitchCents = 0.0f;
        voice.attackPitchDecay = 1.0f;
    }
    else
    {
        voice.attackPitchCents = 3.0f * v * v
            * (0.72f + 0.28f * effectiveTouch(voice));
        voice.attackPitchDecay = std::exp(
            -static_cast<float>(controlPeriod)
            / (0.075f * static_cast<float>(sampleRate_)));
    }
    configureVoice(voice, string, midiNote, true);
    initialisePluck(voice, string, velocity);
    configureVoice(voice, string, midiNote, false);
}

void AcustraEngine::noteOff(int midiNote, int midiChannel) noexcept
{
    if (midiChannel < 1 || midiChannel > midiChannelCount)
        return;
    int candidateIndex = -1;
    for (int string = 0; string < stringCount; ++string)
    {
        auto& voice = voices_[static_cast<std::size_t>(string)];
        if (voice.played && voice.keyDown && voice.midiNote == midiNote
            && voice.midiChannel == midiChannel
            && (candidateIndex < 0 || voice.startOrder
                > voices_[static_cast<std::size_t>(candidateIndex)].startOrder))
            candidateIndex = string;
    }
    if (candidateIndex < 0)
        return;
    auto& candidate = voices_[static_cast<std::size_t>(candidateIndex)];
    if (--candidate.ownerCount > 0)
        return;
    candidate.ownerCount = 0;
    freezeMemberPitchBend(candidate);
    candidate.keyDown = false;
    candidate.pedalHeld = sustainIsDown(candidate);
    if (!candidate.pedalHeld)
        beginRelease(candidate);
}

void AcustraEngine::setSustainPedal(bool down, int midiChannel) noexcept
{
    if (midiChannel < 1 || midiChannel > midiChannelCount)
        return;
    auto& pedal = sustainPedals_[static_cast<std::size_t>(midiChannel - 1)];
    if (pedal == down)
        return;
    pedal = down;
    if (down)
        return;
    for (int string = 0; string < stringCount; ++string)
    {
        auto& voice = voices_[static_cast<std::size_t>(string)];
        if (!voice.pedalHeld || voice.keyDown
            || !channelControlsVoice(midiChannel, voice)
            || sustainIsDown(voice))
            continue;
        beginRelease(voice);
    }
}

void AcustraEngine::setPitchBend(float semitones, int midiChannel) noexcept
{
    if (midiChannel < 1 || midiChannel > midiChannelCount)
        return;
    pitchBendSemitones_[static_cast<std::size_t>(midiChannel - 1)]
        = clamp(std::isfinite(semitones) ? semitones : 0.0f, -96.0f, 96.0f);
}

void AcustraEngine::setLowerZoneMemberCount(int memberCount) noexcept
{
    const int next = std::clamp(memberCount, 0, midiChannelCount - 1);
    if (next == lowerZoneMemberCount_)
        return;

    // MIDI MPE defines layout changes as controller-reset/note-stop
    // boundaries for the union of the old and new zone. Acustra deliberately
    // supports the lower zone only; channels outside that union stay intact.
    const int lastAffectedChannel = std::max(next, lowerZoneMemberCount_) + 1;
    for (int string = 0; string < stringCount; ++string)
    {
        auto& voice = voices_[static_cast<std::size_t>(string)];
        if (voice.played && voice.midiChannel <= lastAffectedChannel)
            returnToOpenString(voice, string);
    }
    for (int midiChannel = 1; midiChannel <= lastAffectedChannel; ++midiChannel)
    {
        pitchBendSemitones_[static_cast<std::size_t>(midiChannel - 1)] = 0.0f;
        sustainPedals_[static_cast<std::size_t>(midiChannel - 1)] = false;
    }
    lowerZoneMemberCount_ = next;
    if (getActiveVoiceCount() == 0)
    {
        for (int string = 0; string < stringCount; ++string)
            returnToOpenString(voices_[static_cast<std::size_t>(string)], string);
        resetSoundState();
    }
}

void AcustraEngine::allNotesOff(int midiChannel) noexcept
{
    if (midiChannel < 1 || midiChannel > midiChannelCount)
        return;
    for (int string = 0; string < stringCount; ++string)
    {
        auto& voice = voices_[static_cast<std::size_t>(string)];
        if (!voice.played || !channelControlsVoice(midiChannel, voice))
            continue;
        voice.ownerCount = 0;
        freezeMemberPitchBend(voice);
        voice.keyDown = false;
        voice.pedalHeld = sustainIsDown(voice);
        if (!voice.pedalHeld)
            beginRelease(voice);
    }
}

void AcustraEngine::allSoundOff(int midiChannel) noexcept
{
    if (midiChannel < 1 || midiChannel > midiChannelCount)
        return;
    for (int string = 0; string < stringCount; ++string)
    {
        auto& voice = voices_[static_cast<std::size_t>(string)];
        if (voice.played && channelControlsVoice(midiChannel, voice))
            returnToOpenString(voice, string);
    }
    if (getActiveVoiceCount() == 0)
    {
        for (int string = 0; string < stringCount; ++string)
            returnToOpenString(voices_[static_cast<std::size_t>(string)], string);
        resetSoundState();
    }
}

void AcustraEngine::setPalmMutePressure(float pressure) noexcept
{
    targetPalmMute_ = std::isfinite(pressure)
        ? clamp(pressure, 0.0f, 1.0f) : 0.0f;
}

void AcustraEngine::setBridgeCouplingEnabled(bool enabled) noexcept
{
    if (bridgeCouplingEnabled_ == enabled)
        return;
    bridgeCouplingEnabled_ = enabled;
    bridgeLoad_.reset();
    bridgeVelocityDerivative_.reset();
    bridgeForceDerivative_.reset();
    bridgeBodyForceDerivative_.reset();
    bridgeTailForceDerivative_.reset();
    lastBridgeVelocity_ = 0.0f;
    lastBridgeReactionForce_ = 0.0f;
    lastBridgeBodyForce_ = 0.0f;
    lastBridgeTailForce_ = 0.0f;
    lastSympatheticRadiationForce_ = 0.0f;
    lastBridgePower_ = 0.0f;
    lastBridgeBodyPower_ = 0.0f;
    lastBridgeTailPower_ = 0.0f;
    bridgeDerivativesNeedPriming_ = true;
    if (!prepared_)
        return;
    for (int string = 0; string < stringCount; ++string)
        configureVoice(voices_[static_cast<std::size_t>(string)], string,
                       voices_[static_cast<std::size_t>(string)].midiNote,
                       false);
}

void AcustraEngine::setSympatheticStringsEnabled(bool enabled) noexcept
{
    sympatheticStringsEnabled_ = enabled;
}

float AcustraEngine::renderExcitation(Voice& voice) noexcept
{
    float excitation = 0.0f;
    if (voice.excitationEnvelope > 1.0e-8f)
    {
        const float rateRatio = static_cast<float>(sampleRate_) / 48000.0f;
        const float noise = nextNoise(voice) * std::sqrt(rateRatio);
        const float referenceCoefficient = 0.05f
            + 0.42f * voice.excitationColour;
        const float excitationCoefficient = 1.0f - std::pow(
            1.0f - referenceCoefficient, 1.0f / rateRatio);
        voice.excitationLowpass += excitationCoefficient
            * (noise - voice.excitationLowpass);
        excitation = (voice.excitationLowpass
            + 0.16f * voice.excitationColour
                * (noise - voice.excitationLowpass))
            * voice.excitationEnvelope;
        voice.excitationEnvelope *= voice.excitationDecay;
    }
    return excitation;
}

void AcustraEngine::finishVoice(Voice& voice, int stringIndex,
                                float verticalIncident,
                                float horizontalIncident, float excitation,
                                float tailIncident, float bridgeDisplacement,
                                float bridgeVelocity, float& directLeft,
                                float& directRight,
                                float& sympatheticForce) noexcept
{
    // A rigid bridge and nut each invert a displacement wave, so the collapsed
    // full-round-trip loop writes +incident.  A moving bridge has reflected
    // wave b=x-a; folding in the nut inversion therefore writes a-x.
    voice.loops[0].write(verticalIncident - bridgeDisplacement
                         + 0.76f * excitation);
    voice.loops[1].write(horizontalIncident + 0.51f * excitation);

    const float sampleRateRatio = static_cast<float>(sampleRate_) / 48000.0f;
    const float verticalVelocity = voice.loops[0].bridgeDerivative.process(
        verticalIncident, sampleRateRatio);
    const float horizontalVelocity = voice.loops[1].bridgeDerivative.process(
        horizontalIncident, sampleRateRatio);
    if (voice.played
        && parameters_.stringMaterial == StringMaterial::Steel)
    {
        const float referenceRate = 48000.0f / static_cast<float>(sampleRate_);
        const float verticalSlope = voice.loops[0].currentDelay
                                  * referenceRate * verticalVelocity;
        const float horizontalSlope = voice.loops[1].currentDelay
                                    * referenceRate * horizontalVelocity;
        const float rawEnergy = verticalSlope * verticalSlope
                              + horizontalSlope * horizontalSlope;
        const float alpha = 1.0f - std::exp(
            -1.0f / std::max(voice.loops[0].currentDelay, 1.0f));
        const float observed = voice.observedSlopeEnergy
            + alpha * (rawEnergy - voice.observedSlopeEnergy);
        voice.observedSlopeEnergy = std::isfinite(observed)
            ? std::max(observed, 0.0f) : 0.0f;
        voice.attackSlopeEnergy = std::isfinite(voice.attackSlopeEnergy)
            ? std::min(std::max(voice.attackSlopeEnergy, 0.0f),
                       voice.observedSlopeEnergy)
            : 0.0f;
    }
    const float impedance = voice.characteristicImpedance;
    if (voice.tailActive)
    {
        // The same one-way coupling the idle open strings use: the taken
        // string keeps radiating while the hand damps it, but it does not
        // load the junction a second time.
        voice.tailLoop.write(tailIncident - bridgeDisplacement);
        const float tailVelocity = voice.tailLoop.bridgeDerivative.process(
            tailIncident, sampleRateRatio);
        const float tailForce = impedance
            * (2.0f * tailVelocity - bridgeVelocity);
        sympatheticForce += tailForce;
        voice.tailLevel += levelSmoothing_
            * (std::abs(tailForce) - voice.tailLevel);
        if (voice.tailLevel < 2.0e-7f)
            ++voice.tailQuietSamples;
        else
            voice.tailQuietSamples = 0;
        if (voice.tailQuietSamples > static_cast<int>(0.08 * sampleRate_))
        {
            voice.tailActive = false;
            voice.tailLevel = 0.0f;
            voice.tailQuietSamples = 0;
            voice.tailLoop.reset();
        }
    }
    const float localReactionForce = impedance
        * (2.0f * verticalVelocity - bridgeVelocity);
    if (!voice.played && sympatheticStringsEnabled_)
        sympatheticForce += localReactionForce;
    const float rawDirectForce = localReactionForce * voice.polarisationMix
        + 0.44f * impedance * horizontalVelocity
            * (1.0f - voice.polarisationMix);
    // Approximate the finite release aperture with a two-pole transient.  The
    // blend applies only to the quiet bridge-local output; the measured
    // force-to-pressure bank receives the unaltered junction reaction force.
    voice.bridgeContactState += voice.bridgeContactCoefficient
        * (rawDirectForce - voice.bridgeContactState);
    voice.bridgeContactState2 += voice.bridgeContactCoefficient
        * (voice.bridgeContactState - voice.bridgeContactState2);
    const float contactBlend = voice.bridgeContactBlend;
    const float directForce = rawDirectForce
        + contactBlend * (voice.bridgeContactState2 - rawDirectForce);
    voice.bridgeContactBlend *= voice.bridgeContactDecay;
    if (voice.bridgeContactBlend < 1.0e-5f)
        voice.bridgeContactBlend = 0.0f;

    const float pan = (static_cast<float>(stringIndex) - 2.5f) / 2.5f;
    // The measured force-to-pressure bank is the acoustic source. Retain only
    // a very quiet bridge-local component; the previous amplified contact
    // residual exposed the periodic string waveform as a harpsichord cue.
    const float direct = physicalCalibration_.directGain * directForce;
    directLeft += direct * (1.0f - 0.18f * pan);
    directRight += direct * (1.0f + 0.18f * pan);

    const float magnitude = std::abs(localReactionForce);
    voice.level += levelSmoothing_ * (magnitude - voice.level);
    if (voice.played && !voice.keyDown && !voice.pedalHeld)
    {
        if (voice.level < 2.0e-7f)
            ++voice.quietSamples;
        else
            voice.quietSamples = 0;
        if (voice.quietSamples > static_cast<int>(0.08 * sampleRate_))
            returnToOpenString(voice, stringIndex);
    }

}

AcustraEngine::BodyOutput AcustraEngine::renderBody(float bridgeInput) noexcept
{
    const auto renderBank = [&] (auto& modes)
    {
        BodyOutput output;
        for (auto& mode : modes)
        {
            mode.process(bridgeInput, output.left, output.right);
            if (std::abs(mode.real) < 1.0e-30f)
                mode.real = 0.0f;
            if (std::abs(mode.imaginary) < 1.0e-30f)
                mode.imaginary = 0.0f;
        }
        return output;
    };

    BodyOutput result = renderBank(bodyModes_);
    if (bodyModelFade_ < 1.0f)
    {
        const BodyOutput previous = renderBank(fadingBodyModes_);
        const float mix = bodyModelFade_;
        result.left = previous.left + mix * (result.left - previous.left);
        result.right = previous.right + mix * (result.right - previous.right);
        bodyModelFade_ = std::min(1.0f, bodyModelFade_ + bodyModelFadeStep_);
    }
    return result;
}

void AcustraEngine::process(float* left, float* right, int numSamples) noexcept
{
    if (!prepared_ || left == nullptr || right == nullptr || numSamples <= 0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (++controlCounter_ >= controlPeriod)
        {
            controlCounter_ = 0;
            updateControlState();
        }

        bodyAmount_ += parameterSmoothing_
            * (targetParameters_.bodyAmount - bodyAmount_);
        width_ += parameterSmoothing_
            * (targetParameters_.stereoWidth - width_);
        outputGain_ += parameterSmoothing_
            * (targetParameters_.outputGain - outputGain_);

        std::array<float, stringCount> verticalIncident {};
        std::array<float, stringCount> horizontalIncident {};
        std::array<float, stringCount> excitation {};
        std::array<float, stringCount> tailIncident {};
        float impedanceSum = 0.0f;
        float tailStiffnessSum = 0.0f;
        float weightedIncident = 0.0f;
        for (int string = 0; string < stringCount; ++string)
        {
            auto& voice = voices_[static_cast<std::size_t>(string)];
            const float releaseGain = (voice.keyDown || voice.pedalHeld
                                       || !voice.played)
                ? 1.0f : voice.releaseDamping;
            excitation[static_cast<std::size_t>(string)]
                = renderExcitation(voice);
            verticalIncident[static_cast<std::size_t>(string)]
                = voice.loops[0].advance(delaySmoothing_, releaseGain);
            horizontalIncident[static_cast<std::size_t>(string)]
                = voice.loops[1].advance(delaySmoothing_, releaseGain);
            if (voice.tailActive)
                tailIncident[static_cast<std::size_t>(string)]
                    = voice.tailLoop.advance(delaySmoothing_, voice.tailDamping);
            if (voice.played)
            {
                impedanceSum += voice.characteristicImpedance;
                tailStiffnessSum += voice.bridgeTailStiffness;
                weightedIncident += voice.characteristicImpedance
                    * verticalIncident[static_cast<std::size_t>(string)];
            }
        }

        // A played string sets the port. When the last one goes quiet the
        // strings are still on the bridge, and the measured body is still
        // ringing - it outlasts them - so the junction keeps the port it had
        // rather than switching it out. Dropping it instead stepped the
        // displacement every string reads: on a chord left to decay in silence
        // that arrived as a transient of 0.96 against a 0.008 background, and
        // it stored the modes' energy until the next note released it as a
        // click.
        const bool hasActivePort = impedanceSum > 1.0e-6f;
        if (hasActivePort)
        {
            lastImpedanceSum_ = impedanceSum;
            lastTailStiffnessSum_ = tailStiffnessSum;
        }
        else if (lastImpedanceSum_ > 1.0e-6f)
        {
            impedanceSum = lastImpedanceSum_;
            tailStiffnessSum = lastTailStiffnessSum_;
        }
        const bool portIsLoaded = impedanceSum > 1.0e-6f;
        const float aggregateIncident = portIsLoaded
            ? weightedIncident / impedanceSum : 0.0f;
        float bridgeDisplacement = 0.0f;
        float reactionWave = portIsLoaded
            ? impedanceSum * 2.0f * aggregateIncident : 0.0f;
        float bodyForceWave = reactionWave;
        float tailForceWave = 0.0f;
        if (bridgeCouplingEnabled_ && portIsLoaded)
        {
            const float reflected = bridgeLoad_.process(
                aggregateIncident, 1.0f / impedanceSum,
                tailStiffnessSum, inverseSampleRate_);
            bridgeDisplacement = aggregateIncident + reflected;
            reactionWave = bridgeLoad_.mainIntegratedForce;
            bodyForceWave = bridgeLoad_.bodyIntegratedForce;
            tailForceWave = bridgeLoad_.tailIntegratedForce;
        }
        const float sampleRateRatio = static_cast<float>(sampleRate_) / 48000.0f;
        if (bridgeDerivativesNeedPriming_
            && (std::abs(reactionWave) + std::abs(bridgeDisplacement)
                > 1.0e-12f))
        {
            bridgeVelocityDerivative_.reset(bridgeDisplacement);
            bridgeForceDerivative_.reset(reactionWave);
            bridgeBodyForceDerivative_.reset(bodyForceWave);
            bridgeTailForceDerivative_.reset(tailForceWave);
            for (int string = 0; string < stringCount; ++string)
            {
                auto& voice = voices_[static_cast<std::size_t>(string)];
                voice.loops[0].bridgeDerivative.reset(
                    verticalIncident[static_cast<std::size_t>(string)]);
                voice.loops[1].bridgeDerivative.reset(
                    horizontalIncident[static_cast<std::size_t>(string)]);
            }
            bridgeDerivativesNeedPriming_ = false;
        }
        lastBridgeVelocity_ = bridgeVelocityDerivative_.process(
            bridgeDisplacement, sampleRateRatio);
        lastBridgeReactionForce_ = bridgeForceDerivative_.process(
            reactionWave, sampleRateRatio);
        lastBridgeBodyForce_ = bridgeBodyForceDerivative_.process(
            bodyForceWave, sampleRateRatio);
        lastBridgeTailForce_ = bridgeTailForceDerivative_.process(
            tailForceWave, sampleRateRatio);
        lastBridgePower_ = lastBridgeVelocity_ * lastBridgeReactionForce_;
        lastBridgeBodyPower_ = lastBridgeVelocity_ * lastBridgeBodyForce_;
        lastBridgeTailPower_ = lastBridgeVelocity_ * lastBridgeTailForce_;

        float directLeft = 0.0f;
        float directRight = 0.0f;
        float sympatheticForce = 0.0f;
        for (int string = 0; string < stringCount; ++string)
            finishVoice(voices_[static_cast<std::size_t>(string)], string,
                verticalIncident[static_cast<std::size_t>(string)],
                horizontalIncident[static_cast<std::size_t>(string)],
                excitation[static_cast<std::size_t>(string)],
                tailIncident[static_cast<std::size_t>(string)],
                bridgeDisplacement, lastBridgeVelocity_,
                directLeft, directRight, sympatheticForce);

        // DAFx-26 Eq. (45): only Fb participates in the measured body
        // compliance, while unplayed open-string voices are summed one-way
        // into radiation. This force-scaled waveguide analogue uses unity
        // gain; neither term returns from the microphone radiation bank.
        lastSympatheticRadiationForce_ = sympatheticForce;
        const BodyOutput body = renderBody(
            lastBridgeBodyForce_ + lastSympatheticRadiationForce_);

        // Strings themselves radiate poorly. Keep the small bridge-local path
        // separate from the measurement-derived, author-transformed soundboard
        // response, and apply
        // Width to both paths so zero is genuinely mono.
        const float directMono = 0.5f * (directLeft + directRight);
        const float spreadDirectLeft = directMono
            + width_ * (directLeft - directMono);
        const float spreadDirectRight = directMono
            + width_ * (directRight - directMono);
        const float bodyScale = 0.68f + 0.72f * bodyAmount_;
        const float directScale = 0.10f + 0.10f * (1.0f - bodyAmount_);
        const float monoBody = 0.5f * (body.left + body.right);
        const float spreadLeft = monoBody + width_ * (body.left - monoBody);
        const float spreadRight = monoBody + width_ * (body.right - monoBody);
        float outputLeft = radiationReferenceGain * outputGain_
            * (bodyScale * spreadLeft + directScale * spreadDirectLeft);
        float outputRight = radiationReferenceGain * outputGain_
            * (bodyScale * spreadRight + directScale * spreadDirectRight);

        // Preserve ordinary notes exactly; only the final 1 dB of headroom is
        // compressed for pathological automation and dense repicks.
        outputLeft = safetyLimit(outputLeft);
        outputRight = safetyLimit(outputRight);
        left[sample] = std::isfinite(outputLeft) ? outputLeft : 0.0f;
        right[sample] = std::isfinite(outputRight) ? outputRight : 0.0f;
    }
}

int AcustraEngine::getActiveVoiceCount() const noexcept
{
    return static_cast<int>(std::count_if(voices_.begin(), voices_.end(),
        [] (const Voice& voice) { return voice.played; }));
}

int AcustraEngine::getSympatheticStringCount() const noexcept
{
    return static_cast<int>(std::count_if(voices_.begin(), voices_.end(),
        [] (const Voice& voice)
        {
            return !voice.played && voice.level > 2.0e-7f;
        }));
}

float AcustraEngine::getLastBridgeVelocity() const noexcept
{
    return lastBridgeVelocity_;
}

float AcustraEngine::getLastBridgeReactionForce() const noexcept
{
    return lastBridgeReactionForce_;
}

float AcustraEngine::getLastBridgeBodyForce() const noexcept
{
    return lastBridgeBodyForce_;
}

float AcustraEngine::getLastBridgeTailForce() const noexcept
{
    return lastBridgeTailForce_;
}

float AcustraEngine::getLastSympatheticRadiationForce() const noexcept
{
    return lastSympatheticRadiationForce_;
}

float AcustraEngine::getLastBridgePower() const noexcept
{
    return lastBridgePower_;
}

float AcustraEngine::getLastBridgeBodyPower() const noexcept
{
    return lastBridgeBodyPower_;
}

float AcustraEngine::getLastBridgeTailPower() const noexcept
{
    return lastBridgeTailPower_;
}

} // namespace acustra
