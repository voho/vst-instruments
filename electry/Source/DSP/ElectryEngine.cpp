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

// Reference geometry of the two anchor instruments. Scale lengths are the
// documented 24.75 in and 25.5 in; pickup distances from the bridge saddles
// are geometric estimates of typical mounting positions, not measurements of
// specific serial numbers.
constexpr float lesPaulScaleMetres = 0.62865f;
constexpr float telecasterScaleMetres = 0.6477f;
constexpr float lesPaulBridgePickupMetres = 0.046f;
constexpr float telecasterBridgePickupMetres = 0.031f;
constexpr float lesPaulNeckPickupMetres = 0.155f;
constexpr float telecasterNeckPickupMetres = 0.163f;

// Effective magnetic aperture windows: a humbucker averages string motion
// across two coils, a narrow single coil across a much shorter window.
constexpr float humbuckerApertureMetres = 0.0175f;
constexpr float singleCoilApertureMetres = 0.0064f;

// Loaded electrical resonance of the two anchor pickup circuits (coil
// inductance and capacitance with typical pot and cable loading).
constexpr float humbuckerResonanceHz = 2450.0f;
constexpr float singleCoilResonanceHz = 4050.0f;
constexpr float humbuckerResonanceQ = 1.35f;
constexpr float singleCoilResonanceQ = 1.95f;

constexpr float steelDensity = 7850.0f;      // kg/m^3
constexpr float steelYoungModulus = 2.0e11f; // Pa

constexpr float clampf(float value, float low, float high) noexcept
{
    return value < low ? low : (value > high ? high : value);
}

bool finitef(float value) noexcept { return std::isfinite(value); }
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
    const float omega = twoPi * frequencyHz / sampleRate;
    const float radius = std::exp(-omega / (2.0f * q));
    a1 = -2.0f * radius * std::cos(omega);
    a2 = radius * radius;
    // Normalise so the resonator's peak response stays comparable across
    // configurations, then apply the mode's relative level.
    gain = (1.0f - radius * radius) * modeGain;
}

// ---------------------------------------------------------------------------
// Polarisation loop
// ---------------------------------------------------------------------------

void ElectryEngine::PolarisationLoop::clear() noexcept
{
    line.fill(0.0f);
    writeIndex = 0;
    damping.reset();
    dispersion1.reset();
    dispersion2.reset();
}

float ElectryEngine::PolarisationLoop::readFractional(float delaySamples) const noexcept
{
    delaySamples = clampf(delaySamples, 4.0f, static_cast<float>(delayLineSize - 8));
    const float position = static_cast<float>(writeIndex) - delaySamples;
    const int index = static_cast<int>(std::floor(position));
    const float fraction = position - static_cast<float>(index);
    const int mask = delayLineSize - 1;
    const float y0 = line[static_cast<std::size_t>((index - 1) & mask)];
    const float y1 = line[static_cast<std::size_t>(index & mask)];
    const float y2 = line[static_cast<std::size_t>((index + 1) & mask)];
    const float y3 = line[static_cast<std::size_t>((index + 2) & mask)];
    // Third-order Lagrange interpolation with y1..y2 as the unit interval.
    const float t = fraction;
    const float term0 = y0 * (-t * (t - 1.0f) * (t - 2.0f)) / 6.0f;
    const float term1 = y1 * ((t + 1.0f) * (t - 1.0f) * (t - 2.0f)) / 2.0f;
    const float term2 = y2 * (-(t + 1.0f) * t * (t - 2.0f)) / 2.0f;
    const float term3 = y3 * ((t + 1.0f) * t * (t - 1.0f)) / 6.0f;
    return term0 + term1 + term2 + term3;
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
    // Standard tuning, light 0.009-0.042 reference set. The gauge parameter
    // scales these diameters toward a 0.011-0.052 set.
    static const std::array<StringSpec, stringCount> specs {{
        { 40, true, 1.0668f, 0.40f, 7.5f },  // E2, wound
        { 45, true, 0.8128f, 0.42f, 7.0f },  // A2, wound
        { 50, true, 0.6096f, 0.44f, 6.4f },  // D3, wound
        { 55, false, 0.4064f, 1.0f, 5.4f },  // G3, plain
        { 59, false, 0.2794f, 1.0f, 4.6f },  // B3, plain
        { 64, false, 0.2286f, 1.0f, 4.0f },  // E4, plain
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

float ElectryEngine::bipolarNoise(std::uint32_t& state) noexcept
{
    state = state * 1664525u + 1013904223u;
    const auto bits = (state >> 9) | 0x3f800000u;
    float unit;
    std::memcpy(&unit, &bits, sizeof(unit));
    return 2.0f * (unit - 1.5f);
}

float ElectryEngine::smoothStep(float value) noexcept
{
    value = clampf(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

float ElectryEngine::onePolePhaseDelay(float coefficient, float omega) noexcept
{
    // H(z) = (1 - a) / (1 - a z^-1)
    const float angle = -std::atan2(coefficient * std::sin(omega),
                                    1.0f - coefficient * std::cos(omega));
    return omega > 1.0e-9f ? -angle / omega : coefficient / (1.0f - coefficient);
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

float ElectryEngine::softLimit(float value) noexcept
{
    return value / std::sqrt(1.0f + 0.4356f * value * value);
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
    sampleRate_ = std::clamp(sampleRate, minimumSupportedSampleRate,
                             maximumSupportedSampleRate);
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);

    // 14 ms continuous-parameter smoothing and a 4 ms pickup selector fade,
    // both advanced at the control tick.
    const float controlRate = static_cast<float>(sampleRate_)
                            / static_cast<float>(controlPeriod);
    parameterSmoothingCoefficient_ =
        1.0f - std::exp(-1.0f / (0.014f * controlRate));
    pickupMixCoefficient_ = 1.0f - std::exp(-1.0f / (0.004f * controlRate));

    // The output DC blocker corner stays at 5 Hz regardless of rate. Inside
    // the string loops there is deliberately no DC filter: a fixed-corner
    // blocker's steep phase lead near a low fundamental would detune the
    // upper partials against the compensated fundamental, and the pickup
    // position comb already rejects DC exactly.
    outputDcCoefficient_ = std::exp(-twoPi * 5.0f * inverseSampleRate_);

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
    }

    smoothedParameters_ = sanitise(targetParameters_);
    articulation_ = Articulation::Downstroke;
    noteSequence_ = 0;
    activeVoiceCount_ = 0;
    controlCountdown_ = 0;
    pitchBendSemitones_ = pitchBendTarget_;
    sustainPedalDown_ = false;

    neckCoil_.reset();
    bridgeCoil_.reset();
    outputDc_.reset();
    for (auto& mode : bodyModes_)
        mode.reset();
    smoothedOutputGain_ = smoothedParameters_.outputGain;
    smoothedBodyLevel_ = smoothedParameters_.bodyResonance;

    configureBody();
    configurePickupFilters();
    appliedVoicingParameters_ = smoothedParameters_;
    neckMix_ = neckMixTarget_;
    bridgeMix_ = bridgeMixTarget_;
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
        return;
    }

    if (! isPlayableNote(midiNote) || velocity <= 0.0f)
        return;

    const auto articulation = articulation_;
    const int stringIndex = chooseString(midiNote, articulation);
    if (stringIndex < 0)
        return;

    auto& voice = voices_[static_cast<std::size_t>(stringIndex)];

    const bool legato = articulation == Articulation::HammerOn
                     && voice.active
                     && voice.midiNote != midiNote;
    if (legato)
        legatoRetarget(voice, midiNote, velocity);
    else
        startVoice(voice, midiNote, velocity, articulation);

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
    return lerp(lesPaulScaleMetres, telecasterScaleMetres,
                smoothedParameters_.scaleLength);
}

float ElectryEngine::deadSpotFactor(int stringIndex, int fret) const noexcept
{
    // Solid-body dead spots: locally raised neck conductance shortens decay
    // at specific fret positions. Bolt-on construction deepens the effect.
    static constexpr std::array<float, stringCount> centres {
        9.5f, 11.0f, 12.5f, 6.0f, 5.0f, 7.5f
    };
    const float depth = 0.05f + 0.30f * smoothedParameters_.construction;
    const float distance = static_cast<float>(fret)
                         - centres[static_cast<std::size_t>(stringIndex)];
    const float gaussian = std::exp(-distance * distance / (2.0f * 1.8f * 1.8f));
    return 1.0f / (1.0f + depth * gaussian);
}

void ElectryEngine::configureVoiceDamping(Voice& voice) noexcept
{
    const auto& parameters = smoothedParameters_;
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];

    float t60 = spec.t60Seconds;
    t60 *= lerp(1.0f, 0.55f, parameters.stringAge);
    t60 *= lerp(1.12f, 0.92f, parameters.construction);
    t60 *= lerp(0.95f, 1.10f, parameters.stringGauge);
    t60 *= deadSpotFactor(voice.stringIndex, voice.fret);

    if (voice.articulation == Articulation::Muted)
    {
        const float muteT60 = lerp(0.60f, 0.09f, parameters.muteDamping);
        t60 = std::min(t60, muteT60);
    }

    // High-frequency decay target relative to the fundamental's decay.
    float highRatio = lerp(0.34f, 0.10f, parameters.stringAge);
    highRatio *= spec.wound ? 0.72f : 1.0f;
    highRatio *= lerp(1.05f, 0.90f, parameters.stringGauge);
    highRatio = clampf(highRatio, 0.02f, 0.9f);

    const float sampleRate = static_cast<float>(sampleRate_);
    const float f0 = voice.baseFrequency;
    const float fHigh = std::min(3600.0f, 0.32f * sampleRate);
    const float omega0 = twoPi * f0 * inverseSampleRate_;
    const float omegaHigh = twoPi * std::max(fHigh, f0 * 1.5f) * inverseSampleRate_;

    const auto configureLoop = [&] (PolarisationLoop& loop, float t60Scale)
    {
        const float t60Fundamental = std::max(0.02f, t60 * t60Scale);
        const float t60High = std::max(0.012f, t60Fundamental * highRatio);
        const float period = sampleRate / f0;
        const float gainAtF0 =
            std::pow(10.0f, -3.0f * period / (t60Fundamental * sampleRate));
        const float gainAtHigh =
            std::pow(10.0f, -3.0f * period / (t60High * sampleRate));
        const float ratio = clampf(gainAtHigh / std::max(gainAtF0, 1.0e-6f),
                                   1.0e-4f, 1.0f);

        // Solve the one-pole coefficient whose magnitude ratio between f0 and
        // fHigh matches the decay-time ratio. The response is monotonic in
        // the coefficient, so bisection is reliable.
        const auto magnitude = [] (float a, float omega)
        {
            const float denom = 1.0f + a * a - 2.0f * a * std::cos(omega);
            return (1.0f - a) / std::sqrt(std::max(denom, 1.0e-12f));
        };
        float low = 0.0f;
        float high = 0.9995f;
        for (int i = 0; i < 18; ++i)
        {
            const float mid = 0.5f * (low + high);
            const float value = magnitude(mid, omegaHigh) / magnitude(mid, omega0);
            if (value > ratio)
                low = mid;
            else
                high = mid;
        }
        loop.loopDampingCoefficient = 0.5f * (low + high);
        const float f0Magnitude = magnitude(loop.loopDampingCoefficient, omega0);
        loop.loopGain = clampf(gainAtF0 / std::max(f0Magnitude, 1.0e-6f),
                               0.0f, 0.99999f);
    };

    // The polarisation parallel to the body outlives the perpendicular one,
    // producing the instrument's two-stage decay.
    configureLoop(voice.vertical, 1.0f);
    configureLoop(voice.horizontal, 1.7f);
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

    const float semitones = bendOffset + legatoOffset + pitchBendSemitones_;

    // The dispersion solve and filter phase compensation are only refreshed
    // when the sounding pitch actually moves; the tension-modulation factor
    // below stays cheap enough for every control tick.
    if (forceDelayJump
        || std::abs(semitones - voice.lastConfiguredSemitones) > 8.0e-4f)
    {
        voice.lastConfiguredSemitones = semitones;

        const float f0 = clampf(voice.baseFrequency * std::exp2(semitones / 12.0f),
                                20.0f, 0.24f * static_cast<float>(sampleRate_));
        const float omega = twoPi * f0 * inverseSampleRate_;
        const float period = static_cast<float>(sampleRate_) / f0;

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

        // Map the inharmonicity coefficient to the pair of first-order loop
        // allpasses: pick the coefficient whose extra low-frequency phase
        // delay reproduces the delay deficit stiffness causes at a reference
        // partial.
        float dispersion = 0.0f;
        if (inharmonicity > 1.0e-7f)
        {
            const float referencePartial =
                clampf(std::floor(0.30f * static_cast<float>(sampleRate_) / f0),
                       2.0f, 16.0f);
            const float stretch =
                std::sqrt((1.0f + inharmonicity * referencePartial * referencePartial)
                          / (1.0f + inharmonicity));
            const float wantedDeficit = period * (1.0f - 1.0f / stretch);
            const float omegaRef = std::min(omega * referencePartial, pi * 0.95f);
            float low = -0.55f;
            float high = 0.0f;
            for (int i = 0; i < 14; ++i)
            {
                const float mid = 0.5f * (low + high);
                const float deficit = 2.0f * (allpassPhaseDelay(mid, omega)
                                              - allpassPhaseDelay(mid, omegaRef));
                if (deficit < wantedDeficit)
                    high = mid;
                else
                    low = mid;
            }
            dispersion = 0.5f * (low + high);
        }
        voice.vertical.dispersionCoefficient = dispersion;
        voice.horizontal.dispersionCoefficient = dispersion;

        // Compensate every loop filter's phase delay at the fundamental so
        // the sounding pitch matches the target frequency.
        const auto loopPhaseDelay = [&] (const PolarisationLoop& loop)
        {
            return onePolePhaseDelay(loop.loopDampingCoefficient, omega)
                 + 2.0f * allpassPhaseDelay(loop.dispersionCoefficient, omega);
        };

        voice.compensatedPeriodVertical = period - loopPhaseDelay(voice.vertical);
        voice.compensatedPeriodHorizontal = period - loopPhaseDelay(voice.horizontal);
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

    const bool dirty = moved(s.stringAge, a.stringAge)
                    || moved(s.stringGauge, a.stringGauge)
                    || moved(s.construction, a.construction)
                    || moved(s.muteDamping, a.muteDamping)
                    || moved(s.scaleLength, a.scaleLength)
                    || moved(s.pickupType, a.pickupType);
    if (! dirty)
        return;

    appliedVoicingParameters_ = smoothedParameters_;
    for (auto& voice : voices_)
    {
        if (! voice.active)
            continue;
        configureVoiceDamping(voice);
        configureVoicePickups(voice);
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

    // Magnetic aperture: the transverse wave speed on the string sets which
    // temporal frequency corresponds to the aperture's spatial average.
    const float aperture = lerp(humbuckerApertureMetres, singleCoilApertureMetres,
                                parameters.pickupType);
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    const float waveSpeed = 2.0f * openLength * midiToHz(
        static_cast<float>(spec.openMidiNote));
    const float cutoff = clampf(0.443f * waveSpeed / aperture, 700.0f,
                                0.45f * static_cast<float>(sampleRate_));
    const float coefficient = std::exp(-twoPi * cutoff * inverseSampleRate_);
    voice.apertureNeckCoefficient = coefficient;
    voice.apertureBridgeCoefficient = coefficient;
}

void ElectryEngine::startExcitation(Voice& voice, float velocity, bool legato) noexcept
{
    const auto& parameters = smoothedParameters_;
    const float sampleRate = static_cast<float>(sampleRate_);
    const float shaped = (1.0f - parameters.velocityAmount)
                       + parameters.velocityAmount * std::pow(velocity, 1.6f);

    float amplitude = 0.62f * shaped;
    // The string leaves the pick in a fraction of a millisecond; the width
    // controls how much of the upper spectrum the release step carries.
    float pulseMs = lerp(0.85f, 0.16f, parameters.pickHardness);
    float pulseCutoff = lerp(1400.0f, 9200.0f, parameters.pickHardness);
    float pluckFraction = lerp(0.055f, 0.42f, parameters.pickPosition);
    float noiseLevel = parameters.pickNoise * (0.16f + 0.55f * shaped);
    float noiseMs = lerp(3.6f, 1.3f, parameters.pickHardness);
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    float noiseCutoff = spec.wound ? 2300.0f : 4300.0f;
    voice.excitationPolarity = 1.0f;

    switch (voice.articulation)
    {
        case Articulation::Downstroke:
            break;
        case Articulation::Upstroke:
            amplitude *= 0.92f;
            pulseCutoff *= 1.18f;
            // The upstroke contact point sits a little closer to the bridge,
            // thinning and brightening the stroke.
            pluckFraction = clampf(pluckFraction - 0.025f, 0.03f, 0.45f);
            noiseMs *= 0.8f;
            noiseCutoff *= 1.2f;
            voice.excitationPolarity = -1.0f;
            break;
        case Articulation::HammerOn:
            amplitude *= legato ? 0.30f : 0.42f;
            pulseMs *= 1.9f;
            pulseCutoff *= 0.42f;
            pluckFraction = 0.12f;
            noiseLevel = parameters.fingerNoise * (0.10f + 0.30f * shaped);
            noiseMs = 1.6f;
            noiseCutoff = 850.0f;
            break;
        case Articulation::Muted:
            amplitude *= 0.88f;
            pulseMs *= 1.2f;
            pulseCutoff *= 0.72f;
            pluckFraction *= 0.8f;
            noiseLevel *= 1.5f;
            break;
        case Articulation::Bend1Up:
        case Articulation::Bend2Up:
        case Articulation::Bend1Down:
        case Articulation::Bend2Down:
            break;
        case Articulation::Slap:
            amplitude *= 1.35f;
            pulseMs *= 0.45f;
            pulseCutoff *= 1.6f;
            pluckFraction = 0.34f;
            noiseLevel *= 1.35f;
            noiseCutoff = 5200.0f;
            noiseMs *= 0.7f;
            break;
    }

    voice.excitationAmplitude = amplitude;
    voice.excitationLength = std::max(
        8, static_cast<int>(pulseMs * 0.001f * sampleRate));
    const int contactSamples = legato
        ? 0
        : std::max(4, static_cast<int>(lerp(2.4f, 0.9f, parameters.pickHardness)
                                       * 0.001f * sampleRate));
    voice.excitationRemaining = voice.excitationLength;
    voice.excitationPhase = legato ? ExcitationPhase::Release
                                   : ExcitationPhase::Contact;
    if (! legato)
        voice.excitationRemaining = contactSamples;

    voice.excitationCombDelay = clampf(pluckFraction, 0.03f, 0.46f)
                              * voice.vertical.targetDelay;
    voice.excitationPulseCoefficient = std::exp(
        -twoPi * clampf(pulseCutoff, 300.0f, 0.45f * sampleRate) * inverseSampleRate_);
    voice.excitationShaper.state = 0.0f;
    voice.noiseBandCoefficient = std::exp(
        -twoPi * clampf(noiseCutoff, 250.0f, 0.45f * sampleRate) * inverseSampleRate_);
    voice.noiseShaper.state = 0.0f;
    voice.noiseBandState = 0.0f;
    voice.noiseAmplitude = 0.55f * noiseLevel;
    voice.noiseLength = std::max(8, static_cast<int>(noiseMs * 0.001f * sampleRate));
    voice.noiseRemaining = voice.noiseLength;
    voice.releaseNoiseDone = false;
}

void ElectryEngine::startVoice(Voice& voice, int midiNote, float velocity,
                               Articulation articulation) noexcept
{
    const auto& parameters = smoothedParameters_;
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    const int fret = midiNote - spec.openMidiNote;

    const bool wasRinging = voice.active;

    voice.active = true;
    voice.keyDown = true;
    voice.sustained = false;
    voice.releasing = false;
    voice.midiNote = midiNote;
    voice.fret = std::clamp(fret, 0, fretCount);
    voice.articulation = articulation;
    voice.velocity = velocity;
    voice.startOrder = ++noteSequence_;
    voice.ageSamples = 0;
    voice.noiseState = hash32(static_cast<std::uint32_t>(voice.stringIndex * 7349)
                              ^ static_cast<std::uint32_t>(midiNote * 131)
                              ^ static_cast<std::uint32_t>(
                                  static_cast<int>(articulation) * 17)
                              ^ static_cast<std::uint32_t>(noteSequence_ * 2654435761u));
    voice.baseFrequency = midiToHz(static_cast<float>(midiNote));
    voice.legatoBlend = 1.0f;
    voice.legatoFromFrequency = 0.0f;
    voice.releaseGain = 1.0f;
    voice.releaseGainTarget = 1.0f;
    voice.releaseGainCoefficient = 0.0f;
    voice.energyEnvelope = 0.0f;
    voice.outputEnergy = 0.0f;

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
    voice.tensionDepth = 0.042f * lerp(1.25f, 0.8f, parameters.stringGauge);
    if (articulation == Articulation::Slap)
        voice.tensionDepth *= 3.2f;

    // Slap drives the string against the frets: a decaying collision window
    // soft-limits displacement and adds rattle noise.
    if (articulation == Articulation::Slap)
    {
        voice.collisionRemaining =
            static_cast<int>(0.085f * static_cast<float>(sampleRate_));
        voice.collisionThreshold =
            clampf(0.34f * (0.35f + 0.65f * velocity), 0.05f, 0.6f);
    }
    else
    {
        voice.collisionRemaining = 0;
        voice.collisionThreshold = 1.0f;
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
    voice.bendStartSemitones = 0.0f;
    voice.bendTargetSemitones = 0.0f;
    voice.bendProgress = 1.0f;
    voice.releaseGain = 1.0f;
    voice.releaseGainTarget = 1.0f;

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
        const float level = smoothedParameters_.releaseNoise
                          * (spec.wound ? 0.22f : 0.13f)
                          * (0.4f + 0.6f * voice.velocity);
        voice.noiseAmplitude = level;
        voice.noiseLength = std::max(
            8, static_cast<int>(0.009f * sampleRate));
        voice.noiseRemaining = voice.noiseLength;
        voice.noiseBandCoefficient = std::exp(-twoPi * (spec.wound ? 1500.0f : 2600.0f)
                                              * inverseSampleRate_);
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
    voice.noiseRemaining = 0;
    voice.collisionRemaining = 0;
    voice.energyEnvelope = 0.0f;
    voice.outputEnergy = 0.0f;
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
    voice.noiseShaper.reset();
    voice.noiseBandState = 0.0f;
    voice.lastConfiguredSemitones = -999.0f;
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
    static constexpr std::array<float, bodyModeCount> lesPaulModes {
        108.0f, 172.0f, 306.0f, 520.0f
    };
    static constexpr std::array<float, bodyModeCount> telecasterModes {
        96.0f, 205.0f, 398.0f, 640.0f
    };
    static constexpr std::array<float, bodyModeCount> modeLevels {
        1.0f, 0.68f, 0.46f, 0.32f
    };

    const float sizeScale = std::exp2((parameters.bodySize - 0.5f) * 0.5f);
    const float q = lerp(24.0f, 15.0f, parameters.bodyWood);

    for (int mode = 0; mode < bodyModeCount; ++mode)
    {
        const auto index = static_cast<std::size_t>(mode);
        const float frequency = lerp(lesPaulModes[index], telecasterModes[index],
                                     parameters.bodyShape) * sizeScale;
        // Ash favours its upper structural modes slightly more than the
        // mahogany/maple blank.
        const float tilt = lerp(1.0f, lerp(0.85f, 1.25f, parameters.bodyWood),
                                static_cast<float>(mode) / 3.0f);
        bodyModes_[index].configure(frequency, q, modeLevels[index] * tilt,
                                    sampleRate);
    }
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
    resonance = lerp(780.0f, resonance, std::pow(tone, 0.75f));
    q *= 0.35f + 0.65f * tone;

    neckCoil_.setResonantLowpass(resonance, q, sampleRate);
    bridgeCoil_.setResonantLowpass(resonance, q, sampleRate);

    // Distance-dependent magnetic flux: hotter, closer humbuckers develop
    // more even-harmonic distortion than a low-wind single coil.
    magneticDriveNeck_ = lerp(0.52f, 0.30f, parameters.pickupType);
    magneticDriveBridge_ = lerp(0.55f, 0.34f, parameters.pickupType);

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

    configureVoicePitch(voice, false);

    // Retire the voice once the string has decayed below audibility and no
    // excitation remains pending.
    const bool excitationIdle = voice.excitationPhase == ExcitationPhase::Idle
                             && voice.noiseRemaining <= 0;
    if (excitationIdle && voice.outputEnergy < 1.0e-10f
        && voice.ageSamples > static_cast<std::uint64_t>(sampleRate_ * 0.05))
    {
        silenceVoice(voice);
    }
}

void ElectryEngine::renderVoice(Voice& voice, float& neckSum, float& bridgeSum,
                                float& bodySum) noexcept
{
    auto& vertical = voice.vertical;
    auto& horizontal = voice.horizontal;

    // Loop reads and the damping/dispersion chain.
    const auto advanceLoop = [&] (PolarisationLoop& loop, float extraFeedback)
    {
        float sample = loop.readFractional(loop.currentDelay);
        sample = loop.dispersion1.process(sample, loop.dispersionCoefficient);
        sample = loop.dispersion2.process(sample, loop.dispersionCoefficient);
        sample = loop.damping.process(sample, loop.loopDampingCoefficient);
        sample *= loop.loopGain * voice.releaseGain * extraFeedback;
        return sample;
    };

    // Pick contact briefly chokes an already ringing string.
    const float contactChoke =
        voice.excitationPhase == ExcitationPhase::Contact ? 0.90f : 1.0f;

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
        voice.noiseBandState += 0.08f * (lowpassed - voice.noiseBandState);
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
        const float window = 0.5f - 0.5f * std::cos(twoPi * clampf(progress, 0.0f, 1.0f));
        const float pulse = voice.excitationAmplitude * window
                          * voice.excitationPolarity;
        excitation = voice.excitationShaper.process(
            pulse, voice.excitationPulseCoefficient);
        if (--voice.excitationRemaining <= 0)
            voice.excitationPhase = ExcitationPhase::Idle;
    }

    const float injected = excitation + noiseSample;

    // Articulations attack the string at different angles, splitting energy
    // differently between the two polarisations.
    float verticalWeight = 0.92f;
    float horizontalWeight = 0.42f;
    switch (voice.articulation)
    {
        case Articulation::Upstroke: verticalWeight = 0.80f; horizontalWeight = 0.60f; break;
        case Articulation::HammerOn: verticalWeight = 0.95f; horizontalWeight = 0.28f; break;
        case Articulation::Slap: verticalWeight = 1.00f; horizontalWeight = 0.22f; break;
        default: break;
    }

    const float verticalTotal = verticalIn + injected * verticalWeight;
    const float horizontalTotal = horizontalIn + injected * horizontalWeight;

    vertical.line[static_cast<std::size_t>(vertical.writeIndex
                                           & (delayLineSize - 1))] = verticalTotal;
    horizontal.line[static_cast<std::size_t>(horizontal.writeIndex
                                             & (delayLineSize - 1))] = horizontalTotal;

    // The pluck-position comb: the same excitation is scattered with opposite
    // sign one comb delay behind the write head, exactly as the second
    // travelling-wave image of the excitation point.
    if (injected != 0.0f)
    {
        vertical.writeAdd(voice.excitationCombDelay,
                          -injected * verticalWeight);
        horizontal.writeAdd(voice.excitationCombDelay,
                            -injected * horizontalWeight);
    }

    // Pickups read the string displacement at their positions: the freshly
    // written bridge-bound sample minus its reflection image at the pickup
    // delay. The magnetic pole senses the perpendicular polarisation more
    // strongly than the parallel one. Taps are taken before the write index
    // advances so the interpolator never touches a stale slot.
    const auto pickupTap = [&] (float delay)
    {
        const float verticalTap = verticalTotal - vertical.readFractional(delay);
        const float horizontalTap = horizontalTotal - horizontal.readFractional(delay);
        return 0.85f * verticalTap + 0.35f * horizontalTap;
    };

    float neckTap = pickupTap(voice.pickupDelayNeck);
    float bridgeTap = pickupTap(voice.pickupDelayBridge);

    vertical.writeIndex = (vertical.writeIndex + 1) & (delayLineSize - 1);
    horizontal.writeIndex = (horizontal.writeIndex + 1) & (delayLineSize - 1);

    vertical.currentDelay += vertical.delaySmoothing
                           * (vertical.targetDelay - vertical.currentDelay);
    horizontal.currentDelay += horizontal.delaySmoothing
                             * (horizontal.targetDelay - horizontal.currentDelay);
    neckTap = voice.apertureNeck.process(neckTap, voice.apertureNeckCoefficient);
    bridgeTap = voice.apertureBridge.process(bridgeTap, voice.apertureBridgeCoefficient);

    // Distance-dependent flux nonlinearity, second-order dominant. The 0.5
    // keeps a six-string strum inside the output guard's linear region.
    const auto magneticTransfer = [] (float displacement, float drive)
    {
        const float x = clampf(displacement * drive, -0.9f, 0.9f);
        return (x + x * x * (0.55f + 0.30f * x)) / std::max(drive, 1.0e-3f);
    };
    neckSum += 0.5f * magneticTransfer(neckTap, magneticDriveNeck_);
    bridgeSum += 0.5f * magneticTransfer(bridgeTap, magneticDriveBridge_);

    // The bridge passes string vibration and playing noise into the body.
    bodySum += 0.5f * (verticalTotal + horizontalTotal) + 1.6f * noiseSample;
    if (voice.articulation == Articulation::Slap
        && voice.excitationPhase != ExcitationPhase::Idle)
        bodySum += 2.4f * excitation;

    // Energy tracking feeds tension modulation and voice retirement. The
    // release side follows the string's own decay scale so the attack pitch
    // glide relaxes over hundreds of milliseconds, as measured tension
    // modulation does.
    const float instantaneous = verticalSample * verticalSample
                              + horizontalSample * horizontalSample;
    const float coefficient = instantaneous > voice.energyEnvelope ? 0.004f : 0.00006f;
    voice.energyEnvelope += coefficient * (instantaneous - voice.energyEnvelope);
    voice.outputEnergy = voice.energyEnvelope;

    if (voice.releasing)
        voice.releaseGain += voice.releaseGainCoefficient
                           * (voice.releaseGainTarget - voice.releaseGain);

    voice.ageSamples++;
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
                || std::abs(s.bodyShape - t.bodyShape) > 1.0e-4f;

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
            s.bendTimeSeconds = t.bendTimeSeconds;
            s.pickupSelector = t.pickupSelector;
            smoothTowards(s.outputGain, t.outputGain);

            pitchBendSemitones_ += 0.35f * (pitchBendTarget_ - pitchBendSemitones_);

            if (pickupDirty)
                configurePickupFilters();
            if (bodyDirty)
                configureBody();
            refreshVoicingIfNeeded();

            neckMix_ += pickupMixCoefficient_ * (neckMixTarget_ - neckMix_);
            bridgeMix_ += pickupMixCoefficient_ * (bridgeMixTarget_ - bridgeMix_);
            smoothedOutputGain_ = s.outputGain;
            smoothedBodyLevel_ = s.bodyResonance;

            for (auto& voice : voices_)
                updateVoiceControl(voice);
            updateActiveVoiceCount();
        }
        controlCountdown_--;

        float neckSum = 0.0f;
        float bridgeSum = 0.0f;
        float bodySum = 0.0f;

        for (auto& voice : voices_)
            if (voice.active)
                renderVoice(voice, neckSum, bridgeSum, bodySum);

        // Solid-body structural colour, sensed by the pickups alongside the
        // strings.
        float body = 0.0f;
        for (auto& mode : bodyModes_)
            body += mode.process(0.02f * bodySum);
        body *= smoothedBodyLevel_;

        const float neckOut = neckCoil_.process(neckSum + 0.6f * body);
        const float bridgeOut = bridgeCoil_.process(bridgeSum + 0.4f * body);
        // Humbuckers are hotter than low-wind single coils.
        const float pickupLevel = lerp(1.32f, 1.0f, smoothedParameters_.pickupType);

        float output = (neckOut * neckMix_ + bridgeOut * bridgeMix_) * pickupLevel;
        output = outputDc_.process(output, outputDcCoefficient_);
        output = softLimit(output) * smoothedOutputGain_;

        if (! finitef(output))
        {
            // A non-finite sample means some state has been corrupted by
            // hostile input; recover silently rather than latching.
            for (auto& voice : voices_)
            {
                silenceVoice(voice);
                voice.vertical.clear();
                voice.horizontal.clear();
            }
            neckCoil_.reset();
            bridgeCoil_.reset();
            outputDc_.reset();
            for (auto& mode : bodyModes_)
                mode.reset();
            output = 0.0f;
        }

        left[sample] = output;
        right[sample] = output;
    }
}

void ElectryEngine::updateActiveVoiceCount() noexcept
{
    int count = 0;
    for (const auto& voice : voices_)
        if (voice.active)
            ++count;
    activeVoiceCount_ = count;
}

int ElectryEngine::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount_;
}

float ElectryEngine::currentSoundingSemitoneOffset(const Voice& voice) const noexcept
{
    return lerp(voice.bendStartSemitones, voice.bendTargetSemitones,
                smoothStep(voice.bendProgress))
         + pitchBendSemitones_;
}

} // namespace electry
