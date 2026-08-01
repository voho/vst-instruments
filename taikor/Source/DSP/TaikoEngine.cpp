#include "DSP/TaikoEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace taikor
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr float piFloat = 3.14159265358979f;

// Air at 20 degrees Celsius. Both constants are load-bearing rather than
// decorative: the first sets how much the air hanging off the head lowers its
// modes, the second sets how hard the enclosed body pushes back.
constexpr float airDensity = 1.2041f;
constexpr float soundSpeed = 343.0f;

// The head's areal density, in kg/m^2, from a thin synthetic film to a thick
// cowhide. Geometric, because the audible parameter is the wave speed and that
// goes as the square root of the ratio.
constexpr float minimumArealDensity = 0.30f;
constexpr float maximumArealDensity = 1.60f;

// Head tension in N/m. A tacked taiko head is under enormous tension compared
// with a drum-kit head, which is why it sits so far above its own diameter.
constexpr float minimumTension = 1200.0f;
constexpr float maximumTension = 22000.0f;
// Backstops on the resolved geometry. They are set wide enough that nothing
// reachable from the (already bounded) controls can touch them, because an
// octave that clamped would stop being an octave: two adjacent octaves would
// resolve to the same drum and the keyboard would go flat at the top. They
// exist only so a future change to the ranges above degrades rather than
// divides by zero.
constexpr float tensionFloor = 3.0f;
constexpr float tensionCeiling = 3.0e7f;
constexpr float radiusFloor = 0.008f;
constexpr float radiusCeiling = 2.50f;

// The bachi. A taiko stick is a heavy piece of hardwood; the hardness control
// moves the Hertz contact stiffness across three orders of magnitude, from a
// felt-wrapped odaiko beater to a stick of seasoned oak.
//
// The mass is quoted for the reference drum and scales with the one actually
// being played, because nobody hits a shime-daiko with an odaiko club: a stick
// is chosen to suit the drum. Leaving it fixed made the smallest drums roughly
// twenty-five decibels louder than the largest, which is a property of the
// wrong stick rather than of the instrument.
constexpr float bachiMass = 0.19f;
constexpr float referenceRadius = 0.275f;
constexpr float minimumBachiScale = 0.30f;
constexpr float maximumBachiScale = 2.20f;
constexpr float minimumContactStiffness = 2.0e6f;
constexpr float maximumContactStiffness = 6.0e8f;
constexpr float restitution = 0.42f;

// Impact speed in m/s at the softest and hardest MIDI velocity.
constexpr float minimumImpactSpeed = 0.45f;
constexpr float maximumImpactSpeed = 6.0f;

// One overall level constant. The model produces physical head displacements,
// which are on the order of tens of microns; this is the only place a number
// is chosen for how it sounds rather than for what it means, and it is a
// single scalar so it cannot distort any relationship inside the model.
constexpr float modelScale = 128.0f;

// Radiation damping is the one loss term whose absolute size depends on how
// the drum is mounted and how much of the body is free to move, none of which
// this model represents. Its shape - which modes lose energy and how that
// changes with size and material - is physical; this scalar sets the overall
// depth so the default drum's fundamental lands where a nagado-daiko's does.
constexpr float radiationCalibration = 0.11f;

// How efficiently the shell's ring modes reach the microphones compared with
// the head's. The body radiates from a curved surface the pair is beside
// rather than in front of, and this model does not describe that geometry, so
// one scalar stands in for it - the same arrangement as radiationCalibration.
constexpr float shellCalibration = 4200.0f;

// Level of the airborne impact path relative to what the head radiates. It
// stands in for the contact patch's radiating area and directivity, neither of
// which this model describes; everything about how that path varies with
// stroke, position and distance is geometry and is computed, not chosen.
constexpr float directCalibration = 0.0021f;

[[nodiscard]] float clampFloat (float value, float low, float high) noexcept
{
    if (! (value == value)) // NaN
        return low;
    return value < low ? low : (value > high ? high : value);
}

[[nodiscard]] float lerp (float a, float b, float t) noexcept
{
    return a + (b - a) * t;
}

// Geometric interpolation, for the quantities whose audible spacing is
// multiplicative rather than additive.
[[nodiscard]] float geometricLerp (float a, float b, float t) noexcept
{
    return a * std::pow (b / a, t);
}

const std::array<ArticulationMetadata, articulationCount> articulationTable {{
    { Articulation::Don, "Don", "don", "don",
      "Full centre strike: the open voice of the drum", 0 },
    { Articulation::Do, "Do", "do", "do",
      "Open stroke a little off centre, quicker than a Don", 1 },
    { Articulation::Tsu, "Tsu", "tsu", "tsu",
      "Damped centre, the free hand resting on the head", 2 },
    { Articulation::Su, "Su", "su", "su",
      "Ghost stroke, barely sounded", 3 },
    { Articulation::DonRim, "Don Rim", "donrim", "don",
      "Head and rim struck together for a rim shot", 4 },
    { Articulation::Ka, "Ka", "ka", "ka",
      "On the edge of the head, near the tacks", 5 },
    { Articulation::Kara, "Kara", "kara", "kara",
      "Extreme edge, thin and cutting", 6 },
    { Articulation::Ko, "Ko", "ko", "ko",
      "Light tap at mid radius", 7 },
    { Articulation::Katsu, "Katsu", "katsu", "katsu",
      "Bachi on the wooden shell", 8 },
    { Articulation::Buzz, "Buzz", "buzz", "zu",
      "Press roll: the stick stays on the head", 9 },
    { Articulation::Flam, "Flam", "flam", "doko",
      "Grace note into a full stroke", 10 },
    { Articulation::Bachi, "Bachi", "bachi", "kata",
      "Stick against stick, with no drum at all", 11 },
}};
} // namespace

const ArticulationMetadata& getArticulationMetadata (Articulation articulation) noexcept
{
    const auto index = static_cast<std::size_t> (articulation);
    return articulationTable[index < articulationCount ? index : 0u];
}

std::string_view getArticulationDisplayName (Articulation articulation) noexcept
{
    return getArticulationMetadata (articulation).displayName;
}

std::string_view getArticulationSlug (Articulation articulation) noexcept
{
    return getArticulationMetadata (articulation).slug;
}

std::optional<Articulation> articulationForMidiNote (int midiNote) noexcept
{
    if (midiNote < lowestPlayableNote || midiNote > highestPlayableNote)
        return std::nullopt;
    return static_cast<Articulation> (midiNote % 12);
}

std::optional<int> octaveOffsetForMidiNote (int midiNote) noexcept
{
    if (midiNote < lowestPlayableNote || midiNote > highestPlayableNote)
        return std::nullopt;
    // Integer division is only floor division for non-negative operands, and
    // the playable range starts well above zero, so this is exact.
    return midiNote / 12 - referenceNote / 12;
}

int midiNoteFor (Articulation articulation, int octaveOffset) noexcept
{
    const auto pitchClass = static_cast<int> (articulation);
    const auto octave = std::clamp (octaveOffset, lowestOctaveOffset, highestOctaveOffset);
    return referenceNote + octave * 12 + pitchClass;
}

// ---------------------------------------------------------------------------
// Static physical tables
// ---------------------------------------------------------------------------

const std::array<TaikoEngine::MembraneModeEntry, TaikoEngine::modeEntryCount>&
TaikoEngine::membraneModes() noexcept
{
    // Zeros of J_m, in the order (m, n). Everything up to about 12.3 is
    // included: past that the modes are both inaudibly quiet and so heavily
    // damped that they have gone before the attack has finished. The four
    // axisymmetric entries must come first, because they are the ones the
    // cavity splits and the code below relies on that grouping.
    static const std::array<MembraneModeEntry, modeEntryCount> table {{
        { 0, 2.4048255576957728 },
        { 0, 5.5200781102863106 },
        { 0, 8.6537279129110122 },
        { 0, 11.791534439014282 },
        { 1, 3.8317059702075123 },
        { 1, 7.0155866698156188 },
        { 1, 10.173468135062722 },
        { 2, 5.1356223018406826 },
        { 2, 8.4172441403998649 },
        { 2, 11.619841172149059 },
        { 3, 6.3801618959239835 },
        { 3, 9.7610231299816697 },
        { 3, 13.015200721698434 },
        { 4, 7.5883424345038044 },
        { 4, 11.064709488501185 },
        { 5, 8.7714838159599540 },
        { 5, 12.338604197466944 },
        { 6, 9.9361095242176849 },
        { 7, 11.086370019245084 },
        { 8, 12.225092264004655 },
    }};
    return table;
}

const TaikoEngine::StrikeProfile& TaikoEngine::strikeProfile (
    Articulation articulation) noexcept
{
    // The whole articulation vocabulary is strike geometry plus what the other
    // hand is doing. Nothing here re-voices the drum: a Ka is bright because
    // hitting the head at 0.78 of its radius drives the modes with a
    // circumferential order and barely moves the axisymmetric ones, which is
    // exactly why it is bright on a real taiko too.
    static const std::array<StrikeProfile, articulationCount> table {{
        // radius, hardness, membrane, shell, noise, level, mute,
        //   contacts, rim, shellFreq, shellDecay
        { 0.00f, 1.00f, 1.00f, 0.18f, 1.00f, 1.00f, 0.00f, 1, 0.00f, 1.0f, 1.0f },  // Don
        { 0.24f, 0.94f, 0.96f, 0.16f, 0.95f, 0.90f, 0.00f, 1, 0.00f, 1.0f, 1.0f },  // Do
        { 0.30f, 1.00f, 0.82f, 0.14f, 1.05f, 0.68f, 0.85f, 1, 0.00f, 1.0f, 1.0f },  // Tsu
        { 0.34f, 0.86f, 0.55f, 0.10f, 1.30f, 0.30f, 0.35f, 1, 0.00f, 1.0f, 1.0f },  // Su
        { 0.86f, 1.15f, 0.92f, 0.72f, 1.55f, 0.90f, 0.00f, 1, 0.80f, 1.0f, 0.85f }, // Don Rim
        { 0.78f, 1.15f, 0.88f, 0.34f, 1.20f, 0.86f, 0.00f, 1, 0.18f, 1.0f, 1.0f },  // Ka
        { 0.93f, 1.28f, 0.72f, 0.46f, 1.35f, 0.74f, 0.10f, 1, 0.34f, 1.0f, 1.0f },  // Kara
        { 0.55f, 0.90f, 0.70f, 0.16f, 1.05f, 0.52f, 0.20f, 1, 0.00f, 1.0f, 1.0f },  // Ko
        { 0.99f, 1.30f, 0.06f, 0.86f, 1.45f, 0.70f, 0.00f, 1, 0.45f, 1.0f, 0.55f }, // Katsu
        { 0.62f, 0.80f, 0.74f, 0.20f, 1.60f, 0.62f, 0.55f, 7, 0.00f, 1.0f, 1.0f },  // Buzz
        { 0.10f, 1.00f, 1.00f, 0.20f, 1.05f, 1.00f, 0.00f, 2, 0.00f, 1.0f, 1.0f },  // Flam
        { 0.00f, 1.70f, 0.00f, 1.00f, 1.20f, 0.55f, 0.00f, 1, 0.00f, 5.4f, 0.16f,
          false }, // Bachi: two sticks, not the drum
    }};

    const auto index = static_cast<std::size_t> (articulation);
    return table[index < articulationCount ? index : 0u];
}

double TaikoEngine::besselJ (int order, double x) noexcept
{
    if (order < 0)
        return 0.0;

    if (x < 0.0)
    {
        const double mirrored = besselJ (order, -x);
        return (order % 2 == 0) ? mirrored : -mirrored;
    }

    // Ascending series. Only ever evaluated while a stroke or a drum is being
    // set up, never in the render loop, and the largest argument the mode
    // table can produce is about 12.3, where the series is comfortably stable
    // in double precision.
    double term = 1.0;
    for (int i = 1; i <= order; ++i)
        term *= (x * 0.5) / static_cast<double> (i);

    double sum = term;
    const double quarterSquared = 0.25 * x * x;

    for (int k = 1; k <= 64; ++k)
    {
        term *= -quarterSquared
              / (static_cast<double> (k) * static_cast<double> (k + order));
        sum += term;
        if (k > 24 && std::abs (term) < 1.0e-17)
            break;
    }

    return sum;
}

float TaikoEngine::radiationEfficiency (int order, float ka) noexcept
{
    // A membrane mode of circumferential order m has 2m sign changes around
    // the head, so the air it pushes out on one side it pulls in on the other
    // and it radiates as a 2m-pole: efficiency rises as (ka)^(2m+2) until ka
    // reaches the mode's own order and then saturates. This single expression
    // is why a centre strike is heard as a boom and an edge strike as a slap.
    const float x = ka > 1.0e-4f ? ka : 1.0e-4f;
    const float exponent = 2.0f * static_cast<float> (order) + 2.0f;
    const float raised = std::pow (x, exponent);
    const float knee = std::pow (1.0f + 1.4f * static_cast<float> (order), exponent);
    return raised / (raised + knee);
}

float TaikoEngine::readDelayLine (const std::array<float, directLineSize>& line,
                                  int writeIndex, float delaySamples) noexcept
{
    static_assert ((directLineSize & (directLineSize - 1)) == 0,
                   "the airborne delay line must be a power of two");
    constexpr int mask = directLineSize - 1;

    delaySamples = clampFloat (delaySamples, 0.0f,
                               static_cast<float> (directLineSize - 2));

    const float position = static_cast<float> (writeIndex) - delaySamples;
    const int index = static_cast<int> (std::floor (position));
    const float fraction = position - static_cast<float> (index);

    // `index` sits just past the requested delay, so the other end of the
    // interpolation is the sample after it, towards the present. Reaching
    // backwards instead adds the fractional part to the delay rather than
    // subtracting it - a quarter of a sample becomes one and three quarters,
    // with a step at every integer - and this delay is the inter-microphone
    // timing cue that places a stroke across the image.
    const float older = line[static_cast<std::size_t> (index & mask)];
    const float newer = line[static_cast<std::size_t> ((index + 1) & mask)];
    return older + (newer - older) * fraction;
}

void TaikoEngine::solveAxisymmetricBranch (float diagonalB, float diagonalR,
                                           float offDiagonal, int branch,
                                           float& eigenvalue, float& vectorB,
                                           float& vectorR) noexcept
{
    const float centre = 0.5f * (diagonalB + diagonalR);
    const float half = 0.5f * (diagonalB - diagonalR);
    const float discriminant = std::sqrt (half * half + offDiagonal * offDiagonal);

    eigenvalue = branch == 0 ? centre + discriminant : centre - discriminant;

    vectorB = offDiagonal;
    vectorR = eigenvalue - diagonalB;

    // Whether the two heads are coupled at all is a structural question, not a
    // question of magnitude: the deciding quantity is the off-diagonal, and
    // testing the vector's own length against a fixed epsilon does not answer
    // it. These values run from 1e5 to 1e10, so the float residue left in
    // `eigenvalue - diagonalB` when the heads are uncoupled is itself far
    // larger than any absolute epsilon.
    const float couplingScale =
        1.0e-6f * std::max (std::abs (diagonalB - diagonalR), 1.0f);

    if (std::abs (offDiagonal) > couplingScale)
    {
        const float length = std::sqrt (vectorB * vectorB + vectorR * vectorR);
        vectorB /= length;
        vectorR /= length;
        return;
    }

    // Uncoupled: this eigenvector is one head or the other, and which one is
    // decided by the diagonal the eigenvalue came from, not by the branch
    // index. The resonant head is lighter than the batter head and so is
    // usually the higher of the two; picking by branch handed both branches to
    // it, the batter share was zero on both, and a centre strike lost its boom
    // entirely the moment Air Coupling reached zero.
    const float toBatter = std::abs (eigenvalue - diagonalB);
    const float toResonant = std::abs (eigenvalue - diagonalR);
    const bool tied = std::abs (toBatter - toResonant)
                    <= 1.0e-6f * std::max (1.0f, std::abs (eigenvalue));
    // Two identical heads give one repeated eigenvalue; there the branch index
    // is the only thing that can separate them.
    const bool isBatter = tied ? (branch == 0) : (toBatter < toResonant);
    vectorB = isBatter ? 1.0f : 0.0f;
    vectorR = isBatter ? 0.0f : 1.0f;
}

std::uint32_t TaikoEngine::hash32 (std::uint32_t value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float TaikoEngine::signedUnitFromHash (std::uint32_t value) noexcept
{
    return static_cast<float> (static_cast<double> (hash32 (value)) / 2147483647.5 - 1.0);
}

float TaikoEngine::nextNoise (std::uint32_t& state) noexcept
{
    state = state * 1664525u + 1013904223u;
    const auto bits = (state >> 9) | 0x3f800000u;
    float unit;
    std::memcpy (&unit, &bits, sizeof (unit));
    return 2.0f * (unit - 1.5f);
}

// ---------------------------------------------------------------------------
// Construction and lifecycle
// ---------------------------------------------------------------------------

TaikoEngine::TaikoEngine() noexcept
{
    applied_ = sanitise (EngineParameters {});
    prepare (48000.0, 512);
}

EngineParameters TaikoEngine::sanitise (const EngineParameters& parameters) noexcept
{
    EngineParameters result;
    result.headDiameter = clampFloat (parameters.headDiameter, 0.15f, 1.20f);
    result.bodyDepth = clampFloat (parameters.bodyDepth, 0.0f, 1.0f);
    result.tension = clampFloat (parameters.tension, 0.0f, 1.0f);
    result.headMaterial = clampFloat (parameters.headMaterial, 0.0f, 1.0f);
    result.shellMaterial = clampFloat (parameters.shellMaterial, 0.0f, 1.0f);
    result.resonantTension = clampFloat (parameters.resonantTension, 0.0f, 1.0f);
    result.cavityCoupling = clampFloat (parameters.cavityCoupling, 0.0f, 1.0f);
    result.headDamping = clampFloat (parameters.headDamping, 0.0f, 1.0f);
    result.shellResonance = clampFloat (parameters.shellResonance, 0.0f, 1.0f);
    result.pitch = clampFloat (parameters.pitch, -24.0f, 24.0f);
    result.bachiHardness = clampFloat (parameters.bachiHardness, 0.0f, 1.0f);
    result.strikePosition = clampFloat (parameters.strikePosition, -1.0f, 1.0f);
    result.velocityDepth = clampFloat (parameters.velocityDepth, 0.0f, 1.0f);
    result.tensionModulation = clampFloat (parameters.tensionModulation, 0.0f, 1.0f);
    result.strikeNoise = clampFloat (parameters.strikeNoise, 0.0f, 1.0f);
    result.humanise = clampFloat (parameters.humanise, 0.0f, 1.0f);
    result.octaveBody = clampFloat (parameters.octaveBody, 0.0f, 1.0f);
    result.micDistance = clampFloat (parameters.micDistance, 0.0f, 1.0f);
    result.micSpread = clampFloat (parameters.micSpread, 0.0f, 1.0f);
    result.stereoWidth = clampFloat (parameters.stereoWidth, 0.0f, 1.0f);
    result.drive = clampFloat (parameters.drive, 0.0f, 1.0f);
    result.outputGain = clampFloat (parameters.outputGain, 0.0f, 2.0f);
    return result;
}

void TaikoEngine::prepare (double sampleRate, int maxBlockSize) noexcept
{
    sampleRate_ = std::clamp (sampleRate, minimumSupportedSampleRate,
                              maximumSupportedSampleRate);
    inverseSampleRate_ = static_cast<float> (1.0 / sampleRate_);
    maxBlockSize_ = maxBlockSize > 0 ? maxBlockSize : 512;

    const auto rate = static_cast<float> (sampleRate_);
    handDampingCoefficient_ = 1.0f - std::exp (-1.0f / (0.020f * rate));
    pitchBendCoefficient_ = 1.0f - std::exp (-1.0f / (0.035f * rate));
    gainSmoothing_ = 1.0f - std::exp (-1.0f / (0.015f * rate));
    // 12 Hz corner: low enough to leave an odaiko's fundamental alone, high
    // enough to remove the offset a one-sided strike leaves behind.
    dcCoefficient_ = std::exp (-2.0f * piFloat * 12.0f / rate);
    meterReleaseMultiplier_ = std::exp (-1.0f / (0.30f * rate));
    visualDecayMultiplier_ = std::exp (-1.0f / (0.35f * rate));

    prepared_ = true;
    drumCacheValid_ = false;
    reset();
}

void TaikoEngine::reset() noexcept
{
    for (auto& voice : voices_)
        silenceVoice (voice);

    handDamping_ = handDampingTarget_;
    pitchBend_ = pitchBendTarget_;
    smoothedOutputGain_ = applied_.outputGain;
    smoothedDrive_ = applied_.drive;
    smoothedWidth_ = applied_.stereoWidth;
    dcInputLeft_ = dcInputRight_ = 0.0f;
    dcOutputLeft_ = dcOutputRight_ = 0.0f;
    driveAdaaLeft_ = driveAdaaRight_ = 0.0f;
    meterLeft_ = meterRight_ = 0.0f;
    visualLevel_ = 0.0f;
    noteSequence_ = 0;
    silentSamples_ = idleFreezeSamples;
    idleFrozen_ = true;

    outputLevelLeft_.store (0.0f, std::memory_order_relaxed);
    outputLevelRight_.store (0.0f, std::memory_order_relaxed);
    visualStrikeLevel_.store (0.0f, std::memory_order_relaxed);
    activeVoiceCount_.store (0, std::memory_order_relaxed);
}

void TaikoEngine::setParameters (const EngineParameters& parameters) noexcept
{
    const auto next = sanitise (parameters);

    // Only the terms that actually enter the physical solve invalidate the
    // cached drums. Mic placement does too, because the mode weights are baked
    // per stroke; the output stage does not.
    const bool physicsChanged =
        next.headDiameter != applied_.headDiameter
        || next.bodyDepth != applied_.bodyDepth
        || next.tension != applied_.tension
        || next.headMaterial != applied_.headMaterial
        || next.shellMaterial != applied_.shellMaterial
        || next.resonantTension != applied_.resonantTension
        || next.cavityCoupling != applied_.cavityCoupling
        || next.headDamping != applied_.headDamping
        || next.shellResonance != applied_.shellResonance
        || next.pitch != applied_.pitch
        || next.octaveBody != applied_.octaveBody
        || next.micDistance != applied_.micDistance
        || next.micSpread != applied_.micSpread;

    applied_ = next;

    if (physicsChanged)
        drumCacheValid_ = false;
}

void TaikoEngine::setHandDamping (float normalised) noexcept
{
    handDampingTarget_ = clampFloat (normalised, 0.0f, 1.0f);
}

void TaikoEngine::setPitchBend (float normalisedBipolar) noexcept
{
    pitchBendTarget_ = clampFloat (normalisedBipolar, -1.0f, 1.0f);
}

void TaikoEngine::allSoundsOff() noexcept
{
    for (auto& voice : voices_)
        silenceVoice (voice);
    updateActiveVoiceCount();

    // Cutting every voice is already a discontinuity, so there is nothing left
    // for the shared DC and drive path to be continuous with. Clearing it is
    // what makes a panic actually silent instead of merely inaudible: the DC
    // blocker's own corner would otherwise take a further quarter of a second
    // to ring out.
    dcInputLeft_ = dcInputRight_ = 0.0f;
    dcOutputLeft_ = dcOutputRight_ = 0.0f;
    driveAdaaLeft_ = driveAdaaRight_ = 0.0f;
    silentSamples_ = idleFreezeSamples;
    idleFrozen_ = true;

    visualLevel_ = 0.0f;
    visualStrikeLevel_.store (0.0f, std::memory_order_relaxed);
}

void TaikoEngine::silenceVoice (Voice& voice) noexcept
{
    voice.active = false;
    voice.modeCount = 0;
    voice.activeModeCount = 0;
    voice.contactCount = 0;
    voice.nextContact = 0;
    voice.contactRemaining = 0u;
    voice.ageSamples = 0;
    voice.handGain = 1.0f;
    voice.retireGain = 1.0f;
    voice.retireStep = 0.0f;
    voice.peakLevel = 0.0f;
    voice.tensionEnvelope = 0.0f;
    voice.appliedTensionShift = 1.0f;
    voice.noiseBandState = 0.0f;
    for (auto& mode : voice.modes)
    {
        mode.resonator.clear();
        mode.drive = 0.0f;
        mode.micLeft = 0.0f;
        mode.micRight = 0.0f;
    }
}

void TaikoEngine::updateActiveVoiceCount() noexcept
{
    int count = 0;
    for (const auto& voice : voices_)
        if (voice.active)
            ++count;
    activeVoiceCount_.store (count, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Resolving the physical drum
// ---------------------------------------------------------------------------

TaikoEngine::DrumState TaikoEngine::resolveDrum (int octaveOffset) const noexcept
{
    return resolveDrumFor (applied_, 2.0f * pitchBend_, octaveOffset);
}

TaikoEngine::DrumState TaikoEngine::resolveDrumFor (const EngineParameters& raw,
                                                    float pitchBendSemitones,
                                                    int octaveOffset) noexcept
{
    const auto applied = sanitise (raw);
    DrumState drum;

    const auto octave = static_cast<float> (
        std::clamp (octaveOffset, lowestOctaveOffset, highestOctaveOffset));

    // An octave can be bought either by halving the drum or by quadrupling the
    // tension; both land on exactly the same pitch, and octaveBody chooses the
    // mixture. They do not sound the same, because the air load, the cavity
    // stiffness and the radiation efficiency all depend on the radius and none
    // of them scale with the tension.
    const float body = applied.octaveBody;
    const float radiusFactor = std::exp2 (-body * octave);
    const float tensionOctaveFactor = std::exp2 (2.0f * (1.0f - body) * octave);

    // The pitch control and the wheel are head tension, because that is what
    // tuning a drum is.
    const float semitones = applied.pitch + pitchBendSemitones;
    const float tensionPitchFactor = std::exp2 (2.0f * semitones / 12.0f);

    drum.radius = clampFloat (0.5f * applied.headDiameter * radiusFactor,
                              radiusFloor, radiusCeiling);
    drum.depth = clampFloat (applied.headDiameter * radiusFactor
                                 * (0.40f + 0.90f * applied.bodyDepth),
                             0.04f, 2.0f);

    const float baseTension =
        geometricLerp (minimumTension, maximumTension, applied.tension);
    drum.tension = clampFloat (baseTension * tensionOctaveFactor * tensionPitchFactor,
                               tensionFloor, tensionCeiling);
    drum.resonantTension = clampFloat (
        drum.tension * (0.85f + 0.30f * applied.resonantTension),
        tensionFloor, tensionCeiling);

    drum.batterDensity =
        geometricLerp (minimumArealDensity, maximumArealDensity, applied.headMaterial);
    // The far head of a taiko is cut from the same hide but is not the one
    // being hit, so it is traditionally a touch lighter.
    drum.resonantDensity = drum.batterDensity * 0.92f;

    drum.waveSpeed = std::sqrt (drum.tension / drum.batterDensity);
    drum.resonantWaveSpeed = std::sqrt (drum.resonantTension / drum.resonantDensity);

    // Hysteretic loss in the head material. A thick hide loses more per cycle
    // than a thin film; the damping control scales what the material gives.
    //
    // The cubic term is what gives the top of the control any authority. Half
    // of the fundamental's loss is radiation, which no amount of damping can
    // touch, so a linear map spends its whole range shortening the tail by
    // about a third - far less than laying a cloth over a real head does. The
    // curve leaves the lower two thirds of the control roughly linear and lets
    // the top of it genuinely deaden the drum.
    const float materialLoss = 0.0040f + 0.0120f * applied.headMaterial;
    const float damping = applied.headDamping;
    drum.headLossFactor =
        materialLoss * (0.55f + 1.30f * damping + 6.0f * damping * damping * damping);

    // Loss into the hoop and the tacks. A soft laminated shell absorbs far
    // more of what reaches the rim than a dense carved log does.
    drum.edgeLoss = (0.40f + 1.80f * (1.0f - applied.shellMaterial))
                  * (0.60f + 1.40f * applied.headDamping);

    // Cavity stiffness per unit area. The per-mode 4/lambda^2 volume weighting
    // is applied where the modes are built, since it belongs to the mode.
    drum.cavityStiffness = applied.cavityCoupling * airDensity * soundSpeed
                         * soundSpeed / drum.depth;

    drum.radiationScale = radiationCalibration;

    // The wooden shell's ring modes. This is the standard thin-cylinder result
    // f_n = n(n^2-1)/sqrt(n^2+1) * h/(2 pi R^2) * sqrt(E/(12 rho (1-nu^2))),
    // so the shell material moves both the frequencies and their spacing.
    const float youngsModulus = lerp (4.0e9f, 14.0e9f, applied.shellMaterial);
    const float woodDensity = lerp (450.0f, 850.0f, applied.shellMaterial);
    constexpr float poisson = 0.30f;
    const float plateSpeed = std::sqrt (
        youngsModulus / (12.0f * woodDensity * (1.0f - poisson * poisson)));
    const float wallThickness = 2.0f * drum.radius
                              * (0.035f + 0.045f * applied.shellMaterial);
    const float geometry = wallThickness / (2.0f * piFloat * drum.radius * drum.radius);
    const float shellQ = 18.0f + 70.0f * applied.shellMaterial;

    for (int index = 0; index < shellResonatorCount; ++index)
    {
        const auto n = static_cast<float> (index + 2);
        const float coefficient = n * (n * n - 1.0f) / std::sqrt (n * n + 1.0f);
        const float frequency = coefficient * geometry * plateSpeed;
        drum.shellFrequencies[static_cast<std::size_t> (index)] = frequency;
        drum.shellDecays[static_cast<std::size_t> (index)] = piFloat * frequency / shellQ;
    }

    // Half the mass of the cylinder wall, which is what a ring mode actually
    // has to move. A carved zelkova body is heavy and barely responds; a light
    // laminated shell rings far more for the same stroke.
    drum.shellModalMass = std::max (
        0.5f * woodDensity * 2.0f * piFloat * drum.radius * drum.depth * wallThickness,
        0.05f);

    drum.shellLevel = applied.shellResonance;

    // The close pair. At zero spread both microphones sit over the centre of
    // the head and the instrument is exactly mono; opening it walks them out
    // towards opposite sides of the rim, where every mode with a
    // circumferential order reaches them with a different sign.
    drum.micRadius = drum.radius * (0.10f + 0.68f * applied.micSpread);
    constexpr float micReference = 0.60f;      // radians, off the mode axis
    const float separation = 2.2f * applied.micSpread;
    drum.micAngleLeft = micReference + 0.5f * separation;
    drum.micAngleRight = micReference - 0.5f * separation;

    drum.micDistanceMetres = lerp (0.03f, 0.40f, applied.micDistance);
    // Close microphones lift the low end. The depth follows the same distance,
    // so backing the pair off thins the drum exactly as it does in a room.
    drum.micProximity = 1.20f * (0.12f / (0.12f + drum.micDistanceMetres));

    return drum;
}

void TaikoEngine::refreshDrumIfNeeded() noexcept
{
    if (drumCacheValid_)
        return;

    for (int octave = lowestOctaveOffset; octave <= highestOctaveOffset; ++octave)
        drumCache_[static_cast<std::size_t> (octave - lowestOctaveOffset)] =
            resolveDrum (octave);

    drumCacheBend_ = pitchBend_;
    drumCacheValid_ = true;
}

// ---------------------------------------------------------------------------
// The stick meeting the head
// ---------------------------------------------------------------------------

void TaikoEngine::solveContact (const DrumState& drum, const StrikeProfile& profile,
                                float bachiHardness, float impactSpeed,
                                float& contactSeconds, float& peakForce) noexcept
{
    const float stiffness = clampFloat (
        geometricLerp (minimumContactStiffness, maximumContactStiffness,
                       bachiHardness) * profile.hardnessScale,
        minimumContactStiffness * 0.25f, maximumContactStiffness * 4.0f);

    // Hertz impact of a rounded tip against a stiff surface. The contact time
    // falls as the fifth root of the impact speed, which is the whole reason a
    // hard stroke is brighter and not merely louder.
    const float speed = std::max (impactSpeed, 0.05f);
    const float mass = bachiMass
                     * clampFloat (drum.radius / referenceRadius, minimumBachiScale,
                                   maximumBachiScale);
    const float hertzTime = 2.94f
                          * std::pow (5.0f * mass / (4.0f * stiffness), 0.4f)
                          * std::pow (speed, -0.2f);

    // A membrane does not behave like a half-space: it carries the energy away
    // resistively at 8*sqrt(T*sigma), so the stick cannot leave the head any
    // faster than the wave does. This is the floor that stops a very hard
    // bachi from producing an impulse the drum could never make.
    const float membraneImpedance =
        8.0f * std::sqrt (drum.tension * drum.batterDensity);
    const float impedanceTime = mass / std::max (membraneImpedance, 1.0f);

    contactSeconds = std::sqrt (hertzTime * hertzTime + impedanceTime * impedanceTime);
    contactSeconds = clampFloat (contactSeconds, 4.0e-5f, 0.05f);

    // Impulse of the collision, spread over the sin^1.5 Hertz force pulse.
    // The integral of sin(x)^1.5 over one arch is 2.3963.
    const float impulse = mass * speed * (1.0f + restitution);
    peakForce = impulse * piFloat / (2.3963f * contactSeconds);
}

float TaikoEngine::measureContactSeconds (Articulation articulation, int octaveOffset,
                                          float velocity) const noexcept
{
    return measureContact (applied_, articulation, octaveOffset, velocity);
}

float TaikoEngine::measureContact (const EngineParameters& raw,
                                   Articulation articulation, int octaveOffset,
                                   float velocity) noexcept
{
    const auto parameters = sanitise (raw);
    const auto drum = resolveDrumFor (parameters, 0.0f, octaveOffset);
    const auto& profile = strikeProfile (articulation);
    const float shaped = clampFloat (velocity, 0.0f, 1.0f);
    const float normalised = lerp (0.72f, shaped, parameters.velocityDepth);
    const float speed = geometricLerp (minimumImpactSpeed, maximumImpactSpeed,
                                       normalised * normalised);
    float contactSeconds = 0.0f;
    float peakForce = 0.0f;
    solveContact (drum, profile, parameters.bachiHardness, speed, contactSeconds,
                  peakForce);
    return contactSeconds;
}

// ---------------------------------------------------------------------------
// Building the modal bank for one stroke
// ---------------------------------------------------------------------------

void TaikoEngine::configureResonator (Resonator& resonator, float frequencyHz,
                                      float decayRate, float gain) const noexcept
{
    const auto rate = static_cast<float> (sampleRate_);
    const float nyquist = 0.5f * rate;

    if (! (frequencyHz > 0.0f) || frequencyHz >= nyquist * 0.98f)
    {
        resonator.a1 = 0.0f;
        resonator.a2 = 0.0f;
        resonator.b0 = 0.0f;
        return;
    }

    const float omega = 2.0f * piFloat * frequencyHz / rate;
    const float radius = std::exp (-decayRate / rate);
    resonator.a1 = -2.0f * radius * std::cos (omega);
    resonator.a2 = radius * radius;
    // Impulse-invariant scaling: with this numerator the resonator's impulse
    // response is exactly the sampled r^n sin(omega n), so the drive gain the
    // caller computes carries its physical meaning through unchanged.
    resonator.b0 = gain * std::sin (omega);
}

void TaikoEngine::buildVoiceModes (Voice& voice, const DrumState& drum,
                                   const StrikeProfile& profile,
                                   float extraDamping) noexcept
{
    const auto& entries = membraneModes();
    const auto rate = static_cast<float> (sampleRate_);
    const float nyquist = 0.5f * rate;

    const float radius = drum.radius;
    const float rho = clampFloat (voice.strikeRadius, 0.0f, 0.995f);
    const float theta = voice.strikeAngle;
    const float sigmaB = drum.batterDensity;
    const float sigmaR = drum.resonantDensity;
    const float area = piFloat * radius * radius;

    const float micRho = drum.micRadius / radius;
    const float micAngleL = drum.micAngleLeft;
    const float micAngleR = drum.micAngleRight;
    const float micDistance = drum.micDistanceMetres;
    // Everything that propagates out of the drum arrives at both microphones
    // alike, and spreads on the way; only the near field carries the shape of
    // the head, and that is what separates the pair.
    const float propagatingSpread = 1.0f / (1.0f + micDistance / 0.12f);

    const float lossFactor = drum.headLossFactor * (1.0f + 2.4f * extraDamping);
    const float edgeLoss = drum.edgeLoss * (1.0f + 3.0f * extraDamping);

    int count = 0;
    float peakMagnitude = 1.0e-12f;

    // A real head is never quite uniform, so each degenerate pair sits a
    // fraction of a percent apart and beats. That asymmetry is a property of
    // the hide rather than of the stroke, so it is seeded from a fixed
    // constant: the same drum must split the same way every time it is hit.
    constexpr std::uint32_t headSeed = 0x9e3779b9u;
    constexpr float splitDepth = 0.0016f;

    for (int entryIndex = 0; entryIndex < modeEntryCount; ++entryIndex)
    {
        const auto& entry = entries[static_cast<std::size_t> (entryIndex)];
        const int order = entry.circumferentialOrder;
        const auto lambda = static_cast<float> (entry.besselZero);

        // Ideal membrane modes: f = c * lambda / (2 pi a). The ratios are set
        // by the Bessel zeros alone, so tension and size move the whole set
        // together and only the air changes its shape.
        const float idealBatter = drum.waveSpeed * lambda / (2.0f * piFloat * radius);
        const float idealResonant =
            drum.resonantWaveSpeed * lambda / (2.0f * piFloat * radius);

        // Air hanging off the head as added mass. Modes with a high radial or
        // circumferential order shift less air per unit of displacement, so
        // they are loaded far less than the fundamental. A light synthetic
        // head is loaded much more than a heavy hide, which is exactly why it
        // sounds lower than its tension alone predicts.
        const float loadShape = (2.4048f / lambda) / (1.0f + 0.6f * static_cast<float> (order));
        const float loadBatter =
            1.0f / std::sqrt (1.0f + 0.85f * loadShape * airDensity * radius / sigmaB);
        const float loadResonant =
            1.0f / std::sqrt (1.0f + 0.85f * loadShape * airDensity * radius / sigmaR);

        const float omegaBatter = 2.0f * piFloat * idealBatter * loadBatter;
        const float omegaResonant = 2.0f * piFloat * idealResonant * loadResonant;

        const auto besselAtZero = static_cast<float> (besselJ (order + 1, entry.besselZero));
        const float besselSquared = std::max (besselAtZero * besselAtZero, 1.0e-9f);

        // Mode shape at the strike point and at each microphone.
        const auto shapeStrike =
            static_cast<float> (besselJ (order, entry.besselZero * rho));
        const auto shapeMic =
            static_cast<float> (besselJ (order, entry.besselZero * micRho));

        if (order == 0)
        {
            // Axisymmetric modes are the only ones that change the body's
            // volume, so they are the only ones the enclosed air can couple.
            // Projecting the uniform cavity pressure onto J0 leaves a clean
            // 4/lambda^2 weighting, which is why the fundamental is lifted so
            // much further than the modes above it.
            const float geometricMass = area * besselSquared;   // per unit density
            const float cavity = drum.cavityStiffness * 4.0f / (lambda * lambda);

            // Symmetrised by w = sqrt(sigma) q, so the two-by-two is symmetric
            // and its eigenvectors are orthonormal.
            const float diagonalB = omegaBatter * omegaBatter + cavity / sigmaB;
            const float diagonalR = omegaResonant * omegaResonant + cavity / sigmaR;
            const float offDiagonal = cavity / std::sqrt (sigmaB * sigmaR);

            for (int branch = 0; branch < 2; ++branch)
            {
                if (count >= membraneResonatorCount)
                    break;

                float eigenvalue = 0.0f;
                float vectorB = 0.0f;
                float vectorR = 0.0f;
                solveAxisymmetricBranch (diagonalB, diagonalR, offDiagonal, branch,
                                         eigenvalue, vectorB, vectorR);
                if (! (eigenvalue > 0.0f))
                    continue;

                const float omega = std::sqrt (eigenvalue);
                const float frequency = omega / (2.0f * piFloat);
                if (frequency >= nyquist * 0.98f)
                    continue;

                // The batter head is the one being struck and the one the
                // microphones are in front of, so the batter component of the
                // eigenvector appears on both the drive and the observation.
                const float batterShare = vectorB / std::sqrt (sigmaB);
                // Net volume flow: what the pair hears once it has backed far
                // enough off the head to stop reading the membrane's shape.
                const float volumeShare = vectorB / std::sqrt (sigmaB)
                                        + vectorR / std::sqrt (sigmaR);

                const float ka = omega * radius / soundSpeed;
                const float efficiency = radiationEfficiency (0, ka);
                const float radiationLoss =
                    drum.radiationScale * airDensity * soundSpeed * efficiency
                    * std::abs (volumeShare) / (2.0f * sigmaB);
                const float decay = 0.5f * lossFactor * omega + radiationLoss + edgeLoss;

                const float drive = shapeStrike * batterShare
                                  / (geometricMass * omega * rate);

                // What a microphone in front of the head actually receives.
                // A mode whose pattern on the head is finer than the sound it
                // makes cannot radiate: its field is evanescent and dies as
                // exp(-sqrt(ks^2 - k^2) d) above the surface. That single
                // exponential is the whole close-microphone story - right on
                // the head the pair reads the membrane's shape and separates,
                // and a hand's width back it reads only what the drum radiates
                // and collapses towards mono.
                const float spatialWavenumber = lambda / radius;
                const float airWavenumber = omega / soundSpeed;
                const float evanescentRate = std::sqrt (std::max (
                    spatialWavenumber * spatialWavenumber
                        - airWavenumber * airWavenumber,
                    0.0f));
                const float nearField = std::exp (-evanescentRate * micDistance);

                const float observed =
                    nearField * shapeMic * batterShare
                    + efficiency * (2.0f * besselAtZero / lambda) * volumeShare
                          * propagatingSpread;
                // Proximity lift: a close microphone reads pressure that has
                // not yet spread, and low modes gain most from it.
                const float proximity =
                    1.0f + drum.micProximity / (1.0f + (frequency / 190.0f)
                                                        * (frequency / 190.0f));

                auto& mode = voice.modes[static_cast<std::size_t> (count)];
                mode.omega = omega;
                mode.decayRate = decay;
                mode.membrane = true;
                mode.drive = drive * profile.membraneGain * modelScale;
                // Axisymmetric modes look identical from both sides of the
                // head, so they are the part of the image that stays centred.
                mode.micLeft = observed * proximity;
                mode.micRight = observed * proximity;
                configureResonator (mode.resonator, frequency, decay, 1.0f);
                ++count;

                peakMagnitude = std::max (peakMagnitude,
                                          std::abs (mode.drive * mode.micLeft));
            }
        }
        else
        {
            // Every other mode comes as a degenerate pair whose two members
            // are the same shape rotated by a quarter of their own period. A
            // real head is never quite uniform, so the pair sits a fraction of
            // a percent apart and beats - which is most of what makes a drum
            // sound like an object rather than an oscillator.
            const float geometricMass = 0.5f * area * besselSquared * sigmaB;
            const auto orderFloat = static_cast<float> (order);

            for (int branch = 0; branch < 2; ++branch)
            {
                if (count >= membraneResonatorCount)
                    break;

                const float detune =
                    1.0f + splitDepth * (branch == 0 ? 1.0f : -1.0f)
                         * (1.0f + 0.5f * signedUnitFromHash (
                                headSeed + static_cast<std::uint32_t> (entryIndex)));
                const float omega = omegaBatter * detune;
                const float frequency = omega / (2.0f * piFloat);
                if (frequency >= nyquist * 0.98f)
                    continue;

                // Branch 0 is the cos(m theta) member, branch 1 the sin one.
                const float strikeAngular =
                    branch == 0 ? std::cos (orderFloat * theta)
                                : std::sin (orderFloat * theta);
                const float micAngularL =
                    branch == 0 ? std::cos (orderFloat * micAngleL)
                                : std::sin (orderFloat * micAngleL);
                const float micAngularR =
                    branch == 0 ? std::cos (orderFloat * micAngleR)
                                : std::sin (orderFloat * micAngleR);

                const float ka = omega * radius / soundSpeed;
                const float efficiency = radiationEfficiency (order, ka);
                const float radiationLoss =
                    drum.radiationScale * airDensity * soundSpeed * efficiency
                    / (2.0f * sigmaB);
                const float decay = 0.5f * lossFactor * omega
                                  + radiationLoss
                                  + edgeLoss * (1.0f + 0.12f * orderFloat);

                const float drive = shapeStrike * strikeAngular
                                  / (geometricMass * omega * rate);

                // These modes move no net air at all, so almost everything the
                // pair hears from them is near field, and it dies faster with
                // distance than the fundamental's does because their pattern
                // on the head is finer. That is why backing the microphones
                // off narrows the image and softens the slap at the same time:
                // it is one mechanism, not two.
                const float spatialWavenumber = lambda / radius;
                const float airWavenumber = omega / soundSpeed;
                const float evanescentRate = std::sqrt (std::max (
                    spatialWavenumber * spatialWavenumber
                        - airWavenumber * airWavenumber,
                    0.0f));
                const float nearField = std::exp (-evanescentRate * micDistance);

                const float proximity =
                    1.0f + drum.micProximity / (1.0f + (frequency / 190.0f)
                                                        * (frequency / 190.0f));
                // The small part that does escape arrives at both microphones
                // alike, which is what keeps the pair from going anti-phase
                // once the near field has decayed away.
                const float propagating =
                    0.35f * efficiency * propagatingSpread;
                const float observedL =
                    nearField * shapeMic * micAngularL * proximity + propagating;
                const float observedR =
                    nearField * shapeMic * micAngularR * proximity + propagating;

                auto& mode = voice.modes[static_cast<std::size_t> (count)];
                mode.omega = omega;
                mode.decayRate = decay;
                mode.membrane = true;
                mode.drive = drive * profile.membraneGain * modelScale;
                mode.micLeft = observedL;
                mode.micRight = observedR;
                configureResonator (mode.resonator, frequency, decay, 1.0f);
                ++count;

                peakMagnitude = std::max (
                    peakMagnitude,
                    std::max (std::abs (mode.drive * mode.micLeft),
                              std::abs (mode.drive * mode.micRight)));
            }
        }
    }

    // The shell. It is struck directly by a Katsu or a rim shot and picked up
    // faintly the rest of the time, because a head cannot move without the
    // body it is stretched over moving too. The Shell Resonance control scales
    // how much of the body colours an ordinary head stroke, but it never
    // silences the body completely: hitting wood makes a sound whatever the
    // player has decided about sympathetic ring.
    const float bodyLevel = profile.usesDrumBody
        ? (0.30f + 0.70f * drum.shellLevel)
        : 1.0f;
    const float shellGain = profile.shellGain * bodyLevel
                          + profile.rimGain * 0.35f;

    for (int index = 0; index < shellResonatorCount; ++index)
    {
        if (count >= resonatorCount)
            break;

        const float frequency =
            drum.shellFrequencies[static_cast<std::size_t> (index)]
            * profile.shellFrequencyScale;
        if (! (frequency > 0.0f) || frequency >= nyquist * 0.98f)
            continue;

        const float decay =
            drum.shellDecays[static_cast<std::size_t> (index)]
                / std::max (profile.shellDecayScale, 0.05f)
            * (1.0f + 1.5f * extraDamping);

        // The shell is a ring, so the pair reads it from two places on it and
        // the wooden component of the image opens with the microphones.
        const auto ring = static_cast<float> (index + 2);
        // The pair stands in front of the head, not around the body, so it
        // reads the shell mostly in common. A modest difference keeps the wood
        // from collapsing to a point without ever putting it out of phase.
        const float shellL = 0.80f + 0.20f * std::cos (ring * micAngleL);
        const float shellR = 0.80f + 0.20f * std::cos (ring * micAngleR);
        const float omega = 2.0f * piFloat * frequency;
        // Same force-over-modal-mass path the head uses, so a heavy body
        // genuinely refuses to move and a light one genuinely rings - times the
        // area it radiates from, which is the term the membrane modes get for
        // free through their own volume displacement and the shell was missing.
        // Without it a small body, whose modal mass falls as the cube of its
        // radius, ran away from the head it is stretched under.
        const float radiatingArea = drum.radius * drum.radius;
        const float level = shellGain * shellCalibration * radiatingArea
                          / (drum.shellModalMass * omega * rate)
                          / (1.0f + 0.35f * static_cast<float> (index));

        auto& mode = voice.modes[static_cast<std::size_t> (count)];
        mode.omega = omega;
        mode.decayRate = decay;
        mode.membrane = false;
        mode.drive = level * modelScale;
        mode.micLeft = shellL;
        mode.micRight = shellR;
        configureResonator (mode.resonator, frequency, decay, 1.0f);
        ++count;

        peakMagnitude = std::max (peakMagnitude,
                                  std::abs (mode.drive * mode.micLeft));
    }

    voice.modeCount = count;

    // How long each mode stays above the retirement floor, so the render loop
    // can drop them as they go. A drum starts with everything ringing and ends
    // on two or three modes; paying for forty the whole way is pure waste.
    for (int index = 0; index < count; ++index)
    {
        auto& mode = voice.modes[static_cast<std::size_t> (index)];
        const float magnitude =
            std::max (std::abs (mode.drive * mode.micLeft),
                      std::abs (mode.drive * mode.micRight));
        const float relative = magnitude / peakMagnitude;

        if (mode.decayRate <= 0.0f || relative <= modeRetirementFloor)
        {
            mode.audibleSamples = 0;
            continue;
        }

        const float seconds =
            std::log (relative / modeRetirementFloor) / mode.decayRate;
        const float bounded = clampFloat (seconds, 0.0f,
                                          static_cast<float> (maximumTailSeconds));
        mode.audibleSamples = static_cast<std::uint64_t> (bounded * rate);
    }

    // Descending, so the active count only ever shrinks. Insertion sort: the
    // array is forty-six entries and this runs once per stroke.
    for (int i = 1; i < count; ++i)
    {
        const Mode key = voice.modes[static_cast<std::size_t> (i)];
        int j = i - 1;
        while (j >= 0
               && voice.modes[static_cast<std::size_t> (j)].audibleSamples
                      < key.audibleSamples)
        {
            voice.modes[static_cast<std::size_t> (j + 1)] =
                voice.modes[static_cast<std::size_t> (j)];
            --j;
        }
        voice.modes[static_cast<std::size_t> (j + 1)] = key;
    }

    voice.activeModeCount = count;
    while (voice.activeModeCount > 0
           && voice.modes[static_cast<std::size_t> (voice.activeModeCount - 1)]
                      .audibleSamples
                  == 0)
        --voice.activeModeCount;

    voice.maximumSamples = voice.activeModeCount > 0
        ? voice.modes[0].audibleSamples + static_cast<std::uint64_t> (rate * 0.02f)
        : static_cast<std::uint64_t> (rate * 0.05f);
    voice.maximumSamples = std::min (
        voice.maximumSamples,
        static_cast<std::uint64_t> (maximumTailSeconds * sampleRate_));
}

void TaikoEngine::scheduleContacts (Voice& voice, const StrikeProfile& profile,
                                    float contactSeconds, float peakForce,
                                    float noiseLevel) noexcept
{
    const auto rate = static_cast<float> (sampleRate_);
    const auto lengthSamples =
        static_cast<std::uint32_t> (std::max (2.0f, contactSeconds * rate));

    voice.contactCount = 0;
    voice.nextContact = 0;
    voice.contactRemaining = 0u;

    const int requested = std::clamp (profile.contactCount, 1, maxContactEvents);
    const auto seed = static_cast<std::uint32_t> (voice.startOrder * 22695477u + 5u);

    if (requested == 1)
    {
        voice.contacts[0] = { 0u, lengthSamples, peakForce, peakForce * noiseLevel };
        voice.contactCount = 1;
        return;
    }

    if (profile.contactCount == 2)
    {
        // A flam: a lighter grace note about 32 ms ahead of the full stroke.
        const float graceOffset =
            0.032f * (1.0f + 0.25f * signedUnitFromHash (seed) * applied_.humanise);
        voice.contacts[0] = { 0u,
                              std::max<std::uint32_t> (2u,
                                  static_cast<std::uint32_t> (lengthSamples * 1.15f)),
                              peakForce * 0.42f, peakForce * 0.42f * noiseLevel };
        voice.contacts[1] = { static_cast<std::uint32_t> (graceOffset * rate),
                              lengthSamples, peakForce, peakForce * noiseLevel };
        voice.contactCount = 2;
        return;
    }

    // A press roll. The stick is pushed into the head and bounces, so each
    // contact is weaker and closer than the one before it.
    float offsetSeconds = 0.0f;
    float amplitude = peakForce * 0.85f;
    float spacing = 0.019f;

    for (int index = 0; index < requested; ++index)
    {
        const float jitter =
            1.0f + 0.30f * applied_.humanise
                 * signedUnitFromHash (seed + static_cast<std::uint32_t> (index));
        voice.contacts[static_cast<std::size_t> (index)] = {
            static_cast<std::uint32_t> (offsetSeconds * rate),
            std::max<std::uint32_t> (2u,
                static_cast<std::uint32_t> (lengthSamples * (1.0f + 0.12f * static_cast<float> (index)))),
            amplitude,
            amplitude * noiseLevel * 1.4f
        };
        offsetSeconds += spacing * jitter;
        spacing *= 0.82f;
        amplitude *= 0.72f;
    }

    voice.contactCount = requested;
}

// ---------------------------------------------------------------------------
// Striking the drum
// ---------------------------------------------------------------------------

int TaikoEngine::findVoiceSlot() noexcept
{
    for (int index = 0; index < maxVoices; ++index)
        if (! voices_[static_cast<std::size_t> (index)].active)
            return index;

    // Everything is sounding, so take the quietest. On a drum this is very
    // nearly inaudible: by the time sixteen strokes overlap, the oldest is
    // buried under the newest ones anyway.
    int quietest = 0;
    float lowest = voices_[0].peakLevel;
    for (int index = 1; index < maxVoices; ++index)
    {
        const float level = voices_[static_cast<std::size_t> (index)].peakLevel;
        if (level < lowest)
        {
            lowest = level;
            quietest = index;
        }
    }
    return quietest;
}

void TaikoEngine::trigger (Articulation articulation, int octaveOffset,
                           float velocity) noexcept
{
    if (! prepared_)
        return;
    if (static_cast<std::size_t> (articulation) >= articulationCount)
        return;
    if (! (velocity > 0.0f) || ! std::isfinite (velocity))
        return;

    refreshDrumIfNeeded();

    const int octave = std::clamp (octaveOffset, lowestOctaveOffset, highestOctaveOffset);
    const auto& drum = drumCache_[static_cast<std::size_t> (octave - lowestOctaveOffset)];
    const auto& profile = strikeProfile (articulation);

    const int slot = findVoiceSlot();
    auto& voice = voices_[static_cast<std::size_t> (slot)];
    silenceVoice (voice);

    const auto order = ++noteSequence_;
    const auto seed = static_cast<std::uint32_t> (order * 2654435761u + 11u);

    voice.startOrder = order;
    voice.articulation = articulation;
    voice.octaveOffset = octave;
    voice.velocity = clampFloat (velocity, 0.0f, 1.0f);

    // No two strokes drag across the hide the same way, so the contact noise
    // is reseeded per stroke - unless Humanise is off, which asks for a machine
    // and must therefore repeat exactly.
    const float humanise = applied_.humanise;
    voice.noiseState = hash32 (humanise > 0.0f
                                   ? seed
                                   : static_cast<std::uint32_t> (
                                         static_cast<std::uint32_t> (articulation) * 131u
                                         + static_cast<std::uint32_t> (octave + 8) * 17u))
                     | 1u;

    // Where the stick lands. The articulation sets the radius, the position
    // control offsets it, and humanising scatters both the radius and the
    // angle - which is what stops a roll from sounding like one sample.
    const float positionOffset = applied_.strikePosition * 0.32f;
    const float radiusJitter = 0.055f * humanise * signedUnitFromHash (seed + 1u);
    voice.strikeRadius = clampFloat (profile.radius + positionOffset + radiusJitter,
                                     0.0f, 0.985f);
    // A player works around the head rather than hitting one spot, and with
    // the microphones apart that is what walks successive strokes across the
    // image. At zero humanising the hand stops wandering and every stroke of an
    // articulation lands in exactly the same place.
    voice.strikeAngle = signedUnitFromHash (seed + 2u) * piFloat * humanise;

    // MIDI velocity to impact speed. Squaring the normalised value before the
    // geometric map gives the low end of the range the resolution a player
    // needs for ghost strokes.
    const float shaped = lerp (0.72f, voice.velocity, applied_.velocityDepth);
    const float speedJitter =
        1.0f + 0.10f * humanise * signedUnitFromHash (seed + 3u);
    const float impactSpeed = clampFloat (
        geometricLerp (minimumImpactSpeed, maximumImpactSpeed, shaped * shaped)
            * speedJitter,
        0.05f, 12.0f);

    float contactSeconds = 0.0f;
    float peakForce = 0.0f;
    solveContact (drum, profile, applied_.bachiHardness, impactSpeed, contactSeconds,
                  peakForce);
    contactSeconds *= 1.0f + 0.08f * humanise * signedUnitFromHash (seed + 4u);

    // The free hand resting on the head, for the muted strokes.
    const float extraDamping = profile.muteAmount;
    buildVoiceModes (voice, drum, profile, extraDamping);

    const float noiseLevel = applied_.strikeNoise * profile.noiseGain * 0.35f;
    scheduleContacts (voice, profile, contactSeconds, peakForce * profile.levelScale,
                      noiseLevel);

    // Contact noise is stick and hide texture, so it sits an octave or so
    // above the drum and follows how sharp the contact was.
    const float noiseCorner = clampFloat (0.55f / std::max (contactSeconds, 1.0e-4f),
                                          200.0f, 0.45f * static_cast<float> (sampleRate_));
    voice.noiseBandCoefficient =
        1.0f - std::exp (-2.0f * piFloat * noiseCorner * inverseSampleRate_);
    voice.noiseBandState = 0.0f;

    // Stretching the head raises its tension, so a hard stroke starts sharp
    // and settles as it decays. The depth follows the impact speed because
    // that is what sets how far the head is pushed.
    const float glideDepth = applied_.tensionModulation * 0.055f
                           * clampFloat (impactSpeed / maximumImpactSpeed, 0.0f, 1.0f)
                           * profile.membraneGain;
    voice.tensionDepth = glideDepth;
    voice.tensionEnvelope = glideDepth > 0.0f ? 1.0f : 0.0f;
    voice.tensionDecay = std::exp (-1.0f / (0.055f * static_cast<float> (sampleRate_))
                                   * static_cast<float> (controlPeriod));
    voice.appliedTensionShift = 1.0f;
    if (glideDepth > 0.0f)
        applyTensionShift (voice, 1.0f + glideDepth);

    // The airborne path from the stick to each microphone. Plain geometry: the
    // strike lands somewhere on the head, each mic sits somewhere in front of
    // it, and the impact reaches them at the speed of sound with the usual
    // inverse-distance spreading. The difference between the two distances is
    // both the level and the time cue that places the stroke across the image.
    {
        const float strikeX = drum.radius * voice.strikeRadius * std::cos (voice.strikeAngle);
        const float strikeY = drum.radius * voice.strikeRadius * std::sin (voice.strikeAngle);
        const float height = drum.micDistanceMetres;

        const auto pathTo = [&] (float angle) noexcept
        {
            const float micX = drum.micRadius * std::cos (angle);
            const float micY = drum.micRadius * std::sin (angle);
            const float dx = micX - strikeX;
            const float dy = micY - strikeY;
            return std::sqrt (dx * dx + dy * dy + height * height);
        };

        const float pathLeft = std::max (pathTo (drum.micAngleLeft), 0.02f);
        const float pathRight = std::max (pathTo (drum.micAngleRight), 0.02f);
        const float nearest = std::min (pathLeft, pathRight);

        voice.directGainLeft = directCalibration / pathLeft;
        voice.directGainRight = directCalibration / pathRight;
        voice.directDelayLeft =
            clampFloat ((pathLeft - nearest) / soundSpeed * static_cast<float> (sampleRate_),
                        0.0f, static_cast<float> (directLineSize - 4));
        voice.directDelayRight =
            clampFloat ((pathRight - nearest) / soundSpeed * static_cast<float> (sampleRate_),
                        0.0f, static_cast<float> (directLineSize - 4));
        voice.directLine.fill (0.0f);
        voice.directWriteIndex = 0;
        voice.directPrevious = 0.0f;
        voice.directLowpassState = 0.0f;
        // A contact patch a few millimetres across stops radiating somewhere
        // around 3.5 kHz, and a softer bachi spreads over more of the head.
        const float patchCorner = 3500.0f * (0.55f + 0.75f * applied_.bachiHardness);
        voice.directLowpassCoefficient = 1.0f - std::exp (
            -2.0f * piFloat * std::min (patchCorner, 0.45f * static_cast<float> (sampleRate_))
            * inverseSampleRate_);
    }

    // A flam's main stroke lands 32 ms after its grace note, and a press roll
    // goes on bouncing for a tenth of a second. Those later contacts drive the
    // same modal bank, but a mode's lifetime was measured from the voice's
    // start, so the bank was being retired out from under them - a press roll
    // at high damping reached its last bounce with nine of its thirty modes
    // left. Shift every lifetime by however long the schedule runs.
    if (voice.contactCount > 0)
    {
        const auto& lastContact =
            voice.contacts[static_cast<std::size_t> (voice.contactCount - 1)];
        const auto scheduleEnd = static_cast<std::uint64_t> (lastContact.startSample)
                               + lastContact.lengthSamples;

        // Adding a constant preserves the descending sort, and leaves the modes
        // that were never audible at zero so the trailing trim still finds them.
        for (int index = 0; index < voice.modeCount; ++index)
        {
            auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (mode.audibleSamples > 0)
                mode.audibleSamples += scheduleEnd;
        }

        voice.maximumSamples = std::min (
            voice.maximumSamples + scheduleEnd,
            static_cast<std::uint64_t> (maximumTailSeconds * sampleRate_));
    }

    voice.handGain = 1.0f;
    voice.tuningAtStrike = applied_.pitch + 2.0f * pitchBend_;
    voice.ageSamples = 0;
    voice.peakLevel = 0.0f;
    voice.controlCountdown = 0;
    idleFrozen_ = false;
    silentSamples_ = 0;
    voice.active = voice.activeModeCount > 0 || voice.contactCount > 0;

    updateActiveVoiceCount();

    // Publish the stroke for the editor's head display.
    visualStrikeRadius_.store (voice.strikeRadius, std::memory_order_relaxed);
    visualStrikeAngle_.store (voice.strikeAngle, std::memory_order_relaxed);
    visualArticulation_.store (static_cast<int> (articulation), std::memory_order_relaxed);
    visualOctave_.store (octave, std::memory_order_relaxed);
    visualLevel_ = std::max (visualLevel_, voice.velocity);

    const auto measurements = measureDrum (octave);
    fundamentalHz_.store (measurements.loadedFundamentalHz, std::memory_order_relaxed);
}

bool TaikoEngine::triggerMidi (int midiNote, float velocity) noexcept
{
    const auto articulation = articulationForMidiNote (midiNote);
    const auto octave = octaveOffsetForMidiNote (midiNote);
    if (! articulation.has_value() || ! octave.has_value())
        return false;

    trigger (*articulation, *octave, velocity);
    return true;
}

void TaikoEngine::applyTensionShift (Voice& voice, float shift) noexcept
{
    const auto rate = static_cast<float> (sampleRate_);
    const float nyquist = 0.5f * rate;

    for (int index = 0; index < voice.activeModeCount; ++index)
    {
        auto& mode = voice.modes[static_cast<std::size_t> (index)];
        // Stretching the head does not stretch the body it is nailed to, and
        // the bank is sorted by lifetime so the shell modes are scattered
        // through it rather than sitting at the end.
        if (! mode.membrane)
            continue;

        const float frequency = mode.omega * shift / (2.0f * piFloat);
        if (frequency >= nyquist * 0.98f || ! (frequency > 0.0f))
        {
            mode.resonator.a1 = 0.0f;
            mode.resonator.a2 = 0.0f;
            mode.resonator.b0 = 0.0f;
            continue;
        }

        const float omega = 2.0f * piFloat * frequency / rate;
        const float poleRadius = std::exp (-mode.decayRate / rate);
        mode.resonator.a1 = -2.0f * poleRadius * std::cos (omega);
        mode.resonator.a2 = poleRadius * poleRadius;
        mode.resonator.b0 = std::sin (omega);
    }

    voice.appliedTensionShift = shift;
}

void TaikoEngine::updateVoiceControl (Voice& voice) noexcept
{
    // Arm the forced fade once the hard cap is close enough that the voice
    // would otherwise be cut while still sounding.
    if (voice.retireStep <= 0.0f)
    {
        const auto fadeSamples =
            static_cast<std::uint64_t> (forcedFadeSeconds * sampleRate_);
        if (voice.maximumSamples > fadeSamples
            && voice.ageSamples + fadeSamples >= voice.maximumSamples)
            voice.retireStep = 1.0f / static_cast<float> (fadeSamples);
    }

    // Retire the modes that have fallen below the floor. They are stored in
    // descending order of lifetime, so this is a walk from the end.
    while (voice.activeModeCount > 0
           && voice.modes[static_cast<std::size_t> (voice.activeModeCount - 1)]
                      .audibleSamples
                  <= voice.ageSamples)
    {
        --voice.activeModeCount;
        voice.modes[static_cast<std::size_t> (voice.activeModeCount)].resonator.clear();
    }

    // Fold the accumulated hand damping into the resonator states. Applying it
    // to a freely decaying resonator's output and applying it to its state are
    // the same operation, so this is exact rather than an approximation - and
    // it keeps the envelope from drifting towards zero without bound.
    if (voice.handGain < 0.9999f)
    {
        for (int index = 0; index < voice.activeModeCount; ++index)
        {
            auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane)
                continue;
            mode.resonator.y1 *= voice.handGain;
            mode.resonator.y2 *= voice.handGain;
        }
        voice.handGain = 1.0f;
    }

    // The attack glide and the wheel are one thing: both press the head, both
    // raise its tension, and both therefore scale the membrane's frequencies.
    // Applying them together also means a stroke that is already ringing bends
    // with the wheel, rather than the wheel only reaching the next stroke.
    if (voice.tensionEnvelope > 1.0e-4f)
        voice.tensionEnvelope *= voice.tensionDecay;
    else
        voice.tensionEnvelope = 0.0f;

    // The voice's modes were built with whatever tuning stood at the strike, so
    // only the movement since then is left to apply. Both the Pitch control and
    // the wheel are head tension - a host automating Pitch through a ringing
    // tail retunes it for the same reason the wheel does - so they are carried
    // together as one offset in semitones.
    const float tuningNow = applied_.pitch + 2.0f * pitchBend_;
    const float shift = (1.0f + voice.tensionDepth * voice.tensionEnvelope)
                      * std::exp2 ((tuningNow - voice.tuningAtStrike) / 12.0f);

    if (std::abs (shift - voice.appliedTensionShift) > 1.0e-5f)
        applyTensionShift (voice, shift);
}

float TaikoEngine::renderVoice (Voice& voice, float& rightOut) noexcept
{
    // Start the next scheduled contact if it is due.
    if (voice.contactRemaining == 0u && voice.nextContact < voice.contactCount)
    {
        const auto& contact =
            voice.contacts[static_cast<std::size_t> (voice.nextContact)];
        if (voice.ageSamples >= contact.startSample)
        {
            voice.contactRemaining = contact.lengthSamples;
            voice.contactLength = contact.lengthSamples;
            voice.contactAmplitude = contact.amplitude;
            voice.contactNoiseAmplitude = contact.noiseAmplitude;
            ++voice.nextContact;
        }
    }

    float force = 0.0f;
    float contactEnvelope = 0.0f;

    if (voice.contactRemaining > 0u)
    {
        const float position =
            1.0f - static_cast<float> (voice.contactRemaining)
                       / static_cast<float> (voice.contactLength);
        // Hertz contact force: a sin^1.5 arch rather than a symmetric bump,
        // because a rounded tip loads the surface faster than it leaves it.
        const float arch = std::sin (piFloat * position);
        contactEnvelope = arch > 0.0f ? arch * std::sqrt (arch) : 0.0f;
        force = voice.contactAmplitude * contactEnvelope;
        --voice.contactRemaining;
    }

    // Broadband contact noise. Most of it goes into the head, because that is
    // what it is: the stick dragging across the hide as it lands.
    float noise = 0.0f;
    if (contactEnvelope > 0.0f && voice.contactNoiseAmplitude > 0.0f)
    {
        const float white = nextNoise (voice.noiseState);
        voice.noiseBandState += voice.noiseBandCoefficient * (white - voice.noiseBandState);
        noise = (white - voice.noiseBandState) * voice.contactNoiseAmplitude
              * contactEnvelope;
    }

    const float excitation = force + noise;

    float left = 0.0f;
    float right = 0.0f;

    // The head and the body are summed apart so a hand laid on the head can
    // damp what it is actually touching.
    float membraneLeft = 0.0f;
    float membraneRight = 0.0f;

    for (int index = 0; index < voice.activeModeCount; ++index)
    {
        auto& mode = voice.modes[static_cast<std::size_t> (index)];
        const float value = mode.resonator.tick (excitation * mode.drive);
        if (mode.membrane)
        {
            membraneLeft += value * mode.micLeft;
            membraneRight += value * mode.micRight;
        }
        else
        {
            left += value * mode.micLeft;
            right += value * mode.micRight;
        }
    }

    // The impact heard straight through the air. Pressure follows the rate of
    // change of the contact force rather than the force itself, so a shorter,
    // harder contact makes a proportionally sharper and louder click - the
    // same v^(-1/5) law that brightens the head, arriving by a second route.
    {
        const float slope = (excitation - voice.directPrevious)
                          * static_cast<float> (sampleRate_) * 1.0e-5f;
        voice.directPrevious = excitation;
        voice.directLowpassState +=
            voice.directLowpassCoefficient * (slope - voice.directLowpassState);
        const float radiated = voice.directLowpassState;

        voice.directLine[static_cast<std::size_t> (voice.directWriteIndex)] = radiated;

        left += readDelayLine (voice.directLine, voice.directWriteIndex,
                               voice.directDelayLeft) * voice.directGainLeft;
        right += readDelayLine (voice.directLine, voice.directWriteIndex,
                                voice.directDelayRight) * voice.directGainRight;

        voice.directWriteIndex = (voice.directWriteIndex + 1) & (directLineSize - 1);
    }

    // Only the membrane is damped. The wooden shell is not what the hand is
    // resting on, the stick-on-stick stroke has nothing to do with the drum at
    // all, and the airborne click of the impact itself reaches the microphones
    // through the air rather than through the head.
    left += membraneLeft * voice.handGain;
    right += membraneRight * voice.handGain;

    // The forced fade at the tail cap. Zero step until the cap is in sight, so
    // an ordinary stroke - which retires because its modes ran out, not because
    // it hit the cap - never touches this.
    if (voice.retireStep > 0.0f)
    {
        voice.retireGain -= voice.retireStep;
        if (voice.retireGain < 0.0f)
            voice.retireGain = 0.0f;
        left *= voice.retireGain;
        right *= voice.retireGain;
    }

    ++voice.ageSamples;

    const float magnitude = std::max (std::abs (left), std::abs (right));
    if (magnitude > voice.peakLevel)
        voice.peakLevel = magnitude;
    else
        voice.peakLevel *= 0.99995f;

    rightOut = right;
    return left;
}

// ---------------------------------------------------------------------------
// Block processing
// ---------------------------------------------------------------------------

void TaikoEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    if (! prepared_)
    {
        std::fill (left, left + numSamples, 0.0f);
        std::fill (right, right + numSamples, 0.0f);
        return;
    }

    refreshDrumIfNeeded();

    const float targetGain = applied_.outputGain;
    const float targetDrive = applied_.drive;
    const float targetWidth = applied_.stereoWidth;

    // A hand laid on the head, as a per-sample multiplier the voices
    // accumulate and the control tick folds into their states.
    const auto rate = static_cast<float> (sampleRate_);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        handDamping_ += handDampingCoefficient_ * (handDampingTarget_ - handDamping_);
        pitchBend_ += pitchBendCoefficient_ * (pitchBendTarget_ - pitchBend_);
        // A tenth of a cent, measured against where the cache actually stands.
        if (std::abs (pitchBend_ - drumCacheBend_) > 0.0005f)
            drumCacheValid_ = false;

        // Up to about 26 dB per second of extra loss at full pressure, which
        // is roughly what a palm flat on a taiko head does.
        const float handRate = 12.0f * handDamping_ * handDamping_;
        const float handStep = handRate > 0.0f ? std::exp (-handRate / rate) : 1.0f;

        smoothedOutputGain_ += gainSmoothing_ * (targetGain - smoothedOutputGain_);
        smoothedDrive_ += gainSmoothing_ * (targetDrive - smoothedDrive_);
        // Width multiplies the side signal, so a step in it steps the audio.
        smoothedWidth_ += gainSmoothing_ * (targetWidth - smoothedWidth_);

        float mixLeft = 0.0f;
        float mixRight = 0.0f;
        bool anyVoiceActive = false;

        for (auto& voice : voices_)
        {
            if (! voice.active)
                continue;
            anyVoiceActive = true;

            if (voice.controlCountdown <= 0)
            {
                updateVoiceControl (voice);
                voice.controlCountdown = controlPeriod;

                if (voice.activeModeCount == 0
                    && voice.contactRemaining == 0u
                    && voice.nextContact >= voice.contactCount)
                {
                    silenceVoice (voice);
                    continue;
                }
                if (voice.ageSamples >= voice.maximumSamples
                    && voice.contactRemaining == 0u
                    && voice.nextContact >= voice.contactCount)
                {
                    silenceVoice (voice);
                    continue;
                }
            }
            --voice.controlCountdown;

            if (handStep < 1.0f)
                voice.handGain *= handStep;

            float voiceRight = 0.0f;
            const float voiceLeft = renderVoice (voice, voiceRight);
            mixLeft += voiceLeft;
            mixRight += voiceRight;
        }

        if (idleFrozen_ && ! anyVoiceActive)
        {
            left[sample] = 0.0f;
            right[sample] = 0.0f;
            meterLeft_ *= meterReleaseMultiplier_;
            meterRight_ *= meterReleaseMultiplier_;
            visualLevel_ *= visualDecayMultiplier_;
            continue;
        }

        // Width trim on the finished pair. At 0 the two close microphones are
        // summed to mono, at 0.5 they are left exactly as the head presented
        // them - already a real stereo image rather than a widened one - and
        // above that the difference is exaggerated.
        const float mid = 0.5f * (mixLeft + mixRight);
        const float side = 0.5f * (mixLeft - mixRight) * (2.0f * smoothedWidth_);
        float outLeft = mid + side;
        float outRight = mid - side;

        // Remove the offset a one-sided strike leaves behind before anything
        // nonlinear can turn it into a thump.
        const float dcInLeft = outLeft;
        const float dcInRight = outRight;
        dcOutputLeft_ = dcInLeft - dcInputLeft_ + dcCoefficient_ * dcOutputLeft_;
        dcOutputRight_ = dcInRight - dcInputRight_ + dcCoefficient_ * dcOutputRight_;
        dcInputLeft_ = dcInLeft;
        dcInputRight_ = dcInRight;
        outLeft = dcOutputLeft_;
        outRight = dcOutputRight_;

        if (smoothedDrive_ > 1.0e-4f)
        {
            const float amount = 1.0f + 7.0f * smoothedDrive_;
            const float makeup = 1.0f / std::sqrt (amount);

            // Antiderivative antialiasing, so pushing the drive does not fold
            // the drum's own high partials back down into its fundamental.
            const auto shape = [] (float x, float& previous) noexcept
            {
                const float current = x;
                const float delta = current - previous;
                float result;
                if (std::abs (delta) > 1.0e-5f)
                {
                    const auto antiderivative = [] (float value) noexcept
                    {
                        const float absolute = std::abs (value);
                        return absolute + std::log1p (std::exp (-2.0f * absolute))
                             - 0.6931472f;
                    };
                    result = (antiderivative (current) - antiderivative (previous)) / delta;
                }
                else
                {
                    result = std::tanh (0.5f * (current + previous));
                }
                previous = current;
                return result;
            };

            // Blended against the dry signal rather than switched into, so the
            // control converges on bypass as it approaches zero. Driving into
            // tanh alone jumps to a fully shaped signal at the first nonzero
            // step, because the shaper's gain approaches one rather than the
            // identity.
            const float wetLeft = shape (outLeft * amount, driveAdaaLeft_) * makeup;
            const float wetRight = shape (outRight * amount, driveAdaaRight_) * makeup;
            outLeft += smoothedDrive_ * (wetLeft - outLeft);
            outRight += smoothedDrive_ * (wetRight - outRight);
        }
        else
        {
            driveAdaaLeft_ = outLeft;
            driveAdaaRight_ = outRight;
        }

        outLeft *= smoothedOutputGain_;
        outRight *= smoothedOutputGain_;

        // Final safety. The model is bounded by its own damping, but a fully
        // wound drive on sixteen simultaneous odaiko strokes should still
        // never leave the buffer.
        outLeft = clampFloat (outLeft, -1.0f, 1.0f);
        outRight = clampFloat (outRight, -1.0f, 1.0f);

        left[sample] = outLeft;
        right[sample] = outRight;

        // Freeze once nothing is sounding and the shared path has rung out.
        if (! anyVoiceActive
            && std::abs (dcOutputLeft_) < idleFreezeLevel
            && std::abs (dcOutputRight_) < idleFreezeLevel
            && std::abs (outLeft) < idleFreezeLevel
            && std::abs (outRight) < idleFreezeLevel)
        {
            if (++silentSamples_ >= idleFreezeSamples)
            {
                dcInputLeft_ = dcInputRight_ = 0.0f;
                dcOutputLeft_ = dcOutputRight_ = 0.0f;
                driveAdaaLeft_ = driveAdaaRight_ = 0.0f;
                idleFrozen_ = true;
            }
        }
        else
        {
            silentSamples_ = 0;
        }

        const float absoluteLeft = std::abs (outLeft);
        const float absoluteRight = std::abs (outRight);
        meterLeft_ = absoluteLeft > meterLeft_
            ? absoluteLeft
            : meterLeft_ * meterReleaseMultiplier_;
        meterRight_ = absoluteRight > meterRight_
            ? absoluteRight
            : meterRight_ * meterReleaseMultiplier_;
        visualLevel_ *= visualDecayMultiplier_;
    }

    updateActiveVoiceCount();
    outputLevelLeft_.store (meterLeft_, std::memory_order_relaxed);
    outputLevelRight_.store (meterRight_, std::memory_order_relaxed);
    visualStrikeLevel_.store (visualLevel_, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Readouts
// ---------------------------------------------------------------------------

int TaikoEngine::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount_.load (std::memory_order_relaxed);
}

float TaikoEngine::getOutputLevel (int channel) const noexcept
{
    return channel <= 0 ? outputLevelLeft_.load (std::memory_order_relaxed)
                        : outputLevelRight_.load (std::memory_order_relaxed);
}

float TaikoEngine::getFundamentalHz() const noexcept
{
    return fundamentalHz_.load (std::memory_order_relaxed);
}

void TaikoEngine::getVisualState (DrumVisualState& destination) const noexcept
{
    destination.strikeRadius = visualStrikeRadius_.load (std::memory_order_relaxed);
    destination.strikeAngle = visualStrikeAngle_.load (std::memory_order_relaxed);
    destination.strikeLevel = visualStrikeLevel_.load (std::memory_order_relaxed);
    destination.fundamentalHz = fundamentalHz_.load (std::memory_order_relaxed);
    const int articulation = visualArticulation_.load (std::memory_order_relaxed);
    destination.lastArticulation = static_cast<Articulation> (
        articulation >= 0 && articulation < static_cast<int> (articulationCount)
            ? articulation : 0);
    destination.lastOctaveOffset = visualOctave_.load (std::memory_order_relaxed);
    destination.activeVoices = activeVoiceCount_.load (std::memory_order_relaxed);
}

TaikoEngine::DrumMeasurements TaikoEngine::measureDrum (int octaveOffset) const noexcept
{
    return measure (applied_, octaveOffset, 2.0f * pitchBend_);
}

TaikoEngine::DrumMeasurements TaikoEngine::measure (const EngineParameters& parameters,
                                                     int octaveOffset,
                                                     float pitchBendSemitones) noexcept
{
    const auto drum = resolveDrumFor (parameters, pitchBendSemitones, octaveOffset);
    const auto& entry = membraneModes()[0]; // the (0,1) mode
    const auto lambda = static_cast<float> (entry.besselZero);

    DrumMeasurements result;
    result.radiusMetres = drum.radius;
    result.depthMetres = drum.depth;
    result.tensionNewtonsPerMetre = drum.tension;
    result.arealDensityKgPerSquareMetre = drum.batterDensity;
    result.waveSpeedMetresPerSecond = drum.waveSpeed;
    result.idealFundamentalHz =
        drum.waveSpeed * lambda / (2.0f * piFloat * drum.radius);

    const float idealResonant =
        drum.resonantWaveSpeed * lambda / (2.0f * piFloat * drum.radius);
    const float loadShape = 1.0f;
    const float loadBatter = 1.0f
        / std::sqrt (1.0f + 0.85f * loadShape * airDensity * drum.radius / drum.batterDensity);
    const float loadResonant = 1.0f
        / std::sqrt (1.0f + 0.85f * loadShape * airDensity * drum.radius / drum.resonantDensity);

    const float omegaBatter = 2.0f * piFloat * result.idealFundamentalHz * loadBatter;
    const float omegaResonant = 2.0f * piFloat * idealResonant * loadResonant;

    const float cavity = drum.cavityStiffness * 4.0f / (lambda * lambda);
    const float diagonalB = omegaBatter * omegaBatter + cavity / drum.batterDensity;
    const float diagonalR = omegaResonant * omegaResonant + cavity / drum.resonantDensity;
    const float offDiagonal =
        cavity / std::sqrt (drum.batterDensity * drum.resonantDensity);

    // Solved through the same routine the render path uses, so the readout and
    // the audio cannot disagree about the drum - including at zero coupling,
    // where the pair is degenerate.
    float upperEigen = 0.0f, upperB = 0.0f, upperR = 0.0f;
    float lowerEigen = 0.0f, lowerB = 0.0f, lowerR = 0.0f;
    solveAxisymmetricBranch (diagonalB, diagonalR, offDiagonal, 0, upperEigen,
                             upperB, upperR);
    solveAxisymmetricBranch (diagonalB, diagonalR, offDiagonal, 1, lowerEigen,
                             lowerB, lowerR);

    const float upper = std::sqrt (std::max (upperEigen, 0.0f));
    const float lower = std::sqrt (std::max (lowerEigen, 0.0f));

    result.breathingModeHz = upper / (2.0f * piFloat);
    result.loadedFundamentalHz = lower / (2.0f * piFloat);

    // How long each branch rings, each with its own radiation share. The two
    // differ a great deal on a sealed drum, because only the branch that
    // changes the body's volume radiates - so reporting one branch's decay
    // beside the other branch's frequency described neither. What the editor
    // labels a tail is how long the drum rings, which is the longer of them.
    const auto branchTail = [&drum] (float omega, float vectorB, float vectorR)
    {
        if (! (omega > 0.0f))
            return 0.0f;

        const float volumeShare = vectorB / std::sqrt (drum.batterDensity)
                                + vectorR / std::sqrt (drum.resonantDensity);
        const float efficiency =
            radiationEfficiency (0, omega * drum.radius / soundSpeed);
        const float decay = 0.5f * drum.headLossFactor * omega
                          + drum.radiationScale * airDensity * soundSpeed * efficiency
                                * std::abs (volumeShare) / (2.0f * drum.batterDensity)
                          + drum.edgeLoss;
        return decay > 0.0f ? 6.9078f / decay : 0.0f;
    };

    result.tailSeconds = std::max (branchTail (upper, upperB, upperR),
                                   branchTail (lower, lowerB, lowerR));

    return result;
}

} // namespace taikor
