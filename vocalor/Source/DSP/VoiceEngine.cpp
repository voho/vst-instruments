#include "VoiceEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace vocalor
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

// Base level of the aspiration noise that is injected at the glottis, relative
// to the voiced excitation at the same point.
constexpr float aspirationLevel = 0.62f;
// Small unfiltered component: not all breath noise is generated below the
// tract, and a little direct air keeps consonantal "hh" detail audible.
constexpr float directAirLevel = 0.055f;
// Overall level of the tract output. Calibrated so a solo AAH at the default
// settings lands within a couple of dB of the level the 1.1 engine produced,
// now that the formant bank no longer changes its own gain with the vowel, the
// formant shift and the sample rate.
constexpr float tractLevel = 1.00f;
// Constant DC offset added to every tract excitation. Far below the noise
// floor of any converter, but large enough that the recursive filter states
// settle on a normal float instead of drifting into denormals.
constexpr float denormalBias = 1.0e-20f;
// Target RMS of harmonics 1..24 for the lax and pressed glottal prototypes.
// These are the measured values of the 1.0 wavetables: matching the band the
// formants actually sit in keeps both the absolute loudness and the level
// relationship of the Tension control unchanged while the pulse shape and the
// spectral tilt become physically derived.
constexpr int sourceBandHarmonics = 24;
constexpr float sourceBandRms[2] { 0.587f, 0.441f };
// The rate every level-calibrated constant in this file is expressed against.
constexpr float referenceSampleRate = 48000.0f;
// Sopranos do not carry the sub-1 kHz-wide tenor/baritone singer's-formant
// cluster into their upper range. At low and middle pitches their measured
// reinforcement region is usually at least 2 kHz wide. One quarter of the
// male convergence is an engineering mapping that produces that aggregate
// span with this five-pole model, not a measured anatomical coefficient. It
// then disappears between the two upper pitches measured by Weiss, Brown and
// Morris (622 and 932 Hz).
constexpr float femaleClusterStrength = 0.25f;
constexpr float sopranoClusterReleaseStartHz = 622.25f;
constexpr float sopranoClusterReleaseEndHz = 932.33f;
// Direct broadband excitation of trained sopranos found that R3 and R4 rise
// with f0 rather than locking to its harmonics (Joliveau et al., 2004). The
// reported regressions were 0.48 +/- 0.39 Hz/Hz and 0.46 +/- 0.38 Hz/Hz. C4 is
// the fixed low-register hinge of this mapping, so every female tract retains
// its existing centre frequencies there and below.
constexpr float sopranoUpperRiseAnchorHz = 261.63f;
// The regression was observed across the singers' comfortable range (the
// paper's high-range summary ends at B5), with C6 the upper worked example and
// the trend reported to about 1 kHz. Keep the measured linear law through B5,
// then ease its derivative to zero by C-sharp6. The resulting displacement is
// almost exactly the linear C6 displacement, but it has no hard corner and is
// not extrapolated through the non-human remainder of MIDI where independent
// population slopes can overtake one another.
constexpr float sopranoUpperRiseSoftLimitHz = 987.77f;
constexpr float sopranoUpperRiseEndHz = 1108.73f;
constexpr float sopranoR3RiseMean = 0.48f;
constexpr float sopranoR3RiseDeviation = 0.39f;
constexpr float sopranoR4RiseMean = 0.46f;
constexpr float sopranoR4RiseDeviation = 0.38f;

/** Drive compensation for a one-pole smoother fed by white noise.

    y += c * (x - y) driven by white noise of variance v settles at output
    variance v * c / (2 - c). Once c is derived from a corner frequency or a
    time constant it falls with the sample rate, so an uncompensated smoother's
    output amplitude falls as 1/sqrt(sampleRate) even though its spectrum is
    now correct. This returns the factor that restores the output the smoother
    would have had at 48 kHz - the same compensation aspirationScale_ applies
    to the (unsmoothed) aspiration noise.
*/
float noiseSmootherScale(float coefficient, float referenceCoefficient) noexcept
{
    const float safe = std::clamp(coefficient, 1.0e-9f, 1.999999f);
    const float reference = std::clamp(referenceCoefficient, 1.0e-9f, 1.999999f);
    return std::sqrt((reference * (2.0f - safe)) / (safe * (2.0f - reference)));
}

float clampUnit(float value) noexcept
{
    if (!std::isfinite(value))
        return 0.0f;
    return std::clamp(value, 0.0f, 1.0f);
}

float smoothStep(float value) noexcept
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

/** Maps the Vowel enum to the 0/1/2 (AAH/OOH/UUH) index the preset tables and
    the drift/display bookkeeping are indexed by. Five call sites resolved
    this same three-way comparison independently; one mapping now backs all
    of them. Named vowelIndexOf() rather than vowelIndex() because several of
    those call sites already have a local variable named vowelIndex. */
int vowelIndexOf(Vowel vowel) noexcept
{
    return vowel == Vowel::Ooh ? 1 : (vowel == Vowel::Uuh ? 2 : 0);
}

void separateUpperFormantBandwidths(
    const std::array<float, kFormantCount>& formantHz,
    std::array<float, kFormantCount>& formantBandwidth) noexcept
{
    constexpr float spacingMargin = 0.98f;
    constexpr float minimumBandwidth = 20.0f;
    for (int formant = 2; formant < 4; ++formant)
    {
        const auto lower = static_cast<std::size_t>(formant);
        const auto upper = static_cast<std::size_t>(formant + 1);
        const float gap = formantHz[upper] - formantHz[lower];
        const float maximumCombined = 2.0f * spacingMargin * gap;
        if (maximumCombined < 2.0f * minimumBandwidth)
            continue;

        const float combined = formantBandwidth[lower]
            + formantBandwidth[upper];
        if (combined <= maximumCombined)
            continue;

        const float scale = maximumCombined / combined;
        float narrowedLower = scale * formantBandwidth[lower];
        float narrowedUpper = scale * formantBandwidth[upper];
        if (narrowedLower < minimumBandwidth)
        {
            narrowedLower = minimumBandwidth;
            narrowedUpper = maximumCombined - minimumBandwidth;
        }
        else if (narrowedUpper < minimumBandwidth)
        {
            narrowedLower = maximumCombined - minimumBandwidth;
            narrowedUpper = minimumBandwidth;
        }
        formantBandwidth[lower] = narrowedLower;
        formantBandwidth[upper] = narrowedUpper;
    }
}

float wrapPhase(float phase) noexcept
{
    phase -= std::floor(phase);
    return phase;
}

std::uint32_t hash32(std::uint32_t value) noexcept
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value == 0u ? 1u : value;
}

float hashFloat(std::uint32_t value) noexcept
{
    return static_cast<float>(hash32(value) & 0x00ffffffu) / 8388607.5f - 1.0f;
}

/** One period of a Liljencrants-Fant style glottal flow derivative.

    @c openQuotient is the fraction of the period the glottis is open,
    @c speedQuotient the ratio of the opening to the closing phase, and
    @c returnQuotient the exponential return-phase time constant that follows
    glottal closure. The return phase is what sets the source spectral tilt, so
    the endpoint shapes differ in a physically meaningful way rather than
    through an arbitrary harmonic roll-off exponent.
*/
float glottalDerivative(float t, float openQuotient, float speedQuotient,
                        float returnQuotient) noexcept
{
    const float peak = openQuotient * speedQuotient / (1.0f + speedQuotient);
    const float close = openQuotient - peak;
    if (t <= peak)
        return 0.5f * (pi / peak) * std::sin(pi * t / peak);
    if (t <= openQuotient)
        return -0.5f * (pi / close) * std::sin(0.5f * pi * (t - peak) / close);
    return -0.5f * (pi / close) * std::exp(-(t - openQuotient) / returnQuotient);
}

/** The glottal flow whose derivative @c glottalDerivative is: the exact
    integral of the two sine segments above, zero at the start of the opening
    and back to zero at the instant of closure, and zero while the folds are
    closed.

    Aspiration turbulence is generated by the jet leaving the glottal
    constriction. With the transglottal pressure roughly constant over the open
    phase the jet velocity is roughly constant too, so the strength of the
    source scales with the area presented to it, which is the flow. That makes
    the flow the noise's envelope rather than a window drawn to look right.
*/
float glottalFlowShape(float t, float openQuotient, float speedQuotient) noexcept
{
    const float peak = openQuotient * speedQuotient / (1.0f + speedQuotient);
    const float close = openQuotient - peak;
    if (t <= peak)
        return 0.5f * (1.0f - std::cos(pi * t / peak));
    if (t <= openQuotient)
        return std::cos(0.5f * pi * (t - peak) / close);
    return 0.0f;
}
} // namespace

struct VoiceEngine::TableBank
{
    std::array<std::array<float, glottalShapeCount * tableSize>, tableLevels>
        glottalTables {};
    std::array<std::array<float, glottalGainTableSize>, tableLevels>
        glottalGainTable {};
    std::array<std::array<float, radiatedPowerHarmonics>, glottalGainTableSize>
        glottalHarmonicPower {};
    std::array<float, glottalShapeCount * flowTableSize> glottalFlowTable {};
    std::array<float, glottalShapeCount> flowMean {};
    std::array<float, glottalShapeCount - 1> flowCross {};
    std::array<float, tableSize> sineTable {};
};

VoiceEngine::VoiceEngine() noexcept
{
    setParameters(EngineParameters {});
    // Give the editor a meaningful tract to draw even if it opens before the
    // host has called prepare().
    blockParameters_ = snapshotParameters();
    updateChunkState(blockParameters_, false);
    publishDisplayState(0.0f, 0.0f, 1);
}

void VoiceEngine::prepare(double sampleRate, int maxBlockSize)
{
    if (!std::isfinite(sampleRate))
        sampleRate = 48000.0;
    sampleRate_ = std::clamp(sampleRate, 8000.0, 192000.0);
    displaySampleRate_.store(static_cast<float>(sampleRate_), std::memory_order_relaxed);
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    mipHarmonicGuardHz_ = 0.46f * static_cast<float>(sampleRate_);
    formantHzCeilingHz_ = 0.465f * static_cast<float>(sampleRate_);
    formantBandwidthCeilingHz_ = 0.25f * static_cast<float>(sampleRate_);
    maxBlockSize_ = std::max(1, maxBlockSize);

    const auto delayFor = [this](float seconds)
    {
        return std::clamp(seconds * static_cast<float>(sampleRate_),
                          8.0f, static_cast<float>(roomBufferSize) * 0.4f);
    };
    roomBaseDelay_ = { delayFor(0.0297f), delayFor(0.0371f),
                       delayFor(0.0411f), delayFor(0.0437f) };
    roomDelay_ = roomBaseDelay_;
    // A metre of air, in samples. Everything about the section's geometry is
    // stated in metres and seconds, so the room is the same room at every rate.
    placementMetresToSamples_ = static_cast<float>(sampleRate_) / speedOfSoundMetres;
    placementSpread_ = -1.0f;
    placementScale_ = -1.0f;
    chunkGainCoefficient_ = 1.0f - std::exp(-static_cast<float>(chunkSize)
                                            * inverseSampleRate_ / 0.020f);

    // Sample-rate-only coefficients. These must be valid before the first
    // process() call because noteOn() already runs a control update.
    parameterSmoothing_ = 1.0f - std::exp(-inverseSampleRate_ / 0.025f);
    airAttackCoefficient_ = 1.0f - std::exp(-inverseSampleRate_ / 0.004f);
    onsetAirMultiplier_ = std::exp(-inverseSampleRate_ / 0.085f);
    abductionCoefficient_ = 1.0f - std::exp(-static_cast<float>(controlPeriod)
                                            * inverseSampleRate_ / 0.050f);
    scoopMultiplier_ = std::exp(-static_cast<float>(controlPeriod) * inverseSampleRate_ / 0.072f);

    // Everything below is expressed as a corner frequency or a time constant so
    // the instrument sounds the same at every supported sample rate.
    const auto onePole = [this](float cornerHz) noexcept
    {
        return 1.0f - std::exp(-twoPi * cornerHz * inverseSampleRate_);
    };
    roomLowCutCoefficient_ = onePole(110.0f);
    roomDampingCoefficient_ = onePole(2510.0f);
    roomEnvelopeDecay_ = std::exp(-inverseSampleRate_ / 0.021f);
    shimmerCoefficient_ = onePole(46.0f);
    // The source-slope shelves. 850 Hz sits above the sung fundamental over
    // almost the whole range, which is what keeps a soft note from losing its
    // own first harmonic: the shelf has unity gain at DC by construction, so
    // only the partials above the corner follow the loudness.
    sourcePresenceCoefficient_ = onePole(850.0f);
    radiatedPowerUpdateControls_ = std::max(1, static_cast<int>(std::lround(
        static_cast<double>(radiatedPowerReferenceUpdateControls)
        * sampleRate_ / static_cast<double>(referenceSampleRate))));
    radiatedPowerCoefficient_ = 1.0f - std::exp(
        -static_cast<float>(radiatedPowerUpdateControls_ * controlPeriod)
        * inverseSampleRate_ / 0.040f);
    // ... and every noise-driven smoother among them needs its drive
    // renormalised, or the correct spectrum arrives at the wrong depth.
    shimmerScale_ = noiseSmootherScale(
        shimmerCoefficient_, 1.0f - std::exp(-twoPi * 46.0f / referenceSampleRate));
    aspirationPreEmphasis_ = std::exp(-twoPi * 1150.0f * inverseSampleRate_);
    // The nasal branch's three pole/zero radii. Each is exp(-pi * bandwidthHz
    // * inverseSampleRate_) for a bandwidth fixed in Hz, so -- like every
    // other coefficient in this block -- it only depends on the prepared
    // sample rate. updateChunkState() used to resolve all three again on
    // every chunk boundary that the nasal branch was active for; they are
    // resolved once here instead.
    nasalMurmurRadius_ = std::exp(-pi * 150.0f * inverseSampleRate_);
    nasalZeroRadius_ = std::exp(-pi * 60.0f * inverseSampleRate_);
    nasalNotchPoleRadius_ = std::exp(-pi * 700.0f * inverseSampleRate_);
    // White noise carries constant power per sample, so its density in the
    // audio band would otherwise fall by 3 dB per doubling of the sample rate.
    aspirationScale_ = std::sqrt(static_cast<float>(sampleRate_) / referenceSampleRate);
    controlGlide_ = 1.0f - std::exp(-static_cast<float>(controlPeriod)
                                    * inverseSampleRate_ / 0.0032f);
    const auto glideFor = [this](float seconds) noexcept {
        return 1.0f - std::exp(-static_cast<float>(controlPeriod) * inverseSampleRate_ / seconds);
    };
    // 16, 9, 5, 4 and 3 ms put a whole vowel change inside 50 ms, which is a
    // de-zipper rather than an articulation: a sung vowel-to-vowel transition
    // runs 100-200 ms. The lower formants follow the larger cavity adjustments
    // and stay the slowest, which is the ordering the constants above already
    // had. A move of a quarter of the formant's own nominal frequency or more
    // takes the full time; anything smaller is proportionally quicker.
    constexpr float slowSeconds[formantCount] { 0.068f, 0.050f, 0.032f, 0.025f, 0.020f };
    constexpr float fastSeconds[formantCount] { 0.020f, 0.015f, 0.011f, 0.009f, 0.008f };
    // Reciprocal of a quarter of each formant's nominal frequency, so the
    // control update scales the distance with a multiply instead of a divide.
    constexpr float nominalHz[formantCount] { 600.0f, 1500.0f, 2600.0f, 3400.0f, 4500.0f };
    for (int formant = 0; formant < formantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        formantGlideSlow_[index] = glideFor(slowSeconds[index]);
        formantGlideFast_[index] = glideFor(fastSeconds[index]);
        formantSpanScale_[index] = 4.0f / nominalHz[index];
    }
    jitterSlowCoefficient_ = 1.0f - std::exp(-static_cast<float>(controlPeriod)
                                             * inverseSampleRate_ / 0.0095f);
    const float controlSeconds = static_cast<float>(controlPeriod)
        * inverseSampleRate_;
    const auto ouCoefficients = [](float intervalSeconds,
                                   float timeConstantSeconds) noexcept
    {
        const float retention = std::exp(-intervalSeconds / timeConstantSeconds);
        const float innovation = std::sqrt(std::max(
            0.0f, 1.0f - retention * retention));
        return std::array<float, 2> { retention, innovation };
    };
    const auto pitchFast = ouCoefficients(controlSeconds, pitchDriftFastSeconds);
    pitchDriftFastRetention_ = pitchFast[0];
    pitchDriftFastInnovation_ = pitchFast[1];
    const auto pitchSlow = ouCoefficients(controlSeconds, pitchDriftSlowSeconds);
    pitchDriftSlowRetention_ = pitchSlow[0];
    pitchDriftSlowInnovation_ = pitchSlow[1];

    vowelDriftUpdateControls_ = std::max(1, static_cast<int>(std::lround(
        vowelDriftUpdateSeconds / controlSeconds)));
    const float vowelDriftInterval = static_cast<float>(
        vowelDriftUpdateControls_ * controlPeriod) * inverseSampleRate_;
    const auto vowelMorph = ouCoefficients(
        vowelDriftInterval, vowelDriftMorphSeconds);
    vowelDriftMorphRetention_ = vowelMorph[0];
    vowelDriftMorphInnovation_ = vowelMorph[1];
    const auto vowelX = ouCoefficients(vowelDriftInterval, vowelDriftXSeconds);
    vowelDriftXRetention_ = vowelX[0];
    vowelDriftXInnovation_ = vowelX[1];
    const auto vowelY = ouCoefficients(vowelDriftInterval, vowelDriftYSeconds);
    vowelDriftYRetention_ = vowelY[0];
    vowelDriftYInnovation_ = vowelY[1];
    // A singer hears the beating and moves onto the just interval; she does not
    // arrive on it. 90 ms is about how long that adjustment takes.
    justGlide_ = 1.0f - std::exp(-static_cast<float>(controlPeriod)
                                 * inverseSampleRate_ / 0.090f);

    tables_ = &sharedTableBank();
    buildSingerIdentities();
    prepared_ = true;
    reset();
}

void VoiceEngine::reset()
{
    allSoundOff();
    samplePosition_ = 0;
    generation_ = 0;
    heldCount_ = 0;
    heldNoteCounts_.fill(0);
    legatoPhrase_ = false;
    intonationRoot_ = -1;
    soundingRoot_ = -1;
    lastRootMidi_ = -1;
    blockParameters_ = snapshotParameters();
    smoothedRoom_ = blockParameters_.room;
    smoothedGain_ = blockParameters_.outputGain;
    smoothedBreath_ = blockParameters_.breath;
    smoothedTension_ = blockParameters_.tension;
    smoothedResonance_ = blockParameters_.resonance;
    smoothedFormantShift_ = blockParameters_.formantShift;
    smoothedNasal_ = clampUnit(blockParameters_.nasal);
    smoothedInstability_ = clampUnit(blockParameters_.instability);
    smoothedRoomSize_ = clampUnit(blockParameters_.roomSize);
    // A host calls prepareToPlay() when the transport or the sample rate moves,
    // and a controller does not resend its position, so the performance state
    // returns to the neutral one the host parameters describe.
    pitchBendSemitones_ = 0.0f;
    modWheel_ = 1.0f;
    modWheelMoved_ = false;
    expression_ = 1.0f;
    sustainPedal_ = false;
    sustainedNotes_.fill(0);
    smoothedDynamics_ = effectiveDynamics(blockParameters_);
    smoothedExpression_ = expression_;
    chunkResponse_ = dynamicResponse(smoothedDynamics_);
    voicedDynamic_ = chunkResponse_.voicedGain;
    airDynamic_ = chunkResponse_.airGain;
    meterLeft_ = meterRight_ = 0.0f;
    chunkStateValid_ = false;
    // Every cached coefficient below depends on the sample rate, and reset()
    // is what prepare() calls after changing it.
    tractInputs_.fill(0.0f);
    jitterHumanize_ = -1.0f;
    glideAmount_ = -1.0f;

    // The ensemble drift is derived from the absolute sample position, so the
    // next chunk update restores a repeatable but non-aligned singer state.
    singersInUse_ = ~0u;

    updateChunkState(blockParameters_, false);
    // allSoundOff() above has already silenced every voice, so this resolves
    // to zero regardless of how stale activeTotal_/activeVoices_ are here.
    publishDisplayState(0.0f, 0.0f, 1);
}

void VoiceEngine::setParameters(const EngineParameters& p)
{
    const int profile = p.profile == VoiceProfile::Male ? 1 : 0;
    const int mode = p.mode == PerformanceMode::Choir ? 1 : (p.mode == PerformanceMode::Chord ? 2 : 0);
    const int vowel = vowelIndexOf(p.vowel);
    const int quality = p.chordQuality == ChordQuality::Minor ? 1 : 0;
    // The engine holds twelve distinct singer identities, so an ensemble larger
    // than that could only be built from duplicates of the ones it already has.
    const int choir = std::clamp(p.choirSize, 2, singerCount);

    atomicParameters_.profile.store(profile, std::memory_order_relaxed);
    atomicParameters_.mode.store(mode, std::memory_order_relaxed);
    atomicParameters_.vowel.store(vowel, std::memory_order_relaxed);
    atomicParameters_.chordQuality.store(quality, std::memory_order_relaxed);
    atomicParameters_.choirSize.store(choir, std::memory_order_relaxed);
    atomicParameters_.breath.store(clampUnit(p.breath), std::memory_order_relaxed);
    atomicParameters_.resonance.store(clampUnit(p.resonance), std::memory_order_relaxed);
    atomicParameters_.vibrato.store(clampUnit(p.vibrato), std::memory_order_relaxed);
    atomicParameters_.humanize.store(clampUnit(p.humanize), std::memory_order_relaxed);
    atomicParameters_.spread.store(clampUnit(p.spread), std::memory_order_relaxed);
    atomicParameters_.tension.store(clampUnit(p.tension), std::memory_order_relaxed);
    atomicParameters_.room.store(clampUnit(p.room), std::memory_order_relaxed);
    const float gain = std::isfinite(p.outputGain) ? std::clamp(p.outputGain, 0.0f, 2.0f) : 0.8f;
    atomicParameters_.outputGain.store(gain, std::memory_order_relaxed);
    atomicParameters_.vowelX.store(clampUnit(p.vowelX), std::memory_order_relaxed);
    atomicParameters_.vowelY.store(clampUnit(p.vowelY), std::memory_order_relaxed);
    atomicParameters_.vowelMorph.store(clampUnit(p.vowelMorph), std::memory_order_relaxed);
    const float shift = std::isfinite(p.formantShift) ? std::clamp(p.formantShift, -12.0f, 12.0f) : 0.0f;
    atomicParameters_.formantShift.store(shift, std::memory_order_relaxed);
    atomicParameters_.glide.store(clampUnit(p.glide), std::memory_order_relaxed);
    atomicParameters_.legato.store(p.legato ? 1 : 0, std::memory_order_relaxed);
    atomicParameters_.roomSize.store(clampUnit(p.roomSize), std::memory_order_relaxed);
    atomicParameters_.dynamics.store(clampUnit(p.dynamics), std::memory_order_relaxed);
    atomicParameters_.intonation.store(clampUnit(p.intonation), std::memory_order_relaxed);
    atomicParameters_.nasal.store(clampUnit(p.nasal), std::memory_order_relaxed);
    atomicParameters_.instability.store(clampUnit(p.instability), std::memory_order_relaxed);
    atomicParameters_.legacyRadiatedPowerBypass.store(
        p.legacyRadiatedPowerBypass ? 1 : 0, std::memory_order_relaxed);
    atomicParameters_.legacyDriftBypass.store(
        p.legacyDriftBypass ? 1 : 0, std::memory_order_relaxed);
}

void VoiceEngine::setPitchBend(float semitones) noexcept
{
    pitchBendSemitones_ = std::isfinite(semitones)
        ? std::clamp(semitones, -48.0f, 48.0f) : 0.0f;
}

void VoiceEngine::setModWheel(float value) noexcept
{
    modWheel_ = clampUnit(value);
    modWheelMoved_ = true;
}

void VoiceEngine::setExpression(float value) noexcept
{
    expression_ = clampUnit(value);
}

void VoiceEngine::resetControllers() noexcept
{
    pitchBendSemitones_ = 0.0f;
    modWheel_ = 1.0f;
    modWheelMoved_ = false;
    expression_ = 1.0f;
}

void VoiceEngine::setSustainPedal(bool down)
{
    if (down == sustainPedal_)
        return;
    sustainPedal_ = down;
    if (down)
        return;

    // Pedal up. Every deferred note-off is delivered through the ordinary
    // path, so the held-note stack, the legato fallback and the release all
    // behave exactly as they would have without the pedal.
    for (int pitch = 0; pitch < 128; ++pitch)
    {
        auto& pending = sustainedNotes_[static_cast<std::size_t>(pitch)];
        while (pending > 0)
        {
            --pending;
            noteOff(pitch);
        }
    }
}

float VoiceEngine::effectiveDynamics(const EngineParameters& p) const noexcept
{
    return modWheelMoved_ ? modWheel_ : clampUnit(p.dynamics);
}

EngineParameters VoiceEngine::snapshotParameters() const noexcept
{
    EngineParameters p;
    p.profile = atomicParameters_.profile.load(std::memory_order_relaxed) == 1 ? VoiceProfile::Male : VoiceProfile::Female;
    const int mode = atomicParameters_.mode.load(std::memory_order_relaxed);
    p.mode = mode == 1 ? PerformanceMode::Choir : (mode == 2 ? PerformanceMode::Chord : PerformanceMode::Solo);
    const int vowel = atomicParameters_.vowel.load(std::memory_order_relaxed);
    p.vowel = vowel == 1 ? Vowel::Ooh : (vowel == 2 ? Vowel::Uuh : Vowel::Aah);
    p.chordQuality = atomicParameters_.chordQuality.load(std::memory_order_relaxed) == 1 ? ChordQuality::Minor : ChordQuality::Major;
    p.choirSize = atomicParameters_.choirSize.load(std::memory_order_relaxed);
    p.breath = atomicParameters_.breath.load(std::memory_order_relaxed);
    p.resonance = atomicParameters_.resonance.load(std::memory_order_relaxed);
    p.vibrato = atomicParameters_.vibrato.load(std::memory_order_relaxed);
    p.humanize = atomicParameters_.humanize.load(std::memory_order_relaxed);
    p.spread = atomicParameters_.spread.load(std::memory_order_relaxed);
    p.tension = atomicParameters_.tension.load(std::memory_order_relaxed);
    p.room = atomicParameters_.room.load(std::memory_order_relaxed);
    p.outputGain = atomicParameters_.outputGain.load(std::memory_order_relaxed);
    p.vowelX = atomicParameters_.vowelX.load(std::memory_order_relaxed);
    p.vowelY = atomicParameters_.vowelY.load(std::memory_order_relaxed);
    p.vowelMorph = atomicParameters_.vowelMorph.load(std::memory_order_relaxed);
    p.formantShift = atomicParameters_.formantShift.load(std::memory_order_relaxed);
    p.glide = atomicParameters_.glide.load(std::memory_order_relaxed);
    p.legato = atomicParameters_.legato.load(std::memory_order_relaxed) != 0;
    p.roomSize = atomicParameters_.roomSize.load(std::memory_order_relaxed);
    p.dynamics = atomicParameters_.dynamics.load(std::memory_order_relaxed);
    p.intonation = atomicParameters_.intonation.load(std::memory_order_relaxed);
    p.nasal = atomicParameters_.nasal.load(std::memory_order_relaxed);
    p.instability = atomicParameters_.instability.load(std::memory_order_relaxed);
    p.legacyRadiatedPowerBypass =
        atomicParameters_.legacyRadiatedPowerBypass.load(
            std::memory_order_relaxed) != 0;
    p.legacyDriftBypass = atomicParameters_.legacyDriftBypass.load(
        std::memory_order_relaxed) != 0;
    return p;
}

const VoiceEngine::TableBank& VoiceEngine::sharedTableBank()
{
    // Function-local static initialisation is thread-safe. Keep the comparatively
    // large bank on the heap so stack-allocated VoiceEngine instances stay small,
    // and build it only once because none of these samples depends on sample rate
    // or on a patch parameter.
    static const auto shared = []
    {
    auto tables = std::make_unique<TableBank>();
    auto& glottalTables_ = tables->glottalTables;
    auto& glottalGainTable_ = tables->glottalGainTable;
    auto& glottalHarmonicPower_ = tables->glottalHarmonicPower;
    auto& glottalFlowTable_ = tables->glottalFlowTable;
    auto& flowMean_ = tables->flowMean;
    auto& flowCross_ = tables->flowCross;
    auto& sineTable_ = tables->sineTable;

    for (int i = 0; i < tableSize; ++i)
        sineTable_[static_cast<std::size_t>(i)] = std::sin(twoPi * static_cast<float>(i) / static_cast<float>(tableSize));

    // Sine and cosine at an exact harmonic multiple of the table step. Every
    // harmonic of a 2048-point table lands on a table entry, so the analysis
    // and synthesis below need no library trigonometry at all.
    const auto tableSin = [&sineTable_](int index) noexcept
    {
        return sineTable_[static_cast<std::size_t>(index & tableMask)];
    };
    const auto tableCos = [&sineTable_](int index) noexcept
    {
        return sineTable_[static_cast<std::size_t>((index + tableSize / 4) & tableMask)];
    };

    struct Shape
    {
        float openQuotient;
        float speedQuotient;
        float returnQuotient;
    };
    // Lax/breathy versus firmly adducted phonation. Both endpoints sit inside
    // the ranges reported for sustained singing (OQ 0.4-0.8, SQ 1.5-4).
    // Tension interpolates the physical LF parameters before analysis. Mixing
    // only the two endpoint waveforms instead would sum closures at different
    // phases and dig deep, non-physiological notches in individual harmonics.
    constexpr Shape endpoints[2] = { { 0.78f, 2.60f, 0.0120f },
                                     { 0.46f, 3.40f, 0.0032f } };
    // Oscillator phase denotes the glottal closure/MFD event, not an arbitrary
    // point in a pulse whose open quotient changes with Tension. Rotating every
    // analysed shape to one closure phase lets adjacent physical shapes
    // interpolate without cancelling harmonics merely because they close at
    // different instants.
    // Keep the lax endpoint at its historical phase. Onsets begin toward this
    // shape through tensionSag, so retaining its 0.78 closure also avoids
    // rephasing the first pulse of every existing patch.
    constexpr float commonClosurePhase = 0.78f;
    std::array<Shape, glottalShapeCount> shapes {};
    for (int shapeIndex = 0; shapeIndex < glottalShapeCount; ++shapeIndex)
    {
        const float amount = static_cast<float>(shapeIndex)
            / static_cast<float>(glottalShapeCount - 1);
        shapes[static_cast<std::size_t>(shapeIndex)] = {
            endpoints[0].openQuotient
                + amount * (endpoints[1].openQuotient - endpoints[0].openQuotient),
            endpoints[0].speedQuotient
                + amount * (endpoints[1].speedQuotient - endpoints[0].speedQuotient),
            endpoints[0].returnQuotient
                + amount * (endpoints[1].returnQuotient - endpoints[0].returnQuotient)
        };
    }
    // Preserve the physical endpoint parameters exactly: evaluating the
    // arithmetic interpolation at one can round a literal endpoint by an ulp
    // even though it describes the same real number.
    shapes.front() = endpoints[0];
    shapes.back() = endpoints[1];

    std::array<std::array<float, maxHarmonics + 1>, glottalShapeCount>
        cosineCoefficients {};
    std::array<std::array<float, maxHarmonics + 1>, glottalShapeCount>
        sineCoefficients {};
    // The former oscillator crossfaded these two normalised endpoint spectra
    // before their closure phases were aligned. Preserve that crossfade's
    // 1..24-harmonic RMS curve as a loudness contract, while the tables below
    // interpolate the closure-aligned physical shapes instead.
    std::array<std::array<float, maxHarmonics + 1>, 2>
        legacyEndpointCosine {};
    std::array<std::array<float, maxHarmonics + 1>, 2>
        legacyEndpointSine {};
    std::array<float, tableSize> prototype {};

    for (int shapeIndex = 0; shapeIndex < glottalShapeCount; ++shapeIndex)
    {
        const float amount = static_cast<float>(shapeIndex)
            / static_cast<float>(glottalShapeCount - 1);
        const auto& shape = shapes[static_cast<std::size_t>(shapeIndex)];

        double mean = 0.0;
        for (int i = 0; i < tableSize; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(tableSize);
            const float value = glottalDerivative(t, shape.openQuotient,
                                                  shape.speedQuotient,
                                                  shape.returnQuotient);
            prototype[static_cast<std::size_t>(i)] = value;
            mean += static_cast<double>(value);
        }
        const float offset = static_cast<float>(mean / static_cast<double>(tableSize));
        for (auto& value : prototype)
            value -= offset;

        const float normalise = 2.0f / static_cast<float>(tableSize);
        for (int harmonic = 1; harmonic <= maxHarmonics; ++harmonic)
        {
            float real = 0.0f;
            float imaginary = 0.0f;
            int index = 0;
            for (int i = 0; i < tableSize; ++i)
            {
                const float value = prototype[static_cast<std::size_t>(i)];
                real += value * tableCos(index);
                imaginary += value * tableSin(index);
                index += harmonic;
            }
            cosineCoefficients[static_cast<std::size_t>(shapeIndex)][static_cast<std::size_t>(harmonic)] =
                real * normalise;
            sineCoefficients[static_cast<std::size_t>(shapeIndex)][static_cast<std::size_t>(harmonic)] =
                imaginary * normalise;
        }

        float energy = 0.0f;
        for (int harmonic = 1; harmonic <= sourceBandHarmonics; ++harmonic)
        {
            const float a = cosineCoefficients[static_cast<std::size_t>(shapeIndex)][static_cast<std::size_t>(harmonic)];
            const float b = sineCoefficients[static_cast<std::size_t>(shapeIndex)][static_cast<std::size_t>(harmonic)];
            energy += a * a + b * b;
        }
        energy = std::sqrt(0.5f * energy);
        const float targetRms = sourceBandRms[0]
            + amount * (sourceBandRms[1] - sourceBandRms[0]);
        const float scale = energy > 0.0f
            ? targetRms / energy : 1.0f;
        for (int harmonic = 1; harmonic <= maxHarmonics; ++harmonic)
        {
            cosineCoefficients[static_cast<std::size_t>(shapeIndex)][static_cast<std::size_t>(harmonic)] *= scale;
            sineCoefficients[static_cast<std::size_t>(shapeIndex)][static_cast<std::size_t>(harmonic)] *= scale;
        }

        const int endpoint = shapeIndex == 0 ? 0
            : (shapeIndex == glottalShapeCount - 1 ? 1 : -1);
        if (endpoint >= 0)
        {
            for (int harmonic = 1; harmonic <= maxHarmonics; ++harmonic)
            {
                const auto index = static_cast<std::size_t>(harmonic);
                legacyEndpointCosine[static_cast<std::size_t>(endpoint)][index]
                    = cosineCoefficients[static_cast<std::size_t>(shapeIndex)][index];
                legacyEndpointSine[static_cast<std::size_t>(endpoint)][index]
                    = sineCoefficients[static_cast<std::size_t>(shapeIndex)][index];
            }
        }

        // A circular time shift preserves every harmonic magnitude and the
        // endpoint RMS exactly. Do it on the analysed coefficients rather than
        // resampling the discontinuous derivative waveform.
        const float shift = commonClosurePhase - shape.openQuotient;
        for (int harmonic = 1; harmonic <= maxHarmonics; ++harmonic)
        {
            const auto index = static_cast<std::size_t>(harmonic);
            const float angle = twoPi * static_cast<float>(harmonic) * shift;
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);
            const float a = cosineCoefficients[static_cast<std::size_t>(shapeIndex)][index];
            const float b = sineCoefficients[static_cast<std::size_t>(shapeIndex)][index];
            cosineCoefficients[static_cast<std::size_t>(shapeIndex)][index]
                = a * cosine - b * sine;
            sineCoefficients[static_cast<std::size_t>(shapeIndex)][index]
                = a * sine + b * cosine;
        }
    }

    // The radiation-efficiency follower needs only the relative power of the
    // first eight source harmonics. Resolve the exact closure-aligned shape
    // interpolation once here. A mip's compatibility gain is common to every
    // one of these harmonics and therefore cancels in Paverage/Pcurrent.
    for (int gainIndex = 0; gainIndex < glottalGainTableSize; ++gainIndex)
    {
        const float tension = static_cast<float>(gainIndex)
            / static_cast<float>(glottalGainTableSize - 1);
        const float shapePosition = tension
            * static_cast<float>(glottalShapeCount - 1);
        const int lowerShape = std::min(static_cast<int>(shapePosition),
                                        glottalShapeCount - 2);
        const float shapeFraction = shapePosition
            - static_cast<float>(lowerShape);
        for (int harmonic = 1; harmonic <= radiatedPowerHarmonics; ++harmonic)
        {
            const auto index = static_cast<std::size_t>(harmonic);
            const float lowerCosine = cosineCoefficients[
                static_cast<std::size_t>(lowerShape)][index];
            const float lowerSine = sineCoefficients[
                static_cast<std::size_t>(lowerShape)][index];
            const float cosine = lowerCosine + shapeFraction
                * (cosineCoefficients[
                       static_cast<std::size_t>(lowerShape + 1)][index]
                   - lowerCosine);
            const float sine = lowerSine + shapeFraction
                * (sineCoefficients[
                       static_cast<std::size_t>(lowerShape + 1)][index]
                   - lowerSine);
            glottalHarmonicPower_[static_cast<std::size_t>(gainIndex)]
                                  [static_cast<std::size_t>(harmonic - 1)]
                = cosine * cosine + sine * sine;
        }
    }

    // Linear interpolation between two nearby physical shapes loses a small
    // amount of energy because their harmonic phases are not identical. Resolve
    // the correction to the legacy endpoint-crossfade RMS curve ahead of time.
    // It belongs to each mip level: a one-harmonic high note and a 256-harmonic
    // bass do not lose the same energy when the shapes move. Nine 257-entry
    // lookups keep the remaining error negligible while the audio thread still
    // pays only two loads, a lerp and a multiply instead of a square root.
    for (int level = 0; level < tableLevels; ++level)
    {
        const int harmonics = harmonicsPerLevel[static_cast<std::size_t>(level)];
        auto& gains = glottalGainTable_[static_cast<std::size_t>(level)];
        for (int gainIndex = 0; gainIndex < glottalGainTableSize; ++gainIndex)
        {
            const float tension = static_cast<float>(gainIndex)
                / static_cast<float>(glottalGainTableSize - 1);
            const float shapePosition = tension
                * static_cast<float>(glottalShapeCount - 1);
            const int lowerShape = std::min(static_cast<int>(shapePosition),
                                            glottalShapeCount - 2);
            const float shapeFraction = shapePosition
                - static_cast<float>(lowerShape);
            double appliedEnergy = 0.0;
            double legacyEnergy = 0.0;
            for (int harmonic = 1; harmonic <= harmonics; ++harmonic)
            {
                const auto index = static_cast<std::size_t>(harmonic);
                const float lowerCosine = cosineCoefficients[
                    static_cast<std::size_t>(lowerShape)][index];
                const float lowerSine = sineCoefficients[
                    static_cast<std::size_t>(lowerShape)][index];
                const float cosine = lowerCosine + shapeFraction
                    * (cosineCoefficients[
                           static_cast<std::size_t>(lowerShape + 1)][index]
                       - lowerCosine);
                const float sine = lowerSine + shapeFraction
                    * (sineCoefficients[
                           static_cast<std::size_t>(lowerShape + 1)][index]
                       - lowerSine);
                appliedEnergy += static_cast<double>(cosine) * cosine
                               + static_cast<double>(sine) * sine;

                const float legacyCosine = legacyEndpointCosine[0][index]
                    + tension * (legacyEndpointCosine[1][index]
                               - legacyEndpointCosine[0][index]);
                const float legacySine = legacyEndpointSine[0][index]
                    + tension * (legacyEndpointSine[1][index]
                               - legacyEndpointSine[0][index]);
                legacyEnergy += static_cast<double>(legacyCosine) * legacyCosine
                              + static_cast<double>(legacySine) * legacySine;
            }
            const float appliedRms = static_cast<float>(
                std::sqrt(0.5 * appliedEnergy));
            const float targetRms = static_cast<float>(
                std::sqrt(0.5 * legacyEnergy));
            gains[static_cast<std::size_t>(gainIndex)] = appliedRms > 0.0f
                ? targetRms / appliedRms : 1.0f;
        }
        gains.front() = 1.0f;
        gains.back() = 1.0f;
    }

    for (int level = 0; level < tableLevels; ++level)
    {
        auto& table = glottalTables_[static_cast<std::size_t>(level)];
        table.fill(0.0f);
        const int harmonics = harmonicsPerLevel[static_cast<std::size_t>(level)];
        for (int shape = 0; shape < glottalShapeCount; ++shape)
        {
            for (int harmonic = 1; harmonic <= harmonics; ++harmonic)
            {
                const float a = cosineCoefficients[static_cast<std::size_t>(shape)][static_cast<std::size_t>(harmonic)];
                const float b = sineCoefficients[static_cast<std::size_t>(shape)][static_cast<std::size_t>(harmonic)];
                int index = 0;
                for (int i = 0; i < tableSize; ++i)
                {
                    table[static_cast<std::size_t>(shape * tableSize + i)]
                        += a * tableCos(index) + b * tableSin(index);
                    index += harmonic;
                }
            }
        }
    }

    // The aspiration envelope uses the same physical shape bank. It multiplies
    // white noise, so it is stored once rather than band limited per level: the
    // product is broadband either way and nothing about it folds back into the
    // harmonic series. Each shape is normalised to unit mean square over the
    // period, making the window a redistribution of noise in time rather than a
    // change in how much noise there is.
    for (int shape = 0; shape < glottalShapeCount; ++shape)
    {
        const auto& parameters = shapes[static_cast<std::size_t>(shape)];
        double meanSquare = 0.0;
        for (int i = 0; i < flowTableSize; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(flowTableSize);
            const float physicalPhase = wrapPhase(
                t - (commonClosurePhase - parameters.openQuotient));
            const float value = glottalFlowShape(physicalPhase,
                                                 parameters.openQuotient,
                                                 parameters.speedQuotient);
            glottalFlowTable_[static_cast<std::size_t>(
                i * glottalShapeCount + shape)] = value;
            meanSquare += static_cast<double>(value) * static_cast<double>(value);
        }
        meanSquare /= static_cast<double>(flowTableSize);
        const float normalise = meanSquare > 0.0
            ? static_cast<float>(1.0 / std::sqrt(meanSquare)) : 1.0f;
        double mean = 0.0;
        for (int i = 0; i < flowTableSize; ++i)
        {
            const auto index = static_cast<std::size_t>(
                i * glottalShapeCount + shape);
            glottalFlowTable_[index] *= normalise;
            mean += static_cast<double>(glottalFlowTable_[index]);
        }
        flowMean_[static_cast<std::size_t>(shape)] =
            static_cast<float>(mean / static_cast<double>(flowTableSize));
    }

    // Adjacent shapes are each unit mean square, but their interpolation is not:
    // their peaks still differ slightly in time. Store each pair's overlap so
    // aspirationWindowGain() can restore the exact energy at control rate.
    for (int shape = 0; shape + 1 < glottalShapeCount; ++shape)
    {
        double cross = 0.0;
        for (int i = 0; i < flowTableSize; ++i)
        {
            const auto lower = static_cast<std::size_t>(
                i * glottalShapeCount + shape);
            cross += static_cast<double>(glottalFlowTable_[lower])
                   * static_cast<double>(glottalFlowTable_[lower + 1]);
        }
        flowCross_[static_cast<std::size_t>(shape)] =
            static_cast<float>(cross / static_cast<double>(flowTableSize));
    }

    return tables;
    }();
    return *shared;
}

float VoiceEngine::aspirationWindowGain(float tension, float depth) const noexcept
{
    // Mean square of 1 + depth * (window - 1), in closed form. The window is a
    // linear interpolation between adjacent unit-mean-square physical shapes,
    // so its mean is linear and its mean square is a quadratic whose only
    // unknown is the precomputed overlap of that pair.
    //
    // tension arrives pre-clamped to [0, 1]: the sole call site, in
    // updateVoiceControl(), already runs windowTension through std::clamp
    // before passing it in here, so re-clamping it a second time was pure
    // wasted work on every voice's control update.
    const float shapePosition = tension * static_cast<float>(glottalShapeCount - 1);
    const int lowerShape = std::min(static_cast<int>(shapePosition),
                                    glottalShapeCount - 2);
    const float fraction = shapePosition - static_cast<float>(lowerShape);
    const auto lower = static_cast<std::size_t>(lowerShape);
    const auto& tables = *tables_;
    const float mean = tables.flowMean[lower]
        + fraction * (tables.flowMean[lower + 1] - tables.flowMean[lower]);
    const float square = 1.0f - 2.0f * fraction * (1.0f - fraction)
        * (1.0f - tables.flowCross[lower]);
    const float rest = 1.0f - depth;
    const float applied = rest * rest + 2.0f * depth * rest * mean + depth * depth * square;
    return applied > 1.0e-6f ? 1.0f / std::sqrt(applied) : 1.0f;
}

void VoiceEngine::buildSingerIdentities()
{
    for (int i = 0; i < singerCount; ++i)
    {
        auto& singer = singers_[static_cast<std::size_t>(i)];
        const std::uint32_t base = 0x9e3779b9u * static_cast<std::uint32_t>(i + 1);
        // Jers and Ternstrom measured 25-30 cents of dispersion between choir
        // singers, and listeners tolerate about 14 cents of standard deviation.
        // A uniform +/-5.6 cents was roughly a quarter of that, which is why
        // twelve singers read as one thick voice rather than as a section. Two
        // hashes averaged give a triangular spread, which concentrates the
        // section near the target the way a real one does rather than spacing
        // it evenly across the extremes.
        singer.detuneCents = 12.0f * (hashFloat(base + 1u) + hashFloat(base + 11u));
        singer.anatomy = 0.045f * hashFloat(base + 2u);
        const float position = singerCount > 1 ? 2.0f * static_cast<float>(i) / static_cast<float>(singerCount - 1) - 1.0f : 0.0f;
        singer.pan = std::clamp(0.82f * position + 0.12f * hashFloat(base + 3u), -1.0f, 1.0f);
        // Habitual asynchrony, seeded across the entry window rather than drawn
        // from a hash: the section's dispersion is a stated quantity, and
        // twelve independent draws deliver it only on average. The stride
        // scrambles the order so the section does not enter left to right --
        // the singer index also sets the pan -- and being coprime with
        // singerCount it spreads a four-singer choir across the window too.
        // The window is inset by the per-note jitter at both ends so no
        // singer's draw is clipped against zero, which would have left the
        // earliest entry identical on repeats.
        const int entrySlot = (entryOrderStride * i) % singerCount;
        const int releaseSlot = (releaseOrderStride * i) % singerCount;
        const float grid = singerCount > 1
            ? static_cast<float>(entrySlot) / static_cast<float>(singerCount - 1) : 0.0f;
        const float releaseGrid = singerCount > 1
            ? static_cast<float>(releaseSlot) / static_cast<float>(singerCount - 1) : 0.0f;
        singer.entryOffset = entryJitterSeconds
            + (entryWindowSeconds - 2.0f * entryJitterSeconds) * grid;
        singer.releaseOffset = releaseJitterSeconds
            + (releaseWindowSeconds - 2.0f * releaseJitterSeconds) * releaseGrid;
        singer.releaseTendency = releaseTendencySpread * hashFloat(base + 13u);
        // Sundberg's definition and the 2022 systematic review both put a sung
        // vibrato at 5-7 Hz. The 4.65-5.37 Hz the identities used to be seeded
        // across is below that band at every point of it, which is a tremble
        // rather than a vibrato; the section is now spread over the band
        // itself.
        singer.vibratoRate = 5.6f + 1.4f * (0.5f + 0.5f * hashFloat(base + 5u));
        singer.vibratoDepth = 0.76f + 0.42f * (0.5f + 0.5f * hashFloat(base + 6u));
        // Airflow/intensity modulation leads the F0 modulation by a singer-
        // dependent amount instead of behaving like a tremolo wired to the
        // same LFO sample. 0.125..0.417 cycle is 45..150 degrees.
        singer.vibratoAmplitudePhase = 0.125f
            + 0.292f * (0.5f + 0.5f * hashFloat(base + 10u));
        singer.driftIncrement = (0.025f + 0.075f * (0.5f + 0.5f * hashFloat(base + 7u))) * inverseSampleRate_;
        singer.drift2Increment = (0.011f + 0.033f * (0.5f + 0.5f * hashFloat(base + 12u))) * inverseSampleRate_;
        singer.depthIncrement = (0.018f + 0.042f * (0.5f + 0.5f * hashFloat(base + 8u))) * inverseSampleRate_;
        singer.formantIncrement = (0.009f + 0.025f * (0.5f + 0.5f * hashFloat(base + 9u))) * inverseSampleRate_;
        for (int formant = 0; formant < formantCount; ++formant)
        {
            // Higher formants disperse more: they are the ones that follow the
            // individual shape of the pharynx and mouth rather than length.
            const float depth = 0.014f + 0.006f * static_cast<float>(formant);
            singer.formantScale[static_cast<std::size_t>(formant)] =
                depth * hashFloat(base + 16u + static_cast<std::uint32_t>(formant));
        }
    }

    // Depth. The draw picks the order and the range fixes the spread, the same
    // way step 7 seeded the entry offsets: twelve independent draws over
    // 1.5-6 m realise only 2.6-4.2 m of the 4.5 m the geometry claims, and the
    // whole point of the range is the 13.1 ms of arrival difference across it.
    // Identity 0 takes the front: Solo sings on it, and the applied delays are
    // referred to the nearest singer, so a soloist keeps her zero latency.
    std::array<float, singerCount> draw {};
    for (int i = 0; i < singerCount; ++i)
        draw[static_cast<std::size_t>(i)] =
            hashFloat(0x9e3779b9u * static_cast<std::uint32_t>(i + 1) + 4u);

    const float span = farSingerMetres - nearSingerMetres - 2.0f * singerDepthJitterMetres;
    for (int i = 0; i < singerCount; ++i)
    {
        int slot = 0;
        if (i > 0)
        {
            slot = 1;
            for (int j = 1; j < singerCount; ++j)
                if (j != i && draw[static_cast<std::size_t>(j)] < draw[static_cast<std::size_t>(i)])
                    ++slot;
        }
        auto& singer = singers_[static_cast<std::size_t>(i)];
        const std::uint32_t base = 0x9e3779b9u * static_cast<std::uint32_t>(i + 1);
        const float grid = singerCount > 1
            ? static_cast<float>(slot) / static_cast<float>(singerCount - 1) : 0.0f;
        // The grid is inset by the jitter at both ends, so the ranking still
        // orders the section in depth and identity 0 is still the nearest.
        singer.distanceMetres = nearSingerMetres + singerDepthJitterMetres + span * grid
                              + singerDepthJitterMetres * hashFloat(base + 20u);
    }

    // Referred to the nearest singer, less half the receiver spacing so the
    // near ear of the nearest singer is still a positive delay.
    placementReferenceMetres_ = singers_[0].distanceMetres - 0.5f * receiverSpacingMetres;
    distanceNormalisation_[0] = 1.0f;
    double inverseSquares = 0.0;
    for (int n = 1; n <= singerCount; ++n)
    {
        const double distance = static_cast<double>(singers_[static_cast<std::size_t>(n - 1)].distanceMetres);
        inverseSquares += 1.0 / (distance * distance);
        distanceNormalisation_[static_cast<std::size_t>(n)] =
            static_cast<float>(std::sqrt(static_cast<double>(n) / inverseSquares));
    }
}

int VoiceEngine::voicesForMode(const EngineParameters& p) const noexcept
{
    if (p.mode == PerformanceMode::Chord)
        return 6;
    if (p.mode == PerformanceMode::Choir)
        return std::clamp(p.choirSize, 2, singerCount);
    return 1;
}

int VoiceEngine::chordMidiForSinger(int root, int singer, const EngineParameters& p) const noexcept
{
    const int third = p.chordQuality == ChordQuality::Minor ? 3 : 4;
    std::array<int, 6> intervals {};
    if (root < 48)
        intervals = { 0, 7, 12, 12 + third, 19, 24 };
    else if (root > 72)
        intervals = { -24, -12, third - 12, -5, 0, third };
    else
        intervals = { -12, 0, third, 7, 12, 12 + third };

    int note = root + intervals[static_cast<std::size_t>(std::clamp(singer, 0, 5))];
    const int low = p.profile == VoiceProfile::Male ? 35 : 47;
    const int high = p.profile == VoiceProfile::Male ? 79 : 91;
    while (note < low)
        note += 12;
    while (note > high)
        note -= 12;
    return std::clamp(note, 0, 127);
}

int VoiceEngine::findFreeVoice() const noexcept
{
    for (int i = 0; i < maxVoices; ++i)
        if (!voices_[static_cast<std::size_t>(i)].active)
            return i;
    return -1;
}

int VoiceEngine::countActiveVoices() const noexcept
{
    int count = 0;
    for (const auto& voice : voices_)
        count += voice.active ? 1 : 0;
    return count;
}

void VoiceEngine::updateIntonationRoot() noexcept
{
    // The bass is what a section tunes to, so the reference is the lowest root
    // still being held. A releasing voice is on its way out and does not get a
    // vote, otherwise a chord would keep re-tuning to notes already let go of.
    int lowest = -1;
    for (const auto& voice : voices_)
        if (voice.active && !voice.releasing && (lowest < 0 || voice.rootMidi < lowest))
            lowest = voice.rootMidi;
    intonationRoot_ = lowest;
}

int VoiceEngine::mipTableLevelForFrequency(float frequencyHz) const noexcept
{
    // Highest level whose per-cycle harmonic count still keeps every
    // harmonic under mipHarmonicGuardHz_ for this fundamental; level 0 (one
    // harmonic) is always safe, so the search only ever raises it.
    const int permissible = std::max(
        1, static_cast<int>(mipHarmonicGuardHz_ / std::max(frequencyHz, 1.0f)));
    int level = 0;
    for (int candidate = 1; candidate < tableLevels; ++candidate)
        if (harmonicsPerLevel[static_cast<std::size_t>(candidate)] <= permissible)
            level = candidate;
    return level;
}

void VoiceEngine::makeRoomFor(int required)
{
    int free = 0;
    for (const auto& voice : voices_)
        free += voice.active ? 0 : 1;

    while (free < required)
    {
        int candidate = -1;
        std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
        for (int i = 0; i < maxVoices; ++i)
        {
            const auto& voice = voices_[static_cast<std::size_t>(i)];
            if (voice.active && voice.releasing && voice.generation < oldest)
            {
                oldest = voice.generation;
                candidate = i;
            }
        }
        if (candidate < 0)
        {
            for (int i = 0; i < maxVoices; ++i)
            {
                const auto& voice = voices_[static_cast<std::size_t>(i)];
                if (voice.active && voice.generation < oldest)
                {
                    oldest = voice.generation;
                    candidate = i;
                }
            }
        }
        if (candidate < 0)
            break;

        const int root = voices_[static_cast<std::size_t>(candidate)].rootMidi;
        const std::uint64_t generation = voices_[static_cast<std::size_t>(candidate)].generation;
        for (auto& voice : voices_)
        {
            if (voice.active && voice.rootMidi == root && voice.generation == generation)
            {
                silenceVoice(voice);
                ++free;
            }
        }
    }
}

void VoiceEngine::pushHeldNote(int midiNote) noexcept
{
    const auto pitch = static_cast<std::size_t>(std::clamp(midiNote, 0, 127));
    if (heldNoteCounts_[pitch] == 0)
    {
        if (heldCount_ < heldNoteCapacity)
            heldNotes_[static_cast<std::size_t>(heldCount_++)] = midiNote;
    }
    else
    {
        // Already held by another source: keep one entry, moved to the top so
        // the legato fallback still sees the most recent press last.
        for (int i = 0; i < heldCount_; ++i)
        {
            if (heldNotes_[static_cast<std::size_t>(i)] != midiNote)
                continue;
            for (int j = i; j + 1 < heldCount_; ++j)
                heldNotes_[static_cast<std::size_t>(j)]
                    = heldNotes_[static_cast<std::size_t>(j + 1)];
            heldNotes_[static_cast<std::size_t>(heldCount_ - 1)] = midiNote;
            break;
        }
    }

    if (heldNoteCounts_[pitch] < std::numeric_limits<std::uint16_t>::max())
        ++heldNoteCounts_[pitch];
}

VoiceEngine::HeldNoteState VoiceEngine::releaseHeldNote(int midiNote) noexcept
{
    const auto pitch = static_cast<std::size_t>(std::clamp(midiNote, 0, 127));
    if (heldNoteCounts_[pitch] == 0)
        return HeldNoteState::NotHeld;
    if (--heldNoteCounts_[pitch] != 0)
        return HeldNoteState::StillHeld;

    for (int i = 0; i < heldCount_; ++i)
    {
        if (heldNotes_[static_cast<std::size_t>(i)] != midiNote)
            continue;
        for (int j = i; j + 1 < heldCount_; ++j)
            heldNotes_[static_cast<std::size_t>(j)] = heldNotes_[static_cast<std::size_t>(j + 1)];
        --heldCount_;
        break;
    }
    return HeldNoteState::Released;
}

bool VoiceEngine::retuneForLegato(int midiNote, const EngineParameters& p)
{
    bool retuned = false;
    for (auto& voice : voices_)
    {
        if (!voice.active || voice.releasing)
            continue;

        const int sounding = p.mode == PerformanceMode::Chord
            ? chordMidiForSinger(midiNote, voice.singer, p) : midiNote;
        // Carry any glide still in flight so a fast run does not snap.
        voice.glideCents = std::clamp(
            100.0f * static_cast<float>(voice.midiNote - sounding) + voice.glideCents,
            -4800.0f, 4800.0f);
        voice.midiNote = sounding;
        voice.rootMidi = midiNote;
        voice.baseFrequency = midiToHz(sounding);
        voice.delaySamples = 0;
        // Retuning is an articulation event, not another elapsed 16-sample
        // control interval. Re-resolve the pitch and tract immediately while
        // leaving Drift's fixed stochastic clocks untouched.
        updateVoiceControl(voice, p, false);
        retuned = true;
    }
    return retuned;
}

void VoiceEngine::noteOn(int midiNote, float velocity)
{
    if (velocity <= 0.0f || !std::isfinite(velocity))
    {
        noteOff(midiNote);
        return;
    }
    if (!prepared_)
        return;

    midiNote = std::clamp(midiNote, 0, 127);
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    const EngineParameters p = snapshotParameters();
    // A new voice may take a singer identity nothing is currently using.
    singersInUse_ = ~0u;
    updateChunkState(p, false);

    // A note lower than anything sounding becomes the reference the rest of the
    // chord tunes to, and it has to be in place before the voices are built so
    // they start on the interval rather than gliding onto it.
    updateIntonationRoot();
    if (intonationRoot_ < 0 || midiNote < intonationRoot_)
        intonationRoot_ = midiNote;

    const bool hadHeldNote = heldCount_ > 0;
    pushHeldNote(midiNote);

    // Legato: an overlapping note bends the sounding voices instead of
    // restarting them, which is how a singer actually moves between pitches.
    // A repeat of the pitch already sounding is not a move, so it falls
    // through to the retrigger below and gets a fresh attack.
    if (p.legato && hadHeldNote && midiNote != soundingRoot_
        && retuneForLegato(midiNote, p))
    {
        updateIntonationRoot();
        soundingRoot_ = midiNote;
        lastRootMidi_ = midiNote;
        legatoPhrase_ = true;
        activeVoiceCount_.store(countActiveVoices(), std::memory_order_relaxed);
        return;
    }

    // A fresh attack ends any legato phrase these voices belonged to.
    legatoPhrase_ = false;

    const int total = voicesForMode(p);

    // A repeated MIDI root is a true retrigger; old ensemble members cannot leak into it.
    for (auto& voice : voices_)
        if (voice.active && voice.rootMidi == midiNote)
            silenceVoice(voice);
    makeRoomFor(total);

    ++generation_;
    const float glideFromCents = (p.glide > 0.0f && lastRootMidi_ >= 0)
        ? std::clamp(100.0f * static_cast<float>(lastRootMidi_ - midiNote), -2400.0f, 2400.0f)
        : 0.0f;
    const float modeTrim = total == 1 ? 0.88f : (p.mode == PerformanceMode::Chord ? 0.61f : 0.72f);
    // The direct path costs each singer 1/r, so the section's total direct
    // power would follow wherever the twelve distances happened to fall. The
    // normalisation puts it back where it was: the placement decides who is in
    // front, not how loud a section of this size is. A soloist is the n = 1
    // case, where it is exactly her own distance and the level is unchanged.
    const float groupGain = modeTrim * distanceNormalisation_[static_cast<std::size_t>(std::clamp(total, 1, singerCount))]
                          / std::sqrt(static_cast<float>(total));
    for (int singer = 0; singer < total; ++singer)
    {
        const int slot = findFreeVoice();
        if (slot < 0)
            break;
        const int soundingMidi = p.mode == PerformanceMode::Chord ? chordMidiForSinger(midiNote, singer, p) : midiNote;
        initialiseVoice(voices_[static_cast<std::size_t>(slot)], midiNote, soundingMidi,
                        singer, velocity, groupGain, total, glideFromCents, p);
    }

    updateIntonationRoot();
    soundingRoot_ = midiNote;
    lastRootMidi_ = midiNote;
    activeVoiceCount_.store(countActiveVoices(), std::memory_order_relaxed);
}

void VoiceEngine::initialiseVoice(Voice& voice, int rootMidi, int soundingMidi, int singerIndex,
                                  float velocity, float groupGain, int singerTotal,
                                  float glideFromCents, const EngineParameters& p)
{
    voice = Voice {};
    voice.active = true;
    voice.rootMidi = rootMidi;
    voice.midiNote = soundingMidi;
    voice.baseFrequency = midiToHz(soundingMidi);
    voice.singer = singerIndex % singerCount;
    voice.generation = generation_;
    voice.velocity = velocity;
    // The singer's own output for this note, with the ensemble trim taken out:
    // 1 at velocity 1 and 0.037 at velocity 0.05. It is what the source slope
    // is read from, so a mode trim must not reach it.
    voice.velocityGain = velocity * (0.67f + 0.33f * std::sqrt(velocity));
    voice.amplitudeGain = groupGain * voice.velocityGain;
    voice.glideCents = glideFromCents;
    voice.noiseState = hash32(static_cast<std::uint32_t>(generation_) ^
                              static_cast<std::uint32_t>(rootMidi * 977 + singerIndex * 131));
    voice.pitchDriftState = hash32(voice.noiseState ^ 0xa511e9b3u);
    voice.pitchDriftFast = std::clamp(
        triangularUnitVariance(voice.pitchDriftState),
        -driftStateLimit, driftStateLimit);
    voice.pitchDriftSlow = std::clamp(
        triangularUnitVariance(voice.pitchDriftState),
        -driftStateLimit, driftStateLimit);
    voice.vowelDriftState = hash32(voice.noiseState ^ 0x63d83595u);
    voice.vowelDriftMorph = std::clamp(
        triangularUnitVariance(voice.vowelDriftState),
        -driftStateLimit, driftStateLimit);
    voice.vowelDriftX = std::clamp(
        triangularUnitVariance(voice.vowelDriftState),
        -driftStateLimit, driftStateLimit);
    voice.vowelDriftY = std::clamp(
        triangularUnitVariance(voice.vowelDriftState),
        -driftStateLimit, driftStateLimit);
    voice.vowelDriftCountdown = vowelDriftUpdateControls_;
    voice.phase = 0.5f + 0.12f * hashFloat(voice.noiseState + 17u);
    // The vibrato was seeded at a fixed phase per singer, so twelve singers
    // arrived in the same relative configuration on every repeat of a note and
    // beat against each other identically. It is drawn from the note's own hash
    // instead: the render is still a pure function of the note sequence, which
    // a stateful random walk would have cost.
    voice.vibratoPhase = wrapPhase(0.5f + 0.5f * hashFloat(voice.noiseState + 29u));
    const auto& singer = singers_[static_cast<std::size_t>(voice.singer)];
    voice.vibratoSeed = hash32(voice.noiseState ^ 0x27d4eb2fu);
    voice.vibratoCycle = 0u;
    drawVibratoCycle(voice);
    // The relationship belongs mostly to the singer, with a small residual
    // for this particular breath and note. It is bounded inside the measured
    // identity window and faded in by Instability at the control update.
    voice.vibratoAmplitudePhase = std::clamp(
        singer.vibratoAmplitudePhase + 0.028f * hashFloat(voice.noiseState + 37u),
        0.125f, 0.417f);
    // Real singers do not start every note's vibrato on the same clock. These
    // are natural-path targets; Drift zero still selects the historical
    // 160/340 ms envelope exactly in updateVoiceControl().
    voice.vibratoFadeStart = 0.12f + 0.14f
        * (0.5f + 0.5f * hashFloat(voice.noiseState + 53u));
    voice.vibratoFadeDuration = 0.24f + 0.28f
        * (0.5f + 0.5f * hashFloat(voice.noiseState + 59u));
    // Habitual offset plus this attempt's own draw. A soloist has nobody to be
    // out of time with, so a single voice keeps entering and leaving on the
    // beat; Humanize scales the whole gesture, so at 0 the section is exact.
    const float ensembleHumanize = singerTotal == 1 ? 0.0f : p.humanize;
    const float delay = ensembleHumanize
        * (singer.entryOffset + entryJitterSeconds * hashFloat(voice.noiseState + 41u));
    voice.delaySamples = std::max(0, static_cast<int>(delay * static_cast<float>(sampleRate_)));
    const float releaseDelay = ensembleHumanize
        * (singer.releaseOffset + releaseJitterSeconds * hashFloat(voice.noiseState + 43u));
    voice.releaseDelaySamples = std::max(0, static_cast<int>(releaseDelay * static_cast<float>(sampleRate_)));
    voice.releaseTimeScale = std::max(0.25f, 1.0f + ensembleHumanize
        * (singer.releaseTendency + releaseNoteSpread * hashFloat(voice.noiseState + 47u)));
    voice.pitchScoop = -(7.0f + 19.0f * (0.5f + 0.5f * hashFloat(voice.noiseState + 23u))) * (0.25f + 0.75f * p.humanize);
    voice.controlCountdown = 0;
    updateVoiceControl(voice, p, false);
    voice.phaseIncrement = voice.targetPhaseIncrement;
}

void VoiceEngine::noteOff(int midiNote)
{
    midiNote = std::clamp(midiNote, 0, 127);
    if (sustainPedal_)
    {
        // The key is treated as still held for as long as the pedal is down,
        // which is what keeps a legato phrase from falling back to a key the
        // player has already let go of.
        auto& pending = sustainedNotes_[static_cast<std::size_t>(midiNote)];
        if (pending < std::numeric_limits<std::uint16_t>::max())
            ++pending;
        return;
    }

    const auto heldState = releaseHeldNote(midiNote);
    // Another controller still holds this pitch, so it keeps sounding.
    if (heldState == HeldNoteState::StillHeld)
        return;
    const bool wasHeld = heldState == HeldNoteState::Released;
    const EngineParameters p = snapshotParameters();

    // Releasing the top of a legato phrase falls back to the note underneath.
    // legatoPhrase_ keeps that true when Legato is switched off mid-phrase:
    // these voices were bent to the pitch being released, so without the
    // fallback the key still held underneath would go silent.
    if ((p.legato || legatoPhrase_) && wasHeld && soundingRoot_ == midiNote
        && heldCount_ > 0)
    {
        const int previous = heldNotes_[static_cast<std::size_t>(heldCount_ - 1)];
        singersInUse_ = ~0u;
        updateChunkState(p, false);
        if (retuneForLegato(previous, p))
        {
            updateIntonationRoot();
            soundingRoot_ = previous;
            lastRootMidi_ = previous;
            return;
        }
    }

    legatoPhrase_ = false;

    for (auto& voice : voices_)
        if (voice.active && voice.rootMidi == midiNote)
            beginRelease(voice);
    updateIntonationRoot();
    if (soundingRoot_ == midiNote)
        soundingRoot_ = -1;
}

/** A note-off is a laryngeal gesture, not the removal of a drive. The folds
    move at the offset, and which way they move is the phonation the note was
    already in: an aspirate offset abducts them, so transglottal flow continues
    while the oscillation stops and the note tapers from voice into breath; a
    glottal offset ends "while the folds are still approximated", so the flow
    is choked and the breath dies with the voice or before it.

    The engine's two adduction controls are Tension, which is the medial
    compression, and Breath, which is the size of the glottal chink; they are
    weighted alike because there is no third one. The gesture itself is an area
    ratio: a full aspirate offset opens the glottis to about twice the area it
    phonated at, a full glottal offset closes it to about half. Aspiration
    amplitude follows the square of the transglottal flow and the flow follows
    the area, so the area ratio reaches the aspiration as its square -- 4 at
    full abduction, 1 at neutral adduction, 1/4 at a pressed offset.

    Latched here rather than read per sample, because the release is the
    phonation the note was in at the instant the key came up, not the phonation
    the knobs drift to afterwards.
*/
void VoiceEngine::beginRelease(Voice& voice) noexcept
{
    if (voice.releasing)
        return;
    voice.releasing = true;
    const float adduction = clampUnit(0.5f * (1.0f - smoothedBreath_)
                                      + 0.5f * smoothedTension_);
    voice.abductionTarget = std::exp2(2.0f * (1.0f - 2.0f * adduction));
}

void VoiceEngine::allNotesOff()
{
    heldCount_ = 0;
    heldNoteCounts_.fill(0);
    sustainedNotes_.fill(0);
    legatoPhrase_ = false;
    intonationRoot_ = -1;
    soundingRoot_ = -1;
    for (auto& voice : voices_)
        if (voice.active)
            beginRelease(voice);
}

void VoiceEngine::allSoundOff() noexcept
{
    for (auto& voice : voices_)
        silenceVoice(voice);
    clearRoom();
    clearPlacement();
    heldCount_ = 0;
    heldNoteCounts_.fill(0);
    sustainedNotes_.fill(0);
    legatoPhrase_ = false;
    intonationRoot_ = -1;
    soundingRoot_ = -1;
    lastRootMidi_ = -1;
    meterLeft_ = meterRight_ = 0.0f;
    activeVoiceCount_.store(0, std::memory_order_relaxed);
    displayLevelLeft_.store(0.0f, std::memory_order_relaxed);
    displayLevelRight_.store(0.0f, std::memory_order_relaxed);
}

void VoiceEngine::clearRoom() noexcept
{
    roomLeft_.fill(0.0f);
    roomRight_.fill(0.0f);
    roomWriteIndex_ = 0;
    roomDampingLeft_ = roomDampingRight_ = 0.0f;
    roomLowCutLeft_ = roomLowCutRight_ = 0.0f;
    roomEnvelope_ = 0.0f;
}

void VoiceEngine::silenceVoice(Voice& voice) noexcept
{
    voice.active = false;
    voice.releasing = false;
    voice.envelope = 0.0f;
    voice.airEnvelope = 0.0f;
    voice.abduction = 1.0f;
    voice.abductionTarget = 1.0f;
    voice.sourceTilt = 0.0f;
    voice.nasalX1 = voice.nasalX2 = 0.0f;
    voice.nasalY1 = voice.nasalY2 = 0.0f;
    voice.nasal.clear();
    for (auto& resonator : voice.tract)
        resonator.clear();
}

float VoiceEngine::midiToHz(int midiNote) noexcept
{
    return 440.0f * std::exp2((static_cast<float>(midiNote) - 69.0f) / 12.0f);
}

float VoiceEngine::sectionReleaseSeconds(float humanize) noexcept
{
    return 0.105f + 0.19f * clampUnit(humanize);
}

void VoiceEngine::updateChunkState(const EngineParameters& p, bool advanceSmoothers)
{
    static constexpr float femaleBw[formantCount] { 75.0f, 90.0f, 125.0f, 185.0f, 260.0f };
    static constexpr float maleBw[formantCount] { 68.0f, 82.0f, 112.0f, 165.0f, 235.0f };
    // Nothing above the fifth formant is modelled, so the bank is not allowed to
    // starve the top octaves completely on the most closed vowels.
    constexpr float amplitudeFloor = 0.010f;

    const bool male = p.profile == VoiceProfile::Male;
    const int vowelIndex = vowelIndexOf(p.vowel);

    // Resonance and formant shift end up in the pole radius, which cannot be
    // smoothed downstream, so a jump on either would step the tract once per
    // chunk. Smooth them here instead.
    const float resonanceTarget = clampUnit(p.resonance);
    const float shiftTarget = std::clamp(p.formantShift, -24.0f, 24.0f);
    // The dynamic reaches the source tilt and the vibrato depth at the control
    // rate, so it is smoothed here as well; the two level gains it produces get
    // a second, per-sample smoother in process().
    const float dynamicsTarget = effectiveDynamics(p);
    const float nasalTarget = clampUnit(p.nasal);
    const float instabilityTarget = clampUnit(p.instability);
    if (!chunkStateValid_)
    {
        smoothedResonance_ = resonanceTarget;
        smoothedFormantShift_ = shiftTarget;
        smoothedDynamics_ = dynamicsTarget;
        smoothedNasal_ = nasalTarget;
        smoothedInstability_ = instabilityTarget;
    }
    else if (advanceSmoothers)
    {
        smoothedResonance_ += chunkGainCoefficient_ * (resonanceTarget - smoothedResonance_);
        smoothedFormantShift_ += chunkGainCoefficient_ * (shiftTarget - smoothedFormantShift_);
        smoothedDynamics_ += chunkGainCoefficient_ * (dynamicsTarget - smoothedDynamics_);
        smoothedNasal_ += chunkGainCoefficient_ * (nasalTarget - smoothedNasal_);
        smoothedInstability_ += chunkGainCoefficient_
            * (instabilityTarget - smoothedInstability_);
        // Drift zero is a structural bypass: once the short automation glide
        // is inaudibly close, land exactly on the target so the OU streams stop
        // consuming random draws instead of living forever in denormals.
        if (std::abs(instabilityTarget - smoothedInstability_) < 1.0e-4f)
            smoothedInstability_ = instabilityTarget;
    }
    chunkResponse_ = dynamicResponse(smoothedDynamics_);
    // A soloist is limited by nothing; a section member settles at the group
    // extent the review measures rather than at her own.
    chunkVibratoCents_ = vibratoExtentCents(
        p.vibrato, p.mode == PerformanceMode::Solo ? 0.0f : kSectionVibratoCents);

    std::array<float, formantCount> target {};
    formantsForPresetVowel(male, vowelIndex, target.data());
    const float morph = clampUnit(p.vowelMorph);
    if (morph > 0.0f)
    {
        std::array<float, formantCount> space {};
        formantsForVowelPoint(male, p.vowelX, p.vowelY, space.data());
        for (int formant = 0; formant < formantCount; ++formant)
            target[static_cast<std::size_t>(formant)] +=
                morph * (space[static_cast<std::size_t>(formant)] - target[static_cast<std::size_t>(formant)]);
    }

    const auto unclusteredTarget = target;

    // The singer's formant is not an amplitude trim. Narrowing the epilaryngeal
    // tube clusters F3, F4 and F5 into one reinforced peak at 2.5-3.5 kHz, and
    // that peak is what lets an unamplified voice carry over an orchestra.
    // Boosting the amplitude of three formants that stay 700 Hz apart does not
    // produce a cluster, which is all the 1.1 engine did. Effort strengthens
    // the configuration as well as tension does, so the dynamic reaches it too.
    const float epilarynx = clampUnit(smoothedTension_ * (0.40f + 0.60f * smoothedDynamics_));
    const float profileCluster = male ? 1.0f : femaleClusterStrength;
    const float nominalEpilarynx = epilarynx * profileCluster;
    chunkVowelDriftScale_.fill(1.0f);
    if (nominalEpilarynx > 0.0f)
    {
        const float clusterHz = male ? 2900.0f : 3200.0f;
        for (int formant = 2; formant < formantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            target[index] += 0.45f * nominalEpilarynx * (clusterHz - target[index]);
            // A local vowel displacement is pulled toward the epilaryngeal
            // target by the same affine map as the nominal pole.
            chunkVowelDriftScale_[index] = 1.0f
                - 0.45f * nominalEpilarynx;
        }
    }

    const float shift = formantShiftRatio(smoothedFormantShift_);
    chunkFormantShiftRatio_ = shift;
    // Moving every resonance by an octave changes which sparse harmonics land
    // on the poles as well as changing perceived body size.  Without a small
    // opposing level law the +/-12-semitone control spans almost 15 dB and is
    // heard as a fader. This power law is unity at the centre and contributes
    // only +/-2.10 dB at the endpoints; the remaining level motion is the real
    // source/tract interaction rather than a broad gain correction.
    chunkFormantShiftGain_ = std::exp2(0.029f * smoothedFormantShift_);
    const float bandwidthScale = (1.18f - 0.30f * smoothedResonance_)
        * (1.0f + 0.12f * smoothedBreath_);
    // The jaw runs out before the pitch does. Roughly 1.55x the open vowel's
    // own F1 is as far as it opens, and a tract shortened by the formant shift
    // reaches proportionally higher.
    chunkMaxF1_ = (male ? 730.0f : 850.0f) * 1.55f * shift;

    // Resolving the tract costs seven exponentials and a five-by-five cascade
    // evaluation, and none of it moves while a note is simply being held. Skip
    // the whole block unless one of its inputs actually changed.
    // The sentinel-zero initial state guarantees the first call resolves.
    bool tractMoved = tractInputs_[formantCount] != shift
                   || tractInputs_[formantCount + 1] != bandwidthScale
                   || tractInputs_[formantCount + 2] != epilarynx;
    for (int formant = 0; formant < formantCount; ++formant)
        tractMoved = tractMoved
            || tractInputs_[static_cast<std::size_t>(formant)] != target[static_cast<std::size_t>(formant)];

    if (tractMoved)
    {
        tractInputs_[formantCount] = shift;
        tractInputs_[formantCount + 1] = bandwidthScale;
        tractInputs_[formantCount + 2] = epilarynx;
        // Resolved once for the prepared sample rate in prepare(); see
        // formantBandwidthCeilingHz_.
        const float maximumBandwidth = formantBandwidthCeilingHz_;
        const float widthScale = bandwidthScale * std::sqrt(shift);
        for (int formant = 0; formant < formantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            tractInputs_[index] = target[index];
            chunkFormantHz_[index] = target[index] * shift;
            chunkUnclusteredFormantHz_[index] = unclusteredTarget[index] * shift;

            // Wider formants track a shifted tract so the resonances keep their
            // shape instead of turning needle-thin when the voice is made small.
            // A narrowed epilarynx damps the cluster less as well as pulling it
            // together, which is the other half of what makes the peak.
            const float ordinaryBandwidth = std::clamp(
                (male ? maleBw[index] : femaleBw[index]) * widthScale,
                20.0f, maximumBandwidth);
            const float clusterWidth = formant >= 2
                ? 1.0f - 0.30f * nominalEpilarynx : 1.0f;
            const float bandwidth = std::clamp(ordinaryBandwidth * clusterWidth,
                                               20.0f, maximumBandwidth);
            chunkBandwidth_[index] = bandwidth;
            chunkUnclusteredBandwidth_[index] = ordinaryBandwidth;
        }

        // A vocal tract does not hand its formants independent amplitudes: they
        // follow from the formant frequencies and bandwidths. Deriving them from
        // the equivalent all-pole cascade is what makes a front vowel actually
        // sound front, and compensating half of the cascade's absolute gain is
        // what stops the vowel pad and the formant shift from doubling as
        // volume controls.
        parallelFormantAmplitudes(chunkFormantHz_.data(), chunkBandwidth_.data(), formantCount,
                                  static_cast<float>(sampleRate_), amplitudeFloor,
                                  chunkAmplitude_.data());
    }

    for (int formant = 0; formant < formantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        // An open velum turns the mouth into a side branch: the sound leaves
        // through the nose, so the oral formants stop being the radiator.
        //
        // Tension does not appear here any more. It used to add 12% of gain to
        // F3 and 6% to F4, but three formants that stay 700 Hz apart with a
        // little more gain each are still three formants. The epilarynx model
        // above moves their frequencies and bandwidths together instead, which
        // is the mechanism that actually builds the singer's formant, and the
        // cascade-derived amplitudes follow from those frequencies.
        const float shaped = chunkAmplitude_[index] * (1.0f - 0.55f * smoothedNasal_);
        if (!chunkStateValid_)
            chunkFormantGain_[index] = shaped;
        else if (advanceSmoothers)
            chunkFormantGain_[index] += chunkGainCoefficient_ * (shaped - chunkFormantGain_[index]);
    }

    // The nasal branch. An oral tract alone cannot make an /m/: a parallel bank
    // of poles has zeros only where its sections happen to cancel, and the
    // anti-resonance a nasalised sound needs has to be placed. The murmur pole
    // sits at the nasal cavity's own resonance and the notch where the closed
    // mouth loads it; both scale with the formant shift like the rest of the
    // tract, because the nose belongs to the same head.
    chunkNasalMix_ = smoothedNasal_;
    chunkNasalActive_ = chunkNasalMix_ > 1.0e-4f;
    // The nostrils are a smaller and far more damped aperture than the mouth,
    // so a hum radiates less for the same effort. Without this the murmur pole
    // alone makes an open velum the loudest thing the instrument does.
    chunkNasalTrim_ = 1.0f / (1.0f + 2.05f * chunkNasalMix_);
    if (chunkNasalActive_)
    {
        const float nyquistGuard = 0.40f * static_cast<float>(sampleRate_);
        // Nasal formants are heavily damped, which is most of why a hum reads
        // as closed rather than as a low vowel.
        const float murmurHz = std::clamp(280.0f * shift, 40.0f, nyquistGuard);
        const float murmurRadius = nasalMurmurRadius_;
        const auto murmurTrig = sineCosineFromCycles(murmurHz * inverseSampleRate_);
        chunkNasalA1_ = 2.0f * murmurRadius * murmurTrig.cosine;
        chunkNasalA2_ = -murmurRadius * murmurRadius;
        // Opposite polarity to F1, for the same reason adjacent oral formants
        // alternate: summed with a common sign they cancel in the valley.
        chunkNasalB0Scale_ = -0.60f * chunkNasalMix_
            * formantResonatorGain(murmurRadius, murmurTrig.sine);

        // A pole-zero pair at one frequency, the zeros nearer the unit circle
        // than the poles. Klatt's bare antiresonator is normalised to unity at
        // DC, which for a zero this low leaves 48 dB of gain at Nyquist: it
        // would make a hum the brightest sound the instrument produces. The
        // matched pole returns the response to unity either side of the notch
        // instead, so what the branch removes is only the band it names.
        const float notchHz = std::clamp(950.0f * shift, 60.0f, nyquistGuard);
        const auto notchTrig = sineCosineFromCycles(notchHz * inverseSampleRate_);
        const float zeroRadius = nasalZeroRadius_;
        const float poleRadius = nasalNotchPoleRadius_;
        const float zeroA1 = 2.0f * zeroRadius * notchTrig.cosine;
        const float zeroA2 = -zeroRadius * zeroRadius;
        chunkNotchA1_ = 2.0f * poleRadius * notchTrig.cosine;
        chunkNotchA2_ = -poleRadius * poleRadius;
        // Unity at DC, so the branch places a notch rather than a tilt.
        const float normalise = (1.0f - chunkNotchA1_ - chunkNotchA2_)
            / std::max(1.0e-6f, 1.0f - zeroA1 - zeroA2);
        chunkZeroB0_ = normalise;
        chunkZeroB1_ = -zeroA1 * normalise;
        chunkZeroB2_ = -zeroA2 * normalise;
    }

    // Two more exponentials that only move when their parameter does.
    const float humanize = clampUnit(p.humanize);
    if (humanize != jitterHumanize_)
    {
        jitterHumanize_ = humanize;
        // The pitch-jitter smoothers run at the control rate; expressing them as
        // time constants keeps the jitter spectrum identical at every rate.
        const float timeConstant = 0.0208f - 0.0097f * humanize;
        jitterCoefficient_ = 1.0f - std::exp(-static_cast<float>(controlPeriod)
                                             * inverseSampleRate_ / timeConstant);
        // Only the first smoother sees white noise, so only its drive needs
        // renormalising; jitterSlow is fed from the already-normalised jitter
        // and inherits a rate-invariant depth from it.
        jitterScale_ = noiseSmootherScale(
            jitterCoefficient_,
            1.0f - std::exp(-static_cast<float>(controlPeriod)
                            / (referenceSampleRate * timeConstant)));
    }
    if (p.glide != glideAmount_)
    {
        glideAmount_ = p.glide;
        const float glideTime = glideTimeSeconds(p.glide);
        chunkGlideDecay_ = glideTime > 0.0f
            ? std::exp(-static_cast<float>(controlPeriod) * inverseSampleRate_ / glideTime)
            : 0.0f;
    }

    // Sub-0.15 Hz ensemble drift, evaluated from the absolute sample position so
    // the result never depends on how the host splits its buffers.
    const double position = static_cast<double>(samplePosition_);
    const auto absolutePhase = [position](double origin, double increment) noexcept
    {
        const double phase = origin + increment * position;
        return static_cast<float>(phase - std::floor(phase));
    };
    sharedPitchDrift_ = sine(absolutePhase(0.173, 0.047 / sampleRate_));
    sharedRateDrift_ = sine(absolutePhase(0.617, 0.019 / sampleRate_));
    sharedFormantDrift_ = sine(absolutePhase(0.391, 0.011 / sampleRate_));
    // Only the singers that are actually sounding need their drift refreshed;
    // a solo note would otherwise pay for eleven ensemble members it never uses.
    for (int singerIndex = 0; singerIndex < singerCount; ++singerIndex)
    {
        if ((singersInUse_ & (1u << singerIndex)) == 0u)
            continue;
        auto& singer = singers_[static_cast<std::size_t>(singerIndex)];
        singer.drift = sine(absolutePhase(0.071 + 0.137 * static_cast<double>(singerIndex),
                                          static_cast<double>(singer.driftIncrement)));
        singer.drift2 = sine(absolutePhase(0.283 + 0.079 * static_cast<double>(singerIndex),
                                           static_cast<double>(singer.drift2Increment)));
        singer.depthDrift = sine(absolutePhase(0.419 + 0.193 * static_cast<double>(singerIndex),
                                               static_cast<double>(singer.depthIncrement)));
        singer.formantDrift = sine(absolutePhase(0.733 + 0.113 * static_cast<double>(singerIndex),
                                                 static_cast<double>(singer.formantIncrement)));
    }

    const float sizeTarget = clampUnit(p.roomSize);
    if (!chunkStateValid_)
        smoothedRoomSize_ = sizeTarget;
    else if (advanceSmoothers)
        smoothedRoomSize_ += chunkGainCoefficient_ * (sizeTarget - smoothedRoomSize_);

    const float sizeScale = roomSizeScale(smoothedRoomSize_);
    const float maximumDelay = static_cast<float>(roomBufferSize) - 8.0f;
    for (int tap = 0; tap < 4; ++tap)
        roomDelay_[static_cast<std::size_t>(tap)] = std::clamp(
            roomBaseDelay_[static_cast<std::size_t>(tap)] * sizeScale, 8.0f, maximumDelay);

    // Slowly moving taps break up the metallic ringing a static comb network
    // produces on sustained vowels.
    const float modulationPhase = absolutePhase(0.041, 0.13 / sampleRate_);
    const float modulationDepth = 1.2f + 2.6f * smoothedRoomSize_;
    for (int tap = 0; tap < 4; ++tap)
        roomModulation_[static_cast<std::size_t>(tap)] = modulationDepth
            * sine(modulationPhase + 0.25f * static_cast<float>(tap));

    // Size 0.5 reproduces the historical decay exactly; a large room both
    // spaces the reflections further apart and rings for longer, which is what
    // separates a vocal booth from a stone hall.
    roomFeedback_ = std::clamp(0.62f + 0.14f * smoothedRoom_
                                   + 0.28f * (smoothedRoomSize_ - 0.5f),
                               0.30f, 0.94f);
    updatePlacement(p);
    chunkStateValid_ = true;
}

void VoiceEngine::drawVibratoCycle(Voice& voice) noexcept
{
    // A held human gesture does not repeat at one exact period and excursion.
    // Draw one bounded pair per cycle from a stream that belongs only to this
    // voice. Three uniforms sum to unit variance and cannot leave +/-3 sigma;
    // the clamps keep even the extreme Instability setting expressive rather
    // than allowing a cycle to stall or lurch.
    const float precedingDepthScale = voice.vibratoDepthScale;
    const float precedingContour = voice.vibratoContour;
    std::uint32_t state = hash32(voice.vibratoSeed
        ^ (voice.vibratoCycle * 2654435761u));
    ++voice.vibratoCycle;
    const auto normal = [&state]() noexcept
    {
        return randomBipolar(state) + randomBipolar(state) + randomBipolar(state);
    };

    const float rateDraw = normal();
    const float depthDraw = normal();
    if (voice.vibratoCycle == 1u)
    {
        // Begin in the stationary distribution. The vibrato fade hides this
        // first draw, but avoiding a slow variance ramp also makes short notes
        // receive the same kind of singer as long ones.
        voice.vibratoRateVariation = rateDraw;
        voice.vibratoDepthVariation = depthDraw;
    }
    else
    {
        // Human cycles are related gestures, not independent dice throws.
        // These AR(1) memories carry a tendency for two to four cycles while
        // the sqrt(1-rho^2) terms retain unit stationary variance.
        voice.vibratoRateVariation = 0.70f * voice.vibratoRateVariation
            + 0.7141428f * rateDraw;
        voice.vibratoDepthVariation = 0.64f * voice.vibratoDepthVariation
            + 0.7683749f * depthDraw;
    }
    voice.vibratoRateScale = std::clamp(
        1.0f + 0.10f * voice.vibratoRateVariation, 0.82f, 1.18f);
    voice.vibratoDepthScale = std::clamp(
        1.0f + 0.24f * voice.vibratoDepthVariation, 0.58f, 1.42f);
    voice.vibratoContour = std::clamp(
        0.55f * voice.vibratoContour + 0.8351647f * normal(), -3.0f, 3.0f);
    // The first draw has nothing to transition from. Later draws retain the
    // completed preceding cycle so updateVoiceControl() can carry both depth
    // and contour smoothly through the airflow-led AM phase at F0 phase zero.
    if (voice.vibratoCycle == 1u)
    {
        voice.vibratoPreviousDepthScale = voice.vibratoDepthScale;
        voice.vibratoPreviousContour = voice.vibratoContour;
    }
    else
    {
        voice.vibratoPreviousDepthScale = precedingDepthScale;
        voice.vibratoPreviousContour = precedingContour;
    }
}

void VoiceEngine::updateVowelDrift(Voice& voice,
                                   const EngineParameters& p,
                                   float driftAmount,
                                   bool advanceState,
                                   float vowelMorph,
                                   float vowelX,
                                   float vowelY) noexcept
{
    const int vowelIndex = vowelIndexOf(p.vowel);
    const int profileIndex = p.profile == VoiceProfile::Male ? 1 : 0;
    const VowelPoint anchor = presetVowelPosition(vowelIndex);
    // Already clampUnit(p.vowelMorph/vowelX/vowelY) -- see the declaration
    // comment in VoiceEngine.h for why these arrive resolved instead of being
    // reclamped from p here.
    const float baseMorph = vowelMorph;
    const float padX = vowelX;
    const float padY = vowelY;
    const float baseX = anchor.x + baseMorph * (padX - anchor.x);
    const float baseY = anchor.y + baseMorph * (padY - anchor.y);
    const bool male = profileIndex != 0;
    std::array<float, formantCount> baseSpace {};
    formantsForVowelPoint(male, baseX, baseY, baseSpace.data());

    voice.vowelDriftProfile = profileIndex;
    voice.vowelDriftVowel = vowelIndex;
    voice.vowelDriftInputMorph = baseMorph;
    voice.vowelDriftInputX = padX;
    voice.vowelDriftInputY = padY;
    voice.vowelDriftWasEnabled = driftAmount > 0.0f;

    if (!(driftAmount > 0.0f))
    {
        // Do not even add a computed zero to the old target. This is the exact
        // 1.3 path used by the model bypass and by Drift zero.
        voice.effectiveVowelMorph = baseMorph;
        voice.effectiveVowelX = baseX;
        voice.effectiveVowelY = baseY;
        voice.vowelDriftFormantHz = baseSpace;
        voice.vowelDriftFormantDeltaHz.fill(0.0f);
        return;
    }

    if (advanceState)
    {
        voice.vowelDriftMorph = advanceBoundedOu(
            voice.vowelDriftMorph, voice.vowelDriftState,
            vowelDriftMorphRetention_, vowelDriftMorphInnovation_);
        voice.vowelDriftX = advanceBoundedOu(
            voice.vowelDriftX, voice.vowelDriftState,
            vowelDriftXRetention_, vowelDriftXInnovation_);
        voice.vowelDriftY = advanceBoundedOu(
            voice.vowelDriftY, voice.vowelDriftState,
            vowelDriftYRetention_, vowelDriftYInnovation_);
    }

    // Close the morph drift smoothly at both endpoints. Besides protecting a
    // preset vowel's identity, this keeps the bounded, mean-zero OU motion
    // symmetric instead of relying on a one-sided clamp at morph 0 or 1.
    const float morphEndpointWindow = 4.0f * baseMorph * (1.0f - baseMorph);
    const float effectiveMorph = std::clamp(
        baseMorph + vowelDriftMorphMidpointDepth * driftAmount
            * voice.vowelDriftMorph * morphEndpointWindow,
        0.0f, 1.0f);
    const float effectiveX = std::clamp(
        anchor.x + effectiveMorph * (padX - anchor.x)
            + vowelDriftXDepth * driftAmount * voice.vowelDriftX,
        0.0f, 1.0f);
    const float effectiveY = std::clamp(
        anchor.y + effectiveMorph * (padY - anchor.y)
            + vowelDriftYDepth * driftAmount * voice.vowelDriftY,
        0.0f, 1.0f);
    voice.effectiveVowelMorph = effectiveMorph;
    voice.effectiveVowelX = effectiveX;
    voice.effectiveVowelY = effectiveY;

    // The shipped morph is linear in formant Hz, whereas the vowel space is a
    // nonlinear inverse-distance surface. Add only the local displacement on
    // that surface to the exact shipped target: at zero displacement the old
    // result survives bit for bit, at non-zero Drift the motion still follows
    // the geometry shown by the pad.
    std::array<float, formantCount> movedSpace {};
    formantsForVowelPoint(male, effectiveX, effectiveY, movedSpace.data());
    for (int formant = 0; formant < formantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        voice.vowelDriftFormantHz[index] = movedSpace[index];
        voice.vowelDriftFormantDeltaHz[index] = movedSpace[index]
            - baseSpace[index];
    }
}

void VoiceEngine::updateVoiceControl(Voice& voice, const EngineParameters& p,
                                     bool advanceDriftClock)
{
    const auto& singer = singers_[static_cast<std::size_t>(voice.singer)];
    const float singerDrift = singer.drift;
    const float singerDrift2 = singer.drift2;
    const float depthDrift = singer.depthDrift;
    const float formantDrift = singer.formantDrift;
    const float sharedPitch = sharedPitchDrift_;
    const float sharedRate = sharedRateDrift_;
    const float sharedFormant = sharedFormantDrift_;

    const float instability = clampUnit(smoothedInstability_);
    const float naturalAmount = std::sqrt(instability);
    const float driftAmount = p.legacyDriftBypass ? 0.0f : instability;
    const float ageSeconds = static_cast<float>(voice.ageSamples) * inverseSampleRate_;
    float vibratoFade = 0.0f;
    if (p.legacyDriftBypass || !(instability > 0.0f))
    {
        // Exact 1.3 branch, including the zero-Drift compatibility path.
        vibratoFade = smoothStep((ageSeconds - 0.16f) / 0.34f);
    }
    else
    {
        const float fadeStart = 0.16f
            + naturalAmount * (voice.vibratoFadeStart - 0.16f);
        const float fadeDuration = 0.34f
            + naturalAmount * (voice.vibratoFadeDuration - 0.34f);
        vibratoFade = smoothStep((ageSeconds - fadeStart) / fadeDuration);
    }

    const int vowelIndex = vowelIndexOf(p.vowel);
    const int profileIndex = p.profile == VoiceProfile::Male ? 1 : 0;
    const float vowelMorph = clampUnit(p.vowelMorph);
    const float vowelX = clampUnit(p.vowelX);
    const float vowelY = clampUnit(p.vowelY);
    const bool vowelDriftEnabled = driftAmount > 0.0f;
    const bool vowelInputsChanged = voice.vowelDriftProfile != profileIndex
        || voice.vowelDriftVowel != vowelIndex
        || voice.vowelDriftInputMorph != vowelMorph
        || voice.vowelDriftInputX != vowelX
        || voice.vowelDriftInputY != vowelY;
    bool advanceVowelDrift = false;
    if (vowelDriftEnabled && voice.controlInitialised && advanceDriftClock)
    {
        if (--voice.vowelDriftCountdown <= 0)
        {
            advanceVowelDrift = true;
            voice.vowelDriftCountdown = vowelDriftUpdateControls_;
        }
    }
    const bool vowelEnableChanged = vowelDriftEnabled
        != voice.vowelDriftWasEnabled;
    if (!vowelDriftEnabled && vowelEnableChanged)
        voice.vowelDriftCountdown = vowelDriftUpdateControls_;
    if (!voice.controlInitialised || vowelInputsChanged || vowelEnableChanged
        || advanceVowelDrift)
        updateVowelDrift(
            voice, p, driftAmount, advanceVowelDrift, vowelMorph, vowelX, vowelY);

    // The random range is a depth control: at zero both scales resolve to
    // exactly one; at full they expose the complete bounded per-cycle draw.
    // The broader Drift model uses a square-root taper so a modest setting is
    // not perceptually indistinguishable from a clocked LFO. A recalled 1.3
    // patch retains its original linear mapping.
    const float cycleVariationAmount = p.legacyDriftBypass
        ? instability : naturalAmount;
    const float cycleRateScale = 1.0f
        + cycleVariationAmount * (voice.vibratoRateScale - 1.0f);
    const float vibratoRate = singer.vibratoRate * cycleRateScale
        * (1.0f + p.humanize * (0.055f * singerDrift + 0.018f * sharedRate));
    // The extent is now a curve on the knob rather than a literal 20 cents per
    // unit, so the wander the identity contributes is a proportion of it rather
    // than a fixed number of cents: the old +/-7 on +/-20 is +/-35 %.
    // Integrating the rate keeps the vibrato phase exact for arbitrarily long
    // notes. Recomputing age * rate amplified any rate drift by the note age
    // and lost sub-sample precision once the age exceeded 2^24 samples.
    const float elapsedSeconds = static_cast<float>(voice.ageSamples - voice.lastControlAge)
        * inverseSampleRate_;
    voice.lastControlAge = voice.ageSamples;
    const float advancedPhase = voice.vibratoPhase + elapsedSeconds * vibratoRate;
    voice.vibratoPhase = wrapPhase(advancedPhase);
    if (advancedPhase >= 1.0f)
        drawVibratoCycle(voice);

    // Pitch is zero at the redraw, but amplitude vibrato leads pitch and is not.
    // A smooth phase-domain handoff prevents the new depth and contour from
    // becoming a short AM step (and therefore broadband sidebands). It is
    // complete before this cycle reaches its positive peak. At Instability 0
    // the scale below still resolves to exactly one and the wave blend to the
    // legacy sine, preserving that signal path bit for bit.
    const float cycleTransition = smoothStep(
        voice.vibratoPhase / vibratoCycleTransitionPhase_);
    const float transitionedDepthScale = voice.vibratoPreviousDepthScale
        + cycleTransition * (voice.vibratoDepthScale - voice.vibratoPreviousDepthScale);
    const float transitionedContour = voice.vibratoPreviousContour
        + cycleTransition * (voice.vibratoContour - voice.vibratoPreviousContour);
    const float cycleDepthScale = 1.0f
        + cycleVariationAmount * (transitionedDepthScale - 1.0f);
    const float vibratoDepth = chunkVibratoCents_ * singer.vibratoDepth * cycleDepthScale
        * (1.0f + 0.35f * p.humanize * depthDrift) * vibratoFade
        * chunkResponse_.vibratoScale;

    // Analysed sung cycles tend toward linear F0 ramps, with the rise a little
    // faster than the fall. Blend the legacy sine toward a mildly asymmetric
    // triangle. The square-root taper makes the useful natural range occupy
    // the lower half of the knob; zero still returns the sine bit for bit.
    const auto vibratoWave = [this, naturalAmount, transitionedContour](float phase) noexcept
    {
        phase = wrapPhase(phase);
        const float sinusoid = sine(phase);
        const float riseFraction = std::clamp(
            0.44f + 0.012f * naturalAmount * transitionedContour, 0.39f, 0.49f);
        const float trianglePhase = wrapPhase(phase + 0.5f * riseFraction);
        const float triangle = trianglePhase < riseFraction
            ? -1.0f + 2.0f * trianglePhase / riseFraction
            : 1.0f - 2.0f * (trianglePhase - riseFraction) / (1.0f - riseFraction);
        return sinusoid + 0.22f * naturalAmount * (triangle - sinusoid);
    };
    const float vibratoShape = vibratoWave(voice.vibratoPhase);
    // The laryngeal component of a sung vibrato. The cricothyroid oscillation
    // that carries the pitch also moves subglottal pressure and adduction, so
    // the level and the source slope swing on the very same cycle. The depth
    // follows the extent actually in force -- after the identity, the fade and
    // the dynamic -- rather than the knob, because it is one gesture and not
    // two: a note whose vibrato has not faded in yet has no laryngeal
    // modulation either, and a pianissimo note has as little of one as it has
    // of the other.
    const float laryngealPerCent = laryngealAmLegacyPerCent_
        + naturalAmount * (laryngealAmNaturalPerCent_ - laryngealAmLegacyPerCent_);
    const float laryngealMaximum = laryngealAmLegacyMaximum_
        + naturalAmount * (laryngealAmNaturalMaximum_ - laryngealAmLegacyMaximum_);
    const float laryngeal = std::min(laryngealPerCent * vibratoDepth,
                                     laryngealMaximum);
    const float amplitudeShape = vibratoWave(
        voice.vibratoPhase + naturalAmount * voice.vibratoAmplitudePhase);
    const float vibratoGainTarget = 1.0f + laryngeal * amplitudeShape;
    voice.vibratoGainStep = (vibratoGainTarget - voice.vibratoGain)
        / static_cast<float>(controlPeriod);
    // The two presence shelves cascade, so each carries the square root of
    // the laryngeal gain. Together they apply it once and the direct source
    // applies it once: the upper band therefore moves twice as far in dB as
    // the fundamental, without putting a square root in the sample loop.
    const float vibratoShelfGainTarget = std::sqrt(vibratoGainTarget);
    voice.vibratoShelfGainStep
        = (vibratoShelfGainTarget - voice.vibratoShelfGain)
        / static_cast<float>(controlPeriod);

    // Two nested smoothers give the pitch noise a 1/f-like spectrum instead of
    // the single-pole tilt a lone follower produces.
    const float random = randomBipolar(voice.noiseState) * jitterScale_;
    voice.jitter += (random - voice.jitter) * jitterCoefficient_;
    voice.jitterSlow += (voice.jitter - voice.jitterSlow) * jitterSlowCoefficient_;
    const float jitterCents = p.humanize * (1.15f * voice.jitter + 1.35f * voice.jitterSlow);
    if (driftAmount > 0.0f)
    {
        // The note-on draw starts in the stationary distribution. Do not step
        // it once more on that same initial control update; short notes should
        // receive the same depth as long ones, not a double draw at t=0.
        if (voice.controlInitialised && advanceDriftClock)
        {
            voice.pitchDriftFast = advanceBoundedOu(
                voice.pitchDriftFast, voice.pitchDriftState,
                pitchDriftFastRetention_, pitchDriftFastInnovation_);
            voice.pitchDriftSlow = advanceBoundedOu(
                voice.pitchDriftSlow, voice.pitchDriftState,
                pitchDriftSlowRetention_, pitchDriftSlowInnovation_);
        }
        voice.pitchDriftCents = pitchDriftDepthCents * driftAmount
            * (pitchDriftFastMix * voice.pitchDriftFast
               + pitchDriftSlowMix * voice.pitchDriftSlow);
    }
    else
    {
        voice.pitchDriftCents = 0.0f;
    }
    // An a cappella section tunes its chord to the bass rather than to a
    // keyboard, and moves onto the interval over about a tenth of a second.
    const float justTarget = intonationRoot_ >= 0
        ? clampUnit(p.intonation)
              * justIntonationOffsetCents(voice.midiNote - intonationRoot_)
        : 0.0f;
    if (!voice.controlInitialised)
        voice.justCents = justTarget;
    else
        voice.justCents += justGlide_ * (justTarget - voice.justCents);

    const float identityCents = singer.detuneCents * p.humanize;
    // A section does not sit at fixed offsets: it drifts and recovers, which
    // is what keeps the dispersion from reading as a chorus setting.
    const float wanderCents = p.humanize * (4.2f * singerDrift + 2.6f * singerDrift2
                                            + 0.75f * sharedPitch);
    voice.pitchScoop *= scoopMultiplier_;
    voice.glideCents *= chunkGlideDecay_;
    float cents = voice.pitchScoop + voice.glideCents + identityCents + wanderCents
                + jitterCents + voice.justCents
                + vibratoDepth * vibratoShape
                + 100.0f * pitchBendSemitones_;
    // Keep the exact legacy expression (and its floating-point association)
    // when the macro is bypassed or at zero.
    if (driftAmount > 0.0f)
        cents += voice.pitchDriftCents;
    const float frequency = voice.baseFrequency * std::exp2(cents * (1.0f / 1200.0f));
    voice.targetPhaseIncrement = std::clamp(frequency * inverseSampleRate_, 0.0f, 0.48f);
    voice.phaseIncrementStep = (voice.targetPhaseIncrement - voice.phaseIncrement)
        / static_cast<float>(controlPeriod);

    voice.tableLevel = mipTableLevelForFrequency(frequency);

    // Vocal effort changes the source spectral slope far more than it changes
    // level: a soft note is dull as well as quiet.
    const float effort = std::clamp((0.42f * voice.velocity + 0.58f * p.tension)
                                        * chunkResponse_.effortScale, 0.0f, 1.0f);
    if (effort != voice.tiltEffort)
    {
        voice.tiltEffort = effort;
        const float tiltCorner = 1400.0f * std::exp2(4.0f * effort);
        // The exact one-pole coefficient. The 2*pi*fc/fs approximation it
        // replaces saturated at its clamp above about fs/6, so the brightest
        // settings lost the tilt filter completely at 44.1 and 48 kHz but kept
        // it at 96 and 192.
        voice.tiltCoefficient = 1.0f - std::exp(-twoPi * tiltCorner * inverseSampleRate_);
    }

    // Sundberg: between pianissimo and fortissimo the partials above 1 kHz rise
    // about twice as fast in dB as the overall sound pressure level. So the
    // source's high-frequency share is not free to be drawn -- it has to fall
    // by exactly as many decibels as the note's own broadband gain does. That
    // gain is the shelf gain: unity at velocity 1 and full dynamic, 0.001 at
    // the quietest a note can be sung, and the two shelves turn it into a
    // spectral slope instead of the fader the level alone would be.
    //
    // What is stored is the square root of it, because the shelf is a cascade
    // of two stages and each of them applies this number. The plateau of the
    // pair is the square of what one stage carries, so a stage that carried the
    // whole gain would put the *square* of it above the corner - and with the
    // broadband gain already on the level, the band above the corner would fall
    // three times as fast as the fundamental instead of twice. At velocity 0.05
    // that is a plateau of -57.2 dB where the law asks for -28.6.
    voice.presence = std::sqrt(std::clamp(
        voice.velocityGain * chunkResponse_.voicedGain, 1.0e-4f, 1.0f));

    // A note's attack is the folds coming onto their limit cycle, and the
    // growth rate of that instability follows how far the subglottal pressure
    // sits above the phonation threshold: an accented note arrives well above
    // it and reaches amplitude in a few milliseconds, a soft one approaches
    // threshold from just above and takes an order of magnitude longer.
    // Velocity is what sets the pressure, with the phonation the tension is
    // already holding contributing the rest. Humanize survives as a multiplier
    // on the result rather than as its source: a loose take is still a late
    // take, it is just no longer the only thing an attack knows about.
    const float onsetDrive = 0.80f * voice.velocity + 0.20f * p.tension;
    // Pressure above threshold, not pressure: the threshold is a fixed offset,
    // which is why the last part of the range is where the attack time runs
    // away rather than the whole of it. Nothing sings below threshold, so the
    // excess is floored rather than allowed through zero.
    const float pressureExcess = std::max(onsetDrive - 0.10f, 0.030f);
    const float attackDrive = pressureExcess / (1.0f + 1.80f * clampUnit(p.humanize));
    if (attackDrive != voice.attackDrive)
    {
        voice.attackDrive = attackDrive;
        const float attackSeconds = std::clamp(0.0034f / attackDrive, 0.0020f, 0.120f);
        voice.attackCoefficient = 1.0f - std::exp(-inverseSampleRate_ / attackSeconds);
    }
    // The decay is the singer's, not the section's: the release time constant
    // is how fast her own subglottal pressure falls once the larynx stops.
    // Cached on the Humanize it was resolved for, so a sustained note pays for
    // the exponential once rather than at every control update.
    if (p.humanize != voice.releaseHumanize)
    {
        voice.releaseHumanize = p.humanize;
        voice.releaseCoefficient = std::exp(-inverseSampleRate_
            / (sectionReleaseSeconds(p.humanize) * voice.releaseTimeScale));
    }
    // The same gesture reaches the source-tension ramp: a hard attack is a
    // pressed one that starts close to its adducted target, a soft one starts
    // abducted and has further to travel. The shipped depth is the value at the
    // reference velocity 0.80, so the onset the first pass measured is
    // unchanged and only the rest of the velocity range moves.
    voice.tensionSag = std::clamp(
        sourceTensionRampDepth_ * (1.0f + 0.90f * (0.80f - voice.velocity)), 0.0f, 1.0f);

    voice.irregularity = voice.midiNote < 52
        ? (1.0f - p.tension) * p.humanize * 0.035f : 0.0f;
    // The offset's area gesture. It is a no-op on a held note, where the
    // target and the state are both 1, and it waits out the singer's own
    // release delay: the folds move when she lets go, not when the key does.
    if (voice.releaseDelaySamples <= 0)
        voice.abduction += abductionCoefficient_ * (voice.abductionTarget - voice.abduction);
    // The pitch-synchronous window moves the noise about inside the period; it
    // must not change how much of it there is. Its mean square depends on the
    // source tension, so the compensating gain is resolved here, at the control
    // rate, and the per-sample cost of the window stays exactly one multiply.
    const float windowTension = std::clamp(
        smoothedTension_ * chunkResponse_.sourceTensionScale
            * (1.0f - voice.tensionSag * voice.onsetAir), 0.0f, 1.0f);
    voice.airShape = aspirationScale_ * aspirationLevel * (0.22f + 0.78f * voice.onsetAir)
        * voice.abduction
        * aspirationWindowGain(windowTension, aspirationModulationDepth_);

    const float anatomy = 1.0f + p.humanize * (singer.anatomy
        + 0.0045f * formantDrift + 0.0022f * sharedFormant);
    // The tract follows the pitch the singer is deliberately travelling to,
    // including portamento and controller bend, but not the vibrato, jitter or
    // short onset scoop. Otherwise a B-flat5 bent down an octave would keep a
    // fully released soprano tract around a sounding B-flat4, and a long glide
    // would move the jaw to its destination before the oscillator arrived.
    const float tractCents = voice.glideCents + voice.justCents
        + 100.0f * pitchBendSemitones_;
    const float tractFundamental = voice.baseFrequency
        * std::exp2(tractCents * (1.0f / 1200.0f));
    const float highAmount = std::max(
        0.0f, 12.0f * std::log2(std::max(tractFundamental, 1.0f) / 440.0f));
    // Both resolved once for the prepared sample rate in prepare(), instead of
    // being recomputed from sampleRate_ on every control update of every
    // active voice; see formantHzCeilingHz_/formantBandwidthCeilingHz_.
    const float upperLimit = formantHzCeilingHz_;
    const float maximumBandwidth = formantBandwidthCeilingHz_;
    // Formant tuning resolves against the intentional, non-vibrato pitch. A
    // tract that followed each vibrato cycle would keep the fundamental on the
    // peak and cancel the amplitude modulation the vibrato produces up there.
    const float tuningFundamental = tractFundamental;
    float sopranoClusterRelease = 0.0f;
    float sopranoUpperRiseSpan = 0.0f;
    float sopranoR3Slope = sopranoR3RiseMean;
    float sopranoR4Slope = sopranoR4RiseMean;
    // The three soprano-register terms below are all female-only and were
    // formerly three separate re-checks of the same profile on every control
    // update of every active voice; one guard now covers all of them.
    if (p.profile == VoiceProfile::Female)
    {
        const float releaseOctaves = std::log2(
            std::max(tractFundamental, 1.0f) / sopranoClusterReleaseStartHz);
        const float releaseRange = std::log2(
            sopranoClusterReleaseEndHz / sopranoClusterReleaseStartHz);
        sopranoClusterRelease = smoothStep(releaseOctaves / releaseRange);

        float riseFundamental = tractFundamental;
        if (riseFundamental > sopranoUpperRiseSoftLimitHz)
        {
            const float range = sopranoUpperRiseEndHz
                - sopranoUpperRiseSoftLimitHz;
            const float amount = std::clamp(
                (riseFundamental - sopranoUpperRiseSoftLimitHz) / range,
                0.0f, 1.0f);
            // y=t-t^2/2 has unit slope at zero and zero slope at one. Its final
            // half-range puts the saturated displacement at almost exactly C6.
            const float eased = amount - 0.5f * amount * amount;
            riseFundamental = sopranoUpperRiseSoftLimitHz + range * eased;
        }
        sopranoUpperRiseSpan = std::max(
            riseFundamental - sopranoUpperRiseAnchorHz, 0.0f);

        // R3 and R4 are two modes of one moving tract, so their population
        // gesture shares one bounded anatomical draw. Independent +/-SD
        // extremes can bring the opposite-polarity poles almost on top of one
        // another even inside the measured range; a correlated gesture retains
        // both reported means and deviations without inventing that
        // cancellation. Reusing the existing R3 anatomy hash advances no
        // random stream, and Humanize zero still resolves to the means.
        constexpr int riseIdentityFormant = 2;
        constexpr float identityDepth = 0.014f
            + 0.006f * static_cast<float>(riseIdentityFormant);
        const float identityDraw = std::clamp(
            singer.formantScale[static_cast<std::size_t>(riseIdentityFormant)]
                / identityDepth,
            -1.0f, 1.0f);
        sopranoR3Slope += p.humanize * sopranoR3RiseDeviation * identityDraw;
        sopranoR4Slope += p.humanize * sopranoR4RiseDeviation * identityDraw;
    }

    const auto vowelDriftHz = [this, &voice, driftAmount,
                               sopranoClusterRelease](std::size_t index) noexcept
    {
        if (!(driftAmount > 0.0f))
            return 0.0f;
        const float clusteredScale = chunkVowelDriftScale_[index];
        const float releasedScale = clusteredScale
            + sopranoClusterRelease * (1.0f - clusteredScale);
        return chunkFormantShiftRatio_ * releasedScale
            * voice.vowelDriftFormantDeltaHz[index];
    };

    // R3-R5 alternate polarity in the parallel formant bank. Broadening two
    // adjacent poles past their centre separation makes their modes
    // geometrically indistinct and can deepen the cancellation inherent in the
    // parallel representation. Preserve every measured centre trajectory and
    // narrow only a near-coincident upper pair, proportionally, with a small
    // margin for float rounding. This is an order/separation guard, not a claim
    // that an alternating-polarity parallel bank becomes cancellation-free.
    // Sequential pairs are safe because narrowing R4 for R4/R5 can only make
    // the already-checked R3/R4 pair less overlapped.
    const bool needsUpperSeparation = p.profile == VoiceProfile::Female
        && sopranoUpperRiseSpan > 0.0f;
    std::array<float, formantCount> separatedTargetBandwidth;
    // Raw (unclamped) target Hz for formants 2-4, as the main loop below would
    // itself resolve them. Only meaningful when needsUpperSeparation: the main
    // loop reads it back instead of re-running the identical releasedHz/scale/
    // highTune/slope arithmetic a second time for those formants.
    std::array<float, formantCount> upperTargetHz {};
    if (needsUpperSeparation)
    {
        std::array<float, formantCount> boundedTargetHz;
        for (int formant = 2; formant < formantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            const float scale = anatomy + p.humanize * singer.formantScale[index];
            const float highTune = 1.0f + highAmount * 0.0003f;
            float releasedHz = chunkFormantHz_[index]
                + sopranoClusterRelease
                    * (chunkUnclusteredFormantHz_[index] - chunkFormantHz_[index]);
            if (driftAmount > 0.0f)
                releasedHz += vowelDriftHz(index);
            float targetHz = releasedHz * scale * highTune;
            if (formant == 2 || formant == 3)
            {
                const float slope = formant == 2
                    ? sopranoR3Slope : sopranoR4Slope;
                targetHz += chunkFormantShiftRatio_ * slope
                    * sopranoUpperRiseSpan;
            }
            upperTargetHz[index] = targetHz;
            boundedTargetHz[index] = std::clamp(targetHz, 25.0f, upperLimit);
            separatedTargetBandwidth[index] = std::clamp(
                chunkBandwidth_[index]
                    + sopranoClusterRelease
                        * (chunkUnclusteredBandwidth_[index]
                           - chunkBandwidth_[index]),
                20.0f, maximumBandwidth);
        }
        separateUpperFormantBandwidths(
            boundedTargetHz, separatedTargetBandwidth);
    }

    float tunedF1 = 0.0f;
    bool tractMoved = !voice.controlInitialised;
    for (int formant = 0; formant < formantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        // Formants 2-4 of a soprano note already ran this exact
        // releasedHz/scale/highTune/slope arithmetic in the pre-pass above,
        // to project the Hz the separation guard narrows bandwidths against;
        // read that resolved value back instead of paying for it twice on
        // every control update of every sounding high female voice.
        const bool usesUpperTarget = needsUpperSeparation && formant >= 2;
        float scale = 0.0f;
        float targetHz;
        if (usesUpperTarget)
        {
            targetHz = upperTargetHz[index];
        }
        else
        {
            scale = anatomy + p.humanize * singer.formantScale[index];
            const float highTune = 1.0f + highAmount * (formant == 0 ? 0.0032f : (formant == 1 ? 0.00125f : 0.0003f));
            // The chunk endpoint is already the broad, quarter-strength soprano
            // reinforcement below E-flat5. From there to B-flat5 interpolate both
            // the centres and the pole widths back to the ordinary vowel tract.
            // Male voices keep the full lower-voice cluster at every pitch.
            float releasedHz = chunkFormantHz_[index]
                + sopranoClusterRelease
                    * (chunkUnclusteredFormantHz_[index] - chunkFormantHz_[index]);
            if (driftAmount > 0.0f)
                releasedHz += vowelDriftHz(index);
            targetHz = releasedHz * scale * highTune;
            if (p.profile == VoiceProfile::Female && (formant == 2 || formant == 3))
            {
                const float slope = formant == 2 ? sopranoR3Slope : sopranoR4Slope;
                // Formant Shift is a synthetic tract-length transform. Scale the
                // measured displacement with that tract as well; adding the same
                // absolute 300--700 Hz after a one-octave-down transform can make
                // the numbered upper resonances cross.
                targetHz += chunkFormantShiftRatio_ * slope * sopranoUpperRiseSpan;
            }
        }
        float targetF1Lift = 0.0f;
        if (formant == 0)
        {
            const float base = targetHz;
            targetHz = tunedFirstFormant(base, tuningFundamental, chunkMaxF1_ * scale);
            targetF1Lift = std::clamp(
                (targetHz / std::max(base, 1.0f) - 1.0f) * 5.0f,
                0.0f, 1.0f);
            tunedF1 = targetHz;
        }
        else if (formant == 1)
        {
            // A tracked F1 that has climbed into F2 would leave the tract with
            // two coincident lowest resonances. The vowel loses its identity
            // instead, which is what happens to a real one at that pitch.
            targetHz = std::max(targetHz, kMinimumFormantSpacing * tunedF1);
        }
        const float currentHz = voice.formantHz[index];
        float coefficient = 1.0f;
        if (currentHz <= 1.0f)
        {
            voice.formantHz[index] = targetHz;
        }
        else
        {
            // An articulator has a speed, not a deadline: a quarter-frequency
            // move or more takes the full jaw-and-tongue time, and a small one
            // settles in a fraction of it.
            const float span = std::min(1.0f, std::abs(targetHz - currentHz)
                                                  * formantSpanScale_[index]);
            coefficient = formantGlideFast_[index]
                + span * (formantGlideSlow_[index] - formantGlideFast_[index]);
            voice.formantHz[index] = currentHz + coefficient * (targetHz - currentHz);
        }

        if (formant == 0)
        {
            if (!voice.controlInitialised || currentHz <= 1.0f)
                voice.formantTuningLift = targetF1Lift;
            else
                voice.formantTuningLift += coefficient
                    * (targetF1Lift - voice.formantTuningLift);
        }

        // Same redundant-work shape as the Hz target above: for these same
        // formants the plain chunkBandwidth_ blend would be computed only to
        // be immediately replaced by the separation guard's result.
        float targetBandwidth;
        if (usesUpperTarget)
            targetBandwidth = separatedTargetBandwidth[index];
        else
            targetBandwidth = chunkBandwidth_[index]
                + sopranoClusterRelease
                    * (chunkUnclusteredBandwidth_[index] - chunkBandwidth_[index]);
        const float currentBandwidth = voice.formantBandwidth[index];
        if (currentBandwidth <= 1.0f)
            voice.formantBandwidth[index] = targetBandwidth;
        else
            voice.formantBandwidth[index] = currentBandwidth
                + coefficient * (targetBandwidth - currentBandwidth);

        voice.formantHz[index] = std::clamp(voice.formantHz[index], 25.0f, upperLimit);
        voice.formantBandwidth[index] = std::clamp(
            voice.formantBandwidth[index], 20.0f, maximumBandwidth);
        tractMoved = tractMoved
            || voice.resolvedFormantHz[index] != voice.formantHz[index]
            || voice.resolvedFormantBandwidth[index] != voice.formantBandwidth[index];
    }

    if (needsUpperSeparation)
    {
        // Different cavities have different articulation rates. Project the
        // realised geometry too, so a safe pair of endpoints cannot overlap
        // while a vowel, formant shift or pitch glide is still in flight.
        separateUpperFormantBandwidths(
            voice.formantHz, voice.formantBandwidth);
        for (int formant = 2; formant < formantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            tractMoved = tractMoved
                || voice.resolvedFormantBandwidth[index]
                    != voice.formantBandwidth[index];
        }
    }

    // A singer spends the resonance she has actually reached on efficiency,
    // not the resonance at the far end of an articulation. Reading the
    // smoothed F1 here prevents a legato note event from applying the entire
    // high-register trim in one sample while the jaw is still moving.
    const float lift = std::clamp(voice.formantTuningLift, 0.0f, 1.0f);
    // Exact per-voice cascade normalisation returns a little more of the tuned
    // pole's gain than the former nominal-tract estimate. Hand that extra
    // efficiency back as breath support too: a fully tuned F1 costs 2.22x
    // gain rather than 2x, keeping the top octave inside the no-shout window.
    const float renderGainTarget = voice.amplitudeGain * chunkNasalTrim_
        * chunkFormantShiftGain_
        / (1.0f + 1.22f * lift);
    if (!voice.controlInitialised)
    {
        voice.renderGain = renderGainTarget;
        voice.renderGainStep = 0.0f;
    }
    else
    {
        voice.renderGainStep = (renderGainTarget - voice.renderGain)
            / static_cast<float>(controlPeriod);
    }

    if (tractMoved)
    {
        std::array<float, formantCount> poleA1 {};
        std::array<float, formantCount> poleA2 {};
        std::array<float, formantCount> peakNormaliser {};
        parallelFormantCoefficients(
            voice.formantHz.data(), voice.formantBandwidth.data(), formantCount,
            static_cast<float>(sampleRate_), 0.010f, voice.formantAmplitude.data(),
            poleA1.data(), poleA2.data(), peakNormaliser.data());
        for (int formant = 0; formant < formantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            voice.resolvedFormantHz[index] = voice.formantHz[index];
            voice.resolvedFormantBandwidth[index] = voice.formantBandwidth[index];
            // The physical Hz/BW values above already carry the articulation
            // smoothing. Keep the rendered pole exactly coherent with that
            // geometry instead of adding a second, sample-rate-dependent lag.
            voice.tract[index].a1 = poleA1[index];
            voice.tract[index].a2 = poleA2[index];
            voice.tract[index].peakNormaliser = peakNormaliser[index];
        }
    }

    // formantGain, tract[].b0 and the nasal branch's own a1/a2/b0 depend only
    // on formantAmplitude and peakNormaliser -- set above, and only inside the
    // tractMoved block -- on smoothedNasal_, and on chunkFormantShiftRatio_
    // (the nasal murmur/notch frequencies retune with formant shift exactly
    // as the oral formants do). updateChunkState() resolves the mix and the
    // shift ratio once per 64-sample chunk, so all three are identical across
    // the up-to-four control updates that chunk spans. Recomputing five
    // formants' worth of gain and pole b0, plus the nasal branch's own
    // coefficients, on every control update reran the same arithmetic on most
    // of them.
    if (tractMoved || voice.resolvedNasalMix != smoothedNasal_
        || voice.resolvedNasalShift != chunkFormantShiftRatio_)
    {
        for (int formant = 0; formant < formantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            voice.formantGain[index] = voice.formantAmplitude[index]
                * (1.0f - 0.55f * smoothedNasal_);
            voice.tract[index].b0 = formantPolarity(formant)
                * voice.formantGain[index] * voice.tract[index].peakNormaliser;
        }
        // The nasal tract does not vary with the vowel or with the singer; its
        // branch coefficients are resolved once per chunk in updateChunkState()
        // and only copied and scaled by this voice's own F1 amplitude here.
        voice.nasal.a1 = chunkNasalA1_;
        voice.nasal.a2 = chunkNasalA2_;
        voice.nasal.b0 = voice.formantAmplitude[0] * chunkNasalB0Scale_;
        voice.resolvedNasalMix = smoothedNasal_;
        voice.resolvedNasalShift = chunkFormantShiftRatio_;
    }

    if (!voice.controlInitialised)
    {
        const float target = radiatedPowerTarget(
            voice, tractFundamental,
            smoothedTension_ * chunkResponse_.sourceTensionScale,
            p.legacyRadiatedPowerBypass);
        voice.radiatedPowerTarget = target;
        voice.radiatedPowerGain = target;
        voice.radiatedPowerGainStep = 0.0f;
        voice.radiatedPowerCountdown = radiatedPowerUpdateControls_;
    }
    else
    {
        if (--voice.radiatedPowerCountdown <= 0)
        {
            const float rawTarget = radiatedPowerTarget(
                voice, tractFundamental,
                smoothedTension_ * chunkResponse_.sourceTensionScale,
                p.legacyRadiatedPowerBypass);
            voice.radiatedPowerTarget += radiatedPowerCoefficient_
                * (rawTarget - voice.radiatedPowerTarget);
            voice.radiatedPowerCountdown = radiatedPowerUpdateControls_;
        }
        voice.radiatedPowerGainStep =
            (voice.radiatedPowerTarget - voice.radiatedPowerGain)
            / static_cast<float>(controlPeriod);
    }

    // A voice no longer carries a pan. Where it is heard from belongs to the
    // singer, not to the note, and it is resolved once per chunk in
    // updatePlacement() for the identity rather than once per control period
    // for every voice that shares her.
    voice.controlInitialised = true;
}

VoiceEngine::GlottalShapePosition VoiceEngine::glottalShapePosition(float tension) noexcept
{
    tension = clampUnit(tension);
    const float shapePosition = tension * static_cast<float>(glottalShapeCount - 1);
    const int lowerShape = std::min(static_cast<int>(shapePosition),
                                    glottalShapeCount - 2);
    return { lowerShape, shapePosition - static_cast<float>(lowerShape), tension };
}

float VoiceEngine::glottalPair(int level, float phase, float tension) const noexcept
{
    return glottalPair(level, phase, glottalShapePosition(tension));
}

float VoiceEngine::glottalPair(int level, float phase,
                               GlottalShapePosition shape) const noexcept
{
    const auto& table = tables_->glottalTables[static_cast<std::size_t>(level)];
    const float position = phase * static_cast<float>(tableSize);
    const int truncated = static_cast<int>(position);
    const int index = truncated & tableMask;
    const float fraction = position - static_cast<float>(truncated);
    const auto next = static_cast<std::size_t>((index + 1) & tableMask);

    const auto lowerOffset = static_cast<std::size_t>(shape.lowerShape * tableSize);
    const auto upperOffset = lowerOffset + static_cast<std::size_t>(tableSize);
    const auto current = static_cast<std::size_t>(index);
    const float lowerAtPhase = table[lowerOffset + current];
    const float upperAtPhase = table[upperOffset + current];
    const float lowerAtNext = table[lowerOffset + next];
    const float upperAtNext = table[upperOffset + next];
    const float atPhase = lowerAtPhase
        + shape.shapeFraction * (upperAtPhase - lowerAtPhase);
    const float atNextPhase = lowerAtNext
        + shape.shapeFraction * (upperAtNext - lowerAtNext);

    // Already clamped once by glottalShapePosition() to resolve lowerShape and
    // shapeFraction above; reuse it instead of clamping the caller's tension a
    // second time on every sample.
    const float gainPosition = shape.clampedTension * static_cast<float>(glottalGainTableSize - 1);
    const int gainIndex = std::min(static_cast<int>(gainPosition),
                                   glottalGainTableSize - 2);
    const float gainFraction = gainPosition - static_cast<float>(gainIndex);
    const auto& gains = tables_->glottalGainTable[static_cast<std::size_t>(level)];
    const float gain = gains[static_cast<std::size_t>(gainIndex)]
        + gainFraction
            * (gains[static_cast<std::size_t>(gainIndex + 1)]
               - gains[static_cast<std::size_t>(gainIndex)]);
    return gain * (atPhase + fraction * (atNextPhase - atPhase));
}

float VoiceEngine::radiatedPowerTarget(const Voice& voice, float fundamental,
                                       float sourceTension,
                                       bool legacyBypass) const noexcept
{
    if (legacyBypass || tables_ == nullptr || !std::isfinite(fundamental)
        || !(fundamental > 0.0f))
        return 1.0f;

    // A fully nasal tract uses a different murmur pole and series notch which
    // this oral estimate intentionally excludes. Make both that case and the
    // test-only depth-zero A/B a real bypass before doing the H8 tract scan.
    // radiatedPowerDepth_ is test-settable to arbitrary values (see
    // setRadiatedPowerDepth() in VoiceEngineTests.cpp) so it still needs
    // clamping here, but smoothedNasal_ is a block-rate smoother that only
    // ever lands on clampUnit(p.nasal) or blends toward it by a coefficient
    // in (0, 1), so it never leaves [0, 1] and re-clamping it on every
    // voice's control update was wasted work.
    const float oralAmount = clampUnit(radiatedPowerDepth_)
        * (1.0f - smoothedNasal_);
    if (!(oralAmount > 0.0f))
        return 1.0f;

    // Above Nyquist the oscillator itself is already bounded. Use the same
    // representable fundamental here so the analysis-rate 8/16 kHz fallback
    // remains finite rather than asking a transfer function about aliases.
    const float boundedFundamental = std::clamp(
        fundamental, 20.0f, 0.45f * static_cast<float>(sampleRate_));
    // Select the analysis mip from the same intentional f0 as the power law.
    // The oscillator's running mip follows vibrato, jitter and scoop; borrowing
    // it here would let those excluded motions flip the analyzed harmonic set
    // and pump the 40 ms support follower near a mip boundary.
    const int tableLevel = mipTableLevelForFrequency(boundedFundamental);
    const int representableHarmonics = std::min(
        static_cast<int>(0.45f * static_cast<float>(sampleRate_)
                         / boundedFundamental),
        harmonicsPerLevel[static_cast<std::size_t>(tableLevel)]);
    const int harmonics = std::clamp(
        representableHarmonics, 1, radiatedPowerHarmonics);

    sourceTension = clampUnit(sourceTension);
    const float powerPosition = sourceTension
        * static_cast<float>(glottalGainTableSize - 1);
    const int powerIndex = std::min(static_cast<int>(powerPosition),
                                    glottalGainTableSize - 2);
    const float powerFraction = powerPosition - static_cast<float>(powerIndex);

    // The two source shelves read the preceding one-pole state. Resolve their
    // exact complex transfer once per harmonic; the final effort filter is an
    // ordinary current-sample one-pole. Only magnitudes are needed because the
    // tract comparison uses the same source partial on every offset probe.
    const float shelfCoefficient = sourcePresenceCoefficient_;
    const float shelfMemory = 1.0f - shelfCoefficient;
    const float shelfGain = std::clamp(voice.presence, 1.0e-4f, 1.0f);
    const float tiltCoefficient = std::clamp(voice.tiltCoefficient, 1.0e-6f, 1.0f);
    const float tiltMemory = 1.0f - tiltCoefficient;

    // b0 is the pole's own signed peak gain -- polarity, cascade amplitude and
    // peak normaliser -- none of which depends on the probe frequency. The
    // scan below calls tractPowerAt() once per harmonic and once per harmonic
    // per offset (up to 8 + 8*8 = 72 times), and every one of those calls
    // used to re-derive the same five values from scratch. Resolved once here
    // instead.
    std::array<float, formantCount> formantB0 {};
    for (int formant = 0; formant < formantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        formantB0[index] = formantPolarity(formant)
            * voice.formantAmplitude[index] * voice.tract[index].peakNormaliser;
    }

    const auto tractPowerAt = [this, &voice, &formantB0](float frequency) noexcept
    {
        const float bounded = std::clamp(
            frequency, 1.0f, 0.499f * static_cast<float>(sampleRate_));
        const auto trig = sineCosineFromCycles(bounded * inverseSampleRate_);
        const float cosine = trig.cosine;
        const float sineValue = trig.sine;
        const float cosTwo = 2.0f * cosine * cosine - 1.0f;
        const float sinTwo = 2.0f * sineValue * cosine;
        double sumReal = 0.0;
        double sumImaginary = 0.0;
        for (int formant = 0; formant < formantCount; ++formant)
        {
            const auto index = static_cast<std::size_t>(formant);
            const auto& pole = voice.tract[index];
            const float real = 1.0f - pole.a1 * cosine - pole.a2 * cosTwo;
            const float imaginary = pole.a1 * sineValue + pole.a2 * sinTwo;
            const float denominator = real * real + imaginary * imaginary;
            if (!(denominator > 0.0f))
                continue;
            const float scale = formantB0[index] / denominator;
            sumReal += static_cast<double>(scale) * real;
            sumImaginary -= static_cast<double>(scale) * imaginary;
        }
        return sumReal * sumReal + sumImaginary * sumImaginary;
    };

    double currentPower = 0.0;
    double averagedPower = 0.0;
    for (int harmonic = 1; harmonic <= harmonics; ++harmonic)
    {
        const float frequency = static_cast<float>(harmonic) * boundedFundamental;
        const auto trig = sineCosineFromCycles(frequency * inverseSampleRate_);
        const float denominatorReal = 1.0f - shelfMemory * trig.cosine;
        const float denominatorImaginary = shelfMemory * trig.sine;
        const float denominator = denominatorReal * denominatorReal
            + denominatorImaginary * denominatorImaginary;
        const float numeratorReal = shelfCoefficient * trig.cosine;
        const float numeratorImaginary = -shelfCoefficient * trig.sine;
        const float precedingLowReal = denominator > 0.0f
            ? (numeratorReal * denominatorReal
               + numeratorImaginary * denominatorImaginary) / denominator
            : 0.0f;
        const float precedingLowImaginary = denominator > 0.0f
            ? (numeratorImaginary * denominatorReal
               - numeratorReal * denominatorImaginary) / denominator
            : 0.0f;
        const float shelfReal = shelfGain
            + (1.0f - shelfGain) * precedingLowReal;
        const float shelfImaginary = (1.0f - shelfGain)
            * precedingLowImaginary;
        const float oneShelfPower = shelfReal * shelfReal
            + shelfImaginary * shelfImaginary;
        const float tiltDenominator = 1.0f + tiltMemory * tiltMemory
            - 2.0f * tiltMemory * trig.cosine;
        const float tiltPower = tiltDenominator > 0.0f
            ? tiltCoefficient * tiltCoefficient / tiltDenominator : 1.0f;

        const auto harmonicIndex = static_cast<std::size_t>(harmonic - 1);
        const float lowerPower = tables_->glottalHarmonicPower[
            static_cast<std::size_t>(powerIndex)][harmonicIndex];
        const float glottalPower = lowerPower + powerFraction
            * (tables_->glottalHarmonicPower[
                   static_cast<std::size_t>(powerIndex + 1)][harmonicIndex]
               - lowerPower);
        const double sourcePower = static_cast<double>(glottalPower)
            * oneShelfPower * oneShelfPower * tiltPower;
        currentPower += sourcePower * tractPowerAt(frequency);

        double offsetPower = 0.0;
        for (int offset = 0; offset < radiatedPowerOffsets; ++offset)
        {
            const float delta = (static_cast<float>(offset) + 0.5f)
                    / static_cast<float>(radiatedPowerOffsets)
                - 0.5f;
            offsetPower += tractPowerAt(
                (static_cast<float>(harmonic) + delta) * boundedFundamental);
        }
        averagedPower += sourcePower * offsetPower
            / static_cast<double>(radiatedPowerOffsets);
    }

    if (!(currentPower > 1.0e-30) || !(averagedPower > 1.0e-30))
        return 1.0f;
    // Full power equalisation would be sqrt(Pavg/Pcurrent). Keep only half of
    // that correction (the fourth root) and never move voiced drive by more
    // than +/-3 dB. The remaining level motion is part of the sung colour.
    constexpr float minimumGain = 0.70794578f;
    constexpr float maximumGain = 1.41253754f;
    float target = std::pow(
        static_cast<float>(averagedPower / currentPower), 0.25f);
    target = std::clamp(target, minimumGain, maximumGain);
    if (oralAmount >= 1.0f)
        return target;
    return std::pow(std::max(target, 1.0e-6f), oralAmount);
}

float VoiceEngine::glottalFlow(float phase, float tension) const noexcept
{
    return glottalFlow(phase, glottalShapePosition(tension));
}

float VoiceEngine::glottalFlow(float phase, GlottalShapePosition shape) const noexcept
{
    // No fractional interpolation. The envelope is smooth and it multiplies
    // noise, so a staircase 1/256 of a period wide is inaudible -- its largest
    // step, on the closing slope, is under 2 % of the peak -- and it saves two
    // loads on a table small enough to stay resident.
    const auto index = static_cast<std::size_t>(
        static_cast<int>(phase * static_cast<float>(flowTableSize)) & flowTableMask);
    const auto lower = index * static_cast<std::size_t>(glottalShapeCount)
        + static_cast<std::size_t>(shape.lowerShape);
    const auto upper = lower + 1;
    return tables_->glottalFlowTable[lower]
        + shape.shapeFraction
            * (tables_->glottalFlowTable[upper] - tables_->glottalFlowTable[lower]);
}

float VoiceEngine::sine(float phase) const noexcept
{
    phase = wrapPhase(phase);
    if (tables_ == nullptr)
        return std::sin(twoPi * phase);
    const float position = phase * static_cast<float>(tableSize);
    const int truncated = static_cast<int>(position);
    const int index = truncated & tableMask;
    const float fraction = position - static_cast<float>(truncated);
    const float a = tables_->sineTable[static_cast<std::size_t>(index)];
    const float b = tables_->sineTable[static_cast<std::size_t>((index + 1) & tableMask)];
    return a + fraction * (b - a);
}

VoiceEngine::SineCosine VoiceEngine::sineCosineFromCycles(float cycles) const noexcept
{
    if (tables_ == nullptr)
    {
        const float angle = twoPi * wrapPhase(cycles);
        return { std::sin(angle), std::cos(angle) };
    }
    const float position = wrapPhase(cycles) * static_cast<float>(tableSize);
    const int truncated = static_cast<int>(position);
    const int index = truncated & tableMask;
    const float fraction = position - static_cast<float>(truncated);
    const auto lookup = [this, index, fraction](int offset) noexcept
    {
        const float a = tables_->sineTable[static_cast<std::size_t>((index + offset) & tableMask)];
        const float b = tables_->sineTable[static_cast<std::size_t>((index + offset + 1) & tableMask)];
        return a + fraction * (b - a);
    };
    return { lookup(0), lookup(tableSize / 4) };
}

float VoiceEngine::randomBipolar(std::uint32_t& state) noexcept
{
    std::uint32_t x = state;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    state = x == 0u ? 1u : x;
    return static_cast<float>(state & 0x00ffffffu) / 8388607.5f - 1.0f;
}

float VoiceEngine::triangularUnitVariance(std::uint32_t& state) noexcept
{
    // A uniform bipolar draw has variance 1/3. Three independent draws sum to
    // variance one, remain exactly bounded to +/-3, and avoid the tails (and
    // transcendentals) of a Gaussian generator on the audio thread.
    return randomBipolar(state) + randomBipolar(state)
        + randomBipolar(state);
}

float VoiceEngine::advanceBoundedOu(float current, std::uint32_t& state,
                                    float retention,
                                    float innovationScale) noexcept
{
    const float next = retention * current
        + innovationScale * triangularUnitVariance(state);
    return std::clamp(next, -driftStateLimit, driftStateLimit);
}

void VoiceEngine::updateRoom(float inputLeft, float inputRight,
                             float& wetLeft, float& wetRight) noexcept
{
    const auto read = [this](const std::array<float, roomBufferSize>& buffer, float delay)
    {
        const int whole = static_cast<int>(delay);
        const float fraction = delay - static_cast<float>(whole);
        int index = roomWriteIndex_ - whole;
        if (index < 0)
            index += roomBufferSize;
        int previous = index - 1;
        if (previous < 0)
            previous += roomBufferSize;
        const float a = buffer[static_cast<std::size_t>(index)];
        const float b = buffer[static_cast<std::size_t>(previous)];
        return a + fraction * (b - a);
    };

    const float la = read(roomLeft_, roomDelay_[0] + roomModulation_[0]);
    const float lb = read(roomLeft_, roomDelay_[2] + roomModulation_[2]);
    const float ra = read(roomRight_, roomDelay_[1] + roomModulation_[1]);
    const float rb = read(roomRight_, roomDelay_[3] + roomModulation_[3]);
    const float rawLeft = 0.72f * la + 0.28f * rb;
    const float rawRight = 0.72f * ra + 0.28f * lb;
    // Tracks the audible room level from above so process() can prove the
    // tail has fully rung out and skip silent rendering entirely.
    roomEnvelope_ = std::max(std::abs(rawLeft) + std::abs(rawRight)
                             + std::abs(inputLeft) + std::abs(inputRight),
                             roomEnvelope_ * roomEnvelopeDecay_);
    roomDampingLeft_ += roomDampingCoefficient_ * (rawLeft - roomDampingLeft_);
    roomDampingRight_ += roomDampingCoefficient_ * (rawRight - roomDampingRight_);

    roomLeft_[static_cast<std::size_t>(roomWriteIndex_)] = 0.30f * inputLeft
        + roomFeedback_ * (0.84f * roomDampingLeft_ + 0.16f * roomDampingRight_);
    roomRight_[static_cast<std::size_t>(roomWriteIndex_)] = 0.30f * inputRight
        + roomFeedback_ * (0.84f * roomDampingRight_ + 0.16f * roomDampingLeft_);
    roomWriteIndex_ = (roomWriteIndex_ + 1) & (roomBufferSize - 1);

    float left = 0.55f * rawLeft + 0.23f * lb;
    float right = 0.55f * rawRight + 0.23f * rb;
    // A gentle low cut stops the tail from clouding the low mids of a choir.
    roomLowCutLeft_ += roomLowCutCoefficient_ * (left - roomLowCutLeft_);
    roomLowCutRight_ += roomLowCutCoefficient_ * (right - roomLowCutRight_);
    wetLeft = left - roomLowCutLeft_;
    wetRight = right - roomLowCutRight_;
}

void VoiceEngine::updatePlacement(const EngineParameters& p) noexcept
{
    const float scale = roomSizeScale(smoothedRoomSize_);
    const float spread = clampUnit(p.spread);
    const bool solo = p.mode == PerformanceMode::Solo;
    if (spread == placementSpread_ && scale == placementScale_ && solo == placementSolo_
        && placementReflectionDepth_ == placementReflectionResolved_)
        return;
    placementSpread_ = spread;
    placementScale_ = scale;
    placementSolo_ = solo;
    placementReflectionResolved_ = placementReflectionDepth_;

    // The shoebox. Only the surfaces move with Size; where the singers stand
    // does not, which is what keeps the direct-path delays fixed.
    const float halfWidth = roomHalfWidthMetres * scale;
    const float ceiling = roomCeilingMetres * scale;
    const float floor = roomFloorMetres * scale;

    constexpr float degreesToRadians = 3.14159265f / 180.0f;
    const float axisSine = std::sin(receiverAxisDegrees * degreesToRadians);
    const float axisCosine = std::cos(receiverAxisDegrees * degreesToRadians);
    const float maximumDelay = static_cast<float>(placementBufferSize) - 4.0f;
    const float depth = placementReflectionDepth_;
    float longest = 0.0f;

    for (int index = 0; index < singerCount; ++index)
    {
        const auto& singer = singers_[static_cast<std::size_t>(index)];
        const float radius = singer.distanceMetres;
        // A soloist stands in front of the listener rather than at her section
        // position, which is the narrowing the pan law used to apply.
        const float narrowing = (index == 0 && solo) ? 0.18f : 1.0f;
        const float azimuth = sectionAzimuthDegrees * degreesToRadians
                            * std::clamp(singer.pan * spread * narrowing, -1.0f, 1.0f);
        const float sourceX = radius * std::sin(azimuth);
        const float sourceY = radius * std::cos(azimuth);

        // The source and its four first-order images: the two side walls
        // mirror it in x, the floor and the ceiling in z.
        const std::array<std::array<float, 4>, placementTapCount> sources {{
            { sourceX, sourceY, 0.0f, 1.0f },
            { -2.0f * halfWidth - sourceX, sourceY, 0.0f, wallReflectance * depth },
            { 2.0f * halfWidth - sourceX, sourceY, 0.0f, wallReflectance * depth },
            { sourceX, sourceY, -2.0f * floor, floorReflectance * depth },
            { sourceX, sourceY, 2.0f * ceiling, wallReflectance * depth }
        }};

        auto& wholes = placementWhole_[static_cast<std::size_t>(index)];
        auto& allpass = placementAllpass_[static_cast<std::size_t>(index)];
        auto& gains = placementGain_[static_cast<std::size_t>(index)];
        std::array<float, 2> direct {};
        for (int ear = 0; ear < 2; ++ear)
        {
            const float side = ear == 0 ? -1.0f : 1.0f;
            const float receiverX = 0.5f * side * receiverSpacingMetres;
            // Cardioid, pointed out to the side at the pair's half-angle.
            const float axisX = side * axisSine;
            const float axisY = axisCosine;
            for (int tap = 0; tap < placementTapCount; ++tap)
            {
                const auto& source = sources[static_cast<std::size_t>(tap)];
                const float dx = source[0] - receiverX;
                const float dy = source[1];
                const float dz = source[2];
                const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                const float inverse = 1.0f / std::max(distance, 0.05f);
                // 0.5 + 0.5 cos of the angle between the arrival and the
                // capsule axis, written on the direction cosines so no inverse
                // trigonometry is needed.
                const float pattern = 0.5f + 0.5f * (axisX * dx + axisY * dy) * inverse;
                const auto slot = static_cast<std::size_t>(ear * placementTapCount + tap);
                gains[slot] = placementTrim_ * source[3] * pattern * inverse;
                const float delay = std::clamp(
                    (distance - placementReferenceMetres_) * placementMetresToSamples_,
                    1.0f, maximumDelay);
                // The fraction is kept in [0.5, 1.5) by borrowing one whole
                // sample, which is where a first-order allpass has its shortest
                // transient and its least group-delay dispersion.
                const int whole = static_cast<int>(delay - 0.5f);
                const float fraction = delay - static_cast<float>(whole);
                wholes[slot] = whole;
                allpass[slot] = (1.0f - fraction) / (1.0f + fraction);
                longest = std::max(longest, delay);
                if (tap == 0)
                    direct[static_cast<std::size_t>(ear)] = delay;
            }
        }
        // When this singer is heard, in samples: the mean of the two receivers'
        // direct-path delays, which is what an ear measures and what the
        // ensemble's timing contract is written on.
        placementDirectSamples_[static_cast<std::size_t>(index)] = 0.5f * (direct[0] + direct[1]);
    }

    placementMaximumSamples_ = static_cast<int>(longest) + 2;
}

void VoiceEngine::renderPlacement(int count) noexcept
{
    // One tap: an integer read and a first-order allpass for the fraction.
    // y[n] = a (x[n] - y[n-1]) + x[n-1], where x[n-1] is simply the next sample
    // down the same line, so the tap costs two loads and has unit magnitude at
    // every frequency.
    const auto read = [](const float* line, int write, int whole, float coefficient,
                         float& state) noexcept
    {
        int index = write - whole;
        if (index < 0)
            index += placementBufferSize;
        int previous = index - 1;
        if (previous < 0)
            previous += placementBufferSize;
        state = coefficient * (line[index] - state) + line[previous];
        return state;
    };

    for (int singer = 0; singer < singerCount; ++singer)
    {
        const auto index = static_cast<std::size_t>(singer);
        const bool sounding = (singersInUse_ & (1u << singer)) != 0u;
        if (sounding)
            placementHold_[index] = placementMaximumSamples_ + chunkSize;
        float* const line = placementLine_.data() + index * placementBufferSize;
        const float* const bus = singerMix_[index].data();
        int write = placementWriteIndex_;

        if (placementHold_[index] <= 0)
        {
            // Nothing of this singer is left in her line, and a hold of zero
            // means she is not sounding either - `sounding` above sets the hold
            // to its maximum. Write silence rather than her bus: outside the
            // hold the bus is no longer cleared for her, so it still holds the
            // last chunk she sang, and copying that in would leave the span the
            // taps reach full of a repeat of it for her next note to read.
            for (int i = 0; i < count; ++i)
            {
                line[write] = 0.0f;
                write = (write + 1) & placementBufferMask;
            }
            continue;
        }

        const auto& wholes = placementWhole_[index];
        const auto& allpass = placementAllpass_[index];
        auto& state = placementAllpassState_[index];
        const auto& gains = placementGain_[index];
        for (int i = 0; i < count; ++i)
        {
            line[write] = bus[static_cast<std::size_t>(i)];
            const float directLeft = read(line, write, wholes[0], allpass[0], state[0]) * gains[0];
            const float directRight = read(line, write, wholes[5], allpass[5], state[5]) * gains[5];
            float earlyLeft = 0.0f;
            float earlyRight = 0.0f;
            for (int tap = 1; tap < placementTapCount; ++tap)
            {
                const auto slot = static_cast<std::size_t>(tap);
                const auto mirror = slot + placementTapCount;
                earlyLeft += read(line, write, wholes[slot], allpass[slot], state[slot]) * gains[slot];
                earlyRight += read(line, write, wholes[mirror], allpass[mirror], state[mirror])
                            * gains[mirror];
            }
            const auto sample = static_cast<std::size_t>(i);
            mixLeft_[sample] += directLeft;
            mixRight_[sample] += directRight;
            earlyLeft_[sample] += earlyLeft;
            earlyRight_[sample] += earlyRight;
            write = (write + 1) & placementBufferMask;
        }

        if (!sounding)
            placementHold_[index] = std::max(0, placementHold_[index] - count);
    }

    placementWriteIndex_ = (placementWriteIndex_ + count) & placementBufferMask;
}

void VoiceEngine::clearPlacement() noexcept
{
    placementLine_.fill(0.0f);
    placementWriteIndex_ = 0;
    placementHold_.fill(0);
    for (auto& state : placementAllpassState_)
        state.fill(0.0f);
    for (auto& bus : singerMix_)
        bus.fill(0.0f);
}

void VoiceEngine::renderVoice(Voice& voice, const EngineParameters& p, int count)
{
    float* const singerBus = singerMix_[static_cast<std::size_t>(voice.singer)].data();
    float phase = voice.phase;
    float phaseIncrement = voice.phaseIncrement;
    float envelope = voice.envelope;
    float airEnvelope = voice.airEnvelope;
    float onsetAir = voice.onsetAir;
    float shimmer = voice.shimmer;
    float lastNoise = voice.lastNoise;
    float sourceTilt = voice.sourceTilt;
    float sourceSlow = voice.sourceSlow;
    float sourceSlower = voice.sourceSlower;
    std::uint32_t noiseState = voice.noiseState;
    bool alternateCycle = voice.alternateCycle;
    std::uint64_t age = voice.ageSamples;

    float amplitude = voice.renderGain;
    float amplitudeStep = voice.renderGainStep;
    const bool releasing = voice.releasing;
    // What develops at a vocal onset is the source, not the filter: the folds
    // start abducted and lax and adduct over the first tens of milliseconds.
    // onsetAir is already that gesture, so the same exponential that gives the
    // note its breathy puff pulls the glottal prototype toward its lax end and
    // lets it firm up onto the block's tension. Re-read on every control update
    // so a sustained note cannot drift onto a stale depth, and scaled by the
    // note's velocity, because a hard attack starts closer to its adducted
    // target than a soft one.
    float tensionSag = voice.tensionSag;
    // Re-read on every control update for the same reason the ramp depth is:
    // the tests switch it mid-render to tell a redistribution of the noise from
    // a change in its level.
    float airModulation = aspirationModulationDepth_;

    float incrementStep = voice.phaseIncrementStep;
    float tilt = voice.tiltCoefficient;
    float presence = voice.presence;
    float vibratoGain = voice.vibratoGain;
    float vibratoGainStep = voice.vibratoGainStep;
    float vibratoShelfGain = voice.vibratoShelfGain;
    float vibratoShelfGainStep = voice.vibratoShelfGainStep;
    float radiatedPowerGain = voice.radiatedPowerGain;
    float radiatedPowerGainStep = voice.radiatedPowerGainStep;
    float attack = voice.attackCoefficient;
    float release = voice.releaseCoefficient;
    float irregularity = voice.irregularity;
    float airShape = voice.airShape;
    int level = voice.tableLevel;
    // Chunk-constant, so the branch is free: an oral-only patch pays nothing
    // for the nasal branch it is not using.
    const bool nasalActive = chunkNasalActive_;
    const float nasalMix = chunkNasalMix_;

    for (int i = 0; i < count; ++i)
    {
        if (voice.delaySamples > 0)
        {
            --voice.delaySamples;
            continue;
        }

        if (--voice.controlCountdown <= 0)
        {
            // The control update draws from the same noise stream as the
            // per-sample shimmer, so the cached state has to change hands.
            voice.phase = phase;
            voice.phaseIncrement = phaseIncrement;
            voice.onsetAir = onsetAir;
            voice.ageSamples = age;
            voice.noiseState = noiseState;
            voice.vibratoGain = vibratoGain;
            voice.vibratoShelfGain = vibratoShelfGain;
            voice.radiatedPowerGain = radiatedPowerGain;
            // updateVoiceControl() aims the efficiency compensation from its
            // exact running value. Hand the local ramp state back first so a
            // host buffer boundary cannot restart or skip any part of it.
            voice.renderGain = amplitude;
            // initialiseVoice() has already resolved age zero. The render
            // loop visits that same instant once more on its first sample, so
            // only later fixed-period visits advance Drift's OU clocks.
            updateVoiceControl(voice, p, voice.ageSamples != 0u);
            noiseState = voice.noiseState;
            voice.controlCountdown = controlPeriod;
            amplitudeStep = voice.renderGainStep;
            incrementStep = voice.phaseIncrementStep;
            tilt = voice.tiltCoefficient;
            irregularity = voice.irregularity;
            airShape = voice.airShape;
            level = voice.tableLevel;
            tensionSag = voice.tensionSag;
            airModulation = aspirationModulationDepth_;
            presence = voice.presence;
            attack = voice.attackCoefficient;
            release = voice.releaseCoefficient;
            vibratoGainStep = voice.vibratoGainStep;
            vibratoShelfGainStep = voice.vibratoShelfGainStep;
            radiatedPowerGainStep = voice.radiatedPowerGainStep;
        }

        amplitude += amplitudeStep;
        vibratoGain += vibratoGainStep;
        vibratoShelfGain += vibratoShelfGainStep;
        radiatedPowerGain += radiatedPowerGainStep;
        phaseIncrement += incrementStep;
        phase += phaseIncrement;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            alternateCycle = !alternateCycle;
        }

        if (releasing && voice.releaseDelaySamples <= 0)
        {
            // Both components are driven by the same decaying subglottal
            // pressure, so once the larynx stops moving they fall together.
            // What separates them at an offset is the area gesture, which
            // rides on airShape at the control rate; the squared envelope
            // this replaced made every note die cleaner than it lived.
            envelope *= release;
            airEnvelope *= release;
        }
        else
        {
            // Either the note is held, or it is one of the twelve that has not
            // let go yet. Both keep singing.
            if (releasing)
                --voice.releaseDelaySamples;
            envelope += (1.0f - envelope) * attack;
            airEnvelope += (1.0f - airEnvelope) * airAttackCoefficient_;
        }
        onsetAir *= onsetAirMultiplier_;

        const float sourceTension = tensionAt_[static_cast<std::size_t>(i)]
            * (1.0f - tensionSag * onsetAir);
        // glottalPair() and glottalFlow() both interpolate the same nine
        // analysed shapes from this same tension; resolving the shared
        // position once here instead of once per call halves that part of
        // the per-sample cost.
        const auto shapePosition = glottalShapePosition(sourceTension);
        float glottal = glottalPair(level, phase, shapePosition);
        glottal *= 1.0f + (alternateCycle ? irregularity : -irregularity);
        // Two first-order shelves, unity at DC and the note's broadband gain
        // above the corner:
        // a quiet note keeps its fundamental and loses its upper partials at
        // twice the rate, which is the measured law rather than a voicing
        // choice. One shelf cannot deliver it -- a first-order transition moves
        // 3 kHz at most 6 dB per octave away from 450 Hz however it is placed,
        // and the law needs about twice that across those two bands. Each stage
        // carries the square root of both the dynamics and vibrato gains, so
        // the pair applies each gain once. With the direct-source gesture below,
        // the upper band moves twice as far in dB as the fundamental.
        const float shelfGain = presence * vibratoShelfGain;
        const float slowDelta = glottal - sourceSlow;
        const float shelved = sourceSlow + shelfGain * slowDelta;
        sourceSlow += sourcePresenceCoefficient_ * slowDelta;
        const float slowerDelta = shelved - sourceSlower;
        const float shaped = sourceSlower + shelfGain * slowerDelta;
        sourceSlower += sourcePresenceCoefficient_ * slowerDelta;
        sourceTilt += tilt * (shaped - sourceTilt);

        const float noise = randomBipolar(noiseState);
        shimmer += (noise - shimmer) * shimmerCoefficient_;
        // Aspiration turbulence is generated by the jet leaving the glottal
        // constriction, so its envelope is the glottal flow: it rises through
        // the open phase and is extinguished while the folds are closed. Noise
        // without that envelope is heard as a separate source sitting behind
        // the voice instead of as the voice's own breath. The window multiplies
        // the turbulent flow *before* the pre-emphasis differentiates it, which
        // is the same radiation accounting the voiced source gets, and the
        // table is normalised to unit mean square so this moves the noise about
        // in time without changing how much of it there is.
        const float turbulence = noise
            * (1.0f + airModulation * (glottalFlow(phase, shapePosition) - 1.0f));
        const float highNoise = turbulence - aspirationPreEmphasis_ * lastNoise;
        lastNoise = turbulence;

        // Aspiration is generated at the glottis, so it belongs in the source
        // and is shaped by the very same tract as the voiced excitation. That
        // is both more faithful and two resonators per voice cheaper than the
        // separate noise filter the 1.0 engine used.
        const float airDrive = airLevelAt_[static_cast<std::size_t>(i)]
            * airShape * airEnvelope;
        const float voicedDrive = envelope * voicedScaleAt_[static_cast<std::size_t>(i)]
            * (1.0f + shimmerDepth_ * shimmer) * vibratoGain
            * radiatedPowerGain;
        // No separate lip-radiation stage: the wavetable is a glottal flow
        // *derivative*, and differentiating the flow is exactly what radiation
        // from the lips does to it. A radiation zero here would apply that
        // differentiation a second time.
        //
        // The vanishingly small bias parks the recursive tract states on a
        // normal-range fixed point. That keeps the filters out of denormal
        // territory on hosts that do not set flush-to-zero for us, without the
        // per-tick compare-and-select that costs a third of the voice budget.
        //
        // Not const: the nasal branch below mixes its anti-resonance into the
        // excitation in place.
        float source = sourceTilt * voicedDrive + highNoise * airDrive + denormalBias;

        // The nasal anti-resonance is in series with the whole tract, so it is
        // applied to the excitation: one biquad per voice instead of one per
        // formant, for exactly the same transfer function.
        if (nasalActive)
        {
            const float notched = chunkZeroB0_ * source + chunkZeroB1_ * voice.nasalX1
                                + chunkZeroB2_ * voice.nasalX2
                                + chunkNotchA1_ * voice.nasalY1
                                + chunkNotchA2_ * voice.nasalY2;
            voice.nasalX2 = voice.nasalX1;
            voice.nasalX1 = source;
            voice.nasalY2 = voice.nasalY1;
            voice.nasalY1 = notched;
            source += nasalMix * (notched - source);
        }

        // The tract geometry is fully formed before the first pulse reaches
        // it, so every formant including the upper reinforcement is present in
        // the first cycle. Later control updates move its per-voice poles only
        // through the physical articulation smoothers above.
        float tract = 0.0f;
        for (int f = 0; f < formantCount; ++f)
        {
            const auto index = static_cast<std::size_t>(f);
            tract += voice.tract[index].tick(source);
        }
        if (nasalActive)
            tract += voice.nasal.tick(source);

        const float output = amplitude * (tractLevel * tract + directAirLevel * highNoise * airDrive);
        // Into the singer's own bus, not into the stereo mix: the voices that
        // share an identity are one source standing in one place, and where
        // that place is is the placement stage's business.
        singerBus[static_cast<std::size_t>(i)] += output;
        ++age;

        if (releasing && envelope < 0.00008f && airEnvelope < 0.00008f)
        {
            voice.ageSamples = age;
            silenceVoice(voice);
            return;
        }
    }

    voice.phase = phase;
    voice.phaseIncrement = phaseIncrement;
    voice.envelope = envelope;
    voice.airEnvelope = airEnvelope;
    voice.onsetAir = onsetAir;
    voice.shimmer = shimmer;
    voice.lastNoise = lastNoise;
    voice.sourceTilt = sourceTilt;
    voice.sourceSlow = sourceSlow;
    voice.sourceSlower = sourceSlower;
    voice.noiseState = noiseState;
    voice.alternateCycle = alternateCycle;
    voice.ageSamples = age;
    voice.phaseIncrementStep = incrementStep;
    voice.renderGain = amplitude;
    voice.renderGainStep = amplitudeStep;
    voice.vibratoGain = vibratoGain;
    voice.vibratoShelfGain = vibratoShelfGain;
    voice.radiatedPowerGain = radiatedPowerGain;
    voice.radiatedPowerGainStep = radiatedPowerGainStep;
}

void VoiceEngine::process(float* left, float* right, int numSamples)
{
    if (numSamples <= 0)
        return;
    if (!prepared_)
    {
        // prepare() owns the process-shared LF table bank's one-time heap build
        // and is deliberately a non-real-time operation. Silencing an engine
        // whose host skipped prepare is safer than allocating and blocking for
        // tens of milliseconds in the first audio callback.
        if (left != nullptr)
            std::fill(left, left + numSamples, 0.0f);
        if (right != nullptr)
            std::fill(right, right + numSamples, 0.0f);
        return;
    }

    blockParameters_ = snapshotParameters();
    // shimmerScale_ renormalises the smoother's output for the sample rate;
    // folding it in here keeps the per-sample loop untouched.
    shimmerDepth_ = 0.026f * blockParameters_.humanize * shimmerScale_;

    // Voices only start in noteOn(), which the audio thread calls between
    // process() calls, so the set of sounding voices is fixed for this block.
    // Collecting it once keeps the per-sample loop from scanning all voice
    // slots (and touching their cold cache lines) when only a few are in use.
    activeTotal_ = 0;
    singersInUse_ = 0u;
    for (auto& voice : voices_)
    {
        if (!voice.active)
            continue;
        activeVoices_[static_cast<std::size_t>(activeTotal_++)] = &voice;
        singersInUse_ |= 1u << voice.singer;
    }
    updateIntonationRoot();

    // Once every voice has ended and the room tail has audibly rung out, the
    // block renders exact digital silence. Advance the state that remains
    // observable at the next note without paying the full per-sample cost.
    // placementHold_ only has to be scanned to decide that, so fold the scan
    // into the condition itself (with an early exit on the first singer still
    // holding) instead of always walking every singer up front: the common
    // case, at least one voice sounding, now short-circuits before the loop
    // ever runs.
    const auto anyPlacementHolding = [this]() noexcept
    {
        for (const auto hold : placementHold_)
            if (hold > 0)
                return true;
        return false;
    };
    if (activeTotal_ == 0 && roomEnvelope_ < 1.0e-9f && !anyPlacementHolding())
    {
        samplePosition_ += static_cast<std::uint64_t>(numSamples);

        const float idleSmoothing = 1.0f - std::exp(
            -static_cast<float>(numSamples) * inverseSampleRate_ / 0.025f);
        const auto smoothTo = [idleSmoothing](float& value, float target) noexcept
        {
            value += idleSmoothing * (target - value);
            if (std::abs(target - value) < 1.0e-4f)
                value = target;
        };
        smoothTo(smoothedRoom_, blockParameters_.room);
        smoothTo(smoothedGain_, blockParameters_.outputGain);
        smoothTo(smoothedBreath_, blockParameters_.breath);
        smoothTo(smoothedTension_, blockParameters_.tension);
        smoothTo(smoothedExpression_, expression_);
        // Nothing is sounding, so the chunk-rate smoothers can snap straight to
        // their targets. Doing that keeps idle automation independent of how
        // many silent blocks the host happens to send.
        chunkStateValid_ = false;
        updateChunkState(blockParameters_, false);
        smoothTo(voicedDynamic_, chunkResponse_.voicedGain);
        smoothTo(airDynamic_, chunkResponse_.airGain);

        // The tracked tail is below the silence threshold. Clearing it once
        // prevents an inaudible delayed sample from resurfacing after Room is
        // automated while the engine is idle.
        if (roomEnvelope_ != 0.0f)
            clearRoom();
        if (left != nullptr)
            std::fill(left, left + numSamples, 0.0f);
        if (right != nullptr && right != left)
            std::fill(right, right + numSamples, 0.0f);
        // activeTotal_ is already 0 on this path, so publishDisplayState()
        // resolves that same zero voice count itself rather than being told it
        // twice, which is the one case this engine is benchmarked and tuned to
        // render for nearly nothing.
        publishDisplayState(0.0f, 0.0f, numSamples);
        return;
    }

    float blockPeakLeft = 0.0f;
    float blockPeakRight = 0.0f;
    int rendered = 0;

    while (rendered < numSamples)
    {
        // Chunk boundaries are aligned to absolute sample positions, so voice
        // rendering, drift and room geometry are identical no matter how the
        // host slices the buffer.
        const int offset = static_cast<int>(samplePosition_ % static_cast<std::uint64_t>(chunkSize));
        const int count = std::min(numSamples - rendered, chunkSize - offset);
        if (offset == 0)
            updateChunkState(blockParameters_, true);

        for (int i = 0; i < count; ++i)
        {
            const auto index = static_cast<std::size_t>(i);
            mixLeft_[index] = 0.0f;
            mixRight_[index] = 0.0f;
            earlyLeft_[index] = 0.0f;
            earlyRight_[index] = 0.0f;
            airLevelAt_[index] = smoothedBreath_ * airDynamic_;
            tensionAt_[index] = smoothedTension_ * chunkResponse_.sourceTensionScale;
            voicedScaleAt_[index] = (0.88f - 0.24f * smoothedBreath_) * voicedDynamic_;
            smoothedBreath_ += parameterSmoothing_ * (blockParameters_.breath - smoothedBreath_);
            smoothedTension_ += parameterSmoothing_ * (blockParameters_.tension - smoothedTension_);
            voicedDynamic_ += parameterSmoothing_ * (chunkResponse_.voicedGain - voicedDynamic_);
            airDynamic_ += parameterSmoothing_ * (chunkResponse_.airGain - airDynamic_);
        }

        // Each singer's bus carries only what her own voices put there this
        // chunk. A bus is cleared when its singer is about to write to it, and
        // also while her placement line is still holding: renderPlacement()
        // keeps feeding a held singer's line from this bus after her last voice
        // has gone, so a bus left uncleared would push the final chunk of her
        // note back down the line once per chunk and repeat the end of the note
        // until the hold expired.
        for (int singer = 0; singer < singerCount; ++singer)
            if ((singersInUse_ & (1u << singer)) != 0u
                || placementHold_[static_cast<std::size_t>(singer)] > 0)
                std::fill(singerMix_[static_cast<std::size_t>(singer)].begin(),
                          singerMix_[static_cast<std::size_t>(singer)].begin() + count, 0.0f);

        for (int v = 0; v < activeTotal_; ++v)
        {
            Voice& voice = *activeVoices_[static_cast<std::size_t>(v)];
            if (voice.active) // may have finished releasing in an earlier chunk
                renderVoice(voice, blockParameters_, count);
        }

        renderPlacement(count);

        const bool roomAudible = smoothedRoom_ > 1.0e-4f || blockParameters_.room > 1.0e-4f
                              || roomEnvelope_ > 1.0e-9f;
        if (!roomAudible && roomEnvelope_ != 0.0f)
            clearRoom();

        for (int i = 0; i < count; ++i)
        {
            const auto index = static_cast<std::size_t>(i);
            const float dryLeft = mixLeft_[index];
            const float dryRight = mixRight_[index];
            smoothedRoom_ += parameterSmoothing_ * (blockParameters_.room - smoothedRoom_);
            smoothedGain_ += parameterSmoothing_ * (blockParameters_.outputGain - smoothedGain_);
            smoothedExpression_ += parameterSmoothing_ * (expression_ - smoothedExpression_);

            float wetLeft = 0.0f;
            float wetRight = 0.0f;
            if (roomAudible)
                updateRoom(placementSendGain_ * earlyLeft_[index],
                           placementSendGain_ * earlyRight_[index], wetLeft, wetRight);

            // Expression rides the output stage, after the room, so a swell
            // shapes the wet tail with the dry signal instead of pumping it.
            const float gain = smoothedGain_ * smoothedExpression_;
            const float dryScale = 1.0f - 0.12f * smoothedRoom_;
            const float wetScale = 0.72f * smoothedRoom_;
            // The image field is room sound, so it rides the same Room control
            // as the tail. Where a singer stands -- her arrival time, her level
            // and the section's depth -- is geometry and is always on; how much
            // of the room comes back is the knob the player has always had, and
            // Room 0 has always been dry.
            const float outLeft = gain * (dryScale * dryLeft
                                          + wetScale * (earlyLeft_[index] + wetLeft));
            const float outRight = gain * (dryScale * dryRight
                                           + wetScale * (earlyRight_[index] + wetRight));
            blockPeakLeft = std::max(blockPeakLeft, std::abs(outLeft));
            blockPeakRight = std::max(blockPeakRight, std::abs(outRight));

            const int destination = rendered + i;
            if (left != nullptr)
                left[destination] = outLeft;
            if (right != nullptr && right != left)
                right[destination] = outRight;
            else if (right != nullptr)
                right[destination] = 0.5f * (outLeft + outRight);
        }

        samplePosition_ += static_cast<std::uint64_t>(count);
        rendered += count;
    }

    // publishDisplayState() resolves the voice count itself, from the same
    // activeVoices_ list this block already built, instead of paying for a
    // second full scan of every voice slot here.
    publishDisplayState(blockPeakLeft, blockPeakRight, numSamples);
}

void VoiceEngine::publishDisplayState(float blockPeakLeft,
                                      float blockPeakRight, int numSamples) noexcept
{
    const float interval = static_cast<float>(std::max(1, numSamples)) * inverseSampleRate_;
    const float release = smoothingCoefficient(0.28f, interval);
    meterLeft_ = meterFollow(meterLeft_, blockPeakLeft, 1.0f, release);
    meterRight_ = meterFollow(meterRight_, blockPeakRight, 1.0f, release);
    displayLevelLeft_.store(meterLeft_, std::memory_order_relaxed);
    displayLevelRight_.store(meterRight_, std::memory_order_relaxed);

    // Prefer the tract of a sounding voice so the curve breathes with the
    // vibrato and the ensemble drift instead of showing a static target, and
    // count what is still active for the status display. activeVoices_ /
    // activeTotal_ already hold exactly the voices active at the start of this
    // block -- process() builds that list once and nothing can have started
    // since -- so walking it costs at most that many checks instead of always
    // scanning every one of the maxVoices slots; the .active test below still
    // excludes any that finished releasing during this same block.
    const Voice* reference = nullptr;
    int voiceCount = 0;
    for (int i = 0; i < activeTotal_; ++i)
    {
        const Voice& voice = *activeVoices_[static_cast<std::size_t>(i)];
        if (!voice.active)
            continue;
        ++voiceCount;
        if (reference == nullptr || (reference->releasing && !voice.releasing))
            reference = &voice;
    }

    for (int formant = 0; formant < formantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        const float hz = reference != nullptr && reference->formantHz[index] > 1.0f
            ? reference->formantHz[index] : chunkFormantHz_[index];
        const float bandwidth = reference != nullptr
                && reference->formantBandwidth[index] > 1.0f
            ? reference->formantBandwidth[index] : chunkBandwidth_[index];
        const float gain = reference != nullptr && reference->formantGain[index] > 0.0f
            ? reference->formantGain[index] : chunkFormantGain_[index];
        displayFormantHz_[index].store(hz, std::memory_order_relaxed);
        displayFormantBandwidth_[index].store(bandwidth, std::memory_order_relaxed);
        displayFormantGain_[index].store(gain, std::memory_order_relaxed);
    }

    // The idle fallback position is only ever read when nothing is sounding:
    // whenever a reference voice exists, displayVowelX_/Y_ publish its own
    // effective point instead. Resolving the preset anchor and the pad blend
    // is therefore wasted on every block a note is held, which is most of
    // them; skip it entirely unless the fallback is actually going to be used.
    if (reference != nullptr)
    {
        displayVowelX_.store(reference->effectiveVowelX, std::memory_order_relaxed);
        displayVowelY_.store(reference->effectiveVowelY, std::memory_order_relaxed);
    }
    else
    {
        const int vowelIndex = vowelIndexOf(blockParameters_.vowel);
        const VowelPoint anchor = presetVowelPosition(vowelIndex);
        const float morph = clampUnit(blockParameters_.vowelMorph);
        const float baseX = anchor.x
            + morph * (clampUnit(blockParameters_.vowelX) - anchor.x);
        const float baseY = anchor.y
            + morph * (clampUnit(blockParameters_.vowelY) - anchor.y);
        displayVowelX_.store(baseX, std::memory_order_relaxed);
        displayVowelY_.store(baseY, std::memory_order_relaxed);
    }
    displayDynamics_.store(smoothedDynamics_, std::memory_order_relaxed);
    activeVoiceCount_.store(voiceCount, std::memory_order_relaxed);
}

EngineDisplayState VoiceEngine::getDisplayState() const noexcept
{
    EngineDisplayState state;
    for (int formant = 0; formant < formantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        state.formantHz[index] = displayFormantHz_[index].load(std::memory_order_relaxed);
        state.formantBandwidth[index] = displayFormantBandwidth_[index].load(std::memory_order_relaxed);
        state.formantGain[index] = displayFormantGain_[index].load(std::memory_order_relaxed);
    }
    state.levelLeft = displayLevelLeft_.load(std::memory_order_relaxed);
    state.levelRight = displayLevelRight_.load(std::memory_order_relaxed);
    state.vowelX = displayVowelX_.load(std::memory_order_relaxed);
    state.vowelY = displayVowelY_.load(std::memory_order_relaxed);
    state.dynamics = displayDynamics_.load(std::memory_order_relaxed);
    state.sampleRate = displaySampleRate_.load(std::memory_order_relaxed);
    state.activeVoices = activeVoiceCount_.load(std::memory_order_relaxed);
    return state;
}

int VoiceEngine::getActiveVoiceCount() const
{
    return activeVoiceCount_.load(std::memory_order_relaxed);
}

} // namespace vocalor
