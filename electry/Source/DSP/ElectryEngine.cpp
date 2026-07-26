#include "ElectryEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace electry
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 6.28318530717958647692f;

// The playable model spans a conventional 25.5-inch electric scale into a
// 28-inch baritone/8-string scale. The longer endpoint keeps Drop-E tension
// and partial definition credible instead of merely pitching a short guitar
// down. Pickup distances remain geometric reference estimates.
constexpr float conventionalScaleMetres = 0.6477f;
constexpr float baritoneScaleMetres = 0.7112f;
constexpr float lesPaulBridgePickupMetres = 0.043f;
constexpr float telecasterBridgePickupMetres = 0.028f;
constexpr float lesPaulNeckPickupMetres = 0.155f;
constexpr float telecasterNeckPickupMetres = 0.163f;

// Effective magnetic aperture windows: a humbucker averages string motion
// across two coils, a narrow single coil across a much shorter window.
constexpr float humbuckerApertureMetres = 0.0210f;
constexpr float singleCoilApertureMetres = 0.0048f;

// Weight on the delayed tap of the pickup position comb.
//
// Subtracting two taps of equal weight puts an exact zero at DC and an
// infinitely deep null at every multiple of c/2x, which is what a point sensor
// on an ideal one-dimensional string would do. A real pickup does not: it sees
// the string through a finite aperture, a humbucker sums two coils at two
// different distances from the bridge, and the field is three-dimensional, so
// the two contributions never cancel exactly. Measured pickup responses notch
// by something like 6 to 15 dB rather than vanishing.
//
// A weight of b makes the null (1-b)/(1+b) deep - 12 dB here - and leaves a
// finite response at low frequencies, where perfect cancellation was costing
// the fundamental most. Against the dry reference recordings this recovered
// 4.7 dB in the 60-85 Hz band on an open low E and 5.4 dB on a muted power
// chord, which is the hollow, bodyless low register the references do not
// have. Explicitly modelling the humbucker's two coils was tried instead and
// measured no better than this, at two extra fractional reads per string.
constexpr float pickupCombDepth = 0.60f;

// Loaded electrical resonance of the two anchor pickup circuits (coil
// inductance and capacitance with typical pot and cable loading).
constexpr float humbuckerResonanceHz = 2000.0f;
constexpr float singleCoilResonanceHz = 6000.0f;
constexpr float humbuckerResonanceQ = 1.0f;
constexpr float singleCoilResonanceQ = 2.40f;

// The bridge hand's loss slope: how deep the shelf goes and where it turns over
// as a multiple of the sounding fundamental. Fitted jointly with the relief
// below against nine dry muted power-chord references at five pitches, on an
// objective that scores the harmonic-number tilt and the peak-relative energy
// contour together so neither can be bought with the other.
// Swept jointly: depth 0, 0.10, 0.20, 0.35, 0.50 against corners of 2, 4, 6, 8,
// 11 and 15 times the fundamental. The minimum is clear and it is not a trade -
// tilt error falls 17.69 to 11.50 dB while the contour term rises only 4.00 to
// 5.45, where the same shelf outside the solve moved the two one for one and
// bought nothing. Corners nearer the fundamental collapse the contour (at 2x the
// term reaches 34 dB); much above eight the tilt returns to where it started.
constexpr float handLossDepthScale = 0.35f;
constexpr float handLossCornerRatio = 8.0f;

constexpr float steelDensity = 7850.0f;      // kg/m^3
constexpr float steelYoungModulus = 2.0e11f; // Pa

constexpr float clampf(float value, float low, float high) noexcept
{
    return value < low ? low : (value > high ? high : value);
}

bool finitef(float value) noexcept { return std::isfinite(value); }

float rateAdjustedCoefficient(float coefficientAt48k, float sampleRate) noexcept
{
    coefficientAt48k = clampf(coefficientAt48k, 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - coefficientAt48k, 48000.0f / sampleRate);
}
} // namespace

// ---------------------------------------------------------------------------
// Small filter blocks
// ---------------------------------------------------------------------------

void ElectryEngine::Biquad::setResonantLowpass(float frequencyHz, float q,
                                               float sampleRate) noexcept
{
    frequencyHz = clampf(frequencyHz, 40.0f, 0.45f * sampleRate);
    q = clampf(q, 0.4f, 6.0f);
    const float omega = twoPi * frequencyHz / sampleRate;
    const float sinOmega = std::sin(omega);
    const float cosOmega = std::cos(omega);
    const float alpha = sinOmega / (2.0f * q);
    const float a0 = 1.0f + alpha;
    const float inverseA0 = 1.0f / a0;
    b0 = 0.5f * (1.0f - cosOmega) * inverseA0;
    b1 = (1.0f - cosOmega) * inverseA0;
    b2 = b0;
    a1 = -2.0f * cosOmega * inverseA0;
    a2 = (1.0f - alpha) * inverseA0;
}

void ElectryEngine::ModalResonator::configure(float frequencyHz, float q,
                                              float modeGain,
                                              float sampleRate) noexcept
{
    frequencyHz = clampf(frequencyHz, 30.0f, 0.4f * sampleRate);
    q = clampf(q, 2.0f, 60.0f);
    const double omega = static_cast<double>(twoPi)
                       * static_cast<double>(frequencyHz)
                       / static_cast<double>(sampleRate);
    const double radius = std::exp(
        -omega / (2.0 * static_cast<double>(q)));
    a1 = -2.0f * radius * std::cos(omega);
    a2 = radius * radius;
    // Exact peak normalisation at the configured modal frequency.  The old
    // `(1-r^2)` numerator grows approximately as 1/sin(omega), so a requested
    // 100 Hz mode at the oversampled clock was amplified by roughly 150x and
    // an E1 sympathetic mode by more than 350x.  That produced sparse,
    // keyboard-like bells rather than a controlled structural response.
    // At the configured frequency the denominator factors into a stable
    // product that avoids subtracting three nearly equal terms at E1 and
    // high sample rates.
    const double oneMinusRadius = 1.0 - radius;
    const double denominatorMagnitude = oneMinusRadius * std::hypot(
        oneMinusRadius, 2.0 * std::sqrt(radius) * std::sin(omega));
    gain = denominatorMagnitude * static_cast<double>(modeGain);
}

void ElectryEngine::HalfbandDecimator::push(float input) noexcept
{
    history[static_cast<std::size_t>(writeIndex)] = input;
    writeIndex = (writeIndex + 1) & (decimatorHistorySize - 1);
}

float ElectryEngine::HalfbandDecimator::output() const noexcept
{
    // 63-tap Blackman-windowed halfband low-pass. At half the internal sample
    // rate every second side coefficient is mathematically zero; retaining
    // only the 15 non-zero symmetric pairs and the centre tap gives the exact
    // 31-non-zero-tap FIR response with 16 multiplies. Rejection exceeds 75 dB by
    // 0.30 * internal Fs, while the -6 dB transition centre is host Nyquist.
    static constexpr std::array<float, 16> sideTaps {
        0.0f,
        0.0000411789433614f,
       -0.000184365757173f,
        0.000476226242826f,
       -0.000989039317420f,
        0.00182325680003f,
       -0.00311016959510f,
        0.00501722155648f,
       -0.00776114313623f,
        0.0116398290043f,
       -0.0171085471172f,
        0.0249696847801f,
       -0.0369009266890f,
        0.0572633729276f,
       -0.102148960367f,
        0.316972234517f,
    };
    static constexpr float centreTap = 0.500000294415f;
    static constexpr int historyMask = decimatorHistorySize - 1;

    // writeIndex points one slot past the newest internal sample.
    const int newest = (writeIndex - 1) & historyMask;
    float sum = centreTap
              * history[static_cast<std::size_t>((newest - 31) & historyMask)];
    for (int pair = 1; pair < static_cast<int>(sideTaps.size()); ++pair)
    {
        const int nearOffset = 2 * pair;
        const int farOffset = 62 - nearOffset;
        sum += sideTaps[static_cast<std::size_t>(pair)]
             * (history[static_cast<std::size_t>((newest - nearOffset) & historyMask)]
                + history[static_cast<std::size_t>((newest - farOffset) & historyMask)]);
    }
    return sum;
}

// ---------------------------------------------------------------------------
// Polarisation loop
// ---------------------------------------------------------------------------

void ElectryEngine::PolarisationLoop::clear() noexcept
{
    line.fill(0.0f);
    writeIndex = 0;
    damping.reset();
    handLoss.reset();
    dispersion1.reset();
    dispersion2.reset();
    dispersion3.reset();
    dispersion4.reset();
    dispersion5.reset();
    dispersion6.reset();
    dispersion7.reset();
    dispersion8.reset();
}

void ElectryEngine::PolarisationLoop::writeAdd(float offsetSamples, float value) noexcept
{
    offsetSamples = clampf(offsetSamples, 1.0f, static_cast<float>(delayLineSize - 8));
    const float position = static_cast<float>(writeIndex) - offsetSamples;
    const int index = static_cast<int>(std::floor(position));
    const float fraction = position - static_cast<float>(index);
    const int mask = delayLineSize - 1;
    line[static_cast<std::size_t>(index & mask)] += value * (1.0f - fraction);
    line[static_cast<std::size_t>((index + 1) & mask)] += value * fraction;
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

const std::array<ElectryEngine::StringSpec, ElectryEngine::stringCount>&
ElectryEngine::stringSpecs() noexcept
{
    // Drop-E eight-string tuning with a light .009-.080 reference set. The
    // gauge parameter scales it toward a heavy .011-.098 set. `constexpr`
    // rather than `const`: a runtime-initialised function-local static costs a
    // guard-variable check on every call, and this table is read from the
    // per-sample render path.
    // The fundamental T60 targets follow a dry electric low-E reference
    // recording rather than a round number: its overall level falls about
    // 24 dB over the eight seconds after the attack, and its fundamental
    // partial decays more slowly still. A solid-body electric's low strings
    // ring for tens of seconds, and the earlier ten-second-scale targets were
    // what left the low register sounding hollow - the fundamental faded while
    // the upper partials were still going.
    static constexpr std::array<StringSpec, stringCount> specs {{
        // Effective bending cores are smaller than the geometric core: the
        // wrap slips under flexure instead of behaving like a solid rod.
        { 28, true, 2.0320f, 0.22f, 20.0f }, // E1, wound (.080)
        { 35, true, 1.5240f, 0.25f, 19.0f }, // B1, wound (.060)
        { 40, true, 1.0668f, 0.28f, 18.0f }, // E2, wound
        { 45, true, 0.8128f, 0.30f, 16.5f }, // A2, wound
        { 50, true, 0.6096f, 0.32f, 15.0f }, // D3, wound
        { 55, false, 0.4064f, 1.0f, 12.0f }, // G3, plain
        { 59, false, 0.2794f, 1.0f, 10.0f }, // B3, plain
        { 64, false, 0.2286f, 1.0f, 8.5f },  // E4, plain
    }};
    return specs;
}

EngineParameters ElectryEngine::sanitise(const EngineParameters& parameters) noexcept
{
    EngineParameters result = parameters;
    const auto clampUnit = [] (float value, float fallback)
    {
        if (! std::isfinite(value))
            return fallback;
        return clampf(value, 0.0f, 1.0f);
    };

    const EngineParameters defaults;
    result.bodyWood = clampUnit(parameters.bodyWood, defaults.bodyWood);
    result.bodySize = clampUnit(parameters.bodySize, defaults.bodySize);
    result.bodyShape = clampUnit(parameters.bodyShape, defaults.bodyShape);
    result.construction = clampUnit(parameters.construction, defaults.construction);
    result.scaleLength = clampUnit(parameters.scaleLength, defaults.scaleLength);
    result.pickupType = clampUnit(parameters.pickupType, defaults.pickupType);
    result.toneKnob = clampUnit(parameters.toneKnob, defaults.toneKnob);
    result.bodyResonance = clampUnit(parameters.bodyResonance, defaults.bodyResonance);
    result.stringGauge = clampUnit(parameters.stringGauge, defaults.stringGauge);
    result.stringAge = clampUnit(parameters.stringAge, defaults.stringAge);
    result.pickPosition = clampUnit(parameters.pickPosition, defaults.pickPosition);
    result.pickHardness = clampUnit(parameters.pickHardness, defaults.pickHardness);
    result.pickNoise = clampUnit(parameters.pickNoise, defaults.pickNoise);
    result.fingerNoise = clampUnit(parameters.fingerNoise, defaults.fingerNoise);
    result.releaseNoise = clampUnit(parameters.releaseNoise, defaults.releaseNoise);
    result.muteDamping = clampUnit(parameters.muteDamping, defaults.muteDamping);
    result.velocityAmount = clampUnit(parameters.velocityAmount, defaults.velocityAmount);
    result.artifactAmount = clampUnit(parameters.artifactAmount, defaults.artifactAmount);
    result.sympatheticAmount = clampUnit(parameters.sympatheticAmount,
                                         defaults.sympatheticAmount);
    result.palmMute = clampUnit(parameters.palmMute, defaults.palmMute);

    if (! std::isfinite(parameters.strumSpreadSeconds))
        result.strumSpreadSeconds = defaults.strumSpreadSeconds;
    else
        result.strumSpreadSeconds = clampf(parameters.strumSpreadSeconds,
                                           0.0f, 0.040f);

    if (! std::isfinite(parameters.vibratoDepthCents))
        result.vibratoDepthCents = defaults.vibratoDepthCents;
    else
        result.vibratoDepthCents = clampf(parameters.vibratoDepthCents,
                                          0.0f, 100.0f);

    if (! std::isfinite(parameters.bendTimeSeconds))
        result.bendTimeSeconds = defaults.bendTimeSeconds;
    else
        result.bendTimeSeconds = clampf(parameters.bendTimeSeconds, 0.04f, 2.0f);

    if (! std::isfinite(parameters.outputGain))
        result.outputGain = defaults.outputGain;
    else
        result.outputGain = clampf(parameters.outputGain, 0.0f, 2.0f);

    const int selector = static_cast<int>(parameters.pickupSelector);
    result.pickupSelector = selector < 0 || selector > 2
        ? defaults.pickupSelector
        : parameters.pickupSelector;
    const int outputMode = static_cast<int>(parameters.outputMode);
    result.outputMode = outputMode < 0 || outputMode > 1
        ? defaults.outputMode
        : parameters.outputMode;
    return result;
}

float ElectryEngine::midiToHz(float midiNote) noexcept
{
    return 440.0f * std::exp2((midiNote - 69.0f) / 12.0f);
}

std::uint32_t ElectryEngine::hash32(std::uint32_t value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float ElectryEngine::stringFluxScale(int stringIndex) noexcept
{
    // Pole-piece balance and ferromagnetic string mass keep a real guitar's
    // thick low strings from losing another full factor of frequency at the
    // pickup. The shallow diameter-derived term deliberately stops far short
    // of geometric area scaling (which would overstate an .080 string by tens
    // of times), while preserving one stable balance for every fret on a given
    // string. It depends only on the string, so it is solved once instead of
    // inside the sample loop, where it used to cost a std::pow per string per
    // rendered sample.
    constexpr float highEStringDiameterMm = 0.2286f;
    const auto index = static_cast<std::size_t>(
        std::clamp(stringIndex, 0, stringCount - 1));
    const float magneticMassBalance = std::pow(
        stringSpecs()[index].plainDiameterMm / highEStringDiameterMm, 0.30f);
    return lerp(1.55f, 0.72f,
                static_cast<float>(index) / static_cast<float>(stringCount - 1))
         * magneticMassBalance;
}

float ElectryEngine::onePolePhaseDelay(float coefficient, float omega) noexcept
{
    // H(z) = (1 - a) / (1 - a z^-1)
    const float angle = -std::atan2(coefficient * std::sin(omega),
                                    1.0f - coefficient * std::cos(omega));
    return omega > 1.0e-9f ? -angle / omega : coefficient / (1.0f - coefficient);
}

// Magnitude and phase of the hand's loss shelf, H(z) = (1-d) + d(1-a)/(1-a/z).
// Used by both the decay solve and the tuning compensation, so the two can never
// disagree about what is in the loop.
void ElectryEngine::handLossResponse(float depth, float coefficient, float omega,
                                     float& magnitude, float& phase) noexcept
{
    if (depth <= 0.0f)
    {
        magnitude = 1.0f;
        phase = 0.0f;
        return;
    }
    const float cw = std::cos(omega), sw = std::sin(omega);
    const float nr = 1.0f - depth * coefficient
                   - (1.0f - depth) * coefficient * cw;
    const float ni = (1.0f - depth) * coefficient * sw;
    const float dr = 1.0f - coefficient * cw;
    const float di = coefficient * sw;
    const float dn = dr * dr + di * di;
    if (dn < 1.0e-20f)
    {
        magnitude = 1.0f;
        phase = 0.0f;
        return;
    }
    magnitude = std::sqrt((nr * nr + ni * ni) / dn);
    phase = std::atan2(ni, nr) - std::atan2(di, dr);
}

float ElectryEngine::allpassPhaseDelay(float coefficient, float omega) noexcept
{
    // H(z) = (c + z^-1) / (1 + c z^-1)
    const float numeratorAngle = std::atan2(-std::sin(omega),
                                            coefficient + std::cos(omega));
    const float denominatorAngle = std::atan2(-coefficient * std::sin(omega),
                                              1.0f + coefficient * std::cos(omega));
    const float angle = numeratorAngle - denominatorAngle;
    if (omega <= 1.0e-9f)
        return (1.0f - coefficient) / (1.0f + coefficient);
    return -angle / omega;
}

ElectryEngine::VelocityProfile
ElectryEngine::makeVelocityProfile(float velocity) const noexcept
{
    const float v = clampf(velocity, 0.0f, 1.0f);
    const float response = smoothedParameters_.velocityAmount;
    const float curve = std::pow(v, 1.35f);

    VelocityProfile profile;
    profile.amplitude = lerp(1.0f, 0.06f + 0.94f * curve, response);
    profile.effort = lerp(0.65f, v, response);
    profile.effortCurve = smoothStep(profile.effort);
    profile.brightness = lerp(0.20f, 2.10f, profile.effortCurve);
    profile.noise = lerp(1.0f, 0.18f + 0.82f * std::sqrt(v), response);
    profile.tension = lerp(0.55f, 1.25f, profile.effortCurve);
    profile.collision = smoothStep(
        clampf((profile.effort - 0.25f) / 0.75f, 0.0f, 1.0f));
    return profile;
}

// ---------------------------------------------------------------------------
// Engine lifecycle
// ---------------------------------------------------------------------------

ElectryEngine::ElectryEngine() noexcept
{
    smoothedParameters_ = sanitise(targetParameters_);
    for (int stringIndex = 0; stringIndex < stringCount; ++stringIndex)
        voices_[static_cast<std::size_t>(stringIndex)].stringIndex = stringIndex;
}

void ElectryEngine::prepare(double sampleRate, int maxBlockSize)
{
    (void) maxBlockSize;
    if (! std::isfinite(sampleRate))
        sampleRate = 48000.0;
    hostSampleRate_ = std::clamp(sampleRate, minimumSupportedSampleRate,
                                 maximumSupportedSampleRate);
    // The nonlinear pickup/string/body path benefits most at conventional
    // rates. At high-rate hosts the native clock already provides at least
    // the same bandwidth, so avoid needlessly exceeding the delay-line and
    // CPU contracts.
    oversamplingFactor_ = hostSampleRate_ <= 96000.0 ? 2 : 1;
    sampleRate_ = hostSampleRate_ * static_cast<double>(oversamplingFactor_);
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);

    // 14 ms continuous-parameter smoothing and a 4 ms pickup selector fade,
    // both advanced at the control tick.
    const float controlRate = static_cast<float>(sampleRate_)
                            / static_cast<float>(controlPeriod);
    parameterSmoothingCoefficient_ =
        1.0f - std::exp(-1.0f / (0.014f * controlRate));
    pickupMixCoefficient_ = 1.0f - std::exp(-1.0f / (0.004f * controlRate));
    contactNoiseBandCoefficient_ = rateAdjustedCoefficient(
        0.08f, static_cast<float>(sampleRate_));

    // The output DC blocker corner stays at 5 Hz regardless of rate. Inside
    // the string loops there is deliberately no DC filter: a fixed-corner
    // blocker's steep phase lead near a low fundamental would detune the
    // upper partials against the compensated fundamental. The pickup position
    // comb no longer rejects DC exactly - `pickupCombDepth` is deliberately
    // below one - so this output stage is now the only thing removing any
    // offset the comb passes, rather than a second line of defence.
    outputDcCoefficient_ = std::exp(-twoPi * 5.0f * inverseSampleRate_);
    bodyEmfLowpassCoefficient_ = std::exp(
        -twoPi * std::min(4000.0f, 0.30f * static_cast<float>(sampleRate_))
        * inverseSampleRate_);

    // Every rate-derived smoothing constant below used to be recomputed with
    // std::pow inside renderVoice(), twice per string per sample. They depend
    // only on the internal clock, so they belong here.
    const float internalRate = static_cast<float>(sampleRate_);
    energyAttackCoefficient_ = rateAdjustedCoefficient(0.004f, internalRate);
    energyReleaseCoefficient_ = rateAdjustedCoefficient(0.00006f, internalRate);
    retireAttackCoefficient_ = rateAdjustedCoefficient(0.01f, internalRate);
    retireReleaseCoefficient_ = rateAdjustedCoefficient(0.0009f, internalRate);
    artifactBandCoefficient_ = rateAdjustedCoefficient(0.12f, internalRate);
    pitchBendCoefficient_ = rateAdjustedCoefficient(0.35f, internalRate);
    vibratoCoefficient_ = rateAdjustedCoefficient(0.12f, internalRate);
    // A 60 ms window on the sympathetic ring's own energy follower: long
    // enough to survive a bar of ringing, short enough to retire a coupled
    // string promptly once it is inaudible.
    sympatheticEnergyCoefficient_ = 1.0f - std::exp(-1.0f / (0.060f * internalRate));

    const float controlTickRate = internalRate / static_cast<float>(controlPeriod);
    displayLevelAttack_ = 1.0f - std::exp(-1.0f / (0.010f * controlTickRate));
    displayLevelRelease_ = 1.0f - std::exp(-1.0f / (0.220f * controlTickRate));

    emfScale_ = internalRate / (twoPi * 220.0f);
    emfLowpassCoefficient_ = std::exp(
        -twoPi * std::min(16000.0f, 0.40f * internalRate) * inverseSampleRate_);

    // A strum's notes reach the engine within one host block; 35 ms is wide
    // enough to group a chord and short enough not to merge separate beats.
    chordWindowSamples_ = std::max(1, static_cast<int>(0.035 * sampleRate_));

    prepared_ = true;
    reset();
}

void ElectryEngine::reset()
{
    for (auto& voice : voices_)
    {
        silenceVoice(voice);
        voice.vertical.clear();
        voice.horizontal.clear();
        voice.fluxScale = stringFluxScale(voice.stringIndex);
        updateArticulationWeights(voice);
    }

    smoothedParameters_ = sanitise(targetParameters_);
    articulation_ = Articulation::Downstroke;
    alternateNextStrokeIsUp_ = false;
    noteSequence_ = 0;
    activeVoiceCount_ = 0;
    sympatheticStringCount_ = 0;
    controlCountdown_ = 0;
    pitchBendSemitones_ = pitchBendTarget_;
    vibratoAmount_ = vibratoTarget_;
    vibratoPhase_ = 0.0f;
    vibratoDepthSemitones_ = 0.01f * smoothedParameters_.vibratoDepthCents;
    sustainPedalDown_ = false;
    engineClock_ = 0;
    lastNoteOnClock_ = -(1ll << 40);
    chordAnchorString_ = 0;
    palmMuteBlend_ = clampf(smoothedParameters_.palmMute + palmMutePressure_,
                            0.0f, 1.0f);
    appliedPalmMute_ = palmMuteBlend_;
    sympatheticBus_ = 0.0f;
    sympatheticBusDelayed_ = 0.0f;
    sympatheticGain_ = 0.0045f * smoothedParameters_.sympatheticAmount;
    sympatheticInjection_ = sympatheticGain_ * (1.0f - palmMuteBlend_);
    sympatheticHandGain_ = 1.0f;
    sympatheticHandGainTarget_ = 1.0f;
    sympatheticHandMute_ = -1.0f;
    sympatheticActive_ = smoothedParameters_.sympatheticAmount > 0.0f;

    for (auto& filter : neckCoils_)
        filter.reset();
    for (auto& filter : bridgeCoils_)
        filter.reset();
    for (auto& blocker : outputDc_)
        blocker.reset();
    for (auto& decimator : decimators_)
        decimator.reset();
    for (auto& mode : bodyModes_)
        mode.reset();
    previousBodyDisplacement_ = 0.0f;
    bodyEmfLowpass_.reset();
    static constexpr std::array<float, stringCount> sympatheticQ {
        55.0f, 52.0f, 48.0f, 44.0f, 40.0f, 36.0f, 32.0f, 30.0f
    };
    for (int stringIndex = 0; stringIndex < stringCount; ++stringIndex)
    {
        auto& mode = sympatheticModes_[static_cast<std::size_t>(stringIndex)];
        mode.reset();
        mode.configure(
            midiToHz(static_cast<float>(
                stringSpecs()[static_cast<std::size_t>(stringIndex)].openMidiNote)),
            sympatheticQ[static_cast<std::size_t>(stringIndex)], 0.026f,
            static_cast<float>(sampleRate_));
    }
    smoothedOutputGain_ = smoothedParameters_.outputGain;
    smoothedBodyLevel_ = 24.5f * smoothedParameters_.bodyResonance;
    stereoWidth_ = smoothedParameters_.outputMode == OutputMode::Stereo ? 1.0f : 0.0f;
    channelsLinked_ = stereoWidth_ == 0.0f;
    artifactsActive_ = smoothedParameters_.artifactAmount > 0.0f;
    artifactContactShape_ = smoothStep(smoothedParameters_.artifactAmount);
    artifactBuzzAmount_ = smoothedParameters_.artifactAmount
                        * smoothedParameters_.artifactAmount;

    configureBody();
    configurePickupFilters();
    appliedVoicingParameters_ = smoothedParameters_;
    neckMix_ = neckMixTarget_;
    bridgeMix_ = bridgeMixTarget_;
    neckPathActive_ = neckMix_ > 1.0e-4f;
    bridgePathActive_ = bridgeMix_ > 1.0e-4f;
    // Everything above has just been cleared, so the engine starts frozen and
    // only wakes when a string is actually asked to vibrate.
    silentInternalSamples_ = 0;
    idleFrozen_ = true;
}

void ElectryEngine::setParameters(const EngineParameters& parameters)
{
    targetParameters_ = sanitise(parameters);
}

void ElectryEngine::setPitchBend(float normalisedBipolar) noexcept
{
    if (! std::isfinite(normalisedBipolar))
        normalisedBipolar = 0.0f;
    pitchBendTarget_ = 2.0f * clampf(normalisedBipolar, -1.0f, 1.0f);
}

void ElectryEngine::setVibratoAmount(float normalised) noexcept
{
    vibratoTarget_ = std::isfinite(normalised)
        ? clampf(normalised, 0.0f, 1.0f) : 0.0f;
}

void ElectryEngine::setPalmMutePressure(float normalised) noexcept
{
    palmMutePressure_ = std::isfinite(normalised)
        ? clampf(normalised, 0.0f, 1.0f) : 0.0f;
}

void ElectryEngine::setSustainPedal(bool down) noexcept
{
    if (sustainPedalDown_ && ! down)
    {
        for (auto& voice : voices_)
            if (voice.active && voice.sustained && ! voice.keyDown)
                beginVoiceRelease(voice);
    }
    sustainPedalDown_ = down;
    if (! down)
        for (auto& voice : voices_)
            voice.sustained = false;
}

// ---------------------------------------------------------------------------
// Notes, keyswitches, and string allocation
// ---------------------------------------------------------------------------

void ElectryEngine::noteOn(int midiNote, float velocity)
{
    if (! prepared_)
        return;

    velocity = clampf(std::isfinite(velocity) ? velocity : 0.0f, 0.0f, 1.0f);

    if (isKeyswitchNote(midiNote))
    {
        articulation_ = static_cast<Articulation>(midiNote - firstKeyswitchNote);
        if (articulation_ == Articulation::AlternateStroke)
            alternateNextStrokeIsUp_ = false;
        return;
    }

    if (! isPlayableNote(midiNote) || velocity <= 0.0f)
        return;

    const auto latchedArticulation = articulation_;
    const auto voiceArticulation = latchedArticulation == Articulation::AlternateStroke
        ? (alternateNextStrokeIsUp_ ? Articulation::Upstroke
                                    : Articulation::Downstroke)
        : latchedArticulation;
    const int stringIndex = chooseString(midiNote, voiceArticulation);
    if (stringIndex < 0)
        return;

    if (latchedArticulation == Articulation::AlternateStroke)
        alternateNextStrokeIsUp_ = ! alternateNextStrokeIsUp_;

    auto& voice = voices_[static_cast<std::size_t>(stringIndex)];

    // Strum travel. Note-ons that arrive inside one chord window belong to the
    // same pick stroke; the first of them fixes the edge the pick starts from,
    // and every later string is offset by the time the pick needs to reach it.
    // At a zero spread this is exactly the previous simultaneous behaviour.
    const bool newChord = engineClock_ - lastNoteOnClock_
                        > static_cast<std::int64_t>(chordWindowSamples_);
    if (newChord)
        chordAnchorString_ = stringIndex;
    lastNoteOnClock_ = engineClock_;

    int startDelaySamples = 0;
    // Read the sanitised target rather than the smoothed copy. This control
    // schedules the stroke instead of shaping it, so the control tick copies it
    // verbatim; a chord whose note-ons land at offset 0 of the same block as
    // the automation change would otherwise be scheduled with the previous
    // block's spread, usually as a block chord.
    const float spreadSeconds = targetParameters_.strumSpreadSeconds;
    if (spreadSeconds > 0.0f && ! newChord)
    {
        const int stringsCrossed = std::abs(stringIndex - chordAnchorString_);
        startDelaySamples = static_cast<int>(
            spreadSeconds * static_cast<float>(sampleRate_))
            * stringsCrossed;
    }

    const bool legato = voiceArticulation == Articulation::HammerOn
                     && voice.active
                     && voice.midiNote != midiNote;
    if (legato)
        legatoRetarget(voice, midiNote, velocity);
    else
        startVoice(voice, midiNote, velocity, voiceArticulation, startDelaySamples);

    updateActiveVoiceCount();
}

void ElectryEngine::noteOff(int midiNote)
{
    if (! prepared_ || isKeyswitchNote(midiNote))
        return;

    for (auto& voice : voices_)
    {
        if (! voice.active || ! voice.keyDown || voice.midiNote != midiNote)
            continue;
        voice.keyDown = false;
        if (sustainPedalDown_)
            voice.sustained = true;
        else
            beginVoiceRelease(voice);
    }
}

void ElectryEngine::allNotesOff()
{
    for (auto& voice : voices_)
    {
        if (! voice.active)
            continue;
        voice.keyDown = false;
        voice.sustained = false;
        beginVoiceRelease(voice);
    }
}

int ElectryEngine::chooseString(int midiNote, Articulation articulation) const noexcept
{
    const auto& specs = stringSpecs();
    const auto fretOn = [&specs, midiNote] (int stringIndex)
    {
        return midiNote - specs[static_cast<std::size_t>(stringIndex)].openMidiNote;
    };
    const auto playable = [&fretOn] (int stringIndex)
    {
        const int fret = fretOn(stringIndex);
        return fret >= 0 && fret <= fretCount;
    };

    // A repick of a note that is already sounding grabs the same string.
    for (int s = 0; s < stringCount; ++s)
        if (voices_[static_cast<std::size_t>(s)].active
            && voices_[static_cast<std::size_t>(s)].midiNote == midiNote
            && playable(s))
            return s;

    // Hammer-on/pull-off continues the closest sounding string when the new
    // note stays within a reachable stretch of the fretting hand.
    if (articulation == Articulation::HammerOn)
    {
        int best = -1;
        int bestDistance = 10;
        for (int s = 0; s < stringCount; ++s)
        {
            const auto& voice = voices_[static_cast<std::size_t>(s)];
            if (! voice.active || ! playable(s) || fretOn(s) < 1)
                continue;
            const int distance = std::abs(fretOn(s) - voice.fret);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = s;
            }
        }
        if (best >= 0)
            return best;
    }

    // Otherwise prefer the free string that plays the note at the lowest
    // fret; ties resolve toward the thicker string. This reproduces common
    // open-position fingerings deterministically.
    int best = -1;
    int bestFret = fretCount + 1;
    for (int s = 0; s < stringCount; ++s)
    {
        if (! playable(s) || voices_[static_cast<std::size_t>(s)].active)
            continue;
        if (fretOn(s) < bestFret)
        {
            bestFret = fretOn(s);
            best = s;
        }
    }
    if (best >= 0)
        return best;

    // All candidate strings are sounding: steal the oldest releasing string
    // first, then the oldest held string.
    const auto stealScore = [this, &playable] (int stringIndex) -> std::uint64_t
    {
        const auto& voice = voices_[static_cast<std::size_t>(stringIndex)];
        if (! playable(stringIndex))
            return 0;
        // Older starts win; releasing voices win over held voices.
        const std::uint64_t age = ~voice.startOrder;
        return voice.releasing ? (age | (1ull << 63)) : (age & ~(1ull << 63));
    };

    std::uint64_t bestScore = 0;
    for (int s = 0; s < stringCount; ++s)
    {
        const auto score = stealScore(s);
        if (score > bestScore)
        {
            bestScore = score;
            best = s;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Voice setup
// ---------------------------------------------------------------------------

float ElectryEngine::scaleLengthMetres() const noexcept
{
    return lerp(conventionalScaleMetres, baritoneScaleMetres,
                smoothedParameters_.scaleLength);
}

float ElectryEngine::deadSpotFactor(int stringIndex, int fret) const noexcept
{
    // Solid-body dead spots: locally raised neck conductance shortens decay
    // at specific fret positions. Bolt-on construction deepens the effect.
    static constexpr std::array<float, stringCount> centres {
        8.0f, 9.0f, 9.5f, 11.0f, 12.5f, 6.0f, 5.0f, 7.5f
    };
    const float depth = lerp(0.03f, 0.45f,
                             smoothedParameters_.construction);
    const float distance = static_cast<float>(fret)
                         - centres[static_cast<std::size_t>(stringIndex)];
    const float gaussian = std::exp(-distance * distance / (2.0f * 1.65f * 1.65f));
    return 1.0f / (1.0f + depth * gaussian);
}

void ElectryEngine::configureVoiceDamping(Voice& voice) noexcept
{
    const auto& parameters = smoothedParameters_;
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];

    float t60 = spec.t60Seconds;
    t60 *= lerp(1.08f, 0.38f, parameters.stringAge);
    t60 *= lerp(1.16f, 0.86f, parameters.construction);
    t60 *= lerp(0.88f, 1.18f, parameters.stringGauge);
    t60 *= lerp(1.05f, 0.95f, parameters.bodyWood);
    t60 *= lerp(1.04f, 0.96f, parameters.bodySize);
    t60 *= deadSpotFactor(voice.stringIndex, voice.fret);

    // The body modes are not merely a parallel EQ. Modal bridge admittance
    // removes string energy fastest when a fundamental or strong low partial
    // lands on a structural resonance, producing construction-dependent
    // sustain and dead-spot behaviour without an unstable additive feedback
    // loop. Six partials cover the body band even for open Drop-E.
    float weightedConductance = 0.0f;
    float conductanceWeight = 0.0f;
    for (int partial = 1; partial <= 6; ++partial)
    {
        const float frequency = voice.baseFrequency * static_cast<float>(partial);
        if (frequency >= 0.4f * static_cast<float>(sampleRate_))
            break;
        const float weight = 1.0f / static_cast<float>(partial);
        weightedConductance += weight * bodyConductanceAt(frequency);
        conductanceWeight += weight;
    }
    voice.bodyConductance = weightedConductance
        / std::max(conductanceWeight, 1.0e-6f);
    const float structuralCoupling = smoothStep(parameters.bodyResonance)
        * lerp(0.82f, 1.18f, parameters.construction);
    voice.bodyLossFactor = 1.0f
        / (1.0f + 3.8f * structuralCoupling * voice.bodyConductance);
    t60 *= voice.bodyLossFactor;
    t60 = clampf(t60, 0.02f, 26.0f);

    // The bridge hand is a broadband absorber, not a rescaling of the string's
    // own frequency-dependent loss. Modelling it as a separate loss that adds
    // to the string's - in parallel, so the reciprocal decay rates sum - is
    // what makes a palm mute behave like a hand and not like a gate.
    //
    // The targets come from dry muted power-chord reference recordings. A real
    // short muted chord falls only a couple of decibels in its first 25 ms and
    // takes around half a second to reach -40 dB; a long one holds a low tail
    // for seconds. The previous model applied the hand as a minimum on the
    // fundamental's T60 and then multiplied *that* by the string's
    // high-frequency ratio, so a half-second mute target implied a seventeen
    // millisecond high-frequency target and the note collapsed 36 dB inside
    // 25 ms. That is what read as a cut chug rather than a muted note.
    //
    // Zero hand means exactly zero: both targets stay at zero and the parallel
    // combination below is skipped entirely, so an unmuted string is
    // bit-for-bit what it was.
    //
    // The two contacts are tracked apart because they sit in different places.
    // `handT60` is the bridge hand - the Muted and Chug styles and the
    // continuous pressure - whose loss is tilted with frequency below.
    // `chokeT60` is the fretting hand's dead-note choke, which is not near the
    // bridge and stays broadband. They still combine in parallel, so a dead
    // note played under palm-mute pressure gets both.
    float handT60 = 0.0f;
    float chokeT60 = 0.0f;
    if (voice.articulation == Articulation::Muted)
    {
        handT60 = std::exp(lerp(std::log(2.60f), std::log(0.32f),
                                parameters.muteDamping));
    }
    else if (voice.articulation == Articulation::Chug)
    {
        // A chug is a firmer bridge-hand mute than the general Muted style, so
        // it lands nearer the reference's short mute than its long one - but it
        // still has to be a note.
        handT60 = std::exp(lerp(std::log(1.40f), std::log(0.20f),
                                parameters.muteDamping));
    }
    else if (voice.articulation == Articulation::DeadNote)
    {
        // Fretting-hand pressure releases before the string establishes a
        // sustained pitch, leaving only a short deterministic percussion hit.
        // This one really is a choke rather than a load, and it is tracked
        // separately from the bridge hand for exactly that reason: it is a
        // contact somewhere up the neck, so the near-the-bridge mode-shape
        // argument that tilts the bridge hand's loss with frequency does not
        // describe it. It stays broadband.
        chokeT60 = 0.032f;
    }
    else if (voice.articulation == Articulation::NaturalHarmonic)
    {
        t60 = std::min(t60, 3.8f);
    }
    else if (voice.articulation == Articulation::PinchHarmonic)
    {
        t60 = std::min(t60, 2.9f);
    }

    // High-frequency decay target relative to the fundamental's decay, at the
    // `fHigh` reference below.
    //
    // A wound string is not a plain one with a thicker core: the wrap slides
    // over the core and dissipates bending energy, so its top end dies far
    // faster than its fundamental. Measured on the reference low E, content
    // above a kilohertz has effectively gone inside a tenth of a second while
    // the fundamental is still ringing seconds later. The ratio was previously
    // an order of magnitude too generous, which is what kept a low note's
    // 1-2 kHz partials sounding for as long as its fundamental - the nasal,
    // clavinet-like register the reference does not have.
    float highRatio = lerp(0.036f, 0.010f, parameters.stringAge);
    highRatio *= spec.wound ? 1.0f : 7.5f;
    highRatio *= lerp(1.15f, 0.78f, parameters.stringGauge);
    highRatio *= lerp(0.86f, 1.16f, parameters.bodyWood);
    highRatio *= lerp(0.58f, 1.70f, parameters.construction);

    // Continuous bridge-hand damping. Unlike the Muted and Chug keyswitches
    // this is a smooth pressure that applies to every play style, so a phrase
    // can open from a dead chug into a ringing chord without a style change.
    // It is the same absorber as those styles, so it joins them in parallel
    // rather than overriding them, and a pressure of exactly zero remains a
    // mathematical no-op. The heel of the hand is a soft, lossy contact, so it
    // also darkens the string as it covers more of it.
    if (palmMuteBlend_ > 0.0f)
    {
        const float pressureT60 = std::exp(lerp(std::log(4.0f), std::log(0.080f),
                                                palmMuteBlend_));
        handT60 = handT60 > 0.0f
            ? 1.0f / (1.0f / handT60 + 1.0f / pressureT60)
            : pressureT60;
        highRatio *= lerp(1.0f, 0.62f, palmMuteBlend_);
    }
    highRatio = clampf(highRatio, 0.0015f, 0.9f);

    // How much of the string's loss the bridge hand accounts for, for the
    // loss shelf below. Zero means no hand and an exactly bypassed shelf.
    float handShareForSlope = 0.0f;
    // The string's own targets, before either hand.
    float t60High = t60 * highRatio;
    if (handT60 > 0.0f || chokeT60 > 0.0f)
    {
        // Losses in parallel: decay rates add, so the reciprocals of the decay
        // times do. A hand therefore dominates wherever it is tighter than
        // the string and disappears wherever it is not, at every frequency
        // independently - which is why a muted note keeps a body instead of
        // having its top end scaled into nothing.
        //
        // The heel of the hand rests near the bridge, and a contact there does
        // not absorb every mode equally: mode n's displacement under the hand
        // goes as sin(n*pi*x/L), so the energy it can take out of that mode
        // rises steeply with n while the fundamental, which barely moves that
        // close to the bridge, is left comparatively free. Treating the hand as
        // a broadband absorber - the same rate at both fitted points - damped
        // the fundamental as hard as the top end, which is what made a palm
        // mute read as a thin, cut-off pick rather than a heavy chug.
        //
        // The mode-shape ratio between the fundamental and the 3.6 kHz fitted
        // point is nearly two orders of magnitude, and it oscillates once
        // n*pi*x/L passes the first quarter period. This bounded factor is a
        // deliberately conservative monotone stand-in for it: enough to keep
        // the fundamental while the hand kills the top, without pretending to
        // resolve the mode shape. Fitted against the muted reference power
        // chords, where it recovered 5.4 dB in the 60-85 Hz band and removed
        // 2.4 dB of the 1.4-2.7 kHz excess.
        //
        // The tilt only ever raised the hand's loss at the top; the same mode
        // shape says the fundamental has to be let go at the bottom, and the
        // reference recordings say so far more bluntly than the tilt allowed.
        // In the five looser muted reference chords the fundamental's own level
        // barely moves over the first third of a second - its loss between 50
        // and 350 ms is +1.0 to -2.8 dB - while the third harmonic drops 7 to
        // 13 dB and the fourth as much as 24 dB in the same window. A palm mute
        // does not shorten f0; it removes the harmonics above it. Charging the
        // hand's full rate at f0 is what left a muted power chord with no bottom
        // and no tail: against those references it sat 13.6 dB low at 400 ms and
        // 30.5 dB low at 800 ms, with the root partial 15 dB under Electry's own
        // strongest low peak where every reference has it at the top.
        //
        // The mode shape on its own argues for a divisor near eight - the heel
        // a tenth of the sounding length from the bridge leaves the fundamental
        // at a few per cent of the plateau the upper modes reach. That number
        // cannot be used here, and the reason is worth recording: the loop is a
        // single one-pole whose corner sits far above these partials, so
        // relieving f0 relieves the second, third and fourth harmonics by very
        // nearly the same amount. Measured, h1 to h4 then move together within
        // about 2 dB at four of the five reference pitches, against references
        // that damp h3 and h4 8 to 24 dB harder than h1. A divisor of eight
        // therefore over-relieves the whole low-mid comb if the loop is asked to
        // carry the harmonic tilt as well. It is not: this term is fitted purely
        // on the decay envelope, and there the mode shape's own answer is close
        // to right. Swept against the five matched reference pitches on a joint
        // objective of tilt shape plus peak-relative energy contour, the contour
        // error falls 11.74, 6.64, 4.84, 4.11, 4.00, 4.38, 5.31 dB at divisors
        // of 2, 3, 4, 5, 6, 8, 12 - a clear minimum at six, and 7.7 dB better
        // than the two this replaced.
        //
        // Prising the harmonics apart from the fundamental is a separate problem
        // and it is not solved here. A mute-gated loss shelf, unity at f0 and
        // falling above a corner set as a multiple of it, was built and measured
        // because it is the only mechanism that can express loss in harmonic
        // number rather than in hertz. It works, and it is still not shippable:
        // across its whole usable range it trades the two terms almost exactly
        // one for one - tilt 17.9 to 13.7 dB against contour 6.6 to 11.3 dB - so
        // the joint score never improves. In this architecture the tilt and the
        // note's total energy are one degree of freedom, not two, because a
        // string's energy lives in the same low harmonics the tilt has to
        // remove, and in a power chord the root's second and third harmonics are
        // the fifth's and octave's fundamentals. Separating them needs the two
        // to stop sharing a single one-pole, which is a change to the loop's
        // order rather than to any constant in it.
        //
        // The dead-note choke is added at the same rate on both, because it is
        // the fretting hand somewhere up the neck rather than the heel resting
        // by the bridge, and none of the reasoning above describes it. Adding
        // the two rates rather than switching between them keeps a dead note
        // played under palm-mute pressure correct: both contacts are present,
        // and each contributes with its own frequency behaviour.
        constexpr float handHighFrequencyTilt = 3.0f;
        constexpr float handFundamentalRelief = 6.0f;
        const float handRate = handT60 > 0.0f ? 1.0f / handT60 : 0.0f;
        // The shelf's depth follows this rather than switching on. It is the
        // hand's share of the string's total loss rate, so it is zero without a
        // hand, near one under a firm mute, and continuous in between - which
        // matters because Palm Mute and CC2 are swept live. Gating full depth on
        // any positive rate put a step in the middle of an expression pedal.
        handShareForSlope = handRate / std::max(1.0f / t60 + handRate, 1.0e-9f);
        const float chokeRate = chokeT60 > 0.0f ? 1.0f / chokeT60 : 0.0f;
        t60 = 1.0f / (1.0f / t60 + handRate / handFundamentalRelief + chokeRate);
        t60High = 1.0f / (1.0f / t60High
                          + handRate * handHighFrequencyTilt + chokeRate);
    }
    t60 = clampf(t60, 0.02f, 26.0f);
    t60High = clampf(t60High, 0.008f, t60);

    const float sampleRate = static_cast<float>(sampleRate_);
    const float f0 = voice.baseFrequency;
    const float fHigh = std::min(3600.0f, 0.32f * sampleRate);
    const float omega0 = twoPi * f0 * inverseSampleRate_;
    const float omegaHigh = twoPi * std::max(fHigh, f0 * 1.5f) * inverseSampleRate_;

    // The shelf's corner is shared; its depth is settled per polarisation below,
    // because how much of it is feasible depends on that loop's own decay ratio.
    const float shelfRequestedDepth =
        clampf(handLossDepthScale * handShareForSlope, 0.0f, 0.95f);
    const float shelfCoefficient = shelfRequestedDepth > 0.0f
        ? std::exp(-twoPi * clampf(handLossCornerRatio * f0, 40.0f,
                                   0.40f * sampleRate) * inverseSampleRate_)
        : 0.0f;

    const auto configureLoop = [&] (PolarisationLoop& loop, float t60Scale)
    {
        const float t60Fundamental = std::max(0.02f, t60 * t60Scale);
        const float t60HighTarget = std::max(0.008f, t60High * t60Scale);
        const float period = sampleRate / f0;
        const float gainAtF0 =
            std::pow(10.0f, -3.0f * period / (t60Fundamental * sampleRate));
        const float gainAtHigh =
            std::pow(10.0f, -3.0f * period / (t60HighTarget * sampleRate));
        const float ratio = clampf(gainAtHigh / std::max(gainAtF0, 1.0e-6f),
                                   1.0e-4f, 1.0f);

        loop.handLossCoefficient = shelfCoefficient;
        // Divide the shelf out of both targets. This is the whole point: the
        // one-pole is then solved for what is left, so the envelope stays where
        // the references put it and the shelf's slope is free to bend the curve
        // in between. A muted note spends most of its loop gain on the mute
        // itself, which is what leaves the headroom for this to be possible -
        // on an unmuted 20 s decay the same compensation would ask for a loop
        // gain above unity and clamp.
        //
        // The shelf can only be as deep as the note's own decay ratio leaves
        // room for. A one-pole cannot raise the high end relative to the low, so
        // if the shelf's own tilt is already steeper than the total the targets
        // ask for, the remainder is unsolvable. Clamping that quotient to one -
        // which is what this did first - hid the problem and let it through: the
        // damping pole collapsed to near zero, the loop gain hit its ceiling, and
        // muted notes above about A2 had their decay set by the shelf instead of
        // by their fitted T60s. Bisecting the depth down until the remainder is
        // solvable keeps the fit honest and simply gives those notes a shallower
        // shelf, which is the correct answer rather than a silent failure.
        const auto shelfTilt = [&] (float depth)
        {
            float atF0 = 1.0f, atHigh = 1.0f, unused = 0.0f;
            handLossResponse(depth, shelfCoefficient, omega0, atF0, unused);
            handLossResponse(depth, shelfCoefficient, omegaHigh, atHigh, unused);
            return clampf(atHigh, 1.0e-3f, 1.0f) / clampf(atF0, 1.0e-3f, 1.0f);
        };
        float depth = shelfRequestedDepth;
        // Leave the one-pole a little to do, so it stays the thing that meets
        // the targets and the shelf stays a correction to its shape.
        const float feasibleTilt = ratio / 0.98f;
        if (depth > 0.0f && shelfTilt(depth) < feasibleTilt)
        {
            float lowDepth = 0.0f, highDepth = depth;
            for (int i = 0; i < 12; ++i)
            {
                const float mid = 0.5f * (lowDepth + highDepth);
                if (shelfTilt(mid) < feasibleTilt)
                    highDepth = mid;
                else
                    lowDepth = mid;
            }
            depth = lowDepth;
        }
        loop.handLossDepth = depth;
        if (depth <= 0.0f)
            loop.handLoss.reset();

        float shelfF0 = 1.0f, shelfHigh = 1.0f, shelfPhase = 0.0f;
        handLossResponse(depth, shelfCoefficient, omega0, shelfF0, shelfPhase);
        handLossResponse(depth, shelfCoefficient, omegaHigh, shelfHigh, shelfPhase);
        shelfF0 = clampf(shelfF0, 1.0e-3f, 1.0f);
        shelfHigh = clampf(shelfHigh, 1.0e-3f, 1.0f);

        // Solve the one-pole coefficient whose magnitude ratio between f0 and
        // fHigh matches the decay-time ratio. The response is monotonic in
        // the coefficient, so bisection is reliable.
        const auto magnitude = [] (float a, float omega)
        {
            const float denom = 1.0f + a * a - 2.0f * a * std::cos(omega);
            return (1.0f - a) / std::sqrt(std::max(denom, 1.0e-12f));
        };
        const float onePoleRatio = clampf(
            ratio / clampf(shelfHigh / shelfF0, 1.0e-4f, 1.0e4f), 1.0e-4f, 1.0f);
        float low = 0.0f;
        float high = 0.9995f;
        for (int i = 0; i < 18; ++i)
        {
            const float mid = 0.5f * (low + high);
            const float value = magnitude(mid, omegaHigh) / magnitude(mid, omega0);
            if (value > onePoleRatio)
                low = mid;
            else
                high = mid;
        }
        loop.loopDampingCoefficient = 0.5f * (low + high);
        const float f0Magnitude =
            magnitude(loop.loopDampingCoefficient, omega0) * shelfF0;
        loop.loopGain = clampf(gainAtF0 / std::max(f0Magnitude, 1.0e-6f),
                               0.0f, 0.99999f);
    };

    // The polarisation parallel to the body outlives the perpendicular one,
    // producing the instrument's two-stage decay.
    configureLoop(voice.vertical, 1.0f);
    configureLoop(voice.horizontal, 1.7f);

    // The loop filters moved, so the analytic phase compensation is stale even
    // if the target pitch did not change.
    voice.compensationDirty = true;
}

void ElectryEngine::configureVoicePitch(Voice& voice, bool forceDelayJump) noexcept
{
    const float bendOffset = lerp(voice.bendStartSemitones, voice.bendTargetSemitones,
                                  smoothStep(voice.bendProgress));
    float legatoOffset = 0.0f;
    if (voice.legatoBlend < 1.0f && voice.legatoFromFrequency > 0.0f)
    {
        const float fromSemis = 12.0f * std::log2(voice.legatoFromFrequency
                                                  / voice.baseFrequency);
        legatoOffset = fromSemis * (1.0f - smoothStep(voice.legatoBlend));
    }

    // A restrained, delayed guitar vibrato: the wheel becomes expressive
    // without making newly picked attacks seasick. CC1 reaches +/- 35 cents.
    const float vibratoFade = smoothStep(clampf(
        static_cast<float>(voice.ageSamples) / (0.09f * static_cast<float>(sampleRate_)),
        0.0f, 1.0f));
    const float vibratoSemitones = vibratoDepthSemitones_ * vibratoAmount_
                                 * vibratoFade * std::sin(vibratoPhase_);
    const float semitones = bendOffset + legatoOffset + pitchBendSemitones_
                          + vibratoSemitones;
    const float f0 = clampf(voice.baseFrequency * std::exp2(semitones / 12.0f),
                            20.0f, 0.24f * static_cast<float>(sampleRate_));

    const bool pitchMoved = forceDelayJump
        || std::abs(semitones - voice.lastConfiguredSemitones) > 8.0e-4f
        || std::abs(f0 - voice.lastConfiguredFrequency)
               > 5.0e-5f * std::max(f0, 20.0f);
    const float omega = twoPi * f0 * inverseSampleRate_;
    const float period = static_cast<float>(sampleRate_) / f0;

    // The dispersion solve is only refreshed when the sounding pitch actually
    // moves; the tension-modulation factor below stays cheap enough for every
    // control tick. Damping-only changes (palm-mute pressure, string age, body
    // loss) reuse this fit and only redo the analytic phase compensation,
    // which avoids several hundred atan2 evaluations per automated control
    // tick per string.
    if (pitchMoved)
    {
        voice.lastConfiguredSemitones = semitones;
        voice.lastConfiguredFrequency = f0;

        // Stiffness inharmonicity from the string's physical make-up. Wound
        // strings bend with their core, not their full winding diameter.
        const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
        const float gaugeScale = lerp(1.0f, 11.0f / 9.0f,
                                      smoothedParameters_.stringGauge);
        const float diameter = spec.plainDiameterMm * 1.0e-3f * gaugeScale;
        const float bendingDiameter = diameter * spec.woundCoreScale;
        const float openLength = scaleLengthMetres();
        const float soundingLength = openLength
            * std::exp2(-static_cast<float>(voice.fret) / 12.0f);
        const float massScale = spec.wound ? 0.85f : 1.0f;
        const float linearMass = massScale * steelDensity * pi
                               * 0.25f * diameter * diameter;
        const float waveSpeed = 2.0f * soundingLength * voice.baseFrequency;
        const float tension = linearMass * waveSpeed * waveSpeed;
        const float bendingStiffness = pi * pi * pi * steelYoungModulus
                                     * bendingDiameter * bendingDiameter
                                     * bendingDiameter * bendingDiameter / 64.0f;
        float inharmonicity = bendingStiffness
            / std::max(tension * soundingLength * soundingLength, 1.0e-9f);
        inharmonicity = clampf(inharmonicity, 0.0f, 3.0e-3f);

        // Eight cascaded first-order sections are fitted in factored form:
        // four share each band coefficient. A
        // bounded two-pass grid minimises relative phase-delay error at both
        // reference partials. This is done only at note/control setup.
        const float highPartial = clampf(
            std::floor(0.30f * static_cast<float>(sampleRate_) / f0),
            4.0f, 16.0f);
        const float lowPartial = std::min(
            4.0f, std::max(2.0f, std::floor(0.5f * highPartial)));
        float lowCoefficient = 0.0f;
        float highCoefficient = 0.0f;

        const auto wantedDeficit = [&] (float partial)
        {
            const float stretch =
                std::sqrt((1.0f + inharmonicity * partial * partial)
                          / (1.0f + inharmonicity));
            return period * (1.0f - 1.0f / stretch);
        };
        const auto pairDeficit = [&] (float coefficient, float partial)
        {
            const float omegaRef = std::min(omega * partial, pi * 0.95f);
            return 2.0f * (allpassPhaseDelay(coefficient, omega)
                           - allpassPhaseDelay(coefficient, omegaRef));
        };
        if (inharmonicity > 1.0e-8f)
        {
            const float wantedLow = wantedDeficit(lowPartial);
            const float wantedHigh = wantedDeficit(highPartial);
            const float lowScale = std::max(wantedLow, 0.01f);
            const float highScale = std::max(wantedHigh, 0.04f);
            float bestError = 1.0e30f;
            float lowMinimum = -0.995f;
            float lowMaximum = 0.0f;
            float highMinimum = -0.995f;
            float highMaximum = 0.0f;

            for (int pass = 0; pass < 2; ++pass)
            {
                const int divisions = pass == 0 ? 17 : 13;
                const float lowStep = (lowMaximum - lowMinimum)
                                    / static_cast<float>(divisions);
                const float highStep = (highMaximum - highMinimum)
                                     / static_cast<float>(divisions);
                for (int lowIndex = 0; lowIndex <= divisions; ++lowIndex)
                {
                    const float candidateLow = lowMinimum
                        + lowStep * static_cast<float>(lowIndex);
                    for (int highIndex = 0; highIndex <= divisions; ++highIndex)
                    {
                        const float candidateHigh = highMinimum
                            + highStep * static_cast<float>(highIndex);
                        const float actualLow =
                            2.0f * pairDeficit(candidateLow, lowPartial)
                            + 2.0f * pairDeficit(candidateHigh, lowPartial);
                        const float actualHigh =
                            2.0f * pairDeficit(candidateLow, highPartial)
                            + 2.0f * pairDeficit(candidateHigh, highPartial);
                        const float lowError = (actualLow - wantedLow) / lowScale;
                        const float highError = (actualHigh - wantedHigh) / highScale;
                        const float error = lowError * lowError
                                          + 1.30f * highError * highError
                                          + 1.0e-5f
                                            * (candidateLow * candidateLow
                                               + candidateHigh * candidateHigh);
                        if (error < bestError)
                        {
                            bestError = error;
                            lowCoefficient = candidateLow;
                            highCoefficient = candidateHigh;
                        }
                    }
                }

                lowMinimum = std::max(-0.995f, lowCoefficient - lowStep);
                lowMaximum = std::min(0.0f, lowCoefficient + lowStep);
                highMinimum = std::max(-0.995f, highCoefficient - highStep);
                highMaximum = std::min(0.0f, highCoefficient + highStep);
            }
        }

        voice.inharmonicity = inharmonicity;
        voice.dispersionLowPartial = lowPartial;
        voice.dispersionHighPartial = highPartial;
        voice.vertical.dispersionLowCoefficient = lowCoefficient;
        voice.vertical.dispersionHighCoefficient = highCoefficient;
        voice.horizontal.dispersionLowCoefficient = lowCoefficient;
        voice.horizontal.dispersionHighCoefficient = highCoefficient;
        voice.compensationDirty = true;
    }

    if (pitchMoved || voice.compensationDirty)
    {
        // Compensate every loop filter's phase delay at the fundamental so
        // the sounding pitch matches the target frequency.
        const auto loopPhaseDelay = [&] (const PolarisationLoop& loop)
        {
            // The shelf is in the loop, so its phase is part of the sounding
            // period; without this a palm-muted string sits 12.5 cents flat.
            float shelfMagnitude = 1.0f, shelfPhase = 0.0f;
            handLossResponse(loop.handLossDepth, loop.handLossCoefficient, omega,
                             shelfMagnitude, shelfPhase);
            const float shelfDelay = omega > 1.0e-9f ? -shelfPhase / omega : 0.0f;
            return onePolePhaseDelay(loop.loopDampingCoefficient, omega)
                 + shelfDelay
                 + 4.0f * allpassPhaseDelay(
                       loop.dispersionLowCoefficient, omega)
                 + 4.0f * allpassPhaseDelay(
                       loop.dispersionHighCoefficient, omega);
        };

        voice.compensatedPeriodVertical = period - loopPhaseDelay(voice.vertical);
        voice.compensatedPeriodHorizontal = period - loopPhaseDelay(voice.horizontal);
        voice.compensationDirty = false;
    }

    const float tensionFactor =
        1.0f / (1.0f + voice.tensionDepth * voice.energyEnvelope);

    const float verticalDelay = voice.compensatedPeriodVertical * tensionFactor;
    // A slightly longer horizontal path detunes the second polarisation by a
    // fraction of a cent, producing the natural slow beating of real strings.
    const float horizontalDelay = voice.compensatedPeriodHorizontal
                                * tensionFactor * 1.00023f + 0.11f;

    voice.vertical.targetDelay = clampf(verticalDelay, 4.0f,
                                        static_cast<float>(delayLineSize - 8));
    voice.horizontal.targetDelay = clampf(horizontalDelay, 4.0f,
                                          static_cast<float>(delayLineSize - 8));

    if (forceDelayJump)
    {
        voice.vertical.currentDelay = voice.vertical.targetDelay;
        voice.horizontal.currentDelay = voice.horizontal.targetDelay;
    }

    // Delay smoothing time constant: fast enough to track bends transparently.
    const float coefficient = 1.0f - std::exp(-static_cast<float>(controlPeriod)
                                              / (0.006f * static_cast<float>(sampleRate_)));
    voice.vertical.delaySmoothing = coefficient;
    voice.horizontal.delaySmoothing = coefficient;
}

void ElectryEngine::refreshVoicingIfNeeded() noexcept
{
    // Damping, pickup geometry, and stiffness react to their controls while
    // notes are held, but only when a relevant control has actually moved.
    const auto& s = smoothedParameters_;
    const auto& a = appliedVoicingParameters_;
    const auto moved = [] (float current, float applied)
    {
        return std::abs(current - applied) > 2.0e-3f;
    };

    // Splitting the refresh by what each control actually changes is a large
    // saving: only the geometry axes require the dispersion grid search to run
    // again, while damping-only moves reuse the existing fit.
    const bool dampingDirty = moved(s.stringAge, a.stringAge)
                           || moved(s.stringGauge, a.stringGauge)
                           || moved(s.bodyWood, a.bodyWood)
                           || moved(s.bodySize, a.bodySize)
                           || moved(s.bodyShape, a.bodyShape)
                           || moved(s.bodyResonance, a.bodyResonance)
                           || moved(s.construction, a.construction)
                           || moved(s.muteDamping, a.muteDamping)
                           || moved(palmMuteBlend_, appliedPalmMute_);
    const bool geometryDirty = moved(s.stringGauge, a.stringGauge)
                            || moved(s.scaleLength, a.scaleLength);
    const bool pickupDirty = moved(s.pickupType, a.pickupType)
                          || moved(s.scaleLength, a.scaleLength);
    if (! dampingDirty && ! geometryDirty && ! pickupDirty)
        return;

    appliedVoicingParameters_ = smoothedParameters_;
    appliedPalmMute_ = palmMuteBlend_;
    for (auto& voice : voices_)
    {
        if (! voice.active)
        {
            // A sympathetically ringing string follows the build in place:
            // its delay target and loop filter are re-solved without clearing
            // the line, so the ring never clicks.
            if (voice.sympatheticReady)
                configureSympatheticString(voice);
            continue;
        }
        if (dampingDirty)
            configureVoiceDamping(voice);
        if (pickupDirty)
            configureVoicePickups(voice);
        if (geometryDirty)
            voice.lastConfiguredSemitones = -999.0f;
        configureVoicePitch(voice, false);
    }
}

void ElectryEngine::configureVoicePickups(Voice& voice) noexcept
{
    const auto& parameters = smoothedParameters_;
    const float openLength = scaleLengthMetres();
    const float soundingLength = openLength
        * std::exp2(-static_cast<float>(voice.fret) / 12.0f);

    const float bridgeDistance = lerp(lesPaulBridgePickupMetres,
                                      telecasterBridgePickupMetres,
                                      parameters.pickupType);
    const float neckDistance = lerp(lesPaulNeckPickupMetres,
                                    telecasterNeckPickupMetres,
                                    parameters.pickupType);

    const float period = static_cast<float>(sampleRate_) / voice.baseFrequency;
    const float bridgeFraction = clampf(bridgeDistance / soundingLength, 0.01f, 0.95f);
    const float neckFraction = clampf(neckDistance / soundingLength, 0.01f, 0.95f);
    voice.pickupDelayBridge = clampf(bridgeFraction * period, 2.0f,
                                     static_cast<float>(delayLineSize - 8));
    voice.pickupDelayNeck = clampf(neckFraction * period, 2.0f,
                                   static_cast<float>(delayLineSize - 8));

    // Magnetic aperture: a true finite rectangular spatial window. Its
    // temporal length is Fs*w/c, where c is transverse wave speed. This has
    // the expected sinc response and -3 dB point at approximately .443*c/w,
    // unlike the previous one-pole approximation.
    const float aperture = lerp(humbuckerApertureMetres, singleCoilApertureMetres,
                                parameters.pickupType);
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    const float waveSpeed = 2.0f * openLength * midiToHz(
        static_cast<float>(spec.openMidiNote));
    const float apertureLength = clampf(
        static_cast<float>(sampleRate_) * aperture / std::max(waveSpeed, 1.0f),
        1.0f, static_cast<float>(apertureHistorySize - 2));
    voice.apertureNeckLength = apertureLength;
    voice.apertureBridgeLength = apertureLength;

}

void ElectryEngine::configureSympatheticString(Voice& voice) noexcept
{
    // An unfingered string vibrates over its full open length. One
    // polarisation is enough for a bridge-coupled ring, so this solves a
    // single loop: period, damping, gain, and the bridge pickup tap.
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    const auto& parameters = smoothedParameters_;
    const float sampleRate = static_cast<float>(sampleRate_);
    const float f0 = midiToHz(static_cast<float>(spec.openMidiNote));
    const float period = sampleRate / f0;
    const float omega = twoPi * f0 * inverseSampleRate_;

    // An open string that nobody is touching rings longer than a fretted one,
    // but it still follows the string set, its age, and the bridge hand.
    float t60 = spec.t60Seconds
              * lerp(1.08f, 0.38f, parameters.stringAge)
              * lerp(0.88f, 1.18f, parameters.stringGauge);
    if (palmMuteBlend_ > 0.0f)
        t60 = std::exp(lerp(std::log(t60), std::log(0.080f), palmMuteBlend_));
    t60 = clampf(t60, 0.03f, 9.5f);

    // A gentle fixed high-frequency loss: the coupled ring is a low-level
    // detail, so it needs no second bisection solve.
    auto& loop = voice.vertical;
    loop.loopDampingCoefficient = clampf(
        lerp(0.34f, 0.72f, parameters.stringAge), 0.0f, 0.9f);
    loop.dispersionLowCoefficient = 0.0f;
    loop.dispersionHighCoefficient = 0.0f;
    const float magnitudeAtF0 = (1.0f - loop.loopDampingCoefficient)
        / std::sqrt(std::max(
            1.0f + loop.loopDampingCoefficient * loop.loopDampingCoefficient
                - 2.0f * loop.loopDampingCoefficient * std::cos(omega),
            1.0e-12f));
    const float gainAtF0 = std::pow(10.0f, -3.0f * period / (t60 * sampleRate));
    loop.loopGain = clampf(gainAtF0 / std::max(magnitudeAtF0, 1.0e-6f),
                           0.0f, 0.9999f);

    voice.sympatheticDelay = clampf(
        period - onePolePhaseDelay(loop.loopDampingCoefficient, omega),
        4.0f, static_cast<float>(delayLineSize - 8));
    loop.targetDelay = voice.sympatheticDelay;
    loop.delaySmoothing = 1.0f - std::exp(-static_cast<float>(controlPeriod)
                                          / (0.006f * sampleRate));
    // A string that is already ringing glides to its new tuning; a freshly
    // woken one starts there, so a build change never clicks the ring.
    if (! voice.sympatheticReady)
        loop.currentDelay = voice.sympatheticDelay;

    const float bridgeDistance = lerp(lesPaulBridgePickupMetres,
                                      telecasterBridgePickupMetres,
                                      parameters.pickupType);
    const float bridgeFraction = clampf(bridgeDistance / scaleLengthMetres(),
                                        0.01f, 0.95f);
    voice.sympatheticPickupDelay = clampf(bridgeFraction * period, 2.0f,
                                          static_cast<float>(delayLineSize - 8));
}

void ElectryEngine::startExcitation(Voice& voice, float velocity, bool legato) noexcept
{
    (void) velocity; // The note-on velocity is cached in velocityProfile.
    const auto& parameters = smoothedParameters_;
    const float sampleRate = static_cast<float>(sampleRate_);
    const auto& profile = voice.velocityProfile;
    // A stiffer pick is a little louder, but most of what it changes is the
    // spectrum: the release corner below carries that, and leaving a wide
    // amplitude range here as well turned Pick Hardness into a level control.
    const float hardnessGain = lerp(0.98f, 1.26f, parameters.pickHardness);

    float amplitude = 0.48f * profile.amplitude * hardnessGain
                    * lerp(1.0f, 0.90f, parameters.stringAge);
    // The string leaves the pick in a fraction of a millisecond; the width
    // controls how much of the upper spectrum the release step carries.
    float pulseMs = lerp(1.15f, 0.10f, parameters.pickHardness)
                  * lerp(1.55f, 0.48f, profile.effortCurve);
    float pulseCutoff = lerp(900.0f, 13000.0f, parameters.pickHardness);
    float pluckFraction = lerp(0.025f, 0.48f, parameters.pickPosition);
    const float pickControl = std::pow(parameters.pickNoise, 0.75f);
    const float fingerControl = std::pow(parameters.fingerNoise, 0.75f);
    float noiseLevel = pickControl * (0.12f + 0.63f * profile.noise);
    noiseLevel += fingerControl * (voice.fret > 0 ? 0.055f : 0.012f)
                * profile.noise;
    float noiseMs = lerp(4.8f, 0.8f, parameters.pickHardness);
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    float noiseCutoff = spec.wound ? 2100.0f : 4800.0f;
    float modalBrightness = 1.0f;
    // Hammer-ons and taps are fingered: their contact noise is a fingertip on
    // the fingerboard, so the plectrum's hardness must not colour it.
    bool plectrumContact = true;
    voice.excitationPolarity = 1.0f;

    const auto applyUpstrokeVoicing = [&]
    {
        amplitude *= 0.92f;
        pulseMs *= 0.58f;
        pulseCutoff *= 2.00f;
        // The upstroke contact point sits a little closer to the bridge,
        // thinning and brightening the stroke.
        pluckFraction = clampf(pluckFraction - 0.020f, 0.03f, 0.45f);
        noiseMs *= 0.8f;
        noiseCutoff *= 1.2f;
        modalBrightness *= 1.42f;
        voice.excitationPolarity = -1.0f;
    };

    switch (voice.articulation)
    {
        case Articulation::Downstroke:
            break;
        case Articulation::Upstroke:
            applyUpstrokeVoicing();
            break;
        case Articulation::AlternateStroke:
            // AlternateStroke is resolved to a concrete Down/Up stroke in
            // noteOn(), while the latched articulation remains unchanged.
            break;
        case Articulation::HammerOn:
            amplitude *= legato ? 0.30f : 0.42f;
            pulseMs *= 1.9f;
            pulseCutoff *= 0.42f;
            pluckFraction = 0.12f;
            noiseLevel = 0.10f * fingerControl
                       * (0.10f + 0.30f * profile.noise);
            noiseMs = 1.6f;
            noiseCutoff = 185.0f;
            plectrumContact = false;
            modalBrightness *= 0.25f;
            break;
        case Articulation::Tap:
            amplitude *= 0.50f;
            pulseMs *= 1.55f;
            pulseCutoff *= 0.48f;
            pluckFraction = 0.11f;
            noiseLevel = fingerControl * (0.14f + 0.34f * profile.noise);
            noiseMs = 1.9f;
            noiseCutoff = 1150.0f;
            plectrumContact = false;
            modalBrightness *= 0.72f;
            break;
        case Articulation::Muted:
            amplitude *= 0.88f;
            pulseMs *= 1.2f;
            pulseCutoff *= 0.72f;
            pluckFraction *= 0.8f;
            noiseLevel *= 1.5f;
            modalBrightness *= 0.74f;
            break;
        case Articulation::Chug:
            amplitude *= 1.05f;
            pulseMs *= 0.58f;
            // A chug is tighter and harder than the general Muted style, but it
            // is still a hand on the string: it cannot be brighter than an open
            // note, which is what a multiplier above one made it.
            pulseCutoff *= 0.88f;
            pluckFraction = clampf(pluckFraction * 0.58f, 0.03f, 0.24f);
            noiseLevel *= 1.75f;
            noiseMs *= 0.62f;
            noiseCutoff *= 0.86f;
            modalBrightness *= 0.94f;
            break;
        case Articulation::DeadNote:
            amplitude *= 0.72f;
            pulseMs *= 0.42f;
            pulseCutoff *= 0.62f;
            pluckFraction = 0.08f;
            noiseLevel = 0.10f + 0.42f
                * (0.55f * parameters.fingerNoise + 0.45f * parameters.pickNoise)
                * (0.3f + 0.7f * profile.noise);
            noiseMs = 5.2f;
            noiseCutoff = spec.wound ? 1650.0f : 2800.0f;
            break;
        case Articulation::NaturalHarmonic:
            amplitude *= 0.62f;
            pulseMs *= 0.50f;
            pulseCutoff *= 1.65f;
            pluckFraction = 0.31f;
            noiseLevel *= 0.55f;
            noiseMs *= 0.7f;
            noiseCutoff *= 1.35f;
            modalBrightness *= 1.48f;
            break;
        case Articulation::PinchHarmonic:
            amplitude *= 0.80f;
            pulseMs *= 0.38f;
            pulseCutoff *= 1.95f;
            pluckFraction = 0.13f;
            noiseLevel *= 1.22f;
            noiseMs *= 0.55f;
            noiseCutoff *= 1.55f;
            modalBrightness *= 1.75f;
            break;
        case Articulation::Tremolo:
            amplitude *= 0.76f;
            pulseMs *= 0.55f;
            pulseCutoff *= 1.15f;
            noiseLevel *= 0.62f;
            noiseMs *= 0.58f;
            if (voice.tremoloStrokeIsUp)
                applyUpstrokeVoicing();
            break;
        case Articulation::Bend1Up:
        case Articulation::Bend2Up:
        case Articulation::Bend1Down:
        case Articulation::Bend2Down:
            break;
        case Articulation::Slap:
            amplitude *= 1.35f;
            pulseMs *= 0.30f;
            pulseCutoff *= 2.3f;
            pluckFraction = 0.16f;
            noiseLevel *= 1.35f;
            noiseCutoff = 5200.0f;
            noiseMs *= 0.7f;
            modalBrightness *= 2.0f;
            break;
    }

    // The heel of the hand is already resting on the string when the pick
    // reaches it, so it absorbs the attack's high end as the pluck forms rather
    // than only afterwards. Without this the bridge hand changes how long a
    // note lasts and almost nothing about its tone: measured, a palm-muted
    // chord's attack centroid sat within one percent of an open one's, which is
    // why a muted note read as a short pick rather than as a muted note. The
    // reference recordings' muted attacks sit around 1.4-1.7 kHz where an
    // unmuted Electry attack sits at 2.4 kHz, with an order of magnitude less
    // energy above 2.6 kHz.
    //
    // The depths match the values the sympathetic bus already uses for the same
    // hand, so one bridge hand damps the played string, its own attack, and the
    // strings it is covering by the same amount.
    float handDamping = palmMuteBlend_;
    if (voice.articulation == Articulation::Muted)
        handDamping = std::max(handDamping, 0.55f + 0.45f * parameters.muteDamping);
    else if (voice.articulation == Articulation::Chug)
        handDamping = std::max(handDamping, 0.75f + 0.25f * parameters.muteDamping);
    else if (voice.articulation == Articulation::DeadNote)
        handDamping = std::max(handDamping, 0.95f);
    handDamping = clampf(handDamping, 0.0f, 1.0f);
    // A hand of exactly zero pressure leaves every factor below at one.
    pulseCutoff *= lerp(1.0f, 0.26f, handDamping);
    noiseCutoff *= lerp(1.0f, 0.40f, handDamping);

    // Player effort and the guitar build change more than level: hard notes
    // are shorter/brighter at the contact, old strings lose the initial edge,
    // and the low Drop-E strings receive a little extra definition.
    const float lowString = 1.0f
        - static_cast<float>(voice.stringIndex) / static_cast<float>(stringCount - 1);
    pulseCutoff *= profile.brightness
                 * lerp(1.08f, 0.42f, parameters.stringAge)
                 * lerp(0.92f, 1.12f, parameters.construction)
                 * (1.0f + 0.24f * lowString);
    noiseCutoff *= lerp(0.72f, 1.22f, profile.effortCurve)
                 * (plectrumContact
                        ? lerp(0.55f, 1.75f, parameters.pickHardness) : 1.0f)
                 * (1.0f + 0.12f * lowString);

    // Most of a real pluck's sustained tone comes from the triangular string
    // displacement present when the pick lets go.  Its modal coefficients
    // fall as 1/n^2. Ordinary sustained picks keep only a much smaller short
    // release edge; feeding the whole attack through that pulse made the
    // position comb and induced-EMF derivative compound into a clavinet-like
    // high-partial tilt. Percussive styles may intentionally weight it higher.
    float displacementGain = 1.55f;
    float transientGain = lerp(0.0006f, 0.0025f, parameters.pickHardness)
                        * lerp(0.88f, 1.08f, profile.effortCurve);
    switch (voice.articulation)
    {
        case Articulation::HammerOn:
            displacementGain = 0.72f;
            transientGain = 0.0f;
            break;
        case Articulation::Tap:
            displacementGain = 0.38f;
            transientGain = 0.008f;
            break;
        case Articulation::Muted:
            displacementGain = 1.28f;
            transientGain *= 1.10f;
            break;
        case Articulation::Chug:
            displacementGain = 1.20f;
            transientGain *= 1.35f;
            break;
        case Articulation::DeadNote:
            displacementGain = 0.0f;
            transientGain = 0.72f;
            break;
        case Articulation::NaturalHarmonic:
            displacementGain = 0.30f;
            transientGain = 0.42f;
            break;
        case Articulation::PinchHarmonic:
            displacementGain = 0.18f;
            transientGain = 0.68f;
            break;
        case Articulation::Tremolo:
            displacementGain = 1.18f;
            transientGain *= 1.20f;
            break;
        case Articulation::Slap:
            displacementGain = 0.62f;
            transientGain = 0.42f;
            break;
        default:
            break;
    }

    // The plectrum's edge is the sharpest thing in the attack, so it is what the
    // heel of the hand absorbs most completely. Narrowing only its bandwidth
    // left it dominating the 1-3 kHz band of a muted note - measured, it was
    // 2.6x the modal path's level at 2 kHz - so the hand changed how long a
    // note lasted without changing its tone at all. This is the term that makes
    // a muted note sound muted rather than merely short.
    transientGain *= lerp(1.0f, 0.10f, handDamping);

    // A compact excitation is projected into a delay line whose modal DFT
    // contains `period` samples.  Without compensating for that length, the
    // same release area creates a modal displacement proportional to 1/N:
    // open E1 was consequently about 18 dB low in the waveguide and roughly
    // 26 dB below open E4 at the default output.
    // Referencing the projection to the high open E keeps equal player effort
    // at a comparable *string displacement* throughout the eight-string
    // range.  This is the normalization that a full triangular delay-line
    // initial condition would obtain automatically.
    constexpr float projectionReferenceHz = 329.62756f; // open E4
    const float modalProjectionGain = clampf(
        projectionReferenceHz / std::max(voice.baseFrequency, 20.0f),
        0.24f, 8.25f);
    voice.excitationAmplitude = amplitude * displacementGain
                              * modalProjectionGain;
    voice.excitationTransientAmplitude = amplitude * transientGain
                                       * modalProjectionGain;
    voice.excitationLength = std::max(
        8, static_cast<int>(pulseMs * 0.001f * sampleRate));
    const int contactSamples = legato
        ? 0
        : std::max(4, static_cast<int>(lerp(3.0f, 0.55f, parameters.pickHardness)
                                       * 0.001f * sampleRate));

    // Contact loss is a total attenuation over the complete pick/string
    // engagement, not ten percent every oversampled frame.  The old 0.90
    // per-sample multiplier erased essentially all energy on a repick.
    float contactRetention = lerp(0.88f, 0.64f, parameters.pickHardness);
    if (voice.articulation == Articulation::Muted)
        contactRetention *= 0.82f;
    else if (voice.articulation == Articulation::Chug)
        contactRetention *= 0.68f;
    else if (voice.articulation == Articulation::DeadNote)
        contactRetention *= 0.48f;
    voice.contactFeedbackGain = contactSamples > 0
        ? std::pow(clampf(contactRetention, 0.20f, 1.0f),
                   1.0f / static_cast<float>(contactSamples))
        : 1.0f;
    voice.excitationRemaining = voice.excitationLength;
    voice.excitationPhase = legato ? ExcitationPhase::Release
                                   : ExcitationPhase::Contact;
    if (! legato)
        voice.excitationRemaining = contactSamples;

    // The picking hand stays at a fixed distance from the bridge; it does not
    // follow the fretting hand up the neck. `pluckFraction` is therefore a
    // fraction of the *open* string, and the comb position as a fraction of
    // the sounding length grows by 2^(fret/12) as the note is fretted higher.
    // At the nut this reproduces the previous behaviour exactly, while a note
    // at the twelfth fret is now picked at twice the relative distance, which
    // is what moves a high fretted note toward the hollow, mid-string comb of
    // a real guitar.
    const float fretStretch = std::exp2(static_cast<float>(voice.fret) / 12.0f);
    voice.excitationCombDelay = clampf(pluckFraction * fretStretch, 0.02f, 0.49f)
                              * voice.vertical.targetDelay;

    // A plectrum touches the string over a patch, not at a point: half a
    // millimetre of contact for a stiff sharp pick, around a millimetre and a
    // half for a soft rounded one. Mapped through the same sounding-length
    // geometry the comb itself uses, that width is a little over one sample on
    // an open Drop-E eighth string and a small fraction of one at the top of
    // the range, which is exactly the frequency dependence a real contact has.
    const float contactMetres = 0.001f * lerp(1.5f, 0.5f, parameters.pickHardness);
    const float soundingMetres = std::max(scaleLengthMetres() / fretStretch,
                                          0.05f);
    voice.excitationCombWidth = 0.5f * (contactMetres / soundingMetres)
                              * voice.vertical.targetDelay;

    // The pick draws the string aside over most of the contact and then slips
    // off it: the release edge is several times faster than the load, and a
    // stiffer pick lets go later and more abruptly.
    const float slipPoint = lerp(0.62f, 0.82f, parameters.pickHardness);
    voice.excitationLoadScale = 1.0f / slipPoint;
    voice.excitationSlipScale = 1.0f / (1.0f - slipPoint);

    voice.excitationPulseCoefficient = std::exp(
        -twoPi * clampf(pulseCutoff, 300.0f, 0.45f * sampleRate) * inverseSampleRate_);

    // Project the pick release onto the string modes instead of treating it
    // as a broadband impulse. Two string-scaled low-pass sections supply the
    // 1/n^2 high-mode falloff of a triangular released displacement while the
    // position comb below retains the actual pluck location. The separate
    // broad path supplies the plectrum edge, not the sustained tone.
    const float modalCutoff = clampf(
        voice.baseFrequency
            * lerp(0.55f, 1.36f, parameters.pickHardness)
            * lerp(0.90f, 1.12f, profile.effortCurve),
        28.0f, std::min(900.0f, 0.20f * sampleRate));
    const float articulationModalCutoff = clampf(
        modalCutoff * modalBrightness,
        28.0f, std::min(900.0f, 0.20f * sampleRate));
    voice.excitationModalCoefficient = std::exp(
        -twoPi * articulationModalCutoff * inverseSampleRate_);
    // The plectrum does not release every string equally quickly: a wound .080
    // carries an order of magnitude more mass per unit length than a plain
    // .009 at comparable tension, so it leaves the pick more slowly, and the
    // duration of that release low-passes what enters the string. One further
    // pole supplies it, with a corner that follows the square root of the
    // string's own open frequency so the treble register keeps its brightness
    // while the wound low strings lose the upper-partial excess.
    //
    // The ideal 1/n^2 release the two modal sections above implement is what a
    // point pluck of a perfectly flexible string would give, and the pickup's
    // induced-EMF differentiation turns that into a 1/n voltage spectrum. A dry
    // low-E reference recording falls roughly 9 dB faster than 1/n by its
    // fourteenth partial, and that surplus is what made the low register sound
    // nasal and clavinet-like. This corner is therefore calibrated against that
    // reference rather than derived; the research contract records it as
    // voicing.
    // A stiff pick, a hard stroke and a bright articulation all shorten the
    // release, so the corner follows the same three factors the rest of the
    // excitation does - a fingered hammer-on leaves the string far more slowly
    // than a slapped one. `modalBrightness` is the per-style factor already
    // resolved above, so the styles stay as distinct as they were.
    const float openFrequency = midiToHz(static_cast<float>(spec.openMidiNote));
    const float stringReleaseCutoff =
        330.0f * std::sqrt(openFrequency / 82.4069f)
               * lerp(0.32f, 2.70f, parameters.pickHardness)
               * lerp(0.72f, 1.45f, profile.effortCurve)
               * lerp(1.12f, 0.72f, parameters.stringAge);
    // The bridge hand darkens the release, but deliberately not the reflected
    // image: the image's loss comes from the extra distance it travels through
    // the string, and lowering its corner leaves more of the direct wave's high
    // end uncancelled - which brightens exactly the notes the hand is supposed
    // to be dulling. Coupling the two cancelled the mute's whole effect on the
    // attack spectrum.
    const float releaseCutoff = clampf(
        stringReleaseCutoff * modalBrightness * lerp(1.0f, 0.30f, handDamping),
        60.0f, std::min(9000.0f, 0.30f * sampleRate));
    voice.excitationReleaseCoefficient = std::exp(
        -twoPi * releaseCutoff * inverseSampleRate_);
    voice.excitationImageCoefficient = std::exp(
        -twoPi * clampf(stringReleaseCutoff, 60.0f,
                        std::min(9000.0f, 0.30f * sampleRate))
        * inverseSampleRate_);

    // The duration of the release governs the excitation's spectrum, not how
    // hard the note lands, so the pole's own attenuation at the sounding
    // fundamental is divided back out. Without this, reaching for Pick
    // Hardness or playing harder would change the loudness as much as the
    // timbre.
    const float releaseRatio = 2.0f * voice.baseFrequency / releaseCutoff;
    voice.excitationAmplitude *= clampf(
        std::sqrt(1.0f + releaseRatio * releaseRatio), 1.0f, 6.0f);

    voice.excitationTailLength = std::clamp(
        static_cast<int>(8.0f * sampleRate
                         / (twoPi * std::min(articulationModalCutoff,
                                             releaseCutoff))),
        16, static_cast<int>(0.075f * sampleRate));
    voice.excitationShaper.reset();
    voice.excitationModalShaper1.reset();
    voice.excitationModalShaper2.reset();
    voice.excitationReleaseShaper.reset();
    voice.excitationImageShaper.reset();
    voice.noiseBandCoefficient = std::exp(
        -twoPi * clampf(noiseCutoff, 250.0f, 0.45f * sampleRate) * inverseSampleRate_);
    voice.noiseShaper.state = 0.0f;
    voice.noiseBandState = 0.0f;
    voice.noiseAmplitude = 0.75f * noiseLevel;
    voice.noiseLength = std::max(8, static_cast<int>(noiseMs * 0.001f * sampleRate));
    voice.noiseRemaining = voice.noiseLength;
    voice.releaseNoiseDone = false;
    updateArticulationWeights(voice);
}

void ElectryEngine::updateArticulationWeights(Voice& voice) noexcept
{
    // Articulations attack the string at different angles, splitting energy
    // differently between the two polarisations, and a perceptual makeup keeps
    // keyswitch changes usable inside one phrase while the excitation spectra
    // and envelopes remain strongly different. Both used to be re-selected by
    // a switch on every rendered sample; they only change at an attack.
    float verticalWeight = 0.92f;
    float horizontalWeight = 0.42f;
    float makeup = 1.0f;
    switch (voice.articulation)
    {
        case Articulation::Upstroke:
            verticalWeight = 0.90f; horizontalWeight = 0.46f; makeup = 1.06f;
            break;
        case Articulation::HammerOn:
            verticalWeight = 0.95f; horizontalWeight = 0.28f; makeup = 1.48f;
            break;
        case Articulation::Tap:
            verticalWeight = 0.96f; horizontalWeight = 0.24f; makeup = 1.30f;
            break;
        case Articulation::Muted:
            makeup = 1.18f;
            break;
        case Articulation::Chug:
            verticalWeight = 0.88f; horizontalWeight = 0.50f;
            break;
        case Articulation::DeadNote:
            verticalWeight = 0.76f; horizontalWeight = 0.62f; makeup = 1.16f;
            break;
        case Articulation::NaturalHarmonic:
            verticalWeight = 0.86f; horizontalWeight = 0.52f; makeup = 1.34f;
            break;
        case Articulation::PinchHarmonic:
            verticalWeight = 0.98f; horizontalWeight = 0.30f; makeup = 1.18f;
            break;
        case Articulation::Tremolo:
            if (voice.tremoloStrokeIsUp)
            {
                verticalWeight = 0.90f;
                horizontalWeight = 0.46f;
            }
            makeup = 1.12f;
            break;
        case Articulation::Slap:
            verticalWeight = 1.00f; horizontalWeight = 0.22f; makeup = 0.84f;
            break;
        default:
            break;
    }
    voice.verticalWeight = verticalWeight;
    voice.horizontalWeight = horizontalWeight;
    voice.articulationMakeup = makeup;
}

void ElectryEngine::startVoice(Voice& voice, int midiNote, float velocity,
                               Articulation articulation,
                               int startDelaySamples) noexcept
{
    const auto& parameters = smoothedParameters_;
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    const int fret = midiNote - spec.openMidiNote;

    const bool wasRinging = voice.active;

    // The fretting hand stops a sympathetically ringing open string before the
    // pick reaches it, so a coupled ring never survives into the played note.
    // A coupled string that has already fallen below audibility needs no work.
    if (! wasRinging && voice.sympatheticReady
        && voice.sympatheticEnergy > 1.0e-11f)
    {
        for (auto& sample : voice.vertical.line)
            sample *= 0.22f;
        voice.sympatheticEnergy = 0.0f;
        voice.sympatheticPreviousFlux = 0.0f;
        voice.sympatheticEmf.reset();
    }
    voice.sympatheticReady = false;

    voice.active = true;
    voice.keyDown = true;
    voice.sustained = false;
    voice.releasing = false;
    voice.midiNote = midiNote;
    voice.fret = std::clamp(fret, 0, fretCount);
    voice.articulation = articulation;
    voice.velocity = velocity;
    voice.velocityProfile = makeVelocityProfile(velocity);
    voice.startOrder = ++noteSequence_;
    voice.ageSamples = 0;
    voice.noiseState = hash32(static_cast<std::uint32_t>(voice.stringIndex * 7349)
                              ^ static_cast<std::uint32_t>(midiNote * 131)
                              ^ static_cast<std::uint32_t>(
                                  static_cast<int>(articulation) * 17)
                              ^ static_cast<std::uint32_t>(noteSequence_ * 2654435761u));
    voice.artifactNoiseState = hash32(voice.noiseState ^ 0xa53c9e17u);
    int harmonicOffset = 0;
    if (articulation == Articulation::NaturalHarmonic)
        harmonicOffset = 12;
    else if (articulation == Articulation::PinchHarmonic)
        harmonicOffset = 19;
    voice.baseFrequency = midiToHz(static_cast<float>(midiNote + harmonicOffset));

    // Each physical string has a slightly different saddle/bridge rattle.
    // Its variation is seeded by the note sequence, never by wall-clock time,
    // so renders remain reproducible while consecutive notes are not clones.
    const float rattleVariation = 0.03f
        * bipolarNoise(voice.artifactNoiseState);
    const float rattleFrequency = (1600.0f + 420.0f
        * static_cast<float>(voice.stringIndex)) * (1.0f + rattleVariation);
    voice.saddleRattle.reset();
    voice.saddleRattle.configure(
        rattleFrequency,
        lerp(10.0f, 24.0f, smoothedParameters_.artifactAmount), 1.0f,
        static_cast<float>(sampleRate_));
    voice.artifactNoiseShaper.reset();
    voice.artifactNoiseCoefficient = std::exp(
        -twoPi * (3400.0f + 280.0f * static_cast<float>(voice.stringIndex))
        * inverseSampleRate_);
    voice.artifactNoiseBandState = 0.0f;
    voice.artifactCollisionCount = 0;
    voice.legatoBlend = 1.0f;
    voice.legatoFromFrequency = 0.0f;
    voice.releaseGain = 1.0f;
    voice.releaseGainTarget = 1.0f;
    voice.releaseGainCoefficient = 0.0f;
    if (! wasRinging)
    {
        voice.energyEnvelope = 0.0f;
        voice.outputEnergy = 0.0f;
    }
    voice.tremoloStrokeIsUp = false;
    voice.tremoloRetriggerCount = 0;
    voice.tremoloSamplesUntilRetrigger = articulation == Articulation::Tremolo
        ? std::max(controlPeriod,
                   static_cast<int>(0.075f * static_cast<float>(sampleRate_)))
        : 0;

    // Bend programs: upward bends start on the played note and travel up;
    // downward bends start above and release onto the played note.
    voice.bendStartSemitones = 0.0f;
    voice.bendTargetSemitones = 0.0f;
    switch (articulation)
    {
        case Articulation::Bend1Up: voice.bendTargetSemitones = 1.0f; break;
        case Articulation::Bend2Up: voice.bendTargetSemitones = 2.0f; break;
        case Articulation::Bend1Down: voice.bendStartSemitones = 1.0f; break;
        case Articulation::Bend2Down: voice.bendStartSemitones = 2.0f; break;
        default: break;
    }
    const bool bending = voice.bendStartSemitones != voice.bendTargetSemitones;
    voice.bendProgress = bending ? 0.0f : 1.0f;
    voice.bendHoldSamples = bending
        ? static_cast<int>(0.055f * static_cast<float>(sampleRate_))
        : 0;
    voice.bendIncrement = bending
        ? static_cast<float>(controlPeriod)
          / (clampf(parameters.bendTimeSeconds, 0.04f, 2.0f)
             * static_cast<float>(sampleRate_))
        : 0.0f;

    // Tension-modulation depth: lighter strings glide more, slaps push the
    // string much harder against its tension.
    voice.tensionDepth = 0.042f * lerp(1.45f, 0.70f, parameters.stringGauge)
                       * voice.velocityProfile.tension;
    if (articulation == Articulation::Slap)
        voice.tensionDepth *= 14.0f;

    // Slap drives the string against the frets: a decaying collision window
    // soft-limits displacement and adds rattle noise.
    if (articulation == Articulation::Slap)
    {
        const float collision = voice.velocityProfile.collision;
        voice.collisionRemaining = static_cast<int>(
            lerp(0.055f, 0.100f, collision) * static_cast<float>(sampleRate_));
        voice.collisionThreshold = lerp(0.40f, 0.20f, collision);
    }
    else
    {
        voice.collisionRemaining = 0;
        voice.collisionThreshold = 1.0f;
    }

    const bool incidentalContact = articulation != Articulation::Slap
                                && articulation != Articulation::HammerOn;
    if (incidentalContact && parameters.artifactAmount > 0.0f)
    {
        const float lowString = 1.0f
            - static_cast<float>(voice.stringIndex)
              / static_cast<float>(stringCount - 1);
        const float contact = std::pow(parameters.artifactAmount, 0.75f)
                            * voice.velocityProfile.collision
                            * (0.70f + 0.30f * lowString);
        voice.artifactCollisionLength = static_cast<int>(
            lerp(0.025f, 0.100f, voice.velocityProfile.collision)
            * static_cast<float>(sampleRate_));
        voice.artifactCollisionRemaining = voice.artifactCollisionLength;
        voice.artifactClearance = lerp(0.52f, 0.24f, contact);
    }
    else
    {
        voice.artifactCollisionRemaining = 0;
        voice.artifactCollisionLength = 0;
        voice.artifactClearance = 1.0f;
    }

    configureVoiceDamping(voice);
    configureVoicePitch(voice, ! wasRinging);
    configureVoicePickups(voice);

    // A retriggered string keeps ringing through the new pick contact, but a
    // large pitch jump on a stolen string is choked first, as a player's
    // fresh grip would.
    if (wasRinging)
    {
        const float relativeJump = std::abs(voice.vertical.targetDelay
                                            - voice.vertical.currentDelay)
                                 / std::max(voice.vertical.currentDelay, 1.0f);
        if (relativeJump > 0.25f)
        {
            for (auto* loop : { &voice.vertical, &voice.horizontal })
            {
                for (auto& sample : loop->line)
                    sample *= 0.28f;
            }
            voice.vertical.currentDelay = voice.vertical.targetDelay;
            voice.horizontal.currentDelay = voice.horizontal.targetDelay;
        }
    }

    // A strummed chord's later strings are already fretted and choked, but the
    // pick has not reached them yet. The excitation fires from the control
    // tick once the travel time has elapsed.
    voice.startDelaySamples = std::max(0, startDelaySamples);
    updateArticulationWeights(voice);
    if (voice.startDelaySamples == 0)
        startExcitation(voice, velocity, false);
}

void ElectryEngine::legatoRetarget(Voice& voice, int midiNote, float velocity) noexcept
{
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    voice.legatoFromFrequency = voice.baseFrequency
        * std::exp2(lerp(voice.bendStartSemitones, voice.bendTargetSemitones,
                         smoothStep(voice.bendProgress)) / 12.0f);
    voice.midiNote = midiNote;
    voice.fret = std::clamp(midiNote - spec.openMidiNote, 0, fretCount);
    voice.articulation = Articulation::HammerOn;
    voice.baseFrequency = midiToHz(static_cast<float>(midiNote));
    voice.keyDown = true;
    voice.sustained = false;
    voice.releasing = false;
    voice.startOrder = ++noteSequence_;
    voice.velocity = velocity;
    voice.velocityProfile = makeVelocityProfile(velocity);
    voice.bendStartSemitones = 0.0f;
    voice.bendTargetSemitones = 0.0f;
    voice.bendProgress = 1.0f;
    voice.tremoloSamplesUntilRetrigger = 0;
    voice.tremoloRetriggerCount = 0;
    voice.tremoloStrokeIsUp = false;
    voice.releaseGain = 1.0f;
    voice.releaseGainTarget = 1.0f;
    voice.artifactCollisionRemaining = 0;
    voice.startDelaySamples = 0;

    // The finger lands over roughly ten milliseconds rather than instantly.
    voice.legatoBlend = 0.0f;
    voice.legatoIncrement = static_cast<float>(controlPeriod)
        / (0.010f * static_cast<float>(sampleRate_));

    configureVoiceDamping(voice);
    configureVoicePitch(voice, false);
    configureVoicePickups(voice);
    startExcitation(voice, velocity, true);
}

void ElectryEngine::beginVoiceRelease(Voice& voice) noexcept
{
    if (! voice.active || voice.releasing)
        return;
    voice.releasing = true;
    voice.sustained = false;

    // A strum spreads the pick across the strings by up to 280 ms. If the key
    // is lifted before the pick arrives, it never lands: leaving the countdown
    // running would excite the string after the release and produce a late
    // attack on short notes.
    voice.startDelaySamples = 0;

    // The fretting or picking hand damps the string over tens of
    // milliseconds; the loop then decays with roughly a 60 ms T60.
    const float sampleRate = static_cast<float>(sampleRate_);
    const float period = sampleRate / std::max(voice.baseFrequency, 20.0f);
    voice.releaseGainTarget =
        std::pow(10.0f, -3.0f * period / (0.060f * sampleRate));
    voice.releaseGainCoefficient =
        1.0f - std::exp(-1.0f / (0.022f * sampleRate));

    if (! voice.releaseNoiseDone && smoothedParameters_.releaseNoise > 0.0f)
    {
        voice.releaseNoiseDone = true;
        const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
        // Reduced from 0.34/0.20 on listening feedback that the release burst was
        // simply too loud. This is a global voicing change, not a consequence of
        // the mute work: it applies to every articulation, and the earlier
        // justification here - that the restored muted tail had left the burst
        // sitting proud - would only have argued for a muted-only reduction.
        // The control's own range is untouched; what moved is what a given
        // setting means.
        const float level = std::pow(smoothedParameters_.releaseNoise, 0.75f)
                          * (spec.wound ? 0.20f : 0.13f)
                          * voice.velocityProfile.noise;
        voice.noiseAmplitude = level;
        const float releaseSeconds = lerp(
            0.006f, 0.015f,
            0.55f * smoothedParameters_.stringAge
                + 0.45f * smoothedParameters_.stringGauge);
        voice.noiseLength = std::max(
            8, static_cast<int>(releaseSeconds * sampleRate));
        voice.noiseRemaining = voice.noiseLength;
        voice.noiseBandCoefficient = std::exp(-twoPi * (spec.wound ? 1500.0f : 2600.0f)
                                              * inverseSampleRate_);
        voice.noiseShaper.reset();
        voice.noiseBandState = 0.0f;
    }
}

void ElectryEngine::silenceVoice(Voice& voice) noexcept
{
    voice.active = false;
    voice.keyDown = false;
    voice.sustained = false;
    voice.releasing = false;
    voice.midiNote = -1;
    voice.excitationPhase = ExcitationPhase::Idle;
    voice.excitationRemaining = 0;
    voice.excitationAmplitude = 0.0f;
    voice.excitationTransientAmplitude = 0.0f;
    voice.excitationTailLength = 0;
    voice.contactFeedbackGain = 1.0f;
    voice.noiseRemaining = 0;
    voice.collisionRemaining = 0;
    voice.artifactCollisionRemaining = 0;
    voice.artifactCollisionLength = 0;
    voice.artifactCollisionCount = 0;
    voice.tremoloSamplesUntilRetrigger = 0;
    voice.tremoloRetriggerCount = 0;
    voice.tremoloStrokeIsUp = false;
    voice.energyEnvelope = 0.0f;
    voice.outputEnergy = 0.0f;
    voice.displayLevel = 0.0f;
    voice.startDelaySamples = 0;
    voice.releaseGain = 1.0f;
    voice.releaseGainTarget = 1.0f;
    voice.releaseGainCoefficient = 0.0f;
    voice.legatoBlend = 1.0f;
    voice.bendProgress = 1.0f;
    voice.bendStartSemitones = 0.0f;
    voice.bendTargetSemitones = 0.0f;
    // Every retained filter state must clear with the voice, or a silenced
    // string could leak residue into a later, otherwise identical render and
    // break the engine's determinism contract.
    voice.apertureNeck.reset();
    voice.apertureBridge.reset();
    voice.excitationShaper.reset();
    voice.excitationModalShaper1.reset();
    voice.excitationModalShaper2.reset();
    voice.previousFluxNeck = 0.0f;
    voice.previousFluxBridge = 0.0f;
    voice.emfLowpassNeck.reset();
    voice.emfLowpassBridge.reset();
    voice.excitationShaper.reset();
    voice.noiseShaper.reset();
    voice.noiseBandState = 0.0f;
    voice.artifactNoiseShaper.reset();
    voice.artifactNoiseBandState = 0.0f;
    voice.saddleRattle.reset();
    voice.lastConfiguredSemitones = -999.0f;
    voice.lastConfiguredFrequency = -1.0f;
    voice.compensationDirty = true;
    // The string is free again: the next bridge-coupled excitation reconfigures
    // and clears the loop for its open pitch.
    voice.sympatheticReady = false;
    voice.sympatheticEnergy = 0.0f;
    voice.sympatheticPreviousFlux = 0.0f;
    voice.sympatheticEmf.reset();
}

// ---------------------------------------------------------------------------
// Shared path configuration
// ---------------------------------------------------------------------------

void ElectryEngine::configureBody() noexcept
{
    const auto& parameters = smoothedParameters_;
    const float sampleRate = static_cast<float>(sampleRate_);

    // Structural mode estimates for the two anchor bodies. These are
    // geometry-informed voicing values, not measured mode tables.
    static constexpr std::array<float, bodyModeCount> carvedModes {
        112.0f, 168.0f, 292.0f, 488.0f
    };
    static constexpr std::array<float, bodyModeCount> slabModes {
        92.0f, 220.0f, 420.0f, 690.0f
    };
    static constexpr std::array<float, bodyModeCount> modeLevels {
        1.0f, 0.68f, 0.46f, 0.32f
    };
    static constexpr std::array<float, bodyModeCount> mahoganyMapleTilt {
        1.20f, 0.95f, 0.60f, 0.35f
    };
    static constexpr std::array<float, bodyModeCount> ashTilt {
        0.72f, 0.82f, 1.15f, 1.55f
    };

    const float sizeScale = std::exp2((parameters.bodySize - 0.5f) * 0.65f);
    const float q = lerp(30.0f, 9.0f, parameters.bodyWood)
                  * lerp(1.12f, 0.82f, parameters.construction);
    const float sizeLevel = lerp(0.90f, 1.15f, parameters.bodySize);

    for (int mode = 0; mode < bodyModeCount; ++mode)
    {
        const auto index = static_cast<std::size_t>(mode);
        const float frequency = lerp(carvedModes[index], slabModes[index],
                                     parameters.bodyShape) * sizeScale;
        const float woodTilt = lerp(mahoganyMapleTilt[index], ashTilt[index],
                                    parameters.bodyWood);
        const float level = clampf(modeLevels[index] * woodTilt * sizeLevel,
                                   0.08f, 1.20f);
        bodyModeFrequencies_[index] = frequency;
        bodyModeQs_[index] = q;
        bodyModeLevels_[index] = level;
        bodyModes_[index].configure(frequency, q, level,
                                    sampleRate);
    }
}

float ElectryEngine::bodyConductanceAt(float frequencyHz) const noexcept
{
    // A normalised modal-conductance envelope. Near a structural mode the
    // bridge accepts more string energy; far from every mode it approaches
    // zero. This response is evaluated only while configuring a note, not in
    // the sample loop, and is used exclusively to add loss.
    float response = 0.0f;
    float normaliser = 0.0f;
    for (int mode = 0; mode < bodyModeCount; ++mode)
    {
        const auto index = static_cast<std::size_t>(mode);
        const float centre = std::max(bodyModeFrequencies_[index], 30.0f);
        const float q = std::max(bodyModeQs_[index], 2.0f);
        const float level = std::max(bodyModeLevels_[index], 0.0f);
        const float omega = twoPi * std::max(frequencyHz, 0.0f);
        const float omegaMode = twoPi * centre;
        const float damping = omegaMode / q;
        const float dissipative = damping * omega;
        const float reactive = omegaMode * omegaMode - omega * omega;
        // Real (conductive) part of a normalised modal mobility. It is
        // positive, bounded by one, and peaks exactly at the body mode;
        // unlike modal magnitude it does not over-damp distant notes.
        const float conductance = dissipative * dissipative
            / std::max(reactive * reactive + dissipative * dissipative,
                       1.0e-12f);
        response += level * conductance;
        normaliser += level;
    }
    return clampf(response / std::max(normaliser, 1.0e-6f), 0.0f, 1.0f);
}

void ElectryEngine::configurePickupFilters() noexcept
{
    const auto& parameters = smoothedParameters_;
    const float sampleRate = static_cast<float>(sampleRate_);

    float resonance = lerp(humbuckerResonanceHz, singleCoilResonanceHz,
                           parameters.pickupType);
    float q = lerp(humbuckerResonanceQ, singleCoilResonanceQ,
                   parameters.pickupType);

    // Selecting both pickups loads each coil with the other, moving the
    // combined resonance down slightly.
    if (parameters.pickupSelector == PickupSelector::Both)
        resonance *= 0.93f;

    // The passive tone control shifts the loaded resonance down and, as the
    // capacitor and pot take over, damps the resonant peak, so rolling the
    // tone off darkens rather than just relocating the hump.
    const float tone = parameters.toneKnob;
    resonance = lerp(600.0f, resonance, std::pow(tone, 1.10f));
    q *= 0.22f + 0.78f * tone;

    for (auto& filter : neckCoils_)
        filter.setResonantLowpass(resonance, q, sampleRate);
    for (auto& filter : bridgeCoils_)
        filter.setResonantLowpass(resonance, q, sampleRate);

    // Distance-dependent magnetic flux: hotter, closer humbuckers develop
    // more even-harmonic distortion than a low-wind single coil.
    magneticDriveNeck_ = lerp(0.62f, 0.24f, parameters.pickupType);
    magneticDriveBridge_ = lerp(0.68f, 0.27f, parameters.pickupType);
    // The flux polynomial normalises by its own drive; the reciprocal is a
    // control-rate constant, not a per-sample division.
    magneticDriveNeckInverse_ = 1.0f / std::max(magneticDriveNeck_, 1.0e-3f);
    magneticDriveBridgeInverse_ = 1.0f / std::max(magneticDriveBridge_, 1.0e-3f);

    switch (parameters.pickupSelector)
    {
        case PickupSelector::Neck:
            neckMixTarget_ = 1.0f;
            bridgeMixTarget_ = 0.0f;
            break;
        case PickupSelector::Both:
            neckMixTarget_ = 0.62f;
            bridgeMixTarget_ = 0.62f;
            break;
        case PickupSelector::Bridge:
            neckMixTarget_ = 0.0f;
            bridgeMixTarget_ = 1.0f;
            break;
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void ElectryEngine::updateVoiceControl(Voice& voice) noexcept
{
    if (! voice.active)
        return;

    // Advance the bend program.
    if (voice.bendProgress < 1.0f)
    {
        if (voice.bendHoldSamples > 0)
            voice.bendHoldSamples -= controlPeriod;
        else
            voice.bendProgress = clampf(voice.bendProgress + voice.bendIncrement,
                                        0.0f, 1.0f);
    }

    if (voice.legatoBlend < 1.0f)
        voice.legatoBlend = clampf(voice.legatoBlend + voice.legatoIncrement,
                                   0.0f, 1.0f);

    // Strum travel: the pick reaches this string after the programmed offset.
    if (voice.startDelaySamples > 0)
    {
        voice.startDelaySamples -= controlPeriod;
        if (voice.startDelaySamples <= 0)
        {
            voice.startDelaySamples = 0;
            startExcitation(voice, voice.velocity, false);
        }
    }

    if (voice.articulation == Articulation::Tremolo
        && voice.keyDown && ! voice.releasing)
    {
        voice.tremoloSamplesUntilRetrigger -= controlPeriod;
        if (voice.tremoloSamplesUntilRetrigger <= 0)
        {
            const int interval = std::max(
                controlPeriod,
                static_cast<int>(0.075f * static_cast<float>(sampleRate_)));
            voice.tremoloSamplesUntilRetrigger += interval;
            voice.tremoloStrokeIsUp = ! voice.tremoloStrokeIsUp;
            ++voice.tremoloRetriggerCount;
            startExcitation(voice, voice.velocity, false);
        }
    }

    configureVoicePitch(voice, false);

    // Ballistic display level for the fretboard readout. It is derived from
    // the same energy follower the retirement logic uses, so it costs one
    // square root and one one-pole per control tick.
    const float target = std::sqrt(std::max(voice.outputEnergy, 0.0f));
    const float ballistic = target > voice.displayLevel ? displayLevelAttack_
                                                        : displayLevelRelease_;
    voice.displayLevel += ballistic * (target - voice.displayLevel);

    // Retire the voice once the string has decayed below audibility and no
    // excitation remains pending.
    const bool excitationIdle = voice.excitationPhase == ExcitationPhase::Idle
                             && voice.noiseRemaining <= 0
                             && voice.startDelaySamples <= 0;
    if (excitationIdle && voice.outputEnergy < 1.0e-10f
        && voice.ageSamples > static_cast<std::uint64_t>(sampleRate_ * 0.05))
    {
        silenceVoice(voice);
    }
}

void ElectryEngine::renderVoice(Voice& voice, RenderSums& sums) noexcept
{
    auto& vertical = voice.vertical;
    auto& horizontal = voice.horizontal;

    // Loop reads and the damping/dispersion chain.
    const auto advanceLoop = [&] (PolarisationLoop& loop, float extraFeedback)
    {
        float sample = loop.readFractional(loop.currentDelay);
        sample = loop.dispersion1.process(sample, loop.dispersionLowCoefficient);
        sample = loop.dispersion2.process(sample, loop.dispersionLowCoefficient);
        sample = loop.dispersion3.process(sample, loop.dispersionLowCoefficient);
        sample = loop.dispersion4.process(sample, loop.dispersionLowCoefficient);
        sample = loop.dispersion5.process(sample, loop.dispersionHighCoefficient);
        sample = loop.dispersion6.process(sample, loop.dispersionHighCoefficient);
        sample = loop.dispersion7.process(sample, loop.dispersionHighCoefficient);
        sample = loop.dispersion8.process(sample, loop.dispersionHighCoefficient);
        sample = loop.damping.process(sample, loop.loopDampingCoefficient);
        // A convex blend of the wave and a lowpass of it, so the magnitude never
        // exceeds one whatever the depth, and depth zero is exactly the identity.
        if (loop.handLossDepth > 0.0f)
            sample += loop.handLossDepth
                    * (loop.handLoss.process(sample, loop.handLossCoefficient)
                       - sample);
        sample *= loop.loopGain * voice.releaseGain * extraFeedback;
        return sample;
    };

    // Pick contact briefly chokes an already ringing string.
    const float contactChoke = voice.excitationPhase == ExcitationPhase::Contact
        ? voice.contactFeedbackGain : 1.0f;

    float verticalSample = advanceLoop(vertical, contactChoke);
    float horizontalSample = advanceLoop(horizontal, contactChoke);

    // Slap window: displacement beyond the fret clearance is limited and
    // re-radiated as rattle.
    if (voice.collisionRemaining > 0)
    {
        voice.collisionRemaining--;
        const float progress = 1.0f - static_cast<float>(voice.collisionRemaining)
            / std::max(1.0f, 0.085f * static_cast<float>(sampleRate_));
        const float threshold = voice.collisionThreshold * (1.0f + 3.0f * progress);
        const float excess = std::abs(verticalSample) - threshold;
        if (excess > 0.0f)
        {
            const float sign = verticalSample > 0.0f ? 1.0f : -1.0f;
            verticalSample = sign * (threshold + excess / (1.0f + 6.0f * excess));
            verticalSample += 0.18f * excess * bipolarNoise(voice.noiseState);
        }
    }

    // Ordinary hard-picked notes can make brief, irregular fret contact too.
    // The Artifacts control blends toward the same bounded collision law used
    // by Slap, but a separate deterministic noise stream preserves the clean
    // engine's exact contact-noise sequence.
    float artifactContactSignal = 0.0f;
    float artifactExcess = 0.0f;
    if (artifactsActive_ && voice.artifactCollisionRemaining > 0)
    {
        --voice.artifactCollisionRemaining;
        const float progress = 1.0f
            - static_cast<float>(voice.artifactCollisionRemaining)
              / static_cast<float>(std::max(1, voice.artifactCollisionLength));
        const float clearance = voice.artifactClearance
                              * (1.0f + 3.5f * progress);
        artifactExcess = std::abs(verticalSample) - clearance;
        if (artifactExcess > 0.0f)
        {
            const float lowString = 1.0f
                - static_cast<float>(voice.stringIndex)
                  / static_cast<float>(stringCount - 1);
            const float contact = artifactContactShape_
                                * voice.velocityProfile.collision
                                * (0.70f + 0.30f * lowString);
            const float sign = verticalSample >= 0.0f ? 1.0f : -1.0f;
            const float limited = sign * (clearance
                + artifactExcess / (1.0f + 6.0f * artifactExcess));
            verticalSample = lerp(verticalSample, limited, contact);

            const float raw = bipolarNoise(voice.artifactNoiseState);
            const float lowpassed = voice.artifactNoiseShaper.process(
                raw, voice.artifactNoiseCoefficient);
            voice.artifactNoiseBandState += artifactBandCoefficient_
                * (lowpassed - voice.artifactNoiseBandState);
            artifactContactSignal = (lowpassed - voice.artifactNoiseBandState)
                                  * artifactExcess * 0.22f * contact;
            ++voice.artifactCollisionCount;
        }
    }

    // Passive bridge coupling exchanges a little energy between the two
    // polarisations; the mixing matrix is contractive, so it stays stable.
    constexpr float coupling = 0.004f;
    const float verticalIn = verticalSample
                           + coupling * (horizontalSample - verticalSample);
    const float horizontalIn = horizontalSample
                             + coupling * (verticalSample - horizontalSample);

    // Excitation: contact noise, then the pick-release pulse, then the tail.
    float excitation = 0.0f;
    float noiseSample = 0.0f;
    if (voice.noiseRemaining > 0)
    {
        voice.noiseRemaining--;
        const float window = static_cast<float>(voice.noiseRemaining)
                           / static_cast<float>(std::max(1, voice.noiseLength));
        const float raw = bipolarNoise(voice.noiseState);
        const float lowpassed = voice.noiseShaper.process(raw, voice.noiseBandCoefficient);
        // Band-shaped scrape: low-passed noise minus its own slower average.
        voice.noiseBandState += contactNoiseBandCoefficient_
                              * (lowpassed - voice.noiseBandState);
        noiseSample = (lowpassed - voice.noiseBandState)
                    * voice.noiseAmplitude * window * window;
    }

    if (voice.excitationPhase == ExcitationPhase::Contact)
    {
        if (--voice.excitationRemaining <= 0)
        {
            voice.excitationPhase = ExcitationPhase::Release;
            voice.excitationRemaining = voice.excitationLength;
        }
    }
    else if (voice.excitationPhase == ExcitationPhase::Release)
    {
        const float progress = 1.0f - static_cast<float>(voice.excitationRemaining)
            / static_cast<float>(std::max(1, voice.excitationLength));
        // Load, then slip. Both halves are smoothsteps, so the product is
        // continuous with a continuous derivative and its area over the window
        // is exactly one half whatever the slip point is - the same area the
        // symmetric raised cosine had, so the asymmetry changes the attack's
        // spectrum without changing how hard the note lands.
        const float load = smoothStep(progress * voice.excitationLoadScale);
        const float slip = smoothStep((1.0f - progress) * voice.excitationSlipScale);
        const float window = load * slip;
        const float releasePulse = window * voice.excitationPolarity;
        const float modal = voice.excitationReleaseShaper.process(
            voice.excitationModalShaper2.process(
                voice.excitationModalShaper1.process(
                    voice.excitationAmplitude * releasePulse,
                    voice.excitationModalCoefficient),
                voice.excitationModalCoefficient),
            voice.excitationReleaseCoefficient);
        const float edge = voice.excitationShaper.process(
            voice.excitationTransientAmplitude * releasePulse,
            voice.excitationPulseCoefficient);
        excitation = modal + edge;
        if (--voice.excitationRemaining <= 0)
        {
            voice.excitationPhase = ExcitationPhase::Tail;
            voice.excitationRemaining = voice.excitationTailLength;
        }
    }
    else if (voice.excitationPhase == ExcitationPhase::Tail)
    {
        const float modal = voice.excitationReleaseShaper.process(
            voice.excitationModalShaper2.process(
                voice.excitationModalShaper1.process(
                    0.0f, voice.excitationModalCoefficient),
                voice.excitationModalCoefficient),
            voice.excitationReleaseCoefficient);
        const float edge = voice.excitationShaper.process(
            0.0f, voice.excitationPulseCoefficient);
        excitation = modal + edge;
        if (--voice.excitationRemaining <= 0)
            voice.excitationPhase = ExcitationPhase::Idle;
    }

    // Pick/finger scrape is mostly a local, short contact voltage and body
    // impulse. Only a trace enters the freely ringing string. Sending the
    // entire noise burst around the delay loop let a few high modes sustain
    // for seconds, overwhelming the correct long-string release spectrum.
    const float injected = excitation + 0.01f * noiseSample;
    float saddleRattle = 0.0f;
    if (artifactsActive_)
    {
        const float rattleDrive = 0.10f * excitation
                                + 0.65f * artifactContactSignal
                                + 0.08f * artifactExcess;
        saddleRattle = voice.saddleRattle.process(rattleDrive);
    }

    // Articulations attack the string at different angles, splitting energy
    // differently between the two polarisations. The split is resolved once
    // per attack in updateArticulationWeights().
    const float verticalWeight = voice.verticalWeight;
    const float horizontalWeight = voice.horizontalWeight;

    const float verticalTotal = verticalIn + injected * verticalWeight
                              + 0.35f * artifactContactSignal;
    const float horizontalTotal = horizontalIn + injected * horizontalWeight
                                + 0.12f * artifactContactSignal;

    vertical.line[static_cast<std::size_t>(vertical.writeIndex
                                           & (delayLineSize - 1))] = verticalTotal;
    horizontal.line[static_cast<std::size_t>(horizontal.writeIndex
                                             & (delayLineSize - 1))] = horizontalTotal;

    // The pluck-position comb: the same excitation is scattered with opposite
    // sign one comb delay behind the write head, exactly as the second
    // travelling-wave image of the excitation point.
    if (injected != 0.0f)
    {
        // The image is distributed over the plectrum's contact patch with a
        // 1/4, 1/2, 1/4 kernel. The weights sum to one, so a zero width
        // reduces exactly to the previous single-point image, while a real
        // width washes the comb notches out with frequency instead of holding
        // them razor sharp all the way to Nyquist.
        //
        // It is also darker than the direct wave: it has travelled to the pick
        // and back through the same lossy string. The low-pass has unity DC
        // gain, so the comb still rejects a net displacement exactly - which
        // matters, because the loop deliberately carries no DC blocker - while
        // the high-order nulls stop cancelling perfectly. An exact copy gave
        // notches twenty decibels deep where the reference recording's comb
        // ripple is a few decibels.
        const float imageSource = voice.excitationImageShaper.process(
            injected, voice.excitationImageCoefficient);
        const float width = voice.excitationCombWidth;
        const float centre = voice.excitationCombDelay;
        for (auto* loop : { &vertical, &horizontal })
        {
            const float image = -imageSource
                * (loop == &vertical ? verticalWeight : horizontalWeight);
            loop->writeAdd(centre - width, 0.25f * image);
            loop->writeAdd(centre, 0.5f * image);
            loop->writeAdd(centre + width, 0.25f * image);
        }
    }

    // Pickups read the string displacement at their positions: the freshly
    // written bridge-bound sample minus its reflection image at the pickup
    // delay. The magnetic pole senses the perpendicular polarisation more
    // strongly than the parallel one. Taps are taken before the write index
    // advances so the interpolator never touches a stale slot.
    // Distance-dependent flux nonlinearity, second-order dominant. The
    // saturating pre-map bounds the polynomial's argument the way approaching
    // magnetic saturation actually bounds it: smoothly, with a unity slope and
    // an unchanged second-order term at small displacement. The previous hard
    // clamp had the same ceiling but a corner in its first derivative, so a
    // full eight-string chord - the one case that reaches the ceiling - was
    // clipped into harmonics of unbounded order and folded them back as
    // aliasing rather than saturating.
    const auto magneticTransfer = [] (float displacement, float drive,
                                      float inverseDrive)
    {
        constexpr float ceiling = 0.9f;
        constexpr float inverseCeilingSquared = 1.0f / (ceiling * ceiling);
        const float driven = displacement * drive;
        const float x = driven
            / std::sqrt(1.0f + driven * driven * inverseCeilingSquared);
        return (x + x * x * (0.55f + 0.30f * x)) * inverseDrive;
    };

    // A pickup whose selector mix has faded to silence contributes nothing, so
    // its two fractional reads, spatial aperture, flux polynomial, induced-EMF
    // difference and guard are skipped outright. At the default Bridge setting
    // this removes the entire neck chain from every string.
    float artifactPickup = 0.0f;
    if (artifactsActive_)
        artifactPickup = artifactContactSignal
                       + 0.035f * artifactBuzzAmount_ * saddleRattle;

    float neckSignal = 0.0f;
    float bridgeSignal = 0.0f;

    if (neckPathActive_)
    {
        const float delay = voice.pickupDelayNeck;
        float tap = 0.85f * (verticalTotal
                             - pickupCombDepth * vertical.readFractional(delay))
                  + 0.35f * (horizontalTotal
                             - pickupCombDepth * horizontal.readFractional(delay))
                  + 0.55f * artifactPickup;
        tap = voice.apertureNeck.process(tap, voice.apertureNeckLength);
        const float flux = voice.fluxScale
            * magneticTransfer(tap, magneticDriveNeck_, magneticDriveNeckInverse_);
        // Faraday's law: a magnetic pickup outputs induced voltage,
        // proportional to d(Phi)/dt, rather than displacement itself.
        // Normalising the finite difference at 220 Hz preserves practical
        // level while retaining the physically important frequency weighting.
        // The oversampled lowpass bounds the differentiator before the
        // loaded-coil circuit.
        neckSignal = voice.emfLowpassNeck.process(
            (flux - voice.previousFluxNeck) * emfScale_, emfLowpassCoefficient_);
        voice.previousFluxNeck = flux;
        // Local contact motion reaches the pickup as a short velocity-like
        // transient. It still passes through the shared loaded-coil circuit,
        // but does not masquerade as a persistent pitched wave on the string.
        neckSignal = (neckSignal + 0.09f * noiseSample)
                   * voice.articulationMakeup;
    }

    if (bridgePathActive_)
    {
        const float delay = voice.pickupDelayBridge;
        float tap = 0.85f * (verticalTotal
                             - pickupCombDepth * vertical.readFractional(delay))
                  + 0.35f * (horizontalTotal
                             - pickupCombDepth * horizontal.readFractional(delay))
                  + artifactPickup;
        tap = voice.apertureBridge.process(tap, voice.apertureBridgeLength);
        const float flux = voice.fluxScale
            * magneticTransfer(tap, magneticDriveBridge_,
                               magneticDriveBridgeInverse_);
        bridgeSignal = voice.emfLowpassBridge.process(
            (flux - voice.previousFluxBridge) * emfScale_, emfLowpassCoefficient_);
        voice.previousFluxBridge = flux;
        bridgeSignal = (bridgeSignal + 0.15f * noiseSample)
                     * voice.articulationMakeup;
    }

    vertical.writeIndex = (vertical.writeIndex + 1) & (delayLineSize - 1);
    horizontal.writeIndex = (horizontal.writeIndex + 1) & (delayLineSize - 1);

    vertical.currentDelay += vertical.delaySmoothing
                           * (vertical.targetDelay - vertical.currentDelay);
    horizontal.currentDelay += horizontal.delaySmoothing
                             * (horizontal.targetDelay - horizontal.currentDelay);

    // A phase-coherent divided-pickup field. Mono leaves both weights at one;
    // Stereo spreads strings by their real lateral order, without delay,
    // chorus, modulation, or random phase. The shared body remains centred.
    // In Mono the two channels are bit-identical, so only one is accumulated.
    if (channelsLinked_)
    {
        sums.neck[0] += 0.5f * neckSignal;
        sums.bridge[0] += 0.5f * bridgeSignal;
    }
    else
    {
        const float lateral = 2.0f * static_cast<float>(voice.stringIndex)
                                / static_cast<float>(stringCount - 1)
                            - 1.0f;
        const float side = 0.24f * stereoWidth_ * lateral;
        const std::array<float, 2> channelWeights { 1.0f - side, 1.0f + side };
        for (int channel = 0; channel < 2; ++channel)
        {
            const float weight = channelWeights[static_cast<std::size_t>(channel)];
            sums.neck[static_cast<std::size_t>(channel)]
                += 0.5f * neckSignal * weight;
            sums.bridge[static_cast<std::size_t>(channel)]
                += 0.5f * bridgeSignal * weight;
        }
    }

    // The bridge passes string vibration and playing noise into the body, and
    // the same bridge force drives the sympathetic coupling bus. Only voices
    // that are being played write to the bus; the coupled strings only read
    // it, so the coupling graph is acyclic and unconditionally stable.
    const float bridgeForce = 0.5f * (verticalTotal + horizontalTotal);
    sympatheticBus_ += bridgeForce;
    sums.body += bridgeForce + 1.6f * noiseSample;
    if (artifactsActive_)
        sums.body += 0.9f * artifactContactSignal
                   + 0.09f * artifactBuzzAmount_ * saddleRattle;
    if (voice.articulation == Articulation::Slap
        && voice.excitationPhase != ExcitationPhase::Idle)
        sums.body += 2.4f * excitation;

    // The slow energy envelope feeds tension modulation: its release side
    // follows the string's own decay scale so the attack pitch glide relaxes
    // over hundreds of milliseconds, as measured tension modulation does.
    const float instantaneous = verticalSample * verticalSample
                              + horizontalSample * horizontalSample;
    const float coefficient = instantaneous > voice.energyEnvelope
        ? energyAttackCoefficient_ : energyReleaseCoefficient_;
    voice.energyEnvelope += coefficient * (instantaneous - voice.energyEnvelope);

    // A separate, faster follower drives voice retirement only. Tying that to
    // the slow tension envelope kept an inaudible released string alive for
    // several seconds, needlessly holding its slot; this follower falls to
    // the retirement floor within about half a second of the audio going
    // silent while still tracking a genuine sustain.
    const float retireCoefficient = instantaneous > voice.outputEnergy
        ? retireAttackCoefficient_ : retireReleaseCoefficient_;
    voice.outputEnergy += retireCoefficient * (instantaneous - voice.outputEnergy);

    if (voice.releasing)
        voice.releaseGain += voice.releaseGainCoefficient
                           * (voice.releaseGainTarget - voice.releaseGain);

    voice.ageSamples++;
}

void ElectryEngine::renderSympatheticString(Voice& voice, RenderSums& sums,
                                            float drive) noexcept
{
    // A string nobody is fingering still vibrates: the bridge carries the
    // played strings' force into it, and it answers at its own open pitch.
    // This is a strictly one-directional slice of bridge coupling - the ring
    // never re-enters the bus - so no amount of coupling gain can create a
    // growing loop.
    auto& loop = voice.vertical;
    if (! voice.sympatheticReady)
    {
        // The voice was retired below audibility, so its residue is inaudible
        // but at the wrong pitch. Start the open string from rest.
        loop.clear();
        voice.sympatheticEnergy = 0.0f;
        voice.sympatheticPreviousFlux = 0.0f;
        voice.sympatheticEmf.reset();
        configureSympatheticString(voice);
        voice.sympatheticReady = true;
    }

    float sample = loop.readFractional(loop.currentDelay);
    sample = loop.damping.process(sample, loop.loopDampingCoefficient);
    sample *= loop.loopGain * sympatheticHandGain_;

    // A bounded rational saturation. The loop is already contractive, so this
    // never engages in normal use; it exists so that a pathological drive
    // landing exactly on a coupled mode still cannot exceed a fixed ceiling.
    const float magnitude = std::abs(sample);
    if (magnitude > 1.0f)
    {
        const float excess = magnitude - 1.0f;
        sample = (sample < 0.0f ? -1.0f : 1.0f)
               * (1.0f + excess / (1.0f + 4.0f * excess));
    }

    const float total = sample + sympatheticInjection_ * drive;
    loop.line[static_cast<std::size_t>(loop.writeIndex & (delayLineSize - 1))]
        = total;
    // The same physical pickup senses this string, so it cancels no better here
    // than it does on a played one: `pickupCombDepth` has to apply to the
    // coupled ring too, or a sympathetically excited low string keeps the
    // hollow fundamental and the infinitely deep position nulls this weight
    // exists to remove.
    const float tap = total
        - pickupCombDepth * loop.readFractional(voice.sympatheticPickupDelay);
    loop.writeIndex = (loop.writeIndex + 1) & (delayLineSize - 1);
    loop.currentDelay += loop.delaySmoothing
                       * (loop.targetDelay - loop.currentDelay);

    const float flux = voice.fluxScale * tap;
    const float emf = voice.sympatheticEmf.process(
        (flux - voice.sympatheticPreviousFlux) * emfScale_,
        emfLowpassCoefficient_);
    voice.sympatheticPreviousFlux = flux;

    if (channelsLinked_)
    {
        sums.bridge[0] += 0.5f * emf;
        sums.neck[0] += 0.28f * emf;
    }
    else
    {
        const float lateral = 2.0f * static_cast<float>(voice.stringIndex)
                                / static_cast<float>(stringCount - 1)
                            - 1.0f;
        const float side = 0.24f * stereoWidth_ * lateral;
        sums.bridge[0] += 0.5f * emf * (1.0f - side);
        sums.bridge[1] += 0.5f * emf * (1.0f + side);
        sums.neck[0] += 0.28f * emf * (1.0f - side);
        sums.neck[1] += 0.28f * emf * (1.0f + side);
    }
    sums.body += 0.35f * total;

    voice.sympatheticEnergy += sympatheticEnergyCoefficient_
        * (total * total - voice.sympatheticEnergy);
}

void ElectryEngine::freezeSharedPath() noexcept
{
    for (auto& filter : neckCoils_)
        filter.reset();
    for (auto& filter : bridgeCoils_)
        filter.reset();
    for (auto& blocker : outputDc_)
        blocker.reset();
    for (auto& mode : bodyModes_)
        mode.reset();
    for (auto& mode : sympatheticModes_)
        mode.reset();
    for (auto& decimator : decimators_)
        decimator.reset();
    previousBodyDisplacement_ = 0.0f;
    bodyEmfLowpass_.reset();
    sympatheticBus_ = 0.0f;
    sympatheticBusDelayed_ = 0.0f;
    silentInternalSamples_ = 0;
}

ElectryEngine::StereoSample ElectryEngine::renderInternalSample() noexcept
{
    if (controlCountdown_ <= 0)
    {
        controlCountdown_ = controlPeriod;

        // Continuous parameter smoothing toward the host targets.
        const auto smoothTowards = [this] (float& value, float target)
        {
            value += parameterSmoothingCoefficient_ * (target - value);
        };
        auto& s = smoothedParameters_;
        const auto& t = targetParameters_;
        const bool pickupDirty =
            s.pickupSelector != t.pickupSelector
            || std::abs(s.pickupType - t.pickupType) > 1.0e-4f
            || std::abs(s.toneKnob - t.toneKnob) > 1.0e-4f;
        const bool bodyDirty =
            std::abs(s.bodyWood - t.bodyWood) > 1.0e-4f
            || std::abs(s.bodySize - t.bodySize) > 1.0e-4f
            || std::abs(s.bodyShape - t.bodyShape) > 1.0e-4f
            || std::abs(s.construction - t.construction) > 1.0e-4f;

        smoothTowards(s.bodyWood, t.bodyWood);
        smoothTowards(s.bodySize, t.bodySize);
        smoothTowards(s.bodyShape, t.bodyShape);
        smoothTowards(s.construction, t.construction);
        smoothTowards(s.scaleLength, t.scaleLength);
        smoothTowards(s.pickupType, t.pickupType);
        smoothTowards(s.toneKnob, t.toneKnob);
        smoothTowards(s.bodyResonance, t.bodyResonance);
        smoothTowards(s.stringGauge, t.stringGauge);
        smoothTowards(s.stringAge, t.stringAge);
        smoothTowards(s.pickPosition, t.pickPosition);
        smoothTowards(s.pickHardness, t.pickHardness);
        smoothTowards(s.pickNoise, t.pickNoise);
        smoothTowards(s.fingerNoise, t.fingerNoise);
        smoothTowards(s.releaseNoise, t.releaseNoise);
        smoothTowards(s.muteDamping, t.muteDamping);
        smoothTowards(s.velocityAmount, t.velocityAmount);
        smoothTowards(s.artifactAmount, t.artifactAmount);
        smoothTowards(s.sympatheticAmount, t.sympatheticAmount);
        smoothTowards(s.palmMute, t.palmMute);
        smoothTowards(s.vibratoDepthCents, t.vibratoDepthCents);
        s.strumSpreadSeconds = t.strumSpreadSeconds;
        s.bendTimeSeconds = t.bendTimeSeconds;
        s.pickupSelector = t.pickupSelector;
        s.outputMode = t.outputMode;
        smoothTowards(s.outputGain, t.outputGain);

        vibratoDepthSemitones_ = 0.01f * s.vibratoDepthCents;
        palmMuteBlend_ = clampf(s.palmMute + palmMutePressure_, 0.0f, 1.0f);
        if (t.palmMute <= 0.0f && s.palmMute < 1.0e-5f)
            s.palmMute = 0.0f;

        // The sympathetic coupling bypasses exactly at zero: the coupled loops
        // stop being rendered and their state is dropped, so an idle engine is
        // bit-for-bit identical to one built without the feature.
        sympatheticGain_ = 0.0045f * s.sympatheticAmount;
        if (t.sympatheticAmount <= 0.0f && s.sympatheticAmount < 1.0e-5f)
        {
            s.sympatheticAmount = 0.0f;
            sympatheticGain_ = 0.0f;
            if (sympatheticActive_)
            {
                for (auto& voice : voices_)
                {
                    if (voice.active)
                        continue;
                    voice.sympatheticReady = false;
                    voice.sympatheticEnergy = 0.0f;
                    voice.sympatheticPreviousFlux = 0.0f;
                    voice.sympatheticEmf.reset();
                }
                sympatheticBus_ = 0.0f;
                sympatheticBusDelayed_ = 0.0f;
                sympatheticStringCount_ = 0;
            }
            sympatheticActive_ = false;
        }
        else
        {
            sympatheticActive_ = true;
        }

        if (sympatheticActive_)
        {
            // The muting hand covers every string. A palm-muted, chugged or
            // dead-note passage therefore damps the coupled strings and stops
            // feeding them, which is what keeps a Drop-E chug tight instead of
            // washing it in open-string ring.
            float handMute = palmMuteBlend_;
            for (const auto& voice : voices_)
            {
                if (! voice.active)
                    continue;
                switch (voice.articulation)
                {
                    case Articulation::Muted:
                        handMute = std::max(handMute, 0.55f + 0.45f * s.muteDamping);
                        break;
                    case Articulation::Chug:
                        handMute = std::max(handMute, 0.75f + 0.25f * s.muteDamping);
                        break;
                    case Articulation::DeadNote:
                        handMute = std::max(handMute, 0.95f);
                        break;
                    default:
                        break;
                }
            }
            if (handMute != sympatheticHandMute_)
            {
                sympatheticHandMute_ = handMute;
                // A per-sample contact loss, which is how a hand resting on a
                // string actually damps it: distributed, not once per period.
                sympatheticHandGainTarget_ = handMute > 0.0f
                    ? std::pow(10.0f,
                               -3.0f / (std::exp(lerp(std::log(4.0f),
                                                      std::log(0.045f), handMute))
                                        * static_cast<float>(sampleRate_)))
                    : 1.0f;
            }
            sympatheticHandGain_ += parameterSmoothingCoefficient_
                * (sympatheticHandGainTarget_ - sympatheticHandGain_);
            sympatheticInjection_ = sympatheticGain_
                * std::max(0.0f, 1.0f - handMute);
        }
        else
        {
            sympatheticInjection_ = 0.0f;
        }

        if (t.artifactAmount <= 0.0f && s.artifactAmount < 1.0e-5f)
        {
            s.artifactAmount = 0.0f;
            if (artifactsActive_)
            {
                for (auto& mode : sympatheticModes_)
                    mode.reset();
                for (auto& voice : voices_)
                {
                    voice.artifactCollisionRemaining = 0;
                    voice.artifactNoiseShaper.reset();
                    voice.artifactNoiseBandState = 0.0f;
                    voice.saddleRattle.reset();
                }
            }
            artifactsActive_ = false;
        }
        else if (t.artifactAmount > 0.0f || s.artifactAmount > 0.0f)
        {
            artifactsActive_ = true;
        }
        artifactContactShape_ = smoothStep(s.artifactAmount);
        artifactBuzzAmount_ = s.artifactAmount * s.artifactAmount;

        pitchBendSemitones_ += pitchBendCoefficient_
                             * (pitchBendTarget_ - pitchBendSemitones_);
        vibratoAmount_ += vibratoCoefficient_
                        * (vibratoTarget_ - vibratoAmount_);
        vibratoPhase_ += twoPi * 5.4f * static_cast<float>(controlPeriod)
                       / static_cast<float>(sampleRate_);
        if (vibratoPhase_ >= twoPi)
            vibratoPhase_ -= twoPi;

        if (pickupDirty)
            configurePickupFilters();
        if (bodyDirty)
            configureBody();
        refreshVoicingIfNeeded();

        neckMix_ += pickupMixCoefficient_ * (neckMixTarget_ - neckMix_);
        bridgeMix_ += pickupMixCoefficient_ * (bridgeMixTarget_ - bridgeMix_);
        if (neckMixTarget_ == 0.0f && neckMix_ < 1.0e-5f)
            neckMix_ = 0.0f;
        if (bridgeMixTarget_ == 0.0f && bridgeMix_ < 1.0e-5f)
            bridgeMix_ = 0.0f;

        // A silent pickup is skipped entirely in renderVoice(). Its per-voice
        // aperture ring and induced-EMF memory are cleared as it comes back so
        // the fade-in starts from a clean path rather than stale history.
        const bool neckWanted = neckMix_ > 1.0e-4f;
        const bool bridgeWanted = bridgeMix_ > 1.0e-4f;
        if (neckWanted && ! neckPathActive_)
        {
            for (auto& voice : voices_)
            {
                voice.apertureNeck.reset();
                voice.previousFluxNeck = 0.0f;
                voice.emfLowpassNeck.reset();
            }
        }
        if (bridgeWanted && ! bridgePathActive_)
        {
            for (auto& voice : voices_)
            {
                voice.apertureBridge.reset();
                voice.previousFluxBridge = 0.0f;
                voice.emfLowpassBridge.reset();
            }
        }
        neckPathActive_ = neckWanted;
        bridgePathActive_ = bridgeWanted;

        smoothedOutputGain_ = s.outputGain;
        smoothedBodyLevel_ = 24.5f * s.bodyResonance;
        const float stereoTarget = s.outputMode == OutputMode::Stereo ? 1.0f : 0.0f;
        stereoWidth_ += pickupMixCoefficient_ * (stereoTarget - stereoWidth_);
        if (stereoTarget == 0.0f && stereoWidth_ < 1.0e-6f)
            stereoWidth_ = 0.0f;

        // Mono is exact dual mono, so the second coil, DC blocker and
        // decimator are redundant. Opening the field copies channel zero's
        // state across, which is exact because both channels have seen
        // identical inputs up to this sample.
        const bool linked = stereoWidth_ == 0.0f;
        if (channelsLinked_ && ! linked)
        {
            neckCoils_[1] = neckCoils_[0];
            bridgeCoils_[1] = bridgeCoils_[0];
            outputDc_[1] = outputDc_[0];
            decimators_[1] = decimators_[0];
        }
        channelsLinked_ = linked;

        for (auto& voice : voices_)
            updateVoiceControl(voice);
        updateActiveVoiceCount();
    }
    controlCountdown_--;
    ++engineClock_;

    RenderSums sums;

    // The coupled strings read the previous sample's bridge total, which makes
    // the result independent of voice order and removes any algebraic loop.
    const float sympatheticDrive = sympatheticBusDelayed_;
    sympatheticBus_ = 0.0f;

    bool rendered = false;
    for (auto& voice : voices_)
    {
        if (voice.active)
        {
            renderVoice(voice, sums);
            rendered = true;
        }
        else if (sympatheticActive_
                 && (voice.sympatheticEnergy > 1.0e-11f
                     || std::abs(sympatheticDrive) > 1.0e-6f))
        {
            renderSympatheticString(voice, sums, sympatheticDrive);
            rendered = true;
        }
    }
    sympatheticBusDelayed_ = sympatheticBus_;

    // Nothing is vibrating and the shared path has already been cleared, so
    // there is no arithmetic left to do.
    if (! rendered && idleFrozen_)
        return {};

    // Open strings and bridge hardware ring sympathetically. This is a
    // strictly feed-forward bank, so it colours the pickup/body drive without
    // threatening the waveguide stability contract.
    if (artifactsActive_)
    {
        // Bridge and saddle hardware is heard through whichever coil is
        // selected, so the artifact bank is driven by the same selector mix
        // the output uses. That is both more physical than an unweighted
        // neck+bridge average and independent of a pickup path that the
        // selector has faded out and the renderer therefore skips.
        const float neckMid = channelsLinked_
            ? sums.neck[0] : 0.5f * (sums.neck[0] + sums.neck[1]);
        const float bridgeMid = channelsLinked_
            ? sums.bridge[0] : 0.5f * (sums.bridge[0] + sums.bridge[1]);
        const float drive = neckMid * neckMix_ + bridgeMid * bridgeMix_;
        float ring = 0.0f;
        for (auto& mode : sympatheticModes_)
            ring += mode.process(drive);
        const float mix = 0.85f * smoothedParameters_.artifactAmount;
        const int channelCount = channelsLinked_ ? 1 : 2;
        for (int channel = 0; channel < channelCount; ++channel)
        {
            sums.neck[static_cast<std::size_t>(channel)] += 0.55f * mix * ring;
            sums.bridge[static_cast<std::size_t>(channel)] += mix * ring;
        }
    }

    // Solid-body bridge drive is displacement-domain. Differentiate it once
    // before the slowly automated structural bank: for fixed coefficients
    // this commutes exactly with the modes, while parameter changes cannot be
    // turned into derivative spikes. The result then joins pickup voltage;
    // displacement is never summed directly into the electrical path.
    const float bodyDisplacement = 0.080f * sums.body;
    const float bodyEmfScale = static_cast<float>(sampleRate_) / (twoPi * 220.0f);
    const float bodyDriveVoltage =
        (bodyDisplacement - previousBodyDisplacement_) * bodyEmfScale;
    previousBodyDisplacement_ = bodyDisplacement;
    float bodyVoltage = 0.0f;
    for (auto& mode : bodyModes_)
        bodyVoltage += mode.process(bodyDriveVoltage);
    bodyVoltage = bodyEmfLowpass_.process(
        bodyVoltage, bodyEmfLowpassCoefficient_) * smoothedBodyLevel_;

    // Humbuckers are hotter than low-wind single coils.
    const float pickupLevel = lerp(1.40f, 0.92f, smoothedParameters_.pickupType);

    StereoSample output;
    std::array<float, 2> raw {};
    const int outputChannels = channelsLinked_ ? 1 : 2;
    for (int channel = 0; channel < outputChannels; ++channel)
    {
        const auto index = static_cast<std::size_t>(channel);
        const float neckOut = neckCoils_[index].process(sums.neck[index]
                                                        + 0.65f * bodyVoltage);
        const float bridgeOut = bridgeCoils_[index].process(sums.bridge[index]
                                                            + 0.45f * bodyVoltage);
        const float pickup = (neckOut * neckMix_ + bridgeOut * bridgeMix_)
                           * pickupLevel;
        raw[index] = outputDc_[index].process(pickup, outputDcCoefficient_);
    }
    if (channelsLinked_)
        raw[1] = raw[0];

    // One linked guard gain preserves the physical stereo field. In Mono the
    // channels are equal and this is the original scalar soft-limit law.
    const float guardInput = std::max(std::abs(raw[0]), std::abs(raw[1]));
    const float guardGain = 1.0f
        / std::sqrt(1.0f + 0.4356f * guardInput * guardInput);
    output.left = raw[0] * guardGain * smoothedOutputGain_;
    output.right = raw[1] * guardGain * smoothedOutputGain_;

    if (rendered || guardInput >= idleFreezeLevel)
    {
        silentInternalSamples_ = 0;
        idleFrozen_ = false;
    }
    else if (++silentInternalSamples_ >= idleFreezeSamples)
    {
        freezeSharedPath();
        idleFrozen_ = true;
        output = {};
    }

    if (! finitef(output.left) || ! finitef(output.right))
    {
        // A non-finite sample means some state has been corrupted by hostile
        // input; recover silently rather than latching.
        for (auto& voice : voices_)
        {
            silenceVoice(voice);
            voice.vertical.clear();
            voice.horizontal.clear();
        }
        for (auto& filter : neckCoils_)
            filter.reset();
        for (auto& filter : bridgeCoils_)
            filter.reset();
        for (auto& blocker : outputDc_)
            blocker.reset();
        for (auto& mode : bodyModes_)
            mode.reset();
        previousBodyDisplacement_ = 0.0f;
        bodyEmfLowpass_.reset();
        for (auto& mode : sympatheticModes_)
            mode.reset();
        for (auto& decimator : decimators_)
            decimator.reset();
        sympatheticBus_ = 0.0f;
        sympatheticBusDelayed_ = 0.0f;
        output = {};
    }

    return output;
}

void ElectryEngine::process(float* left, float* right, int numSamples)
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    if (! prepared_)
    {
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
        return;
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        StereoSample output;
        if (oversamplingFactor_ == 2)
        {
            // In Mono only one decimator runs; its state is copied to the
            // second the instant the stereo field opens.
            bool linked = channelsLinked_;
            bool frozen = true;
            for (int phase = 0; phase < 2; ++phase)
            {
                const auto internal = renderInternalSample();
                if (idleFrozen_)
                    continue;
                frozen = false;
                decimators_[0].push(internal.left);
                linked = linked && channelsLinked_;
                if (! channelsLinked_)
                    decimators_[1].push(internal.right);
            }
            if (! frozen)
            {
                output.left = decimators_[0].output();
                output.right = linked ? output.left : decimators_[1].output();
            }
        }
        else
        {
            output = renderInternalSample();
        }

        if (! finitef(output.left) || ! finitef(output.right))
        {
            for (auto& decimator : decimators_)
                decimator.reset();
            output = {};
        }

        left[sample] = output.left;
        right[sample] = output.right;
    }
}

void ElectryEngine::updateActiveVoiceCount() noexcept
{
    int count = 0;
    int sympathetic = 0;
    for (const auto& voice : voices_)
    {
        if (voice.active)
            ++count;
        else if (voice.sympatheticReady && voice.sympatheticEnergy > 1.0e-11f)
            ++sympathetic;
    }
    activeVoiceCount_ = count;
    sympatheticStringCount_ = sympathetic;
}

int ElectryEngine::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount_;
}

int ElectryEngine::getSympatheticStringCount() const noexcept
{
    return sympatheticStringCount_;
}

void ElectryEngine::getStringVisualState(
    std::array<StringVisualState, stringCount>& destination) const noexcept
{
    for (int stringIndex = 0; stringIndex < stringCount; ++stringIndex)
    {
        const auto& voice = voices_[static_cast<std::size_t>(stringIndex)];
        auto& state = destination[static_cast<std::size_t>(stringIndex)];
        state.sounding = voice.active;
        state.releasing = voice.active && voice.releasing;
        state.articulation = voice.articulation;
        if (voice.active)
        {
            state.sympathetic = false;
            state.midiNote = voice.midiNote;
            state.fret = voice.fret;
            // The display level is a ballistic follower of the same energy the
            // retirement logic uses; 4.0 places a hard picked note near full
            // scale without clipping the meter on a strum.
            state.level = clampf(4.0f * voice.displayLevel, 0.0f, 1.0f);
        }
        else
        {
            const bool ringing = voice.sympatheticReady
                              && voice.sympatheticEnergy > 1.0e-11f;
            state.sympathetic = ringing;
            state.midiNote = ringing
                ? stringSpecs()[static_cast<std::size_t>(stringIndex)].openMidiNote
                : -1;
            state.fret = ringing ? 0 : -1;
            state.level = ringing
                ? clampf(6.0f * std::sqrt(std::max(voice.sympatheticEnergy, 0.0f)),
                         0.0f, 1.0f)
                : 0.0f;
        }
    }
}

float ElectryEngine::currentSoundingSemitoneOffset(const Voice& voice) const noexcept
{
    return lerp(voice.bendStartSemitones, voice.bendTargetSemitones,
                smoothStep(voice.bendProgress))
         + pitchBendSemitones_;
}

} // namespace electry
