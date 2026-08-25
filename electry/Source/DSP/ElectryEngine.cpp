#include "ElectryEngine.h"

#include <algorithm>
#include <cmath>

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

// Magnetic aperture of one coil, and the distance between a humbucker's two.
//
// Both pickups sense the string through the same narrow per-bobbin window -
// a magnet or slug of about this diameter - and what separates them is that a
// humbucker sums two of those windows at two different distances from the
// bridge. The sum of two sensors separated by d has magnitude
// |cos(pi f d / c)| and first nulls at c/2d, where c = 2 L f_open is the
// string's transverse wave speed. A single window of width W nulls at c/W
// instead, most of an octave too high: modelling the humbucker as one 21 mm
// rectangle put string 2's first null at 5507 Hz where a 19 mm coil pair puts
// it at 3043 Hz, against the 3000 Hz Lemme measures on a low E and the
// 4000 Hz he measures on the A.
//
// The two coils also sit at two different distances from the bridge, so each
// sees its own position comb. Writing the pair out shows that this needs no
// second comb tap: with the position comb evaluated at the pickup's centre,
// the sum of the two coils factors exactly into that comb times the two-point
// sum above, so one extra fractional read per pickup buys both.
constexpr float coilApertureMetres = 0.0048f;
constexpr float humbuckerCoilSpacingMetres = 0.0190f;

// Weight on the second coil, which sets how deep the pair's dip goes:
// (1-b)/(1+b), 12 dB here. It is below one for the reason `pickupCombDepth`
// below is, and on the same reading - measured pickup responses notch by
// something like 6 to 15 dB rather than vanishing, because no real pickup is
// a pair of point sensors reading one plane of motion. The screw coil sits
// further from the string than the slug coil and reads quieter for it, so the
// two contributions cannot cancel however well the geometry lines up. Same
// value as `pickupCombDepth` and the same evidence behind it, kept a separate
// constant because the two nulls are different mechanisms.
constexpr float humbuckerCoilBalance = 0.60f;

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
// have. Explicitly modelling the humbucker's two coils was tried instead of
// this weight and measured no better on that objective - which is a statement
// about the low register only. The two coils are modelled above as well as
// this weight, because they set the notch frequency, which the objective that
// comparison used did not score.
constexpr float pickupCombDepth = 0.60f;

// Loaded electrical resonance of the two anchor pickup circuits (coil
// inductance and capacitance with typical pot and cable loading).
constexpr float humbuckerResonanceHz = 2000.0f;
constexpr float singleCoilResonanceHz = 6000.0f;
constexpr float humbuckerResonanceQ = 1.0f;
constexpr float singleCoilResonanceQ = 2.40f;

// The bridge hand's loss band: how deep it goes at full pressure, where it sits
// as a multiple of the sounding fundamental, and how wide it is. Fitted jointly
// with the relief below against nine dry muted power-chord references at five
// pitches, on an objective that scores the harmonic-number tilt and the
// peak-relative energy contour together so neither can be bought with the other.
//
// Anchoring the centre to the string's own series rather than to a frequency is
// the point: the loss a palm mute applies is a function of harmonic number, so
// the same shape has to work an octave down, where a fixed centre in hertz lands
// on a different partial.
//
// Swept: centres of 3, 3.5, 5, 7, 8, 10 and 12 times the fundamental against
// depths of 2 to 20 dB and Q of 0.5, 0.7 and 1.0. The centre is a clear minimum
// at five and it does not move with the relief - it is the best value at every
// relief tried from 20 to 30. Q is a clear minimum at 0.7. Depth saturates:
// 10 dB and 18 dB score within 0.05 dB of each other because the feasibility
// bisection has taken over by then, so the shallower of the two is used.
constexpr float handDipFullDepthDb = 10.0f;
constexpr float handDipCentreRatio = 5.0f;
constexpr float handDipQ = 0.70f;

// The fretting hand's calibrated Dead-note contact. The played string and the
// idle strings share that hand, so both loss paths use one physical decay
// target instead of letting the coupled strings ring as though the hand were
// absent. The sympathetic control maps 0..1 logarithmically from an uncovered
// four-second decay to a firmly stopped 45 ms decay; this helper is its exact
// inverse for a measured T60.
constexpr float deadHandT60 = 1.600f;
constexpr float sympatheticOpenHandT60 = 4.0f;
constexpr float sympatheticStoppedHandT60 = 0.045f;

float sympatheticHandMuteForT60(float t60) noexcept
{
    return clampf((std::log(sympatheticOpenHandT60) - std::log(t60))
                      / (std::log(sympatheticOpenHandT60)
                         - std::log(sympatheticStoppedHandT60)),
                  0.0f, 1.0f);
}

// How much of one transverse polarisation crosses into the other over one
// round trip of the string. The two meet at the bridge saddle and at the nut or
// fret, which is where a real string's polarisations exchange energy, so this
// is charged per round trip and not per rendered sample. A few per cent per
// reflection is what a compliant saddle does; the former per-sample constant
// worked out at 33% per round trip at the top of the range and over 900% at the
// bottom, which mixed the polarisations into one and dissipated the difference.
// Swept at 0.33, 0.16, 0.08, 0.04 and 0.02: everything at or below the open G3
// moves by under 0.3 dB in every decay window, while the top of the range gains
// monotonically. 0.04 takes the 22nd-fret high E from 32 dB down at half a
// second - inaudible before the next beat of a moderate tempo - to 13 dB down,
// which is the sustain the string's own fitted T60 was always asking for.
constexpr float polarisationCouplingPerRoundTrip = 0.04f;

// Bridge coupling between the strings that are actually being played.
//
// Every string terminates on the same saddle, whose mechanical admittance is
// finite, so the saddle velocity driven by the summed string forces drives
// every *other* string back. The engine already sums those forces into
// `sympatheticBus_` and already publishes them one sample late; this is the
// gain at which a played string reads that bus. It is a separate number from
// the `0.0045 * effectiveSympathetic` the *unfingered* strings read it at.
// That path is acyclic - only played voices write it, only idle voices read
// it - so it is stable at any gain. This one closes the loop, and its gain is
// bounded by a spectral radius rather than by taste.
//
// The one-sample publication delay makes the closed network an explicit Jacobi
// iteration on a matrix whose diagonal is exactly zero, because every voice
// subtracts its own contribution before reading, and whose off-diagonal
// entries are `g H_j`, where `H_j` is the transfer from an injection into
// string j's loop to that string's own output. At that string's resonance
// `|H_j| = 1 / (1 - G_j)`, and *not* the round-trip gain `G_j`: an expression
// written in `G_j` alone certifies gains that diverge in simulation. The
// coherent common mode drives all N - 1 other strings in phase, so the row-sum
// norm
//
//     (N - 1) * g * max_j 1 / (1 - G_j)  <=  bridgeCouplingRowSumBound
//
// is what has to hold, with 12 dB of margin at 0.25. It is not left as a
// calibration hope: solveBridgeCoupling() caps the gain by exactly that
// inequality every time the active set or the loop gains move, so the bound is
// structural at every parameter setting rather than only at the ones a test
// happens to sweep. An eight-string open chord runs loop gains from 0.993686
// to 0.998289, so `max_j 1/(1 - G_j)` is 584 there and the bound permits
// 6.1e-5; the nominal gain is that value, and the cap takes over wherever the
// strings are set to ring longer than that.
constexpr float bridgeCouplingGain = 6.0e-5f;
constexpr float bridgeCouplingRowSumBound = 0.25f;

// The strum. Three constants, all of them about the wrist rather than the
// string.
//
// `strumReAnchorSeconds` is the pre-roll every voice of a chord carries. A
// chord window is 35 ms and a host block is typically 5 to 10 ms, so one
// chord's note-ons routinely arrive across several `process()` calls; without
// a pre-roll the first arrival has already sounded by the time the string the
// pick actually starts from is known, and no causal scheduler can put the
// later arrival ahead of it. Holding every voice back by this much buys a
// window in which any of them may still turn out to be the anchor. It is a
// fixed time rather than a block count, so onsets do not depend on the host's
// buffer size, and it is charged only when Strum Spread is non-zero.
constexpr double strumReAnchorSeconds = 0.020;

// The pick enters the string plane at `v0` and accelerates through the
// strings, so `v(x) = sqrt(v0^2 + 2 a x)` and the crossing intervals compress
// as `dt_k = D / v(x_k)`. Written against the entry speed, with the strings
// evenly spaced, `v(x_k) = v0 sqrt(1 + u k)` for `u = 2 a D / v0^2` - one
// dimensionless number for the whole stroke. Requiring the last of the seven
// crossings to take 0.70 of the first gives `1 / sqrt(1 + 6u) = 0.70`, so
// `u = 1/(6*0.49) - 1/6`.
constexpr float strumAcceleration = 0.173469388f;

// How much of that the wrist does not repeat. The acceleration is a property
// of one stroke and is drawn once per chord; the per-crossing draw is what is
// left over, and it is small for a reason the shape itself sets - adjacent
// crossings differ by only 3 to 8 %, so a per-gap jitter of the same size
// would erase the compression rather than humanise it.
constexpr float strumAccelerationSigma = 0.15f;
constexpr float strumCrossingSigma = 0.005f;

constexpr float steelDensity = 7850.0f;      // kg/m^3
constexpr float steelYoungModulus = 2.0e11f; // Pa

// The waveguide is normalised for stable modal projection rather than stored
// in SI units. Mapping loop displacement back to metres lets the nonlinear
// stretch law use the same physical core, tension and length as the dispersion
// solve. The prior physical prototype
// measured 0.002831 loop mean-square at 2.05 mm, giving 0.03853 m per loop
// unit; 0.040 is that calibration rounded, and the rendered follower does not
// exceed the physical seed. Separately, the 4.44 N full-force model load gives
// 2.75 mm of E1 displacement at the default 27.625-inch scale/pick position,
// and therefore a 30-cent initial rise, consistent with the CC0 Drop-E reference
// without baking one string's displacement into all eight strings.
constexpr float waveguideDisplacementMetresPerUnit = 0.040f;
constexpr float fullForcePickLoadNewtons = 4.440f;

// A low eight-string is commonly set near 1.75 mm at the upper frets. Let a
// forceful, artifact-heavy stroke close a little under that nominal action and
// a gentler one require a little more travel, then convert through the same SI
// calibration as the tension model. The former 0.52..0.24 loop-unit range was
// 20.8..9.6 mm: even the engine's 2.75 mm full-force E1 seed could never reach
// it, so the alleged fret-collision branch was dead.
constexpr float looseFretClearanceMetres = 0.00208f;
constexpr float hardFretClearanceMetres = 0.00096f;

// clampf lives in DspMath.h, included via ElectryEngine.h; every call site in
// this file already resolved to it unqualified through that header.

bool finitef(float value) noexcept { return std::isfinite(value); }

float rateAdjustedCoefficient(float coefficientAt48k, float sampleRate) noexcept
{
    coefficientAt48k = clampf(coefficientAt48k, 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - coefficientAt48k, 48000.0f / sampleRate);
}

// A pickup's distance from the bridge becomes a comb-filter tap delay: the
// fraction of the string it sits under, converted to samples via the
// string's own period. Shared by configureVoicePickups' neck and bridge taps
// and configureSympatheticString's bridge tap, which all did this same
// distance/length -> fraction -> samples conversion independently, so the
// three could disagree about the 0.01..0.95 fraction clamp or the delay
// floor without it being obvious from any one call site.
float pickupTapDelaySamples(float distanceMetres, float stringLengthMetres,
                            float periodSamples, float maximumDelay) noexcept
{
    const float fraction = clampf(distanceMetres / stringLengthMetres,
                                  0.01f, 0.95f);
    return clampf(fraction * periodSamples, 2.0f, maximumDelay);
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
    handDip.reset();
    handEnvelope = 0.0f;
    handEnvelopePeak = 0.0f;
    dispersion1.reset();
    dispersion2.reset();
    dispersion3.reset();
    dispersion4.reset();
    dispersion5.reset();
    dispersion6.reset();
    dispersion7.reset();
    dispersion8.reset();
}

void ElectryEngine::DelayTap::setDelay(float delaySamples) noexcept
{
    constexpr float maximumDelay = static_cast<float>(delayLineSize - 8);
    delaySamples = delaySamples < 4.0f
        ? 4.0f
        : (delaySamples > maximumDelay ? maximumDelay : delaySamples);
    // The read is `writeIndex - delaySamples`, and the write index is an
    // integer, so the interpolation's unit interval starts `ceil(delay)` back
    // and its fractional position is `ceil(delay) - delay`.
    const float ceiling = std::ceil(delaySamples);
    offset = static_cast<int>(ceiling);
    const float t = ceiling - delaySamples;
    const float tMinus1 = t - 1.0f;
    const float tMinus2 = t - 2.0f;
    const float tPlus1 = t + 1.0f;
    c0 = -t * tMinus1 * tMinus2 * (1.0f / 6.0f);
    c1 = tPlus1 * tMinus1 * tMinus2 * 0.5f;
    c2 = -tPlus1 * t * tMinus2 * 0.5f;
    c3 = tPlus1 * t * tMinus1 * (1.0f / 6.0f);
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
        // Flexural and axial effective cores are separate empirical fits: a
        // winding can slip differently under bending and tension. Their
        // initial values intentionally retain the calibrated instrument while
        // allowing either fit to move independently when measured data exists.
        { 28, true, 2.0320f, 0.22f, 0.22f, 20.0f }, // E1, wound (.080)
        { 35, true, 1.5240f, 0.25f, 0.25f, 19.0f }, // B1, wound (.060)
        { 40, true, 1.0668f, 0.28f, 0.28f, 18.0f }, // E2, wound
        { 45, true, 0.8128f, 0.30f, 0.30f, 16.5f }, // A2, wound
        { 50, true, 0.6096f, 0.32f, 0.32f, 15.0f }, // D3, wound
        { 55, false, 0.4064f, 1.0f, 1.0f, 12.0f }, // G3, plain
        { 59, false, 0.2794f, 1.0f, 1.0f, 10.0f }, // B3, plain
        { 64, false, 0.2286f, 1.0f, 1.0f, 8.5f },  // E4, plain
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

    result.resonanceDepth = clampUnit(parameters.resonanceDepth,
                                      defaults.resonanceDepth);
    result.vibratoDepth = clampUnit(parameters.vibratoDepth,
                                    defaults.vibratoDepth);

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

float ElectryEngine::bendSensitivity(int stringIndex) noexcept
{
    // A vibrato bar (and this instrument's pitch wheel) works by stretching
    // every string by a comparable amount, and the pitch that stretch buys is
    // dF/F = dT/2T with dT = E*A*(dl/l): the string's elastic core stiffness
    // against its standing tension. For strings tuned over one scale length
    // that ratio reduces to (core fraction)^2 / (mass factor * f_open^2) - the
    // overall gauge cancels, because both the core area and the tension scale
    // with the diameter squared. The floppy low strings and the plain G are
    // therefore the deepest benders and the taut plain E the shallowest, which
    // is exactly the chord smear a real bar produces.
    //
    // The exponent compresses the raw physical spread (about six to one)
    // toward the two-to-one range measured on real tremolo bridges - the raw
    // law assumes the bar stretches every string equally, and a real bridge's
    // geometry evens the travel out. Normalised so the most compliant string
    // reaches the wheel's full nominal range and no string exceeds it.
    static const std::array<float, stringCount> sensitivities = []
    {
        std::array<float, stringCount> raw {};
        float maximum = 0.0f;
        for (int index = 0; index < stringCount; ++index)
        {
            const auto& spec = stringSpecs()[static_cast<std::size_t>(index)];
            const float fOpen = midiToHz(static_cast<float>(spec.openMidiNote));
            const float massScale = spec.wound ? 0.85f : 1.0f;
            raw[static_cast<std::size_t>(index)] =
                spec.axialCoreScale * spec.axialCoreScale
                / (massScale * fOpen * fOpen);
            maximum = std::max(maximum, raw[static_cast<std::size_t>(index)]);
        }
        for (auto& value : raw)
            value = std::pow(value / maximum, 0.35f);
        return raw;
    }();
    return sensitivities[static_cast<std::size_t>(
        std::clamp(stringIndex, 0, stringCount - 1))];
}

float ElectryEngine::onePolePhaseDelay(float coefficient, float omega) noexcept
{
    // H(z) = (1 - a) / (1 - a z^-1)
    const float angle = -std::atan2(coefficient * std::sin(omega),
                                    1.0f - coefficient * std::cos(omega));
    return omega > 1.0e-9f ? -angle / omega : coefficient / (1.0f - coefficient);
}

float ElectryEngine::onePoleMagnitude(float coefficient, float omega) noexcept
{
    // |1 - a e^-jw|^2 is written as (1 - a)^2 + 4 a sin^2(w/2) rather than as
    // 1 + a^2 - 2 a cos w. The two are the same number and neither is more
    // "physical", but the second one cannot be computed in float here. For the
    // low fundamental of a wound string at an oversampled rate both a and
    // cos w sit within a few parts per million of one, so the direct form
    // subtracts two quantities near two to land on a result near 2e-5: the
    // leading digits cancel and what is left is the float rounding of the
    // operands. Measured, that put the magnitude of the coupled low E's solved
    // filter 0.1% out at a 96 kHz host - which sounds negligible and is not,
    // because the loop gain that compensates it is inside a logarithm: a 0.1%
    // error in the magnitude moved the realised T60 by 5.6%, and by a different
    // amount at every host rate, which is exactly the rate dependence the loop
    // filters are solved to avoid. Both terms of this form are non-negative and
    // computed from small quantities, so nothing cancels.
    const float halfAngleSine = std::sin(0.5f * omega);
    const float gap = 1.0f - coefficient;
    const float denominator = gap * gap
                            + 4.0f * coefficient * halfAngleSine * halfAngleSine;
    return gap / std::sqrt(std::max(denominator, 1.0e-30f));
}

float ElectryEngine::solveOnePoleDamping(float magnitudeRatio, float omega0,
                                         float omegaHigh) noexcept
{
    // The ratio falls monotonically with the coefficient, so bisection is
    // reliable without a derivative. Eighteen halvings resolve the coefficient
    // to below 4e-6, well inside what the decay targets are known to.
    const float target = clampf(magnitudeRatio, 1.0e-4f, 1.0f);
    float low = 0.0f;
    float high = 0.9995f;
    for (int i = 0; i < 18; ++i)
    {
        const float mid = 0.5f * (low + high);
        if (onePoleMagnitude(mid, omegaHigh) / onePoleMagnitude(mid, omega0)
            > target)
            low = mid;
        else
            high = mid;
    }
    return 0.5f * (low + high);
}

void ElectryEngine::solveLoopLoss(float t60Fundamental, float t60High,
                                  float periodSamples, float sampleRate,
                                  float omega0, float omegaHigh,
                                  float gainCeiling, float& coefficientOut,
                                  float& gainOut) noexcept
{
    // Two decay targets are two constraints on one first-order filter and one
    // scalar, so they are not independent: the one-pole's own loss at the
    // fundamental has to be bought back by the loop gain, and the loop gain
    // cannot exceed one. A steep ratio drives the pole toward the unit circle,
    // where the filter's magnitude at a low fundamental collapses toward
    // (1 - a)/omega0, and the gain the fundamental would need to compensate
    // runs past unity. Solving the ratio and then clamping the gain - which is
    // what this did first - keeps the tilt and silently discards the
    // fundamental's target with it, which is the wrong trade in both
    // directions: the top end is gone either way, and now so is the note. On
    // the coupled bank that clamp turned an 8.97 s open low E into a 0.099 s
    // one at String Age 1.0, and every string into a click under the palm mute.
    //
    // The feasible thing to give up is the tilt. A high-frequency target equal
    // to the fundamental's is always solvable - the ratio is then one, the pole
    // is at zero, the filter is unity everywhere and the loop gain is exactly
    // the fundamental's, which is below one for any positive T60 - so there is
    // always a bracket. Feasibility is monotone in the high target because a
    // gentler tilt only ever moves the pole toward zero, so bisection finds the
    // darkest realisable filter that still keeps the fundamental where the
    // reference put it.
    const float decayExponent = -3.0f * periodSamples
                              / std::max(sampleRate, 1.0f);
    const float t60Low = std::max(t60Fundamental, 1.0e-3f);
    const float gainAtF0 = std::pow(10.0f, decayExponent / t60Low);

    const auto solveFor = [&] (float target, float& coefficient, float& gain)
    {
        const float gainAtHigh = std::pow(10.0f, decayExponent
                                                 / std::max(target, 1.0e-4f));
        coefficient = solveOnePoleDamping(gainAtHigh
                                              / std::max(gainAtF0, 1.0e-6f),
                                          omega0, omegaHigh);
        gain = gainAtF0 / std::max(onePoleMagnitude(coefficient, omega0),
                                   1.0e-6f);
        return gain <= gainCeiling;
    };

    const float requested = clampf(t60High, 1.0e-4f, t60Low);
    if (solveFor(requested, coefficientOut, gainOut))
        return;

    float infeasible = requested;
    float feasible = t60Low;
    solveFor(feasible, coefficientOut, gainOut);
    // Halving in log space: the two ends can be three orders of magnitude
    // apart, and what matters is the ratio between them.
    for (int i = 0; i < 18; ++i)
    {
        const float mid = std::sqrt(infeasible * feasible);
        float coefficient = 0.0f, gain = 0.0f;
        if (solveFor(mid, coefficient, gain))
        {
            feasible = mid;
            coefficientOut = coefficient;
            gainOut = gain;
        }
        else
        {
            infeasible = mid;
        }
    }
}

float ElectryEngine::highFrequencyDecayRatio(int stringIndex) const noexcept
{
    // A wound string is not a plain one with a thicker core: the wrap slides
    // over the core and dissipates bending energy, so its top end dies far
    // faster than its fundamental. Measured on the reference low E, content
    // above a kilohertz has effectively gone inside a tenth of a second while
    // the fundamental is still ringing seconds later.
    const auto& parameters = smoothedParameters_;
    const auto& spec = stringSpecs()[static_cast<std::size_t>(stringIndex)];
    float highRatio = lerp(0.036f, 0.010f, parameters.stringAge);
    highRatio *= spec.wound ? 1.0f : 7.5f;
    highRatio *= lerp(1.15f, 0.78f, parameters.stringGauge);
    highRatio *= lerp(0.86f, 1.16f, parameters.bodyWood);
    highRatio *= lerp(0.58f, 1.70f, parameters.construction);
    return highRatio;
}

void ElectryEngine::handDipCoefficients(float depth, const HandLossShape& shape,
                                        double& b0, double& b1, double& b2,
                                        double& a1, double& a2) noexcept
{
    b0 = 1.0;
    b1 = b2 = a1 = a2 = 0.0;
    if (depth <= 0.0f || shape.dipFullDepthDb <= 0.0f || shape.dipOmega <= 0.0f)
        return;

    // Computed in double, and then the numerator is renormalised so the section's
    // gain at DC is exactly one. Both are necessary rather than fastidious. The
    // section is analytically unity at DC - numerator and denominator both sum to
    // (2 - 2 cos w0)/a0 - but at the bottom of this instrument's range that
    // quantity is around 3.6e-5 formed by subtracting two numbers near two, so in
    // float the two sums disagree in their leading digits. Measured, that left a
    // real DC gain of 1.00066, and with the loop gain at its 0.99999 ceiling the
    // product exceeds one: a DC component in the loop would then grow by about
    // 0.07% per round trip forever, since the only DC blocker in this engine is
    // on the output and not inside the string. Normalising removes the mode
    // instead of relying on it being small, and can only ever scale the section
    // down, so the magnitude bound the loop depends on is preserved.
    const double gainDb = -(double) depth * (double) shape.dipFullDepthDb;
    const double a = std::pow(10.0, gainDb / 40.0);
    const double w = shape.dipOmega;
    const double alpha = std::sin(w) / (2.0 * std::max((double) shape.dipQ, 0.05));
    const double a0 = 1.0 + alpha / a;
    if (a0 < 1.0e-12)
        return;
    const double inv = 1.0 / a0;
    const double cosw = std::cos(w);
    double nb0 = (1.0 + alpha * a) * inv;
    double nb1 = (-2.0 * cosw) * inv;
    double nb2 = (1.0 - alpha * a) * inv;
    const double na1 = (-2.0 * cosw) * inv;
    const double na2 = (1.0 - alpha / a) * inv;

    const double sumB = nb0 + nb1 + nb2;
    const double sumA = 1.0 + na1 + na2;
    if (std::fabs(sumB) > 1.0e-30 && sumA > 0.0)
    {
        const double correction = sumA / sumB;
        nb0 *= correction;
        nb1 *= correction;
        nb2 *= correction;
    }

    b0 = nb0;
    b1 = nb1;
    b2 = nb2;
    a1 = na1;
    a2 = na2;
}

// Magnitude and phase of what the hand puts in the loop. Used by the decay
// solve, the tuning compensation and the per-voice setup, so none of the three
// can disagree about what is actually in the loop.
void ElectryEngine::handLossResponse(float depth, const HandLossShape& shape,
                                     float omega, float& magnitude,
                                     float& phase) noexcept
{
    magnitude = 1.0f;
    phase = 0.0f;
    if (depth <= 0.0f)
        return;

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    handDipCoefficients(depth, shape, b0, b1, b2, a1, a2);
    if (b1 == 0.0 && b2 == 0.0 && a1 == 0.0 && a2 == 0.0)
        return;

    // Evaluated at z = e^{j omega}, in double for the same cancellation reason
    // the section itself runs in double: the solve divides this magnitude out of
    // its targets, so an error here becomes an error in the fitted decay. The
    // second harmonic is the double angle of the first rather than a second
    // pair of std::cos/std::sin calls - configureVoicePitch() reaches this once
    // per control tick per polarisation for every voice whose pitch is still
    // moving (an active bend or vibrato), so this runs continuously rather
    // than only at note-on.
    const double dw = omega;
    const double dcw = std::cos(dw), dsw = std::sin(dw);
    const double c2 = 2.0 * dcw * dcw - 1.0, s2 = 2.0 * dsw * dcw;
    const double nr = b0 + b1 * dcw + b2 * c2;
    const double ni = -(b1 * dsw + b2 * s2);
    const double dr = 1.0 + a1 * dcw + a2 * c2;
    const double di = -(a1 * dsw + a2 * s2);
    const double dn = dr * dr + di * di;
    if (dn < 1.0e-20)
        return;
    magnitude *= (float) std::sqrt((nr * nr + ni * ni) / dn);
    phase += (float) (std::atan2(ni, nr) - std::atan2(di, dr));
}

// One place that pushes a depth into both bands, so the note-on solve and the
// per-control-period modulation of the dynamic models cannot diverge.
void ElectryEngine::applyDipDepth(PolarisationLoop& loop, float depth) noexcept
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    handDipCoefficients(depth, loop.handLossShape, b0, b1, b2, a1, a2);
    loop.handDip.b0 = b0; loop.handDip.b1 = b1; loop.handDip.b2 = b2;
    loop.handDip.a1 = a1; loop.handDip.a2 = a2;
    const bool active = b1 != 0.0 || b2 != 0.0 || a1 != 0.0 || a2 != 0.0;
    if (! active)
        loop.handDip.reset();
    loop.handDipActive = active;

    loop.handLossDepth = depth;
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

    VelocityProfile profile;

    // Level is the plectrum's deflection, and the deflection is linear in the
    // hand's force. A plectrum holding the string at fraction p of its length
    // needs a lateral force F = T y0 / (p (1-p) L) to hold it at y0, so
    // y0 = F p (1-p) L / T is linear in F, and a magnetic pickup senses
    // displacement. Reading MIDI velocity as that force gives an amplitude
    // that is linear in velocity, and the Velocity Response control is then
    // the exponent rather than a blend: it scales the decibel range linearly
    // and is an exact no-op at zero, because v_eff^0 is one for every stroke.
    // The 0.05 floor is the lightest stroke that still slips off the pick.
    const float force = 0.05f + 0.95f * v;
    profile.amplitude = std::pow(force, response);

    // Effort stays the stroke's force: it decides how hard the string meets
    // the frets (`collision`). The elastic pitch bloom reads the amplitude
    // force law directly below rather than carrying a second velocity curve.
    profile.effort = lerp(0.65f, v, response);

    // The contact spectrum does not follow that force, and the coupling
    // between the two is what used to flatten the top of the keyboard: the
    // extra level went into partials that had already decayed by the time the
    // attack was over. The string leaves the plectrum at the kink velocity
    // F / Z set by the string's transverse wave impedance Z, and it has to
    // travel a slip distance made of two parts - a grip depth d that the
    // stroke does not change, and the pick tip's own elastic recoil F / k. The
    // slip time is therefore
    //     t_s = Z d / F + Z / k,
    // whose second term is a floor no amount of force gets under: the
    // plectrum's stiffness bounds the contact spectrum, not the hand. Writing
    // s for the share of the slip that is pick recoil at a full-force stroke,
    // s = (F_max / k) / (d + F_max / k), the release rate normalised to its
    // full-force value is
    //     rate = 1 / ((1 - s) / F + s).
    // A 0.73 mm celluloid medium has a tip stiffness near 6 kN/m; deflecting
    // the low E by 5 mm at a seventh of its length needs about 4.8 N against
    // its 80 N of tension, so the tip recoils about 0.8 mm past a grip depth
    // near 0.2 mm, giving s = 0.8. The blend is taken in the rate rather than
    // in the force so that zero response leaves the release exactly where it
    // has always been.
    constexpr float pickRecoilShare = 0.80f;
    const float releaseRate = 1.0f
        / ((1.0f - pickRecoilShare) / force + pickRecoilShare);
    profile.releaseRate = lerp(smoothStep(0.65f), releaseRate, response);

    profile.brightness = lerp(0.20f, 2.10f, profile.releaseRate);
    profile.noise = lerp(1.0f, 0.18f + 0.82f * std::sqrt(v), response);
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
    {
        auto& voice = voices_[static_cast<std::size_t>(stringIndex)];
        voice.stringIndex = stringIndex;
        voice.stereoLateral = 2.0f * static_cast<float>(stringIndex)
                                  / static_cast<float>(stringCount - 1)
                              - 1.0f;
        voice.lowStringWeight = 1.0f
            - static_cast<float>(stringIndex) / static_cast<float>(stringCount - 1);
    }
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
    // Calibrated at 48 kHz like its neighbours. Left as a bare per-sample
    // literal it would give the hand a different physical response time at
    // every host rate.
    handEnvelopeCoefficient_ = rateAdjustedCoefficient(0.0015f, internalRate);
    retireAttackCoefficient_ = rateAdjustedCoefficient(0.01f, internalRate);
    retireReleaseCoefficient_ = rateAdjustedCoefficient(0.0009f, internalRate);
    artifactBandCoefficient_ = rateAdjustedCoefficient(0.12f, internalRate);
    // The palm-mute impact thud's fixed 85 Hz one-pole corner. This was
    // previously recomputed with std::exp on every sample a palm-impact
    // envelope was active, once per voice; it depends only on the internal
    // clock, so it belongs here with its neighbours instead.
    palmImpactThudCoefficient_ = 1.0f - std::exp(-twoPi * 85.0f * inverseSampleRate_);
    palmImpactVelocityRetention_ = std::pow(0.992f, 48000.0f / internalRate);
    // A 30 ms lag on the CC1 resonance control, advanced at the control tick:
    // fast enough to ride the wheel, slow enough that a coarse 7-bit
    // controller cannot step the coupling gain audibly.
    resonanceCoefficient_ = 1.0f - std::exp(
        -static_cast<float>(controlPeriod) / (0.030f * internalRate));
    // The wheel's glide coefficient follows the Bend Time parameter; force a
    // recompute at the first control tick of the new rate.
    appliedBendGlideSeconds_ = -1.0f;
    // A 60 ms window on the sympathetic ring's own energy follower: long
    // enough to survive a bar of ringing, short enough to retire a coupled
    // string promptly once it is inaudible.
    sympatheticEnergyCoefficient_ = 1.0f - std::exp(-1.0f / (0.060f * internalRate));

    const float controlTickRate = internalRate / static_cast<float>(controlPeriod);
    displayLevelAttack_ = 1.0f - std::exp(-1.0f / (0.010f * controlTickRate));
    displayLevelRelease_ = 1.0f - std::exp(-1.0f / (0.220f * controlTickRate));

    horizontalDetuneSamples_ = 0.11f * internalRate / 96000.0f;
    // Shared by configureVoicePitch() and configureSympatheticString(), which
    // used to each recompute this identical expression from the same two
    // fixed inputs - the former on every control tick of every active voice.
    voiceDelaySmoothing_ = 1.0f - std::exp(-static_cast<float>(controlPeriod)
                                           / (0.006f * internalRate));
    emfScale_ = internalRate / (twoPi * 220.0f);
    emfLowpassCoefficient_ = std::exp(
        -twoPi * std::min(16000.0f, 0.40f * internalRate) * inverseSampleRate_);

    // A strum's notes reach the engine within one host block; 35 ms is wide
    // enough to group a chord and short enough not to merge separate beats.
    chordWindowSamples_ = std::max(1, static_cast<int>(0.035 * sampleRate_));
    handReturnSamples_ = std::max(1, static_cast<int>(1.5 * sampleRate_));
    // The window inside which a later note-on may still turn out to be the
    // edge the stroke began at, and the pre-roll that keeps every voice of the
    // chord silent while it is open.
    strumReAnchorSamples_ = std::max(
        1, static_cast<int>(strumReAnchorSeconds * sampleRate_));
    // The vibrato's phase advances once per control tick, and the pressure
    // behind it ramps at a bounded rate - the time a player takes to land a
    // note before starting to move on it.
    vibratoPhaseIncrement_ = static_cast<float>(controlPeriod)
                           / static_cast<float>(sampleRate_);
    vibratoOnsetIncrement_ = static_cast<float>(controlPeriod)
                           / (vibratoOnsetSeconds
                              * static_cast<float>(sampleRate_));

    // The compliance table lives behind a guarded function-local static (its
    // initializer is not constexpr-able); touch it here so the one-time
    // initialization - and the lock an implementation may take for it -
    // happens on this thread rather than inside the first audio callback.
    (void) bendSensitivity(0);

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
        updateStyleWeights(voice);
    }

    smoothedParameters_ = sanitise(targetParameters_);
    pickStyle_ = PickStyle::Down;
    playStyle_ = PlayStyle::Sustain;
    alternateNextStrokeIsUp_ = false;
    noteSequence_ = variationSeed_;
    activeVoiceCount_ = 0;
    sympatheticStringCount_ = 0;
    controlCountdown_ = 0;
    pitchBendSemitones_ = pitchBendTarget_;
    sympatheticAppliedBend_ = pitchBendSemitones_;
    appliedBendGlideSeconds_ = -1.0f;
    resonanceAmount_ = resonanceTarget_;
    vibratoAmount_ = 0.0f;
    vibratoRamp_ = 0.0f;
    feedbackRing_.fill(0.0f);
    feedbackWriteIndex_ = 0;
    feedbackReadIndex_ = 0;
    feedbackAvailable_ = 0;
    feedbackCurrent_ = 0.0f;
    feedbackPrevious_ = 0.0f;
    feedbackGain_ = 0.0f;
    feedbackDrive_ = 0.0f;
    feedbackHandScale_ = 1.0f;
    returnLevel_ = returnLevelTarget_;
    sustainPedalDown_ = false;
    engineClock_ = 0;
    lastNoteOnClock_ = -(1ll << 40);
    lastHandContactClock_ = -1;
    lastHandContactPlayStyle_ = PlayStyle::Sustain;
    lastHandContactOrder_ = 0;
    chordAnchorString_ = 0;
    chordStrokeIsUp_ = false;
    chordAlternateConsumed_ = false;
    chordContactOccurred_ = false;
    chordFirstNoteOnClock_ = -(1ll << 40);
    chordSequence_ = variationSeed_;
    strumPreRollSamples_ = 0;
    chordTravelSamples_.fill(0);
    frettingHandPosition_ = 0.0f;
    palmMuteBlend_ = clampf(smoothedParameters_.palmMute + palmMutePressure_,
                            0.0f, 1.0f);
    appliedPalmMute_ = palmMuteBlend_;
    sympatheticBus_ = 0.0f;
    sympatheticBusDelayed_ = 0.0f;
    // CC1 raises the coupling from the parameter's base amount toward total.
    const float resonanceLift = resonanceAmount_
                              * smoothedParameters_.resonanceDepth;
    const float effectiveSympathetic = smoothedParameters_.sympatheticAmount
        + resonanceLift * (1.0f - smoothedParameters_.sympatheticAmount);
    sympatheticGain_ = 0.0045f * effectiveSympathetic;
    sympatheticInjection_ = sympatheticGain_ * (1.0f - palmMuteBlend_);
    bridgeCouplingNominal_ = bridgeCouplingGain * effectiveSympathetic
                           * (1.0f - palmMuteBlend_);
    bridgeCouplingInjection_ = 0.0f;
    bridgeCouplingRowSum_ = 0.0f;
    sympatheticHandGain_ = 1.0f;
    sympatheticHandGainTarget_ = 1.0f;
    sympatheticHandMute_ = -1.0f;
    sympatheticActive_ = effectiveSympathetic > 0.0f;

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
        auto& mode = artifactRingModes_[static_cast<std::size_t>(stringIndex)];
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
    stereoSideScale_ = 0.24f * stereoWidth_;
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

void ElectryEngine::setResonance(float normalised) noexcept
{
    resonanceTarget_ = std::isfinite(normalised)
        ? clampf(normalised, 0.0f, 1.0f) : 0.0f;
}

void ElectryEngine::setAcousticReturnLevel(float normalised) noexcept
{
    returnLevelTarget_ = std::isfinite(normalised)
        ? clampf(normalised, 0.0f, 1.0f) : 0.0f;
}

void ElectryEngine::pushAcousticReturn(const float* left, const float* right,
                                       int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;
    if (right == nullptr)
        right = left;

    // The ring keeps roughly one host block of speaker signal in flight.
    // Anything still unread from an earlier push is stale: with a steady
    // block size the reader has always drained the previous batch by now, so
    // a leftover only appears when the host's block size just shrank - and
    // keeping it would ratchet the modeled speaker-to-string latency up to
    // the largest block ever seen, permanently. Dropping it re-anchors the
    // path to one block and self-heals after a size change.
    if (feedbackAvailable_ > 0)
    {
        feedbackReadIndex_ = feedbackWriteIndex_;
        feedbackAvailable_ = 0;
    }

    // If a hostile caller pushes more than the ring holds, the oldest samples
    // are dropped: the read index is advanced so the path stays a delay
    // rather than becoming an ever-growing queue.
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float value = 0.5f * (left[sample] + right[sample]);
        if (! std::isfinite(value))
            value = 0.0f;
        feedbackRing_[static_cast<std::size_t>(feedbackWriteIndex_)] = value;
        feedbackWriteIndex_ = (feedbackWriteIndex_ + 1) & (feedbackRingSize - 1);
        if (feedbackAvailable_ < feedbackRingSize)
            ++feedbackAvailable_;
        else
            feedbackReadIndex_ = (feedbackReadIndex_ + 1)
                               & (feedbackRingSize - 1);
    }
}

void ElectryEngine::setVibrato(float normalised) noexcept
{
    vibratoTarget_ = clampf(std::isfinite(normalised) ? normalised : 0.0f,
                            0.0f, 1.0f);
}

void ElectryEngine::setPalmMutePressure(float normalised) noexcept
{
    palmMutePressure_ = std::isfinite(normalised)
        ? clampf(normalised, 0.0f, 1.0f) : 0.0f;
    // CC2 and a note-on commonly share one MIDI sample. Note setup reads this
    // cached blend before the render loop gets a control tick, so refresh it
    // here as well or that note's one-shot excitation and palm impact are
    // configured as open even though its loop becomes muted one sample later.
    palmMuteBlend_ = clampf(smoothedParameters_.palmMute + palmMutePressure_,
                            0.0f, 1.0f);
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
        // The two banks latch independently: a picking-style switch never
        // touches the play style and vice versa, so any of the twenty-one
        // combinations survives switching either half.
        const int index = midiNote - firstKeyswitchNote;
        if (index < pickStyleKeyswitchCount)
        {
            pickStyle_ = static_cast<PickStyle>(index);
            if (pickStyle_ == PickStyle::Alternate)
                alternateNextStrokeIsUp_ = false;
        }
        else
        {
            playStyle_ = static_cast<PlayStyle>(index - pickStyleKeyswitchCount);
        }
        return;
    }

    if (! isPlayableNote(midiNote) || velocity <= 0.0f)
        return;

    // A hammered note has no pick stroke at all, so the latched picking style
    // neither colours it nor advances the alternate sequence - a legato run
    // in the middle of alternate picking resumes on the stroke it left off.
    const bool picked = playStyle_ != PlayStyle::Hammer;
    const bool strokeCandidateIsUp = pickStyle_ == PickStyle::Up
        || (pickStyle_ == PickStyle::Alternate && alternateNextStrokeIsUp_);

    // A programmed strum accepts note-ons for a bounded window measured from
    // the chord's first event, because a host can deliver one chord over several
    // blocks. Measuring successive gaps instead would let a staircase of notes
    // extend one stroke indefinitely. At zero spread there is no delayed chord
    // to assemble: only truly simultaneous note-ons share a stroke, while a
    // rapid cross-string riff advances Alternate on every hit.
    const float spreadSeconds = targetParameters_.strumSpreadSeconds;
    const int activeChordWindowSamples = spreadSeconds > 0.0f
        ? chordWindowSamples_ : 0;
    bool newChord = engineClock_ - chordFirstNoteOnClock_
                  > static_cast<std::int64_t>(activeChordWindowSamples);

    // The hand relaxes to the nut when the phrase ends: nothing is held and
    // no note has arrived for over a second. Without this a figure played high
    // up would keep pulling a following open-position chord out of position.
    if (newChord
        && engineClock_ - lastNoteOnClock_
               > static_cast<std::int64_t>(handReturnSamples_))
    {
        bool anyHeld = false;
        for (const auto& voice : voices_)
            anyHeld = anyHeld || (voice.active && voice.keyDown);
        if (! anyHeld)
            frettingHandPosition_ = 0.0f;
    }

    const int stringIndex = chooseString(midiNote, playStyle_);
    if (stringIndex < 0)
        return;

    auto& voice = voices_[static_cast<std::size_t>(stringIndex)];

    // A wrist stroke cannot cross the same string twice. Even inside the chord
    // window, reusing a string already assigned to this chord is a new stroke;
    // this keeps 25-35 ms tremolo picking alternate while different strings
    // arriving in the same window still share one chord direction.
    if (! newChord && chordSequence_ != 0
        && voice.strumChordId == chordSequence_)
        newChord = true;

    // Strum travel. The pick enters the neck at one edge and crosses the chord
    // in one direction, so the edge is the chord's extreme string *in the
    // stroke's direction* - not whichever note-on the host happened to send
    // first - and the offset is the signed distance from it rather than the
    // absolute one. At zero spread its timing is exactly a simultaneous block.
    //
    // Read the sanitised target rather than the smoothed copy. This control
    // schedules the stroke instead of shaping it, so the control tick copies it
    // verbatim; a chord whose note-ons land at offset 0 of the same block as
    // the automation change would otherwise be scheduled with the previous
    // block's spread, usually as a block chord.
    if (newChord)
        beginChordStroke(stringIndex, strokeCandidateIsUp, spreadSeconds);
    else if (strumPreRollSamples_ > 0
             && engineClock_ - chordFirstNoteOnClock_
                    < static_cast<std::int64_t>(strumPreRollSamples_))
        reAnchorChordStroke(stringIndex);

    updateFrettingHand(
        midiNote - stringSpecs()[static_cast<std::size_t>(stringIndex)].openMidiNote,
        newChord);
    lastNoteOnClock_ = engineClock_;

    int startDelaySamples = 0;
    if (strumPreRollSamples_ > 0)
    {
        // One clock for the whole chord: the pick reaches this string at the
        // chord's first note-on plus the pre-roll plus the travel, whenever
        // this particular note-on arrived.
        const int crossings = chordStrokeIsUp_
            ? chordAnchorString_ - stringIndex
            : stringIndex - chordAnchorString_;
        const std::int64_t onset = chordFirstNoteOnClock_
                                 + static_cast<std::int64_t>(strumPreRollSamples_)
                                 + static_cast<std::int64_t>(
                                       strumTravelSamples(std::max(0, crossings)));
        startDelaySamples = static_cast<int>(
            std::max<std::int64_t>(0, onset - engineClock_));
    }
    voice.strumChordId = chordSequence_;

    const bool legato = (playStyle_ == PlayStyle::Hammer
                         || playStyle_ == PlayStyle::Slide)
                     && voice.active
                     && voice.midiNote != midiNote;
    const bool plectrumStroke = picked && ! legato;
    const bool strokeIsUp = plectrumStroke && chordStrokeIsUp_;

    // One chord is one wrist stroke: every crossed string keeps the direction
    // beginChordStroke() resolved, and Alternate advances only once. Waiting
    // until here preserves the legato rule too—a Slide onto a ringing string
    // and a Hammer strike no plectrum, so neither consumes the chord's stroke.
    if (plectrumStroke && pickStyle_ == PickStyle::Alternate
        && ! chordAlternateConsumed_)
    {
        chordAlternateConsumed_ = true;
        alternateNextStrokeIsUp_ = ! chordStrokeIsUp_;
    }

    if (legato)
        legatoRetarget(voice, midiNote, velocity, playStyle_);
    else
        startVoice(voice, midiNote, velocity, playStyle_, strokeIsUp,
                   startDelaySamples);

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
        if (voice.keyDownCount > 1)
        {
            --voice.keyDownCount;
            continue;
        }
        voice.keyDownCount = 0;
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
        voice.keyDownCount = 0;
        voice.keyDown = false;
        voice.sustained = false;
        beginVoiceRelease(voice);
    }
}

int ElectryEngine::chooseString(int midiNote, PlayStyle playStyle) const noexcept
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
    // note stays within a reachable stretch of the fretting hand. A slide has
    // no such limit: the finger stays down and travels, so anything on the
    // same string is reachable, which is the point of the articulation.
    if (playStyle == PlayStyle::Hammer || playStyle == PlayStyle::Slide)
    {
        int best = -1;
        int bestDistance = playStyle == PlayStyle::Slide ? fretCount + 1 : 10;
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

    // Otherwise the fretting hand chooses: the free string that puts the note
    // nearest the fingers, with open strings always available because they
    // need no finger. Ties resolve toward the thicker string, exactly as they
    // did under the lowest-fret rule this replaces.
    int best = -1;
    float bestCost = 1.0e30f;
    for (int s = 0; s < stringCount; ++s)
    {
        if (! playable(s) || voices_[static_cast<std::size_t>(s)].active)
            continue;
        const float cost = frettingCost(fretOn(s));
        if (cost < bestCost)
        {
            bestCost = cost;
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
        // Older starts win; releasing voices win over held voices. A pending
        // same-string repick has already reserved its event order even though
        // its audible attack state is deliberately not committed until contact.
        const std::uint64_t effectiveStartOrder = voice.pendingRepick.active
            ? voice.pendingRepick.startOrder : voice.startOrder;
        const std::uint64_t age = ~effectiveStartOrder;
        const bool effectivelyReleasing = voice.releasing
                                       && ! voice.pendingRepick.active;
        return effectivelyReleasing ? (age | (1ull << 63))
                                    : (age & ~(1ull << 63));
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

float ElectryEngine::frettingCost(int fret) const noexcept
{
    // An open string costs no finger at all, so in first position it is free -
    // which is why the open-position chord shapes come out unchanged. Up the
    // neck it is a decision rather than a convenience: taking it abandons the
    // hand's position and changes the note's timbre, decay and damping, so its
    // cost grows with how far the hand has travelled.
    if (fret <= 0)
        return 0.25f * frettingHandPosition_;

    const float low = frettingHandPosition_;
    const float high = low + static_cast<float>(frettingHandReach);
    if (fret < low)
    {
        // The hand pivots forward from the thumb, so reaching back below the
        // index finger is the more expensive of the two directions.
        return 1.6f * (low - static_cast<float>(fret));
    }
    if (fret > high)
        return static_cast<float>(fret) - high;

    // Inside the hand's span every note is reachable; the small tilt prefers
    // the index finger, which breaks ties the way a player's hand does.
    return 0.05f * (static_cast<float>(fret) - low);
}

void ElectryEngine::updateFrettingHand(int fret, bool newChord) noexcept
{
    // An open string tells the hand nothing, and a chord is one shape: only
    // the note that opens a chord window is allowed to move the hand, so a
    // barre or a stretch is fingered from one position instead of dragging
    // the hand across itself note by note.
    if (fret < 1 || ! newChord)
        return;

    const float low = frettingHandPosition_;
    const float high = low + static_cast<float>(frettingHandReach);
    if (static_cast<float>(fret) >= low && static_cast<float>(fret) <= high)
        return;

    // When the hand does shift, it lands with the note under the middle of the
    // fingers rather than under the index, leaving reach in both directions.
    constexpr int settleOffset = 2;
    frettingHandPosition_ = clampf(
        static_cast<float>(fret - settleOffset), 0.0f,
        static_cast<float>(fretCount - frettingHandReach));
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

void ElectryEngine::configureVoiceDamping(Voice& voice,
                                          PlayStyle dampingStyle) noexcept
{
    voice.dampingStyle = dampingStyle;
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
    // `handT60` is the bridge hand - the Palm Mute style and the continuous
    // pressure - whose loss is tilted with frequency below. `chokeT60` is the
    // fretting hand's dead-note choke, which is not near the bridge and stays
    // broadband. They still combine in parallel, so a dead note played under
    // palm-mute pressure gets both.
    float handT60 = 0.0f;
    float chokeT60 = 0.0f;
    if (dampingStyle == PlayStyle::PalmMute)
    {
        handT60 = std::exp(lerp(std::log(2.60f), std::log(0.32f),
                                parameters.muteDamping));
    }
    else if (dampingStyle == PlayStyle::Dead)
    {
        // The fretting hand laid across the strings without pressing them to
        // the fret. It is the whole hand rather than the heel, and it is
        // nowhere near the bridge, so it takes the fundamental directly - the
        // difference between a dead note and a very tight palm mute, and why
        // the mute's mode-shape relief below must not reach this term. The old
        // 30 ms target erased the E1 body by 34-51 dB over 30-250 ms. Four CC0
        // Drop-E ghost hits instead retain a dark periodic thunk through that
        // interval; this target puts all three measured decay windows inside
        // their observed range while remaining about 16 dB below Sustain in
        // the 100-250 ms window.
        chokeT60 = deadHandT60;
    }
    // A harmonic used to clamp T60 to 3.8 s here, standing in for the touching
    // finger. The finger is now in the loop as the node touch it actually is,
    // and it lifts once the note has formed, so the surviving partials decay
    // at the string's own rate - which is why a natural harmonic on an open
    // string outlasts the fretted note rather than dying before it. Measured
    // on the open A2, the clamp cost the octave partial 16 dB/s of extra loss
    // that nothing physical was asking for.

    // High-frequency decay target relative to the fundamental's decay, at the
    // `fHigh` reference below.
    //
    // The ratio was previously an order of magnitude too generous, which is
    // what kept a low note's 1-2 kHz partials sounding for as long as its
    // fundamental - the nasal, clavinet-like register the reference does not
    // have. The law itself is shared with the bridge-coupled strings.
    float highRatio = highFrequencyDecayRatio(voice.stringIndex);

    // Continuous bridge-hand damping. Unlike the Muted and Chug keyswitches
    // this is a smooth pressure that applies to every play style, so a phrase
    // can open from a dead chug into a ringing chord without a style change.
    // It is the same absorber as those styles, so it joins them in parallel
    // rather than overriding them, and a pressure of exactly zero remains a
    // mathematical no-op. The heel of the hand is a soft, lossy contact, so it
    // also darkens the string as it covers more of it.
    if (palmMuteBlend_ > 0.0f)
    {
        // The mapped time is what a hand at this pressure would impose once it is
        // on the string; the rate is that scaled by the pressure itself, so it
        // vanishes as the pressure does. Without the scaling the mapped time
        // tends to four seconds rather than to infinity, so the first nonzero
        // value of the parameter or of CC2 applied a quarter-per-second loss
        // rate from nothing - a step that was inaudible while it only nudged the
        // decay, and became audible once it also gated the loss shelf's depth.
        // At full pressure the rate is unchanged, so the calibrated endpoint
        // stands.
        const float pressureRate = palmMuteBlend_
            / std::exp(lerp(std::log(4.0f), std::log(0.080f), palmMuteBlend_));
        const float pressureT60 = pressureRate > 0.0f ? 1.0f / pressureRate
                                                      : 0.0f;
        handT60 = handT60 > 0.0f
            ? 1.0f / (1.0f / handT60 + 1.0f / pressureT60)
            : pressureT60;
        highRatio *= lerp(1.0f, 0.62f, palmMuteBlend_);
    }
    highRatio = clampf(highRatio, 0.0015f, 0.9f);

    // How much of the string's loss the bridge hand accounts for, for the
    // loss shelf below. Zero means no hand and an exactly bypassed shelf.
    float handShareForSlope = 0.0f;
    // The string's own targets, before the hand.
    float t60High = t60 * highRatio;
    if (handT60 > 0.0f)
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
        // carry the harmonic tilt as well - and with the dip below in the loop it
        // is not asked to, which is exactly why this divisor is now far larger
        // than the mode shape alone suggests.
        //
        // The two are complementary rather than competing, and that is the whole
        // finding. The relief lengthens the tail; the dip removes the harmonics.
        // Applied alone each one is nearly a wash on the joint objective - the
        // relief buys contour and gives back tilt, the dip the reverse - but
        // together they separate the two terms that used to be one degree of
        // freedom. Re-swept against the five matched reference pitches with the
        // dip present, the joint objective reads 6.20, 6.05, 6.03, 6.05, 6.09 dB
        // at divisors of 16, 19, 22, 25 and 28: a flat minimum, so the middle of
        // it is used rather than an edge. The old value of six scores 8.43 there.
        //
        // What set the ceiling before was measurement, not taste. The reference
        // fundamental barely decays under a mute at all - its level moves +1.0 to
        // -2.8 dB over the first third of a second - so the divisor wants to be
        // large. It is bounded above by the suite's own requirement that a muted
        // note decay dramatically faster than an open one, which still holds at
        // 70 and fails at 110, so this sits with about a factor of four in hand.
        //
        // The 2025 Guitar-TECHS DI set independently confirms that the upper
        // band has to leave first. Raising this tilt from 3.0 to 4.5 moves the
        // default E2's paired >500 Hz loss to -4.85/-15.57 dB in the 50-150 and
        // 150-500 ms windows while its <500 Hz figures move by only 0.06/0.04
        // dB. At maximum Palm Tightness those upper-band losses become
        // -11.20/-28.73
        // dB, spanning the first player's -11.18/-21.82 dB without forcing the
        // second player's much firmer contact onto every muted note.
        constexpr float handHighFrequencyTilt = 4.5f;
        constexpr float handFundamentalRelief = 22.0f;
        // A harder stroke drives the string farther into the hand and makes a
        // tighter contact. This scale is latched at attack from the same force
        // and deterministic stroke draw as the excitation: no second velocity
        // curve and no extra random player. Multiplying the final positive rate
        // preserves exact no-hand bypass and monotonic pressure/mute controls.
        const float handRate = handT60 > 0.0f
            ? voice.handContactScale / handT60 : 0.0f;
        // The dip's depth follows this rather than switching on. It is the
        // hand's share of the string's total loss rate, so it is zero without a
        // hand, near one under a firm mute, and continuous in between - which
        // matters because Palm Mute and CC2 are swept live. Gating full depth on
        // any positive rate put a step in the middle of an expression pedal.
        handShareForSlope = handRate / std::max(1.0f / t60 + handRate, 1.0e-9f);
        t60 = 1.0f / (1.0f / t60 + handRate / handFundamentalRelief);
        t60High = 1.0f / (1.0f / t60High + handRate * handHighFrequencyTilt);
    }
    if (chokeT60 > 0.0f)
    {
        // Broadband and in parallel, like every other contact in this model,
        // so a dead note played under palm-mute pressure gets both.
        const float chokeRate = 1.0f / chokeT60;
        t60 = 1.0f / (1.0f / t60 + chokeRate);
        t60High = 1.0f / (1.0f / t60High + chokeRate);
    }
    t60 = clampf(t60, 0.02f, 26.0f);
    t60High = clampf(t60High, 0.008f, t60);

    const float sampleRate = static_cast<float>(sampleRate_);
    const float f0 = voice.baseFrequency;
    // A distributed fretting hand shapes the low-order modes that make up a
    // dead note's whole audible body, so fit its upper decay target at the
    // eighth partial. The bridge hand and ordinary string still use 3.6 kHz;
    // moving their anchor would change every sustained and palm-muted note.
    const float fHigh = dampingStyle == PlayStyle::Dead
        ? std::min(8.0f * f0, 0.32f * sampleRate)
        : std::min(3600.0f, 0.32f * sampleRate);
    const float omega0 = twoPi * f0 * inverseSampleRate_;
    const float omegaHigh = twoPi * std::max(fHigh, f0 * 1.5f) * inverseSampleRate_;

    // The shape is shared; how much of it is feasible is settled per
    // polarisation below, because that depends on the loop's own decay ratio.
    // It follows the hand's share of the total loss rate, so it is zero without a
    // hand and continuous in between - Palm Mute and CC2 are swept live.
    const float dipRequestedDepth = clampf(handShareForSlope, 0.0f, 0.95f);
    HandLossShape shape {};
    // Clamped below Nyquist so a high note cannot fold the centre.
    shape.dipOmega = dipRequestedDepth > 0.0f
        ? twoPi * clampf(handDipCentreRatio * f0, 40.0f, 0.40f * sampleRate)
              * inverseSampleRate_
        : 0.0f;
    shape.dipQ = handDipQ;
    shape.dipFullDepthDb = handDipFullDepthDb;

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

        loop.handLossShape = shape;
        // Divide the dip out of both targets. This is the whole point: the
        // one-pole is then solved for what is left, so the envelope stays where
        // the references put it and the dip is free to bend the curve in
        // between. A muted note spends most of its loop gain on the mute
        // itself, which is what leaves the headroom for this to be possible -
        // on an unmuted 20 s decay the same compensation would ask for a loop
        // gain above unity and clamp.
        //
        // The dip can only be as deep as the note's own decay ratio leaves
        // room for. A one-pole cannot raise the high end relative to the low, so
        // if the dip's own tilt is already steeper than the total the targets
        // ask for, the remainder is unsolvable. Clamping that quotient to one -
        // which is what this did first - hid the problem and let it through: the
        // damping pole collapsed to near zero, the loop gain hit its ceiling, and
        // muted notes above about A2 had their decay set by the shelf instead of
        // by their fitted T60s. Bisecting the depth down until the remainder is
        // solvable keeps the fit honest and simply gives those notes a shallower
        // shelf, which is the correct answer rather than a silent failure.
        // Solve the one-pole coefficient whose magnitude ratio between f0 and
        // fHigh matches the decay-time ratio, through the shared bisection.
        //
        // How deep the shelf can be is bounded by the note, and by two separate
        // limits rather than one. A one-pole cannot raise the high end relative
        // to the low, so the shelf's own tilt must leave a solvable remainder;
        // and the loop gain cannot exceed unity, so the shelf's loss at the
        // fundamental must fit inside the headroom the note has there. The
        // second limit is the tighter one at the top of the range, where a short
        // period means very little loss per round trip: with only the ratio
        // constrained, muted notes on the top string above about the eighteenth
        // fret still drove the gain into its ceiling and decayed by the shelf
        // rather than by their fitted T60s.
        //
        // Both limits are checked by solving the whole thing for a candidate
        // depth, which is cheap because it only happens when a voice is
        // configured. The predicate is monotonic in depth - a deeper shelf makes
        // both limits harder - so the outer bisection is as reliable as the
        // inner one.
        const auto solveFor = [&] (float depth, float& coefficientOut,
                                   float& gainOut, float& dipAtF0Out)
        {
            float atF0 = 1.0f, atHigh = 1.0f, unused = 0.0f;
            handLossResponse(depth, shape, omega0, atF0, unused);
            handLossResponse(depth, shape, omegaHigh, atHigh, unused);
            atF0 = clampf(atF0, 1.0e-3f, 1.0f);
            atHigh = clampf(atHigh, 1.0e-3f, 1.0f);
            // The shelf must not be what makes the remainder unsolvable. The
            // baseline removes the shelf's extra constraint. It can still ask
            // for more fundamental compensation than a stable scalar gain can
            // supply on a very low, tightly muted string; that separate failure
            // is handled below by the same bounded loop-loss solver used by the
            // sympathetic strings. The 0.98 leaves the one-pole a little to do,
            // so it stays the thing that meets the targets and the shelf stays a
            // correction to its shape.
            const float rawTarget = ratio / (atHigh / atF0);
            if (depth > 0.0f && rawTarget > 0.98f)
                return false;
            coefficientOut = solveOnePoleDamping(rawTarget, omega0, omegaHigh);
            dipAtF0Out = atF0;
            gainOut = gainAtF0
                / std::max(onePoleMagnitude(coefficientOut, omega0) * atF0,
                           1.0e-6f);
            return gainOut <= 0.99999f;
        };

        float coefficient = 0.0f, gain = 0.0f, dipF0 = 1.0f;
        float depth = dipRequestedDepth;
        if (depth <= 0.0f)
        {
            // Preserve the exact no-hand path. Its historical upper-target
            // clamp is part of the calibrated open string; the discontinuity
            // below can only occur while a hand depth is being swept.
            solveFor(0.0f, coefficient, gain, dipF0);
        }
        else if (! solveFor(depth, coefficient, gain, dipF0))
        {
            float lowDepth = 0.0f, highDepth = depth;
            if (! solveFor(0.0f, coefficient, gain, dipF0))
            {
                // Even the zero-shelf target can be overconstrained: forcing
                // its one-pole ratio and then clamping the required loop gain
                // creates abrupt coefficient/delay jumps as Palm Pressure
                // crosses the feasibility boundary. Preserve the fundamental
                // and relax only the upper decay target, exactly as the common
                // bounded solver does for sympathetic strings.
                depth = 0.0f;
                dipF0 = 1.0f;
                solveLoopLoss(t60Fundamental, t60HighTarget, period,
                              sampleRate, omega0, omegaHigh, 0.99999f,
                              coefficient, gain);
            }
            else
            {
                for (int i = 0; i < 12; ++i)
                {
                    const float mid = 0.5f * (lowDepth + highDepth);
                    float tryCoefficient = 0.0f, tryGain = 0.0f, tryDip = 1.0f;
                    if (solveFor(mid, tryCoefficient, tryGain, tryDip))
                    {
                        lowDepth = mid;
                        coefficient = tryCoefficient;
                        gain = tryGain;
                        dipF0 = tryDip;
                    }
                    else
                    {
                        highDepth = mid;
                    }
                }
                depth = lowDepth;
            }
        }

        loop.handLossSolvedDepth = depth;
        applyDipDepth(loop, depth);
        loop.loopDampingCoefficient = coefficient;
        loop.loopGain = clampf(gain, 0.0f, 0.99999f);
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
    float legatoOffset = 0.0f;
    if (voice.legatoBlend < 1.0f && voice.legatoFromFrequency > 0.0f)
    {
        const float fromSemis = 12.0f * std::log2(voice.legatoFromFrequency
                                                  / voice.baseFrequency);
        legatoOffset = fromSemis * (1.0f - smoothStep(voice.legatoBlend));
    }

    // The wheel bends this string by its own physical share of the nominal
    // range - a bar stretches every string, and each string's pitch answers
    // with its own compliance - so a bent chord smears exactly the way a real
    // tremolo bridge smears one.
    // The bar's share is the string's own elastic compliance; the finger's is
    // not, because a finger is controlling a pitch rather than a stretch and
    // adjusts its displacement to reach it. Only fingered strings get it - the
    // sympathetically ringing ones are configured elsewhere and never see it,
    // which is exactly what separates a finger from the bar.
    //
    // An open string is not fingered either. Nothing is holding it down, so
    // there is no contact to rock and no way for the hand to raise its pitch;
    // that is the same distinction the finger-noise term already draws at
    // fret 0. The bar still reaches it, because the bar stretches the whole
    // instrument rather than one stopped note.
    const float vibrato = voice.fret > 0 ? voice.vibratoSemitones : 0.0f;
    const float semitones = legatoOffset
                          + pitchBendSemitones_
                            * bendSensitivity(voice.stringIndex)
                          + vibrato;
    const float f0 = clampf(voice.baseFrequency * std::exp2(semitones / 12.0f),
                            20.0f, 0.24f * static_cast<float>(sampleRate_));

    // The full dispersion grid search is only re-fitted once the pitch has
    // moved beyond a small quantum. Re-fitting on every control tick of a
    // wheel glide ran the 520-candidate search for every bent voice for the
    // whole travel - measured beyond realtime on an eight-string chord at
    // 96 kHz - while a fit six cents stale changes the two deficit targets by
    // well under one percent, far inside the fit's own tolerance. The
    // analytic phase compensation below still tracks every sub-cent move, so
    // tuning stays exact; only the stiffness fit is quantised.
    const bool fitMoved = forceDelayJump
        || std::abs(semitones - voice.lastConfiguredSemitones) > 0.06f
        || std::abs(f0 - voice.lastConfiguredFrequency)
               > 3.5e-3f * std::max(f0, 20.0f);
    const bool pitchMoved =
        std::abs(semitones - voice.lastCompensatedSemitones) > 8.0e-4f;
    const float omega = twoPi * f0 * inverseSampleRate_;
    const float period = static_cast<float>(sampleRate_) / f0;

    // Damping-only changes (palm-mute pressure, string age, body
    // loss) reuse this fit and only redo the analytic phase compensation,
    // which avoids several hundred atan2 evaluations per automated control
    // tick per string.
    if (fitMoved)
    {
        voice.lastConfiguredSemitones = semitones;
        voice.lastConfiguredFrequency = f0;

        // Stiffness inharmonicity from the string's physical make-up. Wound
        // strings bend with their core, not their full winding diameter.
        const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
        const float gaugeScale = lerp(1.0f, 11.0f / 9.0f,
                                      smoothedParameters_.stringGauge);
        const float diameter = spec.plainDiameterMm * 1.0e-3f * gaugeScale;
        const float bendingDiameter = diameter * spec.bendingCoreScale;
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
        // allpassPhaseDelay(coefficient, omega) - the term at the sounding
        // fundamental itself, as opposed to the reference partial - depends on
        // the candidate coefficient alone, not on which of the two reference
        // partials it is being scored against. pairDeficit() used to take that
        // base term as a fresh call every time, so each grid point below
        // recomputed it twice for candidateLow (once scoring lowPartial, once
        // scoring highPartial) and twice for candidateHigh, four calls that
        // could only ever produce two distinct values. pairDeficitFromBase()
        // takes the base term already solved instead, so the caller can share
        // it: candidateLow's base is resolved once per lowIndex, outside the
        // highIndex loop it does not depend on, and candidateHigh's base is
        // resolved once per grid point instead of twice.
        const auto pairDeficitFromBase = [&] (float base, float coefficient,
                                              float partial)
        {
            const float omegaRef = std::min(omega * partial, pi * 0.95f);
            return 2.0f * (base - allpassPhaseDelay(coefficient, omegaRef));
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
                    const float lowBase = allpassPhaseDelay(candidateLow, omega);
                    for (int highIndex = 0; highIndex <= divisions; ++highIndex)
                    {
                        const float candidateHigh = highMinimum
                            + highStep * static_cast<float>(highIndex);
                        const float highBase = allpassPhaseDelay(candidateHigh, omega);
                        const float actualLow =
                            2.0f * pairDeficitFromBase(lowBase, candidateLow, lowPartial)
                            + 2.0f * pairDeficitFromBase(highBase, candidateHigh, lowPartial);
                        const float actualHigh =
                            2.0f * pairDeficitFromBase(lowBase, candidateLow, highPartial)
                            + 2.0f * pairDeficitFromBase(highBase, candidateHigh, highPartial);
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
        voice.lastCompensatedSemitones = semitones;
        // The dispersion fit above always assigns the same lowCoefficient/
        // highCoefficient pair to both polarisations in one stroke, so their
        // allpass phase-delay contribution is identical for vertical and
        // horizontal. loopPhaseDelay() used to call allpassPhaseDelay() (two
        // atan2 evaluations apiece) for both coefficients on each of the two
        // loops; solving each one once here instead halves that cost per
        // recompensation. Kept as two separate terms, summed in the same
        // order as before, rather than one pre-added total, so the result is
        // bit-identical to the previous per-loop expression.
        const float dispersionLowPhaseDelay =
            allpassPhaseDelay(voice.vertical.dispersionLowCoefficient, omega);
        const float dispersionHighPhaseDelay =
            allpassPhaseDelay(voice.vertical.dispersionHighCoefficient, omega);

        // Compensate every loop filter's phase delay at the fundamental so
        // the sounding pitch matches the target frequency.
        const auto loopPhaseDelay = [&] (const PolarisationLoop& loop)
        {
            // The loss band is in the loop, so its phase is part of the
            // sounding period; without this the mute drags the string flat by
            // up to 13 cents on the low E. ("Shelf" here until the band
            // replaced it - the figure was re-measured for the band.)
            float dipMagnitude = 1.0f, dipPhase = 0.0f;
            handLossResponse(loop.handLossDepth, loop.handLossShape, omega,
                             dipMagnitude, dipPhase);
            const float dipDelay = omega > 1.0e-9f ? -dipPhase / omega : 0.0f;
            return onePolePhaseDelay(loop.loopDampingCoefficient, omega)
                 + dipDelay
                 + 4.0f * dispersionLowPhaseDelay
                 + 4.0f * dispersionHighPhaseDelay;
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
    // The additive term is a detuning, so it has to be a fixed fraction of the
    // period rather than a fixed number of samples: left as a bare 0.11 it made
    // the beat rate a function of the host's sample rate - at the top of the
    // range the top string beat 45% faster at 48 kHz than at 192 kHz. It is
    // referenced to the 96 kHz internal clock, so 44.1/48 kHz hosts are
    // unchanged and the faster ones now agree with them in cents.
    const float horizontalDelay = voice.compensatedPeriodHorizontal
                                * tensionFactor * 1.00023f
                                + horizontalDetuneSamples_;

    voice.vertical.targetDelay = clampf(verticalDelay, 4.0f,
                                        static_cast<float>(delayLineSize - 8));
    voice.horizontal.targetDelay = clampf(horizontalDelay, 4.0f,
                                          static_cast<float>(delayLineSize - 8));

    if (forceDelayJump)
    {
        voice.vertical.currentDelay = voice.vertical.targetDelay;
        voice.horizontal.currentDelay = voice.horizontal.targetDelay;
    }

    // The two polarisations are coupled where they meet: at the bridge and the
    // nut, once per round trip. Charging a fixed fraction on every rendered
    // sample instead made the exchange proportional to the loop length, which
    // is proportional to the sample rate and inversely proportional to the
    // pitch. Measured, the former per-sample 0.004 exchanged 33% of the wave
    // per round trip at the top of the range and over 900% at the bottom - so
    // the low strings' two polarisations were averaged into one long before
    // they could produce the two-stage decay and beating they exist for, while
    // the high strings' loops lost 44 dB in the first second against a fitted
    // T60 of eight seconds. Per round trip it is one number at every pitch.
    voice.polarisationCoupling = clampf(
        polarisationCouplingPerRoundTrip
            / std::max(voice.vertical.targetDelay, 4.0f),
        0.0f, 0.25f);

    // Delay smoothing time constant: fast enough to track bends transparently.
    // Resolved once in prepare() as voiceDelaySmoothing_ rather than
    // recomputed with std::exp on every control tick of every active voice.
    voice.vertical.delaySmoothing = voiceDelaySmoothing_;
    voice.horizontal.delaySmoothing = voiceDelaySmoothing_;
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
            configureVoiceDamping(voice, voice.dampingStyle);
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
    const float maximumTapDelay = static_cast<float>(delayLineSize - 8);
    voice.pickupTapBridge.setDelay(pickupTapDelaySamples(
        bridgeDistance, soundingLength, period, maximumTapDelay));
    voice.pickupTapNeck.setDelay(pickupTapDelaySamples(
        neckDistance, soundingLength, period, maximumTapDelay));

    // Magnetic aperture: a true finite rectangular spatial window. Its
    // temporal length is Fs*w/c, where c is transverse wave speed. This has
    // the expected sinc response and -3 dB point at approximately .443*c/w,
    // unlike the previous one-pole approximation. The window is the same for
    // both pickups - it is one bobbin either way - and what the pickup type
    // moves is how far apart the humbucker's two of them sit.
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    const float waveSpeed = 2.0f * openLength * midiToHz(
        static_cast<float>(spec.openMidiNote));
    const float apertureLength = clampf(
        static_cast<float>(sampleRate_) * coilApertureMetres
            / std::max(waveSpeed, 1.0f),
        1.0f, static_cast<float>(apertureHistorySize - 2));
    voice.apertureNeck.setWindow(apertureLength);
    voice.apertureBridge.setWindow(apertureLength);

    // The coil pair, which the type control closes up until the humbucker is
    // a single coil. The position comb above is evaluated at the pickup's
    // centre, which is where the factorisation puts it: two coils at
    // centre +- d/2 read (1 + b z^-d/c) times one comb anchored at the
    // centre, so the pair costs one delayed read rather than a second comb.
    const float coilSpacing = lerp(humbuckerCoilSpacingMetres, 0.0f,
                                   parameters.pickupType);
    const float coilDelay = clampf(
        static_cast<float>(sampleRate_) * coilSpacing
            / std::max(waveSpeed, 1.0f),
        0.0f, static_cast<float>(apertureHistorySize - 2));
    voice.coilPairNeck.setSpacing(coilDelay, humbuckerCoilBalance);
    voice.coilPairBridge.setSpacing(coilDelay, humbuckerCoilBalance);
}

void ElectryEngine::configureSympatheticString(Voice& voice) noexcept
{
    // An unfingered string vibrates over its full open length. One
    // polarisation is enough for a bridge-coupled ring, so this solves a
    // single loop: period, damping, gain, and the bridge pickup tap.
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    const auto& parameters = smoothedParameters_;
    const float sampleRate = static_cast<float>(sampleRate_);
    // The wheel is a bar: it bends the strings nobody is fingering too, each
    // by its own compliance. The control tick retunes a ringing coupled
    // string whenever the wheel moves.
    const float f0 = midiToHz(static_cast<float>(spec.openMidiNote))
        * std::exp2(pitchBendSemitones_
                    * bendSensitivity(voice.stringIndex) / 12.0f);
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

    // A coupled string is the same piece of steel as a played one, so its loop
    // loss is solved from the same two decay targets rather than from a fixed
    // coefficient. The fixed one this replaced was a mild lowpass - 0.45 at the
    // default string age - which left the wound strings' top end ringing for
    // seconds: measured on the coupled low E at 48 kHz, its 3 kHz content had a
    // T60 of 3.7 s where the same string played decays that band in 0.12 s. A
    // sympathetic ring is not a bright metallic reverb, and the whole coupled
    // bank was reading as one.
    auto& loop = voice.vertical;
    loop.dispersionLowCoefficient = 0.0f;
    loop.dispersionHighCoefficient = 0.0f;
    const float highRatio = clampf(
        highFrequencyDecayRatio(voice.stringIndex)
            * (palmMuteBlend_ > 0.0f ? lerp(1.0f, 0.62f, palmMuteBlend_) : 1.0f),
        0.0015f, 0.9f);
    const float t60High = clampf(t60 * highRatio, 0.008f, t60);
    const float fHigh = std::min(3600.0f, 0.32f * sampleRate);
    const float omegaHigh = twoPi * std::max(fHigh, f0 * 1.5f)
                          * inverseSampleRate_;
    // The pair is solved together rather than one after the other. A coupled
    // string asks for a far steeper tilt than a played one at the same age -
    // the mute shortens its fundamental target directly instead of in parallel
    // with a relief - so above about String Age 0.8, and under any palm mute at
    // all, the requested pair does not fit inside a loop gain of one. Solving
    // the tilt and clamping the gain threw the fundamental's target away with
    // it: the open low E realised 0.099 s against an 8.97 s target, and the
    // sympathetic ring simply vanished. Backing the high target off instead
    // keeps the fundamental pinned and costs only the top of the tilt, which is
    // the band the clamp was destroying anyway.
    constexpr float coupledGainCeiling = 0.9999f;
    float coefficient = 0.0f, gain = 0.0f;
    solveLoopLoss(t60, t60High, period, sampleRate, omega, omegaHigh,
                  coupledGainCeiling, coefficient, gain);
    loop.loopDampingCoefficient = coefficient;
    loop.loopGain = clampf(gain, 0.0f, coupledGainCeiling);

    const float compensatedPeriod = clampf(
        period - onePolePhaseDelay(loop.loopDampingCoefficient, omega),
        4.0f, static_cast<float>(delayLineSize - 8));
    loop.targetDelay = compensatedPeriod;
    // Same fixed time constant configureVoicePitch() uses, shared via
    // voiceDelaySmoothing_ so the two call sites cannot drift apart.
    loop.delaySmoothing = voiceDelaySmoothing_;
    // A string that is already ringing glides to its new tuning; a freshly
    // woken one starts there, so a build change never clicks the ring.
    if (! voice.sympatheticReady)
        loop.currentDelay = compensatedPeriod;

    const float bridgeDistance = lerp(lesPaulBridgePickupMetres,
                                      telecasterBridgePickupMetres,
                                      parameters.pickupType);
    voice.sympatheticPickupTap.setDelay(pickupTapDelaySamples(
        bridgeDistance, scaleLengthMetres(), period,
        static_cast<float>(delayLineSize - 8)));
}

// Whether the plectrum meets the string on this attack. A hammer-on or tap is
// the fretting hand landing on the fingerboard and a legato slide is a finger
// already down that simply moves, so on neither of them is there a pick to
// colour the contact, to hold at an angle, or to put down out of place. Both
// the excitation's own plectrum terms and the picking hand's stroke-to-stroke
// variation are read from this one predicate so they cannot disagree.
bool ElectryEngine::plectrumContacts(PlayStyle style, bool legato) noexcept
{
    return ! (style == PlayStyle::Hammer || (style == PlayStyle::Slide && legato));
}

void ElectryEngine::drawStrokeVariation(Voice& voice) noexcept
{
    // A hand does not put the pick down twice in the same place. Four things
    // about the contact move from stroke to stroke, and they are the four the
    // excitation already reads: where along the string the pick lands, how hard
    // it is pushed, at what angle it meets the string, and how much of its tip
    // is touching. Nothing else in the attack is randomised - the pluck is
    // still the same mechanism, differently placed.
    //
    // Every draw is a pure function of the note counter and the string, so
    // `Identical MIDI always renders identical audio` survives it: the same
    // sequence of note-ons produces the same sequence of draws. `startOrder`
    // is taken rather than `noteSequence_` because a strummed chord's later
    // strings excite several blocks after their note-on, by which time the
    // counter has moved on.
    std::uint32_t state = hash32(
        static_cast<std::uint32_t>(voice.startOrder * 2654435761u)
        ^ static_cast<std::uint32_t>(voice.stringIndex * 40503u)
        ^ 0x5bf03635u);
    // Three uniforms on [-1, 1] sum to unit variance and cannot leave +/-3
    // sigma, which is the bound the physical quantities need: the contact
    // cannot move further along the string than the plectrum's width lets the
    // player feel, nor further than the heel of the hand anchored on the
    // bridge allows.
    const auto normal = [&state] { return sumThreeUniforms(state); };

    // Contact position. 4 mm of standard deviation along the string is the
    // scale the plectrum's own width and the anchored hand bracket; at the
    // default Pick Position it is about 5% of the pick-to-bridge distance, so
    // it moves the pluck comb's first notch by about that much.
    voice.strokeContactOffsetMetres = 0.004f * normal();
    // Contact force. The stroke's level is meant to move by about 0.8 dB of
    // standard deviation, but the contact patch below already carries 0.4 dB of
    // that on its own - a longer contact injects more of the pick's work as
    // well as less of its top end - so the force itself draws the remainder.
    // It rides on the velocity profile rather than replacing it, so a written
    // accent still reads as an accent.
    voice.strokeForceGain = std::pow(10.0f, 0.6f * normal() / 20.0f);
    // Attack angle. The pick meets the string a few degrees off the plane it
    // nominally travels in; because the excitation is split between two
    // polarisations, an angle is not a free parameter but exactly that split,
    // so the jitter is applied as a rotation of the split vector in
    // updateStyleWeights() and takes no energy with it.
    voice.strokeAngleOffset = 6.0f * (pi / 180.0f) * normal();
    // Contact patch. 8% of standard deviation on how much of the tip is
    // touching, carried by the length of the release pulse.
    voice.strokeWidthScale = clampf(1.0f + 0.08f * normal(), 0.6f, 1.4f);
}

void ElectryEngine::beginChordStroke(int stringIndex, bool strokeIsUp,
                                     float spreadSeconds) noexcept
{
    // A new pick stroke. The edge it starts from is the chord's extreme string
    // in the stroke's own direction, which is not known yet - only the first
    // note-on has arrived - so it is provisional and `reAnchorChordStroke`
    // below may move it while the pre-roll is still running.
    ++chordSequence_;
    chordAnchorString_ = stringIndex;
    chordStrokeIsUp_ = strokeIsUp;
    chordAlternateConsumed_ = false;
    chordContactOccurred_ = false;
    chordFirstNoteOnClock_ = engineClock_;
    chordTravelSamples_.fill(0);

    if (! (spreadSeconds > 0.0f))
    {
        // No spread, no travel and no pre-roll: the chord is one block again,
        // to the sample.
        strumPreRollSamples_ = 0;
        return;
    }
    strumPreRollSamples_ = strumReAnchorSamples_;

    // The wrist's motion for this one stroke. It is drawn from the chord
    // counter alone, so every string of the chord reads the same ramp and the
    // same MIDI still renders the same audio.
    std::uint32_t state = hash32(
        static_cast<std::uint32_t>(chordSequence_ * 2891336453u) ^ 0x7feb352du);
    const auto normal = [&state] { return sumThreeUniforms(state); };

    // Three uniforms sum to unit variance and cannot leave +/-3 sigma, so the
    // acceleration stays between 0.55 and 1.45 of nominal and the ramp cannot
    // invert.
    const float acceleration = strumAcceleration
                             * (1.0f + strumAccelerationSigma * normal());

    std::array<float, stringCount - 1> gaps {};
    float total = 0.0f;
    for (int k = 0; k < stringCount - 1; ++k)
    {
        gaps[static_cast<std::size_t>(k)] =
            (1.0f + strumCrossingSigma * normal())
            / std::sqrt(1.0f + acceleration * static_cast<float>(k));
        total += gaps[static_cast<std::size_t>(k)];
    }

    // Strum Spread states the time the pick takes per string, so the ramp is
    // normalised to it: whatever shape the draws gave it, seven crossings
    // still take seven times the knob. The acceleration redistributes the
    // stroke, it does not lengthen it.
    const float scale = spreadSeconds * static_cast<float>(sampleRate_)
                      * static_cast<float>(stringCount - 1)
                      / std::max(1.0e-6f, total);
    float travel = 0.0f;
    for (int k = 0; k < stringCount - 1; ++k)
    {
        travel += gaps[static_cast<std::size_t>(k)] * scale;
        chordTravelSamples_[static_cast<std::size_t>(k + 1)] =
            static_cast<int>(travel);
    }
}

void ElectryEngine::reAnchorChordStroke(int stringIndex) noexcept
{
    // A note-on that is further out along the neck than the current anchor, in
    // the direction the pick is travelling, is where the stroke really began.
    // Every voice of this chord is still inside its pre-roll, so none of them
    // has sounded and all of them can be pushed out by the extra travel.
    const int crossings = chordStrokeIsUp_ ? chordAnchorString_ - stringIndex
                                           : stringIndex - chordAnchorString_;
    if (crossings >= 0)
        return;

    chordAnchorString_ = stringIndex;
    for (auto& voice : voices_)
    {
        if (voice.strumChordId != chordSequence_ || voice.startDelaySamples <= 0)
            continue;
        const int moved = chordStrokeIsUp_
            ? chordAnchorString_ - voice.stringIndex
            : voice.stringIndex - chordAnchorString_;
        // Rescheduled against the chord's own clock rather than by adding a
        // difference, so the countdown's control-tick quantisation does not
        // accumulate across re-anchors.
        const std::int64_t onset = chordFirstNoteOnClock_
                                 + static_cast<std::int64_t>(strumPreRollSamples_)
                                 + static_cast<std::int64_t>(strumTravelSamples(moved));
        voice.startDelaySamples = static_cast<int>(
            std::max<std::int64_t>(1, onset - engineClock_));
    }
}

int ElectryEngine::strumTravelSamples(int crossings) const noexcept
{
    // A chord whose note-ons are spread wider than the re-anchor window still
    // travels from its first arrival: a string that turns out to be beyond the
    // anchor once the window has closed is picked as soon as it can be, which
    // is the stated limit of the mechanism rather than an undefined case.
    const int index = std::clamp(crossings, 0, stringCount - 1);
    return chordTravelSamples_[static_cast<std::size_t>(index)];
}

void ElectryEngine::seedVibratoFinger(Voice& voice) noexcept
{
    // The finger that will rock this note starts its own cycle wherever it
    // happens to be when it gets there: the rock is not synchronised to the
    // pick, so the phase is drawn rather than started at zero. The onset
    // shaping holds the excursion at zero regardless, so the note still leaves
    // the fret at exactly its fretted pitch. Only a fresh note reseeds - a
    // hammer-on or a slide is the same finger arriving somewhere else, and it
    // keeps rocking through the move rather than jumping to a new phase.
    voice.vibratoSeed = hash32(
        static_cast<std::uint32_t>(voice.startOrder * 2246822519u)
        ^ static_cast<std::uint32_t>(voice.stringIndex * 68041u)
        ^ 0x27d4eb2fu);
    std::uint32_t phaseState = voice.vibratoSeed;
    voice.vibratoPhase = 0.5f * (bipolarNoise(phaseState) + 1.0f);
    voice.vibratoCycle = 0u;
    drawVibratoCycle(voice);
}

void ElectryEngine::drawVibratoCycle(Voice& voice) noexcept
{
    // A hand does not rock twice at the same speed or to the same width. Both
    // are redrawn once per cycle from a stream this voice advances itself -
    // the note counter only moves on note-on, so a held note drawn from it
    // would get one fixed pair for its whole length.
    std::uint32_t state = hash32(voice.vibratoSeed
                                 ^ (voice.vibratoCycle * 2654435761u));
    ++voice.vibratoCycle;
    const auto normal = [&state] { return sumThreeUniforms(state); };
    // Three uniforms sum to unit variance and cannot leave +/-3 sigma, so the
    // rate stays inside 0.64..1.36 of nominal and the excursion inside
    // 0.55..1.45 - a hand that wanders, not one that stumbles.
    voice.vibratoRateScale = 1.0f + 0.12f * normal();
    voice.vibratoDepthScale = 1.0f + 0.15f * normal();
}

void ElectryEngine::startExcitation(Voice& voice, float velocity, bool legato) noexcept
{
    const auto wholeHandStyle = [] (PlayStyle style)
    {
        return style == PlayStyle::PalmMute || style == PlayStyle::Dead
            ? style : PlayStyle::Sustain;
    };
    const bool isLatestHandContact = engineClock_ > lastHandContactClock_
        || (engineClock_ == lastHandContactClock_
            && voice.startOrder > lastHandContactOrder_);
    if (isLatestHandContact)
    {
        const bool handDampingChanged = lastHandContactClock_ >= 0
            && wholeHandStyle(lastHandContactPlayStyle_)
                != wholeHandStyle(voice.playStyle);
        lastHandContactClock_ = engineClock_;
        lastHandContactPlayStyle_ = voice.playStyle;
        lastHandContactOrder_ = voice.startOrder;

        if (wholeHandStyle(voice.dampingStyle)
            != wholeHandStyle(voice.playStyle))
        {
            configureVoiceDamping(voice, voice.playStyle);
            configureVoicePitch(voice, false);
        }

        if (handDampingChanged)
        {
            for (auto& activeVoice : voices_)
            {
                const bool freshContactPending = activeVoice.startDelaySamples > 0
                    && ! activeVoice.pendingContactPreservesRing;
                if (! activeVoice.active || &activeVoice == &voice
                    || freshContactPending)
                    continue;
                configureVoiceDamping(activeVoice, voice.playStyle);
                configureVoicePitch(activeVoice, false);
            }
        }
    }
    voice.pendingContactPreservesRing = false;
    (void) velocity; // The note-on velocity is cached in velocityProfile.
    const auto& parameters = smoothedParameters_;
    const float sampleRate = static_cast<float>(sampleRate_);
    const auto& profile = voice.velocityProfile;

    // Whether the plectrum touches this string at all. Hammer-ons and taps are
    // the fretting hand landing on the fingerboard, and a legato slide is a
    // finger already down that simply moves. This one predicate gates every
    // picking-hand contribution so the excitation, impact and stroke geometry
    // cannot disagree about whether a pick was present.
    const bool plectrumContact = plectrumContacts(voice.playStyle, legato);

    // The bridge-hand thud belongs to the actual plectrum contact, not to the
    // MIDI note-on that schedules a strummed string. Arming it here keeps the
    // pre-roll silent and means a key released before the pick arrives makes no
    // phantom impact. Hammer and legato Slide have no picking-hand collision.
    if (plectrumContact)
    {
        if (voice.playStyle == PlayStyle::PalmMute || palmMuteBlend_ > 0.0f)
        {
            const float muteIntensity = voice.playStyle == PlayStyle::PalmMute
                ? 0.55f + 0.45f * parameters.muteDamping : palmMuteBlend_;
            // A new collision adds to the still-moving bridge/body response;
            // it cannot erase the preceding thud in zero time. The bound only
            // protects a malformed, audio-rate Note-On flood.
            voice.palmImpactVel = std::min(
                1.0f, voice.palmImpactVel
                          + 0.16f * profile.amplitude * muteIntensity);
        }
    }
    // A stiffer pick is a little louder, but most of what it changes is the
    // spectrum: the release corner below carries that, and leaving a wide
    // amplitude range here as well turned Pick Hardness into a level control.
    const float hardnessGain = lerp(0.98f, 1.26f, parameters.pickHardness);

    if (plectrumContact && voice.strumChordId == chordSequence_)
        chordContactOccurred_ = true;

    // A picked attack can graze a fret on the way. Start this opportunity at
    // the actual plectrum contact: scheduling it in startVoice() spent part—or
    // all—of the window while a delayed strum was still exactly silent.
    if (plectrumContact && parameters.artifactAmount > 0.0f)
    {
        const float contact = std::pow(parameters.artifactAmount, 0.75f)
                            * voice.velocityProfile.collision
                            * (0.70f + 0.30f * voice.lowStringWeight);
        voice.artifactCollisionLength = static_cast<int>(
            lerp(0.025f, 0.100f, voice.velocityProfile.collision)
            * static_cast<float>(sampleRate_));
        voice.artifactCollisionLengthDenominator =
            static_cast<float>(std::max(1, voice.artifactCollisionLength));
        voice.artifactCollisionRemaining = voice.artifactCollisionLength;
        voice.artifactClearance = lerp(looseFretClearanceMetres,
                                       hardFretClearanceMetres, contact)
                                / waveguideDisplacementMetresPerUnit;
    }
    else
    {
        voice.artifactCollisionRemaining = 0;
        voice.artifactCollisionLength = 0;
        voice.artifactClearance = 1.0f;
    }
    // How far out of place the hand put the pick this stroke. A fingered note
    // has no pick to put out of place, so it draws no variation: leaving these
    // live gave repeated hammer-ons and slides a picking hand's spread of
    // level, pulse width and contact position despite the engine's own claim
    // that there is no plectrum on them.
    const float strokeForceGain = plectrumContact ? voice.strokeForceGain : 1.0f;
    const float strokeWidthScale = plectrumContact ? voice.strokeWidthScale : 1.0f;
    const float strokeContactOffsetMetres =
        plectrumContact ? voice.strokeContactOffsetMetres : 0.0f;

    const float ageAmplitudeGain = lerp(1.0f, 0.90f, parameters.stringAge);
    float amplitude = 0.48f * profile.amplitude * hardnessGain
                    * ageAmplitudeGain
                    * strokeForceGain;
    // The string leaves the pick in a fraction of a millisecond; the width
    // controls how much of the upper spectrum the release step carries.
    float pulseMs = lerp(1.15f, 0.10f, parameters.pickHardness)
                  * lerp(1.55f, 0.48f, profile.releaseRate)
                  * strokeWidthScale;
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

    switch (voice.playStyle)
    {
        case PlayStyle::Sustain:
            break;
        case PlayStyle::PalmMute:
            amplitude *= 0.88f;
            pulseMs *= 1.2f;
            pulseCutoff *= 0.72f;
            pluckFraction *= 0.8f;
            noiseLevel *= 1.5f;
            modalBrightness *= 0.74f;
            break;
        case PlayStyle::Hammer:
            amplitude *= legato ? 0.30f : 0.42f;
            pulseMs *= 1.9f;
            pulseCutoff *= 0.42f;
            pluckFraction = 0.12f;
            noiseLevel = 0.10f * fingerControl
                       * (0.10f + 0.30f * profile.noise);
            noiseMs = 1.6f;
            noiseCutoff = 185.0f;
            modalBrightness *= 0.25f;
            break;
        case PlayStyle::Harmonics:
            amplitude *= 0.62f;
            pulseMs *= 0.50f;
            pulseCutoff *= 1.65f;
            pluckFraction = 0.31f;
            noiseLevel *= 0.55f;
            noiseMs *= 0.7f;
            noiseCutoff *= 1.35f;
            modalBrightness *= 1.48f;
            break;
        case PlayStyle::Pinch:
            // An ordinary pick stroke with the thumb following it in. The pick
            // itself is unchanged - the pluck position is the player's, not the
            // style's, because the thumb has to land where the pick did - and
            // the thumb only adds its own contact scrape.
            amplitude *= 0.92f;
            noiseLevel *= 1.15f;
            noiseCutoff *= 1.10f;
            modalBrightness *= 1.10f;
            break;
        case PlayStyle::Dead:
            // The pick lands exactly as it would on a ringing note; what
            // removes the pitch is the hand already on the string, and that
            // hand also drags a good deal more contact noise out of the
            // winding and darkens what the pick leaves behind.
            noiseLevel *= 1.8f;
            noiseMs *= 1.3f;
            modalBrightness *= 0.85f;
            break;
        case PlayStyle::Slide:
            // A slide into a sounding string has no attack at all - the finger
            // is already down and simply moves - so the only thing the note-on
            // contributes is the friction of the move itself, which is set up
            // in legatoRetarget(). Starting a phrase on the Slide keyswitch
            // with nothing to slide from is an ordinary pick stroke, because
            // that is what a player would have to do.
            if (legato)
            {
                amplitude *= 0.12f;
                pulseMs *= 1.9f;
                pulseCutoff *= 0.42f;
                noiseLevel = 0.0f;
                modalBrightness *= 0.25f;
            }
            break;
    }

    // The stroke composes with every picked style: an upstroke palm mute is a
    // muted contact with the upstroke's geometry and polarity. A hammered
    // note has no plectrum, so the latched stroke cannot colour it.
    if (voice.strokeIsUp && voice.playStyle != PlayStyle::Hammer)
        applyUpstrokeVoicing();

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
    if (voice.playStyle == PlayStyle::PalmMute)
        handDamping = std::max(handDamping, 0.55f + 0.45f * parameters.muteDamping);
    else if (voice.playStyle == PlayStyle::Dead)
        // The fretting hand is already touching the string when the pick
        // lands. A light attack-path darkening removes the artificial bright
        // edge without turning Dead into the bridge hand's Palm Mute voicing.
        handDamping = std::max(handDamping, 0.15f);
    handDamping = clampf(handDamping, 0.0f, 1.0f);
    // A hand of exactly zero pressure leaves every factor below at one.
    pulseCutoff *= lerp(1.0f, 0.26f, handDamping);
    noiseCutoff *= lerp(1.0f, 0.40f, handDamping);

    // Player effort and the guitar build change more than level: hard notes
    // are shorter/brighter at the contact, old strings lose the initial edge,
    // and the low Drop-E strings receive a little extra definition.
    pulseCutoff *= profile.brightness
                 * lerp(1.08f, 0.42f, parameters.stringAge)
                 * lerp(0.92f, 1.12f, parameters.construction)
                 * (1.0f + 0.24f * voice.lowStringWeight);
    noiseCutoff *= lerp(0.72f, 1.22f, profile.releaseRate)
                 * (plectrumContact
                        ? lerp(0.55f, 1.75f, parameters.pickHardness) : 1.0f)
                 * (1.0f + 0.12f * voice.lowStringWeight);

    // Most of a real pluck's sustained tone comes from the triangular string
    // displacement present when the pick lets go.  Its modal coefficients
    // fall as 1/n^2. Ordinary sustained picks keep only a much smaller short
    // release edge; feeding the whole attack through that pulse made the
    // position comb and induced-EMF derivative compound into a clavinet-like
    // high-partial tilt. Percussive styles may intentionally weight it higher.
    float displacementGain = 1.55f;
    float transientGain = lerp(0.0006f, 0.0025f, parameters.pickHardness)
                        * lerp(0.88f, 1.08f, profile.releaseRate);
    switch (voice.playStyle)
    {
        case PlayStyle::Sustain:
            break;
        case PlayStyle::PalmMute:
            // The hand damps the released string; it does not stop the pick
            // from loading it. Keep the sustained triangular displacement and
            // leave the sharp edge to the hand attenuation below, otherwise
            // the low E loses body before the contact model has anything to
            // damp.
            transientGain *= 1.10f;
            break;
        case PlayStyle::Hammer:
            displacementGain = 0.72f;
            transientGain = 0.0f;
            break;
        case PlayStyle::Harmonics:
            displacementGain = 0.30f;
            transientGain = 0.42f;
            break;
        case PlayStyle::Pinch:
            displacementGain = 1.35f;
            transientGain *= 1.20f;
            break;
        case PlayStyle::Slide:
            if (legato)
            {
                displacementGain = 0.40f;
                transientGain = 0.0f;
            }
            break;
        case PlayStyle::Dead:
            // Almost all edge and almost no sustained displacement: the string
            // never gets to hold the shape the pick gave it.
            displacementGain = 1.10f;
            transientGain *= 1.45f;
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
    // Solved once here instead of on every rendered sample of the Release
    // phase below, which divides by this same clamped length once per
    // sample to form its progress fraction.
    voice.excitationLengthDenominator =
        static_cast<float>(std::max(1, voice.excitationLength));
    const int contactSamples = legato
        ? 0
        : std::max(4, static_cast<int>(lerp(3.0f, 0.55f, parameters.pickHardness)
                                       * 0.001f * sampleRate));

    // Contact loss is a total attenuation over the complete pick/string
    // engagement, not ten percent every oversampled frame. The old 0.90
    // per-sample multiplier erased essentially all energy on a repick.
    float contactRetention = lerp(0.88f, 0.64f, parameters.pickHardness);
    if (voice.playStyle == PlayStyle::PalmMute)
        contactRetention *= 0.82f;
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
    //
    // The stroke's own contact offset is a distance along the string, so it is
    // added to the open-string fraction before the fret stretch, exactly as the
    // nominal position is: the hand is a few millimetres out of place, not a
    // few millimetres of the sounding length out of place.
    const float fretStretch = std::exp2(static_cast<float>(voice.fret) / 12.0f);
    const float strokePluckFraction = pluckFraction
        + strokeContactOffsetMetres / scaleLengthMetres();
    const float combFraction = clampf(strokePluckFraction * fretStretch,
                                      0.02f, 0.49f);
    // Where the touching hand is, if either hand is touching. The natural
    // harmonic is the fretting hand on the midpoint node: every odd partial
    // has an antinode under it and goes, every even one has a node there and
    // is left exactly alone in magnitude and phase, so the octave is what the
    // string does rather than a transposition of it.
    //
    // The pinch harmonic is the picking hand's thumb catching the string
    // immediately after the pick, so it touches at the pick's own position -
    // which is why moving Pick Position toward the neck moves the squeal down
    // the harmonic series, exactly as moving the hand does on the instrument.
    // Its depth is one rather than the fretting finger's 0.92 because a thumb
    // pressed against a string is a much firmer contact, and it stays on the
    // string far longer, because the mode-shape law gives a contact this close
    // to the bridge little purchase on the low partials: at a tenth of the
    // sounding length the fundamental loses about seven per cent of its energy
    // per round trip where a midpoint touch would take nearly all of it. That
    // asymmetry is the physics of the technique, not a shortcoming of it.
    switch (voice.playStyle)
    {
        case PlayStyle::Harmonics:
            voice.touchFraction = 0.5f;
            voice.touchDepth = 0.92f;
            voice.touchHoldRemaining = static_cast<int>(0.045 * sampleRate_);
            voice.touchReleaseStep = 0.92f
                / std::max(1.0f, 0.080f * sampleRate);
            break;
        case PlayStyle::Pinch:
            voice.touchFraction = combFraction;
            voice.touchDepth = 1.0f;
            voice.touchHoldRemaining = static_cast<int>(0.090 * sampleRate_);
            voice.touchReleaseStep = 1.0f
                / std::max(1.0f, 0.130f * sampleRate);
            break;
        case PlayStyle::Sustain:
        case PlayStyle::PalmMute:
        case PlayStyle::Hammer:
        case PlayStyle::Slide:
        case PlayStyle::Dead:
            voice.touchFraction = 0.0f;
            voice.touchDepth = 0.0f;
            voice.touchHoldRemaining = 0;
            voice.touchReleaseStep = 0.0f;
            break;
    }

    // A plectrum touches the string over a patch, not at a point: half a
    // millimetre of contact for a stiff sharp pick, around a millimetre and a
    // half for a soft rounded one. Mapped through the same sounding-length
    // geometry the comb itself uses, that width is a little over one sample on
    // an open Drop-E eighth string and a small fraction of one at the top of
    // the range, which is exactly the frequency dependence a real contact has.
    const float contactMetres = 0.001f * lerp(1.5f, 0.5f, parameters.pickHardness);
    const float soundingMetres = std::max(scaleLengthMetres() / fretStretch,
                                          0.05f);
    // To second order, a string displaced by y at fraction p of its sounding
    // length grows by y^2 / (2 L p (1-p)). With dT = E A dL/L and the
    // first-order linearisation of f proportional to sqrt(T), the frequency
    // rise is
    //
    //     df/f = E A y^2 / (4 T L^2 p (1-p)).
    //
    // The axial core is its own empirical fit: winding slip under tension need
    // not match the effective flexural core used by the dispersion model.
    // The gauge cancels only in an ideal string; retaining both areas keeps
    // this solve consistent with the actual fitted set.
    const float gaugeScale = lerp(1.0f, 11.0f / 9.0f,
                                  parameters.stringGauge);
    const float diameter = spec.plainDiameterMm * 1.0e-3f * gaugeScale;
    const float coreDiameter = diameter * spec.axialCoreScale;
    const float coreArea = 0.25f * pi * coreDiameter * coreDiameter;
    const float massScale = spec.wound ? 0.85f : 1.0f;
    const float linearMass = massScale * steelDensity * 0.25f * pi
                           * diameter * diameter;
    const float waveSpeed = 2.0f * soundingMetres * voice.baseFrequency;
    const float standingTension = linearMass * waveSpeed * waveSpeed;
    const float pluckGeometry = std::max(combFraction * (1.0f - combFraction),
                                         0.015f);
    voice.tensionDepth = steelYoungModulus * coreArea
        / std::max(4.0f * standingTension * soundingMetres * soundingMetres
                     * pluckGeometry,
                   1.0e-9f);

    // `amplitude` already contains velocity, stroke-to-stroke force and the
    // articulation's displacement reduction. Divide out the purely tonal
    // hardness/age gains, and reference the ordinary sustain's 1.55 modal
    // projection, so this is the stroke's physical force rather than a second
    // output-level curve. The triangular-string statics then turn that force
    // into the string-specific displacement below. Seeding the follower at
    // release also fixes the old backwards trajectory, where the inferred
    // energy did not peak until about 184 ms after the pick.
    constexpr float sustainDisplacementGain = 1.55f;
    const float relativeForce =
        amplitude / std::max(0.48f * hardnessGain * ageAmplitudeGain, 1.0e-6f)
        * (displacementGain / sustainDisplacementGain);
    const float pluckForce = fullForcePickLoadNewtons
                           * std::max(relativeForce, 0.0f);
    const float deflectionMetres = pluckForce * pluckGeometry * soundingMetres
                                 / std::max(standingTension, 1.0e-6f);
    voice.energyEnvelope = std::max(voice.energyEnvelope,
                                    deflectionMetres * deflectionMetres);
    // startVoice() solved the un-stretched period before this attack's contact
    // geometry existed. Re-solve now that the physical deflection is seeded;
    // otherwise onset pitch depends on how far away the next control tick is.
    configureVoicePitch(voice, false);
    voice.excitationCombDelay = combFraction * voice.vertical.targetDelay;
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
            * lerp(0.90f, 1.12f, profile.releaseRate),
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
               * lerp(0.72f, 1.45f, profile.releaseRate)
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
    voice.noiseLengthDenominator = static_cast<float>(std::max(1, voice.noiseLength));
    voice.noiseRemaining = voice.noiseLength;
    voice.releaseNoiseDone = false;
    updateStyleWeights(voice, legato);
}

void ElectryEngine::updateStyleWeights(Voice& voice, bool legato) noexcept
{
    // Play styles attack the string at different angles, splitting energy
    // differently between the two polarisations, and a perceptual makeup keeps
    // keyswitch changes usable inside one phrase while the excitation spectra
    // and envelopes remain strongly different. Both used to be re-selected by
    // a switch on every rendered sample; they only change at an attack.
    float verticalWeight = 0.92f;
    float horizontalWeight = 0.42f;
    float makeup = 1.0f;
    switch (voice.playStyle)
    {
        case PlayStyle::Sustain:
            break;
        case PlayStyle::PalmMute:
            makeup = 1.18f;
            break;
        case PlayStyle::Hammer:
            verticalWeight = 0.95f; horizontalWeight = 0.28f; makeup = 1.48f;
            break;
        case PlayStyle::Harmonics:
            verticalWeight = 0.86f; horizontalWeight = 0.52f; makeup = 1.34f;
            break;
        case PlayStyle::Pinch:
            // The thumb follows the pick in along the same path, so the split
            // stays close to a downstroke's; the makeup pays back the energy
            // the touch takes out of the low partials.
            verticalWeight = 0.90f; horizontalWeight = 0.46f; makeup = 1.55f;
            break;
        case PlayStyle::Slide:
            // Sliding into a sounding string is a fretting-hand move, so it
            // takes the fingered split; sliding from nothing is an ordinary
            // pick and keeps the default one.
            if (legato)
            {
                verticalWeight = 0.95f; horizontalWeight = 0.28f; makeup = 1.20f;
            }
            break;
        case PlayStyle::Dead:
            // The hand lying across the string flattens the attack into the
            // plane of the fingerboard.
            verticalWeight = 0.96f; horizontalWeight = 0.30f; makeup = 1.85f;
            break;
    }
    // The upstroke approaches from below, tilting the attack toward the
    // horizontal polarisation. For a plain sustained upstroke these ratios
    // land exactly on the fitted upstroke voicing; for the other picked
    // styles they compose with the style's own split. A hammered note has no
    // pick, so the stroke leaves it alone.
    if (voice.strokeIsUp && voice.playStyle != PlayStyle::Hammer)
    {
        verticalWeight *= 0.90f / 0.92f;
        horizontalWeight *= 0.46f / 0.42f;
        makeup *= 1.06f;
    }
    // The stroke's own attack angle. The split between the two polarisations
    // *is* the angle at which the pick meets the string, so a few degrees of
    // hand-to-hand variation is a rotation of that vector and nothing else: its
    // length, which is how much of the pick's work reaches the string, does not
    // move. This composes with the upstroke tilt above rather than replacing
    // it, so an alternate-picked repeat varies on top of its up/down colouring
    // instead of losing it. A note played without a plectrum has none to hold
    // at an angle, on the same grounds the upstroke voicing skips it - and that
    // is the legato slide as well as the hammer-on, which is why the predicate
    // is asked rather than the style compared.
    if (plectrumContacts(voice.playStyle, legato) && voice.strokeAngleOffset != 0.0f)
    {
        const float magnitude = std::sqrt(verticalWeight * verticalWeight
                                          + horizontalWeight * horizontalWeight);
        const float angle = clampf(
            std::atan2(horizontalWeight, verticalWeight) + voice.strokeAngleOffset,
            0.02f, 0.5f * pi - 0.02f);
        verticalWeight = magnitude * std::cos(angle);
        horizontalWeight = magnitude * std::sin(angle);
    }
    voice.verticalWeight = verticalWeight;
    voice.horizontalWeight = horizontalWeight;
    voice.articulationMakeup = makeup;
}

void ElectryEngine::startVoice(Voice& voice, int midiNote, float velocity,
                               PlayStyle playStyle, bool strokeIsUp,
                               int startDelaySamples,
                               std::uint64_t reservedStartOrder,
                               bool keyStateAlreadyApplied) noexcept
{
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    const int fret = midiNote - spec.openMidiNote;

    const bool wasRinging = voice.active;
    const int repeatedKeyDownCount = wasRinging && voice.midiNote == midiNote
        ? voice.keyDownCount : 0;
    const bool delayedSameNoteRepick = reservedStartOrder == 0
                                     && wasRinging
                                     && voice.midiNote == midiNote
                                     && startDelaySamples > 0;

    if (delayedSameNoteRepick)
    {
        const bool preservesExistingRing = voice.pendingContactPreservesRing
                                        || voice.startDelaySamples == 0;
        // Strum pre-roll is scheduling lookahead, not an early physical pick.
        // Only MIDI ownership changes now; replacing damping, hand followers,
        // release state or stroke voicing here audibly rewrites the old ring
        // several milliseconds before the new contact exists.
        voice.keyDown = true;
        voice.keyDownCount = std::min(repeatedKeyDownCount + 1, 65535);
        voice.sustained = false;
        voice.startDelaySamples = std::max(1, startDelaySamples);
        voice.pendingContactPreservesRing = preservesExistingRing;
        voice.pendingRepick = {
            true, velocity, playStyle, strokeIsUp, ++noteSequence_
        };
        return;
    }

    voice.pendingRepick.active = false;

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
    // Saturation keeps a malformed endless Note-On stream defined while the
    // ordinary two-event timestamp reorder costs only this counter.
    if (! keyStateAlreadyApplied)
        voice.keyDownCount = std::min(repeatedKeyDownCount + 1, 65535);
    voice.sustained = false;
    voice.releasing = false;
    voice.midiNote = midiNote;
    voice.fret = std::clamp(fret, 0, fretCount);
    voice.playStyle = playStyle;
    voice.strokeIsUp = strokeIsUp;
    voice.velocity = velocity;
    voice.velocityProfile = makeVelocityProfile(velocity);
    voice.startOrder = reservedStartOrder != 0
        ? reservedStartOrder : ++noteSequence_;
    drawStrokeVariation(voice);
    voice.handContactScale = clampf(
        voice.velocityProfile.amplitude
            * (plectrumContacts(playStyle, false)
                   ? voice.strokeForceGain : 1.0f),
        0.32f, 1.25f);
    seedVibratoFinger(voice);
    voice.ageSamples = 0;
    // Per-note, for the same reason ageSamples is: the relax factor is
    // measured against this note's own peak. Carried over, a quiet note
    // following a loud one on the same string is divided by the loud one's
    // peak and sits near the floor for its whole length, which makes two
    // identical notes sound different depending on what preceded them.
    voice.vertical.handEnvelope = 0.0f;
    voice.vertical.handEnvelopePeak = 0.0f;
    voice.horizontal.handEnvelope = 0.0f;
    voice.horizontal.handEnvelopePeak = 0.0f;
    voice.noiseState = hash32(static_cast<std::uint32_t>(voice.stringIndex * 7349)
                              ^ static_cast<std::uint32_t>(midiNote * 131)
                              ^ static_cast<std::uint32_t>(
                                  (static_cast<int>(playStyle) * 2
                                   + (strokeIsUp ? 1 : 0)) * 17)
                              ^ static_cast<std::uint32_t>(
                                  voice.startOrder * 2654435761u));
    voice.artifactNoiseState = hash32(voice.noiseState ^ 0xa53c9e17u);
    voice.baseFrequency = midiToHz(static_cast<float>(midiNote));

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

    // The elastic depth depends on this stroke's contact position, which is
    // resolved in startExcitation(). No fresh attack contributes tension yet.
    voice.tensionDepth = 0.0f;

    // A ringing physical string carries the preceding bridge/body thud through
    // a repick or retarget. A genuinely fresh string starts silent;
    // startExcitation() adds the new impact at contact.
    if (! wasRinging)
    {
        voice.palmImpactVel = 0.0f;
        voice.palmImpactState = 0.0f;
    }

    // A delayed fret retarget still contains the preceding stroke. Keep that
    // ring under the hand that is physically present; the scheduled style is
    // restored by startExcitation() when its contact actually arrives.
    const PlayStyle dampingStyle = wasRinging && startDelaySamples > 0
        && lastHandContactClock_ >= 0
            ? lastHandContactPlayStyle_ : voice.playStyle;
    configureVoiceDamping(voice, dampingStyle);
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
            constexpr float retainedAmplitude = 0.28f;
            for (auto* loop : { &voice.vertical, &voice.horizontal })
            {
                for (auto& sample : loop->line)
                    sample *= retainedAmplitude;
            }
            constexpr float retainedEnergy =
                retainedAmplitude * retainedAmplitude;
            voice.energyEnvelope *= retainedEnergy;
            voice.outputEnergy *= retainedEnergy;
            voice.vertical.currentDelay = voice.vertical.targetDelay;
            voice.horizontal.currentDelay = voice.horizontal.targetDelay;
        }
    }

    // A strummed chord's later strings are already fretted and choked, but the
    // pick has not reached them yet. The audio-rate render countdown fires the
    // excitation once the travel time has elapsed.
    voice.startDelaySamples = std::max(0, startDelaySamples);
    voice.pendingContactPreservesRing = wasRinging
                                     && voice.startDelaySamples > 0;
    updateStyleWeights(voice);
    if (voice.startDelaySamples == 0)
        startExcitation(voice, velocity, false);
}

void ElectryEngine::legatoRetarget(Voice& voice, int midiNote, float velocity,
                                   PlayStyle playStyle) noexcept
{
    const auto& spec = stringSpecs()[static_cast<std::size_t>(voice.stringIndex)];
    const int fromFret = voice.fret;
    voice.legatoFromFrequency = voice.baseFrequency;
    voice.midiNote = midiNote;
    voice.fret = std::clamp(midiNote - spec.openMidiNote, 0, fretCount);
    voice.playStyle = playStyle;
    voice.strokeIsUp = false;
    voice.baseFrequency = midiToHz(static_cast<float>(midiNote));
    voice.keyDown = true;
    voice.keyDownCount = 1;
    voice.sustained = false;
    voice.releasing = false;
    voice.startOrder = ++noteSequence_;
    drawStrokeVariation(voice);
    voice.velocity = velocity;
    voice.velocityProfile = makeVelocityProfile(velocity);
    voice.handContactScale = clampf(
        voice.velocityProfile.amplitude
            * (plectrumContacts(playStyle, true)
                   ? voice.strokeForceGain : 1.0f),
        0.32f, 1.25f);
    voice.releaseGain = 1.0f;
    voice.releaseGainTarget = 1.0f;
    voice.artifactCollisionRemaining = 0;
    voice.startDelaySamples = 0;
    voice.pendingRepick.active = false;
    voice.pendingContactPreservesRing = false;

    // A hammered finger lands over roughly ten milliseconds rather than
    // instantly. A slide does not land at all: it stays down and travels, so
    // its duration is a distance divided by a hand speed rather than a fixed
    // time, and a twelve-fret slide takes six times as long as a two-fret one.
    // The hand speed follows the Bend Time control - the same travel-time
    // control the wheel uses - at 8% of it per fret, so the 280 ms default is
    // 22 ms per fret.
    const int frets = std::abs(voice.fret - fromFret);
    float glideSeconds = 0.010f;
    if (playStyle == PlayStyle::Slide)
        glideSeconds = clampf(0.08f * smoothedParameters_.bendTimeSeconds
                                  * static_cast<float>(std::max(frets, 1)),
                              0.030f, 1.200f);
    voice.legatoBlend = 0.0f;
    voice.legatoIncrement = static_cast<float>(controlPeriod)
        / (glideSeconds * static_cast<float>(sampleRate_));

    // The winding drags under the travelling finger. The ridges pass at v / w,
    // where v is the finger's speed along the string and w the winding pitch,
    // so a fast slide squeaks high and a slow one low - which is the whole
    // character of the sound. The position of fret n along the string is
    // L (1 - 2^(-n/12)) from the nut, so the distance the finger actually
    // covers shrinks as the slide moves up the neck, exactly as the frets do.
    voice.slideNoiseAmplitude = 0.0f;
    voice.slideNoiseLevel = 0.0f;
    voice.slideShaperHigh.reset();
    voice.slideShaperLow.reset();
    if (playStyle == PlayStyle::Slide && frets > 0)
    {
        const float openLength = scaleLengthMetres();
        const float fromPosition = openLength
            * (1.0f - std::exp2(-static_cast<float>(fromFret) / 12.0f));
        const float toPosition = openLength
            * (1.0f - std::exp2(-static_cast<float>(voice.fret) / 12.0f));
        const float speed = std::abs(toPosition - fromPosition) / glideSeconds;

        // Winding pitch. A real wrap wire runs from about 0.36 mm on a .080 to
        // about 0.18 mm on a .024, which is far flatter than the string
        // diameter itself; this linear stand-in reproduces that pair and is a
        // voicing estimate rather than a measurement. A plain string has no
        // winding at all, so it drags rather than squeaks.
        const float gaugeScale = lerp(1.0f, 11.0f / 9.0f,
                                      smoothedParameters_.stringGauge);
        const float diameterMm = spec.plainDiameterMm * gaugeScale;
        const float windingMetres = spec.wound
            ? 0.001f * (0.100f + 0.130f * diameterMm)
            : 0.0008f;
        const float centre = clampf(speed / windingMetres, 200.0f,
                                    0.40f * static_cast<float>(sampleRate_));
        voice.slideBandHigh = std::exp(-twoPi * std::min(
            1.6f * centre, 0.45f * static_cast<float>(sampleRate_))
            * inverseSampleRate_);
        voice.slideBandLow = std::exp(-twoPi * 0.6f * centre * inverseSampleRate_);
        voice.slideNoiseAmplitude = 0.55f
            * std::pow(smoothedParameters_.fingerNoise, 0.75f)
            * (spec.wound ? 1.0f : 0.10f)
            * clampf(speed * 1.6f, 0.0f, 1.4f);
    }

    configureVoiceDamping(voice, voice.playStyle);
    configureVoicePitch(voice, false);
    configureVoicePickups(voice);
    startExcitation(voice, velocity, true);
}

void ElectryEngine::beginVoiceRelease(Voice& voice) noexcept
{
    if (! voice.active)
        return;

    const bool wasAlreadyReleasing = voice.releasing;

    // A strum spreads the pick across the strings by up to 280 ms. If the key
    // is lifted before the pick arrives, it never lands: leaving the countdown
    // running would excite the string after the release and produce a late
    // attack on short notes.
    const bool cancelledBeforeContact = voice.startDelaySamples > 0;
    const bool preserveRingingString = voice.pendingContactPreservesRing;
    const std::uint64_t cancelledChord = voice.strumChordId;
    voice.startDelaySamples = 0;
    voice.pendingRepick.active = false;
    voice.pendingContactPreservesRing = false;
    if (cancelledBeforeContact)
    {
        voice.strumChordId = 0;

        // Alternate reserves one direction for the assembled chord at Note On.
        // If every picked member of the current chord is cancelled before any
        // contact, return that reservation: no wrist stroke actually landed.
        if (pickStyle_ == PickStyle::Alternate
            && cancelledChord == chordSequence_
            && chordAlternateConsumed_
            && ! chordContactOccurred_)
        {
            bool anotherPickedMember = false;
            for (const auto& member : voices_)
            {
                if (member.active && member.strumChordId == cancelledChord
                    && plectrumContacts(member.playStyle, false))
                {
                    anotherPickedMember = true;
                    break;
                }
            }
            if (! anotherPickedMember)
            {
                chordAlternateConsumed_ = false;
                alternateNextStrokeIsUp_ = chordStrokeIsUp_;
            }
        }

        // No plectrum, palm impact or fretting-hand strike ever reached a
        // fresh pending string. Keeping that silent allocation in Release
        // made its Palm/Dead style qualify as a whole-hand mute until the
        // ordinary 50 ms retirement floor elapsed, choking every string that
        // was already ringing. A delayed retrigger is different: it still
        // contains the previous stroke and must follow the normal release.
        if (! preserveRingingString)
        {
            silenceVoice(voice);
            updateActiveVoiceCount();
            return;
        }
    }

    // A released old stroke can receive and then cancel a pending repick. Its
    // release envelope was never replaced, so there is nothing to restart.
    if (wasAlreadyReleasing)
        return;

    voice.keyDownCount = 0;
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
        voice.noiseLengthDenominator = static_cast<float>(std::max(1, voice.noiseLength));
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
    voice.keyDownCount = 0;
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
    voice.artifactCollisionRemaining = 0;
    voice.artifactCollisionLength = 0;
    voice.energyEnvelope = 0.0f;
    voice.outputEnergy = 0.0f;
    voice.busContribution = 0.0f;
    voice.displayLevel = 0.0f;
    voice.startDelaySamples = 0;
    voice.pendingRepick.active = false;
    voice.pendingContactPreservesRing = false;
    voice.strumChordId = 0;
    voice.palmImpactVel = 0.0f;
    voice.palmImpactState = 0.0f;
    voice.releaseGain = 1.0f;
    voice.releaseGainTarget = 1.0f;
    voice.releaseGainCoefficient = 0.0f;
    voice.legatoBlend = 1.0f;
    voice.touchDepth = 0.0f;
    voice.touchHoldRemaining = 0;
    voice.touchFraction = 0.0f;
    voice.slideNoiseAmplitude = 0.0f;
    voice.slideNoiseLevel = 0.0f;
    voice.slideShaperHigh.reset();
    voice.slideShaperLow.reset();
    voice.vibratoSemitones = 0.0f;
    // Every retained filter state must clear with the voice, or a silenced
    // string could leak residue into a later, otherwise identical render and
    // break the engine's determinism contract.
    voice.apertureNeck.reset();
    voice.apertureBridge.reset();
    voice.coilPairNeck.reset();
    voice.coilPairBridge.reset();
    voice.excitationShaper.reset();
    voice.excitationModalShaper1.reset();
    voice.excitationModalShaper2.reset();
    voice.excitationReleaseShaper.reset();
    voice.excitationImageShaper.reset();
    voice.previousFluxNeck = 0.0f;
    voice.previousFluxBridge = 0.0f;
    voice.emfLowpassNeck.reset();
    voice.emfLowpassBridge.reset();
    voice.noiseShaper.reset();
    voice.noiseBandState = 0.0f;
    voice.artifactNoiseShaper.reset();
    voice.artifactNoiseBandState = 0.0f;
    voice.saddleRattle.reset();
    voice.lastConfiguredSemitones = -999.0f;
    voice.lastConfiguredFrequency = -1.0f;
    voice.lastCompensatedSemitones = -999.0f;
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
        bodyModeLevels_[index] = level;
        bodyModes_[index].configure(frequency, q, level,
                                    sampleRate);

        // bodyConductanceAt()'s per-mode geometry, at the same clamped
        // frequency and Q the conductance envelope itself clamps to.
        const float omegaMode = twoPi * std::max(frequency, 30.0f);
        bodyModeOmega_[index] = omegaMode;
        bodyModeOmegaSquared_[index] = omegaMode * omegaMode;
        bodyModeDamping_[index] = omegaMode / std::max(q, 2.0f);
    }
}

float ElectryEngine::bodyConductanceAt(float frequencyHz) const noexcept
{
    // A normalised modal-conductance envelope. Near a structural mode the
    // bridge accepts more string energy; far from every mode it approaches
    // zero. This response is evaluated only while configuring a note, not in
    // the sample loop, and is used exclusively to add loss.
    //
    // Each mode's own omega, omega-squared and loss rate are solved once in
    // configureBody() into bodyModeOmega_/bodyModeOmegaSquared_/
    // bodyModeDamping_ rather than here: this call is reached up to six times
    // per configureVoiceDamping() (once per partial), and only `frequencyHz`
    // - the sounding partial, not the mode geometry - actually changes
    // between those six calls.
    const float omega = twoPi * std::max(frequencyHz, 0.0f);
    const float omegaSquared = omega * omega;
    float response = 0.0f;
    float normaliser = 0.0f;
    for (int mode = 0; mode < bodyModeCount; ++mode)
    {
        const auto index = static_cast<std::size_t>(mode);
        const float level = std::max(bodyModeLevels_[index], 0.0f);
        const float dissipative = bodyModeDamping_[index] * omega;
        const float reactive = bodyModeOmegaSquared_[index] - omegaSquared;
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

    // A real palm mute is not one fixed amount of loss for the whole note. The
    // heel is already touching the string when it is picked, then the grip
    // slackens as the string stops pressing into it, so the tail opens back up
    // rather than staying clamped.
    //
    // This leaves the solved loop gain and damping pole alone and only re-pushes
    // a scaled depth into the loss band, so at a factor of one this is exactly
    // the static model and the fitted decay still holds. Away from one the decay
    // departs from the fit deliberately: that departure is the behaviour.
    {
        const auto modulate = [&] (PolarisationLoop& loop)
        {
            if (loop.handLossSolvedDepth <= 0.0f)
                return;
            const float relaxFactor = loop.handEnvelopePeak > 1.0e-7f
                ? 0.40f + 0.60f * clampf(
                      loop.handEnvelope / loop.handEnvelopePeak, 0.0f, 1.0f)
                : 1.0f;
            // The dip sits inside the loop, so its phase is part of the
            // sounding period - the same reason the loop compensates for it at
            // all. Moving the depth moves that phase, so the correction has to
            // move with it; left alone it stays pinned to the depth the note
            // started on while the audible depth walks away from it.
            //
            // Worth about a cent where it can be measured, not the 13 the
            // compensation is worth in total: most of that is already right
            // because the depth at note-on is the depth the correction was
            // built from, and the error only opens up as the factors depart
            // from one. Kept because the invariant should hold rather than
            // nearly hold, and it costs a recompute only while it is moving.
            // Re-flagged only on real movement so a settled tail stops paying.
            const float previousDepth = loop.handLossDepth;
            applyDipDepth(loop, loop.handLossSolvedDepth * relaxFactor);
            if (std::fabs(loop.handLossDepth - previousDepth) > 1.0e-4f)
                voice.compensationDirty = true;
        };
        modulate(voice.vertical);
        modulate(voice.horizontal);
    }

    if (voice.legatoBlend < 1.0f)
    {
        voice.legatoBlend = clampf(voice.legatoBlend + voice.legatoIncrement,
                                   0.0f, 1.0f);
        // The friction follows how fast the finger is actually moving, which
        // is the derivative of the glide's own smoothstep, 6 b (1 - b). It is
        // therefore zero at both ends and the squeak swells and dies with the
        // movement instead of switching on and off.
        if (voice.slideNoiseAmplitude > 0.0f)
        {
            const float b = voice.legatoBlend;
            voice.slideNoiseLevel = voice.slideNoiseAmplitude
                                  * 6.0f * b * (1.0f - b);
            if (voice.legatoBlend >= 1.0f)
            {
                voice.slideNoiseAmplitude = 0.0f;
                voice.slideNoiseLevel = 0.0f;
            }
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

inline void ElectryEngine::accumulateStereoContribution(RenderSums& sums,
                                                          float stereoLateral,
                                                          float neckWeight,
                                                          float neckSignal,
                                                          float bridgeWeight,
                                                          float bridgeSignal)
    const noexcept
{
    if (channelsLinked_)
    {
        sums.neck[0] += neckWeight * neckSignal;
        sums.bridge[0] += bridgeWeight * bridgeSignal;
        return;
    }
    const float side = stereoSideScale_ * stereoLateral;
    sums.neck[0] += neckWeight * neckSignal * (1.0f - side);
    sums.neck[1] += neckWeight * neckSignal * (1.0f + side);
    sums.bridge[0] += bridgeWeight * bridgeSignal * (1.0f - side);
    sums.bridge[1] += bridgeWeight * bridgeSignal * (1.0f + side);
}

void ElectryEngine::renderVoice(Voice& voice, RenderSums& sums) noexcept
{
    auto& vertical = voice.vertical;
    auto& horizontal = voice.horizontal;

    // The touching finger, if there is one. It is held while the note forms
    // and then lifts: by then the partials it removed have gone and cannot be
    // re-excited, so releasing it is free and stops paying for two extra
    // delay reads.
    if (voice.touchDepth > 0.0f)
    {
        if (voice.touchHoldRemaining > 0)
        {
            --voice.touchHoldRemaining;
        }
        else
        {
            voice.touchDepth -= voice.touchReleaseStep;
            if (voice.touchDepth < 1.0e-4f)
                voice.touchDepth = 0.0f;
        }
    }
    const float touchWeight = 0.5f * voice.touchDepth;
    const float touchDelayScale = 1.0f + voice.touchFraction;

    // Loop reads and the damping/dispersion chain.
    const auto advanceLoop = [&] (PolarisationLoop& loop, float extraFeedback)
    {
        float sample = loop.readFractional(loop.currentDelay);
        if (touchWeight > 0.0f)
        {
            // (1 - d/2) x(n) + (d/2) x(n - M), written as one blend so the
            // untouched path stays exactly the same instruction sequence.
            const float touched = loop.readFractional(
                loop.currentDelay * touchDelayScale);
            sample += touchWeight * (touched - sample);
        }
        sample = loop.dispersion1.process(sample, loop.dispersionLowCoefficient);
        sample = loop.dispersion2.process(sample, loop.dispersionLowCoefficient);
        sample = loop.dispersion3.process(sample, loop.dispersionLowCoefficient);
        sample = loop.dispersion4.process(sample, loop.dispersionLowCoefficient);
        sample = loop.dispersion5.process(sample, loop.dispersionHighCoefficient);
        sample = loop.dispersion6.process(sample, loop.dispersionHighCoefficient);
        sample = loop.dispersion7.process(sample, loop.dispersionHighCoefficient);
        sample = loop.dispersion8.process(sample, loop.dispersionHighCoefficient);
        sample = loop.damping.process(sample, loop.loopDampingCoefficient);
        // A peaking section with sub-unity gain: its magnitude never exceeds one
        // anywhere, so it cannot destabilise the loop at any depth, and because
        // it returns to unity above its band it costs almost nothing at the high
        // fitted point - which is what lets it be deep enough to matter.
        if (loop.handDipActive)
            sample = loop.handDip.process(sample);
        if (loop.handLossSolvedDepth > 0.0f)
        {
            // Cheap one-pole follower on the loop itself: what the heel's grip
            // tracks is how hard the string is still pressing into it. Peak-
            // relative rather than absolute deliberately - an absolute reference
            // was built and measured, and was worse at every scale tried.
            const float rectified = sample < 0.0f ? -sample : sample;
            loop.handEnvelope += handEnvelopeCoefficient_
                               * (rectified - loop.handEnvelope);
            if (loop.handEnvelope > loop.handEnvelopePeak)
                loop.handEnvelopePeak = loop.handEnvelope;
        }
        sample *= loop.loopGain * voice.releaseGain * extraFeedback;
        return sample;
    };

    // Pick contact briefly chokes an already ringing string.
    const float contactChoke = voice.excitationPhase == ExcitationPhase::Contact
        ? voice.contactFeedbackGain : 1.0f;

    float verticalSample = advanceLoop(vertical, contactChoke);
    float horizontalSample = advanceLoop(horizontal, contactChoke);

    // Ordinary hard-picked notes can make brief, irregular fret contact too.
    // The Artifacts control blends toward the same bounded collision law used
    // by Slap, but a separate deterministic noise stream preserves the clean
    // engine's exact contact-noise sequence.
    float artifactContactSignal = 0.0f;
    float artifactExcess = 0.0f;
    // A fresh loop reads silence until the travelling excitation returns.
    // Start spending the opportunity only when the string itself is moving;
    // otherwise an E1 uses almost one 24 ms round trip merely opening an
    // already unreachable clearance.
    if (artifactsActive_ && voice.artifactCollisionRemaining > 0
        && voice.outputEnergy > 0.0f)
    {
        --voice.artifactCollisionRemaining;
        const float progress = 1.0f
            - static_cast<float>(voice.artifactCollisionRemaining)
              / voice.artifactCollisionLengthDenominator;
        const float clearance = voice.artifactClearance
                              * (1.0f + 3.5f * progress);
        artifactExcess = std::max(std::abs(verticalSample) - clearance, 0.0f);
        if (artifactExcess > 0.0f)
        {
            const float contact = artifactContactShape_
                                * voice.velocityProfile.collision
                                * (0.70f + 0.30f * voice.lowStringWeight);
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
        }
    }

    // Passive bridge coupling exchanges a little energy between the two
    // polarisations; the mixing matrix is contractive, so it stays stable. The
    // depth is solved per round trip in configureVoicePitch().
    const float coupling = voice.polarisationCoupling;
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
                           / voice.noiseLengthDenominator;
        const float raw = bipolarNoise(voice.noiseState);
        const float lowpassed = voice.noiseShaper.process(raw, voice.noiseBandCoefficient);
        // Band-shaped scrape: low-passed noise minus its own slower average.
        voice.noiseBandState += contactNoiseBandCoefficient_
                              * (lowpassed - voice.noiseBandState);
        noiseSample = (lowpassed - voice.noiseBandState)
                    * voice.noiseAmplitude * window * window;
    }

    // The travelling finger's friction. Two one-poles form a band at the rate
    // the winding ridges pass under it, so the squeak follows the hand's speed
    // rather than being a fixed sound; the level follows the glide's own
    // motion, so it is exactly zero when the finger is still.
    if (voice.slideNoiseLevel > 0.0f)
    {
        const float raw = bipolarNoise(voice.noiseState);
        const float high = voice.slideShaperHigh.process(raw, voice.slideBandHigh);
        noiseSample += voice.slideNoiseLevel
                     * (high - voice.slideShaperLow.process(
                            high, voice.slideBandLow));
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
            / voice.excitationLengthDenominator;
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
    // per attack in updateStyleWeights().
    const float verticalWeight = voice.verticalWeight;
    const float horizontalWeight = voice.horizontalWeight;

    // The acoustic return pushes the sounding string the way a loudspeaker
    // does: broadband pressure that only the string's own resonances turn
    // into sustained motion. Exactly zero unless the resonance control is up.
    // The muting hand lies across the played strings just as it does across
    // the idle ones, so the same hand factor starves this injection too -
    // without it a palm-muted passage howled at nearly the open level,
    // because the mute's deliberate fundamental relief leaves f0 almost
    // lossless and continuous drive there regenerates.
    float verticalTotal = verticalIn + injected * verticalWeight
                        + 0.35f * artifactContactSignal;
    float horizontalTotal = horizontalIn + injected * horizontalWeight
                          + 0.12f * artifactContactSignal;
    if (feedbackDrive_ != 0.0f)
    {
        const float handStarved = feedbackDrive_ * feedbackHandScale_;
        verticalTotal += handStarved;
        horizontalTotal += 0.35f * handStarved;
    }

    // The saddle this string terminates on is shared and is not rigid. The
    // other played strings' summed force moves it, and a moving termination
    // drives this string - which is why a chord sounds like one instrument
    // rather than six of them, and why a power chord's low interval blooms.
    //
    // `busContribution` is this voice's own share of the bus from the previous
    // sample and is subtracted before the read, so the coupling matrix has an
    // exactly zero diagonal. Without that a single note would drive itself
    // through the bus, which is not string coupling at all but a second copy
    // of its own bridge termination - already carried by `bodyConductance`
    // and `bodyLossFactor`, and with every decay time, T60 and timbre
    // calibration in the instrument sitting downstream of it.
    //
    // The same weights appear on the way in as on the way out: `bridgeForce`
    // below reads the two polarisations 0.5/0.5, so the saddle drives them
    // 0.5/0.5, which is what reciprocity of a passive mechanical junction
    // requires. Folding the two halves into the gain leaves one multiply.
    if (bridgeCouplingInjection_ != 0.0f)
    {
        const float others = sympatheticBusDelayed_ - voice.busContribution;
        const float saddle = bridgeCouplingInjection_ * others;
        verticalTotal += saddle;
        horizontalTotal += saddle;
    }

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

    // Neck and bridge run the identical tap/aperture/coil/flux/EMF chain over
    // their own delay tap, aperture window and coil pair; only the artifact
    // and contact-noise blend weights differ between the two anchors. One
    // shared lambda replaces the two copies of that chain.
    const auto readPickup = [&] (const DelayTap& delayTap,
                                 FractionalMovingAverage& aperture,
                                 CoilPairSum& coilPair,
                                 float drive, float driveInverse,
                                 OnePole& emfLowpass, float& previousFlux,
                                 float artifactWeight, float noiseWeight)
    {
        float tap = 0.85f * (verticalTotal
                             - pickupCombDepth * vertical.readTap(delayTap))
                  + 0.35f * (horizontalTotal
                             - pickupCombDepth * horizontal.readTap(delayTap))
                  + artifactWeight * artifactPickup;
        tap = aperture.process(coilPair.process(tap));
        const float flux = voice.fluxScale
            * magneticTransfer(tap, drive, driveInverse);
        // Faraday's law: a magnetic pickup outputs induced voltage,
        // proportional to d(Phi)/dt, rather than displacement itself.
        // Normalising the finite difference at 220 Hz preserves practical
        // level while retaining the physically important frequency weighting.
        // The oversampled lowpass bounds the differentiator before the
        // loaded-coil circuit.
        float signal = emfLowpass.process(
            (flux - previousFlux) * emfScale_, emfLowpassCoefficient_);
        previousFlux = flux;
        // Local contact motion reaches the pickup as a short velocity-like
        // transient. It still passes through the shared loaded-coil circuit,
        // but does not masquerade as a persistent pitched wave on the string.
        signal = (signal + noiseWeight * noiseSample) * voice.articulationMakeup;
        return signal;
    };

    if (neckPathActive_)
        neckSignal = readPickup(voice.pickupTapNeck, voice.apertureNeck,
                                 voice.coilPairNeck, magneticDriveNeck_,
                                 magneticDriveNeckInverse_, voice.emfLowpassNeck,
                                 voice.previousFluxNeck, 0.55f, 0.09f);

    if (bridgePathActive_)
        bridgeSignal = readPickup(voice.pickupTapBridge, voice.apertureBridge,
                                   voice.coilPairBridge, magneticDriveBridge_,
                                   magneticDriveBridgeInverse_,
                                   voice.emfLowpassBridge, voice.previousFluxBridge,
                                   1.0f, 0.15f);

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
    accumulateStereoContribution(sums, voice.stereoLateral, 0.5f, neckSignal,
                                 0.5f, bridgeSignal);

    // The bridge passes string vibration and playing noise into the body, and
    // the same bridge force drives the sympathetic coupling bus. Every voice
    // writes to the bus; the unfingered strings read all of it and the played
    // strings read it minus their own share, recorded here for the next
    // sample. The graph is no longer acyclic, so the played strings' gain is
    // held under an explicit spectral-radius bound in solveBridgeCoupling().
    const float bridgeForce = 0.5f * (verticalTotal + horizontalTotal);
    sympatheticBus_ += bridgeForce;
    voice.busContribution = bridgeForce;
    sums.body += bridgeForce + 1.6f * noiseSample;
    constexpr float palmImpactVelocityFloor = 1.0e-4f;
    constexpr float palmImpactStateFloor = 1.0e-7f;
    if (voice.palmImpactVel > 0.0f || std::abs(voice.palmImpactState) > 0.0f)
    {
        const float impactDrive = voice.palmImpactVel > palmImpactVelocityFloor
            ? voice.palmImpactVel : 0.0f;
        voice.palmImpactState += palmImpactThudCoefficient_
                                * (impactDrive - voice.palmImpactState);
        voice.palmImpactVel = impactDrive > 0.0f
            ? impactDrive * palmImpactVelocityRetention_ : 0.0f;
        if (voice.palmImpactVel <= palmImpactVelocityFloor)
            voice.palmImpactVel = 0.0f;
        if (voice.palmImpactVel == 0.0f
            && std::abs(voice.palmImpactState) <= palmImpactStateFloor)
        {
            voice.palmImpactState = 0.0f;
        }
        else
        {
            sums.body += 0.45f * voice.palmImpactState;
        }
    }
    if (artifactsActive_)
        sums.body += 0.9f * artifactContactSignal
                   + 0.09f * artifactBuzzAmount_ * saddleRattle;

    // The slow energy envelope feeds tension modulation: its release side
    // follows the string's own decay scale so the attack pitch glide relaxes
    // over hundreds of milliseconds, as measured tension modulation does.
    const float instantaneousLoopEnergy = verticalSample * verticalSample
                                        + horizontalSample * horizontalSample;
    const float deflectionSquared = instantaneousLoopEnergy
        * waveguideDisplacementMetresPerUnit
        * waveguideDisplacementMetresPerUnit;
    const float coefficient = deflectionSquared > voice.energyEnvelope
        ? energyAttackCoefficient_ : energyReleaseCoefficient_;
    voice.energyEnvelope += coefficient
                          * (deflectionSquared - voice.energyEnvelope);

    // A separate, faster follower drives voice retirement only. Tying that to
    // the slow tension envelope kept an inaudible released string alive for
    // several seconds, needlessly holding its slot; this follower falls to
    // the retirement floor within about half a second of the audio going
    // silent while still tracking a genuine sustain.
    const float retireCoefficient = instantaneousLoopEnergy > voice.outputEnergy
        ? retireAttackCoefficient_ : retireReleaseCoefficient_;
    voice.outputEnergy += retireCoefficient
                        * (instantaneousLoopEnergy - voice.outputEnergy);

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

    // The bridge bus carries the played strings' force; the acoustic return
    // carries the loudspeaker's. Both are starved by the muting hand, which
    // is what keeps a palm-muted passage from howling even at full resonance.
    const float total = sample + sympatheticInjection_ * drive
                      + feedbackDrive_ * feedbackHandScale_;
    loop.line[static_cast<std::size_t>(loop.writeIndex & (delayLineSize - 1))]
        = total;
    // The same physical pickup senses this string, so it cancels no better here
    // than it does on a played one: `pickupCombDepth` has to apply to the
    // coupled ring too, or a sympathetically excited low string keeps the
    // hollow fundamental and the infinitely deep position nulls this weight
    // exists to remove.
    const float tap = total
        - pickupCombDepth * loop.readTap(voice.sympatheticPickupTap);
    loop.writeIndex = (loop.writeIndex + 1) & (delayLineSize - 1);
    loop.currentDelay += loop.delaySmoothing
                       * (loop.targetDelay - loop.currentDelay);

    const float flux = voice.fluxScale * tap;
    const float emf = voice.sympatheticEmf.process(
        (flux - voice.sympatheticPreviousFlux) * emfScale_,
        emfLowpassCoefficient_);
    voice.sympatheticPreviousFlux = flux;

    accumulateStereoContribution(sums, voice.stereoLateral, 0.28f, emf, 0.5f,
                                 emf);
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
    for (auto& mode : artifactRingModes_)
        mode.reset();
    for (auto& decimator : decimators_)
        decimator.reset();
    previousBodyDisplacement_ = 0.0f;
    bodyEmfLowpass_.reset();
    sympatheticBus_ = 0.0f;
    sympatheticBusDelayed_ = 0.0f;
    silentInternalSamples_ = 0;
}

ElectryEngine::StereoSample ElectryEngine::renderInternalSample(
    float acousticIn) noexcept
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
        smoothTowards(s.resonanceDepth, t.resonanceDepth);
        smoothTowards(s.vibratoDepth, t.vibratoDepth);
        s.strumSpreadSeconds = t.strumSpreadSeconds;
        s.bendTimeSeconds = t.bendTimeSeconds;
        s.pickupSelector = t.pickupSelector;
        s.outputMode = t.outputMode;
        smoothTowards(s.outputGain, t.outputGain);

        palmMuteBlend_ = clampf(s.palmMute + palmMutePressure_, 0.0f, 1.0f);
        if (t.palmMute <= 0.0f && s.palmMute < 1.0e-5f)
            s.palmMute = 0.0f;

        // CC1 is the performance resonance: it lifts the coupling from the
        // Sympathetic Ring parameter's base amount toward total, and opens
        // the acoustic feedback path, both scaled by the Resonance Depth
        // parameter. At a lowered wheel it snaps to an exact zero so the
        // parameter alone decides everything, bit for bit.
        resonanceAmount_ += resonanceCoefficient_
                          * (resonanceTarget_ - resonanceAmount_);
        if (resonanceTarget_ <= 0.0f && resonanceAmount_ < 1.0e-5f)
            resonanceAmount_ = 0.0f;
        const float resonanceLift = resonanceAmount_ * s.resonanceDepth;
        const float effectiveSympathetic = s.sympatheticAmount
            + resonanceLift * (1.0f - s.sympatheticAmount);

        // The feedback gain grows with the square of the lift, so half a
        // wheel adds bloom while the top of the wheel lets a loud amplified
        // tone regenerate outright. It also scales with the rig's acoustic
        // loudness: a DI that is not audible in the room cannot excite the
        // strings however far the wheel is pushed. The drive itself is
        // soft-clipped where it is consumed, so no input signal can make this
        // gain unsafe.
        returnLevel_ += resonanceCoefficient_
                      * (returnLevelTarget_ - returnLevel_);
        if (returnLevelTarget_ <= 0.0f && returnLevel_ < 1.0e-5f)
            returnLevel_ = 0.0f;
        feedbackGain_ = resonanceLift > 0.0f && returnLevel_ > 0.0f
            ? 0.045f * resonanceLift * resonanceLift * returnLevel_
            : 0.0f;

        // The sympathetic coupling bypasses exactly at zero: the coupled loops
        // stop being rendered and their state is dropped, so an idle engine is
        // bit-for-bit identical to one built without the feature.
        sympatheticGain_ = 0.0045f * effectiveSympathetic;
        if (t.sympatheticAmount <= 0.0f && s.sympatheticAmount < 1.0e-5f
            && resonanceLift <= 0.0f)
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
            // dead-note passage therefore damps the coupled strings and
            // starves their feed, which keeps a Drop-E chug tight instead of
            // washing it in open-string ring.
            // Continuous Palm Pressure is already realised in each coupled
            // string's loop decay by configureSympatheticString(). Only the
            // most recent physical articulation adds this second, whole-hand
            // contact loss; feeding Palm Pressure through both paths made the
            // declared 80 ms endpoint collapse to about 29 ms. A released
            // Palm tail remains the latest instruction through an ordinary
            // chug gap. Once a newer physical contact arrives, however, that
            // contact moves the one shared hand: MIDI lookahead for a delayed
            // stroke cannot outrank an earlier real pick, and retiring the
            // newer voice cannot reveal an older hand position again.
            float styleHandMute = 0.0f;
            if (lastHandContactClock_ >= 0
                && lastHandContactPlayStyle_ == PlayStyle::PalmMute)
                styleHandMute = 0.55f + 0.45f * s.muteDamping;
            else if (lastHandContactClock_ >= 0
                     && lastHandContactPlayStyle_ == PlayStyle::Dead)
                styleHandMute = sympatheticHandMuteForT60(deadHandT60);
            if (styleHandMute != sympatheticHandMute_)
            {
                sympatheticHandMute_ = styleHandMute;
                // A per-sample contact loss, which is how a hand resting on a
                // string actually damps it: distributed, not once per period.
                const float handT60 = std::exp(lerp(
                    std::log(sympatheticOpenHandT60),
                    std::log(sympatheticStoppedHandT60), styleHandMute));
                sympatheticHandGainTarget_ = styleHandMute > 0.0f
                    ? std::pow(10.0f,
                               -3.0f / (handT60
                                        * static_cast<float>(sampleRate_)))
                    : 1.0f;
            }
            sympatheticHandGain_ += parameterSmoothingCoefficient_
                * (sympatheticHandGainTarget_ - sympatheticHandGain_);
            const float handMute = std::max(palmMuteBlend_, styleHandMute);
            // How much of the coupling survives the mute, shared by the three
            // laws below instead of being reclamped from handMute three times:
            // the bridge-bus injection and the played strings' coupling both
            // scale linearly with it, and the feedback path raises it to the
            // fourth power.
            const float handOpen = std::max(0.0f, 1.0f - handMute);
            sympatheticInjection_ = sympatheticGain_ * handOpen;
            // The same hand lies across the strings that are being played, and
            // it lies on them at the saddle, which is exactly where the shared
            // bridge delivers this coupling. So the played strings' share of
            // the bus is starved by the same factor the idle strings' share is.
            bridgeCouplingNominal_ = bridgeCouplingGain * effectiveSympathetic
                * handOpen;
            // A far steeper law than the bridge-bus injection above: feedback
            // is a regenerating loop, so any residue above the loop's
            // regeneration threshold climbs back to a full howl no matter how
            // small it is - measured, the linear 20% residue of a default
            // palm mute howled at nearly the open level, and even its square
            // still regenerated through a cranked amplifier. The fourth power
            // takes the default mute's residue 55 dB down, safely below the
            // threshold, while a light touch (small handMute) still lets a
            // deliberate howl through.
            const float handOpenSquared = handOpen * handOpen;
            feedbackHandScale_ = handOpenSquared * handOpenSquared;
        }
        else
        {
            sympatheticInjection_ = 0.0f;
            bridgeCouplingNominal_ = 0.0f;
            feedbackHandScale_ = 1.0f;
        }

        if (t.artifactAmount <= 0.0f && s.artifactAmount < 1.0e-5f)
        {
            s.artifactAmount = 0.0f;
            if (artifactsActive_)
            {
                for (auto& mode : artifactRingModes_)
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

        // The strings glide to the wheel rather than snapping: the Bend Time
        // parameter is the travel time of the bend, exactly as it was for the
        // finger bends it used to drive, so the wheel bends like a hand on
        // the bar. Settling exactly on the target stops the glide from
        // re-flagging every voice's pitch solve forever.
        if (std::abs(s.bendTimeSeconds - appliedBendGlideSeconds_) > 1.0e-4f)
        {
            appliedBendGlideSeconds_ = s.bendTimeSeconds;
            bendGlideCoefficient_ = 1.0f - std::exp(
                -static_cast<float>(controlPeriod)
                / (0.33f * std::max(s.bendTimeSeconds, 0.01f)
                   * static_cast<float>(sampleRate_)));
        }
        // The fretting hand's vibrato. The pressure ramps at a bounded rate
        // and is then shaped by smoothStep, so the hand leaves rest with zero
        // slope instead of at its steepest, and comes back to rest the same
        // way. At zero pressure the ramp sits at exactly zero, the shaping
        // returns exactly zero, and every voice's pitch solve is bit-for-bit
        // what it was.
        vibratoRamp_ += clampf(vibratoTarget_ - vibratoRamp_,
                               -vibratoOnsetIncrement_, vibratoOnsetIncrement_);
        vibratoAmount_ = smoothStep(vibratoRamp_);
        if (vibratoAmount_ > 0.0f)
        {
            // The pitch follows the *square* of the finger's displacement.
            // Rocking a stopped string sideways by x lengthens its path by
            // dL = k x^2 - the same dL/L relation bendSensitivity() solves for
            // the bar - so a wrist rocking as the raised cosine s(t) moves the
            // pitch by depth * s(t)^2. That is not a flat top: s^2 is
            // fourth-order small where s is second-order small, so the note
            // dwells at the fretted pitch between excursions and the
            // excursions themselves are briefer and sharper-cornered than the
            // rock that makes them. It is still one-sided, so the note is only
            // ever pushed sharp, which is what a finger can do to a string and
            // a bar cannot be made to do.
            //
            // A rock finger vibrato runs around 5 Hz and speeds up as the
            // player leans into it. The Vibrato Depth control scales the
            // excursion only: which way a real hand couples width to speed was
            // never measured here, so the existing coupling stands rather than
            // being reversed on an unsourced argument.
            const float rate = lerp(4.8f, 6.4f, vibratoAmount_);
            const float depth = lerp(vibratoMinimumSemitones,
                                     vibratoMaximumSemitones, s.vibratoDepth)
                              * vibratoAmount_;
            // Every stopped string is its own finger, with its own phase and
            // its own per-cycle rate and excursion. Open strings have nothing
            // holding them down and are left alone, exactly as before.
            for (auto& voice : voices_)
            {
                if (! voice.active || voice.fret <= 0)
                    continue;
                voice.vibratoPhase += rate * voice.vibratoRateScale
                                    * vibratoPhaseIncrement_;
                if (voice.vibratoPhase >= 1.0f)
                {
                    voice.vibratoPhase -= 1.0f;
                    drawVibratoCycle(voice);
                }
                const float rock = 0.5f
                    * (1.0f - std::cos(twoPi * voice.vibratoPhase));
                voice.vibratoSemitones = depth * voice.vibratoDepthScale
                                       * rock * rock;
            }
        }
        else
        {
            // The phase is deliberately left where it stopped rather than
            // reset: a finger that stops rocking and starts again does not
            // begin from the same point in the cycle, which is what made every
            // vibrato in a part start identically.
            for (auto& voice : voices_)
                voice.vibratoSemitones = 0.0f;
        }

        pitchBendSemitones_ += bendGlideCoefficient_
                             * (pitchBendTarget_ - pitchBendSemitones_);
        if (std::abs(pitchBendSemitones_ - pitchBendTarget_) < 5.0e-4f)
            pitchBendSemitones_ = pitchBendTarget_;

        // The bar bends the strings nobody is fingering too: retune the
        // ringing coupled strings in place whenever the wheel has moved.
        if (sympatheticActive_
            && std::abs(pitchBendSemitones_ - sympatheticAppliedBend_) > 1.0e-3f)
        {
            sympatheticAppliedBend_ = pitchBendSemitones_;
            for (auto& voice : voices_)
                if (! voice.active && voice.sympatheticReady)
                    configureSympatheticString(voice);
        }

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
                voice.coilPairNeck.reset();
                voice.previousFluxNeck = 0.0f;
                voice.emfLowpassNeck.reset();
            }
        }
        if (bridgeWanted && ! bridgePathActive_)
        {
            for (auto& voice : voices_)
            {
                voice.apertureBridge.reset();
                voice.coilPairBridge.reset();
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
        stereoSideScale_ = 0.24f * stereoWidth_;

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

    // The loudspeaker's pressure on the strings this sample. The rational
    // soft clip bounds any host signal to +/-1 before the gain, so the
    // injection is bounded whatever comes back around the loop; the gain is
    // exactly zero unless the resonance wheel is up and the rig is audible.
    if (feedbackGain_ > 0.0f)
    {
        const float bounded = acousticIn
            / std::sqrt(1.0f + acousticIn * acousticIn);
        feedbackDrive_ = feedbackGain_ * bounded;
    }
    else
    {
        feedbackDrive_ = 0.0f;
    }

    bool rendered = false;
    for (auto& voice : voices_)
    {
        if (voice.active)
        {
            renderVoice(voice, sums);
            // Strum travel is timed on the audio clock. Render this pending
            // sample silent first, then arm the excitation for the next one;
            // doing this at the control tick charged the first 16 samples
            // immediately and made every delayed pick arrive early.
            if (voice.startDelaySamples > 0
                && --voice.startDelaySamples == 0)
            {
                if (voice.pendingRepick.active)
                {
                    const auto pending = voice.pendingRepick;
                    const bool keyDown = voice.keyDown;
                    const int keyDownCount = voice.keyDownCount;
                    const bool sustained = voice.sustained;
                    voice.pendingRepick.active = false;
                    startVoice(voice, voice.midiNote, pending.velocity,
                               pending.playStyle, pending.strokeIsUp, 0,
                               pending.startOrder, true);
                    // Note Off and CC64 may have changed ownership during the
                    // pre-roll; committing the sound must not erase them.
                    voice.keyDown = keyDown;
                    voice.keyDownCount = keyDownCount;
                    voice.sustained = sustained;
                }
                else
                {
                    startExcitation(voice, voice.velocity, false);
                }
            }
            rendered = true;
        }
        else if (sympatheticActive_
                 && (voice.sympatheticEnergy > 1.0e-11f
                     || std::abs(sympatheticDrive) > 1.0e-6f
                     || std::abs(feedbackDrive_) > 1.0e-9f))
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
        for (auto& mode : artifactRingModes_)
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
    // Same 220 Hz differentiator-to-EMF scale as the per-string pickup path
    // uses, so it reuses `emfScale_` (solved once in prepare()) instead of
    // repeating the division here on every internal sample.
    const float bodyDriveVoltage =
        (bodyDisplacement - previousBodyDisplacement_) * emfScale_;
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
        for (auto& mode : artifactRingModes_)
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
        // One acoustic-return sample per host sample. The path is silent
        // until the host pushes something and while the ring has run dry.
        feedbackPrevious_ = feedbackCurrent_;
        if (feedbackAvailable_ > 0)
        {
            feedbackCurrent_ =
                feedbackRing_[static_cast<std::size_t>(feedbackReadIndex_)];
            feedbackReadIndex_ = (feedbackReadIndex_ + 1)
                               & (feedbackRingSize - 1);
            --feedbackAvailable_;
        }
        else
        {
            feedbackCurrent_ = 0.0f;
        }

        StereoSample output;
        if (oversamplingFactor_ == 2)
        {
            // In Mono only one decimator runs; its state is copied to the
            // second the instant the stereo field opens.
            bool linked = channelsLinked_;
            bool frozen = true;
            for (int phase = 0; phase < 2; ++phase)
            {
                // A linear midpoint upsamples the host-rate acoustic return;
                // the air path this stands in for is far darker than that.
                const float acoustic = phase == 0
                    ? 0.5f * (feedbackPrevious_ + feedbackCurrent_)
                    : feedbackCurrent_;
                const auto internal = renderInternalSample(acoustic);
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
            output = renderInternalSample(feedbackCurrent_);
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
    // With no played string left there is no phrase whose last contact must be
    // preserved. The next real attack starts a fresh physical history.
    if (activeVoiceCount_ == 0)
    {
        lastHandContactClock_ = -1;
        lastHandContactPlayStyle_ = PlayStyle::Sustain;
        lastHandContactOrder_ = 0;
    }
    solveBridgeCoupling();
}

void ElectryEngine::solveBridgeCoupling() noexcept
{
    // With fewer than two voices sounding there is no other string to couple
    // to: the bus is this voice's own contribution and `bus - own` is exactly
    // zero, so the gain is irrelevant and the bound is trivially met.
    if (bridgeCouplingNominal_ <= 0.0f || activeVoiceCount_ < 2)
    {
        bridgeCouplingInjection_ = 0.0f;
        bridgeCouplingRowSum_ = 0.0f;
        return;
    }

    // The worst loop in the chord decides the bound, because the common mode
    // reaches all of them. Both polarisations are read: they carry separate
    // gains and the injection enters both.
    float worstAmplification = 1.0f;
    for (const auto& voice : voices_)
    {
        if (! voice.active)
            continue;
        for (const float gain : { voice.vertical.loopGain,
                                  voice.horizontal.loopGain })
        {
            const float headroom = std::max(1.0f - gain, 1.0e-6f);
            worstAmplification = std::max(worstAmplification, 1.0f / headroom);
        }
    }

    const float others = static_cast<float>(activeVoiceCount_ - 1);
    const float ceiling = bridgeCouplingRowSumBound
                        / (others * worstAmplification);
    bridgeCouplingInjection_ = std::min(bridgeCouplingNominal_, ceiling);
    bridgeCouplingRowSum_ = others * bridgeCouplingInjection_
                          * worstAmplification;
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
        state.playStyle = voice.playStyle;
        state.strokeUp = voice.strokeIsUp;
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
    return pitchBendSemitones_ * bendSensitivity(voice.stringIndex);
}

} // namespace electry
