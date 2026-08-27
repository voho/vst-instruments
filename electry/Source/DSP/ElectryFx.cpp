#include "DSP/ElectryFx.h"
#include "DSP/DspMath.h"

#include <algorithm>
#include <cmath>

namespace electry
{
namespace
{
constexpr float twoPi = 6.283185307179586f;
constexpr double pi = 3.14159265358979323846;

// clampf/lerp/smoothStep live in DspMath.h now, shared with ElectryEngine and
// ElectryVisuals.

float sanitiseMix(float value) noexcept
{
    if (! std::isfinite(value))
        return 0.0f;
    return clampf(value, 0.0f, 1.0f);
}

constexpr std::size_t ampModelIndex(AmpModel model) noexcept
{
    return static_cast<std::size_t>(model);
}

AmpModel sanitiseAmpModel(AmpModel model) noexcept
{
    switch (model)
    {
        case AmpModel::AmericanClean:
        case AmpModel::BritishCrunch:
        case AmpModel::ModernHighGain:
            return model;
    }
    return AmpModel::ModernHighGain;
}

// Zeroth-order modified Bessel function, used only by the halfband window
// design in prepare(). The series converges quickly for the betas a halfband
// kernel needs.
double besselI0(double x) noexcept
{
    const double halfSquare = 0.25 * x * x;
    double term = 1.0;
    double sum = 1.0;
    for (int k = 1; k < 64; ++k)
    {
        term *= halfSquare / (static_cast<double>(k) * static_cast<double>(k));
        sum += term;
        if (term < 1.0e-18 * sum)
            break;
    }
    return sum;
}

// The pedal is the antiparallel 1N4148 RC circuit measured in volts. These are
// the standard Shockley parameters used by the reference model, followed by
// the actual 2.2 kOhm / 10 nF network around the pair.
constexpr double diodeSaturationCurrent = 2.52e-9;
constexpr double diodeThermalVoltage = 1.752 * 0.0258;
constexpr double diodeResistance = 2200.0;
constexpr double diodeCapacitance = 10.0e-9;

// Dempwolf's measured Electro-Harmonix 12AX7 parameters. The quiescent point
// below is the coupled solution of a 250 V supply, 100 kOhm plate resistor and
// 1 kOhm cathode resistor: Vp + 100k Ia = 250 and Vk = 1k Ik. The transfer uses
// the resulting fixed cathode bias and is normalised by its local plate gain,
// so replacing the old curve does not change the small-signal gain structure.
constexpr double triodeG = 1.371e-3;
constexpr double triodeMu = 86.9;
constexpr double triodeGamma = 1.349;
constexpr double triodeC = 4.56;
constexpr double triodeGridG = 3.263e-4;
constexpr double triodeGridXi = 1.156;
constexpr double triodeGridC = 11.99;
constexpr double triodeGridOffset = 3.917e-8;
constexpr double triodeSupplyVoltage = 250.0;
constexpr double triodePlateResistance = 100000.0;
constexpr double triodeCathodeBias = 0.978501831350406;
constexpr double triodeQuiescentPlate = 152.15373624396778;
constexpr double triodePlateGain = 56.87748350866928;
// Normalised engine output is close to, but not literally, volts at a tube's
// grid after the input divider. This calibration keeps ordinary DI peaks off
// the hard plate rails while the drive control can still reach them.
constexpr float ampGridVolts = 0.875f;

// The phase splitters follow the component families in the Fender Twin
// Reverb AB763 and Marshall 1959 Super Lead drawings. Electry keeps its own
// documented power-tube operating points, so these are family circuits rather
// than claims of specimen-accurate named amplifiers:
// https://schematicheaven.net/fenderamps/twin_reverb_ab763_schem.pdf
// https://www.prowessamplifiers.com/schematics/Marshall/1959_superlead.pdf
// The British 400 V rail and 4.7 kOhm presence-tail value follow Fig. 11 of
// Macak and Schimmel's complete amplifier model; the Marshall drawing does not
// annotate that phase-inverter rail:
// https://www.dafx.de/paper-archive/2010/DAFx10/MacakSchimmel_DAFx10_P12.pdf
// Their ECC81/ECC83 plate equations are the measured-curve fits distributed
// in TubeLib's TriodeK macros, not the EHX 12AX7 preamp fit above:
// https://www.dos4ever.com/uTracer3/TubeLib.inc
struct PhaseInverterParameters
{
    double mu;
    double exponent;
    double plateScale;
    double kneeSoftness;
    double kneeVoltage;
    double supply;
    double plateResistanceOne;
    double plateResistanceTwo;
    double cathodeResistance;
    double tailResistance;
    double outputGridLeakResistance;
    double inputCapacitance;
    double inputGridResistance;
    double powerGridBias;
};

constexpr PhaseInverterParameters ecc81PhaseInverter {
    63.24, 1.338, 306.7, 208.5, 2363.0,
    410.0, 82000.0, 100000.0, 470.0, 22100.0,
    220000.0, 1.0e-9, 1.0e6, -37.0
};

constexpr PhaseInverterParameters ecc83PhaseInverter {
    108.93, 0.988, 389.8, 677.7, 10751.0,
    400.0, 82000.0, 100000.0, 470.0, 14700.0,
    220000.0, 22.0e-9, 1.0e6, -36.0
};

constexpr std::size_t phaseInverterDrivePoints = 769;
constexpr double minimumPhaseInverterDrive = -4.0;
constexpr double maximumPhaseInverterDrive = 4.0;

double softplus(double value) noexcept;

const PhaseInverterParameters& phaseInverterParameters(
    AmpModel model) noexcept
{
    return model == AmpModel::BritishCrunch
        ? ecc83PhaseInverter : ecc81PhaseInverter;
}

double measuredPhaseInverterPlateCurrent(
    AmpModel model, double plateToCathodeVoltage,
    double gridToCathodeVoltage) noexcept
{
    const auto& tube = phaseInverterParameters(model);
    const double plate = std::max(plateToCathodeVoltage, 0.0);
    // This checkpoint remains strictly non-grid-conducting. TubeLib's generic
    // 2 kOhm/diode grid branch needs the two output coupling-cap states to be
    // meaningful, so no TriodeK curve is extrapolated past Vgk = 0 here.
    const double grid = std::min(gridToCathodeVoltage, 0.0);
    const double drive = tube.kneeSoftness
        * (1.0 / tube.mu
           + grid / std::sqrt(tube.kneeVoltage + plate * plate));
    const double e1 = plate / tube.kneeSoftness * softplus(drive);
    return std::pow(std::max(e1, 0.0), tube.exponent) / tube.plateScale;
}

struct RawPhaseInverter
{
    double plateOne { 0.0 };
    double plateTwo { 0.0 };
    double cathode { 0.0 };
    double tail { 0.0 };
    double totalCurrent { 0.0 };
};

double solvePhaseInverterPlate(
    AmpModel model, double plateResistance, double cathodeVoltage,
    double gridToCathodeVoltage, double idlePlateVoltage,
    bool applyMidbandLoad) noexcept
{
    const auto& circuit = phaseInverterParameters(model);
    const auto residual = [&] (double plateVoltage)
    {
        double supplied = (circuit.supply - plateVoltage) / plateResistance;
        if (applyMidbandLoad)
            supplied += (idlePlateVoltage - plateVoltage)
                      / circuit.outputGridLeakResistance;
        return supplied - measuredPhaseInverterPlateCurrent(
            model, plateVoltage - cathodeVoltage,
            gridToCathodeVoltage);
    };

    double lower = std::clamp(cathodeVoltage, 0.0, circuit.supply);
    double upper = circuit.supply;
    for (int iteration = 0; iteration < 40; ++iteration)
    {
        const double middle = 0.5 * (lower + upper);
        if (residual(middle) > 0.0)
            lower = middle;
        else
            upper = middle;
    }
    return 0.5 * (lower + upper);
}

RawPhaseInverter solvePhaseInverterRaw(
    AmpModel model, double inputVoltage,
    const RawPhaseInverter* idlePoint) noexcept
{
    const auto& circuit = phaseInverterParameters(model);
    const double totalResistance = circuit.tailResistance
                                 + circuit.cathodeResistance;
    const auto evaluate = [&] (double totalCurrent)
    {
        const double cathode = totalCurrent * totalResistance;
        // The 1 MOhm grid returns establish the DC tail reference. At audio
        // frequencies the input/reference capacitors hold both grids around
        // that *idle* voltage while the cathode moves. Letting them follow the
        // live tail would destroy the defining common-mode rejection of an
        // LTP and is explicitly regression-tested below. The small AC currents
        // through the two 1 MOhm returns, and the frequency-dependent presence
        // branch, remain part of the later full coupling-network nodal solve.
        const double gridReference = idlePoint != nullptr
            ? idlePoint->tail : totalCurrent * circuit.tailResistance;
        const double gridOne = gridReference + inputVoltage;
        const double gridTwo = gridReference;
        const double idlePlateOne = idlePoint != nullptr
            ? idlePoint->plateOne : 0.0;
        const double idlePlateTwo = idlePoint != nullptr
            ? idlePoint->plateTwo : 0.0;
        const double plateOne = solvePhaseInverterPlate(
            model, circuit.plateResistanceOne, cathode,
            gridOne - cathode, idlePlateOne, idlePoint != nullptr);
        const double plateTwo = solvePhaseInverterPlate(
            model, circuit.plateResistanceTwo, cathode,
            gridTwo - cathode, idlePlateTwo, idlePoint != nullptr);
        const double currentOne = measuredPhaseInverterPlateCurrent(
            model, plateOne - cathode, gridOne - cathode);
        const double currentTwo = measuredPhaseInverterPlateCurrent(
            model, plateTwo - cathode, gridTwo - cathode);
        return RawPhaseInverter {
            plateOne, plateTwo, cathode,
            totalCurrent * circuit.tailResistance,
            currentOne + currentTwo
        };
    };

    double lower = 0.0;
    double upper = circuit.supply / totalResistance;
    RawPhaseInverter point {};
    for (int iteration = 0; iteration < 40; ++iteration)
    {
        const double middle = 0.5 * (lower + upper);
        point = evaluate(middle);
        if (middle > point.totalCurrent)
            upper = middle;
        else
            lower = middle;
    }
    const double solvedCurrent = 0.5 * (lower + upper);
    point = evaluate(solvedCurrent);
    point.totalCurrent = solvedCurrent;
    return point;
}

struct PhaseInverterCalibration
{
    RawPhaseInverter idle {};
    double inputVoltsPerDrive { 1.0 };
};

const PhaseInverterCalibration& phaseInverterCalibration(
    AmpModel model) noexcept
{
    const auto make = [] (AmpModel inverterModel)
    {
        const auto idle = solvePhaseInverterRaw(
            inverterModel, 0.0, nullptr);
        constexpr double derivativeStep = 1.0e-4;
        const auto positive = solvePhaseInverterRaw(
            inverterModel, derivativeStep, &idle);
        const auto negative = solvePhaseInverterRaw(
            inverterModel, -derivativeStep, &idle);
        const double powerGridSwing = 2.0 * std::abs(
            phaseInverterParameters(inverterModel).powerGridBias);
        const auto output = [&] (const RawPhaseInverter& point)
        {
            return ((point.plateTwo - idle.plateTwo)
                    - (point.plateOne - idle.plateOne))
                 / powerGridSwing;
        };
        const double slope = (output(positive) - output(negative))
                           / (2.0 * derivativeStep);
        return PhaseInverterCalibration {
            idle, 1.0 / std::max(slope, 1.0e-12)
        };
    };
    static const PhaseInverterCalibration ecc81 = make(
        AmpModel::AmericanClean);
    static const PhaseInverterCalibration ecc83 = make(
        AmpModel::BritishCrunch);
    return model == AmpModel::BritishCrunch ? ecc83 : ecc81;
}

// Derk Reefman's uTracer measurements, fitted by ExtractModel and distributed
// in the 2016 TubeLib SPICE library:
// https://www.dos4ever.com/uTracer3/TubeLib.inc
// The equations and fit rationale are here:
// https://www.dos4ever.com/uTracer3/Theory.pdf
// These are the BTetrodeDE 6L6GC and BTetrodeD EL34 parameter sets, not the
// older generic Koren examples. The American load line is the RCA 450 V
// plate, 400 V screen, -37 V, 5.6 kOhm plate-to-plate AB1 pair:
// https://frank.pocnet.net/sheets/049/6/6L6GC.pdf
// The British one is Mullard's 400 V, -36 V, 3.5 kOhm fixed-bias pair:
// https://frank.pocnet.net/sheets/129/e/EL34.pdf
// This isolated pair solve retains an ideal fixed screen rail and resistive
// transformer load. The nonlinear LTP above now supplies its differential
// drive; the Mullard common screen resistor, two output coupling/grid-current
// branches and reactive speaker load remain explicit boundaries.
struct PowerTubeParameters
{
    double mu;
    double exponent;
    double kneeSoftness;
    double kneeVoltage;
    double screenScale;
    double secondaryScale;
    double secondarySlope;
    double secondaryOffset;
    double secondaryGridWeight;
    double secondaryScreenRatio;
    double plateConstant;
    double plateSlope;
    double lowPlateConstant;
    double lowPlateSlope;
    double lowPlateScreenScale;
    double plateSupply;
    double screenSupply;
    double gridBias;
    double plateToPlateLoad;
    bool beamTetrode;
};

constexpr PowerTubeParameters sixL6GcParameters {
    9.41, 1.306, 45.2, 3205.1, 6672.5,
    0.031, 0.017, -5.0, 3.08, 14.66,
    0.00209, 1.1e-6, 0.00209, 0.069, 8.10,
    450.0, 400.0, -37.0, 5600.0, true
};

constexpr PowerTubeParameters el34Parameters {
    12.50, 1.363, 50.5, 1282.7, 1950.2,
    0.022, 0.033, 64.0, 2.91, 4.23,
    0.00408, 1.6e-6, 0.00408, 0.105, 6.09,
    400.0, 400.0, -36.0, 3500.0, false
};

constexpr std::size_t powerTubeDrivePoints = 4097;
constexpr std::size_t powerTubeRailPoints = 17;
// This checkpoint is strictly class AB1: neither control grid crosses zero.
// TubeLib includes a grid-current diode, but without the phase inverter's
// source impedance and coupling capacitor an ideal positive grid-voltage
// source would be an unphysical class-AB2 driver. Stop at the zero-grid AB1
// boundary; the dynamic grid circuit remains the next stage.
constexpr double minimumPowerTubeDrive = -1.0;
constexpr double maximumPowerTubeDrive = 1.0;
constexpr double minimumPowerTubeRail = 0.75;
constexpr double maximumPowerTubeRail = 1.0;

const PowerTubeParameters& powerTubeParameters(AmpModel model) noexcept
{
    return model == AmpModel::BritishCrunch
        ? el34Parameters : sixL6GcParameters;
}

double softplus(double value) noexcept
{
    return std::max(value, 0.0) + std::log1p(std::exp(-std::abs(value)));
}

double sigmoid(double value) noexcept
{
    if (value >= 0.0)
        return 1.0 / (1.0 + std::exp(-value));
    const double exponential = std::exp(value);
    return exponential / (1.0 + exponential);
}

struct PowerTubeCurrents
{
    double plate { 0.0 };
    double screen { 0.0 };
};

PowerTubeCurrents measuredPowerTubeCurrents(
    AmpModel model, double plateVoltage, double gridVoltage,
    double screenVoltage) noexcept
{
    const auto& tube = powerTubeParameters(model);
    const double plate = std::max(plateVoltage, 0.0);
    const double screen = std::max(screenVoltage, 1.0e-12);
    const double drive = tube.kneeSoftness
        * (1.0 / tube.mu
           + gridVoltage / std::sqrt(tube.kneeVoltage + screen * screen));
    const double e1 = screen / tube.kneeSoftness * softplus(drive);
    const double korenCurrent = std::pow(std::max(e1, 0.0), tube.exponent);
    const double lowPlate = tube.beamTetrode
        ? std::exp(-tube.lowPlateSlope * plate
                   * std::sqrt(tube.lowPlateSlope * plate))
        : 1.0 / (1.0 + tube.lowPlateSlope * plate);
    const double secondaryEmission = tube.secondaryScale / tube.screenScale
        * plate
        * (1.0 + std::tanh(-tube.secondarySlope
            * (plate - screen / tube.secondaryScreenRatio
               + tube.secondaryOffset
               + tube.secondaryGridWeight * gridVoltage)));

    // TubeLib's fitted BTetrodeD/DE macros subtract E3 from plate current but
    // do not add it to G2. Reefman's accompanying theory does add that term to
    // screen current; reproducing the distributed fit here is deliberate, and
    // changing to the charge-conserving variant requires a new fit/validation.
    const double plateCurrent = korenCurrent
        * (tube.plateConstant + tube.plateSlope * plate
           - tube.lowPlateConstant * lowPlate - secondaryEmission);
    const double screenCurrent = korenCurrent / tube.screenScale
        * (1.0 + tube.lowPlateScreenScale * lowPlate);
    return { std::max(plateCurrent, 0.0),
             std::max(screenCurrent, 0.0) };
}

struct RawPowerTubePair
{
    double plateSwing { 0.0 };
    double supplyCurrent { 0.0 };
};

RawPowerTubePair solvePowerTubePairRaw(
    AmpModel model, double drive, double railScale) noexcept
{
    const auto& tube = powerTubeParameters(model);
    const double rail = std::clamp(
        railScale, minimumPowerTubeRail, maximumPowerTubeRail);
    const double plateSupply = tube.plateSupply * rail;
    const double screenSupply = tube.screenSupply * rail;
    const double gridSwing = drive * -tube.gridBias;

    // An ideal centre-tapped transformer reflects Raa/4 to either half of a
    // push-pull pair. The opposed grids are an ideal phase splitter. Solving
    // v = Raa/4 * (Ia+ - Ia-) against the measured tube curves supplies real
    // AB overlap, crossover and knee behaviour without a nonlinear solve on
    // the audio thread.
    const auto residual = [&] (double swing)
    {
        const double positive = measuredPowerTubeCurrents(
            model, plateSupply - swing, tube.gridBias + gridSwing,
            screenSupply).plate;
        const double negative = measuredPowerTubeCurrents(
            model, plateSupply + swing, tube.gridBias - gridSwing,
            screenSupply).plate;
        return swing - 0.25 * tube.plateToPlateLoad
            * (positive - negative);
    };

    double lower = -plateSupply;
    double upper = plateSupply;
    for (int iteration = 0; iteration < 48; ++iteration)
    {
        const double middle = 0.5 * (lower + upper);
        if (residual(middle) > 0.0)
            upper = middle;
        else
            lower = middle;
    }
    const double swing = 0.5 * (lower + upper);
    const auto positive = measuredPowerTubeCurrents(
        model, plateSupply - swing, tube.gridBias + gridSwing,
        screenSupply);
    const auto negative = measuredPowerTubeCurrents(
        model, plateSupply + swing, tube.gridBias - gridSwing,
        screenSupply);
    return { swing, positive.plate + positive.screen
                   + negative.plate + negative.screen };
}

struct PowerTubeCalibration
{
    double plateSwingPerDrive { 1.0 };
    double nominalIdleCurrent { 0.0 };
    double demandSpan { 1.0 };
};

const PowerTubeCalibration& powerTubeCalibration(AmpModel model) noexcept
{
    const auto make = [] (AmpModel tubeModel)
    {
        constexpr double derivativeStep = 1.0e-5;
        const double slope =
            (solvePowerTubePairRaw(tubeModel, derivativeStep, 1.0).plateSwing
             - solvePowerTubePairRaw(tubeModel, -derivativeStep, 1.0).plateSwing)
            / (2.0 * derivativeStep);
        const double idle = solvePowerTubePairRaw(
            tubeModel, 0.0, 1.0).supplyCurrent;
        const double driven = solvePowerTubePairRaw(
            tubeModel, 1.0, 1.0).supplyCurrent;
        return PowerTubeCalibration {
            std::max(slope, 1.0e-12),
            idle,
            std::max(driven - idle, 1.0e-12)
        };
    };
    static const PowerTubeCalibration sixL6 = make(
        AmpModel::AmericanClean);
    static const PowerTubeCalibration el34 = make(
        AmpModel::BritishCrunch);
    return model == AmpModel::BritishCrunch ? el34 : sixL6;
}

struct PowerTubeLookupPoint
{
    float output { 0.0f };
    float supplyDemand { 0.0f };
};

using PowerTubeTable = std::array<
    PowerTubeLookupPoint, powerTubeDrivePoints * powerTubeRailPoints>;

PowerTubeTable makePowerTubeTable(AmpModel model)
{
    PowerTubeTable table {};
    const auto& calibration = powerTubeCalibration(model);
    for (std::size_t railIndex = 0;
         railIndex < powerTubeRailPoints; ++railIndex)
    {
        const double railFraction = static_cast<double>(railIndex)
            / static_cast<double>(powerTubeRailPoints - 1);
        const double rail = minimumPowerTubeRail + railFraction
            * (maximumPowerTubeRail - minimumPowerTubeRail);
        constexpr std::size_t zeroDriveIndex = powerTubeDrivePoints / 2;
        for (std::size_t driveIndex = zeroDriveIndex;
             driveIndex < powerTubeDrivePoints; ++driveIndex)
        {
            const double driveFraction = static_cast<double>(driveIndex)
                / static_cast<double>(powerTubeDrivePoints - 1);
            const double drive = minimumPowerTubeDrive + driveFraction
                * (maximumPowerTubeDrive - minimumPowerTubeDrive);
            const auto solved = solvePowerTubePairRaw(model, drive, rail);
            auto& point = table[railIndex * powerTubeDrivePoints + driveIndex];
            point.output = static_cast<float>(
                solved.plateSwing / calibration.plateSwingPerDrive);
            // Retain the signed nominal-idle delta in the table and clamp
            // only after interpolation. Clamping every knot separately moves
            // the zero-current crossing with rail interpolation.
            point.supplyDemand = static_cast<float>(
                (solved.supplyCurrent - calibration.nominalIdleCurrent)
                    / calibration.demandSpan);
            if (driveIndex == zeroDriveIndex)
            {
                point.output = 0.0f;
                continue;
            }
            // An ideal matched pair has odd output and even current demand.
            // Mirror the solved positive half exactly, halving cold prepare
            // time without approximating another part of the circuit.
            auto& mirrored = table[
                railIndex * powerTubeDrivePoints
                + (powerTubeDrivePoints - 1 - driveIndex)];
            mirrored.output = -point.output;
            mirrored.supplyDemand = point.supplyDemand;
        }
    }
    return table;
}

double cathodeCurrent(double plateVoltage,
                      double gridToCathodeVoltage) noexcept
{
    const double drive = plateVoltage / triodeMu + gridToCathodeVoltage;
    const double conduction = softplus(triodeC * drive) / triodeC;
    return triodeG * std::pow(conduction, triodeGamma);
}

double gridCurrent(double gridToCathodeVoltage) noexcept
{
    const double conduction = softplus(
        triodeGridC * gridToCathodeVoltage) / triodeGridC;
    return triodeGridG * std::pow(conduction, triodeGridXi)
         + triodeGridOffset;
}

double plateCurrentAndSlope(double plateVoltage,
                            double gridToCathodeVoltage,
                            double& plateSlope) noexcept
{
    const double drive = plateVoltage / triodeMu + gridToCathodeVoltage;
    const double conduction = softplus(triodeC * drive) / triodeC;
    const double current = triodeG * std::pow(conduction, triodeGamma);
    plateSlope = conduction > 0.0
        ? triodeG * triodeGamma
            * std::pow(conduction, triodeGamma - 1.0)
            * sigmoid(triodeC * drive) / triodeMu
        : 0.0;
    return current - gridCurrent(gridToCathodeVoltage);
}

// Output trims, measured so that a loud Drop-E riff leaves each stage at
// roughly the level it entered with.
constexpr float pedalTrim = 1.25f;
constexpr float ampTrim = 3.20f;

// The RBJ cookbook quantities every Biquad design shares: the clamped corner
// in radians per sample and its cosine and Q-scaled sine. Lowpass, highpass
// and peaking differ only in how they turn this pair into b0..a2, so factoring
// it out once removes three copies of the same clamp-omega-cosine-alpha
// arithmetic without changing a single coefficient it produces.
struct BiquadDesignBasis
{
    double cosine;
    double alpha;
};

BiquadDesignBasis designBiquadBasis(float frequencyHz, float q,
                                    float sampleRate,
                                    double minimumFrequencyHz) noexcept
{
    const double rate = std::max(static_cast<double>(sampleRate), 1.0);
    const double omega = 2.0 * pi
        * std::clamp(static_cast<double>(frequencyHz), minimumFrequencyHz,
                    0.45 * rate)
        / rate;
    const double cosine = std::cos(omega);
    const double alpha = std::sin(omega)
                       / (2.0 * std::max(static_cast<double>(q), 0.05));
    return { cosine, alpha };
}
} // namespace

// ---------------------------------------------------------------------------
// Building blocks
// ---------------------------------------------------------------------------

void ElectryFx::Biquad::setLowpass(float frequencyHz, float q,
                                   float sampleRate) noexcept
{
    const auto basis = designBiquadBasis(frequencyHz, q, sampleRate, 10.0);
    const double a0 = 1.0 + basis.alpha;
    b0 = (1.0 - basis.cosine) * 0.5 / a0;
    b1 = (1.0 - basis.cosine) / a0;
    b2 = b0;
    a1 = -2.0 * basis.cosine / a0;
    a2 = (1.0 - basis.alpha) / a0;
}

void ElectryFx::Biquad::setHighpass(float frequencyHz, float q,
                                    float sampleRate) noexcept
{
    const auto basis = designBiquadBasis(frequencyHz, q, sampleRate, 5.0);
    const double a0 = 1.0 + basis.alpha;
    b0 = (1.0 + basis.cosine) * 0.5 / a0;
    b1 = -(1.0 + basis.cosine) / a0;
    b2 = b0;
    a1 = -2.0 * basis.cosine / a0;
    a2 = (1.0 - basis.alpha) / a0;
}

void ElectryFx::Biquad::setPeaking(float frequencyHz, float q, float gainDb,
                                   float sampleRate) noexcept
{
    const auto basis = designBiquadBasis(frequencyHz, q, sampleRate, 10.0);
    const double amplitude = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const double a0 = 1.0 + basis.alpha / amplitude;
    b0 = (1.0 + basis.alpha * amplitude) / a0;
    b1 = -2.0 * basis.cosine / a0;
    b2 = (1.0 - basis.alpha * amplitude) / a0;
    a1 = -2.0 * basis.cosine / a0;
    a2 = (1.0 - basis.alpha / amplitude) / a0;
}

void ElectryFx::ToneStack::design(
    double c1, double c2, double c3,
    double r1, double r2, double r3, double r4,
    double treble, double middle, double bass,
    double sampleRate) noexcept
{
    // Yeh and Smith's closed-form passive guitar tone-stack transfer:
    // H(s) = (B1 s + B2 s^2 + B3 s^3)
    //      / (1 + A1 s + A2 s^2 + A3 s^3).
    // Keeping the component products visible makes this implementation
    // directly auditable against the circuit derivation rather than against a
    // set of fitted EQ points.
    const double analogB1 = treble * c1 * r1 + middle * c3 * r3
        + bass * (c1 * r2 + c2 * r2) + c1 * r3 + c2 * r3;
    const double analogB2 =
          treble * (c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4)
        - middle * middle * (c1 * c3 * r3 * r3 + c2 * c3 * r3 * r3)
        + middle * (c1 * c3 * r1 * r3 + c1 * c3 * r3 * r3
                    + c2 * c3 * r3 * r3)
        + bass * (c1 * c2 * r1 * r2 + c1 * c2 * r2 * r4
                  + c1 * c3 * r2 * r4)
        + bass * middle * (c1 * c3 * r2 * r3 + c2 * c3 * r2 * r3)
        + c1 * c2 * r1 * r3 + c1 * c2 * r3 * r4
        + c1 * c3 * r3 * r4;
    const double analogB3 =
          bass * middle * (c1 * c2 * c3 * r1 * r2 * r3
                           + c1 * c2 * c3 * r2 * r3 * r4)
        - middle * middle * (c1 * c2 * c3 * r1 * r3 * r3
                             + c1 * c2 * c3 * r3 * r3 * r4)
        + middle * (c1 * c2 * c3 * r1 * r3 * r3
                    + c1 * c2 * c3 * r3 * r3 * r4)
        + treble * c1 * c2 * c3 * r1 * r3 * r4
        - treble * middle * c1 * c2 * c3 * r1 * r3 * r4
        + treble * bass * c1 * c2 * c3 * r1 * r2 * r4;

    const double analogA1 = c1 * r1 + c1 * r3 + c2 * r3 + c2 * r4
        + c3 * r4 + middle * c3 * r3
        + bass * (c1 * r2 + c2 * r2);
    const double analogA2 =
          middle * (c1 * c3 * r1 * r3 - c2 * c3 * r3 * r4
                    + c1 * c3 * r3 * r3 + c2 * c3 * r3 * r3)
        + bass * middle * (c1 * c3 * r2 * r3 + c2 * c3 * r2 * r3)
        - middle * middle * (c1 * c3 * r3 * r3 + c2 * c3 * r3 * r3)
        + bass * (c1 * c2 * r2 * r4 + c1 * c2 * r1 * r2
                  + c1 * c3 * r2 * r4 + c2 * c3 * r2 * r4)
        + c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4
        + c1 * c2 * r3 * r4 + c1 * c2 * r1 * r3
        + c1 * c3 * r3 * r4 + c2 * c3 * r3 * r4;
    const double analogA3 =
          bass * middle * (c1 * c2 * c3 * r1 * r2 * r3
                           + c1 * c2 * c3 * r2 * r3 * r4)
        - middle * middle * (c1 * c2 * c3 * r1 * r3 * r3
                             + c1 * c2 * c3 * r3 * r3 * r4)
        + middle * (c1 * c2 * c3 * r3 * r3 * r4
                    + c1 * c2 * c3 * r1 * r3 * r3
                    - c1 * c2 * c3 * r1 * r3 * r4)
        + bass * c1 * c2 * c3 * r1 * r2 * r4
        + c1 * c2 * c3 * r1 * r3 * r4;

    // Bilinear transform s = c(1-z^-1)/(1+z^-1), expanded at order three.
    const double bilinear = 2.0 * std::max(sampleRate, 1.0);
    const double cSquared = bilinear * bilinear;
    const double cCubed = cSquared * bilinear;
    const double denominator0 = 1.0 + analogA1 * bilinear
        + analogA2 * cSquared + analogA3 * cCubed;
    const double inverse = 1.0 / denominator0;

    b0 = (analogB1 * bilinear + analogB2 * cSquared
          + analogB3 * cCubed) * inverse;
    b1 = (analogB1 * bilinear - analogB2 * cSquared
          - 3.0 * analogB3 * cCubed) * inverse;
    b2 = (-analogB1 * bilinear - analogB2 * cSquared
          + 3.0 * analogB3 * cCubed) * inverse;
    b3 = (-analogB1 * bilinear + analogB2 * cSquared
          - analogB3 * cCubed) * inverse;
    a1 = (3.0 + analogA1 * bilinear - analogA2 * cSquared
          - 3.0 * analogA3 * cCubed) * inverse;
    a2 = (3.0 - analogA1 * bilinear - analogA2 * cSquared
          + 3.0 * analogA3 * cCubed) * inverse;
    a3 = (1.0 - analogA1 * bilinear + analogA2 * cSquared
          - analogA3 * cCubed) * inverse;
    reset();
}

void ElectryFx::HalfbandStage::design(float kaiserBeta) noexcept
{
    // Ideal halfband impulse response h[n] = sin(pi n / 2) / (pi n), which is
    // exactly zero at every even n and alternates sign at every odd n, tapered
    // by a Kaiser window. The odd taps are then normalised so the kernel sums
    // to exactly one: unity DC gain is what keeps the oversampled detour from
    // changing the level of the signal passing through it.
    const double beta = std::max(0.0, static_cast<double>(kaiserBeta));
    const double normaliser = besselI0(beta);
    const double edge = static_cast<double>(span) + 1.0;
    double sum = 0.0;
    std::array<double, oddTapCount> taps {};
    for (int m = 1; m <= oddTapCount; ++m)
    {
        const double distance = static_cast<double>(2 * m - 1);
        const double ratio = distance / edge;
        const double window = besselI0(beta * std::sqrt(1.0 - ratio * ratio))
                            / normaliser;
        const double sign = (m % 2) == 1 ? 1.0 : -1.0;
        const double value = window * sign / (pi * distance);
        taps[static_cast<std::size_t>(m - 1)] = value;
        sum += value;
    }

    centreTap = 0.5f;
    const double scale = sum > 1.0e-12 ? 0.25 / sum : 0.0;
    for (int m = 0; m < oddTapCount; ++m)
        oddTaps[static_cast<std::size_t>(m)] = static_cast<float>(
            taps[static_cast<std::size_t>(m)] * scale);
    reset();
}

void ElectryFx::Allpass::prepare(int lengthSamples)
{
    line.assign(static_cast<std::size_t>(std::max(1, lengthSamples)), 0.0f);
    writeIndex = 0;
}

void ElectryFx::Allpass::reset() noexcept
{
    std::fill(line.begin(), line.end(), 0.0f);
    writeIndex = 0;
}

void ElectryFx::Comb::prepare(int lengthSamples)
{
    line.assign(static_cast<std::size_t>(std::max(1, lengthSamples)), 0.0f);
    writeIndex = 0;
    state = 0.0f;
}

void ElectryFx::Comb::reset() noexcept
{
    std::fill(line.begin(), line.end(), 0.0f);
    writeIndex = 0;
    state = 0.0f;
}

void ElectryFx::GainChannel::resetPedal() noexcept
{
    pedalHighpass.reset();
    pedalVoice.reset();
    pedalTilt.reset();
    diodeVoltage = 0.0;
    diodeDerivative = 0.0;
    pedalWasActive = false;
}

void ElectryFx::AmpChannel::reset() noexcept
{
    inputHighpass.reset();
    inputVoice.reset();
    interstage.reset();
    toneStack.reset();
    phaseInverterInput.reset();
    bias = 0.0f;
    sag = 0.0f;
    transformerHighpass.reset();
    flux.reset();
    negativeFeedback.reset();
    for (auto& section : cabinet)
        section.reset();
    wasActive = false;
}

void ElectryFx::GainChannel::resetAmp() noexcept
{
    for (auto& amplifier : amplifiers)
        amplifier.reset();
    ampWasActive = false;
}

void ElectryFx::GainChannel::reset() noexcept
{
    for (auto& stage : interpolators)
        stage.reset();
    for (auto& stage : decimators)
        stage.reset();
    resetPedal();
    resetAmp();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ElectryFx::prepare(double sampleRate)
{
    if (! std::isfinite(sampleRate))
        sampleRate = 48000.0;
    sampleRate_ = std::clamp(sampleRate, minimumSampleRate, maximumSampleRate);

    // 4x up to 96 kHz, 2x up to 192 kHz, native above: the point of the detour
    // is a fixed absolute bandwidth for the clipping stages, and a host that
    // already provides it does not need to pay for the halfband chain.
    oversamplingStages_ = sampleRate_ <= 96000.0
        ? 2 : (sampleRate_ <= 192000.0 ? 1 : 0);
    oversampledRate_ = static_cast<float>(
        sampleRate_ * static_cast<double>(1 << oversamplingStages_));

    // The table is generated once from the exact measured-tube load-line
    // solve below. Prime it during prepare rather than allowing first use to
    // perform thread-safe static initialisation on the audio thread.
    static_cast<void>(triodeStageLookup(0.0));
    static_cast<void>(phaseInverterLookup(
        AmpModel::AmericanClean, 0.0f));
    static_cast<void>(phaseInverterLookup(
        AmpModel::BritishCrunch, 0.0f));
    static_cast<void>(powerTubePairLookup(
        AmpModel::AmericanClean, 0.0f, 1.0f));
    static_cast<void>(powerTubePairLookup(
        AmpModel::BritishCrunch, 0.0f, 1.0f));

    // A 15 ms exponential time constant on the five panel controls, their
    // module relays and Amp Voice weights, and 12 ms on the whole gain block's
    // engagement ramp. All are per-sample: the previous
    // chain read its controls once per block and stepped, which is audible as
    // zipper noise on an automated drive.
    parameterCoefficient_ = 1.0f - std::exp(
        -1.0f / static_cast<float>(0.015 * sampleRate_));
    engagementCoefficient_ = 1.0f - std::exp(
        -1.0f / static_cast<float>(0.012 * sampleRate_));

    // 18 ms of attack, not 3. A rhythm compressor that closes inside a pick
    // attack removes the very transient that makes a palm-muted part
    // percussive; letting the attack through and holding the sustain is what
    // makes a chug hit.
    compressorAttack_ = std::exp(-1.0f / static_cast<float>(0.018 * sampleRate_));
    compressorRelease_ = std::exp(-1.0f / static_cast<float>(0.090 * sampleRate_));

    // Repeats darken and thin as they recirculate, as an analogue or tape
    // delay's do; an undamped feedback path turns a lead delay into a metallic
    // stack of identical copies.
    const auto hostRate = static_cast<float>(sampleRate_);
    delayFeedbackDamping_ = std::exp(
        -twoPi * std::min(4200.0f, 0.40f * hostRate) / hostRate);
    delayFeedbackHighpass_ = 1.0f - std::exp(-twoPi * 150.0f / hostRate);

    interstageCoefficient_[ampModelIndex(AmpModel::AmericanClean)] = std::exp(
        -twoPi * std::min(7600.0f, 0.40f * oversampledRate_) / oversampledRate_);
    interstageCoefficient_[ampModelIndex(AmpModel::BritishCrunch)] = std::exp(
        -twoPi * std::min(5600.0f, 0.40f * oversampledRate_) / oversampledRate_);
    // Keep the original modern circuit's exact coefficient expression.
    interstageCoefficient_[ampModelIndex(AmpModel::ModernHighGain)] = std::exp(
        -twoPi * std::min(6800.0f, 0.40f * oversampledRate_) / oversampledRate_);
    // The driven phase-inverter grids are coupled through the documented
    // 1 nF / 22 nF capacitors into their 1 MOhm returns. The feedback return
    // enters the other grid/tail equivalent after this signal capacitor, so
    // it is deliberately not high-passed with the preamplifier output.
    phaseInverterInputCoefficient_[ampModelIndex(AmpModel::AmericanClean)] =
        std::exp(-1.0f / static_cast<float>(
            ecc81PhaseInverter.inputCapacitance
            * ecc81PhaseInverter.inputGridResistance
            * oversampledRate_));
    phaseInverterInputCoefficient_[ampModelIndex(AmpModel::BritishCrunch)] =
        std::exp(-1.0f / static_cast<float>(
            ecc83PhaseInverter.inputCapacitance
            * ecc83PhaseInverter.inputGridResistance
            * oversampledRate_));
    phaseInverterInputCoefficient_[ampModelIndex(AmpModel::ModernHighGain)] =
        0.0f;
    // 45 ms on the grid-bias follower: long enough that a held chord shifts the
    // operating point, short enough to recover between chugs.
    biasCoefficient_ = 1.0f - std::exp(-1.0f / (0.045f * oversampledRate_));
    // The reservoir discharges far faster than it recharges, which is the
    // whole character of sag: the note blooms, ducks, and comes back.
    sagAttack_[ampModelIndex(AmpModel::AmericanClean)] =
        1.0f - std::exp(-1.0f / (0.055f * oversampledRate_));
    sagRelease_[ampModelIndex(AmpModel::AmericanClean)] =
        1.0f - std::exp(-1.0f / (0.550f * oversampledRate_));
    sagAttack_[ampModelIndex(AmpModel::BritishCrunch)] =
        1.0f - std::exp(-1.0f / (0.080f * oversampledRate_));
    sagRelease_[ampModelIndex(AmpModel::BritishCrunch)] =
        1.0f - std::exp(-1.0f / (0.300f * oversampledRate_));
    sagAttack_[ampModelIndex(AmpModel::ModernHighGain)] =
        1.0f - std::exp(-1.0f / (0.070f * oversampledRate_));
    sagRelease_[ampModelIndex(AmpModel::ModernHighGain)] =
        1.0f - std::exp(-1.0f / (0.400f * oversampledRate_));

    fluxCoefficient_[ampModelIndex(AmpModel::AmericanClean)] = std::exp(
        -twoPi * 35.0f / oversampledRate_);
    fluxCoefficient_[ampModelIndex(AmpModel::BritishCrunch)] = std::exp(
        -twoPi * 55.0f / oversampledRate_);
    fluxCoefficient_[ampModelIndex(AmpModel::ModernHighGain)] =
        transformerFluxCoefficient(oversampledRate_);

    // The feedback return is bandwidth limited by the output transformer and
    // phase compensation. Stronger, wider American feedback holds the clean
    // power stage taut; the British path releases it sooner for upper-mid bite.
    feedbackCoefficient_[ampModelIndex(AmpModel::AmericanClean)] = std::exp(
        -twoPi * std::min(6200.0f, 0.40f * oversampledRate_)
        / oversampledRate_);
    feedbackCoefficient_[ampModelIndex(AmpModel::BritishCrunch)] = std::exp(
        -twoPi * std::min(3900.0f, 0.40f * oversampledRate_)
        / oversampledRate_);
    feedbackCoefficient_[ampModelIndex(AmpModel::ModernHighGain)] = 0.0f;

    for (auto& channel : gain_)
    {
        // Beta 8.6 over six odd taps rejects the images by better than 60 dB
        // while keeping the whole four-stage chain inside eighteen host samples
        // of group delay.
        for (auto& stage : channel.interpolators)
            stage.design(8.6f);
        for (auto& stage : channel.decimators)
            stage.design(8.6f);
    }

    designFilters();

    // 360 ms of lead delay plus the right channel's spread, with headroom.
    const auto delaySize = static_cast<std::size_t>(sampleRate_ * 0.60) + 4u;
    for (auto& line : delayLines_)
        line.assign(delaySize, 0.0f);
    delayTaps_[0] = std::max(1, static_cast<int>(0.360 * sampleRate_));
    delayTaps_[1] = std::max(1, static_cast<int>(0.378 * sampleRate_));

    // Coprime diffuser and comb lengths, offset between the channels, so the
    // ambience decorrelates without modulation, a Haas delay or randomised
    // phase - the same constraint the instrument's own stereo field obeys.
    static constexpr std::array<std::array<double, 3>, 2> diffuserMs {{
        { 4.71, 8.13, 12.79 },
        { 5.23, 9.07, 13.87 },
    }};
    static constexpr std::array<std::array<double, 2>, 2> combMs {{
        { 31.73, 43.91 },
        { 34.29, 47.11 },
    }};

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto index = static_cast<std::size_t>(channel);
        for (int stage = 0; stage < 3; ++stage)
        {
            auto& diffuser = diffusers_[index][static_cast<std::size_t>(stage)];
            diffuser.prepare(static_cast<int>(
                diffuserMs[index][static_cast<std::size_t>(stage)]
                * 0.001 * sampleRate_));
            diffuser.coefficient = 0.62f;
        }
        for (int stage = 0; stage < 2; ++stage)
        {
            auto& comb = combs_[index][static_cast<std::size_t>(stage)];
            comb.prepare(static_cast<int>(
                combMs[index][static_cast<std::size_t>(stage)]
                * 0.001 * sampleRate_));
            comb.feedback = stage == 0 ? 0.74f : 0.70f;
            comb.damping = std::exp(
                -twoPi * std::min(3200.0f, 0.40f * hostRate) / hostRate);
        }
    }

    prepared_ = true;
    reset();
}

float ElectryFx::gainStageLatencySamples() const noexcept
{
    if (! isGainStageEngaged())
        return 0.0f;

    // Every interpolating stage looks ahead by `oddTapCount` of its own input
    // samples, and every decimating stage is causal over `span` of its own
    // input samples; both are referred back to the host clock here. The
    // interpolator at index n reads at 2^n times the host rate and the
    // decimator at the same index reads at twice that.
    float latency = 0.0f;
    for (int stage = 0; stage < oversamplingStages_; ++stage)
    {
        const auto interpolatorRate = static_cast<float>(1 << stage);
        latency += static_cast<float>(HalfbandStage::oddTapCount)
                 / interpolatorRate;
        latency += static_cast<float>(HalfbandStage::span)
                 / (2.0f * interpolatorRate);
    }
    return latency;
}

void ElectryFx::designFilters() noexcept
{
    const float rate = oversampledRate_;
    for (auto& channel : gain_)
    {
        // Pedal: a tight input coupling network, a mid-focused voice and a soft
        // top. A distortion pedal that passes the whole low end of a Drop-E
        // eighth string into its clipper turns the fundamental into
        // intermodulation mud instead of a note - but the corner has to sit
        // below that fundamental, not on top of it. At 120 Hz the eighth
        // string's own 41 Hz was already 20 dB down before it reached the
        // clipper, so the stage had almost no low end to generate weight from.
        channel.pedalHighpass.setHighpass(88.0f, 0.78f, rate);
        channel.pedalVoice.setPeaking(800.0f, 0.85f, 5.5f, rate);
        channel.pedalTilt.setLowpass(6800.0f, 0.70f, rate);

        auto& american = channel.amplifiers[
            ampModelIndex(AmpModel::AmericanClean)];
        auto& british = channel.amplifiers[
            ampModelIndex(AmpModel::BritishCrunch)];
        auto& modern = channel.amplifiers[
            ampModelIndex(AmpModel::ModernHighGain)];

        // Mid-1960s American component family: the fixed 6.8 kOhm mid leg and
        // 100 kOhm slope resistor feed the exact third-order passive network.
        // Its parametric open-combo-style voice stays broad and comparatively
        // even, with a restrained presence rise rather than a metal scoop; it
        // is not a loudspeaker or cabinet solve.
        american.inputHighpass.setHighpass(48.0f, 0.72f, rate);
        american.toneStack.design(
            250.0e-12, 100.0e-9, 47.0e-9,
            250.0e3, 250.0e3, 6.8e3, 100.0e3,
            0.60, 1.0, 0.130028710878, rate);
        american.transformerHighpass.setHighpass(21.0f, 0.707f, rate);
        american.cabinet[0].setHighpass(68.0f, 0.66f, rate);
        american.cabinet[1].setPeaking(112.0f, 0.85f, 2.2f, rate);
        american.cabinet[2].setPeaking(520.0f, 0.72f, -1.6f, rate);
        american.cabinet[3].setPeaking(2450.0f, 0.90f, 3.2f, rate);
        american.cabinet[4].setLowpass(4300.0f, 0.5412f, rate);
        american.cabinet[5].setLowpass(4300.0f, 1.3066f, rate);

        // Late British component family: the 33 kOhm slope resistor and
        // 500 pF treble capacitor move the passive stack's centre of gravity
        // upward. A parametric closed-stack-style voice retains the woody
        // 600-800 Hz body and rolls off sooner; it is not an individual
        // loudspeaker or cabinet solve.
        british.inputHighpass.setHighpass(60.0f, 0.78f, rate);
        british.toneStack.design(
            500.0e-12, 22.0e-9, 22.0e-9,
            250.0e3, 1.0e6, 25.0e3, 33.0e3,
            0.60, 0.65, 0.130028710878, rate);
        british.transformerHighpass.setHighpass(32.0f, 0.707f, rate);
        british.cabinet[0].setHighpass(76.0f, 0.78f, rate);
        british.cabinet[1].setPeaking(118.0f, 1.00f, 3.1f, rate);
        british.cabinet[2].setPeaking(690.0f, 0.82f, 2.5f, rate);
        british.cabinet[3].setPeaking(2250.0f, 1.00f, 4.4f, rate);
        british.cabinet[4].setLowpass(4800.0f, 0.5412f, rate);
        british.cabinet[5].setLowpass(4800.0f, 1.3066f, rate);

        // Electry's shipping modern path is intentionally left coefficient-
        // for-coefficient unchanged. It passes the Drop-E fundamental into the
        // nonlinear stages, removes boxy 430 Hz energy and adds a tight 3.1 kHz
        // presence shelf before the steep sealed-cabinet roll-off.
        modern.inputHighpass.setHighpass(52.0f, 0.80f, rate);
        modern.inputVoice.setPeaking(850.0f, 0.75f, 4.25f, rate);
        modern.transformerHighpass.setHighpass(26.0f, 0.707f, rate);
        modern.cabinet[0].setHighpass(74.0f, 0.80f, rate);
        modern.cabinet[1].setPeaking(102.0f, 1.20f, 4.35f, rate);
        modern.cabinet[2].setPeaking(430.0f, 0.85f, -6.5f, rate);
        modern.cabinet[3].setPeaking(3100.0f, 1.10f, 6.5f, rate);
        modern.cabinet[4].setLowpass(5000.0f, 0.5412f, rate);
        modern.cabinet[5].setLowpass(5000.0f, 1.3066f, rate);
    }
}

void ElectryFx::reset() noexcept
{
    for (auto& channel : gain_)
        channel.reset();
    for (auto& line : delayLines_)
        std::fill(line.begin(), line.end(), 0.0f);
    for (auto& damping : delayDamping_)
        damping.reset();
    delayHighpassState_ = {};
    delayWriteIndex_ = 0;
    for (auto& channelDiffusers : diffusers_)
        for (auto& diffuser : channelDiffusers)
            diffuser.reset();
    for (auto& channelCombs : combs_)
        for (auto& comb : channelCombs)
            comb.reset();
    compressorEnvelope_ = 0.0f;
    gainEngagement_ = 0.0f;
    distortionDrive_ = targetParameters_.distortion;
    ampDrive_ = targetParameters_.amp;
    pedalWet_ = targetParameters_.distortion > 0.0f ? 1.0f : 0.0f;
    ampWet_ = targetParameters_.amp > 0.0f ? 1.0f : 0.0f;
    ampModelWeights_.fill(0.0f);
    ampModelWeights_[ampModelIndex(targetParameters_.ampModel)] = 1.0f;
    compressorMix_ = targetParameters_.compressor;
    delayMix_ = targetParameters_.delay;
    roomMix_ = targetParameters_.room;
    updateDriveConstants();
}

void ElectryFx::setParameters(const FxParameters& parameters) noexcept
{
    targetParameters_.distortion = sanitiseMix(parameters.distortion);
    targetParameters_.amp = sanitiseMix(parameters.amp);
    targetParameters_.ampModel = sanitiseAmpModel(parameters.ampModel);
    targetParameters_.compressor = sanitiseMix(parameters.compressor);
    targetParameters_.delay = sanitiseMix(parameters.delay);
    targetParameters_.room = sanitiseMix(parameters.room);
}

float ElectryFx::transformerFluxCoefficient(float sampleRate) noexcept
{
    // The primary-inductance corner. Above it the one-pole is the voltage's
    // integral normalised - unity at DC, falling as 1/f - so the core's
    // volt-second limit becomes a level limit that falls with frequency;
    // below it a transformer cannot hold the flux at all.
    return std::exp(-twoPi * 45.0f / std::max(sampleRate, 1.0f));
}

float ElectryFx::transformerCore(OnePole& flux, float input,
                                 float coefficient) noexcept
{
    // A core saturates at a flux limit, and flux is the integral of the
    // voltage, so the limit is a volt-second one: at the same level the low
    // end reaches it long before the top does. What the core cannot carry is
    // subtracted back out, which leaves the stage transparent well above the
    // corner and compressing and thickening underneath it.
    constexpr float fluxLimit = 0.55f;
    constexpr float inverseLimitSquared = 1.0f / (fluxLimit * fluxLimit);
    const float held = flux.process(input, coefficient);
    const float carried = held
        / std::sqrt(1.0f + held * held * inverseLimitSquared);
    return input - (held - carried);
}

float ElectryFx::diodePairStep(double inputVolts, double sampleRate,
                               double& outputVolts,
                               double& previousDerivative) noexcept
{
    // C dVo/dt = (Vi - Vo) / R - 2 Is sinh(Vo / nVt). Trapezoidal
    // integration makes the capacitor's memory stable even at the lowest
    // supported host rate; four bounded Newton steps solve the diode current
    // without a table or a memoryless approximation.
    inputVolts = std::isfinite(inputVolts)
        ? std::clamp(inputVolts, -12.0, 12.0) : 0.0;
    if (! std::isfinite(outputVolts)
        || ! std::isfinite(previousDerivative))
    {
        outputVolts = 0.0;
        previousDerivative = 0.0;
    }

    const double step = 1.0 / std::max(sampleRate, 1.0);
    const double linearCoefficient = 1.0
        + 0.5 * step / (diodeResistance * diodeCapacitance);
    const double nonlinearCoefficient = step * diodeSaturationCurrent
                                      / diodeCapacitance;
    const double rightHandSide = outputVolts
        + 0.5 * step
            * (previousDerivative
               + inputVolts / (diodeResistance * diodeCapacitance));
    const auto derivative = [inputVolts] (double voltage)
    {
        return (inputVolts - voltage)
                 / (diodeResistance * diodeCapacitance)
             - (2.0 * diodeSaturationCurrent / diodeCapacitance)
                 * std::sinh(voltage / diodeThermalVoltage);
    };
    const auto residual = [=] (double voltage)
    {
        return linearCoefficient * voltage
             + nonlinearCoefficient
                 * std::sinh(voltage / diodeThermalVoltage)
             - rightHandSide;
    };

    double lower = -1.0;
    double upper = 1.0;
    // Ignoring the diode gives one root estimate; ignoring the resistor gives
    // another. The smaller magnitude is close on both sides of the
    // knee and saves Newton from first leaping across the exponential wall.
    const double linearEstimate = rightHandSide / linearCoefficient;
    const double diodeEstimate = diodeThermalVoltage * std::asinh(
        rightHandSide / nonlinearCoefficient);
    double voltage = std::copysign(
        std::min(std::abs(linearEstimate), std::abs(diodeEstimate)),
        rightHandSide);
    voltage = std::clamp(voltage, lower, upper);
    for (int iteration = 0; iteration < 4; ++iteration)
    {
        const double error = residual(voltage);
        if (std::abs(error) < 1.0e-12)
            break;
        if (error > 0.0)
            upper = voltage;
        else
            lower = voltage;

        const double slope = linearCoefficient
            + nonlinearCoefficient / diodeThermalVoltage
                * std::cosh(voltage / diodeThermalVoltage);
        double candidate = voltage - error / slope;
        if (! std::isfinite(candidate)
            || candidate <= lower || candidate >= upper)
            candidate = 0.5 * (lower + upper);
        voltage = candidate;
    }

    outputVolts = voltage;
    previousDerivative = derivative(voltage);
    return static_cast<float>(voltage);
}

double ElectryFx::triodeCathodeCurrent(
    double plateVoltage, double gridToCathodeVoltage) noexcept
{
    return cathodeCurrent(plateVoltage, gridToCathodeVoltage);
}

double ElectryFx::triodeGridCurrent(
    double gridToCathodeVoltage) noexcept
{
    return gridCurrent(gridToCathodeVoltage);
}

double ElectryFx::triodePlateCurrent(
    double plateVoltage, double gridToCathodeVoltage) noexcept
{
    double ignoredSlope = 0.0;
    return plateCurrentAndSlope(plateVoltage, gridToCathodeVoltage,
                                ignoredSlope);
}

double ElectryFx::solveTriodePlate(double gridToCathodeVoltage,
                                   double supplyVoltage,
                                   double& warmStart) noexcept
{
    gridToCathodeVoltage = std::isfinite(gridToCathodeVoltage)
        ? std::clamp(gridToCathodeVoltage, -20.0, 20.0)
        : -triodeCathodeBias;
    supplyVoltage = std::isfinite(supplyVoltage)
        ? std::clamp(supplyVoltage, 1.0, 500.0)
        : triodeSupplyVoltage;

    const auto residual = [=] (double plateVoltage, double& slope)
    {
        const double current = plateCurrentAndSlope(
            plateVoltage, gridToCathodeVoltage, slope);
        return plateVoltage + triodePlateResistance * current
             - supplyVoltage;
    };

    double lower = 0.0;
    double upper = supplyVoltage;
    double slope = 0.0;
    if (residual(lower, slope) >= 0.0)
    {
        warmStart = lower;
        return lower;
    }
    if (residual(upper, slope) <= 0.0)
    {
        warmStart = upper;
        return upper;
    }

    double plateVoltage = std::isfinite(warmStart)
        ? std::clamp(warmStart, lower, upper)
        : std::clamp(triodeQuiescentPlate, lower, upper);
    for (int iteration = 0; iteration < 8; ++iteration)
    {
        const double error = residual(plateVoltage, slope);
        if (std::abs(error) < 1.0e-9)
            break;
        if (error > 0.0)
            upper = plateVoltage;
        else
            lower = plateVoltage;

        const double jacobian = 1.0 + triodePlateResistance * slope;
        double candidate = plateVoltage - error / jacobian;
        if (! std::isfinite(candidate)
            || candidate < lower || candidate > upper
            || candidate == plateVoltage)
            candidate = 0.5 * (lower + upper);
        plateVoltage = candidate;
    }

    // A rail-to-cutoff jump can send the first Newton proposal exactly to a
    // bracket endpoint. The guarded iterations normally settle immediately;
    // this cold-path bisection supplies an actual residual guarantee for any
    // hostile warm start instead of returning the last iteration blindly.
    double finalError = residual(plateVoltage, slope);
    if (std::abs(finalError) >= 1.0e-9)
    {
        for (int iteration = 0; iteration < 48; ++iteration)
        {
            plateVoltage = 0.5 * (lower + upper);
            finalError = residual(plateVoltage, slope);
            if (std::abs(finalError) < 1.0e-9)
                break;
            if (finalError > 0.0)
                upper = plateVoltage;
            else
                lower = plateVoltage;
        }
    }

    warmStart = std::clamp(plateVoltage, 0.0, supplyVoltage);
    return warmStart;
}

float ElectryFx::triodeStage(double gridVoltage,
                             double& plateVoltage) noexcept
{
    const double solved = solveTriodePlate(
        gridVoltage - triodeCathodeBias, triodeSupplyVoltage, plateVoltage);
    return static_cast<float>(
        (triodeQuiescentPlate - solved) / triodePlateGain);
}

float ElectryFx::triodeStageLookup(double gridVoltage) noexcept
{
    // Plate loading and cathode bias are fixed, so the tube's plate solve is a
    // memoryless monotonic transfer. A dense table retains that solved circuit
    // curve while avoiding three sets of pow/exp/log operations per
    // oversampled frame. Linear interpolation over 2.44 mV grid steps is kept
    // below the independently tested transfer-error and alias floors.
    constexpr std::size_t tableSize = 16385;
    constexpr double minimumGridVoltage = -20.0 + triodeCathodeBias;
    constexpr double maximumGridVoltage = 20.0 + triodeCathodeBias;
    static const std::array<float, tableSize> transfer = []
    {
        std::array<float, tableSize> result {};
        double warmStart = triodeSupplyVoltage;
        for (std::size_t index = 0; index < result.size(); ++index)
        {
            const double fraction = static_cast<double>(index)
                                  / static_cast<double>(result.size() - 1);
            const double grid = minimumGridVoltage
                              + fraction
                                  * (maximumGridVoltage - minimumGridVoltage);
            result[index] = triodeStage(grid, warmStart);
        }
        return result;
    }();

    if (! std::isfinite(gridVoltage))
        gridVoltage = 0.0;
    const double clamped = std::clamp(
        gridVoltage, minimumGridVoltage, maximumGridVoltage);
    const double position = (clamped - minimumGridVoltage)
                          / (maximumGridVoltage - minimumGridVoltage)
                          * static_cast<double>(tableSize - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    if (lower >= tableSize - 1)
        return transfer.back();
    const float fraction = static_cast<float>(
        position - static_cast<double>(lower));
    return lerp(transfer[lower], transfer[lower + 1], fraction);
}

double ElectryFx::phaseInverterPlateCurrent(
    AmpModel model, double plateToCathodeVoltage,
    double gridToCathodeVoltage) noexcept
{
    if (model == AmpModel::ModernHighGain)
        return 0.0;
    return measuredPhaseInverterPlateCurrent(
        model, plateToCathodeVoltage, gridToCathodeVoltage);
}

ElectryFx::PhaseInverterResult ElectryFx::phaseInverterDirect(
    AmpModel model, double drive) noexcept
{
    if (model == AmpModel::ModernHighGain)
        return {};
    if (! std::isfinite(drive))
        drive = 0.0;
    const double clampedDrive = std::clamp(
        drive, minimumPhaseInverterDrive, maximumPhaseInverterDrive);
    const auto& calibration = phaseInverterCalibration(model);
    const auto point = solvePhaseInverterRaw(
        model, clampedDrive * calibration.inputVoltsPerDrive,
        &calibration.idle);
    const double powerGridSwing = 2.0 * std::abs(
        phaseInverterParameters(model).powerGridBias);
    return {
        ((point.plateTwo - calibration.idle.plateTwo)
         - (point.plateOne - calibration.idle.plateOne)) / powerGridSwing,
        point.plateOne,
        point.plateTwo,
        point.cathode,
        point.tail,
        point.totalCurrent
    };
}

float ElectryFx::phaseInverterLookup(
    AmpModel model, float drive) noexcept
{
    // The strict-AB1 LTP has no memory once its input coupling capacitor is
    // separated. Preparation retains the two loaded plate solves in a dense
    // table; unequal 82/100 kOhm arms mean it must not be mirrored as an odd
    // transfer. The following coupling/grid-current checkpoint will replace
    // the midband 220 kOhm load with two explicit dynamic branches.
    const auto make = [] (AmpModel inverterModel)
    {
        std::array<float, phaseInverterDrivePoints> result {};
        for (std::size_t index = 0; index < result.size(); ++index)
        {
            const double fraction = static_cast<double>(index)
                                  / static_cast<double>(result.size() - 1);
            const double input = minimumPhaseInverterDrive + fraction
                * (maximumPhaseInverterDrive - minimumPhaseInverterDrive);
            result[index] = static_cast<float>(
                phaseInverterDirect(inverterModel, input).output);
        }
        return result;
    };
    static const auto ecc81 = make(AmpModel::AmericanClean);
    static const auto ecc83 = make(AmpModel::BritishCrunch);
    if (model == AmpModel::ModernHighGain)
        return 0.0f;
    if (! std::isfinite(drive))
        drive = 0.0f;
    const double clamped = std::clamp(
        static_cast<double>(drive), minimumPhaseInverterDrive,
        maximumPhaseInverterDrive);
    const double position = (clamped - minimumPhaseInverterDrive)
        / (maximumPhaseInverterDrive - minimumPhaseInverterDrive)
        * static_cast<double>(phaseInverterDrivePoints - 1);
    const auto lower = std::min(
        static_cast<std::size_t>(std::floor(position)),
        phaseInverterDrivePoints - 2);
    const float fraction = static_cast<float>(
        position - static_cast<double>(lower));
    const auto& table = model == AmpModel::BritishCrunch ? ecc83 : ecc81;
    return lerp(table[lower], table[lower + 1], fraction);
}

double ElectryFx::powerTubePlateCurrent(
    AmpModel model, double plateVoltage, double gridVoltage,
    double screenVoltage) noexcept
{
    return measuredPowerTubeCurrents(
        model, plateVoltage, gridVoltage, screenVoltage).plate;
}

double ElectryFx::powerTubeScreenCurrent(
    AmpModel model, double plateVoltage, double gridVoltage,
    double screenVoltage) noexcept
{
    return measuredPowerTubeCurrents(
        model, plateVoltage, gridVoltage, screenVoltage).screen;
}

ElectryFx::PowerTubeResult ElectryFx::powerTubePairDirect(
    AmpModel model, double drive, double railScale) noexcept
{
    if (model == AmpModel::ModernHighGain)
        return {};
    if (! std::isfinite(drive))
        drive = 0.0;
    if (! std::isfinite(railScale))
        railScale = 1.0;
    const double rail = std::clamp(
        railScale, minimumPowerTubeRail, maximumPowerTubeRail);
    const double clampedDrive = std::clamp(
        drive, minimumPowerTubeDrive, maximumPowerTubeDrive);
    const auto solved = solvePowerTubePairRaw(model, clampedDrive, rail);
    const auto& calibration = powerTubeCalibration(model);
    return {
        solved.plateSwing / calibration.plateSwingPerDrive,
        std::max((solved.supplyCurrent - calibration.nominalIdleCurrent)
                     / calibration.demandSpan,
                 0.0)
    };
}

ElectryFx::PowerTubeResult ElectryFx::powerTubePairLookup(
    AmpModel model, float drive, float railScale) noexcept
{
    // The measured curves and transformer load line are memoryless for a
    // fixed rail. Two preparation-time tables retain that solve over drive and
    // supply voltage; bilinear interpolation is the only audio-thread work.
    static const PowerTubeTable sixL6 = makePowerTubeTable(
        AmpModel::AmericanClean);
    static const PowerTubeTable el34 = makePowerTubeTable(
        AmpModel::BritishCrunch);
    if (model == AmpModel::ModernHighGain)
        return {};
    if (! std::isfinite(drive))
        drive = 0.0f;
    if (! std::isfinite(railScale))
        railScale = 1.0f;
    const double clampedDrive = std::clamp(
        static_cast<double>(drive),
        minimumPowerTubeDrive, maximumPowerTubeDrive);
    const double clampedRail = std::clamp(
        static_cast<double>(railScale),
        minimumPowerTubeRail, maximumPowerTubeRail);
    const double drivePosition =
        (clampedDrive - minimumPowerTubeDrive)
        / (maximumPowerTubeDrive - minimumPowerTubeDrive)
        * static_cast<double>(powerTubeDrivePoints - 1);
    const double railPosition =
        (clampedRail - minimumPowerTubeRail)
        / (maximumPowerTubeRail - minimumPowerTubeRail)
        * static_cast<double>(powerTubeRailPoints - 1);
    const auto driveLower = std::min(
        static_cast<std::size_t>(std::floor(drivePosition)),
        powerTubeDrivePoints - 2);
    const auto railLower = std::min(
        static_cast<std::size_t>(std::floor(railPosition)),
        powerTubeRailPoints - 2);
    const float driveFraction = static_cast<float>(
        drivePosition - static_cast<double>(driveLower));
    const float railFraction = static_cast<float>(
        railPosition - static_cast<double>(railLower));
    const auto& table = model == AmpModel::BritishCrunch ? el34 : sixL6;
    const auto& lowerLeft = table[
        railLower * powerTubeDrivePoints + driveLower];
    const auto& lowerRight = table[
        railLower * powerTubeDrivePoints + driveLower + 1];
    const auto& upperLeft = table[
        (railLower + 1) * powerTubeDrivePoints + driveLower];
    const auto& upperRight = table[
        (railLower + 1) * powerTubeDrivePoints + driveLower + 1];
    const float output = lerp(
        lerp(lowerLeft.output, lowerRight.output, driveFraction),
        lerp(upperLeft.output, upperRight.output, driveFraction),
        railFraction);
    const float demand = lerp(
        lerp(lowerLeft.supplyDemand, lowerRight.supplyDemand, driveFraction),
        lerp(upperLeft.supplyDemand, upperRight.supplyDemand, driveFraction),
        railFraction);
    return { static_cast<double>(output),
             static_cast<double>(std::max(demand, 0.0f)) };
}

void ElectryFx::updateDriveConstants() noexcept
{
    // A pedal's gain range, and two amplifier stages whose drives rise
    // together. The three pairs deliberately do not collapse to different EQ
    // presets: the American path keeps its first voltage stage clean and moves
    // high-control breakup into the opposed output pair, the British path
    // shares drive between them, and the original modern path provides the
    // most cascaded preamp saturation.
    pedalDrive_ = 1.0f + 24.0f * distortionDrive_;
    ampDriveFirst_[ampModelIndex(AmpModel::AmericanClean)] =
        1.0f + 1.8f * ampDrive_;
    ampDriveSecond_[ampModelIndex(AmpModel::AmericanClean)] =
        1.0f + 20.0f * ampDrive_;
    ampDriveFirst_[ampModelIndex(AmpModel::BritishCrunch)] =
        1.0f + 4.5f * ampDrive_;
    ampDriveSecond_[ampModelIndex(AmpModel::BritishCrunch)] =
        1.0f + 7.0f * ampDrive_;
    ampDriveFirst_[ampModelIndex(AmpModel::ModernHighGain)] =
        1.0f + 7.0f * ampDrive_;
    ampDriveSecond_[ampModelIndex(AmpModel::ModernHighGain)] =
        1.0f + 11.0f * ampDrive_;
    // Each stage's trim divides its own small-signal gain back out, so the
    // control travels through tone rather than through level: a saturating
    // stage still ends up louder than the dry DI, because compressing a signal
    // raises its average, but not by the tens of decibels raw cascaded gain
    // would otherwise add.
    pedalMakeup_ = pedalTrim / pedalDrive_;
    // The American 1 nF PI input capacitor removes real Drop-E energy. Its
    // post-circuit trim retains the already-pinned 90%-drive riff level, so a
    // source-set topology correction does not turn voice selection into a
    // volume control; it does not compensate the capacitor's frequency shape.
    ampMakeup_[ampModelIndex(AmpModel::AmericanClean)] = 4.53153f
        / (ampDriveFirst_[ampModelIndex(AmpModel::AmericanClean)]
           * ampDriveSecond_[ampModelIndex(AmpModel::AmericanClean)]);
    ampMakeup_[ampModelIndex(AmpModel::BritishCrunch)] = 1.80f
        / (ampDriveFirst_[ampModelIndex(AmpModel::BritishCrunch)]
           * ampDriveSecond_[ampModelIndex(AmpModel::BritishCrunch)]);
    ampMakeup_[ampModelIndex(AmpModel::ModernHighGain)] = ampTrim
        / (ampDriveFirst_[ampModelIndex(AmpModel::ModernHighGain)]
           * ampDriveSecond_[ampModelIndex(AmpModel::ModernHighGain)]);
}

// ---------------------------------------------------------------------------
// Gain stages
// ---------------------------------------------------------------------------

float ElectryFx::pedalStage(GainChannel& channel, float input) noexcept
{
    float sample = channel.pedalHighpass.process(input);
    sample = channel.pedalVoice.process(sample);
    sample = diodePairStep(sample * pedalDrive_, oversampledRate_,
                           channel.diodeVoltage,
                           channel.diodeDerivative);
    sample = channel.pedalTilt.process(sample);
    return sample * pedalMakeup_;
}

float ElectryFx::ampStage(AmpChannel& channel, AmpModel model,
                          float input) noexcept
{
    const auto index = ampModelIndex(model);

    // This is the original shipping path kept as its own branch: same filter
    // coefficients, operation order, nonlinear transfers and constants. A
    // stable Modern selection therefore renders the same samples as it did
    // before the selector existed.
    if (model == AmpModel::ModernHighGain)
    {
        float sample = channel.inputHighpass.process(input);
        sample = channel.inputVoice.process(sample);

        channel.bias += biasCoefficient_ * (std::abs(sample) - channel.bias);
        const float bias = -0.22f - 1.10f * channel.bias;
        const float biasTriode = triodeStageLookup(bias);

        float stage = triodeStageLookup(
            sample * ampDriveFirst_[index] * ampGridVolts + bias) - biasTriode;
        stage = channel.interstage.process(stage, interstageCoefficient_[index]);

        const float droop = 1.0f
            - 0.30f * channel.sag / (0.30f + channel.sag);
        stage = droop
            * (triodeStageLookup(
                   stage * ampDriveSecond_[index] * ampGridVolts / droop + bias)
               - biasTriode);
        const float rectified = stage < 0.0f ? -stage : stage;
        channel.sag += (rectified > channel.sag
                            ? sagAttack_[index] : sagRelease_[index])
                     * (rectified - channel.sag);

        stage = channel.transformerHighpass.process(stage);
        stage = transformerCore(channel.flux, stage, fluxCoefficient_[index]);

        for (auto& section : channel.cabinet)
            stage = section.process(stage);
        return stage * ampMakeup_[index];
    }

    const bool american = model == AmpModel::AmericanClean;
    float sample = channel.inputHighpass.process(input);

    // The operating point: a standing grid bias plus the drift that grid
    // current adds under sustained level. Each transfer call interpolates the
    // dense table generated from the measured 12AX7 plate-load circuit solve;
    // subtracting the bias point keeps the stage centred instead of pumping DC
    // into the cabinet.
    channel.bias += biasCoefficient_ * (std::abs(sample) - channel.bias);
    const float bias = (american ? -0.12f : -0.18f)
        - (american ? 0.40f : 0.78f) * channel.bias;
    const float biasTriode = triodeStageLookup(bias);

    float stage = triodeStageLookup(
        sample * ampDriveFirst_[index] * ampGridVolts + bias) - biasTriode;
    // Miller capacitance between the stages: each one is progressively darker,
    // which is why a cascaded amplifier saturates smoothly instead of
    // accumulating fizz.
    stage = channel.interstage.process(stage, interstageCoefficient_[index]);

    // The passive RC stack sits where it does in the physical signal path:
    // after the preamplifier and before the driven output pair. Its insertion
    // loss is recovered by the following voltage-amplification stage (4.35x
    // for the deeper American stack and 1.95x for the British stack), not by a
    // post-cabinet EQ or a normalised response fit.
    stage = channel.toneStack.process(stage)
        * (american ? 4.35f : 1.95f);

    // The signal enters the phase inverter through its source-documented
    // coupling capacitor. A 12AT7/ECC81 drives the American 82/100 kOhm
    // plates; a 12AX7/ECC83 drives the British pair. Both solves include the
    // 470 Ohm shared cathode, finite tail and midband 220 kOhm output-grid
    // loading, so unequal halves and nonlinear balance come from the circuit.
    const float phaseDrive = stage * ampDriveSecond_[index];
    const float coupledDrive = phaseDrive
        - channel.phaseInverterInput.process(
            phaseDrive, phaseInverterInputCoefficient_[index]);

    // Output-derived negative feedback is returned around the tube pair and
    // phase splitter. Its one-pole bandwidth represents the transformer/loop
    // phase limit. The return enters after the signal coupling capacitor,
    // matching the reference-grid/tail summing approximation rather than
    // incorrectly passing feedback through the driven-grid capacitor.
    const float feedbackAmount = american ? 0.42f : 0.12f;
    const float inverterInput = coupledDrive
        - feedbackAmount * channel.negativeFeedback.state;
    const float powerInput = phaseInverterLookup(model, inverterInput);

    // Supply sag changes the plate and screen rails used by the solved load
    // line, rather than merely scaling a waveshaper. The lookup returns the
    // pair's incremental plate-plus-screen demand, so the reservoir follows
    // tube current instead of rectified audio. American uses the measured
    // 6L6GC beam-tetrode family; British uses the measured EL34 pentode family.
    // The LTP's local differential slope is normalised to unity, then one
    // output unit remains the fixed-bias voltage needed to bring the driven
    // power grid to zero. Until the two coupling branches are added, their
    // measured sub-4% common-mode component is an explicit discarded boundary.
    const float sagLimit = american ? 0.20f : 0.24f;
    const float railScale = 1.0f
        - sagLimit * channel.sag / (0.30f + channel.sag);
    const auto powerTube = powerTubePairLookup(
        model, powerInput, railScale);
    stage = static_cast<float>(powerTube.output);
    const float supplyDemand = static_cast<float>(powerTube.supplyDemand);
    channel.sag += (supplyDemand > channel.sag
                        ? sagAttack_[index] : sagRelease_[index])
                 * (supplyDemand - channel.sag);

    // Different primary-inductance corners and drive levels make the two
    // output transformers react differently to the same low note. Scaling
    // back after the core keeps the distinction in saturation, not volume.
    const float transformerDrive = american ? 0.72f : 1.08f;
    stage = channel.transformerHighpass.process(stage);
    stage = transformerCore(channel.flux, stage * transformerDrive,
                            fluxCoefficient_[index]) / transformerDrive;
    // The loop represents a transformer-secondary feedback tap before the
    // parametric speaker/cabinet voice. This filtered value becomes the next
    // oversampled frame's causal return around the output pair; it is not a
    // solved transformer/phase-inverter feedback network.
    channel.negativeFeedback.process(stage, feedbackCoefficient_[index]);

    for (auto& section : channel.cabinet)
        stage = section.process(stage);
    return stage * ampMakeup_[index];
}

float ElectryFx::blendedAmpStage(GainChannel& channel, float input) noexcept
{
    std::size_t soleModel = ampModelWeights_.size();
    int activeCount = 0;
    for (std::size_t index = 0; index < ampModelWeights_.size(); ++index)
    {
        if (ampModelWeights_[index] > 0.0f)
        {
            soleModel = index;
            ++activeCount;
        }
        else if (channel.amplifiers[index].wasActive)
        {
            channel.amplifiers[index].reset();
        }
    }

    // The steady-state fast path is also what preserves the modern model's
    // exact pre-selector arithmetic: no multiply, sum or normalisation is put
    // around its result.
    if (activeCount == 1)
    {
        auto& amplifier = channel.amplifiers[soleModel];
        amplifier.wasActive = true;
        return ampStage(amplifier, static_cast<AmpModel>(soleModel), input);
    }

    float output = 0.0f;
    float weightSum = 0.0f;
    for (std::size_t index = 0; index < ampModelWeights_.size(); ++index)
    {
        const float weight = ampModelWeights_[index];
        if (weight <= 0.0f)
            continue;
        auto& amplifier = channel.amplifiers[index];
        amplifier.wasActive = true;
        output += weight
            * ampStage(amplifier, static_cast<AmpModel>(index), input);
        weightSum += weight;
    }
    return weightSum > 0.0f ? output / weightSum : input;
}

float ElectryFx::renderGainFrame(GainChannel& channel, float input) noexcept
{
    // An exactly dry stage contributes no signal and its private state cannot
    // affect the other module. Reset once at the zero crossing so re-entry is
    // from the circuit's resting state rather than a tail frozen seconds ago;
    // mix smoothing brings that clean state in from an inaudible fraction.
    float result = input;
    if (pedalWet_ > 0.0f)
    {
        channel.pedalWasActive = true;
        result = lerp(result, pedalStage(channel, result), pedalWet_);
    }
    else if (channel.pedalWasActive)
        channel.resetPedal();
    if (ampWet_ > 0.0f)
    {
        channel.ampWasActive = true;
        result = lerp(result, blendedAmpStage(channel, result), ampWet_);
    }
    else if (channel.ampWasActive)
        channel.resetAmp();
    return result;
}

float ElectryFx::renderGainStage(GainChannel& channel, float input) noexcept
{
    if (oversamplingStages_ <= 0)
        return renderGainFrame(channel, input);

    // Every stage is stateful, so each one has to see its frames strictly in
    // time order; two four-float scratch buffers are ping-ponged rather than
    // rewriting a source frame that a later pair still needs.
    std::array<float, maximumOversampledFrames> frames {};
    std::array<float, maximumOversampledFrames> scratch {};
    frames[0] = input;
    int frameCount = 1;
    for (int stage = 0; stage < oversamplingStages_; ++stage)
    {
        auto& interpolator = channel.interpolators[static_cast<std::size_t>(stage)];
        for (int frame = 0; frame < frameCount; ++frame)
            interpolator.upsample(frames[static_cast<std::size_t>(frame)],
                                  scratch[static_cast<std::size_t>(2 * frame)],
                                  scratch[static_cast<std::size_t>(2 * frame + 1)]);
        frameCount *= 2;
        frames.swap(scratch);
    }

    for (int frame = 0; frame < frameCount; ++frame)
        frames[static_cast<std::size_t>(frame)] = renderGainFrame(
            channel, frames[static_cast<std::size_t>(frame)]);

    // The decimators are applied innermost first: the stage that halved the
    // rate last is the one that has to halve it back first.
    for (int stage = oversamplingStages_ - 1; stage >= 0; --stage)
    {
        auto& decimator = channel.decimators[static_cast<std::size_t>(stage)];
        frameCount /= 2;
        for (int frame = 0; frame < frameCount; ++frame)
            scratch[static_cast<std::size_t>(frame)] = decimator.decimate(
                frames[static_cast<std::size_t>(2 * frame)],
                frames[static_cast<std::size_t>(2 * frame + 1)]);
        frames.swap(scratch);
    }
    return frames[0];
}

// ---------------------------------------------------------------------------
// Chain
// ---------------------------------------------------------------------------

void ElectryFx::process(float* left, float* right, int numSamples) noexcept
{
    if (! prepared_ || left == nullptr || right == nullptr || numSamples <= 0)
        return;

    const int lineSize = static_cast<int>(delayLines_[0].size());
    if (lineSize <= 1)
        return;

    const auto& target = targetParameters_;
    const bool wantGain = target.distortion > 0.0f || target.amp > 0.0f;
    const float engagementTarget = wantGain ? 1.0f : 0.0f;

    // Smoothing with an exact snap at both ends: a control left at zero has to
    // reach zero rather than approach it, because that is what makes the dry
    // bypass bit-exact instead of merely quiet.
    const auto smooth = [this] (float& value, float targetValue)
    {
        value += parameterCoefficient_ * (targetValue - value);
        if (targetValue <= 0.0f && value < 1.0e-5f)
            value = 0.0f;
        else if (targetValue >= 1.0f && value > 1.0f - 1.0e-5f)
            value = 1.0f;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        smooth(distortionDrive_, target.distortion);
        smooth(ampDrive_, target.amp);
        smooth(pedalWet_, target.distortion > 0.0f ? 1.0f : 0.0f);
        smooth(ampWet_, target.amp > 0.0f ? 1.0f : 0.0f);
        const auto targetModel = ampModelIndex(target.ampModel);
        for (std::size_t model = 0; model < ampModelWeights_.size(); ++model)
        {
            smooth(ampModelWeights_[model], model == targetModel ? 1.0f : 0.0f);
            // A float recurrence toward one stalls roughly 2e-5 short when
            // its correction falls below half an ulp. The older continuous
            // controls retain their established behaviour; model weights use
            // a reachable endpoint so exactly one recursive circuit remains
            // active after a switch.
            if (model == targetModel && ampModelWeights_[model] > 0.99995f)
                ampModelWeights_[model] = 1.0f;
        }
        smooth(compressorMix_, target.compressor);
        smooth(delayMix_, target.delay);
        smooth(roomMix_, target.room);

        gainEngagement_ += engagementCoefficient_
                         * (engagementTarget - gainEngagement_);
        if (engagementTarget <= 0.0f && gainEngagement_ < 1.0e-4f)
        {
            if (gainEngagement_ != 0.0f)
            {
                // Nothing downstream can hear the block any more, so drop its
                // state: the next engagement starts from silence rather than
                // from a stale tail.
                for (auto& channel : gain_)
                    channel.reset();
            }
            gainEngagement_ = 0.0f;
        }
        else if (engagementTarget >= 1.0f && gainEngagement_ > 1.0f - 1.0e-4f)
        {
            gainEngagement_ = 1.0f;
        }

        // The delay and ambience networks are clocked whether or not their
        // controls are open, so one non-finite input sample would otherwise
        // recirculate and keep poisoning the output long after it arrived -
        // even at a mix of zero, where multiplying it by zero still yields a
        // NaN. Finite input passes through this untouched, so the dry bypass
        // stays bit-exact.
        std::array<float, 2> samples {
            std::isfinite(left[i]) ? left[i] : 0.0f,
            std::isfinite(right[i]) ? right[i] : 0.0f };

        // The oversampled gain block. With both controls at zero it is skipped
        // outright, so the chain costs nothing and adds no group delay; while
        // it engages and disengages it is crossfaded, so it cannot click.
        // The two drive smoothers and their derived constants are read only
        // inside pedalStage()/ampStage(), reached only from here, so their
        // recomputation is skipped with the block itself. pedalWet_/ampWet_
        // are engagement ramps, not parallel effect amounts: after the short
        // bypass transition every enabled amp sample has passed its cabinet.
        if (gainEngagement_ > 0.0f)
        {
            updateDriveConstants();
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto index = static_cast<std::size_t>(channel);
                const float wet = renderGainStage(gain_[index], samples[index]);
                samples[index] = lerp(samples[index], wet, gainEngagement_);
            }
        }

        // Rhythm compressor: a soft knee easing into roughly 3.5:1 above
        // -20 dBFS. The knee is what lets a palm-muted part sit still instead
        // of the level grabbing at every pick attack.
        const float detector = std::max(std::abs(samples[0]),
                                        std::abs(samples[1]));
        const float ballistic = detector > compressorEnvelope_
            ? compressorAttack_ : compressorRelease_;
        compressorEnvelope_ = ballistic * compressorEnvelope_
                            + (1.0f - ballistic) * detector;
        constexpr float threshold = 0.1f; // -20 dBFS
        constexpr float kneeWidth = 0.6f;
        float compressedGain = 1.0f;
        const float over = compressorEnvelope_ / threshold;
        if (over > 1.0f)
        {
            const float knee = smoothStep((over - 1.0f) / kneeWidth);
            compressedGain = std::pow(over, -0.72f * knee);
        }
        // The makeup returns what the knee took away at a nominal rhythm
        // level, so reaching for the control levels the part instead of just
        // turning it down.
        const float compressorGain = lerp(
            1.0f, compressedGain * (1.0f + 0.75f * compressorMix_),
            compressorMix_);
        samples[0] *= compressorGain;
        samples[1] *= compressorGain;

        // Lead delay and room. Both networks are linear and always clocked, so
        // reaching for either control never starts from a cold, silent line,
        // and both contribute exactly zero at a mix of zero.
        std::array<float, 2> delayed {};
        std::array<float, 2> ambience {};
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto index = static_cast<std::size_t>(channel);
            auto& line = delayLines_[index];
            const int tap = std::min(delayTaps_[index], lineSize - 1);
            const int readIndex = (delayWriteIndex_ - tap + lineSize) % lineSize;
            delayed[index] = line[static_cast<std::size_t>(readIndex)];

            float feedback = delayDamping_[index].process(delayed[index],
                                                          delayFeedbackDamping_);
            delayHighpassState_[index] += delayFeedbackHighpass_
                * (feedback - delayHighpassState_[index]);
            feedback -= delayHighpassState_[index];
            line[static_cast<std::size_t>(delayWriteIndex_)] =
                samples[index] + feedback * (0.20f + 0.42f * delayMix_);

            float diffused = samples[index] + 0.30f * delayed[index] * delayMix_;
            for (auto& diffuser : diffusers_[index])
                diffused = diffuser.process(diffused);
            float tail = 0.0f;
            for (auto& comb : combs_[index])
                tail += comb.process(diffused);
            ambience[index] = 0.5f * tail;
        }
        delayWriteIndex_ = (delayWriteIndex_ + 1) % lineSize;

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto index = static_cast<std::size_t>(channel);
            samples[index] += delayed[index] * 0.55f * delayMix_
                            + ambience[index] * 0.42f * roomMix_;
        }

        left[i] = clampf(samples[0], -2.0f, 2.0f);
        right[i] = clampf(samples[1], -2.0f, 2.0f);
    }

    // Backstop: the input is sanitised above and every recursion here is
    // bounded, so this should be unreachable, but a non-finite sample must
    // never be allowed to latch - the string engine recovers the same way.
    if (! std::isfinite(left[numSamples - 1])
        || ! std::isfinite(right[numSamples - 1]))
    {
        reset();
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
    }
}

} // namespace electry
