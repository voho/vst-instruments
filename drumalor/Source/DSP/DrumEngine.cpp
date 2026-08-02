#include "DrumEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace drumalor
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;
constexpr float minusSixtyDb = -6.90775527898f;
constexpr float minusOneHundredDb = -11.51292546497f;
constexpr float referenceSampleRate = 48000.0f;
constexpr float silenceThreshold = 1.0e-5f;
// Long enough that a -24 semitone kick (13 Hz at the end of its sweep) cannot
// look quiet merely because the meter spans a zero crossing.
constexpr float peakReleaseSeconds = 0.030f;
constexpr float retirementFadeSeconds = 0.003f;
constexpr float forcedFadeSeconds = 0.005f;
// The slowest oscillator is a -24 semitone Kick at 13 Hz. Requiring more
// than its 38.5 ms absolute-peak spacing prevents zero crossings from being
// mistaken for a completed tail.
constexpr float naturalQuietHoldSeconds = 0.045f;
constexpr float supplySagAttackSeconds = 0.004f;
constexpr float supplySagReleaseSeconds = 0.085f;

// The trailing three defaults are the per-voice mixer: level in decibels, the
// constant-power pan position that used to be hard-coded in initialiseVoice(),
// and the choke/mute group. Only the two hi-hats share a group by default, so
// a factory kit still behaves exactly like the original pedal-linked pair.
constexpr std::array<InstrumentMetadata, instrumentCount> metadata {{
    { Instrument::Kick,      "Kick",       "kick",       36, "Punch",   "Drive",      { 0.68f, 0.42f, 0.0f, 0.55f, 0.0f,  0.00f, 0 } },
    { Instrument::Snare,     "Snare",      "snare",      38, "Wires",   "Snap",       { 0.62f, 0.64f, 0.0f, 0.48f, 0.0f,  0.00f, 0 } },
    { Instrument::Clap,      "Clap",       "clap",       39, "Spread",  "Tone",       { 0.48f, 0.62f, 0.0f, 0.45f, 0.0f,  0.00f, 0 } },
    { Instrument::ClosedHat, "Closed Hat", "closedHat", 42, "Metal",   "Tone",       { 0.58f, 0.70f, 0.0f, 0.30f, 0.0f,  0.16f, 1 } },
    { Instrument::OpenHat,   "Open Hat",   "openHat",   46, "Metal",   "Tone",       { 0.62f, 0.68f, 0.0f, 0.55f, 0.0f,  0.20f, 1 } },
    { Instrument::Ride,      "Ride",       "ride",       51, "Bell",    "Tone",       { 0.45f, 0.62f, 0.0f, 0.62f, 0.0f,  0.27f, 0 } },
    { Instrument::Crash,     "Crash",      "crash",      49, "Spread",  "Brightness", { 0.58f, 0.65f, 0.0f, 0.65f, 0.0f, -0.27f, 0 } },
    { Instrument::LowTom,    "Low Tom",    "lowTom",     45, "Punch",   "Skin",       { 0.55f, 0.40f, 0.0f, 0.60f, 0.0f, -0.20f, 0 } },
    { Instrument::MidTom,    "Mid Tom",    "midTom",     47, "Punch",   "Skin",       { 0.55f, 0.45f, 0.0f, 0.52f, 0.0f,  0.00f, 0 } },
    { Instrument::HighTom,   "High Tom",   "highTom",    50, "Punch",   "Skin",       { 0.50f, 0.50f, 0.0f, 0.45f, 0.0f,  0.20f, 0 } },
    { Instrument::Shaker,    "Shaker",     "shaker",     82, "Density", "Color",      { 0.62f, 0.62f, 0.0f, 0.45f, 0.0f,  0.12f, 0 } },
    { Instrument::Perc1,     "Perc 1",     "perc1",      56, "Ratio",   "Drive",      { 0.50f, 0.45f, 0.0f, 0.45f, 0.0f, -0.12f, 0 } },
    { Instrument::Perc2,     "Perc 2",     "perc2",      75, "Hollow",  "Click",      { 0.55f, 0.55f, 0.0f, 0.40f, 0.0f,  0.12f, 0 } },
}};

constexpr std::array<float, instrumentCount> minimumDecay {{
    0.07f, 0.06f, 0.08f, 0.018f, 0.12f, 0.25f, 0.30f,
    0.10f, 0.08f, 0.06f, 0.05f, 0.05f, 0.025f
}};

constexpr std::array<float, instrumentCount> maximumDecay {{
    2.40f, 1.80f, 1.80f, 0.32f, 3.20f, 6.00f, 7.00f,
    2.40f, 1.80f, 1.40f, 1.20f, 2.00f, 1.10f
}};

// Ideal circular-membrane (Bessel) mode ratios: the zeros of J_m divided by
// the first zero of J_0. A drum head loaded by the air inside its shell rings
// lower than the ideal series at every mode above the fundamental, so the
// engine raises each ratio to an air-loading exponent rather than pretending
// the head is a massless ideal membrane.
//
// The series runs to twelve because a resonator bank has twelve slots and a
// real head has far more than five modes. Stopping at 2.917 put every mode a
// tom could ring inside its bottom two octaves - a 82 Hz floor tom had nothing
// modelled above 240 Hz, which is why its measured spectrum fell off a cliff
// where a real drum still has the stick in it.
//
//                (0,1)  (1,1)  (2,1)  (0,2)  (3,1)  (1,2)
//                (4,1)  (2,2)  (0,3)  (5,1)  (3,2)  (6,1)
constexpr std::array<float, 12> membraneModeRatios {{
    1.000f, 1.593f, 2.135f, 2.295f, 2.653f, 2.917f,
    3.156f, 3.500f, 3.598f, 3.647f, 4.059f, 4.132f
}};

// The circumferential order m of each entry above. A strike on the geometric
// centre of a head drives only the m = 0 family, because J_m(0) is zero for
// every other m; moving the beater or the stick off centre brings the rest in.
// That is the whole difference between a centred and an off-centre hit, and it
// is why a drum struck dead in the middle sounds thin and hollow.
constexpr std::array<int, 12> membraneModeOrders {{
    0, 1, 2, 0, 3, 1, 4, 2, 0, 5, 3, 6
}};

// The zeros themselves. The ratios above are these divided by the first one,
// but the air between two heads couples to a mode through its shape rather than
// its pitch, so the undivided value is needed too.
constexpr std::array<float, 12> membraneModeZeros {{
    2.4048f, 3.8317f, 5.1356f, 5.5201f, 6.3802f, 7.0156f,
    7.5883f, 8.4172f, 8.6537f, 8.7715f, 9.7610f, 9.9361f
}};

// Every recursive state in the engine - envelopes, resonators, biquads, the DC
// blockers and the bus detector - decays geometrically for as long as its voice
// lives. Once one of them falls under the smallest normal float, every further
// multiply is a subnormal operation, which on x86 traps into microcode. Measured
// on a busy thirteen-voice kit that cost 3.1x the engine's normal CPU time in
// any host that does not set flush-to-zero on our behalf. Snapping to zero at
// -600 dBFS is far below the -100 dB at which a voice already counts as silent,
// so it is inaudible while making the cost independent of the host FPU mode.
constexpr float denormalFloor = 1.0e-30f;

float flushDenormal (float value) noexcept
{
    return std::abs (value) < denormalFloor ? 0.0f : value;
}

float clampUnit (float value, float fallback = 0.5f) noexcept
{
    return std::isfinite (value) ? std::clamp (value, 0.0f, 1.0f) : fallback;
}

float decibelsToGain (float decibels) noexcept
{
    if (! std::isfinite (decibels))
        return 1.0f;
    decibels = std::clamp (decibels, minimumVoiceLevelDecibels,
                           maximumVoiceLevelDecibels);
    // Exactly 1.0f at the 0 dB default, so a factory kit is bit-identical to
    // the engine before the per-voice mixer existed.
    return decibels == 0.0f ? 1.0f : std::pow (10.0f, 0.05f * decibels);
}

// One-pole approach that lands exactly, so a control returned to zero actually
// reaches bypass instead of gliding toward it for the rest of the session.
float approachTarget (float current, float target, float coefficient) noexcept
{
    current += coefficient * (target - current);
    return std::abs (target - current) < 1.0e-5f ? target : current;
}

float coefficientForTime (float seconds, float sampleRate) noexcept
{
    return std::exp (minusSixtyDb / std::max (1.0f, seconds * sampleRate));
}

// Ascending series for J_m. Every argument this engine asks for is a Bessel
// zero times a fraction of the head's radius, so nothing above about four ever
// reaches it and a dozen terms are accurate past float precision. Note-on only:
// it never runs in the audio loop.
float besselJ (int order, float value) noexcept
{
    const double half = 0.5 * static_cast<double> (value);
    double term = 1.0;
    for (int factorial = 1; factorial <= order; ++factorial)
        term *= half / static_cast<double> (factorial);
    double sum = term;
    for (int index = 1; index <= 14; ++index)
        sum += (term *= -(half * half)
                / (static_cast<double> (index) * static_cast<double> (index + order)));
    return static_cast<float> (sum);
}

// Air at room temperature: density 1.2 kg/m^3 and sound speed 343 m/s, so the
// adiabatic bulk modulus rho0 c^2 that resists being squeezed inside a drum is
// about 141 kPa.
constexpr float airBulkModulus = 141200.0f;
constexpr float soundSpeed = 343.0f;
constexpr float airDensity = 1.2f;

// See buildHeadBank: the resonators are normalised for noise, and a head that
// is struck rather than driven needs the strike scaled back up to the body.
constexpr float struckHeadScale = 3.0f;

// How much of a strike ever reaches a given frequency. A beater or a stick
// touches the head for a finite time, and the force it delivers across that
// time is close to a single raised cosine; the transform of that pulse is flat
// well below one cycle per contact and falls to nothing at two. This is why a
// hard, fast strike is brighter rather than merely louder, and why no strike
// reaches the whole of a head's series - the contact is a low-pass filter made
// of felt and timing rather than of poles.
float contactSpectrum (float frequency, float contactSeconds) noexcept
{
    const float cycles = frequency * contactSeconds;
    if (cycles < 1.0e-4f)
        return 1.0f;
    const float denominator = 1.0f - cycles * cycles;
    // The transform is finite where both halves vanish together; take the limit
    // rather than the quotient.
    if (std::abs (denominator) < 1.0e-3f)
        return 0.5f;
    const float argument = 0.5f * twoPi * cycles;
    return std::abs (std::sin (argument) / (argument * denominator));
}

// The stiffness the trapped air adds to an axisymmetric mode, as a fraction of
// the mode's own stiffness. A two-headed drum is not two independent membranes:
// the air between them is a spring across both, and every mode that changes the
// enclosed volume feels it.
//
// The 4/lambda^2 is what survives of the mode's swept volume once its modal
// mass is divided out. A mode shaped like J0(lambda r / a) integrates over the
// head to 2 J1(lambda) / lambda of net volume, and the same J1(lambda) squared
// sits inside the modal mass, so the two cancel and leave the bare ratio - and
// with it the head's radius, which is why a drum's air mode depends on how deep
// the shell is and not on how wide.
float cavityStiffness (float frequency, float lambda, float headDensity,
                       float shellDepth) noexcept
{
    const float omega = twoPi * std::max (1.0f, frequency);
    return airBulkModulus * (4.0f / (lambda * lambda))
        / (std::max (0.01f, headDensity * shellDepth) * omega * omega);
}

double besselI0 (double value) noexcept
{
    // Stable power series for the Kaiser-window range used by the metallic
    // decimator. This runs only in prepare/reset, never in the audio loop.
    const double squaredQuarter = 0.25 * value * value;
    double sum = 1.0;
    double term = 1.0;
    for (int order = 1; order <= 24; ++order)
    {
        term *= squaredQuarter
            / static_cast<double> (order * order);
        sum += term;
        if (term < 1.0e-15 * sum)
            break;
    }
    return sum;
}

float polyBlep (float phase, float phaseIncrement) noexcept
{
    phaseIncrement = std::clamp (phaseIncrement, 1.0e-6f, 0.49f);
    if (phase < phaseIncrement)
    {
        const float normalized = phase / phaseIncrement;
        return normalized + normalized - normalized * normalized - 1.0f;
    }
    if (phase > 1.0f - phaseIncrement)
    {
        const float normalized = (phase - 1.0f) / phaseIncrement;
        return normalized * normalized + normalized + normalized + 1.0f;
    }
    return 0.0f;
}

float constantPowerLeft (float pan) noexcept
{
    return std::sqrt (0.5f * (1.0f - std::clamp (pan, -1.0f, 1.0f)));
}

float constantPowerRight (float pan) noexcept
{
    return std::sqrt (0.5f * (1.0f + std::clamp (pan, -1.0f, 1.0f)));
}

float rationalShaper (float value, float positiveCurvature,
                      float negativeCurvature) noexcept
{
    value = flushDenormal (std::clamp (std::isfinite (value) ? value : 0.0f, -64.0f, 64.0f));
    const float curvature = value >= 0.0f ? positiveCurvature : negativeCurvature;
    return value / (1.0f + curvature * std::abs (value));
}

// The antiderivative of rationalShaper: F(x) = integral of t / (1 + c|t|).
//
// Both signs collapse to F(x) = x^2 * h(c|x|) with h(u) = (u - log1p(u)) / u^2,
// which is what this evaluates. That is algebraically the same function as the
// textbook x/c - log1p(cx)/c^2, but it is a very different computation: the
// textbook form subtracts two quantities of size x/c to leave a result of size
// x^2/2, so as the curvature falls the answer is nothing but rounding noise.
// That matters because the ADAA divided difference below then divides F by a
// sample-to-sample step of order 1e-3 and amplifies the noise by three orders
// of magnitude: with c = 1e-4 and x = 0.5 the two terms are ~5e3 with an ulp of
// ~5e-4, and the bus saturator emitted a full-scale square wave. Bus Drive
// reaches that region both statically (its smallest non-zero parameter step is
// 0.001) and, once it is smoothed, on every ramp toward bypass.
//
// h() has no cancellation of its own beyond the numerator, which is evaluated
// in double so the subtraction stays exact far past float precision, and below
// u = 1e-6 - where even double would start to lose digits - by the leading
// terms of the power series h(u) = 1/2 - u/3 + u^2/4 - ... The two agree to
// 1.3e-10 relative at the crossover, three orders inside float precision.
//
// Checked against a long-double evaluation over curvatures from 1e-6 to 10 and
// the stage's whole input range: this form holds 5.8e-8 relative everywhere,
// where the textbook one reached 1.9e-3 at c = 1e-3 and lost the value entirely
// at 1e-6.
//
// The form is also well defined at c = 0 (h -> 1/2, F -> x^2/2), so the stage
// degrades continuously into a straight wire instead of into a 0/0.
float rationalShaperPrimitive (float value, float positiveCurvature,
                               float negativeCurvature) noexcept
{
    value = std::clamp (std::isfinite (value) ? value : 0.0f, -64.0f, 64.0f);
    const float curvature = value >= 0.0f ? positiveCurvature : negativeCurvature;
    const double magnitude = std::abs (static_cast<double> (value));
    const double product = std::max (0.0, static_cast<double> (curvature) * magnitude);
    if (product > 1.0e-6)
    {
        // x^2 * h(u) with u = c|x| is the same value as (u - log1p(u)) / c^2,
        // and the second spelling saves the two multiplies.
        const double scale = static_cast<double> (curvature);
        return static_cast<float> (
            (product - std::log1p (product)) / (scale * scale));
    }
    return static_cast<float> (
        magnitude * magnitude * (0.5 - product * (1.0 / 3.0 - 0.25 * product)));
}

float antialiasedRationalShaper (float input, float& previousInput,
                                 float positiveCurvature,
                                 float negativeCurvature) noexcept
{
    input = flushDenormal (
        std::clamp (std::isfinite (input) ? input : 0.0f, -64.0f, 64.0f));
    const float previous = flushDenormal (std::clamp (
        std::isfinite (previousInput) ? previousInput : input, -64.0f, 64.0f));
    const float difference = input - previous;
    const float threshold = 1.0e-4f * (1.0f + std::abs (input) + std::abs (previous));
    float output = 0.0f;
    if (std::abs (difference) <= threshold)
    {
        // The midpoint is the limiting derivative of the divided difference
        // and avoids cancellation when two consecutive inputs nearly match.
        output = rationalShaper (0.5f * (input + previous),
                                 positiveCurvature, negativeCurvature);
    }
    else
    {
        output = (rationalShaperPrimitive (input, positiveCurvature, negativeCurvature)
                  - rationalShaperPrimitive (previous, positiveCurvature, negativeCurvature))
            / difference;
    }
    // ADAA's divided difference averages even a perfectly linear transfer
    // over the current and previous sample. Apply it only to the nonlinear
    // residual f(x) - x, leaving the non-aliasing linear path undelayed. This
    // preserves low-level brightness and cross-sample-rate gain.
    output += input - 0.5f * (input + previous);
    previousInput = input;
    return std::isfinite (output) ? output : 0.0f;
}

// Cached-antiderivative form of the same stage. F(previous) is bit-identical to
// the F(input) this function already evaluated one sample earlier, so carrying
// it forward removes one log1p per call. That is only true while the two
// curvatures stay fixed between calls, which holds for every voice (they follow
// from the instrument and characterB, both frozen at note-on) and for the
// master stage (fixed at 1.0). The bus drive stage retunes its curvature from
// the smoothed Drive control every sample and keeps the recomputing form above.
// Measured
// on a dense thirteen-voice kit, the voice stage was 41 % of engine time.
float antialiasedRationalShaperCached (float input, float& previousInput,
                                       float& previousPrimitive,
                                       float positiveCurvature,
                                       float negativeCurvature) noexcept
{
    input = flushDenormal (
        std::clamp (std::isfinite (input) ? input : 0.0f, -64.0f, 64.0f));
    const float previous = flushDenormal (std::clamp (
        std::isfinite (previousInput) ? previousInput : input, -64.0f, 64.0f));
    const float primitive = rationalShaperPrimitive (
        input, positiveCurvature, negativeCurvature);
    const float difference = input - previous;
    const float threshold = 1.0e-4f * (1.0f + std::abs (input) + std::abs (previous));
    float output = std::abs (difference) <= threshold
        ? rationalShaper (0.5f * (input + previous),
                          positiveCurvature, negativeCurvature)
        : (primitive - previousPrimitive) / difference;
    output += input - 0.5f * (input + previous);
    previousInput = input;
    previousPrimitive = primitive;
    return std::isfinite (output) ? output : 0.0f;
}

// The voice output stage's transfer curve. Only the kick opens its curvature up
// with Drive; every other voice shares one fixed slightly-mismatched pair.
struct AnalogCurve
{
    float positiveCurvature { 0.205f };
    float negativeCurvature { 0.165f };
    float makeup { 1.0f };
};

AnalogCurve analogCurveFor (Instrument instrument, float characterB) noexcept
{
    // A saturating stage that is exactly gain-compensated only ever takes level
    // away, so both voices whose second control is labelled Drive get modest
    // drive-dependent makeup: saturation should add density rather than making
    // the voice retreat as the knob comes up.
    if (instrument == Instrument::Kick)
        return { 0.18f + 0.30f * characterB, 0.18f - 0.08f * characterB,
                 1.0f + 0.32f * characterB };
    if (instrument == Instrument::Perc1)
        return { 0.205f, 0.165f, 1.0f + 0.42f * characterB };
    return {};
}
} // namespace

const InstrumentMetadata& getInstrumentMetadata (Instrument instrument) noexcept
{
    const auto index = static_cast<std::size_t> (instrument);
    return metadata[index < metadata.size() ? index : 0u];
}

std::string_view getInstrumentDisplayName (Instrument instrument) noexcept
{
    return getInstrumentMetadata (instrument).displayName;
}

std::string_view getInstrumentSlug (Instrument instrument) noexcept
{
    return getInstrumentMetadata (instrument).slug;
}

int getStandardMidiNote (Instrument instrument) noexcept
{
    return getInstrumentMetadata (instrument).standardMidiNote;
}

std::string_view getCharacterALabel (Instrument instrument) noexcept
{
    return getInstrumentMetadata (instrument).characterALabel;
}

std::string_view getCharacterBLabel (Instrument instrument) noexcept
{
    return getInstrumentMetadata (instrument).characterBLabel;
}

std::optional<Instrument> instrumentForMidiNote (int midiNote) noexcept
{
    switch (midiNote)
    {
        case 35: case 36: return Instrument::Kick;
        case 38: case 40: return Instrument::Snare;
        case 39: return Instrument::Clap;
        case 42: case 44: return Instrument::ClosedHat;
        case 46: return Instrument::OpenHat;
        case 51: case 53: case 59: return Instrument::Ride;
        case 49: case 57: return Instrument::Crash;
        case 41: case 43: case 45: return Instrument::LowTom;
        case 47: case 48: return Instrument::MidTom;
        case 50: return Instrument::HighTom;
        case 70: case 82: return Instrument::Shaker;
        case 56: return Instrument::Perc1;
        case 37: case 75: case 76: case 77: return Instrument::Perc2;
        default: return std::nullopt;
    }
}

float DrumEngine::Biquad::tick (float input) noexcept
{
    const float output = b0 * input + z1;
    z1 = flushDenormal (b1 * input - a1 * output + z2);
    z2 = flushDenormal (b2 * input - a2 * output);
    return output;
}

void DrumEngine::Biquad::clear() noexcept
{
    z1 = z2 = 0.0f;
}

float DrumEngine::Resonator::tick (float input) noexcept
{
    const float output = flushDenormal (inputGain * input + a1 * y1 + a2 * y2);
    y2 = y1;
    y1 = output;
    return output;
}

void DrumEngine::Resonator::strike (float amplitude) noexcept
{
    // The state one sample before the strike that a mode starting from rest
    // would have had. Superposing it leaves whatever the resonator was already
    // ringing with untouched.
    y2 -= amplitude * strikeGain;
}

void DrumEngine::Resonator::clear() noexcept
{
    y1 = y2 = 0.0f;
}

DrumEngine::DrumEngine() noexcept
{
    for (const auto& item : metadata)
        setInstrumentParameters (item.instrument, item.defaultParameters);
}

bool DrumEngine::validInstrument (Instrument instrument) noexcept
{
    return static_cast<std::size_t> (instrument) < instrumentCount;
}

std::size_t DrumEngine::indexFor (Instrument instrument) noexcept
{
    return static_cast<std::size_t> (instrument);
}

void DrumEngine::prepare (double sampleRate, int maxBlockSize) noexcept
{
    if (! std::isfinite (sampleRate))
        sampleRate = 48000.0;
    sampleRate_ = std::clamp (sampleRate, 8000.0, 192000.0);
    inverseSampleRate_ = static_cast<float> (1.0 / sampleRate_);
    maxBlockSize_ = std::max (1, maxBlockSize);
    const float floatSampleRate = static_cast<float> (sampleRate_);
    maximumVoiceSamples_ = std::max<std::uint64_t> (
        1u, static_cast<std::uint64_t> (maximumTailSeconds * sampleRate_));
    const auto forcedFadeSamples = std::max<std::uint64_t> (
        1u, static_cast<std::uint64_t> (std::ceil (forcedFadeSeconds * floatSampleRate)));
    forcedFadeStartSamples_ = maximumVoiceSamples_ > forcedFadeSamples
        ? maximumVoiceSamples_ - forcedFadeSamples : 0u;
    naturalQuietHoldSamples_ = std::max<std::uint32_t> (
        1u, static_cast<std::uint32_t> (std::ceil (
            naturalQuietHoldSeconds * floatSampleRate)));
    peakReleaseMultiplier_ = coefficientForTime (peakReleaseSeconds, floatSampleRate);
    retirementFadeMultiplier_ = std::exp (
        minusOneHundredDb / std::max (1.0f, retirementFadeSeconds * floatSampleRate));
    forcedFadeMultiplier_ = std::exp (
        minusOneHundredDb / static_cast<float> (forcedFadeSamples));
    sagAttackCoefficient_ = 1.0f - std::exp (
        -1.0f / std::max (1.0f, supplySagAttackSeconds * floatSampleRate));
    sagReleaseCoefficient_ = 1.0f - std::exp (
        -1.0f / std::max (1.0f, supplySagReleaseSeconds * floatSampleRate));
    gainSmoothingCoefficient_ = 1.0f - std::exp (-inverseSampleRate_ / 0.020f);
    dcBlockerCoefficient_ = std::exp (-twoPi * 12.0f * inverseSampleRate_);
    // Modal excitation energy scales with the sample period; the audible noise
    // layers instead read a fixed 48 kHz grid, so both follow the same ratio for
    // different reasons.
    modalNoiseScale_ = referenceSampleRate / floatSampleRate;
    bandLimitedNoiseIncrement_ = referenceSampleRate / floatSampleRate;
    // The discontinuous metallic source islands run at a high internal rate,
    // while resonators, envelopes and the per-voice circuit stages remain at
    // the host rate. This targets oversampling where Schmitt edges and ring
    // products actually create out-of-band energy instead of multiplying the
    // cost of the entire 128-voice path.
    metallicOversampleFactor_ = floatSampleRate < 44100.0f ? 8
                               : floatSampleRate <= 48000.0f ? 4
                               : floatSampleRate <= 96000.0f ? 2 : 1;
    metallicInternalSampleRate_ = floatSampleRate
        * static_cast<float> (metallicOversampleFactor_);
    metallicInverseSampleRate_ = 1.0f / metallicInternalSampleRate_;
    configureMetallicDecimator();
    metallicIncrementSmoothing_ = 1.0f - std::exp (
        -1.0f / std::max (1.0f, 0.0015f * metallicInternalSampleRate_));
    cymbalClockIncrement_ = std::min (1.0f, 30000.0f * inverseSampleRate_);
    const float reconstructionCutoff = std::min (13500.0f, 0.42f * floatSampleRate);
    cymbalReconstructionCoefficient_ = 1.0f - std::exp (
        -twoPi * reconstructionCutoff / floatSampleRate);
    // The shared bus compressor uses a peak detector with a fast musical
    // attack and a slow release, so it glues a kit without pumping on hats.
    busAttackCoefficient_ = 1.0f - std::exp (
        -1.0f / std::max (1.0f, 0.004f * floatSampleRate));
    busReleaseCoefficient_ = 1.0f - std::exp (
        -1.0f / std::max (1.0f, 0.140f * floatSampleRate));
    for (int i = 0; i < sineTableSize; ++i)
        sineTable_[static_cast<std::size_t> (i)] = std::sin (
            twoPi * static_cast<float> (i) / static_cast<float> (sineTableSize));
    prepared_ = true;
    reset();
}

void DrumEngine::reset() noexcept
{
    for (auto& voice : voices_)
        voice = Voice {};
    for (auto& voice : retiringVoices_)
        voice = Voice {};
    triggerCounters_.fill (0);
    componentDrift_.fill (0.0f);
    metallicBankVoiceCounts_.fill (0);
    metallicBankMask_ = 0u;
    resetMetallicOscillatorBanks();
    anyVoiceActive_ = false;
    generation_ = 0;
    smoothedOutputGain_ = outputGain_.load (std::memory_order_relaxed);
    smoothedBusDrive_ = busDrive_.load (std::memory_order_relaxed);
    smoothedBusCompression_ = busCompression_.load (std::memory_order_relaxed);
    dcInputLeft_ = dcInputRight_ = 0.0f;
    dcOutputLeft_ = dcOutputRight_ = 0.0f;
    masterAdaaPreviousLeft_ = masterAdaaPreviousRight_ = 0.0f;
    masterAdaaPrimitiveLeft_ = masterAdaaPrimitiveRight_ = 0.0f;
    resetBusStage();
    meterPeakLeft_ = meterPeakRight_ = 0.0f;
    outputLevelLeft_.store (0.0f, std::memory_order_relaxed);
    outputLevelRight_.store (0.0f, std::memory_order_relaxed);
    busGainMeter_.store (1.0f, std::memory_order_relaxed);
    for (auto& level : instrumentLevels_)
        level.store (0.0f, std::memory_order_relaxed);
    activeVoiceCount_.store (0, std::memory_order_relaxed);
}

void DrumEngine::resetBusStage() noexcept
{
    busEnvelope_ = 0.0f;
    busGain_ = 1.0f;
    busDriveAdaaLeft_ = busDriveAdaaRight_ = 0.0f;
}

void DrumEngine::setInstrumentParameters (Instrument instrument,
                                           const InstrumentParameters& values) noexcept
{
    if (! validInstrument (instrument))
        return;
    const auto& defaults = getInstrumentMetadata (instrument).defaultParameters;
    auto& target = parameters_[indexFor (instrument)];
    target.characterA.store (clampUnit (values.characterA, defaults.characterA),
                             std::memory_order_relaxed);
    target.characterB.store (clampUnit (values.characterB, defaults.characterB),
                             std::memory_order_relaxed);
    const float pitch = std::isfinite (values.pitch) ? values.pitch : defaults.pitch;
    target.pitch.store (std::clamp (pitch, -24.0f, 24.0f), std::memory_order_relaxed);
    target.decay.store (clampUnit (values.decay, defaults.decay),
                        std::memory_order_relaxed);
    const float level = std::isfinite (values.level) ? values.level : defaults.level;
    target.level.store (std::clamp (level, minimumVoiceLevelDecibels,
                                    maximumVoiceLevelDecibels),
                        std::memory_order_relaxed);
    const float pan = std::isfinite (values.pan) ? values.pan : defaults.pan;
    target.pan.store (std::clamp (pan, -1.0f, 1.0f), std::memory_order_relaxed);
    target.chokeGroup.store (std::clamp (values.chokeGroup, 0, chokeGroupCount),
                             std::memory_order_relaxed);
}

void DrumEngine::setKitParameters (const KitParameters& values) noexcept
{
    humanise_.store (clampUnit (values.humanise, 0.5f), std::memory_order_relaxed);
    busDrive_.store (clampUnit (values.busDrive, 0.0f), std::memory_order_relaxed);
    busCompression_.store (clampUnit (values.busCompression, 0.0f),
                           std::memory_order_relaxed);
}

void DrumEngine::setOutputGain (float linearGain) noexcept
{
    outputGain_.store (std::isfinite (linearGain) ? std::clamp (linearGain, 0.0f, 2.0f) : 0.82f,
                       std::memory_order_relaxed);
}

float DrumEngine::getOutputLevel (int channel) const noexcept
{
    return (channel <= 0 ? outputLevelLeft_ : outputLevelRight_)
        .load (std::memory_order_relaxed);
}

float DrumEngine::getInstrumentLevel (Instrument instrument) const noexcept
{
    return validInstrument (instrument)
        ? instrumentLevels_[indexFor (instrument)].load (std::memory_order_relaxed)
        : 0.0f;
}

float DrumEngine::getBusGain() const noexcept
{
    return busGainMeter_.load (std::memory_order_relaxed);
}

InstrumentParameters DrumEngine::snapshotParameters (Instrument instrument) const noexcept
{
    const auto& source = parameters_[indexFor (instrument)];
    return { source.characterA.load (std::memory_order_relaxed),
             source.characterB.load (std::memory_order_relaxed),
             source.pitch.load (std::memory_order_relaxed),
             source.decay.load (std::memory_order_relaxed),
             source.level.load (std::memory_order_relaxed),
             source.pan.load (std::memory_order_relaxed),
             source.chokeGroup.load (std::memory_order_relaxed) };
}

float DrumEngine::decaySecondsFor (Instrument instrument, float normalizedDecay) const noexcept
{
    const auto index = indexFor (instrument);
    const float low = minimumDecay[index];
    const float high = maximumDecay[index];
    return low * std::pow (high / low, clampUnit (normalizedDecay));
}

std::uint32_t DrumEngine::hash32 (std::uint32_t value) noexcept
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value == 0u ? 1u : value;
}

float DrumEngine::signedUnitFromHash (std::uint32_t value) noexcept
{
    const auto hashed = hash32 (value);
    return static_cast<float> (hashed & 0x00ffffffu) / 8388607.5f - 1.0f;
}

float DrumEngine::nextNoise (Voice& voice) noexcept
{
    std::uint32_t x = voice.noiseState;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    voice.noiseState = x == 0u ? 1u : x;
    return static_cast<float> (voice.noiseState & 0x00ffffffu) / 8388607.5f - 1.0f;
}

// Every noise layer in the kit - snare wires, clap bursts, stick and shaker
// grain - is heard through a filter whose bandwidth is fixed in hertz, so what
// reaches the listener is the noise's power *density*, not its variance. Raw
// full-rate white noise spreads a fixed variance over the whole Nyquist band,
// which means its density, and therefore every filtered noise layer, loses
// 3 dB per doubling of the host sample rate. That measured as a 6 dB quieter
// clap and a snare whose spectral centroid fell from 1.9 kHz to 0.9 kHz between
// 44.1 and 192 kHz. Generating the noise on a fixed 48 kHz grid and reading it
// with linear interpolation holds the density constant instead: the audible
// band is identical at every rate, and at 48 kHz the increment is exactly 1 so
// this reproduces the raw generator sample for sample.
float DrumEngine::nextBandLimitedNoise (Voice& voice) const noexcept
{
    if (! voice.bandLimitedNoiseReady)
    {
        voice.bandLimitedNoiseCurrent = nextNoise (voice);
        voice.bandLimitedNoiseNext = nextNoise (voice);
        voice.bandLimitedNoisePhase = 0.0f;
        voice.bandLimitedNoiseReady = true;
    }

    float sum = voice.bandLimitedNoiseCurrent
        + voice.bandLimitedNoisePhase
              * (voice.bandLimitedNoiseNext - voice.bandLimitedNoiseCurrent);
    int taken = 1;
    voice.bandLimitedNoisePhase += bandLimitedNoiseIncrement_;
    while (voice.bandLimitedNoisePhase >= 1.0f)
    {
        voice.bandLimitedNoisePhase -= 1.0f;
        voice.bandLimitedNoiseCurrent = voice.bandLimitedNoiseNext;
        voice.bandLimitedNoiseNext = nextNoise (voice);
        // Below the reference rate the grid runs faster than the output and
        // values would otherwise be thrown away. Averaging what the output
        // sample spans instead of taking the last of them is what decimation
        // means, and it does two things at once: it puts the same noise density
        // per hertz at every rate - white noise of unchanged per-sample size
        // crammed into a narrower band is a denser noise, by the ratio of the
        // rates, which was 7.8 dB of surplus hiss at 8 kHz - and it leaves the
        // low-rate noise a low-passed copy of the same sequence rather than an
        // unrelated one, so a filtered noise layer sounds like itself there.
        if (bandLimitedNoiseIncrement_ > 1.0f)
        {
            sum += voice.bandLimitedNoiseCurrent;
            ++taken;
        }
    }
    return sum / static_cast<float> (taken);
}

float DrumEngine::applyAnalogOutputStage (Voice& voice, float input) const noexcept
{
    // A compact circuit-inspired output stage: a rectifier-like envelope pulls
    // down a virtual supply rail on loud transients, while unequal positive and
    // negative transfer curves mimic a slightly mismatched transistor pair.
    // Every state variable belongs to its voice, so polyphonic summing remains
    // deterministic and no cross-voice synchronization is needed.
    const float rectified = std::min (2.0f, std::abs (input));
    const float sagCoefficient = rectified > voice.supplySag
        ? sagAttackCoefficient_ : sagReleaseCoefficient_;
    voice.supplySag = flushDenormal (
        voice.supplySag + sagCoefficient * (rectified - voice.supplySag));
    const float rail = 1.0f - 0.042f * std::min (1.5f, voice.supplySag);
    const float drive = voice.circuitDrive / std::max (0.90f, rail);

    // First-order analytic antiderivative antialiasing evaluates the average
    // transfer over the interval between consecutive inputs. It suppresses
    // the dominant discontinuity in the derivative without oversampling. The
    // operating-point form keeps drive and transistor-pair bias independent.
    const float shaperInput = drive * input + voice.circuitBias;
    const float shaped = antialiasedRationalShaperCached (
        shaperInput, voice.analogPreviousInput, voice.analogPreviousPrimitive,
        voice.analogPositiveCurvature, voice.analogNegativeCurvature);
    const float output = voice.analogMakeup * rail * (shaped - voice.analogZero)
        / std::max (1.0f, drive);
    return std::isfinite (output) ? std::clamp (output, -4.0f, 4.0f) : 0.0f;
}

// One interpolated read of the shared table yields both quadrature components,
// because the table length is an exact multiple of four so the cosine is the
// same phasor a fixed quarter of the table further on. The caller guarantees a
// phase already reduced to [0, 1), so no wrap is needed here.
void DrumEngine::sineAndCosineLookup (float phase, float& sine,
                                      float& cosine) const noexcept
{
    const float position = phase * static_cast<float> (sineTableSize);
    const int whole = static_cast<int> (position);
    const float fraction = position - static_cast<float> (whole);
    const int sineIndex = whole & sineTableMask;
    const int cosineIndex = (whole + sineTableSize / 4) & sineTableMask;
    const float sineA = sineTable_[static_cast<std::size_t> (sineIndex)];
    const float sineB = sineTable_[static_cast<std::size_t> ((sineIndex + 1) & sineTableMask)];
    const float cosineA = sineTable_[static_cast<std::size_t> (cosineIndex)];
    const float cosineB = sineTable_[static_cast<std::size_t> ((cosineIndex + 1) & sineTableMask)];
    sine = sineA + fraction * (sineB - sineA);
    cosine = cosineA + fraction * (cosineB - cosineA);
}

float DrumEngine::oscillator (Voice& voice, int oscillatorIndex) const noexcept
{
    const auto index = static_cast<std::size_t> (std::clamp (oscillatorIndex, 0, oscillatorCount - 1));
    // The tonal core represents a lightly asymmetric analogue resonator, not
    // an immutable table sine. A small supply-dependent pitch term couples it
    // to the voice rail, while explicitly band-limited second/third harmonics
    // reproduce component/core asymmetry without passing a discontinuity into
    // a memoryless waveshaper.
    const float railPitch = 1.0f - 0.0018f * std::min (1.5f, voice.supplySag);
    const float increment = std::clamp (
        voice.phaseIncrements[index] * railPitch, 0.0f, 0.45f);
    voice.phases[index] += increment;
    voice.phases[index] -= std::floor (voice.phases[index]);
    const float phase = voice.phases[index];
    const float asymmetry = voice.oscillatorAsymmetries[index];
    const float secondGain = 0.022f * asymmetry
        * std::clamp ((0.48f - 2.0f * increment) / 0.08f, 0.0f, 1.0f);
    const float thirdGain = (0.004f + 0.005f * std::abs (asymmetry))
        * std::clamp ((0.48f - 3.0f * increment) / 0.08f, 0.0f, 1.0f);
    // The two asymmetry harmonics sit 33 and 41 dB under the fundamental, yet
    // reading them from the table cost two further interpolated lookups - about
    // a tenth of the whole engine. The double- and triple-angle identities
    // produce them from the quadrature pair instead, for one table read in
    // total. The fundamental is bit-identical; the harmonics move by well under
    // -140 dB relative to the voice.
    float sine = 0.0f;
    float cosine = 0.0f;
    sineAndCosineLookup (phase, sine, cosine);
    const float second = 2.0f * sine * cosine;
    const float third = sine * (3.0f - 4.0f * sine * sine);
    const float output = sine + secondGain * second + thirdGain * third;
    return output / (1.0f + std::abs (secondGain) + thirdGain);
}

int DrumEngine::metallicBankIndexFor (Instrument instrument) noexcept
{
    switch (instrument)
    {
        case Instrument::ClosedHat: return 0;
        case Instrument::OpenHat:   return 1;
        case Instrument::Ride:      return 2;
        case Instrument::Crash:     return 3;
        case Instrument::Perc1:     return 4;
        default:                    return -1;
    }
}

void DrumEngine::configureMetallicDecimator() noexcept
{
    // The discontinuous source island needs substantially more rejection than
    // a light audio low-pass can provide before an N:1 sample-rate change.
    // These odd-length Kaiser-windowed sinc kernels target an approximately
    // 80 dB stopband between 0.40 and 0.50 of the host sample rate. The tap
    // counts scale with the internal rate so the transition stays comparable
    // for every adaptive oversampling factor.
    metallicDecimatorCoefficients_.fill (0.0f);
    metallicDecimatorTapCount_ = metallicOversampleFactor_ <= 1 ? 1
                                 : metallicOversampleFactor_ == 2 ? 129
                                 : metallicOversampleFactor_ == 4 ? 257 : 401;
    if (metallicDecimatorTapCount_ == 1)
    {
        metallicDecimatorCoefficients_[0] = 1.0f;
        return;
    }

    constexpr double beta = 7.85726; // Kaiser beta for roughly 80 dB rejection.
    const int halfLength = metallicDecimatorTapCount_ / 2;
    const double cutoff = 0.45 / static_cast<double> (metallicOversampleFactor_);
    const double inverseWindowDenominator = 1.0 / besselI0 (beta);
    double coefficientSum = 0.0;
    for (int tap = 0; tap < metallicDecimatorTapCount_; ++tap)
    {
        const int offset = tap - halfLength;
        const double sinc = offset == 0
            ? 2.0 * cutoff
            : std::sin (2.0 * static_cast<double> (pi) * cutoff
                        * static_cast<double> (offset))
                / (static_cast<double> (pi) * static_cast<double> (offset));
        const double ratio = static_cast<double> (offset)
            / static_cast<double> (halfLength);
        const double window = besselI0 (
            beta * std::sqrt (std::max (0.0, 1.0 - ratio * ratio)))
            * inverseWindowDenominator;
        const float coefficient = static_cast<float> (sinc * window);
        metallicDecimatorCoefficients_[static_cast<std::size_t> (tap)] = coefficient;
        coefficientSum += coefficient;
    }

    const float inverseSum = static_cast<float> (1.0 / coefficientSum);
    for (int tap = 0; tap < metallicDecimatorTapCount_; ++tap)
        metallicDecimatorCoefficients_[static_cast<std::size_t> (tap)] *= inverseSum;
}

void DrumEngine::resetMetallicOscillatorBanks() noexcept
{
    static constexpr std::array<Instrument, metallicBankCount> instruments {
        Instrument::ClosedHat, Instrument::OpenHat, Instrument::Ride,
        Instrument::Crash, Instrument::Perc1
    };

    for (std::size_t bankIndex = 0; bankIndex < metallicBanks_.size(); ++bankIndex)
    {
        auto& bank = metallicBanks_[bankIndex];
        bank = RelaxationOscillatorBank {};
        bank.instrument = instruments[bankIndex];

        for (std::size_t oscillatorIndex = 0;
             oscillatorIndex < bank.phases.size(); ++oscillatorIndex)
        {
            const auto seed = static_cast<std::uint32_t> (
                0x9e3779b9u * (bankIndex + 1u)
                + 0x85ebca6bu * (oscillatorIndex + 1u));
            bank.phases[oscillatorIndex] = 0.5f
                + 0.5f * signedUnitFromHash (seed ^ 0x243f6a88u);
            bank.fixedTolerances[oscillatorIndex] = signedUnitFromHash (
                seed ^ 0xb7e15162u);
            bank.dutyCycles[oscillatorIndex] = std::clamp (
                0.4798f + 0.010f * signedUnitFromHash (seed ^ 0x13198a2eu),
                0.445f, 0.515f);
            bank.thresholds[oscillatorIndex] = std::clamp (
                0.50f + 0.035f * signedUnitFromHash (seed ^ 0x03707344u),
                0.40f, 0.60f);
        }

        const auto values = snapshotParameters (bank.instrument);
        bank.lastParameterPitch = values.pitch;
        bank.lastParameterCharacterA = values.characterA;
        configureMetallicOscillatorBank (
            bank.instrument, std::exp2 (values.pitch / 12.0f),
            values.characterA, true);

        // Fill the complete reconstruction history from the running circuit
        // rather than exposing a zero-state filter transient on the first hit.
        for (int substep = 0; substep < metallicDecimatorTapCount_; ++substep)
        {
            bank.decimatorHistory[static_cast<std::size_t> (
                bank.decimatorWriteIndex)] = renderMetallicBankSubstep (bank);
            if (++bank.decimatorWriteIndex >= maximumMetallicDecimatorTaps)
                bank.decimatorWriteIndex = 0;
        }
    }
}

void DrumEngine::wakeMetallicOscillatorBank (RelaxationOscillatorBank& bank) noexcept
{
    // A metallic circuit is only advanced sample by sample while some voice can
    // actually observe it: through a closed VCA its contribution is exactly
    // zero, so the engine freezes it and restores the state it would have
    // reached when the next strike opens the VCA. Short gaps are replayed
    // substep-exactly, longer gaps advance every phase analytically, snap the
    // capacitors onto their settled periodic orbit, and re-render one full
    // reconstruction history. The gap is an absolute sample count and the
    // observability is evaluated per sample, so the result is independent of
    // host block partitioning.
    if (bank.frozenSamples == 0u)
        return;

    const auto substepsToCover = bank.frozenSamples
        * static_cast<std::uint64_t> (metallicOversampleFactor_);
    const auto warmupSubsteps = static_cast<std::uint64_t> (metallicDecimatorTapCount_);
    // Short gaps are replayed substep-exactly, so a briefly idle bank stays
    // sample-identical to one that kept rendering (a tested superposition
    // contract). Beyond this bound the smoothed increments and capacitors have
    // long settled onto their periodic orbit, which the analytic jump restores
    // directly; the bound also caps the wake cost at well under a millisecond.
    constexpr std::uint64_t exactReplaySubsteps = 2048;
    bank.frozenSamples = 0;

    auto replaySubsteps = substepsToCover;
    if (substepsToCover > exactReplaySubsteps)
    {
        const auto advance = substepsToCover - warmupSubsteps;
        for (int oscillatorIndex = 0; oscillatorIndex < bank.activeOscillators;
             ++oscillatorIndex)
        {
            const auto index = static_cast<std::size_t> (oscillatorIndex);
            const double travelled = static_cast<double> (bank.targetIncrements[index])
                * static_cast<double> (advance);
            bank.phases[index] = std::clamp (
                static_cast<float> (bank.phases[index]
                    + travelled - std::floor (bank.phases[index] + travelled)),
                0.0f, 1.0f);
        }
        // Snap increments and capacitor states to the settled values the
        // smoothed circuit converges to within a couple of milliseconds.
        configureMetallicOscillatorBank (
            bank.instrument, std::exp2 (bank.lastParameterPitch / 12.0f),
            bank.lastParameterCharacterA, true);
        replaySubsteps = warmupSubsteps;
    }

    for (std::uint64_t substep = 0; substep < replaySubsteps; ++substep)
    {
        bank.decimatorHistory[static_cast<std::size_t> (
            bank.decimatorWriteIndex)] = renderMetallicBankSubstep (bank);
        if (++bank.decimatorWriteIndex >= maximumMetallicDecimatorTaps)
            bank.decimatorWriteIndex = 0;
    }
}

void DrumEngine::wakeMetallicOscillatorBankFor (Instrument instrument) noexcept
{
    const int bankIndex = metallicBankIndexFor (instrument);
    if (bankIndex >= 0)
        wakeMetallicOscillatorBank (metallicBanks_[static_cast<std::size_t> (bankIndex)]);
}

void DrumEngine::addBankReference (Instrument instrument) noexcept
{
    const int bankIndex = metallicBankIndexFor (instrument);
    if (bankIndex < 0)
        return;
    const auto index = static_cast<std::size_t> (bankIndex);
    ++metallicBankVoiceCounts_[index];
    metallicBankMask_ |= std::uint32_t { 1 } << static_cast<unsigned> (bankIndex);
}

void DrumEngine::releaseBankReference (Instrument instrument) noexcept
{
    const int bankIndex = metallicBankIndexFor (instrument);
    if (bankIndex < 0)
        return;
    const auto index = static_cast<std::size_t> (bankIndex);
    metallicBankVoiceCounts_[index] = std::max (0, metallicBankVoiceCounts_[index] - 1);
    if (metallicBankVoiceCounts_[index] == 0)
        metallicBankMask_ &= ~(std::uint32_t { 1 } << static_cast<unsigned> (bankIndex));
}

void DrumEngine::updateMetallicBankParameterTargets() noexcept
{
    for (auto& bank : metallicBanks_)
    {
        const auto values = snapshotParameters (bank.instrument);
        if (values.pitch == bank.lastParameterPitch
            && values.characterA == bank.lastParameterCharacterA)
            continue;

        configureMetallicOscillatorBank (
            bank.instrument, std::exp2 (values.pitch / 12.0f),
            values.characterA, false);
        bank.lastParameterPitch = values.pitch;
        bank.lastParameterCharacterA = values.characterA;
    }
}

void DrumEngine::configureMetallicOscillatorBank (Instrument instrument,
                                                   float pitchRatio,
                                                   float characterA,
                                                   bool snap) noexcept
{
    const int bankIndex = metallicBankIndexFor (instrument);
    if (bankIndex < 0)
        return;

    auto& bank = metallicBanks_[static_cast<std::size_t> (bankIndex)];
    bank.characterA = std::clamp (characterA, 0.0f, 1.0f);
    pitchRatio = std::clamp (pitchRatio, 0.20f, 4.20f);

    static constexpr std::array<float, metallicOscillatorCount> hatRatios {
        1.0f, 1.342f, 1.778f, 2.133f, 2.697f, 3.415f
    };
    // Measured nominal HD14584 oscillator frequencies from the classic
    // six-inverter cymbal source. The 800/540 Hz pair also anchors Perc 1.
    static constexpr std::array<float, metallicOscillatorCount> cymbalFrequencies {
        205.3f, 369.6f, 304.4f, 522.7f, 800.0f, 540.0f
    };

    bank.activeOscillators = instrument == Instrument::Perc1
        ? 2 : metallicOscillatorCount;
    // Only the hi-hat and Perc 1 mixes read the RC ramps. The ride and crash
    // banks sum their Schmitt pulses alone, so integrating their capacitors
    // every substep was pure dead work in one of the engine's hottest loops.
    bank.usesCapacitors = instrument != Instrument::Ride
        && instrument != Instrument::Crash;
    for (int oscillatorIndex = 0; oscillatorIndex < bank.activeOscillators;
         ++oscillatorIndex)
    {
        const auto index = static_cast<std::size_t> (oscillatorIndex);
        float frequency = 0.0f;
        float toleranceDepth = 0.004f;
        if (instrument == Instrument::ClosedHat || instrument == Instrument::OpenHat)
        {
            const float alternating = (oscillatorIndex & 1) == 0 ? -1.0f : 1.0f;
            frequency = 1550.0f * hatRatios[index]
                * (1.0f + alternating * 0.025f * bank.characterA);
            toleranceDepth = 0.006f;
        }
        else if (instrument == Instrument::Ride || instrument == Instrument::Crash)
        {
            frequency = cymbalFrequencies[index]
                * (instrument == Instrument::Crash ? 0.94f : 1.0f);
            toleranceDepth = oscillatorIndex < 4
                ? 0.004f + (instrument == Instrument::Crash
                                ? 0.004f * bank.characterA : 0.0f)
                : 0.018f + (instrument == Instrument::Crash
                                ? 0.012f * bank.characterA : 0.002f);
        }
        else
        {
            frequency = oscillatorIndex == 0
                ? 535.0f
                : 535.0f * (1.34f + 0.42f * bank.characterA);
            toleranceDepth = 0.008f;
        }

        frequency *= pitchRatio
            * (1.0f + toleranceDepth * bank.fixedTolerances[index]);
        const float increment = std::clamp (
            frequency * metallicInverseSampleRate_, 1.0e-7f, 0.45f);
        bank.targetIncrements[index] = increment;
        if (snap || bank.currentIncrements[index] <= 0.0f)
            bank.currentIncrements[index] = increment;

        const float threshold = bank.thresholds[index];
        const float logarithmicSwing = std::log (
            (1.0f + threshold) / std::max (0.05f, 1.0f - threshold));
        const float duty = bank.dutyCycles[index];
        bank.riseCoefficients[index] = 1.0f - std::exp (
            -logarithmicSwing * increment / std::max (0.10f, duty));
        bank.fallCoefficients[index] = 1.0f - std::exp (
            -logarithmicSwing * increment / std::max (0.10f, 1.0f - duty));

        if (snap)
        {
            const float phase = bank.phases[index];
            if (phase < duty)
            {
                const float normalized = phase / std::max (0.10f, duty);
                bank.capacitorStates[index] = 1.0f
                    - (1.0f + threshold)
                        * std::exp (-logarithmicSwing * normalized);
            }
            else
            {
                const float normalized = (phase - duty)
                    / std::max (0.10f, 1.0f - duty);
                bank.capacitorStates[index] = -1.0f
                    + (1.0f + threshold)
                        * std::exp (-logarithmicSwing * normalized);
            }
        }
    }
}

float DrumEngine::renderMetallicBankSubstep (
    RelaxationOscillatorBank& bank) noexcept
{
    std::array<float, metallicOscillatorCount> pulses {};
    std::array<float, metallicOscillatorCount> capacitors {};
    for (int oscillatorIndex = 0; oscillatorIndex < bank.activeOscillators;
         ++oscillatorIndex)
    {
        const auto index = static_cast<std::size_t> (oscillatorIndex);
        bank.currentIncrements[index] += metallicIncrementSmoothing_
            * (bank.targetIncrements[index] - bank.currentIncrements[index]);
        const float increment = std::clamp (
            bank.currentIncrements[index], 1.0e-7f, 0.45f);
        float phase = bank.phases[index] + increment;
        phase -= std::floor (phase);
        bank.phases[index] = phase;

        const float duty = bank.dutyCycles[index];
        float pulse = phase < duty ? 1.0f : -1.0f;
        pulse += polyBlep (phase, increment);
        float fallingPhase = phase - duty;
        if (fallingPhase < 0.0f)
            fallingPhase += 1.0f;
        pulse -= polyBlep (fallingPhase, increment);
        // Remove the exact duty-cycle DC term before the downstream channel
        // filters. Component mismatch still changes harmonic balance without
        // making overlapping cymbals pull on the master DC blocker.
        pulses[index] = pulse - (2.0f * duty - 1.0f);

        if (bank.usesCapacitors)
        {
            const float target = phase < duty ? 1.0f : -1.0f;
            const float coefficient = phase < duty
                ? bank.riseCoefficients[index] : bank.fallCoefficients[index];
            bank.capacitorStates[index] += coefficient
                * (target - bank.capacitorStates[index]);
            capacitors[index] = bank.capacitorStates[index]
                / std::max (0.25f, bank.thresholds[index]);
        }
    }

    if (bank.instrument == Instrument::ClosedHat
        || bank.instrument == Instrument::OpenHat)
    {
        const float pulseRings = pulses[0] * pulses[1]
                               + pulses[2] * pulses[3]
                               + pulses[4] * pulses[5];
        const float capacitorRings = capacitors[0] * capacitors[1]
                                   + capacitors[2] * capacitors[3]
                                   + capacitors[4] * capacitors[5];
        const float tones = pulses[0] + pulses[2] + pulses[4];
        return (0.18f + 0.12f * bank.characterA)
                   * (0.82f * pulseRings + 0.18f * capacitorRings)
             + 0.07f * bank.characterA * tones;
    }

    if (bank.instrument == Instrument::Perc1)
    {
        const float first = pulses[0] + 0.16f * capacitors[0];
        const float second = pulses[1] + 0.16f * capacitors[1];
        return 0.42f * first + 0.34f * second + 0.14f * first * second;
    }

    float sum = 0.0f;
    for (int oscillatorIndex = 0; oscillatorIndex < bank.activeOscillators;
         ++oscillatorIndex)
        sum += pulses[static_cast<std::size_t> (oscillatorIndex)];
    return sum / static_cast<float> (bank.activeOscillators);
}

float DrumEngine::decimateMetallicBank (
    const RelaxationOscillatorBank& bank) const noexcept
{
    float reconstructed = 0.0f;
    int recentIndex = bank.decimatorWriteIndex - 1;
    if (recentIndex < 0)
        recentIndex += maximumMetallicDecimatorTaps;
    int oldestIndex = bank.decimatorWriteIndex - metallicDecimatorTapCount_;
    if (oldestIndex < 0)
        oldestIndex += maximumMetallicDecimatorTaps;

    // The linear-phase Kaiser kernel is symmetric. Pair equidistant history
    // samples before multiplying to halve the active-bank reconstruction cost
    // without changing its response or persistent state.
    const int centreTap = metallicDecimatorTapCount_ / 2;
    for (int tap = 0; tap < centreTap; ++tap)
    {
        reconstructed += metallicDecimatorCoefficients_[static_cast<std::size_t> (tap)]
            * (bank.decimatorHistory[static_cast<std::size_t> (recentIndex)]
               + bank.decimatorHistory[static_cast<std::size_t> (oldestIndex)]);
        if (--recentIndex < 0)
            recentIndex += maximumMetallicDecimatorTaps;
        if (++oldestIndex >= maximumMetallicDecimatorTaps)
            oldestIndex = 0;
    }
    reconstructed += metallicDecimatorCoefficients_[static_cast<std::size_t> (centreTap)]
        * bank.decimatorHistory[static_cast<std::size_t> (recentIndex)];
    return reconstructed;
}

void DrumEngine::renderMetallicOscillatorBanks (
    std::uint32_t activeBankMask) noexcept
{
    for (std::size_t bankIndex = 0; bankIndex < metallicBanks_.size(); ++bankIndex)
    {
        auto& bank = metallicBanks_[bankIndex];
        const bool isActive = (activeBankMask
            & (std::uint32_t { 1 } << static_cast<unsigned> (bankIndex))) != 0u;
        if (! isActive)
        {
            // Nothing can hear this circuit: its oscillators, capacitors and
            // reconstruction history are frozen and restored analytically by
            // wakeMetallicOscillatorBank() when the next strike opens a VCA.
            // Skipping the substeps and the 129-multiply symmetric convolution
            // is where most of the engine's idle cost used to go.
            ++bank.frozenSamples;
            bank.output = 0.0f;
            continue;
        }

        float latestSource = 0.0f;
        for (int substep = 0; substep < metallicOversampleFactor_; ++substep)
        {
            latestSource = renderMetallicBankSubstep (bank);
            bank.decimatorHistory[static_cast<std::size_t> (
                bank.decimatorWriteIndex)] = latestSource;
            if (++bank.decimatorWriteIndex >= maximumMetallicDecimatorTaps)
                bank.decimatorWriteIndex = 0;
        }

        bank.output = metallicOversampleFactor_ > 1
            ? decimateMetallicBank (bank) : latestSource;
    }
}

float DrumEngine::metallicSourceFor (Instrument instrument) const noexcept
{
    const int bankIndex = metallicBankIndexFor (instrument);
    return bankIndex >= 0
        ? metallicBanks_[static_cast<std::size_t> (bankIndex)].output : 0.0f;
}

float DrumEngine::nextCymbalPcm (Voice& voice, float source) const noexcept
{
    // The TR-909 cymbals replayed compressed PCM at roughly 30 kHz. Drumalor
    // remains fully synthesized, but this voice-local clock and 63-level
    // quantizer contribute the same held-DAC grain to a generated oscillator/
    // noise composite. No sample or copyrighted ROM data is embedded.
    voice.cymbalClockPhase += cymbalClockIncrement_;
    if (voice.cymbalClockPhase >= 1.0f)
    {
        voice.cymbalClockPhase -= std::floor (voice.cymbalClockPhase);
        const float decorrelation = 0.18f * nextNoise (voice);
        const float composite = std::clamp (source + decorrelation, -1.0f, 1.0f);
        voice.cymbalPcmValue = std::round (31.0f * composite) * (1.0f / 31.0f);
    }
    // A real reconstruction network does not expose the held DAC steps
    // directly. This exact one-pole update removes their broadband digital
    // edge while retaining the audible 30 kHz clock grain at ordinary rates.
    voice.cymbalPcmReconstructed = flushDenormal (
        voice.cymbalPcmReconstructed + cymbalReconstructionCoefficient_
            * (voice.cymbalPcmValue - voice.cymbalPcmReconstructed));
    return voice.cymbalPcmReconstructed;
}

void DrumEngine::configureHighpass (Biquad& filter, float frequency, float q) const noexcept
{
    frequency = std::clamp (frequency, 10.0f, 0.45f * static_cast<float> (sampleRate_));
    q = std::clamp (q, 0.15f, 20.0f);
    const float omega = twoPi * frequency * inverseSampleRate_;
    const float cosine = std::cos (omega);
    const float alpha = std::sin (omega) / (2.0f * q);
    const float inverseA0 = 1.0f / (1.0f + alpha);
    filter.b0 = 0.5f * (1.0f + cosine) * inverseA0;
    filter.b1 = -(1.0f + cosine) * inverseA0;
    filter.b2 = filter.b0;
    filter.a1 = -2.0f * cosine * inverseA0;
    filter.a2 = (1.0f - alpha) * inverseA0;
    filter.clear();
}

void DrumEngine::configureBandpass (Biquad& filter, float frequency, float q) const noexcept
{
    frequency = std::clamp (frequency, 10.0f, 0.45f * static_cast<float> (sampleRate_));
    q = std::clamp (q, 0.15f, 20.0f);
    const float omega = twoPi * frequency * inverseSampleRate_;
    const float cosine = std::cos (omega);
    const float alpha = std::sin (omega) / (2.0f * q);
    const float inverseA0 = 1.0f / (1.0f + alpha);
    filter.b0 = alpha * inverseA0;
    filter.b1 = 0.0f;
    filter.b2 = -filter.b0;
    filter.a1 = -2.0f * cosine * inverseA0;
    filter.a2 = (1.0f - alpha) * inverseA0;
    filter.clear();
}

void DrumEngine::configureResonator (Resonator& resonator, float frequency,
                                     float decaySeconds) const noexcept
{
    frequency = std::clamp (frequency, 20.0f, 0.45f * static_cast<float> (sampleRate_));
    decaySeconds = std::max (0.005f, decaySeconds);
    const float omega = twoPi * frequency * inverseSampleRate_;
    const float radius = coefficientForTime (decaySeconds, static_cast<float> (sampleRate_));
    resonator.a1 = 2.0f * radius * std::cos (omega);
    resonator.a2 = -radius * radius;

    // A two-pole resonator's impulse residue is inputGain / sin (omega).
    // Preserve the existing 48 kHz residue while keeping that ratio constant
    // at every sample rate.
    const float referenceFrequency = std::min (frequency, 0.45f * referenceSampleRate);
    const float referenceOmega = twoPi * referenceFrequency / referenceSampleRate;
    const float referenceRadius = coefficientForTime (decaySeconds, referenceSampleRate);
    const float referenceGain = 0.45f * std::sqrt (
        std::max (1.0e-8f, 1.0f - referenceRadius * referenceRadius));
    resonator.inputGain = referenceGain * std::sin (omega)
        / std::max (1.0e-4f, std::sin (referenceOmega));
    // A strike sets the mode moving instead of pushing a sample through it, so
    // its scale is the mode's own geometry and carries no sample rate with it.
    resonator.strikeGain = std::sin (omega) / std::max (1.0e-4f, radius);
    resonator.clear();
}

int DrumEngine::findVoiceSlot() const noexcept
{
    for (int index = 0; index < maxVoices; ++index)
        if (! voices_[static_cast<std::size_t> (index)].active)
            return index;

    int candidate = 0;
    float quietest = std::numeric_limits<float>::max();
    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
    for (int index = 0; index < maxVoices; ++index)
    {
        const auto& voice = voices_[static_cast<std::size_t> (index)];
        const float level = voice.recentPeak * voice.chokeGain;
        if (level < quietest || (level == quietest && voice.generation < oldest))
        {
            candidate = index;
            quietest = level;
            oldest = voice.generation;
        }
    }
    return candidate;
}

void DrumEngine::silenceVoice (Voice& voice) noexcept
{
    if (voice.active)
        releaseBankReference (voice.instrument);
    voice = Voice {};
}

void DrumEngine::beginChoke (Voice& voice, float seconds) noexcept
{
    if (! voice.active)
        return;
    beginFadeToSilence (
        voice, coefficientForTime (std::max (0.0005f, seconds),
                                   static_cast<float> (sampleRate_)));
}

void DrumEngine::beginFadeToSilence (Voice& voice, float multiplier) noexcept
{
    if (! voice.active)
        return;
    voice.choking = true;
    voice.chokeMultiplier = std::min (
        voice.chokeMultiplier, std::clamp (multiplier, 0.0f, 1.0f));
}

void DrumEngine::retireVoice (const Voice& source) noexcept
{
    if (! source.active)
        return;

    Voice* destination = nullptr;
    float quietest = std::numeric_limits<float>::max();
    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
    for (auto& voice : retiringVoices_)
    {
        if (! voice.active)
        {
            destination = &voice;
            break;
        }

        const float level = voice.recentPeak * voice.chokeGain;
        if (level < quietest || (level == quietest && voice.generation < oldest))
        {
            destination = &voice;
            quietest = level;
            oldest = voice.generation;
        }
    }

    // The slot being reused may itself still be sounding; drop its bank
    // reference before it is overwritten by the retired copy.
    if (destination->active)
        releaseBankReference (destination->instrument);
    *destination = source;
    addBankReference (source.instrument);
    beginFadeToSilence (*destination, retirementFadeMultiplier_);
}

void DrumEngine::chokeGroup (int group) noexcept
{
    if (group <= 0)
        return;
    // Every voice remembers the group it was born into, so retuning the
    // parameter never strands a ringing tail that can no longer be cut.
    for (auto& voice : voices_)
        if (voice.active && voice.chokeGroup == group)
            beginChoke (voice, 0.003f);
    for (auto& voice : retiringVoices_)
        if (voice.active && voice.chokeGroup == group)
            beginChoke (voice, 0.003f);
}

void DrumEngine::allSoundsOff() noexcept
{
    for (auto& voice : voices_)
        if (voice.active)
            beginChoke (voice, 0.004f);
    for (auto& voice : retiringVoices_)
        if (voice.active)
            beginChoke (voice, 0.004f);
}

void DrumEngine::updateActiveVoiceCount() noexcept
{
    // One pass over both pools publishes the host-facing voice count and the
    // per-instrument meter levels the editor's channel strip reads. The per
    // voice recentPeak already carries 30 ms meter ballistics, so nothing has
    // to be tracked inside the per-sample loop.
    int count = 0;
    std::array<float, instrumentCount> levels {};
    const auto observe = [&count, &levels] (const Voice& voice)
    {
        if (! voice.active)
            return;
        ++count;
        auto& level = levels[indexFor (voice.instrument)];
        level = std::max (level, voice.recentPeak * voice.chokeGain);
    };
    for (const auto& voice : voices_)
        observe (voice);
    for (const auto& voice : retiringVoices_)
        observe (voice);
    for (std::size_t index = 0; index < instrumentCount; ++index)
        instrumentLevels_[index].store (levels[index], std::memory_order_relaxed);
    activeVoiceCount_.store (count, std::memory_order_relaxed);
}

int DrumEngine::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount_.load (std::memory_order_relaxed);
}

int DrumEngine::buildHeadBank (Voice& voice, float fundamental,
                               const HeadGeometry& head, float decaySeconds,
                               float brightness, ModalLoss loss) noexcept
{
    // What removes energy from one mode of a real head, per second.
    //
    // The multipole order is the whole story of which modes are loud and which
    // ones last, and on a drum those are opposite questions. Two heads breathing
    // together push air out in every direction at once - a monopole, the most
    // efficient radiator there is - so that mode is the loudest thing the drum
    // does and it is over almost before it starts. One head going out while the
    // other comes in moves air from one side to the other and radiates as a
    // dipole, which at these sizes is barely at all: quiet, and it rings for a
    // second. Every circumferential order above that is worse again.
    //
    // A model that damps every mode alike gets this exactly backwards, and it is
    // what makes a synthesised drum sound like one tone with an envelope.
    const auto lossPerSecond = [&head, &loss] (float frequency, int multipole)
    {
        const float omega = twoPi * std::max (1.0f, frequency);
        const float ka = omega * head.radius / soundSpeed;
        const float exponent = 2.0f + 2.0f * static_cast<float> (multipole);
        const float power = std::pow (std::max (1.0e-4f, ka), exponent);
        const float efficiency = power / (1.0f + power);
        return loss.fixed + loss.hysteretic * omega
             + loss.viscous * omega * omega + loss.radiation * efficiency;
    };

    // The Decay control still means the drum's own note, so everything else is
    // measured against the mode that carries it: the heads moving oppositely at
    // the fundamental, which is what the body oscillator is.
    const float referenceLoss = std::max (
        1.0e-3f, lossPerSecond (fundamental, 1));

    float ratios[resonatorCount] {};
    float excitation[resonatorCount] {};
    float decays[resonatorCount] {};
    int count = 0;

    // Air loading is added mass, and a mode that ripples finely across the head
    // drags less of it than the one that moves the whole head at once. So the
    // load is heaviest on the fundamental and lightest at the top, which pushes
    // the series apart. Raising the ratios to a power below one, as this used
    // to, does the opposite - it pulls a head's overtones down toward its note
    // and makes every drum in the kit more harmonic than a drum is.
    //
    // How much air there is to drag is the drum's own business: a wide head of
    // thin film carries a lot of it, a small tight one very little, which is
    // why a floor tom is noticeably less harmonic than a rack tom of the same
    // make.
    const float airLoad = 0.70f * head.airLoadScale * airDensity * head.radius
        / std::max (0.05f, head.headDensity);
    const auto loaded = [airLoad] (std::size_t mode)
    {
        const float relative = membraneModeZeros[mode] / membraneModeZeros[0];
        const float added = airLoad / (1.0f + 0.85f * (relative - 1.0f)
                                       + 0.55f * static_cast<float> (membraneModeOrders[mode]));
        return membraneModeRatios[mode]
            * std::sqrt ((1.0f + airLoad) / (1.0f + added));
    };

    for (std::size_t mode = 0;
         mode < membraneModeRatios.size() && count < resonatorCount; ++mode)
    {
        const float ideal = loaded (mode);
        const int order = membraneModeOrders[mode];
        const float lambda = membraneModeZeros[mode];

        // Where the strike lands decides which modes it can reach at all. Mode
        // (m,n) has the shape J_m(lambda r / a) across the head, and J_m(0) is
        // zero for every m above zero, so a head struck on its exact centre
        // rings its axisymmetric family alone - thin, hollow, and not what
        // anybody plays. Moving the beater or the stick a fifth of the way out
        // brings the rest of the head in.
        const float shape = besselJ (order, lambda * head.strikeRadius);

        // What a strike of this length can put into a mode of this pitch, and
        // what a microphone a hand's width from the head makes of the answer.
        // The contact is the only low-pass in the attack, and it is a real one.
        //
        // A close microphone sits inside the near field, where it hears the head
        // move rather than the room fill, so it is not deaf to the modes that
        // radiate badly the way a listener across a hall would be. It is only
        // partly deaf to them, which is what the fixed share is.
        const auto emit = [&] (float ratio, int multipole, float weight)
        {
            const float frequency = fundamental * ratio;
            const float modeLoss = std::max (1.0e-3f,
                                             lossPerSecond (frequency, multipole));
            const float omega = twoPi * std::max (1.0f, frequency);
            const float ka = omega * head.radius / soundSpeed;
            const float exponent = 2.0f + 2.0f * static_cast<float> (multipole);
            const float power = std::pow (std::max (1.0e-4f, ka), exponent);
            const float heard = 0.34f + 0.66f * std::sqrt (power / (1.0f + power));

            ratios[count] = ratio;
            decays[count] = decaySeconds * referenceLoss / modeLoss;
            excitation[count] = shape * weight * heard
                * contactSpectrum (frequency, head.contactSeconds);
            ++count;
        };

        if (order != 0)
        {
            emit (ideal, order, 1.0f);
            continue;
        }

        // The air between the two heads is a spring across both of them, and it
        // splits every mode that changes the enclosed volume into a pair. The
        // heads moving oppositely leave the volume alone, so that branch sits
        // exactly where an unloaded head would and radiates as a dipole: quiet
        // for the energy it holds, and long. The heads moving together squeeze
        // the air, so that branch is stiffened up to wherever the air spring
        // puts it and radiates as a monopole: loud, and gone quickly.
        //
        // Those two are a bass drum's weight and its punch. A model with one
        // resonator has neither - it has an average of them, which is a tone.
        //
        // The lowest mode's long branch is deliberately absent: the voice's own
        // body oscillator is that branch, tuned and swept and shaped by the
        // nonlinearity, and putting it here as well would double it.
        const float stiffness = cavityStiffness (
            fundamental * ideal, lambda, head.headDensity, head.shellDepth);

        // The volume-preserving branch, if this is not the mode the body
        // oscillator already is.
        if (mode > 0)
        {
            emit (ideal, 1, 0.62f);
            if (count >= resonatorCount)
                break;
        }

        emit (ideal * std::sqrt (1.0f + 2.0f * stiffness), 0, 1.0f);
    }

    // Level and spectral tilt, then the resonators themselves.
    voice.modeCount = count;
    const float tilt = -1.30f + 1.05f * brightness;
    float gainSum = 0.0f;
    for (int mode = 0; mode < count; ++mode)
    {
        const auto slot = static_cast<std::size_t> (mode);
        const float gain = std::pow (std::max (1.0f, ratios[slot]), tilt)
            * std::abs (excitation[slot]);
        voice.modeGains[slot] = gain;
        gainSum += gain;
        configureResonator (voice.resonators[slot], fundamental * ratios[slot],
                            decays[slot]);
    }

    const float scale = gainSum > 0.0f ? struckHeadScale / gainSum : 0.0f;
    float longest = 0.0f;
    for (int mode = 0; mode < count; ++mode)
    {
        voice.modeGains[static_cast<std::size_t> (mode)] *= scale;
        longest = std::max (longest, decays[static_cast<std::size_t> (mode)]);
    }

    // Every mode is under -150 dB after 2.6 of its own decays, so the bank can
    // stop being evaluated once the slowest of them has passed that.
    voice.modalActiveSamples = static_cast<std::uint64_t> (
        std::ceil (2.6 * static_cast<double> (longest) * sampleRate_)) + 1u;
    return count;
}

void DrumEngine::initialiseModalVoice (Voice& voice, const float* ratios, int modeCount,
                                       float baseFrequency, float decaySeconds,
                                       float spread, float brightness,
                                       ModalLoss loss,
                                       const float* excitation) noexcept
{
    modeCount = std::clamp (modeCount, 0, resonatorCount);
    voice.modeCount = modeCount;
    float gainSum = 0.0f;
    for (int mode = 0; mode < modeCount; ++mode)
    {
        const auto hash = hash32 (voice.noiseState + static_cast<std::uint32_t> (mode * 0x9e37));
        const float random = static_cast<float> (hash & 0xffffu) / 32767.5f - 1.0f;
        const float ratio = ratios[mode] * (1.0f + 0.035f * spread * random);
        // Damping is a property of the frequency, not of the mode's position in
        // an array. The old taper ran on the index, so a bank's twelfth mode
        // decayed 1.25x faster than its first whether that mode sat one octave
        // up or four - which made every struck head ring like a bell, all of its
        // modes dying together instead of the top of the head going first.
        const float relative = std::max (1.0f, ratio);
        const float lossFactor = std::max (
            0.05f, loss.fixed + loss.hysteretic * relative
                       + loss.viscous * relative * relative);
        const float modeDecay = decaySeconds / lossFactor;
        configureResonator (voice.resonators[static_cast<std::size_t> (mode)],
                            baseFrequency * ratio, modeDecay);
        const float tilt = -1.30f + 1.05f * brightness;
        const float gain = std::pow (std::max (1.0f, ratio), tilt)
            * (excitation != nullptr
                   ? std::abs (excitation[static_cast<std::size_t> (mode)])
                   : 1.0f);
        voice.modeGains[static_cast<std::size_t> (mode)] = gain;
        gainSum += gain;
    }
    const float scale = gainSum > 0.0f ? 1.35f / gainSum : 1.0f;
    for (int mode = 0; mode < modeCount; ++mode)
        voice.modeGains[static_cast<std::size_t> (mode)] *= scale;

    // Every configured mode reaches -60 dB within decaySeconds, so after 2.6
    // times that the whole bank is below -150 dB: far under the -100 dB level
    // at which a voice is already considered silent. Recording the boundary
    // lets long cymbal and tom tails stop paying for a resonator bank that can
    // no longer contribute, without changing a single audible sample.
    voice.modalActiveSamples = static_cast<std::uint64_t> (std::ceil (
        2.6 * static_cast<double> (std::max (0.0f, decaySeconds)) * sampleRate_)) + 1u;
}

void DrumEngine::initialiseVoice (Voice& voice, Instrument instrument, float velocity,
                                  const InstrumentParameters& values, std::uint32_t seed,
                                  const HitVariation& variation) noexcept
{
    voice = Voice {};
    voice.active = true;
    voice.instrument = instrument;
    voice.chokeGroup = std::clamp (values.chokeGroup, 0, chokeGroupCount);
    voice.generation = ++generation_;
    voice.noiseState = seed == 0u ? 1u : seed;
    voice.velocity = velocity * (0.68f + 0.32f * std::sqrt (velocity));
    voice.recentPeak = voice.velocity;
    voice.characterA = std::clamp (values.characterA + variation.characterAOffset,
                                   0.0f, 1.0f);
    voice.characterB = std::clamp (values.characterB + variation.characterBOffset,
                                   0.0f, 1.0f);
    voice.pitchRatio = std::exp2 ((values.pitch + 0.01f * variation.pitchCents) / 12.0f);
    voice.decaySeconds = decaySecondsFor (instrument, values.decay)
        * variation.decayScale;
    voice.transientScale = variation.transientScale;
    // Treat MIDI velocity as trigger/accent voltage as well as final VCA
    // loudness. The deliberately narrow range preserves the established gain
    // curve while making hard hits inject more energy into the physical core.
    voice.excitationScale = 0.74f + 0.26f * std::sqrt (velocity);
    // A softly struck head, cymbal or shaker radiates a darker spectrum than a
    // hard strike, because less energy reaches the high, heavily damped modes.
    // The curve is unity at full velocity, so the loud end of the existing
    // voice design is preserved and only quiet hits gain the extra realism.
    voice.velocityTimbre = 0.62f + 0.38f * velocity;
    voice.levelGain = decibelsToGain (values.level);
    voice.circuitDrive = std::clamp (
        1.10f + 0.48f * voice.characterB + variation.circuitDriveOffset,
        1.02f, 1.72f);
    voice.circuitBias = variation.circuitBias;
    voice.envelopeMultiplier = coefficientForTime (voice.decaySeconds,
                                                     static_cast<float> (sampleRate_));
    voice.auxiliaryMultiplier = coefficientForTime (voice.decaySeconds * 0.85f,
                                                     static_cast<float> (sampleRate_));
    voice.transientMultiplier = coefficientForTime (0.008f, static_cast<float> (sampleRate_));
    voice.pitchEnvelopeMultiplier = coefficientForTime (0.030f, static_cast<float> (sampleRate_));
    // Quiet voices retire from their measured output level. This is the hard
    // host-facing ceiling; a forced fade begins shortly before it.
    voice.maximumSamples = maximumVoiceSamples_;

    // Placement is now an automatable per-voice control whose defaults are the
    // former hard-coded kit positions, so an untouched kit images identically.
    const float pan = std::clamp (values.pan, -1.0f, 1.0f);
    voice.panLeft = constantPowerLeft (pan);
    voice.panRight = constantPowerRight (pan);

    // A triggered analogue oscillator rarely begins at precisely the same
    // capacitor voltage. Keep the displacement small for punch consistency,
    // but seed it per hit so even tonal voices do not become static samples.
    for (std::size_t oscillatorIndex = 0; oscillatorIndex < voice.phases.size(); ++oscillatorIndex)
    {
        const float oscillatorOffset = signedUnitFromHash (
            seed ^ static_cast<std::uint32_t> (0x4f1bbcdcu + oscillatorIndex * 0x9e3779b9u));
        voice.phases[oscillatorIndex] = variation.phaseOffset
            * (0.72f + 0.28f * oscillatorOffset);
        const auto fixedSeed = static_cast<std::uint32_t> (
            (indexFor (instrument) + 1u) * 0x9e3779b9u
            + (oscillatorIndex + 1u) * 0x85ebca6bu);
        voice.oscillatorAsymmetries[oscillatorIndex] = std::clamp (
            0.82f * signedUnitFromHash (fixedSeed ^ 0x3c6ef372u)
                + 0.18f * oscillatorOffset,
            -1.0f, 1.0f);
    }

    // Short, increasingly dense acoustic modes give the synthetic 909 layer
    // body without allowing a handful of low partials to cling for the tail.
    static constexpr float rideRatios[12] {
        1.0f, 1.431f, 2.097f, 3.042f, 4.181f, 5.528f,
        6.958f, 8.694f, 10.736f, 12.944f, 15.347f, 17.778f
    };
    static constexpr float crashRatios[12] {
        1.0f, 1.468f, 2.129f, 3.032f, 4.161f, 5.548f,
        7.177f, 9.032f, 11.129f, 13.387f, 15.968f, 18.871f
    };

    switch (instrument)
    {
        case Instrument::Kick:
        {
            // A charged energy reservoir excites a stable rotating two-state
            // resonator below. Its settled default is 48 Hz, leaving genuine
            // sub-100 Hz weight while retaining the full pitch range. That
            // resonator is the batter and front heads moving oppositely: the
            // branch the trapped air does not stiffen, the one that radiates
            // badly and therefore lasts. It is where a bass drum keeps its
            // weight, and the bank below is everything else the beater reaches.
            voice.baseFrequency = 48.0f * voice.pitchRatio;
            voice.sweepAmount = 0.70f + 4.0f * voice.characterA;
            voice.pitchEnvelopeMultiplier = coefficientForTime (0.016f + 0.050f * voice.characterA,
                                                                  static_cast<float> (sampleRate_));
            voice.transientMultiplier = coefficientForTime (0.0020f + 0.0045f * voice.characterA,
                                                             static_cast<float> (sampleRate_));
            voice.kickCharge = (0.92f + 0.16f * voice.characterA)
                * voice.transientScale * voice.excitationScale;

            // Punch is the beater. A hard felt or wooden ball stays in contact
            // with the head for less time than a soft one, and Hertz makes that
            // time fall with impact speed too - as the fifth root, a weak law,
            // but it is the whole reason a hard hit is not simply a loud copy of
            // a soft one. Contact time sets how much of the head the strike can
            // reach, so everything about the attack follows from this number
            // rather than from a filter placed by ear.
            voice.contactSeconds = (0.00190f - 0.00120f * voice.characterA)
                * std::pow (std::max (0.08f, velocity), -0.2f);
            voice.contactIncrement = std::min (
                1.0f, inverseSampleRate_ / std::max (1.0e-5f, voice.contactSeconds));
            voice.contactPhase = 0.0f;
            voice.kickBaseRadius = coefficientForTime (
                voice.decaySeconds * 1.08f, static_cast<float> (sampleRate_));
            voice.circuitDrive = std::clamp (
                1.25f + 3.2f * voice.characterB + variation.circuitDriveOffset,
                1.15f, 4.65f);
            // Drive also moves the nonlinear operating point and the mismatch
            // between the virtual diode/transistor branches. This creates the
            // musically useful even harmonics of a biased analogue stage.
            voice.circuitBias += 0.12f * voice.characterB;

            // A 22-inch kick: an 18-inch shell between the heads, a two-ply
            // batter at about 0.45 kg per square metre, and a beater landing a
            // fifth of the way out from the middle, where a pedal puts it.
            HeadGeometry head;
            head.radius = 0.279f;
            head.strikeRadius = 0.18f;
            head.headDensity = 0.45f;
            head.shellDepth = 0.45f;
            head.contactSeconds = voice.contactSeconds;
            // Edge and mounting losses, hysteresis in the plastic, its viscous
            // term, and sound leaving. All in inverse seconds, and only their
            // ratios matter: the Decay control still sets the note's own tail
            // and everything else is measured against it.
            buildHeadBank (voice, voice.baseFrequency, head,
                           voice.decaySeconds * 1.08f,
                           0.30f + 0.28f * velocity,
                           { 8.0f, 0.015f, 1.5e-6f, 220.0f });

            // The contact does not only push the head: felt against plastic is
            // a rough, sliding interface, and it rattles the surface far faster
            // than the pulse itself lasts. That noise is what a listener hears
            // as the beater, and its bandwidth is set by the contact - which is
            // why it brightens when the beater hardens or the foot comes down
            // faster, and why it needs no separate control.
            // Where the head's unresolved region sits. The contact decides how
            // high the strike can reach, and the film's own viscous loss -
            // which climbs as the square of frequency - decides how high
            // anything survives; the lower of the two wins, which is why
            // hardening a beater past a point stops making a drum brighter and
            // only makes it louder. Two sections, because one pole's skirt
            // falls so slowly that the band stops being a band.
            // ...and no band can sit where the sample rate cannot hold it, so
            // the corner also stays clear of Nyquist. Only an 8 kHz host ever
            // notices, and it notices a duller drum rather than a louder one.
            const float kickCorner = std::min (
                { 3000.0f, 2.8f / std::max (0.0002f, voice.contactSeconds),
                  0.30f * static_cast<float> (sampleRate_) });
            configureBandpass (voice.filterA, kickCorner, 0.72f);
            configureBandpass (voice.filterB, kickCorner, 0.72f);
            // Above about four hundred hertz a bass drum head has more modes
            // than can be told apart, and what a microphone hears there is not
            // a series but a band of noise decaying at the rate that region of
            // the head decays at. The old model had a two-millisecond tick
            // standing in for all of it, which is why the drum had a click and
            // no skin.
            voice.auxiliaryMultiplier = coefficientForTime (
                0.020f + 0.10f * voice.decaySeconds, static_cast<float> (sampleRate_));
            voice.auxiliaryEnvelope = 0.0f;
            break;
        }

        case Instrument::Snare:
        {
            voice.baseFrequency = 185.0f * voice.pitchRatio;
            voice.phaseIncrements[0] = std::min (0.45f, voice.baseFrequency * inverseSampleRate_);
            voice.phaseIncrements[1] = std::min (0.45f, voice.baseFrequency * 1.78f * inverseSampleRate_);
            voice.envelopeMultiplier = coefficientForTime (voice.decaySeconds * 0.72f,
                                                            static_cast<float> (sampleRate_));
            // Snap is the stick: a harder tip on a thin, tight head stays down
            // for less time, and Hertz shortens it further as the hit gets
            // harder. A snare's contact is the shortest in the kit, which is
            // most of why it is the brightest thing in it.
            voice.contactSeconds = (0.00085f - 0.00050f * voice.characterB)
                * std::pow (std::max (0.08f, velocity), -0.35f);
            voice.contactIncrement = std::min (
                1.0f, inverseSampleRate_ / std::max (1.0e-5f, voice.contactSeconds));
            voice.contactPhase = 0.0f;

            // Fourteen inches across and five and a half deep. That shallowness
            // is the point: the same trapped air across a much shorter column is
            // a far stiffer spring, so a snare's breathing branch is pushed to
            // well over twice its batter head's note. That branch is the crack -
            // the thing that makes a snare cut through a band - and the model
            // had no way to produce it, because it had no second head.
            HeadGeometry head;
            head.radius = 0.178f;
            head.shellDepth = 0.140f;
            head.headDensity = 0.30f;
            head.strikeRadius = 0.36f;
            head.contactSeconds = voice.contactSeconds;
            head.airLoadScale = 0.75f + 0.50f * voice.characterB;
            // Wires resting on the far head damp everything, and a small shallow
            // drum radiates well for its size, so a snare is the shortest-lived
            // head in the kit at every frequency.
            buildHeadBank (voice, voice.baseFrequency,
                           head, voice.decaySeconds * 0.62f,
                           0.38f + 0.34f * voice.characterB,
                           { 16.0f, 0.030f, 3.0e-6f, 300.0f });
            configureBandpass (voice.filterA,
                               (1250.0f + 4800.0f * voice.characterB)
                                   * std::pow (voice.pitchRatio, 0.30f) * voice.velocityTimbre,
                               0.65f + 0.45f * voice.characterB);
            configureHighpass (voice.filterB, 700.0f + 1700.0f * voice.characterB, 0.7f);
            voice.transientMultiplier = coefficientForTime (0.004f + 0.005f * voice.characterB,
                                                             static_cast<float> (sampleRate_));
            break;
        }

        case Instrument::Clap:
        {
            const auto spacing = static_cast<std::uint64_t> ((0.007f + 0.011f * voice.characterA)
                                                              * static_cast<float> (sampleRate_));
            voice.burstStarts = { 0u, spacing, 2u * spacing + spacing / 5u,
                                  3u * spacing + spacing / 2u };
            voice.minimumSilenceSamples = voice.burstStarts.back() + static_cast<std::uint64_t> (
                std::ceil (0.010f * static_cast<float> (sampleRate_)));
            voice.transientEnvelope = 0.0f;
            voice.transientMultiplier = coefficientForTime (0.0030f + 0.0025f * voice.characterA,
                                                             static_cast<float> (sampleRate_));
            configureBandpass (voice.filterA,
                               (850.0f + 2750.0f * voice.characterB)
                                   * std::pow (voice.pitchRatio, 0.42f) * voice.velocityTimbre,
                               0.68f + 0.42f * voice.characterB);
            configureHighpass (voice.filterB, 430.0f + 1450.0f * voice.characterB, 0.72f);
            break;
        }

        case Instrument::ClosedHat:
        case Instrument::OpenHat:
        {
            configureMetallicOscillatorBank (
                instrument, voice.pitchRatio, voice.characterA, false);
            configureHighpass (voice.filterA, 3400.0f + 6500.0f * voice.characterB, 0.70f);
            configureBandpass (voice.filterB,
                               (6500.0f + 4800.0f * voice.characterB) * voice.velocityTimbre,
                               0.85f);
            voice.transientMultiplier = coefficientForTime (instrument == Instrument::ClosedHat ? 0.0025f : 0.006f,
                                                             static_cast<float> (sampleRate_));
            break;
        }

        case Instrument::Ride:
        {
            configureMetallicOscillatorBank (
                instrument, voice.pitchRatio, voice.characterA, false);

            const float modalPitch = std::pow (voice.pitchRatio, 0.74f);
            const float modalDecay = 0.16f + voice.decaySeconds
                * (0.13f + 0.08f * voice.characterA);
            // Cast bronze has almost no internal loss - a cymbal's high partials
            // die because they radiate well and because the plate's nonlinear
            // coupling drains them, not because the metal absorbs them - so the
            // hysteretic share here is deliberately tiny. This reproduces the
            // shipping bank's decay spread across its 1x to 17.8x span while
            // stating the reason for it.
            initialiseModalVoice (voice, rideRatios, 12, 720.0f * modalPitch,
                                  modalDecay, 0.09f, 0.56f + 0.28f * voice.characterB,
                                  { 0.985f, 0.015f, 0.0f });
            const float filterPitch = std::pow (voice.pitchRatio, 0.46f);
            configureBandpass (voice.filterA, 3440.0f * filterPitch, 0.68f);
            configureBandpass (voice.filterB, 7100.0f * filterPitch, 0.76f);
            configureBandpass (voice.filterC, 10500.0f * filterPitch, 0.90f);
            voice.envelopeMultiplier = coefficientForTime (
                voice.decaySeconds * 0.88f, static_cast<float> (sampleRate_));
            voice.auxiliaryMultiplier = coefficientForTime (
                voice.decaySeconds * 1.06f, static_cast<float> (sampleRate_));
            voice.transientMultiplier = coefficientForTime (
                0.0022f + 0.0038f * voice.characterA,
                static_cast<float> (sampleRate_));
            break;
        }

        case Instrument::Crash:
        {
            configureMetallicOscillatorBank (
                instrument, voice.pitchRatio, voice.characterA, false);

            const float modalPitch = std::pow (voice.pitchRatio, 0.72f);
            const float modalDecay = 0.12f + voice.decaySeconds
                * (0.10f + 0.08f * voice.characterA);
            initialiseModalVoice (voice, crashRatios, 12, 620.0f * modalPitch,
                                  modalDecay, 0.12f + 0.36f * voice.characterA,
                                  0.58f + 0.30f * voice.characterB,
                                  { 0.985f, 0.015f, 0.0f });
            const float filterPitch = std::pow (voice.pitchRatio, 0.44f);
            configureBandpass (voice.filterA, 3440.0f * filterPitch, 0.62f);
            configureBandpass (voice.filterB, 7100.0f * filterPitch, 0.72f);
            configureBandpass (voice.filterC, 10500.0f * filterPitch, 0.84f);
            voice.envelopeMultiplier = coefficientForTime (
                voice.decaySeconds * 0.80f, static_cast<float> (sampleRate_));
            voice.auxiliaryMultiplier = coefficientForTime (
                voice.decaySeconds * 1.12f, static_cast<float> (sampleRate_));
            voice.transientMultiplier = coefficientForTime (
                0.0035f + 0.0065f * voice.characterA,
                static_cast<float> (sampleRate_));
            voice.pitchEnvelopeMultiplier = coefficientForTime (
                0.010f + 0.020f * voice.characterA,
                static_cast<float> (sampleRate_));
            break;
        }

        case Instrument::LowTom:
        case Instrument::MidTom:
        case Instrument::HighTom:
        {
            const float root = instrument == Instrument::LowTom ? 82.0f
                             : instrument == Instrument::MidTom ? 123.0f : 174.0f;
            voice.baseFrequency = root * voice.pitchRatio;
            voice.sweepAmount = 0.12f + 1.20f * voice.characterA;
            voice.phaseIncrements[1] = std::min (
                0.45f, voice.baseFrequency * (1.48f + 0.30f * voice.characterB) * inverseSampleRate_);
            voice.pitchEnvelopeMultiplier = coefficientForTime (0.016f + 0.040f * voice.characterA,
                                                                  static_cast<float> (sampleRate_));
            voice.transientMultiplier = coefficientForTime (0.003f + 0.004f * voice.characterB,
                                                             static_cast<float> (sampleRate_));

            // Punch is the stick. A wooden tip deforms both itself and the head
            // it lands on, and the harder the tip and the faster it arrives, the
            // less time the two stay together. A tip on a tensioned membrane
            // stiffens faster than Hertz's rigid sphere does, so the contact
            // shortens more steeply with speed than a bass drum beater's - which
            // is exactly why a tom cracks under a hard hit where a kick only
            // gets louder.
            voice.contactSeconds = (0.00120f - 0.00070f * voice.characterA)
                * std::pow (std::max (0.08f, velocity), -0.35f);
            voice.contactIncrement = std::min (
                1.0f, inverseSampleRate_ / std::max (1.0e-5f, voice.contactSeconds));
            voice.contactPhase = 0.0f;

            // Sixteen, twelve and ten inches across, with the shells that come
            // with them. Single-ply clear heads at a quarter of a kilogram per
            // square metre, struck a third of the way out from the middle, where
            // a player aims.
            HeadGeometry head;
            head.radius = instrument == Instrument::LowTom ? 0.203f
                        : instrument == Instrument::MidTom ? 0.152f : 0.127f;
            head.shellDepth = instrument == Instrument::LowTom ? 0.400f
                            : instrument == Instrument::MidTom ? 0.260f : 0.210f;
            head.headDensity = 0.25f;
            head.strikeRadius = 0.34f;
            head.contactSeconds = voice.contactSeconds;
            // Skin is how much air the head is made to carry: a slack head drags
            // more of it, spreads its overtones further from the note, and rings
            // less like a pitched drum and more like one being hit.
            head.airLoadScale = 0.70f + 0.70f * voice.characterB;
            buildHeadBank (voice, voice.baseFrequency, head,
                           voice.decaySeconds,
                           (0.34f + 0.30f * voice.characterB) * voice.velocityTimbre,
                           { 9.0f, 0.020f, 2.0e-6f, 260.0f });

            const float tomCorner = std::min (
                { 3400.0f, 2.2f / std::max (0.0002f, voice.contactSeconds),
                  0.30f * static_cast<float> (sampleRate_) });
            configureBandpass (voice.filterA, tomCorner, 0.72f);
            configureBandpass (voice.filterB, tomCorner, 0.72f);
            // The head above the frequency where its modes stop being separable
            // - a band rather than a series, dying at the rate that part of the
            // head dies at. Twelve resonators reach about four hundred hertz on
            // a floor tom; everything a stick puts in above that lives here.
            voice.auxiliaryMultiplier = coefficientForTime (
                0.030f + 0.12f * voice.decaySeconds, static_cast<float> (sampleRate_));
            voice.auxiliaryEnvelope = 0.0f;
            break;
        }

        case Instrument::Shaker:
            voice.transientEnvelope = 0.0f;
            voice.transientMultiplier = coefficientForTime (0.0008f + 0.0012f * voice.characterA,
                                                             static_cast<float> (sampleRate_));
            configureBandpass (voice.filterA,
                               (4400.0f + 6600.0f * voice.characterB)
                                   * std::pow (voice.pitchRatio, 0.65f) * voice.velocityTimbre,
                               0.75f + 1.2f * voice.characterB);
            configureHighpass (voice.filterB, 1900.0f + 2600.0f * voice.characterB, 0.70f);
            break;

        case Instrument::Perc1:
        {
            voice.baseFrequency = 535.0f * voice.pitchRatio;
            configureMetallicOscillatorBank (
                instrument, voice.pitchRatio, voice.characterA, false);
            configureBandpass (voice.filterA, std::min (6200.0f, voice.baseFrequency * 1.55f), 1.0f);
            configureHighpass (voice.filterB, 1800.0f, 0.70f);
            voice.transientMultiplier = coefficientForTime (0.0035f, static_cast<float> (sampleRate_));
            // Perc 1's Drive used to span 1.15 to 3.55 with the output stage's
            // 1/drive compensation cancelling almost all of it: over the whole
            // travel of the knob the voice changed by 6.9 % against a panel
            // average of 90 %, and lost 0.9 dB doing it. The wider span reaches
            // real saturation, and the makeup above keeps it a timbre control.
            voice.circuitDrive = std::clamp (
                1.15f + 8.2f * voice.characterB + variation.circuitDriveOffset,
                1.05f, 9.60f);
            break;
        }

        case Instrument::Perc2:
        {
            const float hollow = voice.characterA;
            const float ratios[4] { 1.0f, 1.42f + 0.35f * hollow,
                                    2.31f + 0.55f * hollow, 3.84f - 0.40f * hollow };
            // Wood's internal friction is close to a constant loss angle across
            // the audio range, so a struck bar's damping climbs roughly with
            // frequency and its bending overtones fade well before the note.
            initialiseModalVoice (voice, ratios, 4, 930.0f * voice.pitchRatio,
                                  voice.decaySeconds, 0.055f,
                                  (0.35f + 0.35f * hollow) * voice.velocityTimbre,
                                  { 0.45f, 0.55f, 0.0f });
            configureBandpass (voice.filterA,
                               (2800.0f + 5000.0f * voice.characterB) * voice.velocityTimbre,
                               0.85f);
            configureHighpass (voice.filterB, 350.0f + 550.0f * (1.0f - hollow), 0.70f);
            voice.transientMultiplier = coefficientForTime (0.0015f + 0.003f * voice.characterB,
                                                             static_cast<float> (sampleRate_));
            break;
        }

        case Instrument::Count:
            break;
    }

    // The output stage's transfer curve is now fully determined, so resolve it
    // once here rather than rebuilding it for every sample of the voice. ADAA
    // starts from the quiescent circuit operating point, which avoids a
    // fictitious interval from zero to the bias voltage on the first sample.
    const auto curve = analogCurveFor (instrument, voice.characterB);
    voice.analogPositiveCurvature = curve.positiveCurvature;
    voice.analogNegativeCurvature = curve.negativeCurvature;
    voice.analogMakeup = curve.makeup;
    voice.analogZero = rationalShaper (
        voice.circuitBias, curve.positiveCurvature, curve.negativeCurvature);
    voice.analogPreviousInput = voice.circuitBias;
    voice.analogPreviousPrimitive = rationalShaperPrimitive (
        voice.circuitBias, curve.positiveCurvature, curve.negativeCurvature);

    const int metallicBankIndex = metallicBankIndexFor (instrument);
    if (metallicBankIndex >= 0)
    {
        auto& bank = metallicBanks_[static_cast<std::size_t> (metallicBankIndex)];
        // The target contains this hit's tiny analogue tolerance, while these
        // values remember the nominal control position. Silent automation can
        // then retune the free-running source without erasing per-hit drift.
        bank.lastParameterPitch = values.pitch;
        bank.lastParameterCharacterA = values.characterA;
    }
}

void DrumEngine::trigger (Instrument instrument, float velocity) noexcept
{
    if (! validInstrument (instrument) || ! std::isfinite (velocity) || velocity <= 0.0f)
        return;
    if (! prepared_)
        prepare (sampleRate_, maxBlockSize_);
    // Restore the skipped interval under the targets that were active during
    // that interval, then publish any control change that arrived with this
    // trigger before the first audible sample is rendered.
    wakeMetallicOscillatorBankFor (instrument);
    updateMetallicBankParameterTargets();
    velocity = std::clamp (velocity, 0.0f, 1.0f);

    const auto index = indexFor (instrument);
    const auto values = snapshotParameters (instrument);
    // Mute groups generalise the hi-hat pedal: any voice can cut the group it
    // belongs to, and the two hats share group A by default.
    chokeGroup (values.chokeGroup);
    const auto counter = ++triggerCounters_[index];
    const std::uint32_t seed = hash32 (0x6d2b79f5u
        ^ static_cast<std::uint32_t> ((index + 1u) * 0x9e3779b9u)
        ^ static_cast<std::uint32_t> (counter)
        ^ static_cast<std::uint32_t> (counter >> 32u));

    // Component values in an analogue voice do not jump independently on
    // every strike: temperature, supply and capacitor history move slowly.
    // Model that as bounded, correlated trigger-domain drift, then layer very
    // small per-hit tolerances on top. Hash-derived values keep the sequence
    // reproducible after reset and independent of process block boundaries.
    auto& drift = componentDrift_[index];
    drift = std::clamp (0.86f * drift
                           + 0.14f * signedUnitFromHash (seed ^ 0xa341316cu),
                        -1.0f, 1.0f);
    // Humanise scales how much of that modelled tolerance actually reaches the
    // voice. The drift accumulator itself is untouched, so the per-instrument
    // sequence stays reproducible at every setting, and the 0.5 default maps to
    // a depth of exactly 1.0 - the historical fixed variation. Zero gives a
    // machine-tight kit; 1.0 doubles the spread. Each raw deviation is clamped
    // before scaling, so the depth-1.0 result is bit-identical to the original.
    const float humaniseDepth = 2.0f * clampUnit (
        humanise_.load (std::memory_order_relaxed), 0.5f);
    HitVariation variation;
    variation.pitchCents = humaniseDepth * (3.2f * drift
        + 2.4f * signedUnitFromHash (seed ^ 0xc8013ea4u));
    variation.decayScale = 1.0f + humaniseDepth * std::clamp (
        0.025f * drift + 0.016f * signedUnitFromHash (seed ^ 0xad90777du),
        -0.045f, 0.045f);
    variation.characterAOffset = humaniseDepth * (0.018f * drift
        + 0.014f * signedUnitFromHash (seed ^ 0x7e95761eu));
    variation.characterBOffset = humaniseDepth * (-0.014f * drift
        + 0.016f * signedUnitFromHash (seed ^ 0x3c6ef372u));
    variation.transientScale = 1.0f + humaniseDepth * std::clamp (
        -0.035f * drift + 0.035f * signedUnitFromHash (seed ^ 0xbb67ae85u),
        -0.075f, 0.075f);
    variation.circuitDriveOffset = humaniseDepth * (0.035f * drift
        + 0.045f * signedUnitFromHash (seed ^ 0x1b873593u));
    variation.circuitBias = humaniseDepth * (0.010f * drift
        + 0.007f * signedUnitFromHash (seed ^ 0x85ebca6bu));
    variation.phaseOffset = humaniseDepth * (0.010f * drift
        + 0.012f * signedUnitFromHash (seed ^ 0xc2b2ae35u));
    auto& voice = voices_[static_cast<std::size_t> (findVoiceSlot())];
    if (voice.active && voice.ageSamples != 0u)
        retireVoice (voice);
    if (voice.active)
        releaseBankReference (voice.instrument);
    initialiseVoice (voice, instrument, velocity, values, seed, variation);
    addBankReference (instrument);
    anyVoiceActive_ = true;
    updateActiveVoiceCount();
}

bool DrumEngine::triggerMidi (int midiNote, float velocity) noexcept
{
    const auto instrument = instrumentForMidiNote (midiNote);
    if (! instrument.has_value())
        return false;
    trigger (*instrument, velocity);
    return true;
}

float DrumEngine::advanceContact (Voice& voice) noexcept
{
    // The strike as the head feels it: nothing at first touch, most force when
    // the head is deepest, nothing again as the beater or stick leaves. One
    // raised cosine lasting the contact time, which is what a Hertzian contact
    // between a ball and a stretched membrane very nearly is.
    if (voice.contactPhase >= 1.0f)
        return 0.0f;
    float sine = 0.0f;
    float cosine = 1.0f;
    sineAndCosineLookup (voice.contactPhase, sine, cosine);
    voice.contactPhase += voice.contactIncrement;
    return 0.5f * (1.0f - cosine);
}

float DrumEngine::renderKick (Voice& voice) noexcept
{
    // Stable time-varying energy-state resonator. Unlike directly changing a
    // biquad's coefficients, each update is a rotation followed by an explicit
    // contraction, so rapid pitch sweeps cannot inject unbounded state energy.
    // Euclidean state energy is invariant under the quadrature rotation.
    // The former L1 estimate changed with phase and unintentionally modulated
    // pitch and damping at twice the kick frequency. The scale preserves its
    // former average operating range while removing that digital fingerprint.
    const float stateMagnitude = std::min (
        1.5f, 1.27323954f * std::sqrt (
            voice.kickStateX * voice.kickStateX
            + voice.kickStateY * voice.kickStateY));
    const float triggerSweep = 0.84f + 0.16f * voice.velocity;
    const float amplitudePitch = 1.0f
        + 0.016f * voice.characterB * stateMagnitude;
    const float frequency = std::clamp (
        voice.baseFrequency
            * (1.0f + voice.sweepAmount * triggerSweep * voice.pitchEnvelope)
            * amplitudePitch,
        4.0f, 0.18f * static_cast<float> (sampleRate_));

    const float angle = twoPi * frequency * inverseSampleRate_;
    const float angleSquared = angle * angle;
    const float angleFourth = angleSquared * angleSquared;
    const float angleSixth = angleFourth * angleSquared;
    float sine = angle * (1.0f - angleSquared / 6.0f
                          + angleFourth / 120.0f - angleSixth / 5040.0f);
    float cosine = 1.0f - angleSquared / 2.0f + angleFourth / 24.0f
        - angleSixth / 720.0f + angleFourth * angleFourth / 40320.0f;
    // One Newton normalization keeps the polynomial rotation on the unit
    // circle, preventing approximation error from becoming hidden damping.
    const float normCorrection = 1.5f
        - 0.5f * (sine * sine + cosine * cosine);
    sine *= normCorrection;
    cosine *= normCorrection;

    // Diode/conductor losses rise with stored energy. Applying the loss as a
    // positive contraction keeps it stable at every supported sample rate.
    const float nonlinearLossPerSecond = (0.45f + 3.5f * voice.characterB)
        * stateMagnitude;
    const float dynamicLoss = std::clamp (
        1.0f - nonlinearLossPerSecond * inverseSampleRate_, 0.98f, 1.0f);
    const float radius = std::clamp (
        voice.kickBaseRadius * dynamicLoss, 0.0f, 0.9999995f);

    // The beater's force history. A Hertzian contact between a ball and a
    // stretched head is very close to a single raised cosine lasting the
    // contact time: nothing at first touch, most force when the head is deepest,
    // nothing again as the beater leaves. Its integral is the impulse the pedal
    // delivered, so the drum takes the same momentum at every sample rate and
    // only the detail of the pulse changes with it - which the exponential this
    // replaced did not manage, being a single sample wide at 8 kHz and fifty at
    // 192 kHz and hitting the head that much harder for it.
    const float contactForce = advanceContact (voice);
    const float discharge = 2.0f * voice.kickCharge * voice.contactIncrement
        * contactForce;
    // Felt against plastic is a rough, sliding interface, and it only rubs
    // while the two are touching. Letting the contact raise the broadband
    // layer rather than starting it at full height on the first sample is both
    // what happens and what keeps the attack the same size at every rate: a
    // step at sample zero is a different step when samples are six times
    // further apart.
    voice.auxiliaryEnvelope = std::max (voice.auxiliaryEnvelope, contactForce);
    const float stateX = voice.kickStateX + discharge;
    const float stateY = voice.kickStateY;
    voice.kickStateX = flushDenormal (radius * (cosine * stateX - sine * stateY));
    voice.kickStateY = flushDenormal (radius * (sine * stateX + cosine * stateY));

    // The same impulse the reservoir feeds the body drives the rest of the head.
    // This is one event, not a click layered over a sine: the beater arrives,
    // the head's modes start together, and the branch the trapped air stiffened
    // dies away first because it is the one that actually radiates.
    //
    // The pulse's own shape is already in the bank - each mode's gain carries
    // how much of a strike this long could ever reach it - so what the bank
    // receives here is the impulse itself, delivered whole. Handing it the
    // sampled pulse instead would make the drum louder at low sample rates,
    // where a resonator's direct term is six times the size and the pulse is
    // nine samples rather than fifty.
    float head = 0.0f;
    if (voice.ageSamples < voice.modalActiveSamples)
    {
        if (voice.ageSamples == 0u)
        {
            const float impulse = voice.kickCharge * voice.excitationScale;
            for (int mode = 0; mode < voice.modeCount; ++mode)
                voice.resonators[static_cast<std::size_t> (mode)].strike (
                    impulse * voice.modeGains[static_cast<std::size_t> (mode)]);
        }
        for (int mode = 0; mode < voice.modeCount; ++mode)
            head += voice.resonators[static_cast<std::size_t> (mode)].tick (0.0f);
    }

    const float skin = voice.filterB.tick (
            voice.filterA.tick (nextBandLimitedNoise (voice)))
        * voice.auxiliaryEnvelope * voice.transientScale * voice.velocityTimbre
        * (0.40f + 1.20f * voice.characterA);
    const float body = (1.30f + 0.16f * voice.characterB) * voice.kickStateY;
    // Harmonics and level-dependent coloration are intentionally delegated to
    // the shared antialiased circuit stage, avoiding a second aliasing clipper.
    return body + skin + (0.55f + 0.40f * voice.characterA) * head;
}

float DrumEngine::renderSnare (Voice& voice) noexcept
{
    const float body = (0.72f * oscillator (voice, 0) + 0.36f * oscillator (voice, 1))
        * voice.envelope;
    const float noise = nextBandLimitedNoise (voice);

    // The two heads and the air between them. The branch where they move
    // together is stiffened by that air into the snare's crack, and because it
    // is also the branch that radiates it is the first thing gone; the branch
    // where they oppose each other is what is left ringing under the wires.
    float headModes = 0.0f;
    if (voice.ageSamples < voice.modalActiveSamples)
    {
        if (voice.ageSamples == 0u)
        {
            const float impulse = voice.transientScale * voice.excitationScale;
            for (int mode = 0; mode < voice.modeCount; ++mode)
                voice.resonators[static_cast<std::size_t> (mode)].strike (
                    impulse * voice.modeGains[static_cast<std::size_t> (mode)]);
        }
        for (int mode = 0; mode < voice.modeCount; ++mode)
            headModes += voice.resonators[static_cast<std::size_t> (mode)].tick (0.0f);
    }

    // Snare wires are not a linear noise envelope: they only leave the resonant
    // head, and therefore rattle, while its displacement exceeds their resting
    // contact. Below that they damp the head instead. Gating the wire noise on
    // instantaneous head displacement reproduces the characteristic buzz that
    // fades into a dry, damped tail rather than a clean exponential hiss.
    // What the wires actually rest on is the head, all of it, not the tuned
    // sine that stands in for its lowest mode.
    const float displacement = std::abs (body) + 0.30f * std::abs (headModes)
        + 0.55f * advanceContact (voice);
    const float rattle = displacement / (0.10f + displacement);
    const float wires = voice.filterA.tick (noise) * voice.auxiliaryEnvelope
        * (0.30f + 0.70f * rattle);
    const float snap = voice.filterB.tick (noise) * voice.transientEnvelope
        * voice.transientScale * voice.velocityTimbre;
    const float wireMix = 0.18f + 0.82f * voice.characterA;
    return 0.72f * ((0.62f - 0.38f * voice.characterA) * body
                    + (0.80f - 0.34f * voice.characterA) * headModes
                    + wireMix * wires + 0.35f * voice.characterB * snap);
}

float DrumEngine::renderClap (Voice& voice) noexcept
{
    for (const auto start : voice.burstStarts)
        if (voice.ageSamples == start)
            voice.transientEnvelope += 1.0f;
    const float noise = voice.filterB.tick (
        voice.filterA.tick (nextBandLimitedNoise (voice)));
    const float burst = (0.38f + 0.16f * voice.characterA)
        * voice.transientEnvelope * voice.transientScale;
    const float tail = (0.13f + 0.24f * voice.characterB) * voice.envelope;
    return noise * (burst + tail);
}

float DrumEngine::renderHat (Voice& voice) noexcept
{
    const float noise = nextBandLimitedNoise (voice);
    // The persistent Schmitt/RC bank is evaluated once per engine sample, so
    // overlapping hits hear the same free-running hardware source instead of
    // restarting six ideal sines with newly randomized components.
    const float metallic = metallicSourceFor (voice.instrument)
                         + 0.20f * (1.0f - voice.characterA) * noise;
    const float high = voice.filterA.tick (metallic);
    const float focused = voice.filterB.tick (metallic);
    const float attack = 0.12f * voice.transientEnvelope * noise * voice.transientScale
        * voice.velocityTimbre;
    return (0.58f * high + (0.18f + 0.20f * voice.characterB) * focused + attack)
        * voice.envelope;
}

float DrumEngine::renderRide (Voice& voice) noexcept
{
    const float oscillatorBank = metallicSourceFor (voice.instrument);
    const float noise = nextBandLimitedNoise (voice);
    const float pcm = nextCymbalPcm (
        voice, 0.68f * oscillatorBank + 0.24f * noise);

    // Three circuit bands mirror the useful structure of the 808 cymbal,
    // while the quantized generated layer fills the continuous spectrum that
    // made the sample-based 909 ride sit easily in a mix.
    const float bodyBand = voice.filterA.tick (
        0.78f * oscillatorBank + 0.22f * pcm);
    const float shimmerBand = voice.filterB.tick (
        0.58f * oscillatorBank + 0.42f * pcm);
    const float airBand = voice.filterC.tick (
        0.34f * oscillatorBank + 0.66f * pcm);

    float modes = 0.0f;
    if (voice.ageSamples < voice.modalActiveSamples)
    {
        const float contact = voice.ageSamples == 0u
            ? 2.15f * voice.transientScale * voice.excitationScale
            : 0.055f * modalNoiseScale_ * noise * voice.transientEnvelope
                * voice.transientScale * voice.excitationScale;
        for (std::size_t mode = 0; mode < resonatorCount; ++mode)
        {
            float gain = voice.modeGains[mode];
            gain *= mode < 4
                ? 0.50f + 1.40f * voice.characterA
                : 0.90f - 0.25f * voice.characterA;
            modes += gain * voice.resonators[mode].tick (contact);
        }
    }

    const float bell = voice.characterA;
    const float tone = voice.characterB;
    const float body = (0.46f + 0.50f * bell - 0.10f * tone)
        * bodyBand * voice.envelope;
    const float shimmer = (0.18f + 0.34f * tone)
        * shimmerBand * voice.auxiliaryEnvelope;
    const float air = (0.065f + 0.255f * tone)
        * airBand * voice.auxiliaryEnvelope;
    const float bellModes = (0.12f + 0.42f * bell) * modes;
    return 1.12f * (body + shimmer + air + bellModes);
}

float DrumEngine::renderCrash (Voice& voice) noexcept
{
    const float oscillatorBank = metallicSourceFor (voice.instrument);
    const float noise = nextBandLimitedNoise (voice);
    const float pcm = nextCymbalPcm (
        voice, 0.52f * oscillatorBank + 0.34f * noise);
    const float spread = voice.characterA;
    const float coherent = 0.68f - 0.28f * spread;
    const float quantized = 0.32f + 0.18f * spread;
    const float diffuse = 0.10f * spread;
    const float source = coherent * oscillatorBank + quantized * pcm
                       + diffuse * noise;

    const float bodyBand = voice.filterA.tick (source);
    const float shimmerBand = voice.filterB.tick (
        (0.86f - 0.18f * spread) * source + 0.18f * spread * noise);
    const float airBand = voice.filterC.tick (
        (0.72f - 0.20f * spread) * source + 0.28f * spread * noise);

    // The modal layer is struck briefly and then left alone. The long tail is
    // carried by diffuse oscillator/PCM bands, avoiding the old continuously
    // driven resonators that exposed a handful of clinging pitches.
    float modes = 0.0f;
    if (voice.ageSamples < voice.modalActiveSamples)
    {
        const float excitation = voice.ageSamples == 0u
            ? 1.75f * voice.transientScale * voice.excitationScale
            : 0.075f * modalNoiseScale_ * noise * voice.transientEnvelope
                * voice.transientScale * voice.excitationScale;
        for (std::size_t mode = 0; mode < resonatorCount; ++mode)
            modes += voice.modeGains[mode] * voice.resonators[mode].tick (excitation);
    }

    const float brightness = voice.characterB;
    const float bloom = 0.34f + 0.66f * (1.0f - voice.pitchEnvelope);
    const float body = (0.43f - 0.11f * brightness)
        * bodyBand * voice.envelope;
    const float shimmer = (0.17f + 0.38f * brightness)
        * shimmerBand * voice.auxiliaryEnvelope * bloom;
    const float air = (0.07f + 0.32f * brightness)
        * airBand * voice.auxiliaryEnvelope * bloom;
    const float struckMetal = (0.20f - 0.07f * spread) * modes;
    return 1.18f * (body + shimmer + air + struckMetal);
}

float DrumEngine::renderTom (Voice& voice) noexcept
{
    // Tension modulation: a displaced head is a stiffer head, so the pitch is
    // highest while the strike energy is still stored and settles as the tom
    // rings out. This nonlinear drop rides on top of the fast contact sweep and
    // is what distinguishes a struck membrane from a pitch-enveloped sine.
    const float tension = 1.0f
        + (0.006f + 0.052f * voice.characterB) * voice.excitationScale
              * voice.envelope * voice.envelope;
    const float frequency = voice.baseFrequency
        * (1.0f + voice.sweepAmount * voice.pitchEnvelope)
        * tension;
    voice.phaseIncrements[0] = std::min (0.45f, frequency * inverseSampleRate_);
    const float fundamental = oscillator (voice, 0);
    const float shell = oscillator (voice, 1);
    const float noise = nextBandLimitedNoise (voice);
    voice.auxiliaryEnvelope = std::max (voice.auxiliaryEnvelope,
                                        advanceContact (voice));
    const float skin = voice.filterB.tick (voice.filterA.tick (noise))
        * voice.auxiliaryEnvelope * voice.transientScale * voice.velocityTimbre;

    // The head itself: every mode the stick reached, with the two heads' air
    // coupling splitting the axisymmetric family into the branch that radiates
    // and dies and the branch that does not and stays. The stick sets them
    // moving and then leaves, so the bank is struck rather than driven - and
    // the low-level noise that used to keep feeding it is gone with it, because
    // a head that has been hit is not being hit again.
    float membrane = 0.0f;
    if (voice.ageSamples < voice.modalActiveSamples)
    {
        if (voice.ageSamples == 0u)
        {
            const float impulse = voice.transientScale * voice.excitationScale;
            for (int mode = 0; mode < voice.modeCount; ++mode)
                voice.resonators[static_cast<std::size_t> (mode)].strike (
                    impulse * voice.modeGains[static_cast<std::size_t> (mode)]);
        }
        for (int mode = 0; mode < voice.modeCount; ++mode)
            membrane += voice.resonators[static_cast<std::size_t> (mode)].tick (0.0f);
    }

    return 0.98f * ((0.90f * fundamental + (0.06f + 0.19f * voice.characterB) * shell)
                        * voice.envelope
                    + (0.55f + 0.40f * voice.characterB) * membrane
                    + (0.72f + 0.80f * voice.characterB) * skin);
}

float DrumEngine::renderShaker (Voice& voice) noexcept
{
    // A collision either happens this sample or it does not, so the scheduler
    // draws straight from the generator; only the grain it excites is audio and
    // therefore has to carry a rate-independent noise density.
    const float decision = 0.5f + 0.5f * nextNoise (voice);
    const float collisionsPerSecond = 320.0f + 4800.0f * voice.characterA;
    const float probability = std::min (0.80f, collisionsPerSecond * inverseSampleRate_);
    const float grainNoise = nextBandLimitedNoise (voice);
    if (decision < probability)
        voice.transientEnvelope += voice.transientScale * voice.excitationScale
            * (0.45f + 0.55f * std::abs (grainNoise));
    const float grains = voice.filterB.tick (voice.filterA.tick (
        grainNoise * voice.transientEnvelope));
    return 0.95f * grains * voice.envelope;
}

float DrumEngine::renderPerc1 (Voice& voice) noexcept
{
    const float metallic = metallicSourceFor (voice.instrument);
    const float click = voice.filterB.tick (nextBandLimitedNoise (voice))
        * voice.transientEnvelope * voice.transientScale;
    const float shaped = voice.filterA.tick (metallic) * voice.envelope + 0.12f * click;
    // Drive is handled by the shared antialiased stage. Keeping a single
    // nonlinear memory here avoids cascading a memoryless alias source.
    return 1.05f * shaped;
}

float DrumEngine::renderPerc2 (Voice& voice) noexcept
{
    const float noise = nextBandLimitedNoise (voice);
    float body = 0.0f;
    if (voice.ageSamples < voice.modalActiveSamples)
    {
        const float excitation = (voice.ageSamples == 0 ? voice.transientScale : 0.0f)
            + 0.10f * modalNoiseScale_ * voice.characterB * voice.transientEnvelope
                * voice.transientScale * noise;
        for (std::size_t mode = 0; mode < 4; ++mode)
            body += voice.modeGains[mode] * voice.resonators[mode].tick (
                excitation * voice.excitationScale);
    }
    // Stick content scales with velocity as well as level: a light tap on a
    // wooden or metallic body puts far less energy into the contact click.
    const float click = voice.filterA.tick (noise) * voice.transientEnvelope
        * voice.velocityTimbre;
    return 1.35f * voice.filterB.tick (body + 0.20f * voice.characterB * click);
}

float DrumEngine::renderVoice (Voice& voice) noexcept
{
    if (voice.ageSamples >= forcedFadeStartSamples_)
        beginFadeToSilence (voice, forcedFadeMultiplier_);

    float output = 0.0f;
    switch (voice.instrument)
    {
        case Instrument::Kick:      output = renderKick (voice); break;
        case Instrument::Snare:     output = renderSnare (voice); break;
        case Instrument::Clap:      output = renderClap (voice); break;
        case Instrument::ClosedHat:
        case Instrument::OpenHat:   output = renderHat (voice); break;
        case Instrument::Ride:      output = renderRide (voice); break;
        case Instrument::Crash:     output = renderCrash (voice); break;
        case Instrument::LowTom:
        case Instrument::MidTom:
        case Instrument::HighTom:   output = renderTom (voice); break;
        case Instrument::Shaker:    output = renderShaker (voice); break;
        case Instrument::Perc1:     output = renderPerc1 (voice); break;
        case Instrument::Perc2:     output = renderPerc2 (voice); break;
        case Instrument::Count:     break;
    }

    output *= voice.velocity * voice.chokeGain * voice.levelGain;
    output = applyAnalogOutputStage (voice, output);
    voice.recentPeak = flushDenormal (std::max (
        std::abs (output), voice.recentPeak * peakReleaseMultiplier_));
    voice.envelope = flushDenormal (voice.envelope * voice.envelopeMultiplier);
    voice.auxiliaryEnvelope = flushDenormal (
        voice.auxiliaryEnvelope * voice.auxiliaryMultiplier);
    voice.transientEnvelope = flushDenormal (
        voice.transientEnvelope * voice.transientMultiplier);
    voice.pitchEnvelope = flushDenormal (
        voice.pitchEnvelope * voice.pitchEnvelopeMultiplier);
    if (voice.choking)
        voice.chokeGain *= voice.chokeMultiplier;
    ++voice.ageSamples;

    if (! voice.choking && voice.ageSamples >= voice.minimumSilenceSamples)
    {
        if (voice.recentPeak < silenceThreshold)
            ++voice.quietSamples;
        else
            voice.quietSamples = 0u;
    }

    if ((voice.choking && voice.chokeGain <= silenceThreshold)
        || voice.quietSamples >= naturalQuietHoldSamples_
        || voice.ageSamples >= voice.maximumSamples)
        silenceVoice (voice);
    return output;
}

void DrumEngine::applyBusStage (float& left, float& right, float driveAmount,
                                float compressionAmount) noexcept
{
    if (driveAmount > 0.0f)
    {
        // A shared transformer/console-style softener. The linear region is
        // gain-matched, so Drive adds density and level dependence rather than
        // simply making the kit louder, and the same first-order ADAA used by
        // the voice stages keeps its corner from splattering aliases.
        const float driveGain = 1.0f + 3.0f * driveAmount;
        // Curvature scales from zero so the stage meets bypass continuously.
        // A fixed offset meant the first fraction of a percent on the knob
        // jumped straight to a third of the full curve -- a steady 0.5 landed
        // at roughly 0.435 -- so automation crossing zero clicked. Both the
        // gain and its compensation already reach unity at zero, so this is
        // the last discontinuity, and full drive is unchanged at 1.0. Letting
        // the curvature run all the way down to zero is only safe because
        // rationalShaperPrimitive() is evaluated in a form that stays accurate
        // there; the textbook antiderivative loses every significant digit
        // below about 1e-3 and turned this stage into a square wave.
        const float curvature = driveAmount;
        const float compensation = (1.0f + 0.55f * driveAmount) / driveGain;
        left = compensation * antialiasedRationalShaper (
            driveGain * left, busDriveAdaaLeft_, curvature, curvature);
        right = compensation * antialiasedRationalShaper (
            driveGain * right, busDriveAdaaRight_, curvature, curvature);
    }
    else
    {
        // Keep the ADAA memory tracking the signal while the stage is bypassed.
        // Otherwise the first sample after Drive is switched back on takes its
        // divided difference against whatever was on the bus when it was
        // switched off, however long ago that was - the same stale-state click
        // the detector below already avoids. driveGain is 1 at zero drive, so
        // the bypassed stage's input is the sample itself.
        busDriveAdaaLeft_ = left;
        busDriveAdaaRight_ = right;
    }

    // Stereo-linked peak detector with a 4 ms attack and 140 ms release. It
    // keeps running while the compressor is bypassed: freezing it meant that
    // automating Bus Compression back on applied the gain reduction that was in
    // flight when it was switched off, however long ago that was, instead of
    // responding to the signal actually present.
    const float detector = std::max (std::abs (left), std::abs (right));
    const float detectorCoefficient = detector > busEnvelope_
        ? busAttackCoefficient_ : busReleaseCoefficient_;
    busEnvelope_ = flushDenormal (
        busEnvelope_ + detectorCoefficient * (detector - busEnvelope_));

    if (compressionAmount > 0.0f)
    {
        // The gain law blends continuously between unity and hard limiting, an
        // approximation of a soft-knee ratio that avoids a per-sample pow().
        const float threshold = 0.50f - 0.42f * compressionAmount;
        const float slope = compressionAmount / (0.35f + 0.65f * compressionAmount);
        float gain = 1.0f;
        if (busEnvelope_ > threshold)
            gain = 1.0f + slope * (threshold / busEnvelope_ - 1.0f);
        busGain_ = std::clamp (std::isfinite (gain) ? gain : 1.0f, 0.02f, 1.0f);

        const float makeup = 1.0f + 1.15f * compressionAmount;
        left *= busGain_ * makeup;
        right *= busGain_ * makeup;
    }
    else
    {
        busGain_ = 1.0f;
    }
}

void DrumEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;
    if (! prepared_)
        prepare (sampleRate_, std::max (maxBlockSize_, numSamples));

    updateMetallicBankParameterTargets();

    // Voice activity can only decrease inside one engine chunk; triggers split
    // processing at their exact event offsets. Collect the audible voices once
    // per chunk instead of rescanning both 64-voice pools for every sample.
    std::array<Voice*, maxVoices + retiringVoiceCount> chunkVoices {};
    int chunkVoiceCount = 0;
    const auto observeVoice = [&chunkVoices, &chunkVoiceCount] (Voice& voice)
    {
        if (voice.active)
            chunkVoices[static_cast<std::size_t> (chunkVoiceCount++)] = &voice;
    };
    for (auto& voice : voices_)
        observeVoice (voice);
    for (auto& voice : retiringVoices_)
        observeVoice (voice);

    const float gainTarget = outputGain_.load (std::memory_order_relaxed);
    const float gainSmoothing = gainSmoothingCoefficient_;
    const float dcCoefficient = dcBlockerCoefficient_;
    const float driveAmount = clampUnit (
        busDrive_.load (std::memory_order_relaxed), 0.0f);
    const float compressionAmount = clampUnit (
        busCompression_.load (std::memory_order_relaxed), 0.0f);
    // Refreshed per block, then smoothed into each ringing voice below so
    // channel-strip automation is audible on a tail rather than only on the
    // next hit.
    for (std::size_t index = 0; index < instrumentCount; ++index)
    {
        const auto values = snapshotParameters (static_cast<Instrument> (index));
        auto& mixer = mixerTargets_[index];
        mixer.levelGain = decibelsToGain (values.level);
        const float pan = std::clamp (values.pan, -1.0f, 1.0f);
        mixer.panLeft = constantPowerLeft (pan);
        mixer.panRight = constantPowerRight (pan);
    }

    const bool busActive = driveAmount > 0.0f || compressionAmount > 0.0f
        || smoothedBusDrive_ > 0.0f || smoothedBusCompression_ > 0.0f;
    if (! busActive)
        resetBusStage();
    std::uint64_t silentSamples = 0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // While no voice exists in either pool, every downstream stage is at
        // its zero rest state and the block is exact digital silence, so only
        // the output-gain smoother needs to advance. The skipped samples are
        // accounted to every metallic bank below so the next trigger can
        // restore the circuit state it would have reached.
        if (! anyVoiceActive_)
        {
            ++silentSamples;
            smoothedOutputGain_ += gainSmoothing * (gainTarget - smoothedOutputGain_);
            smoothedBusDrive_ = approachTarget (
                smoothedBusDrive_, driveAmount, gainSmoothing);
            smoothedBusCompression_ = approachTarget (
                smoothedBusCompression_, compressionAmount, gainSmoothing);
            if (left != nullptr)
                left[sample] = 0.0f;
            if (right != nullptr && right != left)
                right[sample] = 0.0f;
            continue;
        }

        // Component oscillators advance whenever a voice can observe them, so
        // a strike samples the circuit's actual phase rather than restarting a
        // synthetic waveform at note-on. Banks nobody can hear are frozen and
        // restored analytically, which is exact through a closed VCA.
        renderMetallicOscillatorBanks (metallicBankMask_);
        float dryLeft = 0.0f;
        float dryRight = 0.0f;
        bool hasActiveVoices = false;
        for (int voiceIndex = 0; voiceIndex < chunkVoiceCount; ++voiceIndex)
        {
            auto& voice = *chunkVoices[static_cast<std::size_t> (voiceIndex)];
            if (! voice.active)
                continue;
            // Track the channel strip at the same 20 ms constant as the master
            // gain. A voice starts at its instrument's current value, so with
            // static parameters this is exactly a no-op.
            const auto& mixer = mixerTargets_[indexFor (voice.instrument)];
            voice.levelGain += gainSmoothing * (mixer.levelGain - voice.levelGain);
            voice.panLeft += gainSmoothing * (mixer.panLeft - voice.panLeft);
            voice.panRight += gainSmoothing * (mixer.panRight - voice.panRight);

            // Pan is captured first because a voice completing its tail this
            // sample is reset to defaults inside renderVoice.
            const float panLeft = voice.panLeft;
            const float panRight = voice.panRight;
            const float value = renderVoice (voice);
            dryLeft += value * panLeft;
            dryRight += value * panRight;
            hasActiveVoices = hasActiveVoices || voice.active;
        }

        smoothedOutputGain_ += gainSmoothing * (gainTarget - smoothedOutputGain_);
        // Both bus stages follow the same 20 ms law as the master gain. Taking
        // them straight from the block's parameter value put a hard step in the
        // mix: measured on a sustained 24 Hz kick, switching Bus Compression on
        // between two blocks produced a sample-to-sample jump 143 times the
        // largest step anywhere else in that waveform - an audible click.
        smoothedBusDrive_ = approachTarget (
            smoothedBusDrive_, driveAmount, gainSmoothing);
        smoothedBusCompression_ = approachTarget (
            smoothedBusCompression_, compressionAmount, gainSmoothing);
        float busLeft = dryLeft - dcInputLeft_ + dcCoefficient * dcOutputLeft_;
        float busRight = dryRight - dcInputRight_ + dcCoefficient * dcOutputRight_;
        dcInputLeft_ = flushDenormal (dryLeft);
        dcInputRight_ = flushDenormal (dryRight);
        dcOutputLeft_ = flushDenormal (busLeft);
        dcOutputRight_ = flushDenormal (busRight);

        if (busActive)
            applyBusStage (busLeft, busRight, smoothedBusDrive_,
                           smoothedBusCompression_);

        const float outputLeft = std::clamp (antialiasedRationalShaperCached (
            smoothedOutputGain_ * busLeft, masterAdaaPreviousLeft_,
            masterAdaaPrimitiveLeft_, 1.0f, 1.0f), -1.0f, 1.0f);
        const float outputRight = std::clamp (antialiasedRationalShaperCached (
            smoothedOutputGain_ * busRight, masterAdaaPreviousRight_,
            masterAdaaPrimitiveRight_, 1.0f, 1.0f), -1.0f, 1.0f);
        meterPeakLeft_ = flushDenormal (std::max (
            std::abs (outputLeft), meterPeakLeft_ * peakReleaseMultiplier_));
        meterPeakRight_ = flushDenormal (std::max (
            std::abs (outputRight), meterPeakRight_ * peakReleaseMultiplier_));
        if (left != nullptr && right == left)
        {
            left[sample] = 0.5f * (outputLeft + outputRight);
        }
        else
        {
            if (left != nullptr)
                left[sample] = outputLeft;
            if (right != nullptr)
                right[sample] = outputRight;
        }

        // The final voice has already reached the inaudible end of its natural
        // or forced fade. Do not let the mix DC blocker extend the host tail.
        if (! hasActiveVoices)
        {
            dcInputLeft_ = dcInputRight_ = 0.0f;
            dcOutputLeft_ = dcOutputRight_ = 0.0f;
            masterAdaaPreviousLeft_ = masterAdaaPreviousRight_ = 0.0f;
            masterAdaaPrimitiveLeft_ = masterAdaaPrimitiveRight_ = 0.0f;
        }
        anyVoiceActive_ = hasActiveVoices;
    }

    if (silentSamples != 0u)
    {
        for (auto& bank : metallicBanks_)
            bank.frozenSamples += silentSamples;
        const auto releasedSilence = static_cast<float> (silentSamples);
        const float release = std::pow (peakReleaseMultiplier_, releasedSilence);
        meterPeakLeft_ *= release;
        meterPeakRight_ *= release;
    }

    // With no voice left, the bus sees exact zero: its detector would decay to
    // rest and its gain to unity. Doing that in one step keeps silence free and
    // stops a stale gain-reduction reading from sticking on the editor's meter.
    if (! anyVoiceActive_)
        resetBusStage();

    outputLevelLeft_.store (meterPeakLeft_, std::memory_order_relaxed);
    outputLevelRight_.store (meterPeakRight_, std::memory_order_relaxed);
    busGainMeter_.store (busGain_, std::memory_order_relaxed);
    updateActiveVoiceCount();
}

} // namespace drumalor
