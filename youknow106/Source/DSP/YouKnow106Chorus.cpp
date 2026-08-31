#include "YouKnow106Chorus.h"

#include <algorithm>
#include <cmath>
#include <limits>

#if defined(YOUKNOW106_WORK_AUDIT)
#include "../../Tools/OversamplingAuditSupport.h"

#define YOUKNOW106_COUNT_DOMAIN_WORK(field, amount)                         \
    do                                                                      \
    {                                                                       \
        if (auto* counters =                                                \
                youknow106::oversampling_audit::activeDomainWorkCounters)   \
            counters->field += (amount);                                    \
    } while (false)
#endif

namespace youknow106
{
namespace
{
// Shared double-precision pi. Four support-network builders below and the
// deterministic tone/heterodyne-bleed oscillators each wrote this literal out
// independently; one file-scope constant keeps them from drifting apart.
constexpr double pi = 3.14159265358979323846;

// Aggregate charge-transfer inefficiency of the whole line, condensed to one
// pole advanced at the BBD clock rate. The MN3009's 12 kHz bandwidth row at a
// 40 kHz clock describes the complete part, including the rectangular output
// hold, and is referenced to 1 kHz. This model's hold contributes
// sinc(12/40) = -1.326 dB versus DC, so the residual pole contributes -1.674 dB
// rather than another -3 dB. Solving at 0.3 cycles/sample gives -3.000 dB versus
// DC and -2.972 dB versus 1 kHz, within 0.03 dB of that datasheet row. The state
// advances once per modeled BBD shift (one fCP period), so a fixed coefficient
// already makes the pole's absolute frequency follow the clock. Scaling it
// again from clockHz would change the normalized response and double-count it.
//
// The 2026-08-07 two-phase output-stage solve confirmed this hold-plus-
// residual-pole structure outright -- OUT1/OUT2 present the same sample on
// complementary half-cycles, so their sum is a full-period hold -- but left
// the typical part's coefficient an unresolved span, because the datasheet's
// Gi-fi and Gi-fcp panels contradict each other about what the curves
// measure. The guaranteed-minimum anchor below sits inside every candidate
// reading's band and stands; the suites fence the cross-reading guard band.
constexpr float transferSmear = 0.8654743f;

// Static transfer of the delay line, referred to the model's signal scale
// (1.0 = 2.6 V at the node). A plain tanh with a 2.9 V asymptote already bends
// too far below the rails: it gives about 1.2% THD at the part's 0.78 Vrms
// test point instead of the specified typical 0.3%. The datasheet's 1.5 Vrms
// input-swing row is a guaranteed minimum measured at the 2.5% THD criterion,
// not a second typical-distortion anchor. This constrained algebraic clip keeps
// the approximately 2.9 V asymptote while fitting 0.3% at 0.78 Vrms and the
// typical curve's approximately 2% at 2.0 Vrms. Its quadratic term supplies
// the gentle low-level bend independently of the steep near-rail closure.
constexpr float bbdSaturationLevel = 1.1246614f; // 2.924 V at the node
constexpr float bbdSaturationCurvature = 1.2044546f;
constexpr float bbdSaturationExponent = 12.9395323f;

double bbdTransferBase(double normalised) noexcept
{
    return 1.0
         + static_cast<double>(bbdSaturationCurvature)
               * normalised * normalised
         + std::pow(normalised,
                    static_cast<double>(bbdSaturationExponent));
}

// The fitted exponent is non-integral, so there is no multiply/root identity
// analogous to the output summer's fixed eighth power. Sample the reference
// normalized curve through four rails instead; beyond that already-saturated
// range the defensive double-power path remains available for arbitrary finite
// inputs. Analytic slopes make a compact 512-interval Hermite table agree with
// the original float result to one ULP while remaining monotone.
constexpr std::size_t bbdTransferHermiteIntervals = 512u;
constexpr double bbdTransferHermiteLimit = 4.0;

struct BbdTransferHermiteNode
{
    double value {};
    double slope {};
};

const std::array<BbdTransferHermiteNode,
                 bbdTransferHermiteIntervals + 1u> bbdTransferHermiteTable = [] {
    std::array<BbdTransferHermiteNode,
               bbdTransferHermiteIntervals + 1u> result {};
    const double exponent = static_cast<double>(bbdSaturationExponent);
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        const double normalised = bbdTransferHermiteLimit
            * static_cast<double>(index)
            / static_cast<double>(bbdTransferHermiteIntervals);
        const double squared = normalised * normalised;
        const double base = bbdTransferBase(normalised);
        const double denominator = std::pow(base, 1.0 / exponent);
        result[index].value = normalised / denominator;
        result[index].slope =
            (1.0 + static_cast<double>(bbdSaturationCurvature)
                         * (1.0 - 2.0 / exponent) * squared)
            / (base * denominator);
    }
    return result;
}();

double interpolatedBbdTransfer(double normalised) noexcept
{
    const double position = normalised
        * static_cast<double>(bbdTransferHermiteIntervals)
        / bbdTransferHermiteLimit;
    const auto index = static_cast<std::size_t>(position);
    const double fraction = position - static_cast<double>(index);
    const double squared = fraction * fraction;
    const double cubic = squared * fraction;
    constexpr double width = bbdTransferHermiteLimit
                           / static_cast<double>(bbdTransferHermiteIntervals);
    const auto& first = bbdTransferHermiteTable[index];
    const auto& second = bbdTransferHermiteTable[index + 1u];
    return (2.0 * cubic - 3.0 * squared + 1.0) * first.value
         + (cubic - 2.0 * squared + fraction) * width * first.slope
         + (-2.0 * cubic + 3.0 * squared) * second.value
         + (cubic - squared) * width * second.slope;
}

// There is no divider ahead of the line. A previous revision put one here --
// 33 kOhm against 12 kOhm -- on a reading of the schematic that the schematic
// does not support: the 100 kOhm at each line's input injects adjustable DC
// bias from VR1/VR2 and is not the lower leg of anything, and the 10 kOhm
// against 2.2 nF beside it is a low-pass pole, not an attenuator. VR1/VR2 trim
// each line's DC bias so the positive and negative clipping points are
// symmetrical during the service procedure's 10 Vpp, 1 kHz TP2 test; they do
// not set a signal gain.
//
// The voice summer ahead of the whole effect does attenuate, but it attenuates
// dry and wet alike, so it is not a chorus divider either and cannot change the
// balance the summing resistors set.

// The independent random noise each wet line writes at its own clock edges is
// normalised by the explicit HISS-100 wet-line product policy. Its derivation
// is on `Chorus::independentLineRandomAmplitude` in the header, beside the
// separate Panasonic part-output maximum and the modelled board transfer.

// Support filters, from the schematic rather than from an estimate of it. Each
// side of the line carries two emitter-follower Sallen-Key sections built on
// equal 22 kOhm pairs, so each section's corner comes from its capacitors and
// its Q from their ratio alone:
//
//   Tr13 / Tr15   820 pF feedback, 680 pF shunt  ->  9.69 kHz, Q 0.549
//   Tr14 / Tr16   1.8 nF feedback, 270 pF shunt  -> 10.38 kHz, Q 1.291
//
// and the input adds one passive pole, R122 10 kOhm against C52 2.2 nF, at
// 7.23 kHz. The coupling capacitor C44/C47 adds a wet-only high-pass against
// R120/R114 100 kOhm at 15.9 Hz.
//
// This replaces a single pole at 9.9 kHz in and 9.5 kHz out, which was a guess
// at a fifth-order response rather than the response itself, and was therefore
// far too bright: four poles near 10 kHz reach -12 dB at 15 kHz where one
// reaches -4. It also settles the standing disagreement in the right
// direction. A sibling's wet path had been fitted with a *single* pole at
// 14 kHz, which cannot describe this circuit; the schematic shows why a
// single-pole fit was never going to.
constexpr float antiAliasFirstHz = 9688.0f;
constexpr float antiAliasFirstFeedbackF = 820.0e-12f;
constexpr float antiAliasFirstShuntF = 680.0e-12f;
constexpr float antiAliasSecondHz = 10377.0f;
constexpr float antiAliasSecondFeedbackF = 1.8e-9f;
constexpr float antiAliasSecondShuntF = 270.0e-12f;
constexpr float antiAliasPassiveHz = 7234.0f;   // R122 10 kOhm x C52 2.2 nF
constexpr float inputCouplingHz = 15.9155f;     // C44 0.1 uF x R120 100 kOhm
constexpr float wetOutputCouplingCapacitanceF = 1.0e-6f; // C28 / C25
constexpr float wetOutputBleedOhms = 22000.0f;           // R103 / R81
constexpr float wetMixerInputOhms = 39000.0f;            // R72 / R74

// The output's fifth pole is the BBD tap-summing node: either active output
// reaches C45/C48 2.2 nF through 3.3 kOhm, and R117/R110 47 kOhm returns the
// node to ground. Treating the active BBD output as an ideal source gives
// (3.3 kOhm || 47 kOhm) against 2.2 nF, or 23.46138 kHz. The datasheet's
// Gi-RL panel now bounds the summed-output source impedance near 3.7 kOhm,
// which would put the loaded pole at 11.9-22.2 kHz depending on how the two
// follower legs share the node -- recorded against OQ-04, whose declared MNA
// or wet-only sweep route decides it rather than a silent retune here.
//
// The reconstruction corners below equal the anti-alias corners because the
// 106's own p. 15 scan reads them so at designator level (2026-08-07):
// pre-BBD C33 820p/C31 680p then C34 1.8n/C32 270p, and per output line
// C37/C35 and C38/C36 (line 1), C42/C40 and C43/C41 (line 2), every section
// the same 22k/22k pair, alongside per-BBD 10k/2.2n input poles (R122/R115,
// C52/C56), both 3.3k tap pairs into 47k/2.2n, and 100n/100k branch
// coupling -- agreeing with the sister board's clone netlist that first
// corroborated the family. OQ-04 keeps only the loaded transfer.
constexpr float idealSourceTapPoleHz = 23461.38f;
constexpr float reconstructionFirstHz = 9688.0f;
constexpr float reconstructionSecondHz = 10377.0f;

// The wet-mute glide is expressed relative to dry. The final IC6 summer's
// absolute 100/47 dry gain is applied after the BBDs in process(); putting it
// before them would drive their fitted nonlinearity too hard.
constexpr float lineGain = Chorus::wetToDryGain;   // 47/39, +1.62 dB

template <std::size_t Size>
using FixedMatrix = std::array<std::array<double, Size>, Size>;

template <std::size_t Size>
FixedMatrix<Size> matrixIdentity() noexcept
{
    FixedMatrix<Size> result {};
    for (std::size_t index = 0; index < Size; ++index)
        result[index][index] = 1.0;
    return result;
}

template <std::size_t Size>
FixedMatrix<Size> matrixMultiply(const FixedMatrix<Size>& left,
                                 const FixedMatrix<Size>& right) noexcept
{
    FixedMatrix<Size> result {};
    for (std::size_t row = 0; row < Size; ++row)
        for (std::size_t inner = 0; inner < Size; ++inner)
        {
            const double value = left[row][inner];
            for (std::size_t column = 0; column < Size; ++column)
                result[row][column] += value * right[inner][column];
        }
    return result;
}

template <std::size_t Size>
FixedMatrix<Size> matrixLinearCombination(
    const FixedMatrix<Size>& left, double leftScale,
    const FixedMatrix<Size>& right, double rightScale) noexcept
{
    FixedMatrix<Size> result {};
    for (std::size_t row = 0; row < Size; ++row)
        for (std::size_t column = 0; column < Size; ++column)
            result[row][column] = leftScale * left[row][column]
                                + rightScale * right[row][column];
    return result;
}

template <std::size_t Size>
bool matrixSolve(FixedMatrix<Size> left, FixedMatrix<Size> right,
                 FixedMatrix<Size>& result) noexcept
{
    for (std::size_t pivot = 0; pivot < Size; ++pivot)
    {
        std::size_t best = pivot;
        double bestMagnitude = std::abs(left[pivot][pivot]);
        for (std::size_t row = pivot + 1; row < Size; ++row)
        {
            const double magnitude = std::abs(left[row][pivot]);
            if (magnitude > bestMagnitude)
            {
                best = row;
                bestMagnitude = magnitude;
            }
        }
        if (!(bestMagnitude > 1.0e-30) || !std::isfinite(bestMagnitude))
            return false;
        if (best != pivot)
        {
            std::swap(left[best], left[pivot]);
            std::swap(right[best], right[pivot]);
        }

        const double inversePivot = 1.0 / left[pivot][pivot];
        for (std::size_t column = pivot; column < Size; ++column)
            left[pivot][column] *= inversePivot;
        for (std::size_t column = 0; column < Size; ++column)
            right[pivot][column] *= inversePivot;

        for (std::size_t row = 0; row < Size; ++row)
        {
            if (row == pivot)
                continue;
            const double factor = left[row][pivot];
            left[row][pivot] = 0.0;
            for (std::size_t column = pivot + 1; column < Size; ++column)
                left[row][column] -= factor * left[pivot][column];
            for (std::size_t column = 0; column < Size; ++column)
                right[row][column] -= factor * right[pivot][column];
        }
    }
    result = right;
    return true;
}

// Higham's scaling-and-squaring [13/13] Pade construction.  The augmented
// matrix is dimensionless (one numerical interval), keeping its norm bounded
// even though the analog state coordinates are expressed in volts.
template <std::size_t Size>
FixedMatrix<Size> matrixExponential(FixedMatrix<Size> matrix) noexcept
{
    constexpr std::array<double, 14> b {
        64764752532480000.0, 32382376266240000.0,
        7771770303897600.0, 1187353796428800.0,
        129060195264000.0, 10559470521600.0,
        670442572800.0, 33522128640.0, 1323241920.0,
        40840800.0, 960960.0, 16380.0, 182.0, 1.0
    };
    constexpr double theta13 = 5.371920351148152;

    double norm = 0.0;
    for (std::size_t column = 0; column < Size; ++column)
    {
        double sum = 0.0;
        for (std::size_t row = 0; row < Size; ++row)
            sum += std::abs(matrix[row][column]);
        norm = std::max(norm, sum);
    }
    int squarings = 0;
    if (norm > theta13)
        squarings = std::max(0, static_cast<int>(
            std::ceil(std::log2(norm / theta13))));
    const double scale = std::ldexp(1.0, -squarings);
    for (auto& row : matrix)
        for (double& value : row)
            value *= scale;

    const auto identity = matrixIdentity<Size>();
    const auto a2 = matrixMultiply(matrix, matrix);
    const auto a4 = matrixMultiply(a2, a2);
    const auto a6 = matrixMultiply(a4, a2);

    auto innerU = matrixLinearCombination(a6, b[13], a4, b[11]);
    innerU = matrixLinearCombination(innerU, 1.0, a2, b[9]);
    innerU = matrixMultiply(a6, innerU);
    innerU = matrixLinearCombination(innerU, 1.0, a6, b[7]);
    innerU = matrixLinearCombination(innerU, 1.0, a4, b[5]);
    innerU = matrixLinearCombination(innerU, 1.0, a2, b[3]);
    innerU = matrixLinearCombination(innerU, 1.0, identity, b[1]);
    const auto u = matrixMultiply(matrix, innerU);

    auto innerV = matrixLinearCombination(a6, b[12], a4, b[10]);
    innerV = matrixLinearCombination(innerV, 1.0, a2, b[8]);
    auto v = matrixMultiply(a6, innerV);
    v = matrixLinearCombination(v, 1.0, a6, b[6]);
    v = matrixLinearCombination(v, 1.0, a4, b[4]);
    v = matrixLinearCombination(v, 1.0, a2, b[2]);
    v = matrixLinearCombination(v, 1.0, identity, b[0]);

    const auto denominator = matrixLinearCombination(v, 1.0, u, -1.0);
    const auto numerator = matrixLinearCombination(v, 1.0, u, 1.0);
    FixedMatrix<Size> result {};
    if (!matrixSolve(denominator, numerator, result))
        return identity;
    for (int step = 0; step < squarings; ++step)
        result = matrixMultiply(result, result);
    return result;
}

using AnalogMatrix = FixedMatrix<6>;
using AnalogDrive = std::array<double, 6>;

Chorus::SupportChain::ExactTransition exactTransition(
    const AnalogMatrix& analog, const AnalogDrive& analogDrive,
    const std::array<double, 6>& constantInputEquilibrium,
    double sampleRate) noexcept
{
    FixedMatrix<10> augmented {};
    const double interval = 1.0 / sampleRate;
    for (std::size_t row = 0; row < 6; ++row)
    {
        for (std::size_t column = 0; column < 6; ++column)
            augmented[row][column] = interval * analog[row][column];
        augmented[row][6] = interval * analogDrive[row];
    }
    augmented[6][7] = 1.0;
    augmented[7][8] = 1.0;
    augmented[8][9] = 1.0;
    const auto exponential = matrixExponential(augmented);

    // Initial polynomial coordinates at the previous sample for the unique
    // cubic through u[n], u[n-1], u[n-2], u[n-3].
    constexpr std::array<std::array<double, 4>, 4> polynomial {
        std::array<double, 4> { 0.0, 1.0, 0.0, 0.0 },
        std::array<double, 4> { 1.0 / 3.0, 0.5, -1.0, 1.0 / 6.0 },
        std::array<double, 4> { 1.0, -2.0, 1.0, 0.0 },
        std::array<double, 4> { 1.0, -3.0, 3.0, -1.0 }
    };

    Chorus::SupportChain::ExactTransition result;
    for (std::size_t row = 0; row < 6; ++row)
    {
        for (std::size_t column = 0; column < 6; ++column)
            result.stateByColumn[column][row] = exponential[row][column];
        for (std::size_t sample = 0; sample < 4; ++sample)
            for (std::size_t derivative = 0; derivative < 4; ++derivative)
                result.driveBySample[sample][row] +=
                    exponential[row][6 + derivative]
                    * polynomial[derivative][sample];

        // Force the constant-input equilibrium identity exactly in stored
        // double arithmetic. This removes an otherwise tiny DC leak from
        // cancellation around the very slow coupling pole at 768 kHz.
        double target = constantInputEquilibrium[row];
        for (std::size_t column = 0; column < 6; ++column)
            target -= result.stateByColumn[column][row]
                    * constantInputEquilibrium[column];
        double actual = 0.0;
        for (std::size_t sample = 0; sample < 4; ++sample)
            actual += result.driveBySample[sample][row];
        result.driveBySample[0][row] += target - actual;
    }
    return result;
}

AnalogMatrix inputSupportMatrix() noexcept
{
    const double w1 = 2.0 * pi * antiAliasFirstHz;
    const double w2 = 2.0 * pi * antiAliasSecondHz;
    const double wc = 2.0 * pi * inputCouplingHz;
    const double wp = 2.0 * pi * antiAliasPassiveHz;
    const double k1 = 1.0 / Chorus::sallenKeyQ(
        antiAliasFirstFeedbackF, antiAliasFirstShuntF);
    const double k2 = 1.0 / Chorus::sallenKeyQ(
        antiAliasSecondFeedbackF, antiAliasSecondShuntF);
    AnalogMatrix matrix {};
    // Voltage-like analog integrator coordinates: BP1, LP1, BP2, LP2,
    // coupling-capacitor lowpass voltage and passive-pole output voltage.
    matrix[0][0] = -k1 * w1;
    matrix[0][1] = -w1;
    matrix[1][0] = w1;
    matrix[2][1] = w2;
    matrix[2][2] = -k2 * w2;
    matrix[2][3] = -w2;
    matrix[3][2] = w2;
    matrix[4][3] = wc;
    matrix[4][4] = -wc;
    matrix[5][3] = wp;
    matrix[5][4] = -wp;
    matrix[5][5] = -wp;
    return matrix;
}

AnalogDrive inputSupportDrive() noexcept
{
    AnalogDrive drive {};
    drive[0] = 2.0 * pi * antiAliasFirstHz;
    return drive;
}

AnalogMatrix outputSupportMatrix(bool wetConnected) noexcept
{
    const double wt = 2.0 * pi * idealSourceTapPoleHz;
    const double w1 = 2.0 * pi * reconstructionFirstHz;
    const double w2 = 2.0 * pi * reconstructionSecondHz;
    const double wc = 2.0 * pi
        * Chorus::wetOutputCouplingCornerHz(wetConnected);
    const double k1 = 1.0 / Chorus::sallenKeyQ(
        antiAliasFirstFeedbackF, antiAliasFirstShuntF);
    const double k2 = 1.0 / Chorus::sallenKeyQ(
        antiAliasSecondFeedbackF, antiAliasSecondShuntF);
    AnalogMatrix matrix {};
    // Tap LP, then BP1/LP1, BP2/LP2, and the output-coupling lowpass
    // capacitor voltage.  Every coordinate remains meaningful when wc swaps.
    matrix[0][0] = -wt;
    matrix[1][0] = w1;
    matrix[1][1] = -k1 * w1;
    matrix[1][2] = -w1;
    matrix[2][1] = w1;
    matrix[3][2] = w2;
    matrix[3][3] = -k2 * w2;
    matrix[3][4] = -w2;
    matrix[4][3] = w2;
    matrix[5][4] = wc;
    matrix[5][5] = -wc;
    return matrix;
}

AnalogDrive outputSupportDrive() noexcept
{
    AnalogDrive drive {};
    drive[0] = 2.0 * pi * idealSourceTapPoleHz;
    return drive;
}

void advanceExactSupport(
    std::array<double, 6>& state,
    const Chorus::SupportChain::ExactTransition& transition,
    double current, double previous, double previous2,
    double previous3) noexcept
{
    constexpr double maximumState =
        static_cast<double>(std::numeric_limits<float>::max()) / 16.0;
    // Both production callers sanitize the current sample before storing it
    // in these histories. Keep the final state guard below as the recovery
    // boundary instead of rechecking all four owned values on every advance.
    const std::array<double, 4> samples {
        current, previous, previous2, previous3
    };
    std::array<double, 6> next {};
    for (std::size_t column = 0; column < 6; ++column)
        for (std::size_t row = 0; row < 6; ++row)
            next[row] += transition.stateByColumn[column][row]
                       * state[column];
    for (std::size_t sample = 0; sample < 4; ++sample)
        for (std::size_t row = 0; row < 6; ++row)
            next[row] += transition.driveBySample[sample][row]
                       * samples[sample];
    for (double value : next)
    {
        if (!(std::abs(value) <= maximumState))
        {
            state.fill(0.0);
            return;
        }
    }
    state = next;
}

std::uint32_t nextNoiseState(std::uint32_t state) noexcept
{
    if (state == 0u)
        state = 0xd1b54a35u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float noiseFromState(std::uint32_t state) noexcept
{
    return static_cast<float>(state & 0xffffffu) * (2.0f / 16777215.0f) - 1.0f;
}

// Symmetric triangle over a 0..1 phase, in -1..+1.
//
// This is the shape the circuit makes, not a convenient stand-in for it. IC1b
// integrates a constant current for the whole of each half cycle -- see the
// derivation on Chorus::lfoTimingOhms -- so both flanks are straight. An RC
// relaxation oscillator would bend them, and this one is not that.
float triangle(double phase) noexcept
{
    const double folded = phase < 0.5 ? phase : 1.0 - phase;
    return static_cast<float>(folded * 4.0 - 1.0);
}

} // namespace

Chorus::StereoNoiseSample Chorus::correlatedRandomStep(
    std::uint32_t& commonState, std::uint32_t& orthogonalState,
    float correlation) noexcept
{
    commonState = nextNoiseState(commonState);
    orthogonalState = nextNoiseState(orthogonalState);

    const float common = noiseFromState(commonState);
    const float orthogonal = noiseFromState(orthogonalState);
    const float rho = std::isfinite(correlation)
        ? std::clamp(correlation, -1.0f, 1.0f) : 0.0f;
    const float orthogonalGain = std::sqrt(std::max(0.0f, 1.0f - rho * rho));
    return { common, rho * common + orthogonalGain * orthogonal };
}

float Chorus::deterministicToneStep(double& phase, float frequencyHz,
                                    float sampleRate) noexcept
{
    if (!std::isfinite(frequencyHz) || !std::isfinite(sampleRate)
        || sampleRate <= 0.0f)
        return 0.0f;

    phase += static_cast<double>(std::max(frequencyHz, 0.0f))
           / static_cast<double>(sampleRate);
    phase -= std::floor(phase);
    return static_cast<float>(std::sin(2.0 * pi * phase));
}

Chorus::ModeSettings Chorus::settingsFor(ChorusMode mode) noexcept
{
    // The rates are this instrument's own, straight from its circuit:
    // derivedRateHz() evaluates f = 1/(4 * beta * R_eff * C3) with the
    // schematic's timing network (6.4352941 MOhm for I, 3.9638889 MOhm for
    // II), the summing-node comparator ratio 33/47 and the 0.1 uF integrator
    // capacitor, landing 0.55329 Hz and 0.89826 Hz. Scope readings of a
    // 106-chorus clone corroborate both within 3% (0.537/0.879 Hz), and both
    // truncate to the published about-0.5 and about-0.8. The JUNO-60 pair that
    // used to stand in for the scale is superseded, as its 1.682 ratio already
    // was; the suites keep both only as comparison values.
    //
    // The sweep endpoints are the 106's own third-party-measured figures:
    // delay 1.4 ms to 6.4 ms, scoped on a designator-faithful build of the
    // p. 15 chorus board carrying genuine 256-stage MN3009s and compared
    // directly against a real JUNO-106 by its owner, who published the scope
    // plots and called the two sweeps identical. The same excursion --
    // +/-2.5 ms about a 3.9 ms centre -- also matches the one independent
    // depth report on record. This supersedes the JUNO-60's calibrated
    // 1.66-5.35 ms capture, which stays in the suite as the
    // sibling-instrument comparison value: the two boards share their Tr22
    // clock driver, but not every timing part around it. Two narrower clone
    // clock readings (28-38 kHz expected by that kit's build guide, 28-60 kHz
    // observed on a suspected-faulty build) remain on record as
    // contradictions under OQ-01, which still requests a calibrated capture
    // of an original unit.
    //
    // Modes I and II differ in speed alone, not in depth: the mode line changes
    // a timing resistance, while the triangle's amplitude is set by the
    // comparator's threshold ratio, which the mode line does not touch. That
    // is why II reads as more agitated rather than wider.
    constexpr float centre = 0.5f * (0.0014f + 0.0064f);
    constexpr float sweep = 0.5f * (0.0064f - 0.0014f);
    constexpr float rateOne = static_cast<float>(derivedRateHz(true));
    constexpr float rateTwo = static_cast<float>(derivedRateHz(false));
    switch (mode)
    {
        case ChorusMode::One:  return { rateOne, centre, sweep, lineGain };
        case ChorusMode::Two:  return { rateTwo, centre, sweep, lineGain };
        // Product compatibility policy, not a claim about a third resistance
        // in the original circuit. The audited Roland Cloud JUNO-106 exposes
        // only Off/I/II, and no calibrated original-unit both-button trace is
        // published. Retain this plug-in's established summed-rate sound so
        // the fourth state is stable and audibly distinct. The shared centre,
        // depth and gain are the least speculative continuation of the two
        // measured modes; a qualifying I+II capture can replace this policy.
        case ChorusMode::OneTwo:
            return { rateOne + rateTwo, centre, sweep, lineGain };
        case ChorusMode::Off:
        default:
            // Bypass only mutes the wet return. The oscillator and both BBDs
            // continue to run behind the mute, so an effect prepared while
            // off still needs a real clock programme and sweep depth.
            return { rateOne, centre, sweep, 0.0f };
    }
}

float Chorus::bbdTransfer(float input) noexcept
{
    if (!std::isfinite(input))
        return 0.0f;

    // Interpolate in double precision through the range reached by ordinary
    // audio. The double-power fallback beyond four rails lets even an extreme
    // finite float approach the rail instead of overflowing an intermediate
    // power and folding back to zero.
    const double normalised = std::abs(static_cast<double>(input))
                            / static_cast<double>(bbdSaturationLevel);
    if (normalised < bbdTransferHermiteLimit)
        return std::copysign(static_cast<float>(
            static_cast<double>(bbdSaturationLevel)
                * interpolatedBbdTransfer(normalised)), input);
    const double inverse = 1.0 / normalised;
    const double exponent = static_cast<double>(bbdSaturationExponent);
    const double correction = std::pow(
        1.0
            + static_cast<double>(bbdSaturationCurvature)
                  * std::pow(inverse, exponent - 2.0)
            + std::pow(inverse, exponent),
        1.0 / exponent);
    return std::copysign(static_cast<float>(
        static_cast<double>(bbdSaturationLevel) / correction), input);
}

float Chorus::transferLossStep(float& state, float input) noexcept
{
    // One fixed per-transfer coefficient, advanced once per clock edge. The
    // recursion runs on the clock grid, so the absolute corner already moves
    // with the clock exactly as the physical per-stage inefficiency does; a
    // previous revision additionally scaled the coefficient with the clock
    // (unity at an uncited 26 kHz, slope 1.5e-6 per Hz), which un-anchored
    // the derivation -- at the datasheet's own 40 kHz condition it rendered
    // -2.76 dB where the part is specified -3.0 dB -- and brightened with
    // clock where per-transfer loss physically worsens. The fixture drives
    // this exact function, so the anchor holds at every clock again.
    state += transferSmear * (input - state);
    return state;
}

float Chorus::interpolateBbdInput(float current, float previous,
                                  float previous2, float previous3,
                                  double ageInSamples) noexcept
{
    if (!std::isfinite(current) || !std::isfinite(previous)
        || !std::isfinite(previous2) || !std::isfinite(previous3)
        || !std::isfinite(ageInSamples))
        return 0.0f;

    const double age = std::clamp(ageInSamples, 0.0, 1.0);
    const double t = -age;
    const double l0 = (t + 1.0) * (t + 2.0) * (t + 3.0) / 6.0;
    const double l1 = -t * (t + 2.0) * (t + 3.0) / 2.0;
    const double l2 = t * (t + 1.0) * (t + 3.0) / 2.0;
    const double l3 = -t * (t + 1.0) * (t + 2.0) / 6.0;
    const double interpolated = l0 * static_cast<double>(current)
                              + l1 * static_cast<double>(previous)
                              + l2 * static_cast<double>(previous2)
                              + l3 * static_cast<double>(previous3);
    if (!std::isfinite(interpolated))
        return 0.0f;

    constexpr double maximum =
        static_cast<double>(std::numeric_limits<float>::max());
    return static_cast<float>(std::clamp(interpolated, -maximum, maximum));
}

double Chorus::bbdPolyBlepResidual(double distanceInSamples) noexcept
{
    // Integrated third-order Lagrange residual used by Gabrielli, D'Angelo
    // and Squartini's BBD reference implementation. Its support covers the
    // two numerical samples on either side of a discontinuity. Distances in
    // this model are always non-negative; handling NaN and negative test input
    // as zero keeps this pure helper defensive without extending the curve.
    if (!(distanceInSamples >= 0.0) || distanceInSamples >= 2.0)
        return 0.0;

    const double d = distanceInSamples;
    if (d < 1.0)
    {
        return ((((0.125 * d - 1.0 / 3.0) * d - 0.25) * d + 1.0)
                 * d - 0.5);
    }

    return ((((-1.0 / 24.0 * d + 1.0 / 3.0) * d - 11.0 / 12.0)
             * d + 1.0) * d - 1.0 / 3.0);
}

float Chorus::onePoleG(float cutoffHz, float sampleRate) noexcept
{
    // The wet coupling pole is at 15.9 Hz, so the old 20 Hz defensive floor
    // silently moved a real component value. A small positive floor still
    // protects tan() from invalid callers without voicing the circuit.
    //
    // The upper ceiling is defensive: tan() turns negative past fs/2 and
    // would invert the recursion. On the audited 44.1-96 kHz fallback grids,
    // shipping calls use only the 15.9 Hz coupling and 7.234 kHz passive
    // corners, comfortably below it. Lower supported stress-test grids are
    // necessarily Nyquist-limited; the exact output transition owns the
    // higher tap/reconstruction poles at every rate.
    const float limited = std::clamp(cutoffHz, 0.1f, sampleRate * 0.45f);
    const float g = std::tan(3.14159265358979324f * limited / sampleRate);
    return g / (1.0f + g);
}

// One topology-preserving-transform lowpass step. The coefficient above only
// places the corner where it was asked for if the state is advanced by twice
// the difference term: the plain `state += g * (input - state)` recursion wants
// a different coefficient entirely and would put the 9.9 kHz corner near
// 4.6 kHz at the engine's 192 kHz internal rate.
float Chorus::supportFilterStep(float& state, float input, float g) noexcept
{
    const float difference = (input - state) * g;
    const float output = difference + state;
    state = output + difference;
    return output;
}

float Chorus::wetOutputCouplingCornerHz(bool wetConnected) noexcept
{
    const float resistance = wetConnected
        ? wetOutputBleedOhms * wetMixerInputOhms
            / (wetOutputBleedOhms + wetMixerInputOhms)
        : wetOutputBleedOhms;
    return 1.0f / (2.0f * static_cast<float>(pi)
                   * wetOutputCouplingCapacitanceF * resistance);
}

// An equal-resistor Sallen-Key's damping comes entirely from its capacitor
// ratio. Both sections here are built that way -- 22 kOhm twice -- so this is
// the whole of their Q.
float Chorus::sallenKeyQ(float feedbackFarads, float shuntFarads) noexcept
{
    if (shuntFarads <= 0.0f)
        return 0.5f;
    return 0.5f * std::sqrt(feedbackFarads / shuntFarads);
}

Chorus::BiquadCoefficients Chorus::sallenKeyCoefficients(float cutoffHz, float q,
                                                         float sampleRate) noexcept
{
    const float limited = std::clamp(cutoffHz, 20.0f, sampleRate * 0.45f);
    BiquadCoefficients coefficients;
    coefficients.g = std::tan(3.14159265358979324f * limited / sampleRate);
    coefficients.k = 1.0f / std::max(q, 0.05f);
    return coefficients;
}

// Zavalishin's topology-preserving state-variable form, lowpass output. The
// prewarped `g` puts the corner where it was asked for at any host rate, which
// a direct-form biquad with bilinear coefficients would not do as cleanly at
// the engine's 192 kHz internal rate.
float Chorus::biquadStep(BiquadState& state, float input,
                         const BiquadCoefficients& coefficients) noexcept
{
    const float g = coefficients.g;
    const float k = coefficients.k;
    // `k` is the form's own damping term, which is already 1/Q -- doubling it
    // here would halve every section's Q and darken the whole chain.
    const float highPass = (input - (k + g) * state.s1 - state.s2)
                         / (1.0f + g * (g + k));
    const float bandPass = g * highPass + state.s1;
    state.s1 = g * highPass + bandPass;
    const float lowPass = g * bandPass + state.s2;
    state.s2 = g * bandPass + lowPass;
    return lowPass;
}

Chorus::SupportChain Chorus::supportChainFor(float sampleRate) noexcept
{
    SupportChain chain;
    chain.inputCouplingG = onePoleG(inputCouplingHz, sampleRate);
    chain.passiveG = onePoleG(antiAliasPassiveHz, sampleRate);
    chain.antiAliasFirst = sallenKeyCoefficients(
        antiAliasFirstHz,
        sallenKeyQ(antiAliasFirstFeedbackF, antiAliasFirstShuntF), sampleRate);
    chain.antiAliasSecond = sallenKeyCoefficients(
        antiAliasSecondHz,
        sallenKeyQ(antiAliasSecondFeedbackF, antiAliasSecondShuntF), sampleRate);
    constexpr std::array<double, 6> inputEquilibrium {
        0.0, 1.0, 0.0, 1.0, 1.0, 0.0
    };
    constexpr std::array<double, 6> outputEquilibrium {
        1.0, 0.0, 1.0, 0.0, 1.0, 1.0
    };
    chain.exactInput = exactTransition(
        inputSupportMatrix(), inputSupportDrive(), inputEquilibrium, sampleRate);
    chain.exactOutputMuted = exactTransition(
        outputSupportMatrix(false), outputSupportDrive(), outputEquilibrium,
        sampleRate);
    chain.exactOutputConnected = exactTransition(
        outputSupportMatrix(true), outputSupportDrive(), outputEquilibrium,
        sampleRate);
    return chain;
}

void Chorus::InputSupport::reset() noexcept
{
    couplingState = 0.0f;
    passiveState = 0.0f;
    antiAliasFirst.reset();
    antiAliasSecond.reset();
    exactState.fill(0.0);
    exactPrevious = 0.0;
    exactPrevious2 = 0.0;
    exactPrevious3 = 0.0;
}

float Chorus::advanceInputSupport(float input) noexcept
{
    // Band-limit ahead of the lines. Everything above half the clock would
    // fold, exactly as it does in the part. The two Sallen-Key sections
    // precede the wet-only C44/C47 coupling high-pass; the passive
    // 10 kOhm / 2.2 nF pole is last, immediately beside the MN3009 input.
    // A corrupt host buffer must not poison the persistent support state.
    // Sixty-four model units is over 160 V at this node and therefore far
    // outside every circuit or engine fixture; treat anything beyond it as a
    // corrupt sample instead of turning it into a long full-scale BBD burst.
    constexpr float maximumSupportInput = 64.0f;
    const float supportInput = std::isfinite(input)
            && std::abs(input) <= maximumSupportInput
        ? input : 0.0f;
    if (sampleRate_ >= Chorus::minimumExactInputSupportRate)
    {
#if defined(YOUKNOW106_WORK_AUDIT)
        YOUKNOW106_COUNT_DOMAIN_WORK(bbdExactInputSupportAdvances, 1);
        YOUKNOW106_COUNT_DOMAIN_WORK(bbdExactSupportCoordinateUpdates, 6);
        YOUKNOW106_COUNT_DOMAIN_WORK(bbdExactSupportMacs, 60);
#endif
        const double exactInput = static_cast<double>(supportInput);
        advanceExactSupport(
            inputSupport_.exactState, support_.exactInput,
            exactInput, inputSupport_.exactPrevious,
            inputSupport_.exactPrevious2, inputSupport_.exactPrevious3);
        inputSupport_.exactPrevious3 = inputSupport_.exactPrevious2;
        inputSupport_.exactPrevious2 = inputSupport_.exactPrevious;
        inputSupport_.exactPrevious = exactInput;
        return static_cast<float>(inputSupport_.exactState[5]);
    }

#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(bbdLegacyInputSupportFrames, 1);
#endif
    float limited = Chorus::biquadStep(
        inputSupport_.antiAliasFirst, supportInput, support_.antiAliasFirst);
    limited = Chorus::biquadStep(
        inputSupport_.antiAliasSecond, limited, support_.antiAliasSecond);
    const float couplingLow = Chorus::supportFilterStep(
        inputSupport_.couplingState, limited, support_.inputCouplingG);
    limited -= couplingLow;
    return Chorus::supportFilterStep(
        inputSupport_.passiveState, limited, support_.passiveG);
}

void Chorus::Line::reset(std::uint32_t seed) noexcept
{
    cells.fill(0.0f);
    writeIndex = 0;
    clockPhase = 0.0;
    held = 0.0f;
    previousInput = 0.0f;
    previousInput2 = 0.0f;
    previousInput3 = 0.0f;
    exactOutputState.fill(0.0);
    exactOutputPrevious = 0.0f;
    exactOutputPrevious2 = 0.0f;
    exactOutputPrevious3 = 0.0f;
    transferState = 0.0f;
    noiseState = seed | 1u;
    pastBlepEvents.fill({});
    pastBlepEventCount = 0;
}

void Chorus::Line::resetAudioRateSupport() noexcept
{
    // The interpolation histories are indexed on the numerical grid, and the
    // legacy low-rate TPT carries below embed its old interval. The engine
    // calls this only at zero output gain after waiting for musical tails.
    // BBD buckets, write/clock position, transfer state, held output and RNG
    // remain free-running.
    previousInput = 0.0f;
    previousInput2 = 0.0f;
    previousInput3 = 0.0f;
    // The exact coordinates are physical and mode-compatible, but this rate
    // boundary still deliberately reinitializes the entire numerical support
    // under the engine's established zero-gain transition. Preserving them
    // while clearing/reseeding the cubic drive is a separate qualification.
    // The shared input side is cleared beside this, by the same callers.
    exactOutputState.fill(0.0);
    exactOutputPrevious = 0.0f;
    exactOutputPrevious2 = 0.0f;
    exactOutputPrevious3 = 0.0f;
    // Event ages are measured in samples of the numerical output grid. They
    // cannot be reinterpreted at the new rate, unlike the literal BBD state
    // deliberately preserved above. Their complete support is only two old
    // samples and the engine performs this reset under a zero-gain fade.
    pastBlepEvents.fill({});
    pastBlepEventCount = 0;
}

void Chorus::Line::ageBlepEvents() noexcept
{
    int retained = 0;
    for (int index = 0; index < pastBlepEventCount; ++index)
    {
        auto event = pastBlepEvents[static_cast<std::size_t>(index)];
        event.ageInSamples += 1.0;
        if (event.ageInSamples < 2.0)
            pastBlepEvents[static_cast<std::size_t>(retained++)] = event;
    }
    pastBlepEventCount = retained;
}

void Chorus::Line::rememberBlepEvent(float jump,
                                     double ageInSamples) noexcept
{
    // The compile-time bound above proves this cannot fill at any supported
    // clock/sample-rate ratio. Keep the guard in release builds nonetheless:
    // a future caller violating those declared limits must remain finite and
    // real-time safe rather than writing outside the fixed array.
    if (pastBlepEventCount >= maximumBlepEvents)
        return;

    pastBlepEvents[static_cast<std::size_t>(pastBlepEventCount++)] = {
        jump, ageInSamples
    };
}

double Chorus::Line::deterministicBlepCorrection(
    double clockIncrement) const noexcept
{
    double correction = 0.0;

    // If the most recent edge changed s[-1] to s[0] by delta, the reference
    // output is s[0] + delta * beta(age). Older past edges have the same sign.
    for (int index = 0; index < pastBlepEventCount; ++index)
    {
#if defined(YOUKNOW106_WORK_AUDIT)
        YOUKNOW106_COUNT_DOMAIN_WORK(blepPastCorrectionVisits, 1);
#endif
        const auto& event = pastBlepEvents[static_cast<std::size_t>(index)];
        correction += static_cast<double>(event.jump)
                    * Chorus::bbdPolyBlepResidual(event.ageInSamples);
    }

    if (!(clockIncrement > 0.0) || !std::isfinite(clockIncrement))
        return correction;

    // Buckets that will emerge during the residual's two-sample lookahead are
    // already in the ring: even at 200 kHz / 8 kHz there are at most 50, short
    // of one 128-cell revolution. Advance a local copy of the aggregate
    // transfer-loss state through those known values. Nothing physical --
    // bucket contents/index, phase, held noise, transfer state or RNG -- moves.
    const double inverseIncrement = 1.0 / clockIncrement;
    double distance = (1.0 - clockPhase) * inverseIncrement;
    float predictedTransferState = transferState;
    int futureIndex = writeIndex;

    for (int event = 0;
         event < maximumBlepEvents && distance < 2.0;
         ++event, distance += inverseIncrement)
    {
#if defined(YOUKNOW106_WORK_AUDIT)
        YOUKNOW106_COUNT_DOMAIN_WORK(blepFuturePredictionVisits, 1);
#endif
        futureIndex = futureIndex + 1 < cellPairs ? futureIndex + 1 : 0;
        const float before = predictedTransferState;
        Chorus::transferLossStep(
            predictedTransferState,
            cells[static_cast<std::size_t>(futureIndex)]);
        const float jump = predictedTransferState - before;

        // A future change s[0] -> s[1] enters the authors' correction with
        // the opposite sign: s[0] - (s[1] - s[0]) * beta(timeUntilEdge).
        correction -= static_cast<double>(jump)
                    * Chorus::bbdPolyBlepResidual(distance);
    }

    return correction;
}

float Chorus::Line::processClockedCore(float limitedInput, float clockHz,
                                       float sampleRate,
                                       float noiseScale) noexcept
{
    ageBlepEvents();

    const double increment =
        static_cast<double>(clockHz) / static_cast<double>(sampleRate);
    clockPhase += increment;
    // A clock above the host rate needs more than one shift per sample, which
    // is exactly what happens at 44.1 or 48 kHz with oversampling switched off.
    // The bound is the worst ratio the model supports -- the fastest clock
    // against the lowest host rate -- so every elapsed edge is consumed and no
    // backlog can build up and drag the delay off its setting.
    int shifts = 0;
    while (clockPhase >= 1.0 && shifts < maximumShiftsPerSample)
    {
        clockPhase -= 1.0;
        ++shifts;
#if defined(YOUKNOW106_WORK_AUDIT)
        YOUKNOW106_COUNT_DOMAIN_WORK(bbdShifts, 1);
#endif

        // The remaining phase is the time since this edge in clock cycles.
        // Dividing it by the increment gives exactly the Octave reference's
        // distance in numerical samples, including when several edges occur
        // during this one sample. Its complement locates the input between the
        // previous and current numerical samples.
        const double ageInSamples = increment > 0.0
            ? std::clamp(clockPhase / increment, 0.0, 1.0)
            : 0.0;
        const float atEdge = Chorus::interpolateBbdInput(
            limitedInput, previousInput, previousInput2, previousInput3,
            ageInSamples);
        // The line's own overload. The charge a cell can hold is bounded by
        // its bias window, so the wet path saturates before anything around
        // it does; driving the chorus hot grits the delayed signal only.
        const float bounded = Chorus::bbdTransfer(atEdge);

        writeIndex = writeIndex + 1 < cellPairs ? writeIndex + 1 : 0;
        const float emerging = cells[static_cast<std::size_t>(writeIndex)];
        cells[static_cast<std::size_t>(writeIndex)] = bounded;

        const float transferBefore = transferState;
        Chorus::transferLossStep(transferState, emerging);
        rememberBlepEvent(transferState - transferBefore, ageInSamples);

        // Noise remains a literal random, edge-held BBD contribution. BLEP is
        // applied later as a deterministic delta to this already-rounded held
        // value, so neither its spectrum nor the RNG sequence is predicted or
        // altered by the numerical reconstruction.
        noiseState = nextNoiseState(noiseState);
        held = transferState
             + noiseFromState(noiseState)
               * Chorus::independentLineRandomAmplitude * noiseScale;
    }
    // If the ratio somehow exceeded even that bound, drop the remainder rather
    // than carrying it: a backlog would make the line run slower than the clock
    // it was asked for and drift further out every sample.
    if (clockPhase >= 1.0)
        clockPhase -= std::floor(clockPhase);
    previousInput3 = previousInput2;
    previousInput2 = previousInput;
    previousInput = limitedInput;

    return held + static_cast<float>(deterministicBlepCorrection(increment));
}

float Chorus::Line::process(
    float limited, float clockHz, float sampleRate,
    const SupportChain::ExactTransition& outputTransition,
    float noiseScale, bool useBlep) noexcept
{
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(bbdLineFrames, 1);
#endif
    const float correctedHold = processClockedCore(
        limited, clockHz, sampleRate, noiseScale);
    const float reconstructedHold = useBlep ? correctedHold : held;

    // Sum the complementary BBD output taps through their 3.3 kOhm resistors,
    // then reconstruct the BLEP-sampled physical staircase through the two
    // output sections. The correction is deliberately upstream of every
    // hardware reconstruction pole and downstream of charge transfer/noise.
    const double exactOutput = std::isfinite(reconstructedHold)
        ? static_cast<double>(reconstructedHold) : 0.0;
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(bbdExactOutputSupportAdvances, 1);
    YOUKNOW106_COUNT_DOMAIN_WORK(bbdExactSupportCoordinateUpdates, 6);
    YOUKNOW106_COUNT_DOMAIN_WORK(bbdExactSupportMacs, 60);
#endif
    advanceExactSupport(
        exactOutputState, outputTransition,
        exactOutput, exactOutputPrevious,
        exactOutputPrevious2, exactOutputPrevious3);
    exactOutputPrevious3 = exactOutputPrevious2;
    exactOutputPrevious2 = exactOutputPrevious;
    exactOutputPrevious = exactOutput;
    return static_cast<float>(exactOutputState[4] - exactOutputState[5]);
}

void Chorus::prepareSupportRates(double hostSampleRate) noexcept
{
    constexpr std::array<int, 3> factors { 1, 2, 4 };
    const double base = std::clamp(hostSampleRate, 8000.0, 768000.0);
    for (std::size_t index = 0; index < factors.size(); ++index)
    {
        const auto rate = static_cast<float> (
            std::clamp(base * static_cast<double> (factors[index]),
                       8000.0, 768000.0));
        preparedSupportRates_[index] = rate;
        preparedSupport_[index] = supportChainFor(rate);
        ++supportBuildCount_;
    }
    supportRatesPrepared_ = true;
}

void Chorus::prepare(double sampleRate, bool preserveState) noexcept
{
    sampleRate_ = static_cast<float>(std::clamp(sampleRate, 8000.0, 768000.0));
    inverseSampleRate_ = 1.0f / sampleRate_;
    wetMuteGlide_ = 1.0f - std::exp(-inverseSampleRate_ / wetMuteTimeConstantSeconds);
    const auto cached = supportRatesPrepared_
        ? std::find(preparedSupportRates_.begin(), preparedSupportRates_.end(),
                    sampleRate_)
        : preparedSupportRates_.end();
    if (cached != preparedSupportRates_.end())
    {
        support_ = preparedSupport_[static_cast<std::size_t> (
            std::distance(preparedSupportRates_.begin(), cached))];
    }
    else
    {
        support_ = supportChainFor(sampleRate_);
        ++supportBuildCount_;
    }
    if (preserveState)
    {
        lineA_.resetAudioRateSupport();
        lineB_.resetAudioRateSupport();
        inputSupport_.reset();
    }
    else
        reset(false);
}

void Chorus::reset(bool preserveLfoPhase) noexcept
{
    const double continuingPhase = lfoPhase_;
    lineA_.reset(0x9e3779b9u);
    lineB_.reset(0x85ebca6bu);
    inputSupport_.reset();
    lfoPhase_ = preserveLfoPhase ? continuingPhase : 0.0;
    const auto runningWhileMuted = settingsFor(ChorusMode::Off);
    wetGain_ = runningWhileMuted.wetGain;
    rateHz_ = runningWhileMuted.rateHz;
    sweep_ = runningWhileMuted.sweepSeconds;
    centreDelay_ = runningWhileMuted.centreDelaySeconds;
    commonNoiseState_ = 0xd1b54a35u;
    orthogonalNoiseState_ = 0x94d049bbu;
    humPhase_ = 0.0;
    clockSpurPhaseA_ = 0.0;
    clockSpurPhaseB_ = 0.0;
    optionalSpurPhaseA_ = 0.0;
    optionalSpurPhaseB_ = 0.0;
    runningMode_ = ChorusMode::One;
    // A patch loaded with the effect switched on is not a player reaching for
    // the button: there is nothing to glide from. The first sample after a
    // reset takes the mode as it stands, and only changes made afterwards
    // glide.
    primed_ = false;
}

float Chorus::rateProportionalNoiseGain(float rateHz) noexcept
{
    const float reference = static_cast<float>(derivedRateHz(true));
    if (!(rateHz > 0.0f) || !(reference > 0.0f))
        return 1.0f;
    return rateHz / reference;
}

bool Chorus::processBypassedWhenSettled(float input, float& left,
                                        float& right) noexcept
{
    // Until the wet-mute glide has decayed to exactly zero -- or before the
    // first process() call has primed the settings -- the full path still
    // contributes audible wet, so the caller must keep running it.
    if (!primed_ || wetGain_ != 0.0f)
        return false;

    // The modulation LFO free-runs behind the switch, exactly as it does
    // through the full path with mode Off.
    lfoPhase_ += rateHz_ * inverseSampleRate_;
    if (lfoPhase_ >= 1.0f)
        lfoPhase_ -= std::floor(lfoPhase_);

    wetPathFlushPending_ = true;
    left = dryMixGain * input;
    right = dryMixGain * input;
    return true;
}

void Chorus::process(float input, ChorusMode mode, float noiseScale,
                     float& left, float& right,
                     bool enableClockBleed,
                     bool enableHyperbolicSweep,
                     float calibration,
                     bool useRateProportionalNoiseHypothesis,
                     bool enableNarrowOneTwo) noexcept
{
#if defined(YOUKNOW106_WORK_AUDIT)
    YOUKNOW106_COUNT_DOMAIN_WORK(chorusFrames, 1);
#endif
    // The settled bypass skipped the muted lines, so their content is stale
    // history from before the skip began. Rebuild the wet path from silence:
    // the established from-zero wet glide then brings the effect in exactly
    // as a patch loaded with chorus engaged comes in.
    if (wetPathFlushPending_) [[unlikely]]
    {
        wetPathFlushPending_ = false;
        lineA_.reset(0x9e3779b9u);
        lineB_.reset(0x85ebca6bu);
        inputSupport_.reset();
        clockSpurPhaseA_ = 0.0;
        clockSpurPhaseB_ = 0.0;
    }

    const auto target = settingsFor(mode);

    if (!primed_)
    {
        rateHz_ = target.rateHz;
        sweep_ = target.sweepSeconds;
        centreDelay_ = target.centreDelaySeconds;
        wetGain_ = target.wetGain;
        primed_ = true;
    }

    if (mode != ChorusMode::Off)
    {
        rateHz_ = target.rateHz;
        sweep_ = target.sweepSeconds;
        centreDelay_ = target.centreDelaySeconds;
        runningMode_ = mode;
    }
    wetGain_ += (target.wetGain - wetGain_) * wetMuteGlide_;
    // TR11/TR12 add no modelled distortion or switching artefact of their own.
    // Conducting, a 2SK30A's few hundred ohms sit against IC6's 39 kOhm wet
    // input, so it drops about 1% of the signal and sees some 30 mV across
    // itself at full level. Ohmic-region channel resistance moves by roughly
    // V_ds / 2|V_p - V_gs| -- about 0.7% -- and that reaches the output only
    // through the same 1% divider, so the distortion is on the order of
    // 0.007%, or -83 dBc. A revision modelled 1.1% (-39 dBc) instead, which is
    // some 44 dB too much, applied to every wet sample. Their switching
    // transient and leakage remain OQ-20 and are deliberately not invented;
    // the 5 ms wet-mute glide above is declared plug-in declick policy.

    lfoPhase_ += rateHz_ * inverseSampleRate_;
    if (lfoPhase_ >= 1.0f)
        lfoPhase_ -= std::floor(lfoPhase_);
    const float modulation = triangle(lfoPhase_);

    // Delay sweep trajectory. The linear-in-delay law below is the shipped
    // default, because the one trajectory measurement in existence says so:
    // a ~50-point click-timing series across the 106's modulation cycle fits
    // a straight line in delay with 16 us RMS residual and "no exponential
    // curvature" (recorded in OQ-01; below the anchoring bar, but a direct
    // measurement standing against an explicit assumption). It also renders
    // the instrument's fixed-detune character: a linear delay flank is a
    // constant pitch offset, where a bent flank slides through it.
    //
    // The hyperbolic path behind `enableHyperbolicSweep` keeps the competing
    // frequency-linear reading of Tr22's voltage-to-current converter -- the
    // clock linear in the control voltage, hence delay bending -- available
    // for the calibrated clock time-series OQ-01 still requests. When it
    // engages it bends about the clock's own endpoints, not the delay's
    // centre: an earlier centre-relative revision rendered a 38%-too-wide
    // 2.30-7.40 ms range at Unit Character 1.0 instead of the then-shipped
    // 1.66-5.35 ms, which OQ-01 records. Bending about the endpoint clocks
    // keeps both endpoints exact at every blend amount, so the two laws
    // differ only in the trajectory between them.
    float nominalDelayA = centreDelay_ + sweep_ * modulation;
    float nominalDelayB = centreDelay_ - sweep_ * modulation;

    if (enableHyperbolicSweep && calibration > 0.0f && centreDelay_ > 1.0e-5f)
    {
        const float maxDelay = centreDelay_ + sweep_;
        const float minDelay = std::max(centreDelay_ - sweep_, 1.0e-5f);
        const float clockAtMinDelay = clockForDelaySeconds(minDelay);
        const float clockAtMaxDelay = clockForDelaySeconds(maxDelay);
        const float clockMid = 0.5f * (clockAtMinDelay + clockAtMaxDelay);
        const float clockSpread = 0.5f * (clockAtMinDelay - clockAtMaxDelay);

        const float hypDelayA = delaySecondsForClock(clockMid - clockSpread * modulation);
        const float hypDelayB = delaySecondsForClock(clockMid + clockSpread * modulation);

        // The blend saturates at one: which trajectory the clock follows is a
        // topology hypothesis, not a component tolerance, so Character can
        // select it but never exaggerate it. Unclamped, the 0..2 range
        // extrapolated past the hyperbolic path -- leaving the measured
        // delay envelope and folding the sweep back mid-flank.
        const float blend = std::clamp(calibration, 0.0f, 1.0f);
        nominalDelayA += (hypDelayA - nominalDelayA) * blend;
        nominalDelayB += (hypDelayB - nominalDelayB) * blend;
    }

    const float delayA = std::max(nominalDelayA, 1.0e-4f);
    const float delayB = std::max(nominalDelayB, 1.0e-4f);
    const float clockA = std::clamp(clockForDelaySeconds(delayA),
                                    minimumClockHz, maximumClockHz);
    const float clockB = std::clamp(clockForDelaySeconds(delayB),
                                    minimumClockHz, maximumClockHz);

    const auto& wetOutputTransition = mode == ChorusMode::Off
        ? support_.exactOutputMuted
        : support_.exactOutputConnected;
    // The relative real-instrument calibration and its alternative causal
    // hypothesis act on the lines' random floor only. Neither is a claim that
    // a standalone mode-II MN3009 exceeds its datasheet row: the observation
    // was made at the completed instrument's output, and the exact physical
    // insertion point remains OQ-03. The optional common/hum/spur layers below
    // stay on the plain Chorus Noise master.
    const float modeNoiseGain = useRateProportionalNoiseHypothesis
        ? rateProportionalNoiseGain(rateHz_)
        : measuredModeNoiseGain(runningMode_);
    const float lineNoiseScale = noiseScale * modeNoiseGain;
    // One input support network for both wet branches; only the clock differs
    // between them. See `InputSupport` for why that is the model rather than
    // an optimisation of it.
    const float limitedInput = advanceInputSupport(input);
    float wetA = lineA_.process(limitedInput, clockA, sampleRate_,
                                wetOutputTransition, lineNoiseScale);
    float wetB = lineB_.process(limitedInput, clockB, sampleRate_,
                                wetOutputTransition, lineNoiseScale);

    if (enableClockBleed)
    {
        clockSpurPhaseA_ += static_cast<double>(clockA) * inverseSampleRate_;
        clockSpurPhaseB_ += static_cast<double>(clockB) * inverseSampleRate_;
        clockSpurPhaseA_ -= std::floor(clockSpurPhaseA_);
        clockSpurPhaseB_ -= std::floor(clockSpurPhaseB_);
        // Scaled by the one Chorus Noise master with no floor. A revision
        // clamped this to `max(noiseScale, 0.1f)`, which left a tenth of the
        // bleed tone alive at noiseScale 0 and broke this class's own
        // contract -- process() documents 0.0 as removing every declared
        // chorus-noise component, and the bleed is one of them.
        const float bleedScale = 0.005f * noiseScale;
        const float heterodyneBleedA = bleedScale * static_cast<float>(std::sin(2.0 * pi * clockSpurPhaseA_));
        const float heterodyneBleedB = bleedScale * static_cast<float>(std::sin(2.0 * pi * clockSpurPhaseB_));
        wetA += heterodyneBleedA;
        wetB += heterodyneBleedB;
    }

    // These mechanisms are deliberately separate from the compatibility hiss
    // above.  Their insertion point, spectra, levels and stereo correlation
    // are all voiced/unknown pending the calibrated OQ-03 capture.  Zero is
    // therefore the production default, and this branch leaves the old render
    // bit-identical when no optional component has been configured.
    const bool hasOptionalNoise = optionalNoise_.commonRandomAmplitude != 0.0f
        || optionalNoise_.humAmplitude != 0.0f
        || optionalNoise_.clockSpurAmplitude != 0.0f;
    if (hasOptionalNoise)
    {
        float optionalA = 0.0f;
        float optionalB = 0.0f;

        if (optionalNoise_.commonRandomAmplitude != 0.0f)
        {
            const auto common = correlatedRandomStep(
                commonNoiseState_, orthogonalNoiseState_,
                optionalNoise_.commonRandomCorrelation);
            optionalA += optionalNoise_.commonRandomAmplitude * common.lineA;
            optionalB += optionalNoise_.commonRandomAmplitude * common.lineB;
        }

        if (optionalNoise_.humAmplitude != 0.0f)
        {
            // A common deterministic term is the smallest useful hypothesis;
            // polarity and channel imbalance remain unknown.
            const float hum = optionalNoise_.humAmplitude
                * deterministicToneStep(humPhase_, optionalNoise_.humFrequencyHz,
                                        sampleRate_);
            optionalA += hum;
            optionalB += hum;
        }

        if (optionalNoise_.clockSpurAmplitude != 0.0f)
        {
            // Each candidate spur follows its own modulated BBD clock, on its
            // own accumulator.  The harmonic and post-line insertion level are
            // disabled hypotheses, not claims about a measured unit.
            // `deterministicToneStep` advances the phase it is handed, and the
            // heterodyne bleed above already advances clockSpurPhaseA_/B_ by
            // the same clock: sharing them made each tone run at twice its
            // intended frequency whenever both were enabled together.
            optionalA += optionalNoise_.clockSpurAmplitude
                * deterministicToneStep(
                    optionalSpurPhaseA_, clockA * optionalNoise_.clockSpurHarmonic,
                    sampleRate_);
            optionalB += optionalNoise_.clockSpurAmplitude
                * deterministicToneStep(
                    optionalSpurPhaseB_, clockB * optionalNoise_.clockSpurHarmonic,
                    sampleRate_);
        }

        wetA += optionalA * noiseScale;
        wetB += optionalB * noiseScale;
    }

    // Both ordinary modes carry dry plus one wet line per channel. I+II is a
    // live product extension rather than a tone-memory state, and its former
    // implementation simply reused that wide routing. An original-unit owner
    // remembers the physical both-button result as conspicuously narrow and
    // coloured. Equal mid folding is the only zero-parameter continuation of
    // the known two-line circuit: it preserves the exact mono sum (and thus
    // the comb colour heard in mono) while removing only the unsupported side.
    // The comparison switch retains the former wide result pending a capture.
    if (mode == ChorusMode::OneTwo && enableNarrowOneTwo)
    {
        const float wetMid = 0.5f * (wetA + wetB);
        wetA = wetMid;
        wetB = wetMid;
    }
    left = dryMixGain * (input + wetA * wetGain_);
    right = dryMixGain * (input + wetB * wetGain_);
}

} // namespace youknow106

#if defined(YOUKNOW106_WORK_AUDIT)
#undef YOUKNOW106_COUNT_DOMAIN_WORK
#endif
