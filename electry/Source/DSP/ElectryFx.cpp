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

void ElectryFx::GainChannel::resetAmp() noexcept
{
    ampHighpass.reset();
    ampVoice.reset();
    interstage.reset();
    bias = 0.0f;
    sag = 0.0f;
    transformerHighpass.reset();
    flux.reset();
    for (auto& section : cabinet)
        section.reset();
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

    // 15 ms on the five panel controls and their module relays, and 12 ms on
    // the whole gain block's engagement ramp. All are per-sample: the previous
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

    interstageCoefficient_ = std::exp(
        -twoPi * std::min(6800.0f, 0.40f * oversampledRate_) / oversampledRate_);
    // 45 ms on the grid-bias follower: long enough that a held chord shifts the
    // operating point, short enough to recover between chugs.
    biasCoefficient_ = 1.0f - std::exp(-1.0f / (0.045f * oversampledRate_));
    // The reservoir discharges far faster than it recharges, which is the
    // whole character of sag: the note blooms, ducks, and comes back.
    sagAttack_ = 1.0f - std::exp(-1.0f / (0.070f * oversampledRate_));
    sagRelease_ = 1.0f - std::exp(-1.0f / (0.400f * oversampledRate_));
    fluxCoefficient_ = transformerFluxCoefficient(oversampledRate_);

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

        // Amp: the input stage passes the whole Drop-E fundamental, because
        // clipping it is what generates the second and third harmonics the
        // cabinet turns into a chug's weight. The mid emphasis sits above the
        // cabinet's scoop rather than inside it: pushing 560 Hz into the stage
        // and then cutting 470 Hz after it wasted gain on the one region a
        // metal rhythm tone wants out of the way.
        channel.ampHighpass.setHighpass(52.0f, 0.80f, rate);
        channel.ampVoice.setPeaking(850.0f, 0.75f, 4.25f, rate);
        // A transformer passes no DC, and this also keeps the bias drift's
        // residue out of the flux integrator in front of the core model.
        channel.transformerHighpass.setHighpass(26.0f, 0.707f, rate);

        // Cabinet. A single one-pole - the previous model - has neither the
        // thump, the mid character nor the steep top-end death of a real sealed
        // 4x12, and those three features are most of what makes a recorded
        // metal guitar recognisable as a guitar rather than as a waveshaper.
        // A 4x12's useful output starts just below the low strings' second
        // harmonic, and the thump that a palm-muted chug lives on sits in the
        // octave above that.
        channel.cabinet[0].setHighpass(74.0f, 0.80f, rate);   // no output below the box
        channel.cabinet[1].setPeaking(102.0f, 1.20f, 4.35f, rate); // cabinet thump
        channel.cabinet[2].setPeaking(430.0f, 0.85f, -6.5f, rate); // boxy honk removed
        channel.cabinet[3].setPeaking(3100.0f, 1.10f, 6.5f, rate); // presence
        // Fourth-order Butterworth roll-off: a 12-inch speaker is essentially
        // gone an octave above five kilohertz. Running it here rather than
        // after decimation removes the alias-generating content first.
        channel.cabinet[4].setLowpass(5000.0f, 0.5412f, rate);
        channel.cabinet[5].setLowpass(5000.0f, 1.3066f, rate);
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
    compressorMix_ = targetParameters_.compressor;
    delayMix_ = targetParameters_.delay;
    roomMix_ = targetParameters_.room;
    updateDriveConstants();
}

void ElectryFx::setParameters(const FxParameters& parameters) noexcept
{
    targetParameters_.distortion = sanitiseMix(parameters.distortion);
    targetParameters_.amp = sanitiseMix(parameters.amp);
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

void ElectryFx::updateDriveConstants() noexcept
{
    // A pedal's gain range, and two amplifier stages whose drives rise
    // together. The trims below hold the chain's loudness within a few
    // decibels of the dry DI across the whole travel, so auditioning the amp is
    // a change of tone rather than a jump in level.
    pedalDrive_ = 1.0f + 24.0f * distortionDrive_;
    ampDriveFirst_ = 1.0f + 7.0f * ampDrive_;
    ampDriveSecond_ = 1.0f + 11.0f * ampDrive_;
    // Each stage's trim divides its own small-signal gain back out, so the
    // control travels through tone rather than through level: a saturating
    // stage still ends up louder than the dry DI, because compressing a signal
    // raises its average, but not by the tens of decibels raw cascaded gain
    // would otherwise add.
    pedalMakeup_ = pedalTrim / pedalDrive_;
    ampMakeup_ = ampTrim / (ampDriveFirst_ * ampDriveSecond_);
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

float ElectryFx::ampStage(GainChannel& channel, float input) noexcept
{
    float sample = channel.ampHighpass.process(input);
    sample = channel.ampVoice.process(sample);

    // The operating point: a standing grid bias plus the drift that grid
    // current adds under sustained level. Each transfer call interpolates the
    // dense table generated from the measured 12AX7 plate-load circuit solve;
    // subtracting the bias point keeps the stage centred instead of pumping DC
    // into the cabinet.
    channel.bias += biasCoefficient_ * (std::abs(sample) - channel.bias);
    const float bias = -0.22f - 1.10f * channel.bias;
    const float biasTriode = triodeStageLookup(bias);

    float stage = triodeStageLookup(
        sample * ampDriveFirst_ * ampGridVolts + bias) - biasTriode;
    // Miller capacitance between the stages: each one is progressively darker,
    // which is why a cascaded amplifier saturates smoothly instead of
    // accumulating fizz.
    stage = channel.interstage.process(stage, interstageCoefficient_);

    // Supply sag. The second stage stands in for the power stage, so it is
    // where the rail droop is charged. A loud passage draws current the supply
    // cannot hold up and the plate voltage falls - real amplifiers measure
    // around 350 V down to around 250 V within a tenth of a second, recovering
    // over three to six tenths - so the follower attacks in about 70 ms and
    // recovers over about 400 ms.
    //
    // What the rail sets is the *headroom*, not the gain: `droop * triode(u /
    // droop)` leaves the small-signal slope exactly where it was and lowers
    // the ceiling in proportion. That is why a chug blooms and then ducks - the
    // follower has not caught up when the pick lands - and why a quiet passage
    // is bit-for-bit what it was without the feature, since a droop of one is
    // the identity.
    // What discharges the reservoir is the current the stage draws, which
    // follows its *output* rather than its grid signal - so the follower reads
    // the stage's own last sample, not the drive. Bounded at 30%, which is the
    // roughly 350 V to 250 V a real supply measures.
    const float droop = 1.0f - 0.30f * channel.sag / (0.30f + channel.sag);
    stage = droop
        * (triodeStageLookup(
               stage * ampDriveSecond_ * ampGridVolts / droop + bias)
           - biasTriode);
    const float rectified = stage < 0.0f ? -stage : stage;
    channel.sag += (rectified > channel.sag ? sagAttack_ : sagRelease_)
                 * (rectified - channel.sag);

    // The output transformer. Its core saturates at a flux limit, and flux is
    // the integral of the voltage, so the limit is a volt-second one: at the
    // same level the low end reaches it long before the top does. The one-pole
    // is that integral normalised - unity at DC, falling as 1/f above the
    // primary-inductance corner - and the excess the core cannot carry is
    // subtracted back out, which leaves the stage transparent well above the
    // corner and compressing and thickening underneath it. The high-pass in
    // front is the transformer's own inability to pass DC.
    stage = channel.transformerHighpass.process(stage);
    stage = transformerCore(channel.flux, stage, fluxCoefficient_);

    for (auto& section : channel.cabinet)
        stage = section.process(stage);
    return stage * ampMakeup_;
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
        result = lerp(result, ampStage(channel, result), ampWet_);
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
