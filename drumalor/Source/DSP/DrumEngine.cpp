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
    { Instrument::Kick,      "Kick",       "kick",       36, "Punch",   "Drive",      { 0.68f, 0.42f, 0.4f, 0.55f, 0.0f,  0.00f, 0 } },
    { Instrument::Snare,     "Snare",      "snare",      38, "Wires",   "Snap",       { 0.62f, 0.64f, 1.0f, 0.48f, 0.0f,  0.00f, 0 } },
    { Instrument::Clap,      "Clap",       "clap",       39, "Spread",  "Tone",       { 0.48f, 0.62f, 0.0f, 0.45f, 0.0f,  0.00f, 0 } },
    { Instrument::ClosedHat, "Closed Hat", "closedHat", 42, "Metal",   "Tone",       { 0.58f, 0.70f, 0.0f, 0.30f, 0.0f,  0.16f, 1 } },
    { Instrument::OpenHat,   "Open Hat",   "openHat",   46, "Metal",   "Tone",       { 0.62f, 0.68f, 0.0f, 0.55f, 0.0f,  0.20f, 1 } },
    // Machine runs 0 = the 1980 analogue cymbal channel, 1 = the 1983 digital
    // one. The defaults sit each voice on the machine that actually made that
    // sound: the ride existed only on the digital machine, and the analogue
    // one's single cymbal was, in everything but name, a crash.
    { Instrument::Ride,      "Ride",       "ride",       51, "Machine", "Tone",       { 0.78f, 0.62f, 0.0f, 0.62f, 0.0f,  0.27f, 0 } },
    { Instrument::Crash,     "Crash",      "crash",      49, "Machine", "Brightness", { 0.68f, 0.82f, 0.0f, 0.86f, 0.0f, -0.27f, 0 } },
    { Instrument::LowTom,    "Low Tom",    "lowTom",     45, "Punch",   "Skin",       { 0.55f, 0.40f, -1.9f, 0.60f, 0.0f, -0.20f, 0 } },
    { Instrument::MidTom,    "Mid Tom",    "midTom",     47, "Punch",   "Skin",       { 0.55f, 0.45f, -0.1f, 0.52f, 0.0f,  0.00f, 0 } },
    { Instrument::HighTom,   "High Tom",   "highTom",    50, "Punch",   "Skin",       { 0.50f, 0.50f, -2.9f, 0.45f, 0.0f,  0.20f, 0 } },
    { Instrument::Shaker,    "Shaker",     "shaker",     82, "Density", "Color",      { 0.62f, 0.62f, 0.0f, 0.45f, 0.0f,  0.12f, 0 } },
    { Instrument::Perc1,     "Perc 1",     "perc1",      56, "Ratio",   "Drive",      { 0.50f, 0.45f, -0.8f, 0.45f, 0.0f, -0.12f, 0 } },
    { Instrument::Perc2,     "Perc 2",     "perc2",      75, "Hollow",  "Click",      { 0.55f, 0.55f, 0.9f, 0.40f, 0.0f,  0.12f, 0 } },
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
// The series runs to twelve mode families, which is already far more than the
// five modes a real head needs. That is no longer the resonator bank's own
// slot count - the bank has grown to eighteen slots so buildHeadBank can split
// some m > 0 families into the physical pair each one actually is (see
// resonatorCount in DrumEngine.h). Stopping the family list at 2.917 put every
// mode a tom could ring inside its bottom two octaves - an 82 Hz floor tom had
// nothing modelled above 240 Hz, which is why its measured spectrum fell off a
// cliff where a real drum still has the stick in it.
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

// Where a player aims around the hoop, in radians, before Humanise moves it.
// Every m > 0 mode of a circular head is two modes at right angles to each
// other, and a strike at azimuth phi drives them as cos(m phi) and sin(m phi),
// so the aim decides the balance of every pair - and an aim of zero would
// strike one member of each pair and leave the other silent, which is a head
// with no warble in it at all.
//
// Twenty-five degrees is not decoration. It keeps m*phi off every multiple of
// ninety degrees for all six circumferential orders the mode table carries, so
// no pair is silently reduced to one member; on m = 1 it puts the two members
// at 0.906 and 0.423, which is about 8.8 dB of beat depth in the tail. It also
// has to be a fixed nominal rather than a Humanise-scaled draw, because
// Humanise scales its deviations to nothing at zero and that is the setting
// every measurement of this engine is taken at.
constexpr float nominalStrikeAzimuth = 25.0f * (pi / 180.0f);

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

// MIDI velocity as the analogue front end sees it. Named because two things
// need it: the voice being built, and the strike depth below, which has to
// evaluate the same two curves at a velocity that is not the one being played.
float accentVoltage (float velocity) noexcept
{
    return velocity * (0.68f + 0.32f * std::sqrt (velocity));
}

float excitationScaleFor (float velocity) noexcept
{
    return 0.74f + 0.26f * std::sqrt (velocity);
}

// Hertz's contact-time law for an elastic impact: duration falls with impact
// velocity raised to a negative power, so a harder strike is on the surface
// for less time and its force spectrum reaches higher. Every struck voice
// applies the same law at note-on with its own base duration and its own
// exponent - shallower for a soft beater on a slack head, steeper for a hard
// tip on a tight one - so this is the one place that shape is written down.
// Velocity is floored so a note-on of zero cannot make the contact infinite.
float hertzianContactSeconds (float baseSeconds, float velocity, float exponent) noexcept
{
    return baseSeconds * std::pow (std::max (0.08f, velocity), exponent);
}

// Where the drawn pitch sweep is fully used. A head is stiff because it is
// stretched, so the amount the pitch bends follows the energy the strike put
// into it rather than the panel knob alone - but a knob that only reached its
// marked value on a velocity-127 hit would be a knob nobody could set, so the
// depth saturates a little below the top of the range and every accent keeps
// the sweep it has today.
//
// Driving the bend from a running estimate of the system's energy, rather than
// from a solved nonlinear membrane, follows Avanzini and Marogna's
// energy-estimation approach to tension modulation
// (https://pubmed.ncbi.nlm.nih.gov/22280712/). That is what makes it
// affordable inside a thirteen-voice kit, and the same argument sets how deep
// the sweep goes: the depth is the square of what the strike leaves in the
// head, latched at note-on rather than followed.
constexpr float sweepSaturationVelocity = 0.85f;

// accentVoltage() and excitationScaleFor() of a fixed velocity are themselves
// fixed, so their product is the same on every note-on. initialiseVoice()
// used to re-evaluate both curves - two sqrt() calls - for every trigger of
// every voice in the kit; resolving the constant once here instead saves
// that work where the answer never changes.
const float sweepSaturationAmplitude = accentVoltage (sweepSaturationVelocity)
    * excitationScaleFor (sweepSaturationVelocity);

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

// A free bar's bending modes go as the square of 3.011, 5, 7, 9 - so
// 1 : 2.76 : 5.40 : 8.93, spreading fast where a membrane's crowd together.
// These are the boundary conditions, not something a maker or a knob can move.
constexpr std::array<float, 4> percBarRatios {{ 1.0f, 2.756f, 5.404f, 8.933f }};
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

// Only applyPan() below calls these now, always with the value it just
// clamped to [-1, 1] itself, so the two functions trust that contract
// instead of reclamping a result their one caller just computed.
float constantPowerLeft (float clampedPan) noexcept
{
    return std::sqrt (0.5f * (1.0f - clampedPan));
}

float constantPowerRight (float clampedPan) noexcept
{
    return std::sqrt (0.5f * (1.0f + clampedPan));
}

// Every one of the four call sites above - a triggered voice, a sympathetic
// bed's initial configuration, its per-block pan refresh, and the per-block
// mixer target update - clamped pan to [-1, 1] into a local of its own and
// then spelled out the same two constant-power assignments. One shared
// helper now does the clamp once and writes both channels, with no change to
// what any of them compute.
void applyPan (float pan, float& panLeft, float& panRight) noexcept
{
    const float clampedPan = std::clamp (pan, -1.0f, 1.0f);
    panLeft = constantPowerLeft (clampedPan);
    panRight = constantPowerRight (clampedPan);
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
// Antiderivative anti-aliasing of the nonlinear stages follows Gabrielli and
// Squartini's 2025 ADAA study, which motivates it as a lower-cost route to
// reduced aliasing than oversampling the stage:
// https://www.dafx.de/paper-archive/2025/DAFx25_paper_30.pdf
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

// Both ADAA forms below open with the identical pair of steps: clamp the new
// sample to the shaper's finite domain, falling a non-finite one back to
// zero, then clamp the carried-over previous sample the same way, falling a
// non-finite one back to the (already-sanitized) new sample rather than to
// zero - a stage that has just recovered from a NaN should not see a
// synthetic discontinuity against silence. Both results are flushed for
// denormals before use. It was duplicated verbatim in each function; this is
// that one sequence, shared, with no change to either function's output.
void sanitizeAdaaPair (float& input, float& previous) noexcept
{
    input = flushDenormal (
        std::clamp (std::isfinite (input) ? input : 0.0f, -64.0f, 64.0f));
    previous = flushDenormal (
        std::clamp (std::isfinite (previous) ? previous : input, -64.0f, 64.0f));
}

float antialiasedRationalShaper (float input, float& previousInput,
                                 float positiveCurvature,
                                 float negativeCurvature) noexcept
{
    float previous = previousInput;
    sanitizeAdaaPair (input, previous);
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
    float previous = previousInput;
    sanitizeAdaaPair (input, previous);
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

// How fast a hand takes a cymbal down, from how hard it is pressing. A choke
// is a contact damper: what it removes per cycle goes with the area in contact
// and the force behind it, and both of those rise together as the hand closes,
// so the time constant falls roughly as the square of the pressure rather than
// linearly. A full grab is six milliseconds, which is a cymbal stopping; a
// finger laid on the edge is nearly half a second, which is a cymbal being
// shortened.
float chokeSecondsForPressure (float pressure) noexcept
{
    const float grip = std::clamp (pressure, 0.0f, 1.0f);
    const float slack = 1.0f - grip;
    return 0.006f + 0.46f * slack * slack;
}

// The RBJ cookbook's high-pass, band-pass and low-pass sections share
// everything except their numerator: the same frequency/Q clamp, the same
// omega, and the a1/a2 denominator that follows from cosine/alpha/inverseA0.
// This is that shared core, so each configure*() below only has to state its
// own b0/b1/b2 and its own a1/a2 line.
struct TwoPoleBasis
{
    float cosine;
    float alpha;
    float inverseA0;
};

TwoPoleBasis twoPoleBasis (float frequency, float q, float sampleRate,
                           float inverseSampleRate) noexcept
{
    frequency = std::clamp (frequency, 10.0f, 0.45f * sampleRate);
    q = std::clamp (q, 0.15f, 20.0f);
    const float omega = twoPi * frequency * inverseSampleRate;
    const float cosine = std::cos (omega);
    const float alpha = std::sin (omega) / (2.0f * q);
    return { cosine, alpha, 1.0f / (1.0f + alpha) };
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

std::optional<MidiTrigger> midiTriggerForNote (int midiNote) noexcept
{
    switch (midiNote)
    {
        case 35: case 36: return MidiTrigger { Instrument::Kick, Articulation::Head };
        case 38: return MidiTrigger { Instrument::Snare, Articulation::Head };
        // 40 and 37 are General MIDI's Electric Snare and Side Stick, and are
        // what every electronic kit and every mainstream drum instrument sends
        // for a rimshot and a cross-stick. They used to be a second name for a
        // plain snare and a second name for the claves.
        case 40: return MidiTrigger { Instrument::Snare, Articulation::Rimshot };
        case 37: return MidiTrigger { Instrument::Snare, Articulation::CrossStick };
        case 39: return MidiTrigger { Instrument::Clap, Articulation::Head };
        case 42: return MidiTrigger { Instrument::ClosedHat, Articulation::Head };
        // General MIDI 44 is Pedal Hi-Hat, which is a foot chick and not a
        // stick hit. It used to be a second name for note 42, which is the same
        // class of mis-alias notes 37 and 40 used to carry.
        case 44: return MidiTrigger { Instrument::ClosedHat, Articulation::FootChick };
        case 46: return MidiTrigger { Instrument::OpenHat, Articulation::Head };
        case 51: case 59: return MidiTrigger { Instrument::Ride, Articulation::Head };
        // 53 is Ride Bell, 52 Chinese Cymbal and 55 Splash Cymbal. 53 used to
        // be a third name for a plain bow strike; 52 and 55 were silent.
        case 53: return MidiTrigger { Instrument::Ride, Articulation::Bell };
        case 49: case 57: return MidiTrigger { Instrument::Crash, Articulation::Head };
        case 52: return MidiTrigger { Instrument::Crash, Articulation::China };
        case 55: return MidiTrigger { Instrument::Crash, Articulation::Splash };
        case 41: case 43: case 45: return MidiTrigger { Instrument::LowTom, Articulation::Head };
        case 47: case 48: return MidiTrigger { Instrument::MidTom, Articulation::Head };
        case 50: return MidiTrigger { Instrument::HighTom, Articulation::Head };
        case 70: case 82: return MidiTrigger { Instrument::Shaker, Articulation::Head };
        case 56: return MidiTrigger { Instrument::Perc1, Articulation::Head };
        case 75: case 76: case 77: return MidiTrigger { Instrument::Perc2, Articulation::Head };
        default: return std::nullopt;
    }
}

std::optional<Instrument> instrumentForMidiNote (int midiNote) noexcept
{
    const auto trigger = midiTriggerForNote (midiNote);
    return trigger.has_value() ? std::optional<Instrument> { trigger->instrument }
                               : std::nullopt;
}

float velocityFromMidi (int velocityByte, std::optional<int> highResolutionLsb) noexcept
{
    const int coarse = std::clamp (velocityByte, 0, 127);
    if (! highResolutionLsb.has_value())
        return static_cast<float> (coarse) / 127.0f;
    const int fine = std::clamp (*highResolutionLsb, 0, 127);
    return static_cast<float> (coarse * 128 + fine) / 16383.0f;
}

void HighResolutionVelocityPrefix::set (int channel, int lowBits) noexcept
{
    if (! valid (channel))
        return;
    pending_[static_cast<std::size_t> (channel)] = std::clamp (lowBits, 0, 127);
}

std::optional<int> HighResolutionVelocityPrefix::take (int channel) noexcept
{
    if (! valid (channel))
        return std::nullopt;
    auto& slot = pending_[static_cast<std::size_t> (channel)];
    const auto prefix = slot;
    slot.reset();
    return prefix;
}

void HighResolutionVelocityPrefix::clear (int channel) noexcept
{
    if (valid (channel))
        pending_[static_cast<std::size_t> (channel)].reset();
}

void HighResolutionVelocityPrefix::clearAll() noexcept
{
    for (auto& slot : pending_)
        slot.reset();
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

void DrumEngine::Resonator::setTension (float relativeFrequencyChange) noexcept
{
    // The clamp is the pole's own geometry: a1 is 2 r cos(theta) for some
    // angle, so it can never leave [-2r, 2r] whatever the linearisation says at
    // the top of the band. Staying inside it is what keeps the mode a pair of
    // conjugate poles at radius r rather than two real ones.
    a1 = std::clamp (nominalA1 + tensionSlope * relativeFrequencyChange,
                     -poleDiameter, poleDiameter);
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
    // The shared bus compressor uses a peak detector with a fast musical
    // attack and a slow release, so it glues a kit without pumping on hats.
    busAttackCoefficient_ = 1.0f - std::exp (
        -1.0f / std::max (1.0f, 0.004f * floatSampleRate));
    busReleaseCoefficient_ = 1.0f - std::exp (
        -1.0f / std::max (1.0f, 0.140f * floatSampleRate));
    for (int i = 0; i < sineTableSize; ++i)
        sineTable_[static_cast<std::size_t> (i)] = std::sin (
            twoPi * static_cast<float> (i) / static_cast<float> (sineTableSize));
    // Force the mask to exist now. It is built on first use, and first use
    // would otherwise be the first cymbal note-on - which happens on the audio
    // thread, where a table build has no business running. Every later call is
    // a load of an already-initialised static.
    (void) cymbalRoms();

    prepared_ = true;
    reset();
}

const DrumEngine::CymbalRoms& DrumEngine::cymbalRoms() noexcept
{
    // Built once for the process, not once per engine and emphatically not
    // once per prepare(). The contents do not depend on the host sample rate -
    // they are designed at the machine's own sample clock, and retuning is
    // what moves them - so rebuilding them would be recomputing a constant.
    // A mask ROM is shared by every voice in a machine; this is shared by
    // every engine in the process, for the same reason.
    static const CymbalRoms roms = [] () noexcept
    {
    CymbalRoms built;
    // What a 909 cymbal ROM holds is a recorded cymbal with its envelope
    // divided out, so that six bits can be spent on waveform rather than on
    // level. Divide the envelope out of a real cymbal and what is left is not
    // a tone: it is a dense, stationary, inharmonic wash with a handful of
    // strong low partials under it - the plate's first few modes surviving as
    // pitch while everything above them has already smeared into noise.
    //
    // That is what is built here: shaped noise for the wash, a small set of
    // inharmonic partials for the clang. No recording is embedded, and none of
    // this comes from the 808's oscillator bank - on the hardware the two
    // machines share nothing, and the whole point of the digital channel is
    // that it is a different sound source, not a different filter.
    //
    // The tables are fixed data, so the design rate is the nominal sample
    // clock rather than the host rate. Retuning moves the clock and carries
    // the whole spectrum with it, which is the machine's only pitch control.
    struct RomSpec
    {
        std::array<float, cymbalRomSize>* table;
        float clockRate;
        std::uint32_t seed;
        float washCentre;
        float washWidth;
        float partialLow;
        float partialHigh;
        float partialLevel;
        int partialCount;
    };
    // The wash carries these sounds and the partials only colour them. A 909
    // cymbal is remembered as sizzle, not as pitch, and the ROM has to be
    // weighted accordingly: partials loud enough to keep the plate from
    // sounding like filtered hiss, quiet enough that they do not eat the full
    // scale the wash needs.
    const std::array<RomSpec, 2> specs { {
        // The ride is the tighter, more periodic of the two: fewer partials,
        // placed higher, over a narrower wash.
        { &built.ride, 30000.0f, 0x9E3779B9u, 8600.0f, 2.30f, 620.0f, 5200.0f, 0.24f, 18 },
        // The crash is the bright one. Its wash sits high and wide, its
        // partials are quiet enough to colour rather than pitch it, and its
        // clock runs faster than the ride's - which also lifts the
        // reconstruction filter's corner, so more of that top survives the
        // trip out of the converter.
        { &built.crash, 31000.0f, 0x85EBCA6Bu, 11200.0f, 3.15f, 520.0f, 5400.0f, 0.15f, 30 },
    } };

    for (const auto& spec : specs)
    {
        auto& table = *spec.table;
        const float nyquist = 0.5f * spec.clockRate;

        // ---------------------------------------------------------- wash --
        // Deterministic white noise, then shaped. The shaping runs over the
        // table twice and only the second lap is kept, so the filter state at
        // the end of the table is the state it had at the start: the wash
        // loops without a seam, which a straight single pass would not.
        for (int i = 0; i < cymbalRomSize; ++i)
            table[static_cast<std::size_t> (i)] = signedUnitFromHash (
                hash32 (spec.seed + static_cast<std::uint32_t> (i)));

        // A broad resonant band-pass is enough: a cymbal's wash has no
        // features worth more than this once its envelope is gone.
        const float omega = twoPi * std::min (spec.washCentre, 0.45f * spec.clockRate)
            / spec.clockRate;
        const float sinOmega = std::sin (omega);
        const float alpha = sinOmega
            * std::sinh (0.5f * std::log (2.0f) * spec.washWidth * omega
                         / std::max (1.0e-6f, sinOmega));
        const float inverseA0 = 1.0f / (1.0f + alpha);
        const float b0 = alpha * inverseA0;
        const float b2 = -b0;
        const float a1 = -2.0f * std::cos (omega) * inverseA0;
        const float a2 = (1.0f - alpha) * inverseA0;

        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        for (int lap = 0; lap < 2; ++lap)
        {
            for (int i = 0; i < cymbalRomSize; ++i)
            {
                const auto index = static_cast<std::size_t> (i);
                const float input = table[index];
                const float output = b0 * input + b2 * x2 - a1 * y1 - a2 * y2;
                x2 = x1;
                x1 = input;
                y2 = y1;
                y1 = output;
                if (lap == 1)
                    table[index] = output;
            }
        }

        // ------------------------------------------------------ partials --
        // Frequencies are quantized to a whole number of cycles per table, so
        // every partial closes on itself at the wrap and the loop stays
        // seamless. Inharmonic by construction: a cymbal's modes are not a
        // series, and spacing them by an irrational-ish walk keeps any two of
        // them from beating into a pitch.
        const float cyclesPerHertz = static_cast<float> (cymbalRomSize) / spec.clockRate;
        std::uint32_t state = spec.seed ^ 0xC2B2AE35u;
        for (int partial = 0; partial < spec.partialCount; ++partial)
        {
            state = hash32 (state);
            const float position = static_cast<float> (partial)
                / static_cast<float> (std::max (1, spec.partialCount - 1));
            // Geometric spread across the partial band, jittered so the set
            // never lines up into a harmonic series.
            const float jitter = 1.0f + 0.34f * signedUnitFromHash (state);
            const float frequency = std::clamp (
                spec.partialLow * std::pow (spec.partialHigh / spec.partialLow, position)
                    * jitter,
                20.0f, 0.94f * nyquist);
            const auto cycles = static_cast<float> (std::max (1,
                static_cast<int> (std::lround (frequency * cyclesPerHertz))));
            state = hash32 (state);
            const float phase = signedUnitFromHash (state);
            // The low modes of a plate are the loud ones and the ones that
            // survive; above them the partials are only there to thicken.
            const float amplitude = spec.partialLevel
                / (1.0f + 5.0f * position * position);

            for (int i = 0; i < cymbalRomSize; ++i)
                table[static_cast<std::size_t> (i)] += amplitude * std::sin (
                    twoPi * (cycles * static_cast<float> (i)
                             / static_cast<float> (cymbalRomSize) + phase));
        }

        // ----------------------------------------------------- normalise --
        // The ROM is stored at full scale, which is the entire reason the
        // machine gets away with six bits: the quantizer always sees a signal
        // using its whole range, and the envelope that makes it a cymbal is
        // applied afterwards by the VCA.
        float peak = 0.0f;
        for (const float sample : table)
            peak = std::max (peak, std::abs (sample));
        const float normalise = peak > 1.0e-6f ? 0.985f / peak : 0.0f;
        for (float& sample : table)
            sample *= normalise;
    }
        return built;
    }();
    return roms;
}

void DrumEngine::reset() noexcept
{
    forEachVoice ([] (Voice& voice) { voice = Voice {}; });
    triggerCounters_.fill (0);
    componentDrift_.fill (0.0f);
    engineSamples_ = 0;
    hiHatPedal_ = 0.0f;
    hiHatPedalActive_ = false;
    metallicBankVoiceCounts_.fill (0);
    metallicBankMask_ = 0u;
    resetMetallicOscillatorBanks();
    anyVoiceActive_ = false;
    generation_ = 0;
    smoothedOutputGain_ = outputGain_.load (std::memory_order_relaxed);
    smoothedBusDrive_ = busDrive_.load (std::memory_order_relaxed);
    smoothedBusCompression_ = busCompression_.load (std::memory_order_relaxed);
    smoothedBleed_ = bleed_.load (std::memory_order_relaxed);
    bleedExcitation_ = 0.0f;
    configureSympatheticBeds();
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
    {
        ++rejectedSetInstrumentParametersCount_;
        return;
    }
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
    bleed_.store (clampUnit (values.bleed, 0.0f), std::memory_order_relaxed);
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

float DrumEngine::getNewestVoicePitchHz() const noexcept
{
    return newestVoicePitch_.load (std::memory_order_relaxed);
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

float DrumEngine::advanceXorshiftNoise (std::uint32_t& state) noexcept
{
    std::uint32_t x = state;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    state = x == 0u ? 1u : x;
    return static_cast<float> (state & 0x00ffffffu) / 8388607.5f - 1.0f;
}

float DrumEngine::nextNoise (Voice& voice) noexcept
{
    return advanceXorshiftNoise (voice.noiseState);
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

    // At or above the reference rate the grid runs no faster than the output
    // and one interpolated read is the whole answer.
    if (bandLimitedNoiseIncrement_ <= 1.0f)
    {
        const float output = voice.bandLimitedNoiseCurrent
            + voice.bandLimitedNoisePhase
                  * (voice.bandLimitedNoiseNext - voice.bandLimitedNoiseCurrent);
        voice.bandLimitedNoisePhase += bandLimitedNoiseIncrement_;
        while (voice.bandLimitedNoisePhase >= 1.0f)
        {
            voice.bandLimitedNoisePhase -= 1.0f;
            voice.bandLimitedNoiseCurrent = voice.bandLimitedNoiseNext;
            voice.bandLimitedNoiseNext = nextNoise (voice);
        }
        return output;
    }

    // Below it the grid runs faster, and values would otherwise be thrown away.
    // Averaging what the output sample actually spans is what decimation means,
    // and it does two things at once: it puts the same noise density per hertz
    // at every rate - white noise of unchanged per-sample size crammed into a
    // narrower band is a denser noise, by the ratio of the rates, which was
    // 7.8 dB of surplus hiss at 8 kHz - and it leaves the low-rate noise a
    // low-passed copy of the same sequence rather than an unrelated one, so a
    // filtered noise layer sounds like itself there.
    //
    // Each value is weighted by how much of the interval it covers rather than
    // by having been crossed. At 47999 Hz the interval crosses one boundary but
    // barely enters the value beyond it; counting both equally would halve the
    // noise the moment the host dropped under 48 kHz.
    float remaining = bandLimitedNoiseIncrement_;
    float integral = 0.0f;
    while (remaining > 0.0f)
    {
        const float span = std::min (remaining, 1.0f - voice.bandLimitedNoisePhase);
        integral += span * voice.bandLimitedNoiseCurrent;
        remaining -= span;
        voice.bandLimitedNoisePhase += span;
        if (voice.bandLimitedNoisePhase >= 1.0f)
        {
            voice.bandLimitedNoisePhase = 0.0f;
            voice.bandLimitedNoiseCurrent = voice.bandLimitedNoiseNext;
            voice.bandLimitedNoiseNext = nextNoise (voice);
        }
    }
    return integral / bandLimitedNoiseIncrement_;
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
            pushMetallicSubstep (bank);
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
        pushMetallicSubstep (bank);
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
    // The six Schmitt-trigger inverter oscillators of the classic analogue
    // cymbal source, at their measured nominal frequencies. Four of them are
    // hardwired by their own resistor and capacitor; the last two are set by
    // trimpots reachable only with the machine open, which is why the 800/540
    // pair is also what the cowbell is built from and why no two units agree
    // about them. The tolerance depths below say the same thing: a few tenths
    // of a percent on the fixed four, a couple of percent on the trimmed pair,
    // fixed per virtual unit rather than redrawn per hit.
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
            // A real machine has one cymbal channel, so its ride and its crash
            // would be the same sound. Two cymbals on a kit are not, and the
            // cheapest honest way to have both is a second board with the
            // whole oscillator set trimmed down together.
            frequency = cymbalFrequencies[index]
                * (instrument == Instrument::Crash ? 0.94f : 1.0f);
            // Fixed per unit, and deliberately not a control. These are the
            // component tolerances of six oscillators on a board: four set by
            // their own R and C, two by trimpots reachable only with the case
            // open. Nothing on the panel moves them.
            //
            // They used to read characterA, back when that was the Crash's
            // Spread. It is Machine now, so leaving the dependency would have
            // meant choosing a machine also retuned the analogue one - and
            // because this bank is shared engine state rather than per-voice,
            // the per-hit tolerance on characterA would have retuned crash
            // tails that were still ringing.
            toleranceDepth = oscillatorIndex < 4
                ? (instrument == Instrument::Crash ? 0.0063f : 0.004f)
                : (instrument == Instrument::Crash ? 0.0250f : 0.020f);
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

float DrumEngine::pushMetallicSubstep (RelaxationOscillatorBank& bank) noexcept
{
    // The one thing every caller that advances a bank's circuit does with the
    // result: write it into the ring buffer the reconstruction filter reads
    // and step the write index, wrapping it back to the start. Resetting a
    // bank, waking one from a frozen gap, and the engine's own per-sample
    // render loop each used to spell this out separately.
    const float sample = renderMetallicBankSubstep (bank);
    bank.decimatorHistory[static_cast<std::size_t> (bank.decimatorWriteIndex)] = sample;
    if (++bank.decimatorWriteIndex >= maximumMetallicDecimatorTaps)
        bank.decimatorWriteIndex = 0;
    return sample;
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
            latestSource = pushMetallicSubstep (bank);

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

float DrumEngine::swingVcaGain (float control, float knee) noexcept
{
    // The 808's "swing type" VCA is a steered pair, not a multiplier. Its gain
    // is not the control voltage: the steering transistor only begins to hand
    // signal across once the control has climbed past its own base-emitter
    // drop, so the law is superlinear at the bottom and straightens out above
    // it. That is why an 808 cymbal's decay does not sound like an exponential
    // - the envelope is one, and the VCA bends it into something that hangs on
    // and then leaves quickly.
    //
    // Normalised so a fully open control is unity gain: the knee changes the
    // shape of the decay, not the level of the hit.
    control = std::max (0.0f, control);
    knee = std::max (1.0e-4f, knee);
    return control * control * (1.0f + knee) / (control + knee);
}

DrumEngine::CymbalBands DrumEngine::renderCymbalBands (Voice& voice,
                                                       float source) noexcept
{
    auto& channel = voice.cymbal;

    // The attack smoother. A trigger pulse arrives as an edge, but what
    // reaches the envelope capacitors is that edge through an RC, so every
    // band opens over a ramp. It is a small thing that is audible on every
    // hit: without it the first sample of a cymbal is a step into a resonant
    // band-pass, which clicks.
    channel.gate = flushDenormal (
        channel.gate + channel.gateCoefficient * (1.0f - channel.gate));

    // What the stick could deliver into the plate at all, ahead of the
    // sections that decide what the machine does with it. The oscillator bank
    // free-runs whether or not anything was struck, so the contact tilt has to
    // sit between the bank and the band-passes rather than across the finished
    // voice: put on the output it would drag the onset the smoother above just
    // shaped, and put on the envelope it would not be a spectrum at all.
    const float excitation = channel.contactAnalogue.tick (source);

    // Two active band-passes hang off the oscillator summing node. Their
    // centres are the measured 808 values; the multiple-feedback topology puts
    // the Q where these do. Between them they throw away the six oscillators'
    // fundamentals and keep the intermodulation above them, which is the whole
    // trick - six squares in the low hundreds of hertz become a metallic
    // spectrum three octaves higher.
    const float lowBand = channel.bandLow.tick (excitation);

    // Three swing VCAs on two envelope generators. The 808 gives the low band
    // the long one - the DECAY pot is on that capacitor - and the top of the
    // spectrum a short one, so the channel darkens as it rings for the same
    // reason a real cymbal does, and the panel control moves the part of it
    // that lasts.
    const float peak = channel.peak * channel.gate;
    const float lowControl = peak * voice.envelope;

    // A steered pair also leaks its control voltage into the signal path. It
    // is inaudible as a tone and audible as the envelope's own shape riding
    // under the band, which is part of why the machine thumps slightly on the
    // low band of a hard hit.
    CymbalBands bands;
    bands.low = channel.highpassLow.tick (
        swingVcaGain (lowControl, channel.vcaKnee) * lowBand
        + channel.feedthrough * lowControl);

    // Once a VCA is shut past the point where its band could reach -150 dB,
    // the section feeding it is doing arithmetic nobody can hear. Because the
    // swing law squares its control, the envelope only has to fall 90 dB for
    // the gain to be 180 dB down.
    const bool midActive = voice.ageSamples < channel.midActiveSamples;
    const bool highActive = voice.ageSamples < channel.highActiveSamples;
    if (! midActive && ! highActive)
        return bands;

    const float highBand = channel.bandHigh.tick (excitation);
    if (midActive)
        bands.mid = channel.highpassMid.tick (
            swingVcaGain (peak * voice.pitchEnvelope, channel.vcaKnee) * highBand);
    if (highActive)
        bands.high = channel.highpassHigh.tick (
            swingVcaGain (peak * voice.auxiliaryEnvelope, channel.vcaKnee) * highBand);
    return bands;
}

float DrumEngine::boardDriftAt (std::uint64_t sampleIndex) const noexcept
{
    // Rail voltage and board temperature wander with time, not with how many
    // notes have been played - a machine left switched on drifts whether or
    // not anything is triggering. Deriving this from the engine clock rather
    // than from a trigger counter is what keeps it honest, and it is also what
    // keeps one voice's sound from depending on another voice's history.
    //
    // A smooth wander between hash-derived corners a few seconds apart: slow
    // enough that consecutive hits agree with each other, fast enough that a
    // long take does not sit at one offset.
    const double period = std::max (1.0, 2.9 * sampleRate_);
    const double position = static_cast<double> (sampleIndex) / period;
    const auto corner = static_cast<std::uint64_t> (position);
    const auto fraction = static_cast<float> (position - static_cast<double> (corner));
    const auto cornerHash = [] (std::uint64_t index) noexcept
    {
        return signedUnitFromHash (hash32 (
            static_cast<std::uint32_t> (index) ^ 0x27d4eb2fu));
    };
    const float from = cornerHash (corner);
    const float to = cornerHash (corner + 1u);
    const float smooth = fraction * fraction * (3.0f - 2.0f * fraction);
    return from + (to - from) * smooth;
}

float DrumEngine::companding6BitDac (float value) noexcept
{
    // The TR-909 stores its cymbals as six bits per sample and gets away with
    // it by not storing them linearly: the code is a sign, a two-bit chord and
    // a three-bit step inside that chord, so the quantizer's step doubles with
    // every octave of level. Four chords, eight steps each, both signs - 64
    // codes, and the bottom chord stays linear so small samples do not fall
    // into a dead band.
    //
    // This is a segmented converter, so it is shifts and compares rather than
    // a logarithm, exactly as the hardware is.
    const float magnitude = std::min (1.0f, std::abs (value));
    const float sign = value < 0.0f ? -1.0f : 1.0f;
    float base = 0.0f;
    float step = 1.0f / 64.0f;
    if (magnitude >= 0.5f)
    {
        base = 0.5f;
        step = 1.0f / 16.0f;
    }
    else if (magnitude >= 0.25f)
    {
        base = 0.25f;
        step = 1.0f / 32.0f;
    }
    else if (magnitude >= 0.125f)
    {
        base = 0.125f;
        step = 1.0f / 64.0f;
    }
    const float code = std::floor ((magnitude - base) / step);
    return sign * (base + step * (std::min (7.0f, code) + 0.5f));
}

float DrumEngine::nextCymbalPcm (Voice& voice) const noexcept
{
    auto& channel = voice.cymbal;
    if (channel.rom == nullptr)
        return 0.0f;

    // A counter walks the ROM at the sample clock - about 30 kHz nominal, and
    // tunable, because the clock is what the TUNE control moves. Drumalor
    // generates what the counter reads instead of embedding a recording, but
    // the rest of the path is the machine's: the stored waveform carries no
    // envelope of its own, so it is quantized at full scale and the VCA below
    // puts the decay back. That is why 909 cymbals are gritty at the start and
    // clean at the end rather than dissolving into a fixed noise floor: the
    // quantization error decays with the sound because it is multiplied by the
    // same envelope.
    channel.clockPhase += channel.clockIncrement;
    if (channel.clockPhase >= 1.0f)
    {
        channel.clockPhase -= std::floor (channel.clockPhase);

        // One address per clock, and nothing between addresses: the counter
        // steps, the ROM answers, the DAC holds. Reading the table with a
        // nearest-address lookup rather than interpolating is the point - the
        // machine has no interpolator, and the aliasing that step produces is
        // part of why its cymbals sound the way they do.
        channel.romPhase += 1.0f;
        if (channel.romPhase >= static_cast<float> (cymbalRomSize))
            channel.romPhase -= static_cast<float> (cymbalRomSize);
        const auto address = static_cast<int> (channel.romPhase) & cymbalRomMask;
        channel.hold = companding6BitDac (
            channel.rom[static_cast<std::size_t> (address)]);

        // A second DAC reads the address lines and an anti-log converter turns
        // its ramp into the envelope. Because it steps with the counter and
        // not with a clock of its own, the decay always finishes exactly where
        // the ROM does - retune the machine and pitch and length move together.
        channel.romEnvelope = flushDenormal (channel.romEnvelope * channel.romDecay);

        // The VCA that restores it is an operational transconductance
        // amplifier, and its control current buys bandwidth as well as gain.
        // A cymbal fading out on one of these does not simply get quieter: it
        // gets duller on the way down, which is most of why a 909 tail sounds
        // like metal leaving a room rather than a fader closing.
        // Accent belongs in this control current too, not only in the level
        // downstream. An OTA's transconductance is set by what is driving it,
        // and a softer hit drives it less - so it is duller as well as quieter,
        // and gives its top up sooner. Leaving velocity out here made it a
        // plain output gain on the digital channel, which is precisely what
        // the analogue channel's swing VCAs are shaped to avoid being.
        const float control = channel.romEnvelope * channel.peak;
        channel.vcaBandwidthCoefficient = channel.vcaBandwidthOpen
            * (channel.vcaBandwidthFloor
               + (1.0f - channel.vcaBandwidthFloor)
                     * std::sqrt (std::max (0.0f, control)));
    }

    // The DAC holds its code until the next clock. What leaves the machine is
    // that staircase through the closing VCA and a low-pass that removes the
    // clock, so the grain survives and the sampling image does not.
    // The contact tilt belongs to the recording rather than to the playback,
    // so it takes the held code before the VCA does and not the finished
    // channel afterwards. The DAC's staircase carries the quantization error
    // with it, and that error is part of this machine's sound, so it is
    // filtered along with the word it rode in on.
    const float held = channel.contactDigital.tick (channel.hold);
    channel.vcaBandwidthState = flushDenormal (
        channel.vcaBandwidthState + channel.vcaBandwidthCoefficient
            * (held * channel.romEnvelope - channel.vcaBandwidthState));
    const float reconstructed = channel.reconstruction.tick (
        channel.vcaBandwidthState);

    // Both channels meet at the tone mixer, so this one is split there as
    // well. One first-order crossover stands in for the three analogue legs a
    // digital channel does not have.
    channel.pcmSplitState = flushDenormal (
        channel.pcmSplitState
        + channel.pcmSplitCoefficient * (reconstructed - channel.pcmSplitState));

    // The trigger smoother, last, on the channel rather than on the address
    // lines: what the RC delays is the envelope voltage reaching the VCA, and
    // the counter is already running by then.
    channel.digitalGate = flushDenormal (
        channel.digitalGate
        + channel.digitalGateCoefficient * (1.0f - channel.digitalGate));
    return channel.digitalGate
        * (channel.pcmLowGain * channel.pcmSplitState
           + channel.pcmHighGain * (reconstructed - channel.pcmSplitState));
}

void DrumEngine::configureCymbalChannel (Voice& voice, Instrument instrument,
                                         float velocity,
                                         float machineSelect,
                                         Articulation articulation) noexcept
{
    const bool ride = instrument == Instrument::Ride;
    auto& channel = voice.cymbal;
    const auto floatSampleRate = static_cast<float> (sampleRate_);

    // The three voiced variants. Neither machine has a modal bank, so none of
    // these can be a strike position: what a cymbal note can honestly change
    // here is what the two machines are set to, and that is what these do.
    //
    // A bell is the thick, stiff, doubly-curved dome at the middle of a ride.
    // It is a stiffer radiator than the bow around it, so it sits higher, it
    // holds a far narrower band, and - having very little area to lose energy
    // through - it rings longer than the bow it grew out of. On this channel
    // that is a faster sample clock, the band gains pulled up onto the top
    // sections, and a longer recording.
    //
    // A china is a crash with its edge turned up. The reversed flange spoils
    // the plate's own symmetry, which is what makes it trashy rather than
    // pitched: broader and lower at the bottom, and gone much sooner.
    //
    // A splash is simply a small crash - ten inches against sixteen - so it is
    // brighter and very much shorter, and there is very little of it below the
    // top two sections.
    struct CymbalVoicing
    {
        float clockRatio { 1.0f };     // how much faster the counter runs
        float decayRatio { 1.0f };     // how long the recording is
        float contactRatio { 1.0f };   // how long the tip is on the bronze
        float lowTilt { 1.0f };        // the 3.44 kHz section
        float midTilt { 1.0f };        // the 7.1 kHz section
        float highTilt { 1.0f };       // above both
        // Where the digital leg's one crossover sits. That leg is 86 % of a
        // crash and 90 % of a ride at the shipping Machine defaults, so the
        // corner between its two gains is the largest tone control either
        // cymbal has, and it is the one that decides where a plate's weight
        // sits rather than only how much of it there is.
        float splitCorner { 3000.0f };
    };
    const CymbalVoicing voicing =
        articulation == Articulation::Bell
            ? CymbalVoicing { 1.46f, 1.85f, 0.68f, 0.24f, 1.14f, 1.66f, 5200.0f }
        : articulation == Articulation::China
            ? CymbalVoicing { 0.74f, 0.42f, 1.35f, 1.86f, 1.24f, 0.52f, 1500.0f }
        : articulation == Articulation::Splash
            ? CymbalVoicing { 1.40f, 0.14f, 0.74f, 0.20f, 0.92f, 1.86f, 6400.0f }
            : CymbalVoicing {};
    // Applied to the voice's own decay rather than to the recording alone, so
    // the analogue channel's three envelopes, the digital channel's address
    // envelope and the band retirement ages all follow one number. This runs
    // ahead of everything in initialiseVoice that reads decaySeconds.
    // The ceiling is the Decay control's own top of range for this instrument.
    // A bell rings longer than the bow it grew out of, but not longer than the
    // longest thing this cymbal is allowed to be: past that the voice would be
    // finished by the engine's eight-second tail bound instead of by its own
    // envelope, which is a fade rather than a decay.
    voice.decaySeconds = std::min (voice.decaySeconds * voicing.decayRatio,
                                   decaySecondsFor (instrument, 1.0f));

    // Hertz's contact law, and the only place it appears on this half of the
    // kit. An elastic impact lasts for the impact speed to the power -1/5, so
    // a faster stick is on the bronze for less time. That single law sets two
    // different numbers here, and they are not the same number: how quickly
    // the trigger opens the channel, and how far up the plate the strike
    // reaches. Collapsing them would put a contact time of about a
    // millisecond on the spectrum, whose corner at 1/(2*tau) would be a few
    // hundred hertz - which does not soften a cymbal, it deletes it.
    const float velocityStretch = std::pow (
        std::max (0.08f, std::clamp (velocity, 0.0f, 1.0f)), -0.2f);

    // ---------------------------------------------------------------- 808 --
    // The band-pass centres are the measured ones. They track Pitch only
    // weakly: these are RC sections on a board, so transposing the machine
    // moves its oscillators far more than it moves the filters they feed.
    const float filterPitch = std::pow (voice.pitchRatio, 0.40f);
    configureBandpass (channel.bandLow, 3440.0f * filterPitch, ride ? 4.20f : 3.30f);
    configureBandpass (channel.bandHigh, 7100.0f * filterPitch, ride ? 3.00f : 2.40f);
    // A multiple-feedback section is not a unit-gain filter: its midband gain
    // is the ratio of the feedback resistor to the input one, and it is large,
    // because what these sections have to lift out of the summing node is the
    // intermodulation between six squares rather than the squares themselves.
    const auto applyMidbandGain = [] (Biquad& filter, float gain) noexcept
    {
        filter.b0 *= gain;
        filter.b1 *= gain;
        filter.b2 *= gain;
    };
    applyMidbandGain (channel.bandLow, 6.4f);
    applyMidbandGain (channel.bandHigh, 5.6f);

    // One Sallen-Key high-pass per band ahead of the tone mixer. Their job is
    // to take the low end off each VCA, which is where the six oscillators'
    // own fundamentals would otherwise survive as a hum under the metal.
    configureHighpass (channel.highpassLow, 1500.0f * filterPitch, 0.72f);
    configureHighpass (channel.highpassMid, 4200.0f * filterPitch, 0.72f);
    configureHighpass (channel.highpassHigh, 8000.0f * filterPitch, 0.72f);

    // The attack smoother between the trigger pulse and the envelope
    // capacitors. Without it the first sample of a cymbal is a step into a
    // resonant band-pass, which clicks; with it the channel opens over a ramp.
    const float attackSeconds = ride ? 0.00085f : 0.00160f;
    channel.gate = 0.0f;
    channel.gateCoefficient = 1.0f - std::exp (
        -1.0f / std::max (1.0f, attackSeconds * floatSampleRate));

    // ACCENT is a voltage on the trigger line, so a hard hit charges the
    // envelope capacitors further and drives the VCAs past their knee instead
    // of merely turning the channel up. That is a timbre change, and it is why
    // a quiet 808 cymbal is softer-edged rather than the same sound quieter.
    channel.peak = 0.58f + 0.42f * std::clamp (velocity, 0.0f, 1.0f);
    channel.vcaKnee = ride ? 0.22f : 0.27f;
    channel.feedthrough = 0.020f;

    // ---------------------------------------------------------------- 909 --
    // A free-running oscillator around 60 kHz, divided by two, is the sample
    // clock; TUNE moves it, and moving it is the only pitch control the
    // machine's cymbals have. Below a host rate that cannot carry it the clock
    // is necessarily limited to the host rate.
    const float nominalClockRate = (ride ? 30000.0f : 31000.0f) * voice.pitchRatio
        * voicing.clockRatio;
    channel.clockIncrement = std::clamp (
        nominalClockRate * inverseSampleRate_, 1.0e-4f, 1.0f);
    const float clockRate = channel.clockIncrement * floatSampleRate;

    // How long a cymbal was recorded into the ROM, and therefore how long the
    // counter takes to walk it. Decay sets the recording; the clock sets the
    // playback, so a transposed cymbal is a shorter one - the address envelope
    // always finishes exactly where the ROM does. The only departure from the
    // hardware is the ceiling, which is the engine's own eight-second limit on
    // how long any voice may ring.
    const float recordedSeconds = voice.decaySeconds * (ride ? 0.72f : 1.15f);
    // From the clock the counter actually runs at, not from the requested
    // pitch. Above roughly +8 semitones at 48 kHz the increment hits its
    // one-address-per-sample ceiling and the channel stops rising in pitch;
    // dividing by the requested ratio past that point would keep shortening
    // the tail after the pitch had stopped moving, so +12 and +24 would sound
    // identical but decay differently. Pitch and tail move together or neither
    // does - that is the whole point of taking the envelope off the address
    // lines - so the ceiling has to apply to both.
    // The variant's own clock offset divides out here: a bell is a different
    // plate rather than a transposed one, so the rate it is nominally read at
    // is its own nominal rate and its recording is not shortened for being a
    // bell. What is left in this ratio is the Pitch control, which is a
    // transposition and does shorten the tail.
    const float effectiveClockRatio = clockRate
        / ((ride ? 30000.0f : 31000.0f) * voicing.clockRatio);
    const float playbackSeconds = std::clamp (
        recordedSeconds / std::max (0.20f, effectiveClockRatio), 0.04f, 7.0f);
    channel.romEnvelope = 1.0f;
    channel.romDecay = std::exp (
        minusSixtyDb / std::max (4.0f, playbackSeconds * clockRate));
    channel.clockPhase = 1.0f;
    channel.hold = 0.0f;

    // The digital channel's trigger RC. The hardware's data path opens in one
    // clock, which is why this leg used to reach full level in three samples
    // and why a brushed ride tip and a crash-ride accent used to have the same
    // onset. The component is the analogue channel's own trigger smoother -
    // same resistor, same capacitor, the value read off that leg at the top of
    // this function - because on this board there is one trigger bus. What is
    // not the same is that this leg has nothing else velocity can reach: the
    // ROM is a fixed recording and the address envelope is walked by the
    // counter, so the swing VCAs' accent knee has no counterpart here. So the
    // contact law rides on the smoother, and a soft hit opens the channel over
    // a longer ramp as well as to a lower level.
    channel.digitalGate = 0.0f;
    channel.digitalGateCoefficient = 1.0f - std::exp (
        -1.0f / std::max (1.0f, attackSeconds * velocityStretch * floatSampleRate));

    // The contact time proper - a wooden tip against something under a
    // millimetre thick, which is the shortest touch anything in the kit makes.
    // Its spectral corner is 1/(2*tau): at full velocity 10.9 kHz, at the
    // softest hit the law is allowed to see 6.6 kHz. Both ends sit above the
    // cymbal's body and below the reconstruction filter, so what this moves is
    // the top of the plate and nothing else. One filter per machine, each on
    // its own carrier ahead of its own envelope, because the contact decides
    // what the strike puts in rather than how what is in gets out.
    const float contactSeconds = 0.000046f * velocityStretch * voicing.contactRatio;
    const float contactCorner = 0.5f / contactSeconds;
    configureOnePoleLowpass (channel.contactAnalogue, contactCorner);
    configureOnePoleLowpass (channel.contactDigital, contactCorner);

    // The low-pass that takes the sample clock back out again. Two poles below
    // half the clock: enough to bury the image, not enough to hide the grain.
    const float reconstructionCorner = std::min (13000.0f, 0.42f * clockRate);
    configureLowpass (channel.reconstruction, reconstructionCorner, 0.72f);
    // The VCA's own pole, wide open. It closes with the control current above.
    channel.vcaBandwidthState = 0.0f;
    channel.vcaBandwidthOpen = std::clamp (
        1.0f - std::exp (-twoPi * 1.6f * reconstructionCorner * inverseSampleRate_),
        1.0e-4f, 1.0f);
    channel.vcaBandwidthFloor = ride ? 0.26f : 0.52f;
    channel.vcaBandwidthCoefficient = channel.vcaBandwidthOpen;

    // The counter is reset by the trigger, so every hit reads the mask from
    // its first address: the ROM contributes nothing to hit-to-hit variation,
    // which is exactly the digital machine's reputation. What varies is the
    // rate it is read at, because the sample clock is a free-running analogue
    // oscillator and drifts with the board like everything else - so two hits
    // are the same recording at very slightly different speeds. The 808's
    // oscillators vary far more, since they free-run and a strike samples them
    // wherever they happen to be rather than restarting them.
    const auto& roms = cymbalRoms();
    channel.rom = ride ? roms.ride.data() : roms.crash.data();
    channel.romPhase = 0.0f;

    // ------------------------------------------------------- tone control --
    // Both machines' outputs meet at this mixer, so this is where the choice
    // between them is made. Machine picks the machine; Tone tilts whichever
    // one is playing.
    //
    // The crossfade is equal-power because the two channels are genuinely
    // uncorrelated - separate sources rather than one source through two
    // filters - so their powers add and a linear fade would sag in the middle.
    channel.pcmSplitState = 0.0f;
    channel.pcmSplitCoefficient = std::clamp (
        1.0f - std::exp (-twoPi * voicing.splitCorner * inverseSampleRate_),
        1.0e-4f, 1.0f);

    const float machine = std::clamp (machineSelect, 0.0f, 1.0f);
    const float analogueMix = std::cos (0.25f * twoPi * machine);
    // The two channels do not arrive at the mixer at the same level - a
    // band-passed oscillator bank and a six-bit converter have no reason to -
    // so the digital leg carries the trim that matches them. Both machines
    // were levelled at their own output stages too; a control that changed
    // loudness rather than character would be useless for choosing between
    // them. Measured across the full sweep, not guessed.
    const float digitalMix = std::sin (0.25f * twoPi * machine)
        * (ride ? 1.39f : 1.88f);
    const float tone = voice.characterB;

    if (ride)
    {
        // The 808 never had a ride, so at the analogue end this is that
        // machine's cymbal channel tuned up and given the shorter decay a ride
        // pattern needs, not an imitation of a ride cymbal.
        channel.lowGain = analogueMix * (1.24f - 0.70f * tone) * voicing.lowTilt;
        channel.midGain = analogueMix * (0.34f + 1.06f * tone) * voicing.midTilt;
        channel.highGain = analogueMix * (0.06f + 1.36f * tone) * voicing.highTilt;
        channel.pcmLowGain = digitalMix * (1.34f - 1.12f * tone) * voicing.lowTilt;
        channel.pcmHighGain = digitalMix * (0.20f + 1.78f * tone) * voicing.highTilt;
    }
    else
    {
        // The analogue machine's cymbal is a splash, not a dark wash: most of
        // what leaves it comes off the 7.1 kHz section, and weighting the two
        // top VCAs below the 3.44 kHz one is what made this read as dull.
        const float brightness = voice.characterB;
        channel.lowGain = analogueMix * (1.02f - 0.44f * brightness) * voicing.lowTilt;
        channel.midGain = analogueMix * (0.40f + 1.16f * brightness) * voicing.midTilt;
        channel.highGain = analogueMix * (0.20f + 1.72f * brightness) * voicing.highTilt;
        channel.pcmLowGain = digitalMix * (1.22f - 0.98f * brightness) * voicing.lowTilt;
        channel.pcmHighGain = digitalMix * (0.28f + 1.46f * brightness) * voicing.highTilt;
    }
}

void DrumEngine::configureHighpass (Biquad& filter, float frequency, float q) const noexcept
{
    const auto basis = twoPoleBasis (frequency, q, static_cast<float> (sampleRate_),
                                     inverseSampleRate_);
    filter.b0 = 0.5f * (1.0f + basis.cosine) * basis.inverseA0;
    filter.b1 = -(1.0f + basis.cosine) * basis.inverseA0;
    filter.b2 = filter.b0;
    filter.a1 = -2.0f * basis.cosine * basis.inverseA0;
    filter.a2 = (1.0f - basis.alpha) * basis.inverseA0;
    filter.clear();
}

void DrumEngine::configureBandpass (Biquad& filter, float frequency, float q) const noexcept
{
    const auto basis = twoPoleBasis (frequency, q, static_cast<float> (sampleRate_),
                                     inverseSampleRate_);
    filter.b0 = basis.alpha * basis.inverseA0;
    filter.b1 = 0.0f;
    filter.b2 = -filter.b0;
    filter.a1 = -2.0f * basis.cosine * basis.inverseA0;
    filter.a2 = (1.0f - basis.alpha) * basis.inverseA0;
    filter.clear();
}

void DrumEngine::configureLowpass (Biquad& filter, float frequency, float q) const noexcept
{
    const auto basis = twoPoleBasis (frequency, q, static_cast<float> (sampleRate_),
                                     inverseSampleRate_);
    filter.b0 = 0.5f * (1.0f - basis.cosine) * basis.inverseA0;
    filter.b1 = (1.0f - basis.cosine) * basis.inverseA0;
    filter.b2 = filter.b0;
    filter.a1 = -2.0f * basis.cosine * basis.inverseA0;
    filter.a2 = (1.0f - basis.alpha) * basis.inverseA0;
    filter.clear();
}

void DrumEngine::configureOnePoleLowpass (Biquad& filter, float frequency) const noexcept
{
    // The bilinear transform rather than the exponential one-pole used
    // elsewhere for envelope smoothing. A corner this close to Nyquist reads
    // several kilohertz high under the exponential mapping and reads a
    // different number at every host rate, and this filter's whole job is to
    // put a stated frequency in a stated place.
    frequency = std::clamp (frequency, 10.0f, 0.45f * static_cast<float> (sampleRate_));
    const float warped = std::tan (0.5f * twoPi * frequency * inverseSampleRate_);
    const float inverseA0 = 1.0f / (1.0f + warped);
    filter.b0 = warped * inverseA0;
    filter.b1 = filter.b0;
    filter.b2 = 0.0f;
    filter.a1 = (warped - 1.0f) * inverseA0;
    filter.a2 = 0.0f;
    filter.clear();
}

void DrumEngine::configureResonator (Resonator& resonator, float frequency,
                                     float decaySeconds) const noexcept
{
    frequency = std::clamp (frequency, 20.0f, 0.45f * static_cast<float> (sampleRate_));
    decaySeconds = std::max (0.005f, decaySeconds);
    const float omega = twoPi * frequency * inverseSampleRate_;
    const float radius = coefficientForTime (decaySeconds, static_cast<float> (sampleRate_));
    // sin(omega) feeds the tension slope, the input gain and the strike gain
    // below; omega does not change between them, so it is one transcendental
    // call rather than three of the identical one.
    const float sineOmega = std::sin (omega);
    resonator.a1 = 2.0f * radius * std::cos (omega);
    resonator.a2 = -radius * radius;
    resonator.nominalA1 = resonator.a1;
    resonator.tensionSlope = -2.0f * radius * omega * sineOmega;
    resonator.poleDiameter = 2.0f * radius;

    // A two-pole resonator's impulse residue is inputGain / sin (omega).
    // Preserve the existing 48 kHz residue while keeping that ratio constant
    // at every sample rate.
    const float referenceFrequency = std::min (frequency, 0.45f * referenceSampleRate);
    const float referenceOmega = twoPi * referenceFrequency / referenceSampleRate;
    const float referenceRadius = coefficientForTime (decaySeconds, referenceSampleRate);
    const float referenceGain = 0.45f * std::sqrt (
        std::max (1.0e-8f, 1.0f - referenceRadius * referenceRadius));
    resonator.inputGain = referenceGain * sineOmega
        / std::max (1.0e-4f, std::sin (referenceOmega));
    // A strike sets the mode moving instead of pushing a sample through it, so
    // its scale is the mode's own geometry and carries no sample rate with it.
    resonator.strikeGain = sineOmega / std::max (1.0e-4f, radius);
    resonator.clear();
}

void DrumEngine::retuneResonatorDecay (Resonator& resonator, float cosine,
                                       float angle, float decaySeconds) const noexcept
{
    // a1 is 2 r cos(theta) and a2 is -r^2, so the pole a mode is currently
    // ringing on gives back both its radius and its angle exactly. Rebuilding
    // the coefficients at a new radius and the same angle changes how fast the
    // mode dies and nothing else - the frequency is preserved to the last bit,
    // and y1/y2 are left alone, which is what makes this a change of law on a
    // note that is still sounding rather than a new note.
    //
    // cosine and angle are that recovered pole, not a new frequency: the sole
    // caller (applyHatAperture) already has to recover them from this same
    // resonator's a1/a2 to turn the angle into a frequency for its own loss
    // law, so it hands them in here instead of this function re-deriving the
    // identical radius/cosine/acos from the coefficients a second time.
    const float radius = coefficientForTime (std::max (0.005f, decaySeconds),
                                             static_cast<float> (sampleRate_));
    const float sine = std::sqrt (std::max (0.0f, 1.0f - cosine * cosine));
    resonator.a1 = 2.0f * radius * cosine;
    resonator.a2 = -radius * radius;
    resonator.nominalA1 = resonator.a1;
    // The slope only needs the product r*omega*sin(omega); omega is the angle
    // the caller already recovered.
    resonator.tensionSlope = -2.0f * radius * angle * sine;
    resonator.poleDiameter = 2.0f * radius;
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

bool DrumEngine::isHiHat (Instrument instrument) noexcept
{
    return instrument == Instrument::ClosedHat || instrument == Instrument::OpenHat;
}

bool DrumEngine::isCymbal (Instrument instrument) noexcept
{
    return instrument == Instrument::Ride || instrument == Instrument::Crash;
}

void DrumEngine::applyAftertouch (float pressure) noexcept
{
    if (! std::isfinite (pressure) || pressure <= 0.0f)
        return;
    const float seconds = chokeSecondsForPressure (pressure);
    forEachVoice ([this, seconds] (Voice& voice)
    {
        if (voice.active && isCymbal (voice.instrument))
            beginChoke (voice, seconds);
    });
}

bool DrumEngine::applyAftertouch (int midiNote, float pressure) noexcept
{
    const auto mapping = midiTriggerForNote (midiNote);
    if (! mapping.has_value() || ! isCymbal (mapping->instrument))
        return false;
    if (! std::isfinite (pressure) || pressure <= 0.0f)
        return true;
    const float seconds = chokeSecondsForPressure (pressure);
    // A hand lands on one cymbal. Two articulations of the same instrument are
    // two pieces of bronze on the stand - Crash 49 and China 52 share a channel
    // strip, as do Ride 51 and Bell 53 - so matching the instrument alone made
    // the note form of aftertouch channel-wide within it, and grabbing the
    // china took the crash with it.
    //
    // The articulation is the identity rather than the note itself, because a
    // note is a name for a cymbal and not a cymbal: General MIDI gives the
    // crash two names (49 and 57) and the ride two (51 and 59), and a player
    // who struck the crash on 57 and grabbed it on 49 has grabbed the cymbal
    // that is ringing.
    forEachVoice ([this, &mapping, seconds] (Voice& voice)
    {
        if (voice.active && voice.instrument == mapping->instrument
            && voice.articulation == mapping->articulation)
            beginChoke (voice, seconds);
    });
    return true;
}

// How far apart the two plates are, from 0 (clamped) to 1 (free). Before any
// controller has touched the pedal the two notes are the two endpoints, exactly
// as they always were; afterwards the pedal decides and the note only chooses
// which channel strip is playing.
float DrumEngine::hiHatAperture (Instrument instrument) const noexcept
{
    if (! hiHatPedalActive_)
        return instrument == Instrument::ClosedHat ? 0.0f : 1.0f;
    return std::clamp (1.0f - hiHatPedal_, 0.0f, 1.0f);
}

float DrumEngine::getHiHatPedal() const noexcept
{
    return hiHatPedalActive_ ? hiHatPedal_ : 0.0f;
}

float DrumEngine::hatDecaySecondsFor (float aperture,
                                      float decayVariation) const noexcept
{
    // How long the pair rings is the pedal's business, and the two channels'
    // own Decay settings are its endpoints. The interpolation is geometric
    // because the control is logarithmic in seconds, so half a pedal is half
    // the travel a listener hears rather than half the number. Both ends are
    // guarded rather than trusted to arrive there: `open + (closed - open) * 1`
    // is not bit-identical to `closed` in float.
    const float closedSeconds = decaySecondsFor (
        Instrument::ClosedHat, snapshotParameters (Instrument::ClosedHat).decay);
    const float openSeconds = decaySecondsFor (
        Instrument::OpenHat, snapshotParameters (Instrument::OpenHat).decay);
    const float clamped = 1.0f - aperture;
    return (clamped >= 1.0f ? closedSeconds
            : clamped <= 0.0f ? openSeconds
            : closedSeconds * std::pow (openSeconds / closedSeconds, aperture))
        * decayVariation;
}

void DrumEngine::applyHatAperture (Voice& voice, float aperture) noexcept
{
    aperture = std::clamp (aperture, 0.0f, 1.0f);
    const float clamped = 1.0f - aperture;
    const auto pedalBlend = [clamped] (float openValue, float closedValue)
    {
        return clamped <= 0.0f ? openValue
             : clamped >= 1.0f ? closedValue
             : openValue + (closedValue - openValue) * clamped;
    };
    const auto floatSampleRate = static_cast<float> (sampleRate_);

    voice.hatAperture = aperture;
    voice.decaySeconds = hatDecaySecondsFor (aperture, voice.decayVariation);
    voice.envelopeMultiplier = coefficientForTime (voice.decaySeconds, floatSampleRate);
    voice.auxiliaryMultiplier = coefficientForTime (
        voice.decaySeconds * pedalBlend (0.42f, 0.78f), floatSampleRate);
    voice.transientMultiplier = coefficientForTime (
        pedalBlend (0.006f, 0.0025f), floatSampleRate);

    // The plate bank follows the same two loss laws the note-on path uses. Only
    // the damping moves: the modes keep the frequencies they are ringing at,
    // because retuning them would be the pair changing pitch under the note
    // rather than changing how fast it dies, and a foot lifting off does not
    // retune a plate that is already moving.
    const ModalLoss loss { pedalBlend (0.55f, 0.86f), pedalBlend (0.30f, 0.10f),
                           pedalBlend (0.15f, 0.04f), 0.0f };
    const float plateSeconds = voice.decaySeconds * pedalBlend (0.70f, 0.85f);
    float longest = 0.0f;
    for (int mode = 0; mode < voice.modeCount; ++mode)
    {
        auto& resonator = voice.resonators[static_cast<std::size_t> (mode)];
        const float radius = std::sqrt (std::max (0.0f, -resonator.a2));
        if (radius <= 1.0e-6f)
            continue;
        const float cosine = std::clamp (
            resonator.a1 / (2.0f * radius), -1.0f, 1.0f);
        const float angle = std::acos (cosine);
        const float frequency = angle * floatSampleRate / twoPi;
        const float relative = std::max (
            1.0f, frequency / std::max (1.0f, voice.baseFrequency));
        const float lossFactor = std::max (
            0.05f, loss.fixed + loss.hysteretic * relative
                       + loss.viscous * relative * relative);
        const float seconds = plateSeconds / lossFactor;
        longest = std::max (longest, seconds);
        // cosine and angle are the pole this mode is already ringing on,
        // recovered above to turn it into a frequency for the loss law; hand
        // them to retuneResonatorDecay instead of letting it recover the same
        // pole from a1/a2 a second time.
        retuneResonatorDecay (resonator, cosine, angle, seconds);
    }

    // The bank stops being evaluated once its slowest mode has passed 2.6 of
    // its own decays, and that deadline was computed at note-on against the
    // decay the voice had then. Opening the pedal on a ringing hat hands the
    // plates back to the open plate's own much longer loss law, so the deadline
    // the note-on left behind now falls inside the sound: renderHat() stopped
    // evaluating the bank partway through the foot splash and cut the plate
    // component out from under it. The new deadline is measured from where the
    // voice actually is rather than from its note-on, and it is only ever
    // extended - closing the pedal shortens the modes, and a shorter mode does
    // not entitle anything to stop early while the longer law is still audible.
    if (longest > 0.0f)
    {
        const auto remaining = static_cast<std::uint64_t> (
            std::ceil (2.6 * static_cast<double> (longest) * sampleRate_)) + 1u;
        voice.modalActiveSamples = std::max (voice.modalActiveSamples,
                                             voice.ageSamples + remaining);
    }
}

void DrumEngine::setHiHatPedal (float position) noexcept
{
    if (! std::isfinite (position))
        return;
    position = std::clamp (position, 0.0f, 1.0f);
    const bool wasActive = hiHatPedalActive_;
    const float previous = wasActive ? hiHatPedal_ : 0.0f;
    hiHatPedalActive_ = true;
    hiHatPedal_ = position;
    const float aperture = 1.0f - position;

    // Closing the pedal on a ringing hat is not a switch being thrown. The two
    // plates come together over the travel of the foot and the friction between
    // their faces takes the sound out of the pair as they meet, so a pedal all
    // the way down cuts a hat in a few milliseconds and one that only half
    // closes leaves it ringing, shorter and duller than it was.
    //
    // A pedal that opens again does not undo it: the energy friction took has
    // gone, and nothing here puts it back. What opening does do is stop the
    // friction, and hand what is left back to the open plate's own loss law -
    // which is what a foot splash is. Both directions therefore re-derive the
    // ringing voice's decay at the new aperture; only the closing direction
    // adds the friction on top, and only the opening direction takes it away.
    forEachVoice ([this, aperture] (Voice& voice)
    {
        if (! voice.active || ! isHiHat (voice.instrument)
            || std::abs (aperture - voice.hatAperture) < 0.02f)
            return;
        const bool closing = aperture < voice.hatAperture;
        applyHatAperture (voice, aperture);
        if (closing)
        {
            const float frictionSeconds = 0.004f + 0.36f * aperture;
            beginChoke (voice, frictionSeconds);
            voice.pedalFrictionMultiplier = std::clamp (
                coefficientForTime (std::max (0.0005f, frictionSeconds),
                                    static_cast<float> (sampleRate_)),
                0.0f, 1.0f);
        }
        else if (voice.choking
                 && voice.chokeMultiplier == voice.pedalFrictionMultiplier)
        {
            voice.choking = false;
            voice.chokeMultiplier = 1.0f;
            voice.pedalFrictionMultiplier = 0.0f;
        }
    });

    // A foot coming down hard enough to shut the pair makes its own sound, and
    // it is the only stroke on a kit that is played without a stick. Its
    // strength follows the size of the move rather than a clock, so a host that
    // thins its controller stream changes when the chick lands, never how hard.
    // It is a chick and not a stick hit, so it is played as one. The two-
    // argument trigger() defaults to Head, which put the engine's own pedal
    // stroke on the stick path: it kept the broadband tip noise, missed the
    // shorter clamped decay, the six-times longer plate-on-plate contact and
    // the blunt-contact filtering, and came out sounding like a quiet closed
    // hat rather than a foot. GM note 44 has always taken the other branch.
    const float travel = position - previous;
    if (wasActive && previous < 0.55f && position >= 0.55f && travel > 0.18f)
        trigger (Instrument::ClosedHat,
                 std::clamp (0.18f + 1.25f * travel, 0.15f, 1.0f),
                 Articulation::FootChick);
}

bool DrumEngine::isStruckMembrane (Instrument instrument) noexcept
{
    switch (instrument)
    {
        case Instrument::Kick:
        case Instrument::Snare:
        case Instrument::LowTom:
        case Instrument::MidTom:
        case Instrument::HighTom:
            return true;
        default:
            return false;
    }
}

void DrumEngine::dampRingingMembrane (Instrument instrument, float velocity) noexcept
{
    // A stick landing on a head that is still moving does not produce a second
    // drum. It lands on the drum that is already there, and the contact both
    // adds the new strike and takes energy out of what it lands on: a hand or a
    // stick against a vibrating membrane is an absorber, which is why a
    // drummer's press roll dies away instead of growing, and why a flam is one
    // event with two attacks rather than two drums a few milliseconds apart.
    //
    // The bank is left in place and scaled instead of being cut, so the ring
    // that survives is the same ring, just smaller - and the new strike is
    // superposed into it by Resonator::strike(), which was written to add to
    // whatever state it finds. How much survives follows the new strike: a
    // ghost note laid on a ringing tom barely touches it, a full stroke very
    // nearly resets it.
    //
    // Only the struck membranes. A cymbal is metres of plate against a stick
    // tip the size of a fingernail, and a second strike on one really does add.
    if (! isStruckMembrane (instrument))
        return;

    const float retained = std::clamp (0.78f - 0.58f * velocity, 0.20f, 0.78f);
    const auto damp = [instrument, retained] (Voice& voice)
    {
        if (! voice.active || voice.instrument != instrument)
            return;
        for (auto& resonator : voice.resonators)
        {
            resonator.y1 *= retained;
            resonator.y2 *= retained;
        }
        voice.envelope *= retained;
        voice.auxiliaryEnvelope *= retained;
        voice.transientEnvelope *= retained;
        voice.kickStateX *= retained;
        voice.kickStateY *= retained;
        voice.modalEnergy *= retained * retained;
        voice.recentPeak *= retained;
    };
    forEachVoice (damp);
}

void DrumEngine::chokeGroup (int group) noexcept
{
    if (group <= 0)
        return;
    // Every voice remembers the group it was born into, so retuning the
    // parameter never strands a ringing tail that can no longer be cut.
    forEachVoice ([this, group] (Voice& voice)
    {
        if (voice.active && voice.chokeGroup == group)
            beginChoke (voice, 0.003f);
    });
}

void DrumEngine::allSoundsOff() noexcept
{
    forEachVoice ([this] (Voice& voice)
    {
        if (voice.active)
            beginChoke (voice, 0.004f);
    });
}

void DrumEngine::updateActiveVoiceCount() noexcept
{
    // One pass over both pools publishes the host-facing voice count and the
    // per-instrument meter levels the editor's channel strip reads. The per
    // voice recentPeak already carries 30 ms meter ballistics, so nothing has
    // to be tracked inside the per-sample loop.
    int count = 0;
    std::array<float, instrumentCount> levels {};
    // The same pass carries the newest voice's drawn pitch out to the same
    // place. Voices are numbered as they are triggered, so the newest sounding
    // one is the highest generation in either pool.
    std::uint64_t newestGeneration = 0u;
    float newestPitch = 0.0f;
    const auto observe = [&count, &levels, &newestGeneration, &newestPitch] (const Voice& voice)
    {
        if (! voice.active)
            return;
        ++count;
        auto& level = levels[indexFor (voice.instrument)];
        level = std::max (level, voice.recentPeak * voice.chokeGain);
        if (voice.generation >= newestGeneration)
        {
            newestGeneration = voice.generation;
            newestPitch = voice.oscillatorFrequency;
        }
    };
    forEachVoice (observe);
    for (std::size_t index = 0; index < instrumentCount; ++index)
        instrumentLevels_[index].store (levels[index], std::memory_order_relaxed);
    activeVoiceCount_.store (count, std::memory_order_relaxed);
    newestVoicePitch_.store (newestPitch, std::memory_order_relaxed);
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
    // efficiencyOut, when given, receives the radiation-efficiency term this
    // computes anyway. emit() below needs that same term for its "heard"
    // weight, and pow() is the expensive part of it - taking it here instead
    // of re-deriving omega/ka/exponent/power a second time from the same
    // (frequency, multipole) pair saves a transcendental call per mode for no
    // change to either result.
    const auto lossPerSecond = [&head, &loss] (
        float frequency, int multipole, float* efficiencyOut = nullptr)
    {
        const float omega = twoPi * std::max (1.0f, frequency);
        const float ka = omega * head.radius / soundSpeed;
        const float exponent = 2.0f + 2.0f * static_cast<float> (multipole);
        const float power = std::pow (std::max (1.0e-4f, ka), exponent);
        const float efficiency = power / (1.0f + power);
        if (efficiencyOut != nullptr)
            *efficiencyOut = efficiency;
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
    // The circumferential order each emitted slot came from, and zero for the
    // slots that are not a member of a degenerate pair. Only the m > 0 modes
    // are degenerate: an axisymmetric mode has no azimuth to rotate about, so
    // there is nothing for an uneven hoop to lift apart.
    int orders[resonatorCount] {};
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
            // A mode the rate cannot render is not a mode. See
            // initialiseModalVoice: clamping it onto the top of the band would
            // stack it in phase on whatever else landed there.
            if (frequency >= 0.44f * static_cast<float> (sampleRate_))
                return;
            float efficiency = 0.0f;
            const float modeLoss = std::max (1.0e-3f,
                                             lossPerSecond (frequency, multipole, &efficiency));
            const float heard = 0.34f + 0.66f * std::sqrt (efficiency);

            ratios[count] = ratio;
            decays[count] = decaySeconds * referenceLoss / modeLoss;
            excitation[count] = shape * weight * heard
                * contactSpectrum (frequency, head.contactSeconds);
            // Zero for the axisymmetric family, whose two branches above are
            // the air spring's doing and not a degeneracy.
            orders[count] = order;
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

    const float tilt = -1.30f + 1.05f * brightness;

    // Every mode above the axisymmetric family is two modes, not one. An ideal
    // circular head has a rotational symmetry, so for each m > 0 there are two
    // shapes at the same frequency - cos(m theta) and sin(m theta), one rotated
    // half a lobe from the other - and any departure from that symmetry lifts
    // them apart. Worland measured that ordinary non-uniform lug tension is
    // enough to do it - normal modes of a drumhead under non-uniform tension,
    // https://pubs.aip.org/asa/jasa/article/127/1/525/793705/ - which is why
    // every mode above the first is emitted here as the two modes it
    // physically is, and a drummer hears the result as the warble that a real
    // head's decay has and a single pole pair cannot produce: two close
    // frequencies sounding together beat at their difference.
    //
    // How far apart is a property of this drum and of nobody's playing - a
    // tuning key was turned once and the head has been that shape ever since -
    // so the split is hashed from the instrument rather than from the hit, and
    // reproduces after a reset. The range is what a cleared head shows, floored
    // at the point where the beat is slower than the tail it has to appear in:
    // 1.5 % on the Kick's m = 1 mode is 1.3 Hz against a 0.93 s tail, and half
    // a per cent would be a 2.6 s period inside that same tail, which is not a
    // warble anybody could hear.
    const float split = 0.020f + 0.005f * signedUnitFromHash (
        static_cast<std::uint32_t> ((indexFor (voice.instrument) + 1u) * 0x9e3779b9u)
        ^ 0x51ed270bu);

    // Splitting costs a slot each time, so the loudest pairs get the slots that
    // are left once the table has been emitted. Gain here is the same product
    // the normalisation below uses, so "loudest" means loudest as heard and not
    // loudest as struck.
    //
    // Every candidate's gain depends only on its own emitted ratio and
    // excitation, and neither changes until the slot is actually chosen and
    // split - at which point it stops being a candidate at all. The search
    // below used to call pow()-based gainOf() on every still-eligible slot on
    // every one of the (up to four) rounds it takes to find each pair, so an
    // untouched candidate had its gain re-derived from the same inputs once
    // per remaining round. Resolving it here instead, once per candidate, saves
    // those repeats with no change to which slot the search picks.
    float candidateGain[resonatorCount] {};
    for (int slot = 0; slot < count; ++slot)
        if (orders[slot] > 0)
            candidateGain[static_cast<std::size_t> (slot)] = std::pow (
                std::max (1.0f, ratios[static_cast<std::size_t> (slot)]), tilt)
                * std::abs (excitation[static_cast<std::size_t> (slot)]);

    const float renderable = 0.44f * static_cast<float> (sampleRate_);
    while (count < resonatorCount)
    {
        int best = -1;
        float bestGain = 0.0f;
        for (int slot = 0; slot < count; ++slot)
        {
            // Zero is the axisymmetric family and negative marks a slot that is
            // already one member of a pair. Splitting a member again would be
            // inventing a degeneracy that is not there.
            if (orders[slot] <= 0)
                continue;
            // The upper member has to be a mode the rate can still render, on
            // the same terms the emission above applies.
            const float upper = ratios[static_cast<std::size_t> (slot)]
                * (1.0f + 0.5f * split);
            if (fundamental * upper >= renderable)
                continue;
            const float gain = candidateGain[static_cast<std::size_t> (slot)];
            if (gain > bestGain)
            {
                bestGain = gain;
                best = slot;
            }
        }
        if (best < 0)
            break;

        const auto lower = static_cast<std::size_t> (best);
        const auto upper = static_cast<std::size_t> (count);
        const int order = orders[lower];
        // The two members lie at right angles to each other around the head, so
        // a strike at azimuth phi drives them as cos(m phi) and sin(m phi).
        // Squared those sum to one, so the pair carries exactly the energy the
        // single mode carried and the split is a redistribution rather than a
        // gain: what changes is that the energy now arrives at two frequencies
        // instead of one.
        const float excited = excitation[lower];
        const float lowerRatio = ratios[lower] * (1.0f - 0.5f * split);
        const float upperRatio = ratios[lower] * (1.0f + 0.5f * split);
        const auto decayFor = [&] (float ratio)
        {
            return decaySeconds * referenceLoss
                / std::max (1.0e-3f, lossPerSecond (fundamental * ratio, order));
        };

        ratios[lower] = lowerRatio;
        excitation[lower] = excited * std::cos (static_cast<float> (order)
                                                * voice.strikeAzimuth);
        decays[lower] = decayFor (lowerRatio);
        ratios[upper] = upperRatio;
        excitation[upper] = excited * std::sin (static_cast<float> (order)
                                                * voice.strikeAzimuth);
        decays[upper] = decayFor (upperRatio);
        // Both members are spoken for now, so neither is a candidate again.
        orders[lower] = -order;
        orders[upper] = -order;
        ++count;
    }

    // Level and spectral tilt, then the resonators themselves.
    voice.modeCount = count;
    float gainSum = 0.0f;
    for (int mode = 0; mode < count; ++mode)
    {
        const auto slot = static_cast<std::size_t> (mode);
        // A slot still at orders[slot] > 0 here was an eligible split candidate
        // the search above never picked, so its ratio and excitation are
        // exactly what they were when candidateGain[] was resolved: reusing
        // that cached value is the same pow() call the search already paid
        // for, not a new one. A slot at order <= 0 is either the axisymmetric
        // family (never a candidate) or a pair the split rewrote, so it is
        // resolved fresh.
        const float gain = orders[slot] > 0
            ? candidateGain[slot]
            : std::pow (std::max (1.0f, ratios[slot]), tilt)
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
    // configureResonator clamps a frequency it cannot render onto the top of
    // the band rather than refusing it, which is right for one stray mode and
    // very wrong for a bank: a twelve-mode plate at an 8 kHz host rate had its
    // top five modes land on exactly the same frequency and, being struck
    // together, pile up in phase there as a resonance that followed the sample
    // rate instead of the instrument. A mode the rate cannot carry is not a
    // mode, so it is dropped before the gains are normalised and the rest of
    // the bank simply keeps its energy.
    const float renderable = 0.44f * static_cast<float> (sampleRate_);
    const float tilt = -1.30f + 1.05f * brightness;
    int kept = 0;
    float gainSum = 0.0f;
    for (int mode = 0; mode < modeCount; ++mode)
    {
        const auto hash = hash32 (voice.noiseState + static_cast<std::uint32_t> (mode * 0x9e37));
        const float random = static_cast<float> (hash & 0xffffu) / 32767.5f - 1.0f;
        const float ratio = ratios[mode] * (1.0f + 0.035f * spread * random);
        if (baseFrequency * ratio >= renderable)
            continue;

        // Damping is a property of the frequency, not of the mode's position in
        // an array. The old taper ran on the index, so a bank's twelfth mode
        // decayed 1.25x faster than its first whether that mode sat one octave
        // up or four - which made every struck head ring like a bell, all of its
        // modes dying together instead of the top of the head going first.
        const float relative = std::max (1.0f, ratio);
        const float lossFactor = std::max (
            0.05f, loss.fixed + loss.hysteretic * relative
                       + loss.viscous * relative * relative);
        const auto slot = static_cast<std::size_t> (kept);
        configureResonator (voice.resonators[slot], baseFrequency * ratio,
                            decaySeconds / lossFactor);
        const float gain = std::pow (std::max (1.0f, ratio), tilt)
            * (excitation != nullptr
                   ? std::abs (excitation[static_cast<std::size_t> (mode)])
                   : 1.0f);
        voice.modeGains[slot] = gain;
        gainSum += gain;
        ++kept;
    }
    voice.modeCount = kept;
    const float scale = gainSum > 0.0f ? 1.35f / gainSum : 1.0f;
    for (int mode = 0; mode < kept; ++mode)
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
                                  const HitVariation& variation,
                                  Articulation articulation) noexcept
{
    voice = Voice {};
    voice.active = true;
    voice.instrument = instrument;
    voice.articulation = articulation;
    voice.chokeGroup = std::clamp (values.chokeGroup, 0, chokeGroupCount);
    voice.generation = ++generation_;
    voice.noiseState = seed == 0u ? 1u : seed;
    voice.velocity = accentVoltage (velocity);
    voice.recentPeak = voice.velocity;
    voice.characterA = std::clamp (values.characterA + variation.characterAOffset,
                                   0.0f, 1.0f);
    voice.characterB = std::clamp (values.characterB + variation.characterBOffset,
                                   0.0f, 1.0f);
    voice.pitchRatio = std::exp2 ((values.pitch + 0.01f * variation.pitchCents) / 12.0f);
    voice.decaySeconds = decaySecondsFor (instrument, values.decay)
        * variation.decayScale;
    voice.decayVariation = variation.decayScale;
    voice.transientScale = variation.transientScale;
    // The nominal aim, moved by whatever this stroke's Humanise draw was. Only
    // buildHeadBank reads it, so the voices with no head do no work for it.
    voice.strikeAzimuth = nominalStrikeAzimuth
        + variation.strikeAzimuthDegrees * (pi / 180.0f);
    // Treat MIDI velocity as trigger/accent voltage as well as final VCA
    // loudness. The deliberately narrow range preserves the established gain
    // curve while making hard hits inject more energy into the physical core.
    voice.excitationScale = excitationScaleFor (velocity);

    // How much of the drawn sweep this strike gets, latched here rather than
    // followed. The displacement the strike leaves in the head is the accent
    // voltage times the excitation scale - the same product the modal bank is
    // struck with - and the energy stored in a stretched head goes as the
    // square of it, so that square, normalized at the saturation velocity, is
    // the depth. voice.modalEnergy is deliberately not used: it is a follower,
    // two orders of magnitude below its own peak at the strike sample, still
    // holding 0.19 % of it at 60 ms where the pitch envelope is at 0.025 %, so
    // driving the sweep with it would start the note at settled pitch, bend it
    // upward over the first two milliseconds and leave eleven cents of residual
    // bend on the Kick against one and a half. The shape of the sweep is the
    // pitch envelope's, unchanged; only its depth is the strike's.
    const float strikeAmplitude = voice.velocity * voice.excitationScale;
    voice.strikeDepth = std::min (1.0f, (strikeAmplitude * strikeAmplitude)
        / (sweepSaturationAmplitude * sweepSaturationAmplitude));
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
    // Four milliseconds is a couple of periods of the lowest thing a drum head
    // carries, which is what "short-time average" has to mean for an energy
    // estimate that is not allowed to follow the waveform itself.
    voice.tensionSmoothing = 1.0f - std::exp (
        -1.0f / std::max (1.0f, 0.004f * static_cast<float> (sampleRate_)));
    // Quiet voices retire from their measured output level. This is the hard
    // host-facing ceiling; a forced fade begins shortly before it.
    voice.maximumSamples = maximumVoiceSamples_;

    // Placement is now an automatable per-voice control whose defaults are the
    // former hard-coded kit positions, so an untouched kit images identically.
    applyPan (values.pan, voice.panLeft, voice.panRight);

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

    // A hi-hat is a pair of thin bronze plates, and a plate's modes are dense,
    // inharmonic and spaced further apart as they climb - nothing like a
    // membrane's series and nothing like the six square waves that were
    // standing in for the whole instrument.
    static constexpr float hatRatios[12] {
        1.0f, 1.593f, 2.135f, 2.782f, 3.474f, 4.316f,
        5.278f, 6.389f, 7.628f, 9.012f, 10.550f, 12.240f
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
            voice.contactSeconds = hertzianContactSeconds (
                0.00190f - 0.00120f * voice.characterA, velocity, -0.2f);
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
            // A bass drum head is wide, heavy and slack, but the beater only
            // ever displaces it by a small fraction of its own radius, so its
            // tension swing is the smallest of the struck drums.
            voice.tensionDepth = 0.060f;

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
            // A drummer's snare is three instruments, and the only difference
            // between them is where the stick landed and how long it stayed.
            //
            // A rimshot is the head and the rim struck together: the tip lands
            // right against the hoop, where J_m is far from the centre and the
            // whole circumferential series is fed at once, and the shaft meets
            // steel, so the contact is the shortest anything in this kit makes.
            // The head is driven hard, so the wires leave it further and buzz
            // more, and the hoop itself rings - which is the crack.
            //
            // A cross-stick is the opposite: the stick lies flat across the
            // head with the player's hand resting on it, and the butt is
            // dropped onto the rim. The hand is a heavy absorber sitting on the
            // membrane, so the head is gone in a fifth of the time and the
            // wires never lift off at all. What radiates is the shell, and that
            // is why a cross-stick is a woody knock rather than a quiet snare.
            const bool rimshot = articulation == Articulation::Rimshot;
            const bool crossStick = articulation == Articulation::CrossStick;
            voice.baseFrequency = 185.0f * voice.pitchRatio;
            voice.phaseIncrements[0] = std::min (0.45f, voice.baseFrequency * inverseSampleRate_);
            voice.phaseIncrements[1] = std::min (0.45f, voice.baseFrequency * 1.78f * inverseSampleRate_);
            voice.envelopeMultiplier = coefficientForTime (voice.decaySeconds * 0.72f,
                                                            static_cast<float> (sampleRate_));
            // Snap is the stick: a harder tip on a thin, tight head stays down
            // for less time, and Hertz shortens it further as the hit gets
            // harder. A snare's contact is the shortest in the kit, which is
            // most of why it is the brightest thing in it.
            voice.contactSeconds = hertzianContactSeconds (
                0.00085f - 0.00050f * voice.characterB, velocity, -0.35f)
                * (rimshot ? 0.38f : crossStick ? 0.60f : 1.0f);
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
            // Where the stick landed, as a fraction of the head's radius. This
            // is the whole articulation: J_m(lambda r/a) decides which modes a
            // strike can reach, and a hoop strike reaches all of them.
            head.strikeRadius = rimshot ? 0.93f : crossStick ? 0.80f : 0.36f;
            head.contactSeconds = voice.contactSeconds;
            head.airLoadScale = 0.75f + 0.50f * voice.characterB;
            // Wires resting on the far head damp everything, and a small shallow
            // drum radiates well for its size, so a snare is the shortest-lived
            // head in the kit at every frequency.
            // A hand resting on a membrane is a far heavier absorber than any
            // loss inside the film, and it is frequency-independent because it
            // is contact rather than material, so it goes into the fixed term.
            buildHeadBank (voice, voice.baseFrequency,
                           head,
                           voice.decaySeconds * (crossStick ? 0.14f
                                                 : rimshot ? 0.72f : 0.62f),
                           0.38f + 0.34f * voice.characterB
                               + (rimshot ? 0.30f : 0.0f),
                           { crossStick ? 92.0f : 16.0f, 0.030f, 3.0e-6f, 300.0f });
            // A batter head tensioned hard enough to answer a stick has little
            // room left to stretch, which is why a snare's note barely moves
            // where a tom's audibly does.
            voice.tensionDepth = 0.080f;
            configureBandpass (voice.filterA,
                               (1250.0f + 4800.0f * voice.characterB)
                                   * std::pow (voice.pitchRatio, 0.30f) * voice.velocityTimbre,
                               0.65f + 0.45f * voice.characterB);
            configureHighpass (voice.filterB, 700.0f + 1700.0f * voice.characterB, 0.7f);
            voice.transientMultiplier = coefficientForTime (
                (0.004f + 0.005f * voice.characterB)
                    * (rimshot ? 0.55f : crossStick ? 0.70f : 1.0f),
                static_cast<float> (sampleRate_));

            // The hoop. A cast or triple-flanged rim struck by a stick shaft is
            // a short, hard, strongly pitched ring, and it sits an octave and a
            // half apart between the two strokes because they excite different
            // parts of it: a rimshot rings the hoop against a driven head, a
            // cross-stick rings the shell through a dead one.
            if (rimshot || crossStick)
            {
                configureBandpass (voice.filterC,
                                   (rimshot ? 1750.0f : 720.0f)
                                       * std::pow (voice.pitchRatio, 0.5f),
                                   rimshot ? 9.0f : 6.5f);
                voice.rimLevel = rimshot ? 2.10f : 1.80f;
            }
            // A driven head lifts its wires further; a hand-damped one never
            // lifts them at all, and a cross-stick's body is the shell rather
            // than the batter head's own note.
            voice.wireScale = rimshot ? 1.60f : crossStick ? 0.05f : 1.0f;
            voice.bodyScale = crossStick ? 0.10f : 1.0f;
            if (crossStick)
            {
                voice.envelopeMultiplier = coefficientForTime (
                    voice.decaySeconds * 0.20f, static_cast<float> (sampleRate_));
                voice.auxiliaryMultiplier = coefficientForTime (
                    voice.decaySeconds * 0.16f, static_cast<float> (sampleRate_));
            }
            else if (rimshot)
            {
                voice.excitationScale *= 1.22f;
            }
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
            // Both bandpasses track Pitch by the same fractional power - the
            // direct-impact band and the diffuse tail band are two filters on
            // one strike, not two independently tuned voices - so it is one
            // std::pow call shared between them rather than the identical one
            // computed twice for the same voice.pitchRatio.
            const float pitchStretch = std::pow (voice.pitchRatio, 0.42f);
            configureBandpass (voice.filterA,
                               (850.0f + 2750.0f * voice.characterB)
                                   * pitchStretch * voice.velocityTimbre,
                               0.68f + 0.42f * voice.characterB);
            configureHighpass (voice.filterB, 430.0f + 1450.0f * voice.characterB, 0.72f);
            // The tail of a clap is the room answering, not the hands again.
            // Sending it through its own wider, lower band decorrelates it from
            // the direct impacts, which is what a diffuse field is: the same
            // energy arriving from everywhere with its own spectrum, rather
            // than the strike itself held down by a fader.
            configureBandpass (voice.filterC,
                               (620.0f + 1500.0f * voice.characterB)
                                   * pitchStretch
                                   * voice.velocityTimbre,
                               0.45f);
            break;
        }

        case Instrument::ClosedHat:
        case Instrument::OpenHat:
        {
            // The pedal, as one number. Everything that used to be chosen by
            // which of the two notes arrived is now read off it, and the two
            // notes are exactly its endpoints - so an untouched pedal, where a
            // Closed Hat note means nought and an Open Hat note means one,
            // reproduces both voices sample for sample.
            //
            // The blend is guarded at both ends rather than trusted to arrive
            // there: `open + (closed - open) * 1` is not bit-identical to
            // `closed` in float, and a hat that changed in the last place of
            // its decay constant when nobody had touched the pedal would be a
            // silly thing to ship.
            // A foot chick is played with the pair already clamped, whatever
            // the pedal was doing a moment earlier: the stroke *is* the plates
            // arriving against each other, so there is no aperture left to read
            // off the controller.
            const bool footChick = articulation == Articulation::FootChick;
            voice.hatAperture = footChick ? 0.0f : hiHatAperture (instrument);
            const float clamped = 1.0f - voice.hatAperture;
            const auto pedalBlend = [clamped] (float openValue, float closedValue)
            {
                return clamped <= 0.0f ? openValue
                     : clamped >= 1.0f ? closedValue
                     : openValue + (closedValue - openValue) * clamped;
            };

            voice.decaySeconds = hatDecaySecondsFor (voice.hatAperture,
                                                     variation.decayScale)
                // Two plate faces meeting flat carry far more contact area than
                // a tip does, and the pair is damped by that contact for the
                // whole of the stroke rather than struck through it, so a chick
                // is shorter than the shortest stick hit the same pair makes.
                * (footChick ? 0.42f : 1.0f);
            voice.envelopeMultiplier = coefficientForTime (
                voice.decaySeconds, static_cast<float> (sampleRate_));

            configureMetallicOscillatorBank (
                instrument, voice.pitchRatio, voice.characterA, false);
            configureHighpass (voice.filterA, 3400.0f + 6500.0f * voice.characterB, 0.70f);
            configureBandpass (voice.filterB,
                               (6500.0f + 4800.0f * voice.characterB) * voice.velocityTimbre,
                               0.85f);
            voice.transientMultiplier = coefficientForTime (
                pedalBlend (0.006f, 0.0025f), static_cast<float> (sampleRate_));
            // The very top of a plate goes first. On an open hat that is most
            // of what you hear happen - it starts bright and darkens as it
            // rings - while a closed pair is damped by friction, which takes
            // every partial at much the same rate and leaves far less to hear.
            voice.auxiliaryMultiplier = coefficientForTime (
                voice.decaySeconds * pedalBlend (0.42f, 0.78f),
                static_cast<float> (sampleRate_));

            // The plates themselves. Closing the pedal clamps them together,
            // which both stiffens the pair - the contact is a boundary the
            // modes did not have - and damps it hard, by friction between two
            // faces rather than by anything inside the bronze. That is why a
            // closed hat is not a short open hat: friction takes every partial
            // at much the same rate, where an open plate loses its top first
            // and darkens as it rings.
            // A wooden tip on a thin plate: the shortest contact in the kit,
            // which is why a hat has the sharpest edge of anything in it, and
            // why a soft hat is duller and not merely quieter.
            //
            // A foot chick is the one stroke where the thing arriving is not a
            // tip. Two plate faces meet over the whole of their overlap, and a
            // contact that broad and that compliant lasts far longer than a
            // point one - so by the same reach law below it puts much less at
            // the top of the series. That is the whole difference between a
            // chick and a quiet closed hat, and it is why the step's word for
            // this contact is "blunter": under this engine's own reach law a
            // blunt contact is a long one, not a short one.
            voice.contactSeconds = hertzianContactSeconds (
                0.00042f - 0.00022f * voice.characterA, velocity, -0.35f)
                * (footChick ? 6.0f : 1.0f);
            // And there is no stick, so there is no stick noise: the broadband
            // burst renderHat lays in front of the plate is a wooden tip
            // scuffing bronze, and nothing scuffs anything here.
            voice.strikeNoise = footChick ? 0.0f : 1.0f;
            // Clamping the pair is a boundary the modes did not have, so the
            // whole plate stiffens as the pedal comes down.
            voice.baseFrequency = pedalBlend (540.0f, 610.0f)
                * std::pow (voice.pitchRatio, 0.86f);
            // How far up the plate a strike gets is the contact again, but on a
            // plate the contact patch is a stick tip against something a
            // millimetre thick, so it is the sharpness of the onset rather than
            // the gross duration that reaches the top of the series. Carrying it
            // as a tilt on the bank keeps that honest without pretending the
            // force history has the nulls a raised cosine would.
            // How far up the plate the strike gets is the contact, so the tilt
            // is read from it rather than from velocity directly: a shorter
            // touch reaches higher, whether it got shorter because the stick
            // arrived faster or because the tip is harder.
            const float reach = std::clamp (
                0.00042f / std::max (1.0e-5f, voice.contactSeconds), 0.30f, 1.60f);
            // On a stick hit the reach is carried as a tilt on the modal bank
            // and nowhere else, which is right, because the bank is the only
            // part of the voice whose modes the engine knows individually. A
            // foot chick has no bank of its own to tilt into: the pair is
            // clamped and its plate modes are the shortest things in the voice,
            // so what the blunt contact has to reach - or fail to reach - is
            // the circuit source that carries most of what a hat radiates. One
            // pole at 14 kHz scaled by the same contact ratio the reach above
            // is read from does that: at a stick's own contact it would sit at
            // 19 kHz and be very nearly bypass, and at the chick's six-times
            // longer one it is at 3.2 kHz. It is unclamped, because the clamp
            // on the reach exists to keep a bank's tilt sane rather than to
            // bound a corner. It is deliberately applied to
            // this articulation alone. Putting it on the two stick-struck hats
            // as well would re-voice both of them, which is a change that has
            // to be judged by ear rather than measured into a step about MIDI.
            if (footChick)
                configureOnePoleLowpass (
                    voice.filterC,
                    14000.0f * 0.00042f / std::max (1.0e-5f, voice.contactSeconds));
            initialiseModalVoice (
                voice, hatRatios, 12, voice.baseFrequency,
                voice.decaySeconds * pedalBlend (0.70f, 0.85f),
                0.10f, 0.60f + 0.14f * voice.characterB + 0.34f * reach,
                // An open plate loses its top first, so its damping climbs
                // steeply with frequency; a clamped pair is damped by friction
                // between two faces instead, which takes every partial at much
                // the same rate. The pedal moves continuously between the two
                // laws rather than switching between two sounds.
                ModalLoss { pedalBlend (0.55f, 0.86f), pedalBlend (0.30f, 0.10f),
                            pedalBlend (0.15f, 0.04f) });
            break;
        }

        case Instrument::Ride:
        case Instrument::Crash:
        {
            const bool ride = instrument == Instrument::Ride;
            configureMetallicOscillatorBank (
                instrument, voice.pitchRatio, voice.characterA, false);
            // The unvaried parameter, deliberately. Every other control on a
            // cymbal is a component that drifts, but Machine is not a control
            // on either machine - it chooses which machine this is. Letting a
            // per-hit tolerance move it would mean a hit set to one circuit
            // occasionally arriving with a few percent of the other one mixed
            // in, which is not something any tolerance does.
            configureCymbalChannel (voice, instrument, velocity, values.characterA,
                                    articulation);

            // The 808's three VCAs run off two envelope generators, and the
            // third band is the same capacitor read through a different
            // resistor, so the channel carries three time constants: the
            // DECAY-controlled one on the low band, a middle one, and a short
            // one at the top. A cymbal darkens as it rings for exactly this
            // reason in metal too - the high partials radiate best and the
            // plate's own nonlinear coupling drains them downward - so the
            // circuit and the instrument agree about which end goes first.
            const auto floatSampleRate = static_cast<float> (sampleRate_);
            const float midSeconds = voice.decaySeconds * (ride ? 0.52f : 0.78f);
            const float highSeconds = voice.decaySeconds * (ride ? 0.20f : 0.52f);
            voice.envelopeMultiplier = coefficientForTime (
                voice.decaySeconds * (ride ? 1.20f : 1.55f), floatSampleRate);
            voice.pitchEnvelopeMultiplier = coefficientForTime (
                midSeconds, floatSampleRate);
            voice.auxiliaryMultiplier = coefficientForTime (
                highSeconds, floatSampleRate);
            // Each envelope reaches -60 dB in its own time, so at 1.5 times
            // that it is 90 dB down and the squared swing law puts its band
            // 180 dB below the hit - far under the -150 dB at which the engine
            // already retires a resonator bank.
            const auto retirementAge = [this] (float seconds) noexcept
            {
                return static_cast<std::uint64_t> (std::ceil (
                    1.5 * static_cast<double> (std::max (0.0f, seconds))
                    * sampleRate_)) + 1u;
            };
            voice.cymbal.midActiveSamples = retirementAge (midSeconds);
            voice.cymbal.highActiveSamples = retirementAge (highSeconds);
            voice.transientMultiplier = coefficientForTime (
                ride ? 0.0022f + 0.0038f * voice.characterA
                     : 0.0035f + 0.0065f * voice.characterA,
                floatSampleRate);

            // No modal bank is set up here, because neither cymbal renders one
            // any more. initialiseVoice() has already cleared the voice, so
            // modeCount and modalActiveSamples are zero and every generic
            // modal guard downstream is false. Configuring the resonator
            // bank's eighteen slots whose state nothing reads would be a
            // note-on cost - transcendental work on the audio thread - paid
            // by every hit of a dense ride.
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
            voice.contactSeconds = hertzianContactSeconds (
                0.00120f - 0.00070f * voice.characterA, velocity, -0.35f);
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
            // The tom is where this is loudest, and Skin is the reason: a head
            // carrying more air is a slacker head, and a slacker head stretches
            // further for the same blow.
            voice.tensionDepth = 0.100f + 0.140f * voice.characterB;

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
            // The grains are a mass, and a mass has to travel. A shake sends
            // them down the tube, they arrive at the far wall more or less
            // together, and they rebound: the collision rate swells and falls
            // over the length of that flight rather than sitting at a constant
            // Poisson rate for the whole note. That swell is the "cha" - the
            // only thing that separates a shaker from a burst of filtered hiss -
            // and a harder shake gets them there sooner.
            voice.contactSeconds = hertzianContactSeconds (
                0.042f - 0.018f * voice.characterA, velocity, -0.30f);
            voice.contactIncrement = std::min (
                1.0f, inverseSampleRate_ / std::max (1.0e-5f, voice.contactSeconds));
            voice.contactPhase = 0.0f;
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
            // A cowbell is a folded steel plate, and above its famous pair of
            // partials it has the dense, fast-dying plate response that gives
            // the strike its edge. One VCA over a free-running waveform left the
            // spectrum frozen for the whole note, which is the difference
            // between a struck bell and a square wave being faded.
            configureBandpass (voice.filterC,
                               std::min (9000.0f, voice.baseFrequency * 5.4f), 0.60f);
            voice.auxiliaryMultiplier = coefficientForTime (
                0.018f + 0.05f * voice.decaySeconds, static_cast<float> (sampleRate_));
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
            // A clave is a short solid bar struck across its length, and a free
            // bar's bending modes go as the square of an odd-ish integer:
            // 1 : 2.76 : 5.40 : 8.93, spreading fast rather than crowding
            // together the way a membrane's do. Hollow drills the bar out
            // toward a tube, which softens that spread.
            // Hollow is the hand, not the bar. A clave's overtone ratios are
            // fixed by its boundary conditions and no player can move them;
            // what a player does move is how tightly the fingers close under it,
            // which retunes the cavity that does the radiating. The control used
            // to detune all four modes instead, which is a thing a bar cannot
            // do and which made every setting of it a different instrument.
            const float hollow = voice.characterA;
            // Wood's internal friction is close to a constant loss angle across
            // the audio range, so a struck bar's damping climbs roughly with
            // frequency and its bending overtones fade well before the note.
            voice.baseFrequency = 930.0f * voice.pitchRatio;
            initialiseModalVoice (voice, percBarRatios.data(), 4, voice.baseFrequency,
                                  voice.decaySeconds, 0.055f,
                                  (0.35f + 0.35f * hollow) * voice.velocityTimbre,
                                  { 0.45f, 0.55f, 0.0f });
            configureBandpass (voice.filterA,
                               (2800.0f + 5000.0f * voice.characterB) * voice.velocityTimbre,
                               0.85f);
            configureHighpass (voice.filterB, 350.0f + 550.0f * (1.0f - hollow), 0.70f);
            // The cupped hand a clave is played over is a Helmholtz resonator,
            // and the player tunes it by how tightly the fingers close. It is
            // what actually radiates: a bar this small is a hopeless radiator on
            // its own, and the cavity is the reason one of them can be heard
            // over a full percussion section.
            configureBandpass (voice.filterC, 340.0f - 150.0f * hollow, 1.35f);
            // A hard wooden tip on a hard wooden bar: the shortest contact in
            // the instrument, and it shortens further as the strike gets harder.
            voice.contactSeconds = hertzianContactSeconds (
                0.00026f - 0.00012f * voice.characterB, velocity, -0.2f);
            voice.contactIncrement = std::min (
                1.0f, inverseSampleRate_ / std::max (1.0e-5f, voice.contactSeconds));
            voice.contactPhase = 0.0f;
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

void DrumEngine::trigger (Instrument instrument, float velocity,
                          Articulation articulation) noexcept
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
    // Before the new voice exists, so the strike lands on whatever this drum is
    // still doing rather than beside it.
    dampRingingMembrane (instrument, velocity);
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
    // The board the voice sits on drifts too, and it drifts for everything on
    // it at once: one supply, one ambient temperature. Two drums struck a
    // moment apart therefore lean the same way, which is the part of an
    // analogue kit that a per-voice tolerance cannot produce - it makes the
    // kit sound like one instrument having a moment rather than thirteen
    // independent ones jittering.
    const float boardDrift = boardDriftAt (engineSamples_);

    // Humanise scales how much of that modelled tolerance actually reaches the
    // voice. The drift accumulators themselves are untouched, so the sequence
    // stays reproducible at every setting. Zero gives a machine-tight kit, 0.5
    // is the calibrated unit, and 1.0 doubles the spread into a visibly older
    // one. The deviations are sized to be heard rather than merely measured:
    // roughly a sixth of a semitone of pitch at the default, which is where a
    // repeated hit stops reading as a loop of one recording.
    // humanise_ is stored through clampUnit() by setKitParameters(), so it is
    // already finite and already in [0, 1] by the time trigger() reads it;
    // clampUnit() is idempotent, so reapplying it here only reproduced the
    // stored value at the cost of a redundant isfinite/clamp per note-on.
    const float humaniseDepth = 2.0f * humanise_.load (std::memory_order_relaxed);
    HitVariation variation;
    variation.pitchCents = humaniseDepth * (5.6f * drift
        + 4.4f * signedUnitFromHash (seed ^ 0xc8013ea4u)
        + 6.5f * boardDrift);
    variation.decayScale = 1.0f + humaniseDepth * std::clamp (
        0.044f * drift + 0.030f * signedUnitFromHash (seed ^ 0xad90777du)
            + 0.038f * boardDrift,
        -0.090f, 0.090f);
    // The board term is deliberately absent from these two: what drifts with
    // temperature is a frequency or a time constant, not where a panel control
    // is set. Leaving it out also keeps the kit's overall level steady while
    // it drifts, which is the difference between a machine warming up and a
    // machine with a fader moving on it.
    variation.characterAOffset = humaniseDepth * (0.032f * drift
        + 0.026f * signedUnitFromHash (seed ^ 0x7e95761eu));
    variation.characterBOffset = humaniseDepth * (-0.026f * drift
        + 0.029f * signedUnitFromHash (seed ^ 0x3c6ef372u));
    variation.transientScale = 1.0f + humaniseDepth * std::clamp (
        -0.062f * drift + 0.064f * signedUnitFromHash (seed ^ 0xbb67ae85u)
            + 0.030f * boardDrift,
        -0.150f, 0.150f);
    variation.circuitDriveOffset = humaniseDepth * (0.062f * drift
        + 0.080f * signedUnitFromHash (seed ^ 0x1b873593u)
        + 0.055f * boardDrift);
    variation.circuitBias = humaniseDepth * (0.018f * drift
        + 0.013f * signedUnitFromHash (seed ^ 0x85ebca6bu)
        + 0.012f * boardDrift);
    variation.phaseOffset = humaniseDepth * (0.018f * drift
        + 0.022f * signedUnitFromHash (seed ^ 0xc2b2ae35u));
    // Where the stick landed, as a deviation from the nominal aim. Neither
    // drift term appears: see HitVariation. Four degrees at the calibrated
    // unit, eight at the top of the control, which on the m = 1 members of a
    // split pair is the difference between about 5 dB and about 13 dB of beat
    // depth in the tail - a stroke that lands closer to a nodal line rings a
    // deeper warble than one that splits the pair evenly.
    variation.strikeAzimuthDegrees = humaniseDepth * 4.0f
        * signedUnitFromHash (seed ^ 0x27d4eb2fu);
    auto& voice = voices_[static_cast<std::size_t> (findVoiceSlot())];
    if (voice.active && voice.ageSamples != 0u)
        retireVoice (voice);
    if (voice.active)
        releaseBankReference (voice.instrument);
    initialiseVoice (voice, instrument, velocity, values, seed, variation, articulation);
    addBankReference (instrument);
    anyVoiceActive_ = true;
    updateActiveVoiceCount();
}

bool DrumEngine::triggerMidi (int midiNote, float velocity) noexcept
{
    const auto mapping = midiTriggerForNote (midiNote);
    if (! mapping.has_value())
        return false;
    trigger (mapping->instrument, velocity, mapping->articulation);
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

void DrumEngine::advanceModalTension (Voice& voice, float bankOutput) noexcept
{
    if (voice.tensionDepth <= 0.0f)
        return;

    // A membrane's restoring force is its tension, and a head that has been
    // pushed out of its plane is a head that has been stretched, so the whole
    // bank is sharp while the strike energy is still in it and settles back as
    // the drum rings out. That is the pitch drop a struck tom has and a
    // pitch-enveloped oscillator only imitates: it is not a fixed sweep from a
    // fixed starting note, it follows how hard the drum was actually hit, so a
    // ghost stroke barely bends at all and an accent bends audibly.
    //
    // Avanzini and Marogna's result is that the short-time average of the
    // tension rise is proportional to the system's energy, which is what this
    // leaky mean square estimates. It is why the model costs one multiply-add
    // per sample instead of a nonlinear solve. The bank's internal amplitude is
    // deliberately almost velocity-independent - the strike is normalised and
    // the VCA is applied after the voice - so the physical displacement has to
    // be recovered by scaling with the voice's own velocity here, or a ghost
    // note would bend exactly as far as an accent.
    const float displacement = voice.velocity * bankOutput;
    voice.modalEnergy = flushDenormal (
        voice.modalEnergy
        + voice.tensionSmoothing * (displacement * displacement - voice.modalEnergy));
    // Six per cent is about a semitone, and it is a ceiling rather than an
    // operating point: a real head that stretched further than that would be
    // being played with a hammer. It also keeps the linearised coefficient
    // update well inside the region where it is accurate.
    const float tension = std::min (0.060f, voice.tensionDepth * voice.modalEnergy);

    // The coefficients are rewritten every sixteenth sample. The estimate moves
    // on the scale of the strike envelope - tens of milliseconds - so a third of
    // a millisecond of staleness is inaudible, and it keeps the whole model at a
    // fraction of the bank's own per-sample cost. The interval is counted in the
    // voice's own age, so it falls on the same samples at every host block size.
    if ((voice.ageSamples & 15u) != 0u
        || std::abs (tension - voice.modalTension) < 1.0e-5f)
        return;
    voice.modalTension = tension;
    for (int mode = 0; mode < voice.modeCount; ++mode)
        voice.resonators[static_cast<std::size_t> (mode)].setTension (tension);
}

float DrumEngine::renderModalBank (Voice& voice, float impulse,
                                   bool applyTension) noexcept
{
    float output = 0.0f;
    if (voice.ageSamples < voice.modalActiveSamples)
    {
        if (voice.ageSamples == 0u)
            for (int mode = 0; mode < voice.modeCount; ++mode)
                voice.resonators[static_cast<std::size_t> (mode)].strike (
                    impulse * voice.modeGains[static_cast<std::size_t> (mode)]);
        for (int mode = 0; mode < voice.modeCount; ++mode)
            output += voice.resonators[static_cast<std::size_t> (mode)].tick (0.0f);
        if (applyTension)
            advanceModalTension (voice, output);
    }
    return output;
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
    const float amplitudePitch = 1.0f
        + 0.016f * voice.characterB * stateMagnitude;
    // Punch draws the sweep; the beater decides how much of it this hit uses.
    // The former trigger-voltage term here spanned 0.84 to 1.00 over the whole
    // velocity range, which is a hundredth of the sweep it multiplied, so a
    // ghost stroke and an accent glided the same fourth-and-a-half. It read
    // exactly 1.00 at full velocity, which is what strikeDepth reads at and
    // above its saturation velocity, so a hard hit is unchanged to the bit.
    const float frequency = std::clamp (
        voice.baseFrequency
            * (1.0f + voice.sweepAmount * voice.strikeDepth * voice.pitchEnvelope)
            * amplitudePitch,
        4.0f, 0.18f * static_cast<float> (sampleRate_));
    voice.oscillatorFrequency = frequency;

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
    // The stored charge is already the strike's energy - it was scaled by
    // excitationScale when the voice was built - so scaling it again here
    // would square it for the head while the body got it once, and quietly
    // bury the head under the body on every soft hit.
    const float head = renderModalBank (voice, voice.kickCharge, true);

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
    const float body = voice.bodyScale
        * (0.72f * oscillator (voice, 0) + 0.36f * oscillator (voice, 1))
        * voice.envelope;
    const float noise = nextBandLimitedNoise (voice);

    // The two heads and the air between them. The branch where they move
    // together is stiffened by that air into the snare's crack, and because it
    // is also the branch that radiates it is the first thing gone; the branch
    // where they oppose each other is what is left ringing under the wires.
    const float headModes = renderModalBank (
        voice, voice.transientScale * voice.excitationScale, true);

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
    const float wireMix = (0.18f + 0.82f * voice.characterA) * voice.wireScale;
    // The hoop is struck, not driven: it takes the contact and then rings on its
    // own, which is why it is fed the transient rather than the head.
    const float rim = voice.rimLevel <= 0.0f
        ? 0.0f
        : voice.rimLevel * voice.filterC.tick (
              voice.transientEnvelope * voice.transientScale
              + 0.22f * noise * voice.transientEnvelope);
    return 0.72f * ((0.62f - 0.38f * voice.characterA) * body
                    + (0.80f - 0.34f * voice.characterA) * headModes
                    + wireMix * wires + 0.35f * voice.characterB * snap
                    + rim);
}

float DrumEngine::renderClap (Voice& voice) noexcept
{
    // Two hands do not meet the same way twice in a row, and the four impacts
    // of a clap are four different collisions - different areas of skin,
    // different amounts of trapped air, different force. Giving each its own
    // size from the voice's own seed is the difference between a clap and one
    // impact played four times on a grid.
    for (std::size_t burstIndex = 0; burstIndex < voice.burstStarts.size(); ++burstIndex)
        if (voice.ageSamples == voice.burstStarts[burstIndex])
            voice.transientEnvelope += 0.72f + 0.42f * std::abs (signedUnitFromHash (
                voice.noiseState ^ static_cast<std::uint32_t> (
                    0x9e3779b9u * (burstIndex + 1u))));

    const float source = nextBandLimitedNoise (voice);
    const float direct = voice.filterB.tick (voice.filterA.tick (source));
    const float room = voice.filterC.tick (source);
    const float burst = (0.38f + 0.16f * voice.characterA)
        * voice.transientEnvelope * voice.transientScale;
    const float tail = (0.16f + 0.30f * voice.characterB) * voice.envelope;
    return direct * burst + room * tail;
}

float DrumEngine::renderHat (Voice& voice) noexcept
{
    const float noise = nextBandLimitedNoise (voice);
    // The persistent Schmitt/RC bank is evaluated once per engine sample, so
    // overlapping hits hear the same free-running hardware source instead of
    // restarting six ideal sines with newly randomized components.
    float metallic = metallicSourceFor (voice.instrument)
                   + 0.20f * (1.0f - voice.characterA) * noise;
    // The blunt contact of a foot chick, ahead of the two channel filters
    // rather than after them, because a contact decides what the strike puts
    // in and not how what is in gets out. filterC is unused on a stick-struck
    // hat and untouched by it, so this costs the other two articulations
    // nothing but the branch.
    if (voice.articulation == Articulation::FootChick)
        metallic = voice.filterC.tick (metallic);
    const float high = voice.filterA.tick (metallic);
    const float focused = voice.filterB.tick (metallic);
    const float attack = 0.12f * voice.transientEnvelope * noise * voice.transientScale
        * voice.velocityTimbre * voice.strikeNoise;

    // The plates, struck once and then left. The circuit bank above is the
    // hat's hiss and its clank; this is the metal it comes out of.
    // No velocity weighting here: the plate is the low half of what a hat
    // radiates, and scaling it against the hiss the way a filter corner is
    // scaled would make a quiet hat brighter than a loud one. Where the
    // strike strength belongs is the bank's tilt, above.
    const float plate = renderModalBank (
        voice, struckHeadScale * voice.transientScale * voice.excitationScale,
        false);

    return (0.58f * high + attack) * voice.envelope
         + (0.18f + 0.20f * voice.characterB) * focused * voice.auxiliaryEnvelope
         + (0.075f + 0.085f * voice.characterA) * plate;
}

float DrumEngine::renderCymbalVoice (Voice& voice, float outputGain) noexcept
{
    const auto& channel = voice.cymbal;
    // The six Schmitt-trigger oscillators, summed at the virtual earth the two
    // band-passes hang off. It free-runs behind the VCAs, so a strike samples
    // whatever the circuit happens to be doing rather than restarting it.
    const float oscillatorBank = metallicSourceFor (voice.instrument);
    const auto bands = renderCymbalBands (voice, oscillatorBank);

    // What the 909's counter reads out of its own ROM. It shares nothing with
    // the oscillator bank above: on the hardware these are two machines, and
    // feeding both from one source is what made the pair average into a single
    // timbre instead of sounding like either of them.
    const float pcm = nextCymbalPcm (voice);

    // The output buffer sums the three high-passed analogue bands and the
    // digital channel into one node, which is where the voice's own output
    // stage picks it up. Nothing else: neither machine has a modal plate, and
    // the one that used to sit here is what kept a pitched ring on top of two
    // circuits that do not produce one.
    return outputGain * (channel.lowGain * bands.low
                    + channel.midGain * bands.mid
                    + channel.highGain * bands.high
                    + pcm);
}

float DrumEngine::renderRide (Voice& voice) noexcept
{
    return renderCymbalVoice (voice, 1.14f);
}

float DrumEngine::renderCrash (Voice& voice) noexcept
{
    return renderCymbalVoice (voice, 1.02f);
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
    // The drawn sweep beside it had no strike term at all, so its depth is now
    // the strike's as well: same shape, same ceiling, but only an accent
    // reaches the top of it.
    const float frequency = voice.baseFrequency
        * (1.0f + voice.sweepAmount * voice.strikeDepth * voice.pitchEnvelope)
        * tension;
    voice.oscillatorFrequency = frequency;
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
    const float membrane = renderModalBank (
        voice, voice.transientScale * voice.excitationScale, true);

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
    // Density follows the flight: a few stragglers before and after, and the
    // body of the mass arriving in the middle of it.
    const float flight = advanceContact (voice);
    const float collisionsPerSecond = (320.0f + 4800.0f * voice.characterA)
        * (0.12f + 2.30f * flight + 0.16f * voice.envelope);
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
    const float plate = voice.filterC.tick (metallic) * voice.auxiliaryEnvelope;
    const float shaped = voice.filterA.tick (metallic) * voice.envelope + 0.12f * click;
    // Drive is handled by the shared antialiased stage. Keeping a single
    // nonlinear memory here avoids cascading a memoryless alias source.
    return 1.05f * shaped + (0.30f + 0.22f * voice.characterA) * plate;
}

float DrumEngine::renderPerc2 (Voice& voice) noexcept
{
    const float noise = nextBandLimitedNoise (voice);
    // A blow deposits an impulse and the bar answers by moving, so the modes are
    // set going rather than pushed through a gain meant for continuous noise -
    // which is what had a clave arriving fourteen decibels under everything
    // else on the panel.
    float body = 0.0f;
    if (voice.ageSamples < voice.modalActiveSamples)
    {
        if (voice.ageSamples == 0u)
        {
            const float impulse = struckHeadScale * voice.transientScale
                * voice.excitationScale;
            for (int mode = 0; mode < voice.modeCount; ++mode)
            {
                // A bar's overtones climb steeply - the fourth is nine times the
                // note - so the contact decides how many of them a blow can
                // reach at all. It is what separates a clave struck with a hard
                // stick from the same bar tapped with a finger.
                const float reach = contactSpectrum (
                    voice.baseFrequency * percBarRatios[static_cast<std::size_t> (mode)],
                    voice.contactSeconds);
                voice.resonators[static_cast<std::size_t> (mode)].strike (
                    impulse * reach
                        * voice.modeGains[static_cast<std::size_t> (mode)]);
            }
        }
        const float rub = 0.10f * modalNoiseScale_ * voice.characterB
            * voice.transientEnvelope * voice.transientScale * noise
            * voice.excitationScale;
        for (int mode = 0; mode < voice.modeCount; ++mode)
            body += voice.resonators[static_cast<std::size_t> (mode)].tick (rub);
    }
    // Stick content scales with velocity as well as level: a light tap on a
    // wooden or metallic body puts far less energy into the contact click.
    // The stick is only on the bar while it is on the bar.
    const float click = voice.filterA.tick (noise)
        * std::max (voice.transientEnvelope, advanceContact (voice))
        * voice.velocityTimbre;
    const float cavity = voice.filterC.tick (body);
    return 0.62f * voice.filterB.tick (body + 0.20f * voice.characterB * click)
         + (0.55f + 0.75f * voice.characterA) * cavity;
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


void DrumEngine::configureSympatheticBeds() noexcept
{
    for (std::size_t index = 0; index < sympatheticBeds_.size(); ++index)
        configureSympatheticBed (index);
}

void DrumEngine::configureSympatheticBed (std::size_t index) noexcept
{
    // The four things on a kit that answer when something else is struck: the
    // snare's resonant head with its wires lying on it, and the three toms.
    // A kick is the only drum that mostly does not - its heads are heavy, its
    // note is below almost everything else on the kit, and nobody has ever
    // complained about a bass drum ringing when the snare was hit.
    static constexpr std::array<Instrument, sympatheticBedCount> instruments {
        Instrument::Snare, Instrument::LowTom, Instrument::MidTom, Instrument::HighTom
    };
    // The snare's resonant head is tuned above its batter, and it is the wires
    // resting on it that decide the three modes worth carrying. The toms answer
    // at their own note and its first two membrane partials.
    static constexpr std::array<float, sympatheticModeCount> snareModes {
        1.18f, 1.88f, 2.52f
    };
    static constexpr std::array<float, sympatheticModeCount> tomModes {
        1.0f, 1.59f, 2.14f
    };
    static constexpr std::array<float, sympatheticBedCount> roots {
        185.0f, 82.0f, 123.0f, 174.0f
    };

    auto& bed = sympatheticBeds_[index];
    bed.instrument = instruments[index];
    bed.hasWires = bed.instrument == Instrument::Snare;
    bed.noiseState = hash32 (static_cast<std::uint32_t> (index + 1u) * 0x9e3779b9u);

    const auto values = snapshotParameters (bed.instrument);
    bed.lastPitch = values.pitch;
    bed.lastDecay = values.decay;
    applyPan (values.pan, bed.panLeft, bed.panRight);

    const float root = roots[index] * std::exp2 (values.pitch / 12.0f);
    // An undriven head rings for a fraction of a struck one: nothing is
    // putting energy into it except what the rest of the kit leaks, and on
    // the snare the wires damp it further.
    const float seconds = decaySecondsFor (bed.instrument, values.decay)
        * (bed.hasWires ? 0.30f : 0.45f);
    const auto& ratios = bed.hasWires ? snareModes : tomModes;
    bed.modeCount = 0;
    for (int mode = 0; mode < sympatheticModeCount; ++mode)
    {
        const float frequency = root * ratios[static_cast<std::size_t> (mode)];
        if (frequency >= 0.44f * static_cast<float> (sampleRate_))
            continue;
        auto& resonator = bed.resonators[static_cast<std::size_t> (bed.modeCount)];
        configureResonator (resonator, frequency, seconds);
        // configureResonator normalises a mode for being struck once. A head
        // that is driven continuously by a whole kit has to be normalised for
        // its resonant peak instead, which is inputGain/(1 - r) - a factor of
        // several hundred at these decay times, and the difference between a
        // sympathetic bed and a howl.
        resonator.inputGain *= 1.0f - std::sqrt (std::max (0.0f, -resonator.a2));
        ++bed.modeCount;
    }

    // What of the kit actually reaches a head that nobody is hitting: the
    // low-mid the shells and the floor carry, not the stick noise.
    configureBandpass (bed.drive, bed.hasWires ? 150.0f : root, 0.45f);
    if (bed.hasWires)
        configureBandpass (bed.wires, 4200.0f, 0.55f);
}

void DrumEngine::updateSympatheticBeds() noexcept
{
    for (std::size_t index = 0; index < sympatheticBeds_.size(); ++index)
    {
        auto& bed = sympatheticBeds_[index];
        const auto values = snapshotParameters (bed.instrument);
        applyPan (values.pan, bed.panLeft, bed.panRight);
        if (values.pitch == bed.lastPitch && values.decay == bed.lastDecay)
            continue;
        // Retuning a head clears what it was ringing with, which is what
        // slackening a lug does anyway - but only for the head actually being
        // retuned. This used to call configureSympatheticBeds() and rebuild
        // every one of the four beds as soon as any one of them changed, which
        // ran configureResonator() - and its trailing resonator.clear() - on
        // the other three as well, silencing whatever they were ringing with
        // for no reason: turning the Snare's own Decay knob could cut off a
        // Tom's still-ringing sympathetic bed that the knob never touched.
        configureSympatheticBed (index);
    }
}

void DrumEngine::clearSympatheticBeds() noexcept
{
    // A bed that stops being rendered would otherwise keep whatever it was
    // ringing with, and hand it back as a burst the next time the control came
    // off zero - the same stale-state click the bus detector already avoids.
    // The control's smoother lands exactly on zero, so this runs once per block
    // while the path is off rather than on every transition.
    for (auto& bed : sympatheticBeds_)
    {
        for (auto& resonator : bed.resonators)
            resonator.clear();
        bed.drive.clear();
        bed.wires.clear();
    }
}

void DrumEngine::renderSympatheticBeds (float excitation, float amount,
                                        float& left, float& right) noexcept
{
    for (auto& bed : sympatheticBeds_)
    {
        if (bed.modeCount == 0)
            continue;
        const float driven = bed.drive.tick (excitation);
        float displacement = 0.0f;
        for (int mode = 0; mode < bed.modeCount; ++mode)
            displacement += bed.resonators[static_cast<std::size_t> (mode)].tick (driven);

        float output = displacement;
        if (bed.hasWires)
        {
            // The same law the struck snare uses: wires only rattle while the
            // head under them lifts them off their resting contact, and below
            // that they damp it instead. It is why a kick makes a snare buzz at
            // all, and why the buzz appears suddenly as the kick gets louder
            // rather than fading up in proportion to it.
            const float noise = advanceXorshiftNoise (bed.noiseState);
            // A struck snare drives its own wires far clear of the head, so
            // the first-order law renderSnare() uses spends its life in that
            // law's saturating region. Sympathetic excitation does not: it
            // lives at the lift-off, where a first-order form has no dead zone
            // at all and simply tracks the exciter. The squared form does have
            // one - it falls away as the square below the threshold and
            // saturates above it - which is why a bled kick's buzz appears as
            // the kick gets loud instead of following it up from nothing.
            const float squared = displacement * displacement;
            constexpr float liftOff = 0.20f * 0.20f;
            const float rattle = squared / (liftOff + squared);
            output = 0.30f * displacement + 3.20f * bed.wires.tick (noise) * rattle;
        }

        const float scaled = std::clamp (0.80f * amount * output, -4.0f, 4.0f);
        left += scaled * bed.panLeft;
        right += scaled * bed.panRight;
    }
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
    {
        ++nonPositiveProcessCallCount_;
        return;
    }
    if (! prepared_)
        prepare (sampleRate_, std::max (maxBlockSize_, numSamples));

    // Whole blocks, so the count after a given stretch of audio is the same
    // however the host chose to divide it.
    engineSamples_ += static_cast<std::uint64_t> (numSamples);

    updateMetallicBankParameterTargets();
    updateSympatheticBeds();

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
    forEachVoice (observeVoice);

    const float gainTarget = outputGain_.load (std::memory_order_relaxed);
    const float gainSmoothing = gainSmoothingCoefficient_;
    const float dcCoefficient = dcBlockerCoefficient_;
    // setKitParameters() already ran these through clampUnit() before storing
    // them, so re-clamping an already-finite, already-[0,1] value here would
    // just reproduce it - the same idempotent-reclamp waste already trimmed
    // from constantPowerLeft/Right, and consistent with how reset() reads
    // these same three atomics with no re-clamp of its own.
    const float driveAmount = busDrive_.load (std::memory_order_relaxed);
    const float compressionAmount = busCompression_.load (std::memory_order_relaxed);
    const float bleedAmount = bleed_.load (std::memory_order_relaxed);
    // Refreshed per block, then smoothed into each ringing voice below so
    // channel-strip automation is audible on a tail rather than only on the
    // next hit.
    for (std::size_t index = 0; index < instrumentCount; ++index)
    {
        const auto values = snapshotParameters (static_cast<Instrument> (index));
        auto& mixer = mixerTargets_[index];
        mixer.levelGain = decibelsToGain (values.level);
        applyPan (values.pan, mixer.panLeft, mixer.panRight);
    }

    // The whole coupling path is skipped, not merely scaled to nothing, while
    // the control and its smoother are both at zero. A kit with Bleed off is
    // therefore bit-identical to the engine before it existed, and pays nothing.
    const bool bleedActive = bleedAmount > 0.0f || smoothedBleed_ > 0.0f;
    const bool busActive = driveAmount > 0.0f || compressionAmount > 0.0f
        || smoothedBusDrive_ > 0.0f || smoothedBusCompression_ > 0.0f;
    if (! busActive)
        resetBusStage();
    if (! bleedActive)
        clearSympatheticBeds();
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
            smoothedBleed_ = approachTarget (smoothedBleed_, bleedAmount, gainSmoothing);
            bleedExcitation_ = 0.0f;
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
        smoothedBleed_ = approachTarget (smoothedBleed_, bleedAmount, gainSmoothing);

        // The beds hear the previous sample's dry mix and nothing they added to
        // it, so the path is strictly feed-forward and cannot ring on itself.
        if (bleedActive)
        {
            const float excitation = bleedExcitation_;
            bleedExcitation_ = flushDenormal (dryLeft + dryRight);
            renderSympatheticBeds (excitation, smoothedBleed_, dryLeft, dryRight);
        }
        else
        {
            bleedExcitation_ = 0.0f;
        }

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
    //
    // The sympathetic beds are collapsed for the same reason and at the same
    // moment. Their excitation is the dry mix, so with no voice they are driven
    // by exact zero, but unlike the bus they are resonators: whatever they were
    // ringing with is held rather than decayed, because the silent path stops
    // clocking them. A choke or a CC 120 that ends the last voice mid-ring
    // would otherwise park that energy for an arbitrary silence and hand it
    // back on the next strike, which is precisely the stale burst
    // clearSympatheticBeds() already exists to prevent when Kit Bleed is
    // switched off.
    if (! anyVoiceActive_)
    {
        resetBusStage();
        clearSympatheticBeds();
    }

    outputLevelLeft_.store (meterPeakLeft_, std::memory_order_relaxed);
    outputLevelRight_.store (meterPeakRight_, std::memory_order_relaxed);
    busGainMeter_.store (busGain_, std::memory_order_relaxed);
    updateActiveVoiceCount();
}

} // namespace drumalor
