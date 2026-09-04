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
#include <span>

namespace acustra
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;
constexpr int localMaximumDelaySamples = 8192;
// The g21 minimum-phase residues retain the calibrated H1 coefficient scale
// and the measured treble/bass microphone balance. The string drive remains a
// reference-scaled bridge-force proxy, so this is an output calibration rather
// than an absolute-SPL claim; it changes no spectrum, decay, or coupling.
// Overall level is excluded from the descriptor fit. This fixed calibration
// places the quietest public physical render just above -20 dBFS while keeping
// ordinary output well below the limiter's -1 dBFS knee.
constexpr float radiationReferenceGain = 18.0f;
static_assert(detail::measuredSteelBodyModes.size() <= ACUSTRA_BODY_MODE_COUNT);
static_assert(detail::measuredNylonBodyModes.size() <= ACUSTRA_BODY_MODE_COUNT);
static_assert(detail::measuredSteelBridgeModes.size()
              <= ACUSTRA_BRIDGE_MODE_COUNT);
static_assert(detail::measuredNylonBridgeModes.size()
              <= ACUSTRA_BRIDGE_MODE_COUNT);

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

// Woodhouse, Acta Acustica 90 (2004) 945-965, Table I (corrected), D'Addario
// Pro Arte "Composites, hard tension" strings at L = 0.65 m - a different
// set from the EJ45 family the density/modulus tables above use, so this
// pairs Woodhouse's measured EI with the engine's own EJ45-derived tensions
// rather than his (his string tensions are 71.6/73.9/71.2/58.3/53.4/70.3 N,
// E2..E4, against the engine's 70.3/78.6/80.4/58.1/62.0/73.8 N - up to a 16%
// shift in B on B3). EI in N*m^2, published E4/B3/G3/D3/A2/E2
// 130/160/310/51/40/57 x1e-6 reversed into engine order (E2..E4). Measured
// directly rather than inferred from the outside diameter and a handbook
// nylon modulus: bending stiffness of a wound bass or a plasticised jacket
// is not E*pi*d^4/64 on the outside diameter, and Lynch-Aird and Woodhouse
// (Materials 10(5):497, 2017) attribute plain nylon's own dynamic bending
// modulus (~13 GPa implied) exceeding the handbook static 2.7 GPa to the
// same effect.
constexpr std::array<float, AcustraEngine::stringCount> nylonBendingEI {{
    57.0e-6f, 40.0e-6f, 51.0e-6f, 310.0e-6f, 160.0e-6f, 130.0e-6f
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

// Axial rigidity E*A, in newtons: what a bend works against. Steel uses the
// same effective core the bending and attack-pitch models use, since a wound
// string's wrap carries almost no axial load, and plain nylon its own
// diameter and modulus. Nylon's three wound basses return zero: their
// tabulated density is an effective composite that is right for transverse
// mass and wrong for axial stiffness - the same reason the longitudinal drive
// is left out for them - so what a stretch does to their pitch is not
// something this data fixes, and they keep the length convention below.
float stringAxialRigidity(bool steel, int stringIndex) noexcept
{
    const auto index = static_cast<std::size_t>(stringIndex);
    if (!steel && stringIndex < 3)
        return 0.0f;
    const float diameter = steel ? steelBendingDiameter[index]
                                 : nylonDiameterMetres[index];
    const float youngsModulus = steel ? steelYoungsModulus
                                      : nylonYoungsModulus[index];
    return youngsModulus * 0.25f * pi * diameter * diameter;
}

// The tension a requested interval needs, from Grimes, PLoS ONE 9(7):e102088
// (2014), Eq. 6. His bent string stretches by e = 1/cos(theta) - 1, which is
// the strain dT/EA, so it carries tension T = T0 + dT, its mass per length
// falls to mu0/(1+e), and the path it vibrates along grows to L0(1+e). Those
// last two together are the mu0(1+e) his Eq. 6 puts under the root at the
// unstretched length: f = (1/2L0) sqrt(T/(mu0(1+e))), so the ratio to the
// open string is f/f0 = sqrt((T/T0)/(1+e)). Requiring that ratio to be r
// inverts to dT = T0 (r^2 - 1) / (1 - r^2 T0/EA). Validated in that paper on
// measured Ernie Ball sets, with plain steel at 177.6-188.4 GPa against the
// 200 GPa this engine's table uses.
float bentStringTension(float tension, float axialRigidity,
                        float frequencyRatio) noexcept
{
    // The denominator vanishes where the string's own extension would eat the
    // whole of the added tension - a string past breaking, not a bend - so
    // the tension the model follows stops half way to it, which is 36.8
    // semitones up on the plain high E and 44.0 on the wound low E, well
    // inside the +-96 semitones a bend input can ask for. Past that the pitch
    // still follows the wheel through the delay, as a slide does.
    const float ratioSquared = std::min(frequencyRatio * frequencyRatio,
                                        0.5f * axialRigidity / tension);
    const float added = tension * (ratioSquared - 1.0f)
                      / (1.0f - ratioSquared * tension / axialRigidity);
    // Grimes' law describes a string in tension; a slackened one leaves it.
    return std::max(tension + added, 0.05f * tension);
}

// A steel-string and a classical are different instruments, and neither
// archive record describes the other, so each material plays the guitar its
// own banks were measured on. The two banks need not be the same length; the
// arrays are sized to the larger and the surplus slots are silenced.
std::span<const detail::MeasuredBridgeMode> measuredBridgeBank(
    StringMaterial material) noexcept
{
    if (material == StringMaterial::Steel)
        return detail::measuredSteelBridgeModes;
    return detail::measuredNylonBridgeModes;
}

std::span<const detail::MeasuredBodyMode> measuredBodyBank(
    StringMaterial material) noexcept
{
    if (material == StringMaterial::Steel)
        return detail::measuredSteelBodyModes;
    return detail::measuredNylonBodyModes;
}

// Where a string crosses the saddle, in units of the half-separation between
// the archive's two bridge impacts. Method.pdf section 2b puts the treble
// impact between B3 and E4 and the bass impact between E2 and A2, so they are
// the midpoints of string pairs (4,5) and (0,1) and sit at u = +1 and -1; the
// six strings are then at (i - 2.5)/2. Only the ratio enters, so the set-up
// spacing at the saddle -- the one strumDelaySamples uses -- cancels.
// Tools/GenerateMeasuredBridge.py fits the bank against these same arms.
constexpr float saddleLeverArm(int stringIndex) noexcept
{
    return 0.5f * (static_cast<float>(stringIndex) - 2.5f);
}

bool includeMeasuredBridgeMode(const detail::MeasuredBridgeMode& mode) noexcept
{
#if defined(ACUSTRA_ANALYSIS_EXCLUDE_MEASURED_OPEN_STRINGS)
    // Analysis only, and applied to whichever bank the material selects: the
    // archive's setup photographs show installed strings.  Of the retained
    // candidates only the steel bank's 82.764 Hz lies within 25 cents of the
    // one open string this surrogate names (E2, 82.407 Hz); on the nylon bank
    // it removes nothing.  Do not ship it: the measurement string's impedance
    // metadata is unavailable, and the generator's own open-string screen
    // already gates every retained mode near a standard open string on its
    // resolved Q.
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

// The loop's fractional delay is a second-order Thiran allpass read from the
// line at an integer tap: an allpass has magnitude exactly one at every
// frequency, and Thiran's coefficients make its phase delay maximally flat at
// D samples around DC (Laakso, Valimaki, Karjalainen and Laine, "Splitting
// the Unit Delay: Tools for Fractional Delay Filter Design", IEEE Signal
// Processing Magazine 13(1), 1996, 30-60, Eq. 86 on p. 49, "Maximally Flat
// Group Delay Design of Allpass Filters"). The four-point
// Catmull-Rom read it replaces lost 0.23 and 0.53 dB per pass at 8 and 10 kHz
// at a half-sample fraction and nothing at an integer one, so a string's
// upper partials decayed at a rate set by the accidental fraction of its loop
// length and by the host rate.
//
// D is kept in [1.1, 2.1). Both edges of that band sit next to a delay at
// which the section is exact - at D = 2 it is two unit delays, at D = 1 one -
// so the loop phase steps by only 0.037 rad at half the Nyquist rate when a
// slewing delay crosses a band edge and the tap moves, and by a higher power
// of frequency below it; its largest phase-delay error inside the band is
// 0.045 rad there, at D = 1.59. A first-order section on its own best band,
// [0.5, 1.5), is worse at both: its phase-delay error runs from 0.142 rad at
// the bottom edge to 0.391 at the top, and its band-edge step is 0.533 rad.
// Going nearer D = 1 shrinks both further, but the pole radius is
// already 0.87 at 1.1 and at D = 1 exactly a pole sits on the unit circle
// with only a zero to cancel it. The band is one sample wide, so the tap is a
// function of the delay alone: the loop and the design that tunes it split
// the same delay the same way, which a hysteretic band would not, and a
// crossing moves D a whole sample to the far edge, so a slewing delay cannot
// chatter across it.
int delayAnchor(double samples) noexcept
{
    return static_cast<int>(std::floor(samples - 1.1));
}

void thiranCoefficients(double samples, double& a1, double& a2) noexcept
{
    a1 = -2.0 * (samples - 2.0) / (samples + 1.0);
    a2 = (samples - 2.0) * (samples - 1.0)
       / ((samples + 1.0) * (samples + 2.0));
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

// Phase lag of tap plus allpass. The tap is passed in rather than derived
// from the delay: folding delayAnchor() into this leaves the residual the
// tuning solves discontinuous where D crosses its band edge, and the
// collocation stalls on it. Every solve below instead pins one tap, which
// makes its residual smooth in the delay and lets the delay travel as far as
// the pole pair moves it, and then re-derives the tap from its own answer and
// solves again at that tap.
double thiranDelayPhase(double samples, int anchor, double omega) noexcept
{
    double a1 = 0.0;
    double a2 = 0.0;
    thiranCoefficients(samples - static_cast<double>(anchor), a1, a2);
    return static_cast<double>(anchor) * omega
         + secondOrderAllpassPhase(a1, a2, omega);
}

double tunedLoopDelay(double fundamental, double sampleRate,
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
    for (int round = 0; round < 3; ++round)
    {
        const int anchor = delayAnchor(delay);
        for (int iteration = 0; iteration < 6; ++iteration)
        {
            const double residual = thiranDelayPhase(delay, anchor, omega)
                                  + fixedPhase - twoPiDouble;
            if (std::abs(residual) < 1.0e-11)
                break;
            constexpr double step = 0.01;
            const double slope
                = (thiranDelayPhase(delay + step, anchor, omega)
                 - thiranDelayPhase(delay - step, anchor, omega))
                / (2.0 * step);
            if (std::abs(slope) < 1.0e-12)
                break;
            delay = std::clamp(delay - residual / slope, 3.0,
                static_cast<double>(localMaximumDelaySamples - 3));
        }
        if (delayAnchor(delay) == anchor)
            break;
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
        calibration.delay = tunedLoopDelay(
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

    // The integer tap the delay is split at. It is held fixed inside a
    // solve, so the residual stays smooth in the delay (see
    // thiranDelayPhase), and re-derived from each answer by solve() below.
    int splitAnchor = 0;
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
            residuals[index] = thiranDelayPhase(values[0], splitAnchor, omega)
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

    // Nine damped Gauss-Newton steps on (delay, decayRatio, poleRatio)
    // against the three collocation residuals, at the tap splitAnchor
    // currently holds. The delay is free: the pole pair's own lag at the
    // fundamental moves it by several samples from the starting point, so a
    // solve that could not leave its tap's fraction band could not converge.
    constexpr double steps[] { 0.02, 0.01, 0.01 };
    const auto refine = [&] (double values[3])
    {
        for (int iteration = 0; iteration < 9; ++iteration)
        {
            double residuals[3] {};
            evaluate(values, residuals);
            const double oldNorm = maximumResidual(residuals);
            if (oldNorm < 1.0e-10)
                break;

            double jacobian[3][3] {};
            for (int column = 0; column < 3; ++column)
            {
                double higher[] { values[0], values[1], values[2] };
                double lower[] { values[0], values[1], values[2] };
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
                    std::clamp(values[0] + amount * update[0], 3.0,
                        static_cast<double>(localMaximumDelaySamples - 3)),
                    std::clamp(values[1] + amount * update[1], 0.1, 30.0),
                    std::clamp(values[2] + amount * update[2], 0.05, 15.0)
                };
                double candidateResiduals[3] {};
                evaluate(candidate, candidateResiduals);
                if (maximumResidual(candidateResiduals) < oldNorm)
                {
                    std::copy(std::begin(candidate), std::end(candidate),
                        values);
                    accepted = true;
                    break;
                }
            }
            if (!accepted)
                break;
        }
    };
    // Re-derive the tap from the answer and solve again if it moved, so the
    // split the collocation was fitted at is the split the loop reads with.
    // The first round starts from a tap the pole pair has not yet moved the
    // delay off, so it is the one that walks; the second finds its own
    // answer's tap unchanged and only trims the delay by about a thousandth
    // of a sample.
    const auto solve = [&] (double values[3])
    {
        for (int round = 0; round < 4; ++round)
        {
            splitAnchor = delayAnchor(values[0]);
            refine(values);
            if (delayAnchor(values[0]) == splitAnchor)
                break;
        }
        splitAnchor = delayAnchor(values[0]);
    };

    double parameters[] {
        128.0, calibration.decayRatio, calibration.poleRatio
    };
    secondOrderAllpassCoefficients(
        omega0, parameters[1], parameters[2], calibration.a1, calibration.a2);
    parameters[0] = tunedLoopDelay(
        fundamental, sampleRate, broadCoefficient, broadMix,
        highCoefficient, highMix, calibration.a1, calibration.a2);
    solve(parameters);

    // This is a Newton solve on a non-convex residual, so it can stall in a
    // corner of the (decayRatio, poleRatio) box instead of reaching the
    // near-zero residual a well-posed collocation admits. Which starting
    // points stall is not a property of the string: at 384 kHz a top-fret
    // treble stalls at a residual of 0.40 rad from the default start on
    // steel and 0.22 on nylon, where a coarse sweep of the box shows a
    // solution within 0.014 rad of exact, and moving to the Thiran read
    // moved which notes land in which basin. So the fallback restarts the
    // same cheap three-parameter solve from a spread of both starting ratios
    // and keeps the lowest-residual run, which puts every note of the
    // rate/material/fret matrix back inside 3.0 cents of H2-H12 placement.
    // It costs nothing on a note whose first solve converged: the sweep is
    // skipped entirely once the residual is below 1e-9.
    {
        double residuals[3] {};
        evaluate(parameters, residuals);
        double bestNorm = maximumResidual(residuals);
        int bestAnchor = splitAnchor;
        for (const double startDecayRatio : { 5.0, 10.0, 20.0 })
        for (const double startPoleRatio : { 1.0, 2.0, 4.0, 8.0 })
        {
            if (bestNorm < 1.0e-9)
                break;
            double a1 = 0.0;
            double a2 = 0.0;
            secondOrderAllpassCoefficients(
                omega0, startDecayRatio, startPoleRatio, a1, a2);
            double candidate[] {
                tunedLoopDelay(fundamental, sampleRate, broadCoefficient,
                    broadMix, highCoefficient, highMix, a1, a2),
                startDecayRatio, startPoleRatio
            };
            solve(candidate);
            double candidateResiduals[3] {};
            evaluate(candidate, candidateResiduals);
            const double candidateNorm = maximumResidual(candidateResiduals);
            if (candidateNorm < bestNorm)
            {
                bestNorm = candidateNorm;
                bestAnchor = splitAnchor;
                std::copy(std::begin(candidate), std::end(candidate),
                    parameters);
            }
        }
        // Every candidate is compared at its own tap, which is the tap the
        // loop would read it with; the winner's has to be put back.
        splitAnchor = bestAnchor;
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
    // -1 is the "nothing received yet" sentinel for both; see mpeTimbre_ /
    // mpePressure_'s declaration. reset() re-applies this on every prepare(),
    // but the sentinel has to hold from construction too, before the first
    // prepare() -- the {} default-member-initialiser above zero-inits them,
    // which is a different, meaningful value (0 is a real received CC74/
    // pressure of "none"/"no grip"), not this sentinel.
    mpeTimbre_.fill(-1.0f);
    mpePressure_.fill(-1.0f);

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
                fittedPhysicalCalibration.bridgeConductanceCornerHz),
        bounded(source.bridgeTailLengthMetres, 0.00325f, 0.060f,
                fittedPhysicalCalibration.bridgeTailLengthMetres),
        bounded(source.longitudinalGain, 0.0f, 0.5f,
                fittedPhysicalCalibration.longitudinalGain),
        bounded(source.longitudinalQ, 10.0f, 400.0f,
                fittedPhysicalCalibration.longitudinalQ),
        bounded(source.polarisationEndCorrectionMetres, 0.0f, 0.82e-3f,
                fittedPhysicalCalibration.polarisationEndCorrectionMetres)
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
    allpassY1 = 0.0f;
    allpassY2 = 0.0f;
    broadLossFilter.reset();
    lossFilter.reset();
    dispersion.reset();
    bridgeDerivative.reset();
    derivativeNeedsPriming = true;
    appliedReleaseGain = 1.0f;
    requestedReleaseGain = 1.0f;
    releaseGainStep = 0.0f;
}

float AcustraEngine::StringLoop::bridgeVelocity(
    float incident, float sampleRateRatio) noexcept
{
    if (derivativeNeedsPriming)
    {
        bridgeDerivative.reset(incident);
        derivativeNeedsPriming = false;
    }
    else if (derivativeCrossesRelease)
    {
        // The release gain is a loss per round trip, but advance() applies it
        // to the sample it returns, so the moment it changes the whole wave
        // steps by that factor at once. The hand landing on a string damps it;
        // it does not move it, so the step is not motion either.
        derivativeCrossesRelease = false;
        return bridgeDerivative.processAcrossRelease(
            incident, sampleRateRatio);
    }
    return bridgeDerivative.process(incident, sampleRateRatio);
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

float AcustraEngine::FixedDerivative::processAcrossRelease(
    float input, float sampleRateRatio) noexcept
{
    int previous = index - 1;
    while (previous < 0)
        previous += static_cast<int>(history.size());
    const float shift = input - history[static_cast<std::size_t>(previous)];
    for (auto& value : history)
        value += shift;
    return process(input, sampleRateRatio);
}

float AcustraEngine::StringLoop::readDelay(float samples) noexcept
{
    const float bounded = AcustraEngine::clamp(
        samples, 3.0f, static_cast<float>(maximumDelaySamples - 3));
    const int whole = delayAnchor(bounded);
    const float fraction = bounded - static_cast<float>(whole);
    const auto at = [&] (int samplesAgo)
    {
        return delay[static_cast<std::size_t>(
            wrapDelayIndex(writeIndex - samplesAgo))];
    };

    // Second-order Thiran allpass, exactly lossless at every frequency (see
    // delayAnchor). Direct form I: the only state is the two previous
    // outputs, and the previous inputs are read from the line at whichever
    // tap is current, so when a slewing delay crosses a band edge and moves
    // the tap and the coefficients together, the filter's memory of its input
    // is already the memory it would have had at the new tap - the transient
    // elimination of Valimaki, Laakso and Mackenzie, "Elimination of
    // transients in time-varying allpass fractional delay filters with
    // application to digital waveguide modeling", ICMC 1995, 327-334, which
    // for a line-read section is exactly this signal-valued state.
    double a1 = 0.0;
    double a2 = 0.0;
    thiranCoefficients(static_cast<double>(fraction), a1, a2);
    const float first = static_cast<float>(a1);
    const float second = static_cast<float>(a2);
    const float output = second * at(whole) + first * at(whole + 1)
                       + at(whole + 2)
                       - first * allpassY1 - second * allpassY2;
    allpassY2 = allpassY1;
    allpassY1 = output;
    return output;
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
    if (releaseGain != requestedReleaseGain)
    {
        requestedReleaseGain = releaseGain;
        releaseGainStep = std::abs(releaseGain - appliedReleaseGain)
                        / std::max(currentDelay, 1.0f);
    }
    if (appliedReleaseGain < requestedReleaseGain)
        appliedReleaseGain = std::min(requestedReleaseGain,
                                      appliedReleaseGain + releaseGainStep);
    else if (appliedReleaseGain > requestedReleaseGain)
        appliedReleaseGain = std::max(requestedReleaseGain,
                                      appliedReleaseGain - releaseGainStep);
    return reflected * loopGain * appliedReleaseGain;
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
    pastHeave = 0.0f;
    pastRock = 0.0f;
    tailIntegratedForce = 0.0f;
    tailIntegratedMoment = 0.0f;
    previousDisplacement = 0.0f;
    previousRotation = 0.0f;
    displacement = 0.0f;
    rotation = 0.0f;
    mainIntegratedForce = 0.0f;
    mainIntegratedMoment = 0.0f;
    bodyIntegratedForce = 0.0f;
    bodyIntegratedMoment = 0.0f;
    for (auto& mode : heaveModes)
        mode.reset();
    for (auto& mode : rockModes)
        mode.reset();
}

void AcustraEngine::BridgeLoad::process(const BridgeDrive& drive,
                                        float samplePeriod) noexcept
{
    // The saddle has two degrees of freedom, heave and rock, because that is
    // what the archive measures: two impacts and two accelerometers, "taken
    // together allow to trace two of the degrees of freedom of the bridge"
    // (Method.pdf 2b). A string at lever arm u ends on x_u = x + u*theta and
    // pushes F_u = Z(2a_u - x_u) there, so the strings contribute the force
    // sum and its first moment, and the same for the anchor stubs, each of
    // which sits at its own string's u. Solving
    //     [x; theta] = Y (b - G [x; theta]),  G = string + anchor moments,
    // is one 2x2 per sample and stays algebraic-loop-free because Y here is
    // only the immediate part of the modal bank.
    //
    // DAFx-26 attaches the measured body a short distance from the string's
    // end, leaving a fixed-end tail. Below its first resonance that segment
    // is the passive spring K=T/L_t; trapezoidal integration gives its
    // current-step impedance K*dt/2. Its three moments are the anchor's
    // stiffness matrix in the same two coordinates.
    const float half = 0.5f * samplePeriod;
    const float c0 = half * drive.stiffness0;
    const float c1 = half * drive.stiffness1;
    const float c2 = half * drive.stiffness2;
    const float historyForce = tailIntegratedForce
        + c0 * previousDisplacement + c1 * previousRotation;
    const float historyMoment = tailIntegratedMoment
        + c1 * previousDisplacement + c2 * previousRotation;

    const float g00 = drive.impedance0 + c0;
    const float g01 = drive.impedance1 + c1;
    const float g11 = drive.impedance2 + c2;
    const float b0 = drive.incidentHeave - historyForce;
    const float b1 = drive.incidentRock - historyMoment;

    // (I + Y G) [x; theta] = Y b + past
    const float m00 = 1.0f + immediateHeave * g00 + immediateCross * g01;
    const float m01 = immediateHeave * g01 + immediateCross * g11;
    const float m10 = immediateCross * g00 + immediateRock * g01;
    const float m11 = 1.0f + immediateCross * g01 + immediateRock * g11;
    const float r0 = immediateHeave * b0 + immediateCross * b1 + pastHeave;
    const float r1 = immediateCross * b0 + immediateRock * b1 + pastRock;
    const float determinant = m00 * m11 - m01 * m10;
    if (!(std::abs(determinant) > 1.0e-12f) || !std::isfinite(b0)
        || !std::isfinite(b1))
    {
        reset();
        return;
    }

    const float nextDisplacement = (r0 * m11 - r1 * m01) / determinant;
    const float nextRotation = (r1 * m00 - r0 * m10) / determinant;
    if (!std::isfinite(nextDisplacement) || !std::isfinite(nextRotation))
    {
        reset();
        return;
    }
    displacement = nextDisplacement;
    rotation = nextRotation;

    const float nextTailForce = historyForce
        + c0 * displacement + c1 * rotation;
    const float nextTailMoment = historyMoment
        + c1 * displacement + c2 * rotation;
    // The string force less what the anchor takes, in both coordinates.
    const float bodyForce = b0 - g00 * displacement - g01 * rotation;
    const float bodyMoment = b1 - g01 * displacement - g11 * rotation;

    float nextPastHeave = 0.0f;
    float nextPastRock = 0.0f;
    for (std::size_t index = 0; index < heaveModes.size(); ++index)
    {
        const float heaveState = heaveModes[index].processPast(bodyForce);
        nextPastHeave += residueHeave[index] * heaveState;
        if (!rocking[index])
            continue;
        const float rockState = rockModes[index].processPast(bodyMoment);
        nextPastHeave += residueCross[index] * rockState;
        nextPastRock += residueCross[index] * heaveState
                      + residueRock[index] * rockState;
    }
    pastHeave = std::isfinite(nextPastHeave) ? nextPastHeave : 0.0f;
    pastRock = std::isfinite(nextPastRock) ? nextPastRock : 0.0f;
    previousDisplacement = displacement;
    previousRotation = rotation;
    tailIntegratedForce = nextTailForce;
    tailIntegratedMoment = nextTailMoment;
    mainIntegratedForce = drive.incidentHeave
        - drive.impedance0 * displacement - drive.impedance1 * rotation;
    mainIntegratedMoment = drive.incidentRock
        - drive.impedance1 * displacement - drive.impedance2 * rotation;
    bodyIntegratedForce = bodyForce;
    bodyIntegratedMoment = bodyMoment;
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
    prepared_ = true;
    reset();
}

void AcustraEngine::reset() noexcept
{
    resetSoundState();
    palmMute_ = targetPalmMute_;
    pitchBendSemitones_.fill(0.0f);
    mpeTimbre_.fill(-1.0f);
    mpePressure_.fill(-1.0f);
    sustainPedals_.fill(false);
    vibrato_ = 0.0f;
    vibratoPhase_ = 0.0f;
    vibratoOnset_ = 0.0f;
    noteOrder_ = 0;
    controlCounter_ = 0;
    parameters_ = sanitise(targetParameters_);
    bodyAmount_ = parameters_.bodyAmount;
    width_ = parameters_.stereoWidth;
    outputGain_ = parameters_.outputGain;
    bodyConfigured_ = false;
    // Both banks follow the string material, which is only known here: prepare
    // runs before the pending parameters are adopted.
    configureBridge();
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
        voice.returnSamples = 0;
        returnToOpenString(voice, string, true);
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
    bridgeRotationDerivative_.reset();
    bridgeForceDerivative_.reset();
    bridgeForceMomentDerivative_.reset();
    bridgeBodyForceDerivative_.reset();
    bridgeBodyMomentDerivative_.reset();
    bridgeTailForceDerivative_.reset();
    bridgeTailMomentDerivative_.reset();
    lastBridgeVelocity_ = 0.0f;
    lastBridgeReactionForce_ = 0.0f;
    lastBridgeBodyForce_ = 0.0f;
    lastBridgeTailForce_ = 0.0f;
    lastSympatheticRadiationForce_ = 0.0f;
    lastLongitudinalForce_ = 0.0f;
    lastBridgePower_ = 0.0f;
    lastBridgeBodyPower_ = 0.0f;
    lastBridgeTailPower_ = 0.0f;
    bridgeDerivativesNeedPriming_ = true;
    bridgeDerivativesCrossRelease_ = false;
    lastImpedanceSum_ = 0.0f;
    lastImpedanceMoment_ = 0.0f;
    lastImpedanceInertia_ = 0.0f;
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

    // The string material selects which measured guitar the bridge and body
    // banks come from, so it reconfigures both. configureBody crossfades; the
    // bridge filters are a different instrument's and are rebuilt, which the
    // junction's cross-release below already covers.
    if (bodyChanged || stringChanged)
        configureBody();
    if (stringChanged)
        configureBridge();

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
            returnToOpenString(voice, string, true);
        else if (constructionChanged || ageChanged || stringChanged)
            configureVoice(voice, string, voice.midiNote, false);
    }
    // Switching the string set or the tuning under a ringing chord changes
    // every string's impedance at once, so the junction's wave variables step
    // with the port. That is the strings being exchanged, not the bridge
    // moving, and differencing it made a click 26 times the chord it landed
    // on. Shape, material and age move smoothly and are left alone.
    if (stringChanged || tuningChanged)
        bridgeDerivativesCrossRelease_ = true;

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
    if (vibrato_ > 0.0f)
    {
        // Erkut's two measured rates as the wheel's endpoints, in the
        // direction he measured them: the fast vibrato is the shallow one.
        const float rate = 4.9f + vibrato_ * (1.4f - 4.9f);
        vibratoPhase_ += twoPi * rate
            * static_cast<float>(controlPeriod) * inverseSampleRate_;
        if (vibratoPhase_ >= twoPi)
            vibratoPhase_ -= twoPi;
        vibratoOnset_ = std::min(1.0f, vibratoOnset_
            + static_cast<float>(controlPeriod) * inverseSampleRate_ / 0.5f);
    }
    else
    {
        vibratoPhase_ = 0.0f;
        vibratoOnset_ = 0.0f;
    }
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
    const auto bank = measuredBodyBank(parameters_.stringMaterial);

    for (int index = 0; index < bodyModeCount; ++index)
    {
        auto& mode = bodyModes_[static_cast<std::size_t>(index)];
        if (static_cast<std::size_t>(index) >= bank.size())
        {
            mode = {};
            continue;
        }
        const auto& measured = bank[static_cast<std::size_t>(index)];
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
    bridgeLoad_.immediateHeave = 0.0f;
    bridgeLoad_.immediateCross = 0.0f;
    bridgeLoad_.immediateRock = 0.0f;
    const float rate = static_cast<float>(sampleRate_);
    const float bilinear = 2.0f * rate;
    // Each mode's residue matrix multiplies the continuous mobility
    // s/(s^2 + 2 damping s + omega^2), which is shared by both coordinates.
    // A per-mode prewarped bilinear transform preserves its measured centre
    // frequency, and a positive semidefinite residue matrix keeps every
    // string's own combination of the three positive real.
    const auto configure = [&] (std::size_t index, float frequency, float q,
                                float heave, float cross, float rock)
    {
        auto& heaveMode = bridgeLoad_.heaveModes[index];
        auto& rockMode = bridgeLoad_.rockModes[index];
        bridgeLoad_.residueHeave[index] = 0.0f;
        bridgeLoad_.residueCross[index] = 0.0f;
        bridgeLoad_.residueRock[index] = 0.0f;
        bridgeLoad_.rocking[index] = false;
        const bool active = (heave > 0.0f || rock > 0.0f)
                          && frequency < 0.45f * rate;
        if (!active)
        {
            for (auto* mode : { &heaveMode, &rockMode })
            {
                mode->denominator1 = mode->denominator2 = 0.0f;
                mode->numerator1 = mode->numerator2 = 0.0f;
                mode->reset();
            }
            return;
        }
        const float omega = bilinear * std::tan(pi * frequency / rate);
        const float damping = omega / (2.0f * q);
        const float denominator0 = bilinear * bilinear
            + 2.0f * damping * bilinear + omega * omega;
        const float denominator1 = (-2.0f * bilinear * bilinear
            + 2.0f * omega * omega) / denominator0;
        const float denominator2 = (bilinear * bilinear
            - 2.0f * damping * bilinear + omega * omega) / denominator0;
        const float immediate = bilinear / denominator0;
        for (auto* mode : { &heaveMode, &rockMode })
        {
            mode->denominator1 = denominator1;
            mode->denominator2 = denominator2;
            mode->numerator1 = -immediate * denominator1;
            mode->numerator2 = -immediate * (1.0f + denominator2);
            mode->reset();
        }
        bridgeLoad_.residueHeave[index] = heave;
        bridgeLoad_.residueCross[index] = cross;
        bridgeLoad_.residueRock[index] = rock;
        bridgeLoad_.rocking[index] = rock > 0.0f || cross != 0.0f;
        bridgeLoad_.immediateHeave += heave * immediate;
        bridgeLoad_.immediateCross += cross * immediate;
        bridgeLoad_.immediateRock += rock * immediate;
    };

    const auto bank = measuredBridgeBank(parameters_.stringMaterial);
    const float scale = physicalCalibration_.bridgeMobilityScale;
    for (std::size_t index = 0;
         index < static_cast<std::size_t>(bridgeModeCount); ++index)
    {
        if (index >= bank.size())
        {
            configure(index, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
            continue;
        }
        const auto& measured = bank[index];
        const bool include = includeMeasuredBridgeMode(measured);
        configure(index, measured.frequency, measured.q,
                  include ? measured.heave * scale : 0.0f,
                  include ? measured.cross * scale : 0.0f,
                  include ? measured.rock * scale : 0.0f);
    }

    // The plate conductance floor is the dense overlap of a plate's own
    // driving-point response, which the archive never resolves into a
    // rocking pair, so it enters as heave alone.
    const auto plate = plateConductanceMode(physicalCalibration_);
    configure(static_cast<std::size_t>(bridgeModeCount), plate.frequency,
              plate.q, plate.weight, 0.0f, 0.0f);
    bridgeLoad_.pastHeave = 0.0f;
    bridgeLoad_.pastRock = 0.0f;
}

float AcustraEngine::bridgePhaseDelay(float frequency,
                                      int stringIndex) const noexcept
{
    if (!bridgeCouplingEnabled_ || !(frequency > 0.0f))
        return 0.0f;

    const float rate = static_cast<float>(sampleRate_);
    const float bilinear = 2.0f * rate;
    const float digitalOmega = twoPi * frequency / rate;
    const std::complex<float> s(
        0.0f, bilinear * std::tan(0.5f * digitalOmega));
    // Each string ends at its own point on the saddle, so it is its own
    // combination of the heaving and rocking banks that folds into its tuned
    // delay, not one shared driving point.
    const float arm = saddleLeverArm(stringIndex);
    std::complex<float> mobilityHeave {};
    std::complex<float> mobilityCross {};
    std::complex<float> mobilityRock {};
    for (const auto& measured
         : measuredBridgeBank(parameters_.stringMaterial))
    {
        if (measured.frequency >= 0.45f * rate
            || !includeMeasuredBridgeMode(measured))
            continue;
        const float omega = bilinear * std::tan(
            pi * measured.frequency / rate);
        const float damping = omega / (2.0f * measured.q);
        const std::complex<float> shape
            = physicalCalibration_.bridgeMobilityScale * s
            / (s * s + 2.0f * damping * s + omega * omega);
        mobilityHeave += measured.heave * shape;
        mobilityCross += measured.cross * shape;
        mobilityRock += measured.rock * shape;
    }

    const auto plate = plateConductanceMode(physicalCalibration_);
    if (plate.weight > 0.0f && plate.frequency < 0.45f * rate)
    {
        const float omega = bilinear * std::tan(pi * plate.frequency / rate);
        const float damping = omega / (2.0f * plate.q);
        mobilityHeave += plate.weight * s
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
    // Body and anchor are in parallel at the saddle, but on a bridge with two
    // degrees of freedom that parallel has to be taken as matrices and only
    // then read at this string's own point: the anchor a string finds is
    // softer at the ends, where it can rock the bridge against the others,
    // than in the middle.
    float stiffness0 = 0.0f;
    float stiffness1 = 0.0f;
    float stiffness2 = 0.0f;
    bridgeAnchorMoments(stiffness0, stiffness1, stiffness2);
    // (Y^-1 + K/s)^-1 = det(Y) * (adj(Y) + det(Y) K/s)^-1, which needs no
    // division by a determinant that goes to zero wherever the bank has no
    // rocking residue.
    const std::complex<float> determinant
        = mobilityHeave * mobilityRock - mobilityCross * mobilityCross;
    const std::complex<float> ratio = determinant / s;
    const std::complex<float> a00 = mobilityRock + ratio * stiffness0;
    const std::complex<float> a01 = -mobilityCross + ratio * stiffness1;
    const std::complex<float> a11 = mobilityHeave + ratio * stiffness2;
    const std::complex<float> inner = a00 * a11 - a01 * a01;
    std::complex<float> effectiveMobility {};
    if (std::abs(inner) > 0.0f)
    {
        effectiveMobility = determinant
            * (a11 - 2.0f * arm * a01 + arm * arm * a00) / inner;
    }
    else
    {
        // A bank with no rocking residue anywhere leaves the determinant, and
        // with it the whole adjugate above, exactly zero.  The rocking
        // coordinate is then immovable, every string sees the same heave port
        // with the anchor springs in parallel, and the load is the scalar
        // (1/Yhh + k0/s)^-1 the one-point junction used.  Neither committed
        // bank reaches this, but AuditBridgeFits.py emits heave-only banks.
        const std::complex<float> denominator
            = s + stiffness0 * mobilityHeave;
        if (!(std::abs(denominator) > 0.0f))
            return 0.0f;
        effectiveMobility = mobilityHeave * s / denominator;
    }
    // This is the folded full-round-trip multiplier -b/a.  Its phase is the
    // phase contributed by both measured body motion and the saddle anchor; the
    // speaking-string delay is shortened by exactly that amount when tuned.
    const std::complex<float> selfReflection
        = (characteristicAdmittance - effectiveMobility)
        / (characteristicAdmittance + effectiveMobility);
    return -std::arg(selfReflection) / digitalOmega;
}

void AcustraEngine::bridgeAnchorMoments(float& stiffness0,
                                        float& stiffness1,
                                        float& stiffness2) const noexcept
{
    // Every string is anchored behind the saddle whether or not it is being
    // played, but each stub stands at its own point on it, so the six springs
    // are one stiffness matrix rather than one sum.
    stiffness0 = stiffness1 = stiffness2 = 0.0f;
    for (int string = 0; string < stringCount; ++string)
    {
        const float arm = saddleLeverArm(string);
        const float stiffness
            = voices_[static_cast<std::size_t>(string)].bridgeTailStiffness;
        stiffness0 += stiffness;
        stiffness1 += arm * stiffness;
        stiffness2 += arm * arm * stiffness;
    }
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
    float memberBendSemitones = 0.0f;
    if (voice.played && voice.mpeMember)
    {
        const float memberBend = voice.memberPitchBendFrozen
            ? voice.frozenMemberPitchBendSemitones
            : pitchBendSemitones_[channel];
        performedBend = pitchBendSemitones_[0] + memberBend;
        memberBendSemitones = memberBend;
    }
    // The wheel's vibrato is the fretting hand modulating the string's
    // tension at a fixed length (see vibratoSemitones), and a tension
    // modulation is heard as a pitch modulation: the same excursion goes into
    // the performed interval, so the tuned delay carries it, and into the
    // tension below, so the inharmonicity and the junction port carry it too.
    // A string whose axial stiffness this data does not fix - nylon's wound
    // basses, see stringAxialRigidity - gets none, rather than a pitch move
    // with no tension behind it. The wheel down is + 0.0f, which is exact.
    const float axialRigidity = stringAxialRigidity(steel, stringIndex);
    const float vibratoInterval = axialRigidity > 0.0f
        ? vibratoSemitones(voice, fret) : 0.0f;
    const float performedSemitones = clamp(performedBend, -192.0f, 192.0f)
        + 0.01f * voice.attackPitchCents + vibratoInterval;
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
    // Slide or bend, settled. A channel's pitch bend is a slide: the fretting
    // hand moves along the neck, the sounding length changes and the tension
    // does not. That is what the frequency and delay above have always done
    // and what the README documents, so it is left exactly as it was. An MPE
    // member channel's own bend is the other gesture - one finger pushing one
    // string across the fret, at a length the fret fixes - so it goes through
    // the string's tension: the frequency it asks for is still reached by the
    // delay, but the inharmonicity that goes as 1/T and the impedance the
    // junction reads move with the tension that would produce it. The
    // manager's zone-wide bend stays a slide, because a whole zone bending is
    // a hand moving rather than six fingers pushing. Grimes (see
    // bentStringTension) notes a lateral bend can only raise pitch; a
    // downward member bend is the release of a pre-bend, the same law run
    // backwards. The attack glide stays out of it: it is the model's own
    // few-cent tension transient (updateAttackPitch), already carried as a
    // pitch, and routing it here would modulate the junction port on every
    // note's attack, which no measurement asks for.
    const float tensionSemitones = axialRigidity > 0.0f
        ? memberBendSemitones + vibratoInterval : 0.0f;
    const float bentTension = tensionSemitones != 0.0f
        ? bentStringTension(tension, axialRigidity,
                            std::exp2(tensionSemitones / 12.0f))
        : tension;
    // Steel keeps the E*I = E*(pi*d^4/64) solid-cylinder model on its fitted
    // effective bending diameter and stiffnessScale. Nylon reads Woodhouse's
    // measured E*I directly (see nylonBendingEI above) rather than deriving
    // it from a diameter and a handbook modulus, so nylon.stiffnessScale is
    // not part of the calibration.
    const float stiffness = steel
        ? pi * pi * pi * youngsModulus * bendingDiameter * bendingDiameter
            * bendingDiameter * bendingDiameter / 64.0f
        : pi * pi * nylonBendingEI[index];
    const float stiffnessScale = steel ? physical.stiffnessScale : 1.0f;
    const float inharmonicity = clamp(stiffness * stiffnessScale
        / (bentTension * soundingLength * soundingLength), 0.0f, 0.004f);
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
        // A bend or a vibrato moves B continuously, and this design is an
        // iterative solve, so B is only re-solved once it has moved 0.2%.
        // That relative tolerance applies to every caller, not only to a
        // moving bend: the one other continuous route into B is
        // physical.stiffnessScale, and a fitted move of it smaller than 0.2%
        // now leaves the allpass as designed until something else here
        // changes. 0.2% of B is 0.03 cents of H12 stretch on a low E, two
        // orders below the 3-cent tolerance the dispersion test holds, and
        // every discrete change that reaches here - fret, tuning, string set,
        // a calibration edit worth hearing - moves B by far more than that.
        || std::abs(voice.dispersionDesignInharmonicity - inharmonicity)
               > std::max(1.0e-9f, 0.002f * inharmonicity)
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
    const float rawDelay = static_cast<float>(tunedLoopDelay(
        frequency, sampleRate_, broadLossCoefficient, broadLoss,
        lowpassCoefficient, mutedHighLoss, dispersionA1, dispersionA2));
    // The segment between saddle and anchor does not move when a string is
    // fretted and does not change tension, so its spring T/L is a constant of
    // the string rather than a fraction of the speaking length.
    voice.bridgeTailStiffness = tension / std::max(
        physicalCalibration_.bridgeTailLengthMetres, 1.0e-5f);
    // The longitudinal wave speed is sqrt(E*A/mu) for a wound string, whose
    // axial load the core carries while the whole construction supplies the
    // mass; plain strings use the same expression with their own diameter.
    // Both come from the tables the transverse model already uses.
    {
        // Where the axial construction is determined. Steel's wound basses
        // have a published core diameter and plain nylon is homogeneous, but
        // nylon's three wound basses are given an effective composite density
        // that is right for transverse mass and wrong for axial stiffness -
        // the wrap carries almost no axial load - so their longitudinal speed
        // is not something this data fixes, and they are left out rather than
        // given a frequency nothing supports.
        const bool axialKnown = steel || stringIndex >= 3;
        const float axialDiameter = steel ? steelBendingDiameter[index]
                                          : diameter;
        const float axialArea = 0.25f * pi * axialDiameter * axialDiameter;
        const float longitudinalSpeed = std::sqrt(std::max(
            youngsModulus * axialArea / std::max(linearMass, 1.0e-9f), 1.0f));
        const float longitudinal = clamp(
            longitudinalSpeed / (2.0f * soundingLength), 100.0f,
            0.45f * static_cast<float>(sampleRate_));
        for (int mode = 0; mode < Voice::longitudinalModeCount; ++mode)
        {
            const int harmonic = 2 * mode + 1;
            const float modeFrequency = std::min(
                longitudinal * static_cast<float>(harmonic),
                0.45f * static_cast<float>(sampleRate_));
            const float omegaLong = twoPi * modeFrequency * inverseSampleRate_;
            const float radius = std::exp(-omegaLong
                / (2.0f * std::max(physicalCalibration_.longitudinalQ, 1.0f)));
            voice.longitudinalA1[mode]
                = 2.0f * radius * std::cos(omegaLong);
            voice.longitudinalA2[mode] = -radius * radius;
            // Constant peak gain. Projecting the integrated extension drive
            // onto sin(n*pi*x/L) gives L*(1-(-1)^n)/(n*pi): even modes cancel
            // and the observable odd modes carry the fixed-fixed string's
            // 1/n participation. No modal weighting is fitted here.
            voice.longitudinalB0[mode] = (1.0f - radius * radius)
                * std::sin(omegaLong) / static_cast<float>(harmonic);
        }
        // DAFx-26's tension increase, in newtons, from the same displacement
        // scale the attack-pitch surrogate is calibrated with.
        const float displacement
            = physicalCalibration_.steelDisplacementScaleMetres;
        voice.longitudinalDrive = axialKnown
            ? youngsModulus * axialArea * displacement * displacement
                / (2.0f * soundingLength * soundingLength)
            : 0.0f;
    }
    const float measuredBridgeDelay = bridgePhaseDelay(frequency, stringIndex);
    const float desiredPeriodGain = std::pow(0.001f,
        1.0f / std::max(fundamentalT60 * frequency, 1.0f));
    const float filterGain = magnitudeForOnePoleMix(
        broadLossCoefficient, broadLoss, omega)
        * magnitudeForOnePoleMix(lowpassCoefficient, mutedHighLoss, omega);
    const float loopGain = desiredPeriodGain / std::max(filterGain, 0.50f);

    for (int polarisation = 0; polarisation < 2; ++polarisation)
    {
        auto& loop = voice.loops[static_cast<std::size_t>(polarisation)];
        // The pair is split by an end correction, not by the body: see
        // polarisationEndCorrectionMetres in FittedPhysicalData.h for the
        // measurement and its bound. The whole difference lengthens the
        // parallel loop, so the normal one is the higher member as Woodhouse
        // measures it, and the normal loop keeps exactly the sounding length
        // that the tuning, the fret compensation and the bridge phase delay
        // are all built on. The previous split was an authored -0.32 / +0.41
        // cents with the opposite sign and a third of the measured size.
        const float endCorrection = polarisation == 0 ? 0.0f
            : physicalCalibration_.polarisationEndCorrectionMetres;
        const float polarisationDelay = rawDelay
            - (polarisation == 0 ? measuredBridgeDelay : 0.0f);
        loop.targetDelay = clamp(
            polarisationDelay * (1.0f + endCorrection / soundingLength),
            3.0f, static_cast<float>(maximumDelaySamples - 3));
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
    // Z = sqrt(T*mu) with Grimes' stretched mass per length is Z0 times the
    // frequency ratio the same tension produces - 12.3% for a whole tone -
    // and saturates with the tension where his law stops. The junction reads
    // characteristicImpedance times the applied scale, so a string set or
    // tuning switched under a ringing chord still steps the port exactly as
    // it did, and only a bend slews it.
    const float stretchedMass = 1.0f
        + (bentTension - tension) / std::max(axialRigidity, 1.0f);
    voice.tensionNewtons = bentTension;
    voice.bendImpedanceScale = std::sqrt((bentTension / tension)
                                         / stretchedMass);
    if (clearDelay)
        voice.appliedBendImpedanceScale = voice.bendImpedanceScale;
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

// The modulation wheel's vibrato. Grimes' Sec. 0.3 and Erkut et al. (AES
// 108th Conv. 2000, preprint 5114, Sec. 3.3) describe the same gesture: the
// classical player's vibrato is axial, the fretting hand modulating tension
// at a fixed bend angle, so it belongs on the tension route and not on the
// fret - and what a tension modulation is heard as is a modulation of the
// fundamental (Erkut: the string is repeatedly stretched to fluctuate the
// fundamental frequency), which is why configureVoice puts the interval this
// returns into the performed pitch as well as into the tension. What those
// measurements fix: the lowest frequency during a vibrato is the note's own
// unmodulated one (Erkut), so the modulation is one-sided upward and starts
// from the note's pitch; the depth converges over a transient of about 0.5 s
// from the moment the player begins it (Erkut's tt); the fitted rates are
// 1.4 Hz slow and 4.9 Hz fast (Erkut), the fast one just under Laurson et
// al.'s 5-6 Hz for the same instrument (CMJ 25(3):38-49, 2001), and the fast
// vibrato's depth is systematically the smaller, so the wheel runs from the
// shallow fast end to the deep slow one rather than raising both; and an
// open string gets none, because no finger is stopping it
// (Laurson's max-depth is zero at fret zero). What no source fixes is the
// wheel's top in cents - Erkut reports the depth varying with string and fret
// without a figure, Laurson scales it by an integer 1 to 9 - so that endpoint
// is authored, and set to the same 20 cents the attack-pitch surrogate is
// bounded at, which keeps a new magnitude out of the model.
float AcustraEngine::vibratoSemitones(const Voice& voice,
                                      int fret) const noexcept
{
    if (!(vibrato_ > 0.0f) || !voice.played || !voice.keyDown
        || voice.harmonic > 1 || fret <= 0)
        return 0.0f;
    constexpr float fullWheelSemitones = 0.20f;
    // MPE channel pressure biases how deep this one note's vibrato reaches,
    // a firmer grip letting more of the wheel's own travel through; it never
    // raises the wheel's own 20-cent ceiling above, only trims it down for a
    // lighter grip, so no new pitch magnitude enters the model. A pressure
    // that was never sent leaves the factor at 1, exactly as before.
    constexpr float pressureDepthFloor = 0.5f;
    const float pressure = mpePressureFor(voice);
    const float depthBias = pressure >= 0.0f
        ? pressureDepthFloor + (1.0f - pressureDepthFloor) * pressure
        : 1.0f;
    return fullWheelSemitones * depthBias * vibrato_ * vibratoOnset_
        * 0.5f * (1.0f - std::cos(vibratoPhase_));
}

float AcustraEngine::effectiveTouch(const Voice& voice) const noexcept
{
    const auto& physical = parameters_.stringMaterial == StringMaterial::Steel
        ? physicalCalibration_.steel : physicalCalibration_.nylon;
    return clamp(parameters_.touch + physical.velocityBrightnessDepth
        * (voice.velocity - 0.5f), 0.0f, 1.0f);
}

// -1: no lower zone, off a member channel, or no channel-pressure message
// received for this note yet -- every caller must treat that as "apply no
// bias", not as a pressure of zero.
float AcustraEngine::mpePressureFor(const Voice& voice) const noexcept
{
    if (!voice.mpeMember || voice.midiChannel < 1
        || voice.midiChannel > midiChannelCount)
        return -1.0f;
    return mpePressure_[static_cast<std::size_t>(voice.midiChannel - 1)];
}

namespace
{
float xorshiftNoise(std::uint32_t& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    state = state == 0 ? 0x6d2b79f5u : state;
    return static_cast<float>(static_cast<std::int32_t>(state))
         / static_cast<float>(std::numeric_limits<std::int32_t>::max());
}
} // namespace

float AcustraEngine::nextNoise(Voice& voice) noexcept
{
    return xorshiftNoise(voice.randomState);
}

void AcustraEngine::beginStrum() noexcept
{
    // A stroke's own pick speed varies stroke to stroke (GuitarSet's
    // comping tracks, Tools/MeasureStrums.py -- see strumDelaySamples and
    // noteOn's strumMember path for the measured figures this scale is
    // fitted to). Drawn from the engine's own generator, not any one
    // voice's, so it is one shared value applied to every string of this
    // stroke regardless of which voice noteOn happens to land it on.
    strumSpeedScale_ = 1.0f + strumSpeedJitterHalfWidth
        * xorshiftNoise(strumRandomState_);
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
    // No two plucks land in the same place. The archtop's three takes of each
    // note put their pluck points a median 0.02 of the string length apart
    // (Traube-Smith comb on the soft rows, where the estimate is clean),
    // which for three draws from a uniform spread is the spread's half-width;
    // each pluck draws its own offset within it.
    const float takeOffset = 0.02f * nextNoise(voice);
    // MPE Timbre, CC74, on this note's own member channel says directly
    // where the string was met (0-1 across the same 0.05-0.46 band the
    // panel control reaches), in place of the panel's one hand position for
    // every string. -1 is "no CC74 received this note", the panel's own
    // distance-from-bridge law applies unchanged, and this is the only path
    // reachable without a lower zone.
    const std::size_t channelIndex
        = static_cast<std::size_t>(voice.midiChannel - 1);
    const bool hasTimbre = voice.mpeMember && voice.midiChannel >= 1
        && voice.midiChannel <= midiChannelCount
        && mpeTimbre_[channelIndex] >= 0.0f;
    const float basePosition = hasTimbre
        ? 0.05f + 0.41f * mpeTimbre_[channelIndex]
        : distanceFromBridge / soundingLength;
    const float position = clamp(basePosition + takeOffset, 0.05f, 0.46f);
    voice.pluckPoint = position;
    // Velocity response has two bounded parts: touch brightens with velocity,
    // while the displacement exponent moves from the legacy 1.32 toward the
    // reference-response 0.82 as the same fitted depth rises.
    const float velocityExponent = 1.32f
        - 0.50f * physical.velocityBrightnessDepth;
    // A strummed string's own level varies stroke to stroke too, at the
    // one nominal velocity a strum's mean gives every string: GuitarSet's
    // comping tracks put the pooled deviation of a repeated string's own
    // level, across 25 runs of >=3 repeats of one chord and direction, at
    // a 4.47 dB standard deviation, matched here by uniform jitter at a
    // half-width of std*sqrt(3). A single note never sets voice.strumming,
    // so it never draws this and stays exactly as it was.
    const float strumLevelGain = voice.strumming
        ? std::pow(10.0f, 7.74f * nextNoise(voice) / 20.0f) : 1.0f;
    const float amplitude = (steel ? 0.24f : 0.29f)
        * std::pow(v, velocityExponent) * (0.92f + 0.08f * touch)
        * strumLevelGain;
    const float randomAngle = 0.025f * nextNoise(voice);
    voice.polarisationMix = clamp(0.91f - 0.08f * touch + randomAngle,
                                  0.78f, 0.96f);
    // The shared register law pivots at one fixed 48 kHz MIDI-61 period,
    // independent of material, string choice and host sample rate.
    const float apertureReferenceDelay = 48000.0f / midiFrequency(61);

    // A string that was still sounding has already been taken into the tail
    // by the caller: the hand landing on it is the same contact a taken
    // string goes through, so the pluck itself is always released from rest.
    for (int polarisation = 0; polarisation < 2; ++polarisation)
    {
        auto& loop = voice.loops[static_cast<std::size_t>(polarisation)];
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
        for (int sample = 1; sample <= length; ++sample)
        {
            const float phase = static_cast<float>(sample - 1)
                              / static_cast<float>(length);
            const float triangle = std::max(
                releasedAt(phase) - endpoint, 0.0f);
            loop.delay[static_cast<std::size_t>(wrapDelayIndex(
                loop.writeIndex - sample))]
                = amplitude * polarisationGain * triangle;
        }
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
    voice.level = std::max(voice.level, 0.02f * v);
    voice.releaseDamping = 1.0f;
    voice.returnSamples = 0;
}

void AcustraEngine::returnToOpenString(Voice& voice, int stringIndex,
                                       bool clearDelay) noexcept
{
    voice.played = false;
    voice.keyDown = false;
    voice.pedalHeld = false;
    voice.mpeMember = false;
    voice.memberPitchBendFrozen = false;
    voice.ownerCount = 0;
    voice.legatoHeldCount = 0;
    voice.midiNote = voice.openMidi;
    voice.midiChannel = 1;
    voice.fret = 0;
    voice.velocity = 0.0f;
    voice.excitationEnvelope = 0.0f;
    voice.attackPitchCents = 0.0f;
    voice.attackPitchDecay = 1.0f;
    voice.frozenMemberPitchBendSemitones = 0.0f;
    voice.attackSlopeEnergy = 0.0f;
    voice.observedSlopeEnergy = 0.0f;
    voice.longitudinalY1.fill(0.0f);
    voice.longitudinalY2.fill(0.0f);
    voice.harmonic = 1;
    voice.releaseDamping = 1.0f;
    voice.fingerLift = 0.0f;
    voice.touchDamping = 1.0f;
    voice.touchSamples = 0;
    voice.returnSamples = 0;
    voice.pluckDelay = 0;
    voice.tailActive = false;
    voice.tailLevel = 0.0f;
    voice.tailQuietSamples = 0;
    voice.tailLoop.reset();
    configureVoice(voice, stringIndex, voice.openMidi, clearDelay);
}

void AcustraEngine::captureTail(Voice& voice) noexcept
{
    // A string is only taken or replucked while the plucking hand is on it:
    // the picking hand landing to repluck or cut a note short, not the
    // fretting finger beginRelease's 0.16 s models. Laurson, Erkut, Valimaki
    // and Kuuskankare (CMJ 25(3), 2001, Sec. on re-excitation) report the loop
    // gain driven to zero in about 10 ms before a re-pluck; Erkut, Karjalainen,
    // Huang and Valimaki (AES 109th Convention, 2000) put the picking-hand
    // flesh contact's active regime at 20-60 ms. 10 ms is the specific figure
    // the first source gives for this contact and sits at the near edge of the
    // second source's regime, so it is used rather than a value picked between
    // the two.
    if (!(voice.level > 2.0e-7f))
    {
        voice.tailActive = false;
        return;
    }
    voice.tailLoop = voice.loops[0];
    constexpr float contactSeconds = 0.010f;
    voice.tailDamping = std::pow(0.001f,
        1.0f / std::max(contactSeconds * midiFrequency(voice.midiNote), 1.0f));
    voice.tailLevel = voice.level;
    voice.tailQuietSamples = 0;
    voice.tailActive = true;
}

void AcustraEngine::beginRelease(Voice& voice, int stringIndex) noexcept
{
    voice.pedalHeld = false;
    if (voice.fingerLift > 0.0f && voice.fret > 0 && voice.harmonic == 1)
    {
        liftFinger(voice, stringIndex, voice.openMidi);
        return;
    }
    const float releaseSeconds = voice.fret == 0 ? 1.25f : 0.16f;
    voice.releaseDamping = std::pow(0.001f,
        1.0f / std::max(releaseSeconds * midiFrequency(voice.midiNote), 1.0f));
    voice.returnSamples = static_cast<int>(
        (releaseSeconds + 0.08f) * static_cast<float>(sampleRate_));
}

float AcustraEngine::handContactGain(float frequency) const noexcept
{
    // The 0.16 s the model already gives a hand stopping a fretted string.
    // A finger still touching the string it is rising off is damped with the
    // same figure because no published one replaces it: Bilbao and Torin's
    // stopping finger takes its loss from Hunt and Crossley, as
    // Xi = beta_f K_f [eta_f]^alpha_f d(eta_f)/dt "for some constant
    // beta_f >= 0" (DAFx-14, Sec. 2.3), and beta_f is never given a value
    // there -- their finger runs are stated lossless, and the finger of the
    // real-time model that reuses those parameters (Bilbao, Russo, Webb et
    // al., "Real-Time Guitar Synthesis", Proc. DAFx-24, Guildford 2024,
    // Eqs. 9-10) carries no loss term at all.
    return std::pow(0.001f, 1.0f / std::max(0.16f * frequency, 1.0f));
}

// Factory set-up heights of the string over the fret crown: 3/32" bass and
// 1/16" treble at the twelfth fret for a steel-string (Martin's and Taylor's
// published specifications), 4 and 3 mm for a classical, with 0.5 and 0.7 mm
// of clearance over the first fret. A straight neck puts the height at any
// distance from the nut on the line between those two points, so this is a
// dimension of the instrument, not a fitted value.
float AcustraEngine::actionHeight(int stringIndex,
                                  float nutDistance) const noexcept
{
    const bool steel = parameters_.stringMaterial == StringMaterial::Steel;
    const float scaleLength = steel ? 0.648f : 0.650f;
    const float mix = static_cast<float>(stringIndex)
                    / static_cast<float>(stringCount - 1);
    const float twelfth = steel ? 2.4e-3f + (1.6e-3f - 2.4e-3f) * mix
                                : 4.0e-3f + (3.0e-3f - 4.0e-3f) * mix;
    const float nut = steel ? 0.5e-3f : 0.7e-3f;
    return nut + (twelfth - nut) * nutDistance / (0.5f * scaleLength);
}

// The pluck's own velocity law, as the string energy it injects, so that
// velocity means the same thing to the fretting hand as to the picking hand:
// a hammer-on or a lift at a MIDI velocity carries the energy a pluck at that
// velocity would, bounded by what the mechanism can physically release.
float AcustraEngine::pluckEnergy(float velocity, float soundingLength,
                                 float tension) const noexcept
{
    const bool steel = parameters_.stringMaterial == StringMaterial::Steel;
    const auto& physical = steel ? physicalCalibration_.steel
                                 : physicalCalibration_.nylon;
    const float v = clamp(velocity, 0.0f, 1.0f);
    const float touch = clamp(parameters_.touch
        + physical.velocityBrightnessDepth * (v - 0.5f), 0.0f, 1.0f);
    const float velocityExponent = 1.32f
        - 0.50f * physical.velocityBrightnessDepth;
    const float amplitude = (steel ? 0.24f : 0.29f)
        * std::pow(v, velocityExponent) * (0.92f + 0.08f * touch);
    const float distanceFromBridge = (0.045f
        + 0.135f * parameters_.pluckPosition) * physical.pluckDistanceScale;
    const float position = clamp(distanceFromBridge / soundingLength,
                                 0.05f, 0.46f);
    const float metres = amplitude
        * std::max(physicalCalibration_.steelDisplacementScaleMetres, 1.0e-4f);
    return 0.5f * tension * metres * metres
         * (1.0f / position + 1.0f / (1.0f - position)) / soundingLength;
}

// The loop holds one period of the wave the bridge reads. A shape released
// from rest is one hump of its own height; a velocity profile v(x) is the
// integral of v, mirrored about the half period, at half height over c. All
// three add to what the loop already holds: the string was vibrating and
// goes on doing so.
void AcustraEngine::addReleasedTriangle(StringLoop& loop, float height,
                                        float apexFraction,
                                        float sign) noexcept
{
    // The loop may be slewing to a new length. Only samples younger than the
    // current read age are ever read again, so the shape spans the current
    // delay, with its zero at the sample about to be read.
    const int length = std::clamp(
        static_cast<int>(std::round(loop.currentDelay)), 8,
        maximumDelaySamples - 3);
    const float p = clamp(apexFraction, 0.002f, 0.98f);
    for (int sample = 1; sample <= length; ++sample)
    {
        const float phase = static_cast<float>(sample - 1)
                          / static_cast<float>(length);
        const float value = phase < p ? phase / p : (1.0f - phase) / (1.0f - p);
        loop.delay[static_cast<std::size_t>(wrapDelayIndex(
            loop.writeIndex - sample))] += sign * height * value;
    }
}

void AcustraEngine::addUniformVelocity(StringLoop& loop, float plateau,
                                       float extentFraction,
                                       float sign) noexcept
{
    const int length = std::clamp(
        static_cast<int>(std::round(loop.currentDelay)), 8,
        maximumDelaySamples - 3);
    const float w = clamp(extentFraction, 0.002f, 1.0f);
    for (int sample = 1; sample <= length; ++sample)
    {
        const float x = 2.0f * static_cast<float>(sample - 1)
                      / static_cast<float>(length);
        const float mirrored = x < 1.0f ? x : 2.0f - x;
        loop.delay[static_cast<std::size_t>(wrapDelayIndex(
            loop.writeIndex - sample))]
            += -sign * plateau * std::min(mirrored, w) / w;
    }
}

void AcustraEngine::addTriangleVelocity(StringLoop& loop, float scale,
                                        float apexFraction,
                                        float sign) noexcept
{
    const int length = std::clamp(
        static_cast<int>(std::round(loop.currentDelay)), 8,
        maximumDelaySamples - 3);
    const float p = clamp(apexFraction, 0.002f, 0.98f);
    for (int sample = 1; sample <= length; ++sample)
    {
        const float x = 2.0f * static_cast<float>(sample - 1)
                      / static_cast<float>(length);
        const float mirrored = x < 1.0f ? x : 2.0f - x;
        float integral = 0.0f;
        if (mirrored < p)
            integral = 0.5f * mirrored * mirrored / p;
        else
        {
            const float remaining = (1.0f - mirrored) / (1.0f - p);
            integral = 0.5f * p
                     + 0.5f * (1.0f - p) * (1.0f - remaining * remaining);
        }
        loop.delay[static_cast<std::size_t>(wrapDelayIndex(
            loop.writeIndex - sample))] += -sign * scale * integral;
    }
}

// The finger leaves a stopped string. The string was pressed to the fret by
// the action height there, a triangle over the segment it now belongs to.
// If the energy the lift carries is at least that triangle's elastic energy
// the finger is gone before the string moves and the shape is released
// whole, which is a pull-off; below it the string keeps up with the finger
// through the same triangle and leaves it at the rest line with the finger's
// velocity over that shape. The vibration the stopped segment held goes on
// over the new length, and the finger, still touching until it has risen
// clear, damps it for h / v with the hand's own loss. A lift to the open
// string leaves the string to nobody: it rings on in the junction with no
// key and no hand on it until it has died away.
void AcustraEngine::liftFinger(Voice& voice, int stringIndex,
                               int targetMidi) noexcept
{
    const bool steel = parameters_.stringMaterial == StringMaterial::Steel;
    const float scaleLength = steel ? 0.648f : 0.650f;
    const float lift = clamp(voice.fingerLift, 0.0f, 1.0f);
    const int liftedFret = std::max(voice.fret, 0);
    const int targetFret = std::max(targetMidi - voice.openMidi, 0);
    if (lift <= 0.0f || liftedFret <= targetFret)
        return;
    const float liftedDistance = scaleLength
        * (1.0f - std::exp2(-static_cast<float>(liftedFret) / 12.0f));
    const float targetDistance = scaleLength
        * (1.0f - std::exp2(-static_cast<float>(targetFret) / 12.0f));
    const float targetLength = std::max(scaleLength - targetDistance, 1.0e-3f);
    // Height of the string over the lifted fret while stopped at the target:
    // the stopped string runs from that fret crown to the saddle.
    const float height = std::max(actionHeight(stringIndex, liftedDistance)
        - actionHeight(stringIndex, targetDistance)
            * (scaleLength - liftedDistance) / targetLength, 0.0f);
    const float apex = clamp((liftedDistance - targetDistance) / targetLength,
                             0.002f, 0.98f);
    const float displacementScale = std::max(
        physicalCalibration_.steelDisplacementScaleMetres, 1.0e-4f);
    const float waveSpeed = 2.0f * scaleLength * midiFrequency(voice.openMidi);
    const float tension = voice.characteristicImpedance * waveSpeed;
    // MPE channel pressure does not reach this lift: an earlier version
    // biased the elastic threshold below to move some lifts into the full-
    // release branch at less than the fret's own elastic energy, but
    // addReleasedTriangle always injects that unbiased elastic energy (a
    // triangle of a fixed physical height and apex, not the lift's own
    // energy), so a biased lift below the true threshold released more
    // energy than it carried -- measured up to 1.61x the unbiased case's
    // radiated tail energy on some lifts. No formulation was found that
    // both moves the threshold and keeps the two branches' injected energy
    // continuous without a second invented constant, so the branch stays on
    // the fret's own physics only; grip pressure biases vibrato depth (see
    // vibratoSemitones) but not this.

    voice.fingerLift = 0.0f;
    voice.releaseDamping = 1.0f;
    if (targetMidi == voice.openMidi)
    {
        // Nobody holds the string now, but it is still on the bridge and still
        // ringing, so it stays a played voice in the junction with no key and
        // no hand loss until it has died away and returns to rest like any
        // other released note. The allocator takes a free string first, and
        // takes this one through the tail a taken string already uses.
        voice.keyDown = false;
        voice.pedalHeld = false;
        voice.mpeMember = false;
        voice.memberPitchBendFrozen = false;
        voice.ownerCount = 0;
        voice.legatoHeldCount = 0;
        voice.midiChannel = 1;
        voice.attackPitchCents = 0.0f;
        voice.attackPitchDecay = 1.0f;
        voice.frozenMemberPitchBendSemitones = 0.0f;
        voice.harmonic = 1;
        voice.returnSamples = static_cast<int>(1.33f * static_cast<float>(sampleRate_));
    }
    configureVoice(voice, stringIndex, targetMidi, false);

    const float energy = pluckEnergy(lift, targetLength, tension);
    const float elastic = 0.5f * tension * height * height
        * (1.0f / apex + 1.0f / (1.0f - apex)) / targetLength;
    voice.touchDamping = handContactGain(midiFrequency(targetMidi));
    if (energy >= elastic)
    {
        addReleasedTriangle(voice.loops[0], height / displacementScale, apex,
                            1.0f);
        voice.touchSamples = 0;
    }
    else
    {
        // Kinetic energy of v*tri over the segment is mu v^2 L/6.
        const float speed = waveSpeed
            * std::sqrt(6.0f * energy / std::max(tension * targetLength, 1.0e-9f));
        addTriangleVelocity(voice.loops[0],
            0.5f * speed / waveSpeed * targetLength / displacementScale, apex,
            1.0f);
        const float clearSeconds = height / std::max(speed, 1.0e-3f);
        voice.touchSamples = static_cast<int>(std::min(clearSeconds, 1.0f)
            * static_cast<float>(sampleRate_));
    }
    voice.level = std::max(voice.level, 1.0e-6f);
}

// A finger hammers a sounding string down onto a fret. A point driven across
// an ideal string at speed v drags a V-shaped dent whose flanks have slope
// v/c and which moves down with it, so when the string meets the fret crown,
// its clearance h below the old line, the dent is a triangle of half-width
// w = c*h/v carrying velocity v throughout. Relative to the new segment's own
// rest line, the crown-to-saddle line, that leaves a released triangle with
// its apex at w and height h(1 - w/L), plus the uniform velocity over [0, w].
// A finger slower than c*h/L has the dent's front reach the saddle first,
// and then the whole segment moves down with it. The speed comes from the
// energy the pluck's velocity law assigns to the same MIDI velocity, so a
// hammer-on at a velocity is as loud as a pluck at it and gets brighter as
// it gets faster, the way a real one does.
//
// The finger driving that point is rigid, and the published finger says a
// rigid one is right here. Bilbao and Torin, "Numerical Simulation of
// String/Barrier Collisions: The Fretboard", Proc. 17th Int. Conf. Digital
// Audio Effects (DAFx-14), Erlangen 2014, Fig. 4 caption, give the stopping
// finger a mass M_FG = 5e-3 kg and a collision force f = K_FG [eta_FG]^a_FG
// with K_FG = 1e10 and a_FG = 2.3 (the caption prints no unit for K_FG; the
// exponent makes it N/m^2.3); the real-time guitar of Bilbao, Russo, Webb
// and Ducceschi, Proc. DAFx-24, Guildford 2024, Sec. 2.5, runs the same
// finger ("parameters as given in [11]", [11] being that paper). Such a
// contact has incremental stiffness a_FG K_FG eta^(a_FG - 1), and driving
// the string's own drive-point resistance 2T/c through it is a first-order
// lag of rise time 2(T/c) / (a_FG K_FG eta^(a_FG - 1)) at the working
// penetration eta = (2(T/c) v / K_FG)^(1/a_FG). Swept over both materials,
// all six strings, every fret pair up to fretCount and velocities 0.1 to
// 1.0, that rise time is 0.15 % to 57 % of the dent's own descent h/v. The
// worst case is the steel high E hammered from fret 19 to 20 at velocity
// 1.0: 1.38 us of rise against 2.43 us of descent, a contact corner of
// 115 kHz, above Nyquist at every supported rate. Where that corner does
// fall inside the audio band the rise never exceeds 30 % of the descent.
// K_FG would have to be 3.7 times softer (2.7e9 N/m^2.3) before the
// contact's rise matched the descent, so a mass-spring finger on these
// constants writes this same dent and the rigid one stays.
//
// The published finger does not bound the speed either. Over that same
// sweep the descent below reaches 62 m/s on steel and 43 m/s on nylon at
// velocity 1.0 -- far faster than a hand moves -- but neither the mass nor
// the stiffness limits it, because the finger is driven. The only published
// value for that driving force is DAFx-24 Sec. 6.2's f_e,FG = 0.9 N, from
// one finger-tap demonstration; DAFx-14's own finger is unforced (Sec. 4.3)
// and its one kinematic figure is the 3 m/s approach of the Fig. 4 caption.
// Neither is a playing range. Bounding the top of this map still needs a
// measured fingertip speed -- see Known gaps.
void AcustraEngine::hammerString(Voice& voice, int stringIndex,
                                 int previousMidi, float velocity) noexcept
{
    const bool steel = parameters_.stringMaterial == StringMaterial::Steel;
    const float scaleLength = steel ? 0.648f : 0.650f;
    const int previousFret = std::max(previousMidi - voice.openMidi, 0);
    const int newFret = std::max(voice.fret, 0);
    if (newFret <= previousFret)
        return;
    const float previousDistance = scaleLength
        * (1.0f - std::exp2(-static_cast<float>(previousFret) / 12.0f));
    const float newDistance = scaleLength
        * (1.0f - std::exp2(-static_cast<float>(newFret) / 12.0f));
    const float soundingLength = std::max(scaleLength - newDistance, 1.0e-3f);
    const float height = std::max(actionHeight(stringIndex, newDistance)
        - actionHeight(stringIndex, previousDistance) * soundingLength
            / std::max(scaleLength - previousDistance, 1.0e-3f), 1.0e-5f);
    const float displacementScale = std::max(
        physicalCalibration_.steelDisplacementScaleMetres, 1.0e-4f);
    const float waveSpeed = 2.0f * scaleLength * midiFrequency(voice.openMidi);
    const float tension = voice.characteristicImpedance * waveSpeed;
    const float energy = pluckEnergy(clamp(velocity, 0.0f, 1.0f),
                                     soundingLength, tension);
    // Energy of the dent at the speed where its front just reaches the
    // saddle, which is also the whole segment moving at that speed.
    const float threshold = 0.5f * tension * height * height / soundingLength;
    float speed = 0.0f;
    float width = soundingLength;
    if (energy <= threshold)
        speed = waveSpeed * std::sqrt(2.0f * energy
            / std::max(tension * soundingLength, 1.0e-9f));
    else
    {
        speed = (energy + threshold) * waveSpeed / (tension * height);
        width = waveSpeed * height / speed;
    }
    const float extent = width / soundingLength;
    addUniformVelocity(voice.loops[0],
        0.5f * speed * width / waveSpeed / displacementScale, extent, -1.0f);
    // The string is still above the crown-to-saddle line while it moves down
    // toward it, so the released triangle and the velocity have opposite
    // signs.
    if (extent < 1.0f)
        addReleasedTriangle(voice.loops[0],
            height * (1.0f - extent) / displacementScale, extent, 1.0f);
    voice.level = std::max(voice.level, 0.02f * clamp(velocity, 0.0f, 1.0f));
    voice.touchSamples = 0;
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
    // A note repeated after its key came up is replucked on the string still
    // sounding it, as a guitarist does, rather than hopping to whichever free
    // string can also reach it and leaving the first one ringing.
    for (int string = stringCount - 1; string >= 0; --string)
    {
        const auto& voice = voices_[static_cast<std::size_t>(string)];
        if (voice.played && !voice.keyDown && voice.harmonic == 1
            && voice.midiNote == midiNote && voice.level > 2.0e-7f)
            return string;
    }
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

void AcustraEngine::noteOn(int midiNote, float velocity, int midiChannel,
                           int pluckDelaySamples, bool strumMember) noexcept
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
            // The picking hand lands on a sounding string before it plucks it
            // again, which is the contact a taken string already goes through:
            // its vibration carries on in the tail under the hand while the
            // new pluck is released from rest. Projecting the stored shape onto
            // the modes with a node at the contact instead, in one sample,
            // clicked into the limiter and let the near-node modes pile up
            // pluck after pluck.
            if (voice.level > 2.0e-7f)
                captureTail(voice);
            configureVoice(voice, string, midiNote, true);
            voice.strumming = strumMember;
            initialisePluck(voice, string, velocity);
            bridgeDerivativesCrossRelease_ = true;
            configureVoice(voice, string, midiNote, false);
            return;
        }
    }

    if (legato_)
    {
        const int hammered = chooseLegatoString(midiNote, midiChannel);
        if (hammered >= 0)
        {
            auto& voice = voices_[static_cast<std::size_t>(hammered)];
            // The fretting finger stops the string; it does not release it
            // from rest. So the loop keeps what it holds and only its length
            // changes, on the same slew a slide already uses, and the finger's
            // own strike on the string is added to it below.
            if (voice.legatoHeldCount == 0)
                voice.legatoHeld[0] = voice.midiNote;
            voice.legatoHeldCount = std::max(voice.legatoHeldCount, 1);
            voice.legatoHeld[static_cast<std::size_t>(voice.legatoHeldCount)]
                = midiNote;
            ++voice.legatoHeldCount;
            voice.ownerCount = voice.legatoHeldCount;
            voice.startOrder = ++noteOrder_;
            const int previousMidi = voice.midiNote;
            configureVoice(voice, hammered, midiNote, false);
            hammerString(voice, hammered, previousMidi,
                         clamp(velocity, 0.001f, 1.0f));
            return;
        }
    }

    int string = -1;
    int harmonic = 1;
    // Guitar-controller mode: the channel already says which string, the way
    // a GK pickup or TriplePlay does in mono mode, so the fret-distance guess
    // below never runs. A note the channel's own string cannot fret is
    // dropped rather than handed to a different string, matching what such
    // a controller can physically pick.
    if (stringPerChannelMode_ && midiChannel >= 1 && midiChannel <= stringCount)
    {
        const int candidate = midiChannel - 1;
        const int fret = midiNote
            - voices_[static_cast<std::size_t>(candidate)].openMidi;
        if (fret < 0 || fret > fretCount)
            return;
        string = candidate;
    }
    else
    {
        string = chooseString(midiNote);
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
    }
    auto& voice = voices_[static_cast<std::size_t>(string)];
    // Taking a string that is still sounding, for any note, is a refret and a
    // repluck, not a cut: what it still holds carries on under the hand while
    // the new pluck is released from rest.
    if (voice.level > 2.0e-7f)
        captureTail(voice);
    voice.harmonic = harmonic;
    voice.played = true;
    voice.keyDown = true;
    voice.pedalHeld = false;
    voice.ownerCount = 1;
    voice.legatoHeldCount = 0;
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
    voice.strumming = strumMember;
    int delaySamples = pluckDelaySamples;
    if (strumMember)
    {
        // The pick's own speed varies stroke to stroke even at one nominal
        // strum velocity (GuitarSet's comping tracks, Tools/MeasureStrums.py
        // -- see beginStrum, which draws strumSpeedScale_ once per stroke
        // and shares it across every string). Scaling the deterministic
        // delay by that one shared factor, instead of jittering each
        // string's release time independently, is what keeps the strings
        // of a stroke in the pick's own order: a slower or faster draw
        // stretches or compresses the whole stroke together, it never lets
        // a later string catch up with an earlier one.
        delaySamples = std::max(0, static_cast<int>(
            std::round(static_cast<float>(pluckDelaySamples) * strumSpeedScale_)));
    }
    if (delaySamples > 0)
    {
        // Fretted and waiting: a junction member with nothing on it until
        // the pick arrives. The countdown fires at the top of that sample,
        // exactly where a note-on issued then would have put the shape.
        voice.excitationEnvelope = 0.0f;
        voice.pluckDelay = delaySamples + 1;
        return;
    }
    firePluck(voice, string);
}

void AcustraEngine::firePluck(Voice& voice, int stringIndex) noexcept
{
    voice.pluckDelay = 0;
    initialisePluck(voice, stringIndex, voice.velocity);
    bridgeDerivativesCrossRelease_ = true;
    configureVoice(voice, stringIndex, voice.midiNote, false);
}

int AcustraEngine::strumDelaySamples(int stringRank,
                                     float velocity) const noexcept
{
    // The pick crosses the strings at one speed, so the k-th string it
    // reaches sounds k spacings later. The spacing is the set-up dimension
    // at the saddle: 2 1/8" across the six on a steel-string, 58 mm on a
    // classical. The pick's speed is the one number MIDI does not carry;
    // GuitarSet's comping tracks (Tools/MeasureStrums.py on the hex-pickup
    // channels and JAMS note onsets, Zenodo 3371780 CC BY 4.0), clustered
    // into 641 same-stroke (>=3 strings within a window derived from each
    // track's own annotated tempo -- a sixteenth note at its fastest, so
    // the window cannot merge two distinct strokes) onset clusters, put the
    // pooled 10-90% traversal speed, at the shipping 10.8 mm steel spacing,
    // at 0.51 to 2.46 m/s (five 10.8 mm gaps across six strings: 106 ms to
    // 22 ms); the speed showed no resolvable dependence on stroke level
    // (plain Pearson r of speed against mean stroke level, about -0.03 --
    // that mean level is averaged across strings on the hex pickup's
    // uncalibrated per-coil sensitivity, which biases the correlation
    // toward zero, so this null is weaker evidence than a same-channel
    // measurement would give), so the map from MIDI velocity to speed
    // across that measured range stays a player's map, not a fitted
    // dependence.
    const bool steel = parameters_.stringMaterial == StringMaterial::Steel;
    const float spacing = steel ? 0.0540f / 5.0f : 0.0580f / 5.0f;
    const float speed = 0.51f + 1.95f * clamp(velocity, 0.0f, 1.0f);
    return static_cast<int>(static_cast<float>(std::max(stringRank, 0))
        * spacing / speed * static_cast<float>(sampleRate_) + 0.5f);
}

void AcustraEngine::noteOff(int midiNote, int midiChannel,
                            float fingerLift) noexcept
{
    if (midiChannel < 1 || midiChannel > midiChannelCount)
        return;
    const float lift = std::isfinite(fingerLift)
        ? clamp(fingerLift, 0.0f, 1.0f) : 0.0f;
    if (releaseLegatoNote(midiNote, midiChannel, lift))
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
    candidate.pluckDelay = 0;
    freezeMemberPitchBend(candidate);
    candidate.keyDown = false;
    candidate.fingerLift = lift;
    candidate.pedalHeld = sustainIsDown(candidate);
    if (!candidate.pedalHeld)
        beginRelease(candidate, candidateIndex);
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
        beginRelease(voice, string);
    }
}

void AcustraEngine::setLegato(bool on) noexcept
{
    if (legato_ == on)
        return;
    legato_ = on;
    if (on)
        return;
    // Lifting the switch leaves each string sounding what it is sounding; it
    // only stops the fretting hand's stack being tracked, so the notes under
    // the top one are forgotten rather than pulled off to.
    for (auto& voice : voices_)
        voice.legatoHeldCount = 0;
}

// A guitarist hammers on to the string already under the hand, so prefer the
// nearest pitch and then the string played most recently. A hammer-on only
// goes up: the way down is a pull-off, which is a note-off on this instrument
// because that is what it is on the guitar.
int AcustraEngine::chooseLegatoString(int midiNote,
                                      int midiChannel) const noexcept
{
    int best = -1;
    int bestInterval = fretCount + 1;
    std::uint64_t bestOrder = 0;
    for (int string = 0; string < stringCount; ++string)
    {
        const auto& voice = voices_[static_cast<std::size_t>(string)];
        if (!voice.played || !voice.keyDown || voice.harmonic != 1
            || voice.midiChannel != midiChannel
            || midiNote <= voice.midiNote)
            continue;
        const int fret = midiNote - voice.openMidi;
        if (fret < 0 || fret > fretCount)
            continue;
        if (voice.legatoHeldCount >= legatoHeldLimit)
            continue;
        const int interval = midiNote - voice.midiNote;
        if (best < 0 || interval < bestInterval
            || (interval == bestInterval && voice.startOrder > bestOrder))
        {
            best = string;
            bestInterval = interval;
            bestOrder = voice.startOrder;
        }
    }
    return best;
}

// Releasing one of the notes a string is holding. If it was the sounding one,
// the string falls back to the newest note still held on it, which is the
// pull-off; if it was underneath, nothing sounds different.
bool AcustraEngine::releaseLegatoNote(int midiNote, int midiChannel,
                                      float fingerLift) noexcept
{
    for (int string = 0; string < stringCount; ++string)
    {
        auto& voice = voices_[static_cast<std::size_t>(string)];
        if (voice.legatoHeldCount <= 0 || voice.midiChannel != midiChannel)
            continue;
        int found = -1;
        for (int index = voice.legatoHeldCount - 1; index >= 0; --index)
            if (voice.legatoHeld[static_cast<std::size_t>(index)] == midiNote)
            {
                found = index;
                break;
            }
        if (found < 0)
            continue;
        const bool wasSounding = voice.midiNote == midiNote;
        for (int index = found; index + 1 < voice.legatoHeldCount; ++index)
            voice.legatoHeld[static_cast<std::size_t>(index)]
                = voice.legatoHeld[static_cast<std::size_t>(index + 1)];
        --voice.legatoHeldCount;
        voice.ownerCount = voice.legatoHeldCount;
        if (voice.legatoHeldCount <= 0)
        {
            voice.ownerCount = 0;
            freezeMemberPitchBend(voice);
            voice.keyDown = false;
            voice.fingerLift = fingerLift;
            voice.pedalHeld = sustainIsDown(voice);
            if (!voice.pedalHeld)
                beginRelease(voice, string);
            return true;
        }
        if (wasSounding)
        {
            // A pull-off: the finger leaves the top note and the string falls
            // to the one under it, plucked by the leaving finger as fast as
            // it left.
            const int target = voice.legatoHeld[static_cast<std::size_t>(
                voice.legatoHeldCount - 1)];
            voice.startOrder = ++noteOrder_;
            voice.fingerLift = fingerLift;
            if (fingerLift > 0.0f)
                liftFinger(voice, string, target);
            else
                configureVoice(voice, string, target, false);
        }
        return true;
    }
    return false;
}

void AcustraEngine::setPitchBend(float semitones, int midiChannel) noexcept
{
    if (midiChannel < 1 || midiChannel > midiChannelCount)
        return;
    pitchBendSemitones_[static_cast<std::size_t>(midiChannel - 1)]
        = clamp(std::isfinite(semitones) ? semitones : 0.0f, -96.0f, 96.0f);
}

void AcustraEngine::setVibrato(float amount) noexcept
{
    vibrato_ = clamp(std::isfinite(amount) ? amount : 0.0f, 0.0f, 1.0f);
}

void AcustraEngine::setMpeTimbre(float value, int midiChannel) noexcept
{
    if (midiChannel < 1 || midiChannel > midiChannelCount)
        return;
    mpeTimbre_[static_cast<std::size_t>(midiChannel - 1)]
        = std::isfinite(value) && value >= 0.0f
            ? clamp(value, 0.0f, 1.0f) : -1.0f;
}

void AcustraEngine::setMpePressure(float value, int midiChannel) noexcept
{
    if (midiChannel < 1 || midiChannel > midiChannelCount)
        return;
    mpePressure_[static_cast<std::size_t>(midiChannel - 1)]
        = std::isfinite(value) && value >= 0.0f
            ? clamp(value, 0.0f, 1.0f) : -1.0f;
}

void AcustraEngine::setStringPerChannelMode(bool enabled) noexcept
{
    stringPerChannelMode_ = enabled;
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
            returnToOpenString(voice, string, true);
    }
    for (int midiChannel = 1; midiChannel <= lastAffectedChannel; ++midiChannel)
    {
        pitchBendSemitones_[static_cast<std::size_t>(midiChannel - 1)] = 0.0f;
        mpeTimbre_[static_cast<std::size_t>(midiChannel - 1)] = -1.0f;
        mpePressure_[static_cast<std::size_t>(midiChannel - 1)] = -1.0f;
        sustainPedals_[static_cast<std::size_t>(midiChannel - 1)] = false;
    }
    lowerZoneMemberCount_ = next;
    if (getActiveVoiceCount() == 0)
    {
        for (int string = 0; string < stringCount; ++string)
            returnToOpenString(voices_[static_cast<std::size_t>(string)], string, true);
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
        voice.legatoHeldCount = 0;
        freezeMemberPitchBend(voice);
        voice.keyDown = false;
        voice.fingerLift = 0.0f;
        voice.pedalHeld = sustainIsDown(voice);
        if (!voice.pedalHeld)
            beginRelease(voice, string);
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
            returnToOpenString(voice, string, true);
    }
    if (getActiveVoiceCount() == 0)
    {
        for (int string = 0; string < stringCount; ++string)
            returnToOpenString(voices_[static_cast<std::size_t>(string)], string, true);
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
    bridgeRotationDerivative_.reset();
    bridgeForceDerivative_.reset();
    bridgeForceMomentDerivative_.reset();
    bridgeBodyForceDerivative_.reset();
    bridgeBodyMomentDerivative_.reset();
    bridgeTailForceDerivative_.reset();
    bridgeTailMomentDerivative_.reset();
    lastBridgeVelocity_ = 0.0f;
    lastBridgeReactionForce_ = 0.0f;
    lastBridgeBodyForce_ = 0.0f;
    lastBridgeTailForce_ = 0.0f;
    lastSympatheticRadiationForce_ = 0.0f;
    lastLongitudinalForce_ = 0.0f;
    lastBridgePower_ = 0.0f;
    lastBridgeBodyPower_ = 0.0f;
    lastBridgeTailPower_ = 0.0f;
    bridgeDerivativesNeedPriming_ = true;
    bridgeDerivativesCrossRelease_ = false;
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
                                float& sympatheticForce,
                                float& longitudinalForce) noexcept
{
    // A rigid bridge and nut each invert a displacement wave, so the collapsed
    // full-round-trip loop writes +incident.  A moving bridge has reflected
    // wave b=x-a; folding in the nut inversion therefore writes a-x.
    voice.loops[0].write(verticalIncident - bridgeDisplacement
                         + 0.76f * excitation);
    voice.loops[1].write(horizontalIncident + 0.51f * excitation);

    const float sampleRateRatio = static_cast<float>(sampleRate_) / 48000.0f;
    const float verticalVelocity = voice.loops[0].bridgeVelocity(
        verticalIncident, sampleRateRatio);
    const float horizontalVelocity = voice.loops[1].bridgeVelocity(
        horizontalIncident, sampleRateRatio);
    // The squared slope is the same quantity for both materials; only the
    // pitch surrogate above it is steel-only.
    const float referenceRate48 = 48000.0f / static_cast<float>(sampleRate_);
    const float slopeV = voice.loops[0].currentDelay
                       * referenceRate48 * verticalVelocity;
    const float slopeH = voice.loops[1].currentDelay
                       * referenceRate48 * horizontalVelocity;
    const float slopeEnergy = slopeV * slopeV + slopeH * slopeH;
    if (physicalCalibration_.longitudinalGain > 0.0f && voice.played)
    {
        // Stretching the string adds tension, and that tension is a
        // longitudinal wave with the string's own axial resonances. The drive
        // is a square, so it carries the products of transverse partials: what
        // comes out are the sum and difference phantom partials rather than an
        // added tone.
        for (int mode = 0; mode < Voice::longitudinalModeCount; ++mode)
        {
            const float excitation = voice.longitudinalB0[mode]
                * voice.longitudinalDrive * slopeEnergy;
            const float output = excitation
                + voice.longitudinalA1[mode] * voice.longitudinalY1[mode]
                + voice.longitudinalA2[mode] * voice.longitudinalY2[mode];
            voice.longitudinalY2[mode] = voice.longitudinalY1[mode];
            voice.longitudinalY1[mode]
                = std::isfinite(output) ? output : 0.0f;
            longitudinalForce += physicalCalibration_.longitudinalGain
                * voice.longitudinalY1[mode];
        }
    }
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
    const float impedance = voice.characteristicImpedance
                          * voice.appliedBendImpedanceScale;
    if (voice.tailActive)
    {
        // The taken string keeps sounding while the hand damps it; its wave
        // is in the junction with the new note's, so its force reaches the
        // body through the bridge like any other string's.
        voice.tailLoop.write(tailIncident - bridgeDisplacement);
        const float tailVelocity = voice.tailLoop.bridgeVelocity(
            tailIncident, sampleRateRatio);
        const float tailForce = impedance
            * (2.0f * tailVelocity - bridgeVelocity);
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
    const float directForce = localReactionForce * voice.polarisationMix
        + 0.44f * impedance * horizontalVelocity
            * (1.0f - voice.polarisationMix);

    const float pan = (static_cast<float>(stringIndex) - 2.5f) / 2.5f;
    // The measured force-to-pressure bank is the acoustic source. Retain only
    // a very quiet bridge-local component; the previous amplified contact
    // residual exposed the periodic string waveform as a harpsichord cue.
    const float direct = physicalCalibration_.directGain * directForce;
    directLeft += direct * (1.0f - 0.18f * pan);
    directRight += direct * (1.0f + 0.18f * pan);

    const float magnitude = std::abs(localReactionForce);
    voice.level += levelSmoothing_ * (magnitude - voice.level);
    // A released string is handed back once its release damping has had its
    // T60 and the hand's 80 ms; waiting for it to fall silent would wait for
    // ever, because the bridge keeps driving it while anything else sounds.
    // What it still carries goes on as an idle string's wave.
    if (voice.played && !voice.keyDown && !voice.pedalHeld
        && voice.returnSamples > 0 && --voice.returnSamples == 0)
        returnToOpenString(voice, stringIndex, false);

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
        BridgeDrive drive {};
        for (int string = 0; string < stringCount; ++string)
        {
            auto& voice = voices_[static_cast<std::size_t>(string)];
            if (voice.pluckDelay > 0 && --voice.pluckDelay == 0)
                firePluck(voice, string);
            float releaseGain = (voice.keyDown || voice.pedalHeld
                                 || !voice.played)
                ? 1.0f : voice.releaseDamping;
            if (voice.touchSamples > 0)
            {
                releaseGain = std::min(releaseGain, voice.touchDamping);
                --voice.touchSamples;
            }
            // A bend is a tension change, so the port this string presents
            // moves with it. The junction sums impedances every sample and a
            // whole-tone bend moves this one by 12%; followed at the delay's
            // own rate rather than stepped once a control period, the sum
            // never steps. A scale of exactly 1 leaves both arithmetic and
            // output bit-identical to the unbent engine.
            voice.appliedBendImpedanceScale += delaySmoothing_
                * (voice.bendImpedanceScale
                   - voice.appliedBendImpedanceScale);
            excitation[static_cast<std::size_t>(string)]
                = renderExcitation(voice);
            verticalIncident[static_cast<std::size_t>(string)]
                = voice.loops[0].advance(delaySmoothing_, releaseGain);
            horizontalIncident[static_cast<std::size_t>(string)]
                = voice.loops[1].advance(delaySmoothing_, releaseGain);
            if (voice.tailActive)
                tailIncident[static_cast<std::size_t>(string)]
                    = voice.tailLoop.advance(delaySmoothing_, voice.tailDamping);
            // Every string is anchored behind the saddle whether or not it
            // is being played, so the anchor the junction sees is a constant
            // of the instrument. Summing only the played ones made it stiffen
            // with each voice held, which more than doubled a note's sustain
            // inside a chord. Each stub stands at its own string's point on
            // the saddle, so the six enter as the three moments of a
            // stiffness matrix rather than as one sum.
            const float arm = saddleLeverArm(string);
            drive.stiffness0 += voice.bridgeTailStiffness;
            drive.stiffness1 += arm * voice.bridgeTailStiffness;
            drive.stiffness2 += arm * arm * voice.bridgeTailStiffness;
            // Every string on the bridge is a member of the junction, played
            // or not: an idle string on a moving bridge carries a wave, and
            // at its resonance it presents thousands of times its
            // characteristic impedance, which is what pins a real bridge
            // there and bounds the sympathetic energy. Driving idle strings
            // from the bridge while leaving them out of the sum let their
            // reaction grow without limit. A taken string's tail is a second
            // wave on the same string, so it adds to the incident with the
            // string's impedance counted once.
            if (voice.played || sympatheticStringsEnabled_)
            {
                const float port = voice.characteristicImpedance
                                 * voice.appliedBendImpedanceScale;
                const float incident = 2.0f * port
                    * (verticalIncident[static_cast<std::size_t>(string)]
                       + tailIncident[static_cast<std::size_t>(string)]);
                drive.impedance0 += port;
                drive.impedance1 += arm * port;
                drive.impedance2 += arm * arm * port;
                drive.incidentHeave += incident;
                drive.incidentRock += arm * incident;
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
        if (drive.impedance0 > 1.0e-6f)
        {
            lastImpedanceSum_ = drive.impedance0;
            lastImpedanceMoment_ = drive.impedance1;
            lastImpedanceInertia_ = drive.impedance2;
        }
        else if (lastImpedanceSum_ > 1.0e-6f)
        {
            drive.impedance0 = lastImpedanceSum_;
            drive.impedance1 = lastImpedanceMoment_;
            drive.impedance2 = lastImpedanceInertia_;
        }
        const bool portIsLoaded = drive.impedance0 > 1.0e-6f;
        float bridgeDisplacement = 0.0f;
        float bridgeRotation = 0.0f;
        float reactionWave = portIsLoaded ? drive.incidentHeave : 0.0f;
        float reactionMoment = portIsLoaded ? drive.incidentRock : 0.0f;
        float bodyForceWave = reactionWave;
        float bodyMomentWave = reactionMoment;
        float tailForceWave = 0.0f;
        float tailMomentWave = 0.0f;
        if (bridgeCouplingEnabled_ && portIsLoaded)
        {
            bridgeLoad_.process(drive, inverseSampleRate_);
            bridgeDisplacement = bridgeLoad_.displacement;
            bridgeRotation = bridgeLoad_.rotation;
            reactionWave = bridgeLoad_.mainIntegratedForce;
            reactionMoment = bridgeLoad_.mainIntegratedMoment;
            bodyForceWave = bridgeLoad_.bodyIntegratedForce;
            bodyMomentWave = bridgeLoad_.bodyIntegratedMoment;
            tailForceWave = bridgeLoad_.tailIntegratedForce;
            tailMomentWave = bridgeLoad_.tailIntegratedMoment;
        }
        const float sampleRateRatio = static_cast<float>(sampleRate_) / 48000.0f;
        if (bridgeDerivativesNeedPriming_
            && (std::abs(reactionWave) + std::abs(bridgeDisplacement)
                > 1.0e-12f))
        {
            bridgeVelocityDerivative_.reset(bridgeDisplacement);
            bridgeRotationDerivative_.reset(bridgeRotation);
            bridgeForceDerivative_.reset(reactionWave);
            bridgeForceMomentDerivative_.reset(reactionMoment);
            bridgeBodyForceDerivative_.reset(bodyForceWave);
            bridgeBodyMomentDerivative_.reset(bodyMomentWave);
            bridgeTailForceDerivative_.reset(tailForceWave);
            bridgeTailMomentDerivative_.reset(tailMomentWave);
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
        // A note that starts while the instrument is sounding puts a whole
        // released shape into the junction's wave variables in one sample.
        // Differencing that reads as a bridge velocity the size of the entire
        // displacement: an impulse about ten times the note it belongs to,
        // on every note-on after the first.
        const bool crossingRelease = bridgeDerivativesCrossRelease_;
        bridgeDerivativesCrossRelease_ = false;
        const auto motion = [&] (FixedDerivative& derivative, float wave)
        {
            return crossingRelease
                ? derivative.processAcrossRelease(wave, sampleRateRatio)
                : derivative.process(wave, sampleRateRatio);
        };
        lastBridgeVelocity_ = motion(
            bridgeVelocityDerivative_, bridgeDisplacement);
        // The saddle's rocking rate. A string reads its own end's motion as
        // the heave plus its lever arm times this; the derivative is linear,
        // so two of them serve all six.
        const float bridgeRotationRate = motion(
            bridgeRotationDerivative_, bridgeRotation);
        lastBridgeReactionForce_ = motion(bridgeForceDerivative_, reactionWave);
        lastBridgeBodyForce_ = motion(bridgeBodyForceDerivative_, bodyForceWave);
        lastBridgeTailForce_ = motion(bridgeTailForceDerivative_, tailForceWave);
        // Power crosses the saddle in both coordinates, so each branch's is
        // the heave product plus the rocking one; reading only the first
        // would let the tail spring look like it stored negative energy.
        const float reactionMomentRate
            = motion(bridgeForceMomentDerivative_, reactionMoment);
        const float bodyMomentRate
            = motion(bridgeBodyMomentDerivative_, bodyMomentWave);
        const float tailMomentRate
            = motion(bridgeTailMomentDerivative_, tailMomentWave);
        lastBridgePower_ = lastBridgeVelocity_ * lastBridgeReactionForce_
            + bridgeRotationRate * reactionMomentRate;
        lastBridgeBodyPower_ = lastBridgeVelocity_ * lastBridgeBodyForce_
            + bridgeRotationRate * bodyMomentRate;
        lastBridgeTailPower_ = lastBridgeVelocity_ * lastBridgeTailForce_
            + bridgeRotationRate * tailMomentRate;

        float directLeft = 0.0f;
        float directRight = 0.0f;
        float sympatheticForce = 0.0f;
        float longitudinalForce = 0.0f;
        for (int string = 0; string < stringCount; ++string)
        {
            const float arm = saddleLeverArm(string);
            finishVoice(voices_[static_cast<std::size_t>(string)], string,
                verticalIncident[static_cast<std::size_t>(string)],
                horizontalIncident[static_cast<std::size_t>(string)],
                excitation[static_cast<std::size_t>(string)],
                tailIncident[static_cast<std::size_t>(string)],
                bridgeDisplacement + arm * bridgeRotation,
                lastBridgeVelocity_ + arm * bridgeRotationRate,
                directLeft, directRight, sympatheticForce,
                longitudinalForce);
        }

        // DAFx-26 Eq. (45): only Fb participates in the measured body
        // compliance, while unplayed open-string voices are summed one-way
        // into radiation. This force-scaled waveguide analogue uses unity
        // gain; neither term returns from the microphone radiation bank.
        lastSympatheticRadiationForce_ = sympatheticForce;
        lastLongitudinalForce_ = longitudinalForce;
        const BodyOutput body = renderBody(lastBridgeBodyForce_
            + lastSympatheticRadiationForce_ + lastLongitudinalForce_);

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

float AcustraEngine::getLastLongitudinalForce() const noexcept
{
    return lastLongitudinalForce_;
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
