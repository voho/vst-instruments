#include "VoiceEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vocalor
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

// Base level of the aspiration noise that is injected at the glottis. Chosen so
// the default Breath setting lands within a fraction of a dB of the 1.0 engine.
constexpr float aspirationLevel = 0.62f;
// Small unfiltered component: not all breath noise is generated below the
// tract, and a little direct air keeps consonantal "hh" detail audible.
constexpr float directAirLevel = 0.055f;
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
    the two prototypes differ in a physically meaningful way rather than through
    an arbitrary harmonic roll-off exponent.
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
} // namespace

VoiceEngine::VoiceEngine() noexcept
{
    setParameters(EngineParameters {});
    // Give the editor a meaningful tract to draw even if it opens before the
    // host has called prepare().
    blockParameters_ = snapshotParameters();
    updateChunkState(blockParameters_, false);
    publishDisplayState(0, 0.0f, 0.0f, 1);
}

void VoiceEngine::prepare(double sampleRate, int maxBlockSize)
{
    if (!std::isfinite(sampleRate))
        sampleRate = 48000.0;
    sampleRate_ = std::clamp(sampleRate, 8000.0, 192000.0);
    displaySampleRate_.store(static_cast<float>(sampleRate_), std::memory_order_relaxed);
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    maxBlockSize_ = std::max(1, maxBlockSize);

    const auto delayFor = [this](float seconds)
    {
        return std::clamp(seconds * static_cast<float>(sampleRate_),
                          8.0f, static_cast<float>(roomBufferSize) * 0.4f);
    };
    roomBaseDelay_ = { delayFor(0.0297f), delayFor(0.0371f),
                       delayFor(0.0411f), delayFor(0.0437f) };
    roomDelay_ = roomBaseDelay_;
    roomLowCutCoefficient_ = std::clamp(twoPi * 110.0f * inverseSampleRate_, 0.0f, 1.0f);
    chunkGainCoefficient_ = 1.0f - std::exp(-static_cast<float>(chunkSize)
                                            * inverseSampleRate_ / 0.020f);

    // Sample-rate-only coefficients. These must be valid before the first
    // process() call because noteOn() already runs a control update.
    parameterSmoothing_ = 1.0f - std::exp(-inverseSampleRate_ / 0.025f);
    airAttackCoefficient_ = 1.0f - std::exp(-inverseSampleRate_ / 0.004f);
    onsetAirMultiplier_ = std::exp(-inverseSampleRate_ / 0.085f);
    scoopMultiplier_ = std::exp(-static_cast<float>(controlPeriod) * inverseSampleRate_ / 0.072f);

    buildTables();
    buildSingerIdentities();
    prepared_ = true;
    reset();
}

void VoiceEngine::reset()
{
    allSoundOff();
    sharedPitchPhase_ = 0.173f;
    sharedRatePhase_ = 0.617f;
    sharedFormantPhase_ = 0.391f;
    samplePosition_ = 0;
    generation_ = 0;
    heldCount_ = 0;
    heldNoteCounts_.fill(0);
    soundingRoot_ = -1;
    lastRootMidi_ = -1;
    blockParameters_ = snapshotParameters();
    smoothedRoom_ = blockParameters_.room;
    smoothedGain_ = blockParameters_.outputGain;
    smoothedBreath_ = blockParameters_.breath;
    smoothedTension_ = blockParameters_.tension;
    smoothedRoomSize_ = clampUnit(blockParameters_.roomSize);
    meterLeft_ = meterRight_ = 0.0f;
    chunkStateValid_ = false;

    // The singer LFOs keep a repeatable but non-aligned ensemble state.
    for (int i = 0; i < singerCount; ++i)
    {
        singers_[static_cast<std::size_t>(i)].driftPhase = wrapPhase(0.071f + 0.137f * static_cast<float>(i));
        singers_[static_cast<std::size_t>(i)].depthPhase = wrapPhase(0.419f + 0.193f * static_cast<float>(i));
        singers_[static_cast<std::size_t>(i)].formantPhase = wrapPhase(0.733f + 0.113f * static_cast<float>(i));
    }

    updateChunkState(blockParameters_, false);
    publishDisplayState(0, 0.0f, 0.0f, 1);
}

void VoiceEngine::setParameters(const EngineParameters& p)
{
    const int profile = p.profile == VoiceProfile::Male ? 1 : 0;
    const int mode = p.mode == PerformanceMode::Choir ? 1 : (p.mode == PerformanceMode::Chord ? 2 : 0);
    const int vowel = p.vowel == Vowel::Ooh ? 1 : (p.vowel == Vowel::Uuh ? 2 : 0);
    const int quality = p.chordQuality == ChordQuality::Minor ? 1 : 0;
    int choir = p.choirSize;
    choir = choir <= 6 ? 4 : (choir <= 10 ? 8 : 12);

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
    return p;
}

void VoiceEngine::buildTables()
{
    for (int i = 0; i < tableSize; ++i)
        sineTable_[static_cast<std::size_t>(i)] = std::sin(twoPi * static_cast<float>(i) / static_cast<float>(tableSize));

    // Sine and cosine at an exact harmonic multiple of the table step. Every
    // harmonic of a 2048-point table lands on a table entry, so the analysis
    // and synthesis below need no library trigonometry at all.
    const auto tableSin = [this](int index) noexcept
    {
        return sineTable_[static_cast<std::size_t>(index & tableMask)];
    };
    const auto tableCos = [this](int index) noexcept
    {
        return sineTable_[static_cast<std::size_t>((index + tableSize / 4) & tableMask)];
    };

    struct Shape
    {
        float openQuotient;
        float speedQuotient;
        float returnQuotient;
    };
    // Lax/breathy versus firmly adducted phonation. Both sets sit inside the
    // ranges reported for sustained singing (OQ 0.4-0.8, SQ 1.5-4).
    constexpr Shape shapes[2] = { { 0.78f, 2.60f, 0.0120f },
                                  { 0.46f, 3.40f, 0.0032f } };

    std::array<std::array<float, maxHarmonics + 1>, 2> cosineCoefficients {};
    std::array<std::array<float, maxHarmonics + 1>, 2> sineCoefficients {};
    std::array<float, tableSize> prototype {};

    for (int shape = 0; shape < 2; ++shape)
    {
        double mean = 0.0;
        for (int i = 0; i < tableSize; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(tableSize);
            const float value = glottalDerivative(t, shapes[shape].openQuotient,
                                                  shapes[shape].speedQuotient,
                                                  shapes[shape].returnQuotient);
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
            cosineCoefficients[static_cast<std::size_t>(shape)][static_cast<std::size_t>(harmonic)] =
                real * normalise;
            sineCoefficients[static_cast<std::size_t>(shape)][static_cast<std::size_t>(harmonic)] =
                imaginary * normalise;
        }

        float energy = 0.0f;
        for (int harmonic = 1; harmonic <= sourceBandHarmonics; ++harmonic)
        {
            const float a = cosineCoefficients[static_cast<std::size_t>(shape)][static_cast<std::size_t>(harmonic)];
            const float b = sineCoefficients[static_cast<std::size_t>(shape)][static_cast<std::size_t>(harmonic)];
            energy += a * a + b * b;
        }
        energy = std::sqrt(0.5f * energy);
        const float scale = energy > 0.0f
            ? sourceBandRms[static_cast<std::size_t>(shape)] / energy : 1.0f;
        for (int harmonic = 1; harmonic <= maxHarmonics; ++harmonic)
        {
            cosineCoefficients[static_cast<std::size_t>(shape)][static_cast<std::size_t>(harmonic)] *= scale;
            sineCoefficients[static_cast<std::size_t>(shape)][static_cast<std::size_t>(harmonic)] *= scale;
        }
    }

    for (int level = 0; level < tableLevels; ++level)
    {
        auto& table = glottalTables_[static_cast<std::size_t>(level)];
        table.fill(0.0f);
        const int harmonics = harmonicsPerLevel[static_cast<std::size_t>(level)];
        for (int shape = 0; shape < 2; ++shape)
        {
            for (int harmonic = 1; harmonic <= harmonics; ++harmonic)
            {
                const float a = cosineCoefficients[static_cast<std::size_t>(shape)][static_cast<std::size_t>(harmonic)];
                const float b = sineCoefficients[static_cast<std::size_t>(shape)][static_cast<std::size_t>(harmonic)];
                int index = 0;
                for (int i = 0; i < tableSize; ++i)
                {
                    table[static_cast<std::size_t>(2 * i + shape)] += a * tableCos(index) + b * tableSin(index);
                    index += harmonic;
                }
            }
        }
    }
}

void VoiceEngine::buildSingerIdentities()
{
    for (int i = 0; i < singerCount; ++i)
    {
        auto& singer = singers_[static_cast<std::size_t>(i)];
        const std::uint32_t base = 0x9e3779b9u * static_cast<std::uint32_t>(i + 1);
        singer.detuneCents = 5.6f * hashFloat(base + 1u);
        singer.anatomy = 0.045f * hashFloat(base + 2u);
        const float position = singerCount > 1 ? 2.0f * static_cast<float>(i) / static_cast<float>(singerCount - 1) - 1.0f : 0.0f;
        singer.pan = std::clamp(0.82f * position + 0.12f * hashFloat(base + 3u), -1.0f, 1.0f);
        singer.onsetOffset = std::max(0.0f, 0.009f + 0.009f * hashFloat(base + 4u));
        singer.vibratoRate = 4.65f + 0.72f * (0.5f + 0.5f * hashFloat(base + 5u));
        singer.vibratoDepth = 0.76f + 0.42f * (0.5f + 0.5f * hashFloat(base + 6u));
        singer.driftIncrement = (0.025f + 0.075f * (0.5f + 0.5f * hashFloat(base + 7u))) * inverseSampleRate_;
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
}

int VoiceEngine::voicesForMode(const EngineParameters& p) const noexcept
{
    if (p.mode == PerformanceMode::Chord)
        return 6;
    if (p.mode == PerformanceMode::Choir)
        return p.choirSize <= 6 ? 4 : (p.choirSize <= 10 ? 8 : 12);
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
        updateVoiceControl(voice, p);
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
        prepare(sampleRate_, maxBlockSize_);

    midiNote = std::clamp(midiNote, 0, 127);
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    const EngineParameters p = snapshotParameters();
    updateChunkState(p, false);

    const bool hadHeldNote = heldCount_ > 0;
    pushHeldNote(midiNote);

    // Legato: an overlapping note bends the sounding voices instead of
    // restarting them, which is how a singer actually moves between pitches.
    // A repeat of the pitch already sounding is not a move, so it falls
    // through to the retrigger below and gets a fresh attack.
    if (p.legato && hadHeldNote && midiNote != soundingRoot_
        && retuneForLegato(midiNote, p))
    {
        soundingRoot_ = midiNote;
        lastRootMidi_ = midiNote;
        activeVoiceCount_.store(countActiveVoices(), std::memory_order_relaxed);
        return;
    }

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
    const float groupGain = modeTrim / std::sqrt(static_cast<float>(total));
    for (int singer = 0; singer < total; ++singer)
    {
        const int slot = findFreeVoice();
        if (slot < 0)
            break;
        const int soundingMidi = p.mode == PerformanceMode::Chord ? chordMidiForSinger(midiNote, singer, p) : midiNote;
        initialiseVoice(voices_[static_cast<std::size_t>(slot)], midiNote, soundingMidi,
                        singer, velocity, groupGain, total, glideFromCents, p);
    }

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
    voice.amplitudeGain = groupGain * velocity * (0.67f + 0.33f * std::sqrt(velocity));
    voice.groupGain = groupGain;
    voice.glideCents = glideFromCents;
    voice.vibratoPhase = wrapPhase(0.173f * static_cast<float>(singerIndex % singerCount));
    voice.noiseState = hash32(static_cast<std::uint32_t>(generation_) ^
                              static_cast<std::uint32_t>(rootMidi * 977 + singerIndex * 131));
    voice.phase = 0.5f + 0.12f * hashFloat(voice.noiseState + 17u);
    const auto& singer = singers_[static_cast<std::size_t>(voice.singer)];
    const float delay = singerTotal == 1 ? 0.0f : singer.onsetOffset * p.humanize;
    voice.delaySamples = std::max(0, static_cast<int>(delay * static_cast<float>(sampleRate_)));
    voice.pitchScoop = -(7.0f + 19.0f * (0.5f + 0.5f * hashFloat(voice.noiseState + 23u))) * (0.25f + 0.75f * p.humanize);
    const float onsetStart = 0.050f + 0.020f * (0.5f + 0.5f * hashFloat(voice.noiseState + 29u));
    const float onsetEnd = onsetStart + 0.065f + 0.060f * (0.5f + 0.5f * hashFloat(voice.noiseState + 31u));
    const float onsetSpan = std::max(0.001f, onsetEnd - onsetStart);
    voice.onsetMix = -onsetStart / onsetSpan;
    voice.onsetMixStep = inverseSampleRate_ / onsetSpan;
    voice.controlCountdown = 0;
    updateVoiceControl(voice, p);
    voice.phaseIncrement = voice.targetPhaseIncrement;
}

void VoiceEngine::noteOff(int midiNote)
{
    midiNote = std::clamp(midiNote, 0, 127);
    const auto heldState = releaseHeldNote(midiNote);
    // Another controller still holds this pitch, so it keeps sounding.
    if (heldState == HeldNoteState::StillHeld)
        return;
    const bool wasHeld = heldState == HeldNoteState::Released;
    const EngineParameters p = snapshotParameters();

    // Releasing the top of a legato phrase falls back to the note underneath.
    if (p.legato && wasHeld && soundingRoot_ == midiNote && heldCount_ > 0)
    {
        const int previous = heldNotes_[static_cast<std::size_t>(heldCount_ - 1)];
        updateChunkState(p, false);
        if (retuneForLegato(previous, p))
        {
            soundingRoot_ = previous;
            lastRootMidi_ = previous;
            return;
        }
    }

    for (auto& voice : voices_)
        if (voice.active && voice.rootMidi == midiNote)
            voice.releasing = true;
    if (soundingRoot_ == midiNote)
        soundingRoot_ = -1;
}

void VoiceEngine::allNotesOff()
{
    heldCount_ = 0;
    heldNoteCounts_.fill(0);
    soundingRoot_ = -1;
    for (auto& voice : voices_)
        if (voice.active)
            voice.releasing = true;
}

void VoiceEngine::allSoundOff() noexcept
{
    for (auto& voice : voices_)
        silenceVoice(voice);
    clearRoom();
    heldCount_ = 0;
    heldNoteCounts_.fill(0);
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
    voice.sourceTilt = 0.0f;
    for (auto& resonator : voice.tract)
        resonator.clear();
    for (auto& resonator : voice.early)
        resonator.clear();
}

float VoiceEngine::midiToHz(int midiNote) noexcept
{
    return 440.0f * std::exp2((static_cast<float>(midiNote) - 69.0f) / 12.0f);
}

void VoiceEngine::updateChunkState(const EngineParameters& p, bool advanceSmoothers)
{
    static constexpr float femaleBw[formantCount] { 75.0f, 90.0f, 125.0f, 185.0f, 260.0f };
    static constexpr float maleBw[formantCount] { 68.0f, 82.0f, 112.0f, 165.0f, 235.0f };
    static constexpr float baseGain[formantCount] { 1.0f, 0.66f, 0.34f, 0.18f, 0.095f };

    const bool male = p.profile == VoiceProfile::Male;
    const int vowelIndex = p.vowel == Vowel::Ooh ? 1 : (p.vowel == Vowel::Uuh ? 2 : 0);

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

    const float shift = formantShiftRatio(p.formantShift);
    const float bandwidthScale = (1.18f - 0.30f * p.resonance) * (1.0f + 0.12f * p.breath);
    const float maximumBandwidth = 0.25f * static_cast<float>(sampleRate_);
    for (int formant = 0; formant < formantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        chunkFormantHz_[index] = target[index] * shift;

        const float gainTarget = baseGain[index] * (0.72f + 0.50f * p.resonance)
            * (1.0f + (formant == 2 ? 0.10f * p.tension : 0.0f));
        if (!chunkStateValid_)
            chunkFormantGain_[index] = gainTarget;
        else if (advanceSmoothers)
            chunkFormantGain_[index] += chunkGainCoefficient_ * (gainTarget - chunkFormantGain_[index]);

        // Wider formants track a shifted tract so the resonances keep their
        // shape instead of turning needle-thin when the voice is made small.
        const float bandwidth = std::clamp(
            (male ? maleBw[index] : femaleBw[index]) * bandwidthScale * std::sqrt(shift),
            20.0f, maximumBandwidth);
        chunkBandwidth_[index] = bandwidth;
        const float radius = std::exp(-pi * bandwidth * inverseSampleRate_);
        chunkPoleScale_[index] = 2.0f * radius;
        chunkA2_[index] = -radius * radius;
        chunkB0_[index] = std::max(0.00005f, 1.0f - radius);

        if (formant < 2)
        {
            const float earlyBandwidth = std::clamp(bandwidth * 1.75f, 20.0f, maximumBandwidth);
            const float earlyRadius = std::exp(-pi * earlyBandwidth * inverseSampleRate_);
            earlyPoleScale_[index] = 2.0f * earlyRadius;
            earlyA2_[index] = -earlyRadius * earlyRadius;
            earlyB0_[index] = std::max(0.00005f, 1.0f - earlyRadius);
        }
    }

    const float glideTime = glideTimeSeconds(p.glide);
    chunkGlideDecay_ = glideTime > 0.0f
        ? std::exp(-static_cast<float>(controlPeriod) * inverseSampleRate_ / glideTime)
        : 0.0f;

    // Sub-0.15 Hz ensemble drift, evaluated from the absolute sample position so
    // the result never depends on how the host splits its buffers.
    const double position = static_cast<double>(samplePosition_);
    const auto absolutePhase = [position](double origin, double increment) noexcept
    {
        const double phase = origin + increment * position;
        return static_cast<float>(phase - std::floor(phase));
    };
    sharedPitchPhase_ = absolutePhase(0.173, 0.047 / sampleRate_);
    sharedRatePhase_ = absolutePhase(0.617, 0.019 / sampleRate_);
    sharedFormantPhase_ = absolutePhase(0.391, 0.011 / sampleRate_);
    for (int singerIndex = 0; singerIndex < singerCount; ++singerIndex)
    {
        auto& singer = singers_[static_cast<std::size_t>(singerIndex)];
        singer.driftPhase = absolutePhase(0.071 + 0.137 * static_cast<double>(singerIndex),
                                          static_cast<double>(singer.driftIncrement));
        singer.depthPhase = absolutePhase(0.419 + 0.193 * static_cast<double>(singerIndex),
                                          static_cast<double>(singer.depthIncrement));
        singer.formantPhase = absolutePhase(0.733 + 0.113 * static_cast<double>(singerIndex),
                                            static_cast<double>(singer.formantIncrement));
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
    chunkStateValid_ = true;
}

void VoiceEngine::updateVoiceControl(Voice& voice, const EngineParameters& p)
{
    const auto& singer = singers_[static_cast<std::size_t>(voice.singer)];
    const float singerDrift = sine(singer.driftPhase);
    const float depthDrift = sine(singer.depthPhase);
    const float formantDrift = sine(singer.formantPhase);
    const float sharedPitch = sine(sharedPitchPhase_);
    const float sharedRate = sine(sharedRatePhase_);
    const float sharedFormant = sine(sharedFormantPhase_);

    const float ageSeconds = static_cast<float>(voice.ageSamples) * inverseSampleRate_;
    const float vibratoFade = smoothStep((ageSeconds - 0.16f) / 0.34f);
    const float vibratoRate = singer.vibratoRate *
        (1.0f + p.humanize * (0.055f * singerDrift + 0.018f * sharedRate));
    const float vibratoDepth = p.vibrato * singer.vibratoDepth *
        (20.0f + 7.0f * p.humanize * depthDrift) * vibratoFade;
    // Integrating the rate keeps the vibrato phase exact for arbitrarily long
    // notes. Recomputing age * rate amplified any rate drift by the note age
    // and lost sub-sample precision once the age exceeded 2^24 samples.
    const float elapsedSeconds = static_cast<float>(voice.ageSamples - voice.lastControlAge)
        * inverseSampleRate_;
    voice.lastControlAge = voice.ageSamples;
    voice.vibratoPhase = wrapPhase(voice.vibratoPhase + elapsedSeconds * vibratoRate);

    // Two nested smoothers give the pitch noise a 1/f-like spectrum instead of
    // the single-pole tilt a lone follower produces.
    const float random = randomBipolar(voice.noiseState);
    voice.jitter += (random - voice.jitter) * (0.016f + 0.014f * p.humanize);
    voice.jitterSlow += (voice.jitter - voice.jitterSlow) * 0.035f;
    const float jitterCents = p.humanize * (1.15f * voice.jitter + 1.35f * voice.jitterSlow);
    const float identityCents = singer.detuneCents * p.humanize;
    const float wanderCents = p.humanize * (2.1f * singerDrift + 0.75f * sharedPitch);
    voice.pitchScoop *= scoopMultiplier_;
    voice.glideCents *= chunkGlideDecay_;
    const float cents = voice.pitchScoop + voice.glideCents + identityCents + wanderCents
                      + jitterCents + vibratoDepth * sine(voice.vibratoPhase);
    const float frequency = voice.baseFrequency * std::exp2(cents * (1.0f / 1200.0f));
    voice.targetPhaseIncrement = std::clamp(frequency * inverseSampleRate_, 0.0f, 0.48f);
    voice.phaseIncrementStep = (voice.targetPhaseIncrement - voice.phaseIncrement)
        / static_cast<float>(controlPeriod);

    const int permissible = std::max(1, static_cast<int>(0.46f * static_cast<float>(sampleRate_) / std::max(frequency, 1.0f)));
    voice.tableLevel = 0;
    for (int level = 1; level < tableLevels; ++level)
        if (harmonicsPerLevel[static_cast<std::size_t>(level)] <= permissible)
            voice.tableLevel = level;

    // Vocal effort changes the source spectral slope far more than it changes
    // level: a soft note is dull as well as quiet.
    const float effort = std::clamp(0.42f * voice.velocity + 0.58f * p.tension, 0.0f, 1.0f);
    const float tiltCorner = 1400.0f * std::exp2(4.0f * effort);
    voice.tiltCoefficient = std::clamp(twoPi * tiltCorner * inverseSampleRate_, 0.0f, 1.0f);
    voice.irregularity = voice.midiNote < 52
        ? (1.0f - p.tension) * p.humanize * 0.035f : 0.0f;
    voice.airShape = aspirationLevel * (0.22f + 0.78f * voice.onsetAir);

    const float anatomy = 1.0f + p.humanize * (singer.anatomy
        + 0.0045f * formantDrift + 0.0022f * sharedFormant);
    const float highAmount = std::max(0.0f, static_cast<float>(voice.midiNote - 69));
    const float upperLimit = 0.465f * static_cast<float>(sampleRate_);
    for (int formant = 0; formant < formantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        const float scale = anatomy + p.humanize * singer.formantScale[index];
        const float highTune = 1.0f + highAmount * (formant == 0 ? 0.0032f : (formant == 1 ? 0.00125f : 0.0003f));
        const float targetHz = chunkFormantHz_[index] * scale * highTune;
        if (voice.formantHz[index] <= 1.0f)
            voice.formantHz[index] = targetHz;
        else
            voice.formantHz[index] += 0.105f * (targetHz - voice.formantHz[index]);

        const float bounded = std::clamp(voice.formantHz[index], 25.0f, upperLimit);
        const float cosine = cosineFromCycles(bounded * inverseSampleRate_);
        voice.tract[index].a1 = chunkPoleScale_[index] * cosine;
        if (formant < 2)
            voice.early[index].a1 = earlyPoleScale_[index] * cosine;
    }

    const float pan = std::clamp(singer.pan * p.spread * (voice.singer == 0 && p.mode == PerformanceMode::Solo ? 0.18f : 1.0f), -1.0f, 1.0f);
    const float targetPanLeft = std::sqrt(0.5f * (1.0f - pan));
    const float targetPanRight = std::sqrt(0.5f * (1.0f + pan));
    if (!voice.controlInitialised)
    {
        voice.panLeft = targetPanLeft;
        voice.panRight = targetPanRight;
        voice.controlInitialised = true;
    }
    else
    {
        // Glide like the formant targets so a spread automation jump cannot
        // step the pan gains audibly at the control rate.
        voice.panLeft += 0.105f * (targetPanLeft - voice.panLeft);
        voice.panRight += 0.105f * (targetPanRight - voice.panRight);
    }
}

float VoiceEngine::glottalPair(int level, float phase, float tension) const noexcept
{
    const auto& table = glottalTables_[static_cast<std::size_t>(level)];
    const float position = phase * static_cast<float>(tableSize);
    const int truncated = static_cast<int>(position);
    const int index = truncated & tableMask;
    const float fraction = position - static_cast<float>(truncated);
    const auto low = static_cast<std::size_t>(2 * index);
    const auto high = static_cast<std::size_t>(2 * ((index + 1) & tableMask));
    const float a = table[low] + tension * (table[low + 1] - table[low]);
    const float b = table[high] + tension * (table[high + 1] - table[high]);
    return a + fraction * (b - a);
}

float VoiceEngine::sine(float phase) const noexcept
{
    phase = wrapPhase(phase);
    const float position = phase * static_cast<float>(tableSize);
    const int truncated = static_cast<int>(position);
    const int index = truncated & tableMask;
    const float fraction = position - static_cast<float>(truncated);
    const float a = sineTable_[static_cast<std::size_t>(index)];
    const float b = sineTable_[static_cast<std::size_t>((index + 1) & tableMask)];
    return a + fraction * (b - a);
}

float VoiceEngine::cosineFromCycles(float cycles) const noexcept
{
    float phase = cycles + 0.25f;
    if (phase >= 1.0f)
        phase -= 1.0f;
    const float position = phase * static_cast<float>(tableSize);
    const int truncated = static_cast<int>(position);
    const int index = truncated & tableMask;
    const float fraction = position - static_cast<float>(truncated);
    const float a = sineTable_[static_cast<std::size_t>(index)];
    const float b = sineTable_[static_cast<std::size_t>((index + 1) & tableMask)];
    return a + fraction * (b - a);
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
                             roomEnvelope_ * 0.999f);
    roomDampingLeft_ += 0.28f * (rawLeft - roomDampingLeft_);
    roomDampingRight_ += 0.28f * (rawRight - roomDampingRight_);

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

void VoiceEngine::renderVoice(Voice& voice, const EngineParameters& p, int count)
{
    float phase = voice.phase;
    float phaseIncrement = voice.phaseIncrement;
    float envelope = voice.envelope;
    float airEnvelope = voice.airEnvelope;
    float onsetAir = voice.onsetAir;
    float shimmer = voice.shimmer;
    float lastNoise = voice.lastNoise;
    float sourceTilt = voice.sourceTilt;
    float onsetMix = voice.onsetMix;
    std::uint32_t noiseState = voice.noiseState;
    bool alternateCycle = voice.alternateCycle;
    std::uint64_t age = voice.ageSamples;

    const float amplitude = voice.amplitudeGain;
    const float onsetMixStep = voice.onsetMixStep;
    const bool releasing = voice.releasing;

    float incrementStep = voice.phaseIncrementStep;
    float tilt = voice.tiltCoefficient;
    float irregularity = voice.irregularity;
    float airShape = voice.airShape;
    float panLeft = voice.panLeft;
    float panRight = voice.panRight;
    int level = voice.tableLevel;

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
            updateVoiceControl(voice, p);
            noiseState = voice.noiseState;
            voice.controlCountdown = controlPeriod;
            incrementStep = voice.phaseIncrementStep;
            tilt = voice.tiltCoefficient;
            irregularity = voice.irregularity;
            airShape = voice.airShape;
            panLeft = voice.panLeft;
            panRight = voice.panRight;
            level = voice.tableLevel;
        }

        phaseIncrement += incrementStep;
        phase += phaseIncrement;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            alternateCycle = !alternateCycle;
        }

        if (releasing)
        {
            envelope *= releaseMultiplier_;
            airEnvelope *= airReleaseMultiplier_;
        }
        else
        {
            envelope += (1.0f - envelope) * attackCoefficient_;
            airEnvelope += (1.0f - airEnvelope) * airAttackCoefficient_;
        }
        onsetAir *= onsetAirMultiplier_;

        float glottal = glottalPair(level, phase, tensionAt_[static_cast<std::size_t>(i)]);
        glottal *= 1.0f + (alternateCycle ? irregularity : -irregularity);
        sourceTilt += tilt * (glottal - sourceTilt);

        const float noise = randomBipolar(noiseState);
        shimmer += (noise - shimmer) * 0.006f;
        const float highNoise = noise - 0.86f * lastNoise;
        lastNoise = noise;

        // Aspiration is generated at the glottis, so it belongs in the source
        // and is shaped by the very same tract as the voiced excitation. That
        // is both more faithful and two resonators per voice cheaper than the
        // separate noise filter the 1.0 engine used.
        const float breath = breathAt_[static_cast<std::size_t>(i)];
        const float airDrive = breath * airShape * airEnvelope;
        const float voicedDrive = envelope * voicedScaleAt_[static_cast<std::size_t>(i)]
            * (1.0f + shimmerDepth_ * shimmer);
        // The vanishingly small bias parks the recursive tract states on a
        // normal-range fixed point. That keeps the filters out of denormal
        // territory on hosts that do not set flush-to-zero for us, without the
        // per-tick compare-and-select that costs a third of the voice budget.
        const float source = sourceTilt * voicedDrive + highNoise * airDrive + denormalBias;

        float tract = 0.0f;
        onsetMix += onsetMixStep;
        if (!voice.onsetComplete && onsetMix < 1.0f)
        {
            const float clamped = onsetMix > 0.0f ? onsetMix : 0.0f;
            const float mix = clamped * clamped * (3.0f - 2.0f * clamped);
            float early = 0.0f;
            for (int f = 0; f < 2; ++f)
            {
                const auto index = static_cast<std::size_t>(f);
                early += chunkFormantGain_[index]
                    * voice.early[index].tick(source, earlyB0_[index], earlyA2_[index]);
            }
            float full = 0.0f;
            for (int f = 0; f < formantCount; ++f)
            {
                const auto index = static_cast<std::size_t>(f);
                full += chunkFormantGain_[index]
                    * voice.tract[index].tick(source, chunkB0_[index], chunkA2_[index]);
            }
            tract = early + mix * (full - early);
        }
        else
        {
            // Age only grows within a note, so once the onset crossfade reaches
            // its end the early stage cancels out of the mix exactly and its
            // resonators can stop ticking for the rest of the note.
            voice.onsetComplete = true;
            for (int f = 0; f < formantCount; ++f)
            {
                const auto index = static_cast<std::size_t>(f);
                tract += chunkFormantGain_[index]
                    * voice.tract[index].tick(source, chunkB0_[index], chunkA2_[index]);
            }
        }

        const float output = amplitude * (0.62f * tract + directAirLevel * highNoise * airDrive);
        mixLeft_[static_cast<std::size_t>(i)] += output * panLeft;
        mixRight_[static_cast<std::size_t>(i)] += output * panRight;
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
    voice.onsetMix = onsetMix;
    voice.noiseState = noiseState;
    voice.alternateCycle = alternateCycle;
    voice.ageSamples = age;
    voice.phaseIncrementStep = incrementStep;
}

void VoiceEngine::process(float* left, float* right, int numSamples)
{
    if (numSamples <= 0)
        return;
    if (!prepared_)
        prepare(sampleRate_, std::max(maxBlockSize_, numSamples));

    blockParameters_ = snapshotParameters();
    attackCoefficient_ = 1.0f - std::exp(-inverseSampleRate_ / (0.010f + 0.018f * blockParameters_.humanize));
    releaseMultiplier_ = std::exp(-inverseSampleRate_ / (0.105f + 0.19f * blockParameters_.humanize));
    airReleaseMultiplier_ = releaseMultiplier_ * releaseMultiplier_;
    shimmerDepth_ = 0.026f * blockParameters_.humanize;

    // Voices only start in noteOn(), which the audio thread calls between
    // process() calls, so the set of sounding voices is fixed for this block.
    // Collecting it once keeps the per-sample loop from scanning all voice
    // slots (and touching their cold cache lines) when only a few are in use.
    activeTotal_ = 0;
    for (auto& voice : voices_)
        if (voice.active)
            activeVoices_[static_cast<std::size_t>(activeTotal_++)] = &voice;

    // Once every voice has ended and the room tail has audibly rung out, the
    // block renders exact digital silence. Advance the state that remains
    // observable at the next note without paying the full per-sample cost.
    if (activeTotal_ == 0 && roomEnvelope_ < 1.0e-9f)
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
        // Nothing is sounding, so the chunk-rate smoothers can snap straight to
        // their targets. Doing that keeps idle automation independent of how
        // many silent blocks the host happens to send.
        chunkStateValid_ = false;
        updateChunkState(blockParameters_, false);

        // The tracked tail is below the silence threshold. Clearing it once
        // prevents an inaudible delayed sample from resurfacing after Room is
        // automated while the engine is idle.
        if (roomEnvelope_ != 0.0f)
            clearRoom();
        if (left != nullptr)
            std::fill(left, left + numSamples, 0.0f);
        if (right != nullptr && right != left)
            std::fill(right, right + numSamples, 0.0f);
        activeVoiceCount_.store(0, std::memory_order_relaxed);
        publishDisplayState(0, 0.0f, 0.0f, numSamples);
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
            breathAt_[index] = smoothedBreath_;
            tensionAt_[index] = smoothedTension_;
            voicedScaleAt_[index] = 0.88f - 0.24f * smoothedBreath_;
            smoothedBreath_ += parameterSmoothing_ * (blockParameters_.breath - smoothedBreath_);
            smoothedTension_ += parameterSmoothing_ * (blockParameters_.tension - smoothedTension_);
        }

        for (int v = 0; v < activeTotal_; ++v)
        {
            Voice& voice = *activeVoices_[static_cast<std::size_t>(v)];
            if (voice.active) // may have finished releasing in an earlier chunk
                renderVoice(voice, blockParameters_, count);
        }

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

            float wetLeft = 0.0f;
            float wetRight = 0.0f;
            if (roomAudible)
                updateRoom(dryLeft, dryRight, wetLeft, wetRight);

            const float dryScale = 1.0f - 0.12f * smoothedRoom_;
            const float wetScale = 0.72f * smoothedRoom_;
            const float outLeft = smoothedGain_ * (dryScale * dryLeft + wetScale * wetLeft);
            const float outRight = smoothedGain_ * (dryScale * dryRight + wetScale * wetRight);
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

    const int count = countActiveVoices();
    activeVoiceCount_.store(count, std::memory_order_relaxed);
    publishDisplayState(count, blockPeakLeft, blockPeakRight, numSamples);
}

void VoiceEngine::publishDisplayState(int voiceCount, float blockPeakLeft,
                                      float blockPeakRight, int numSamples) noexcept
{
    const float interval = static_cast<float>(std::max(1, numSamples)) * inverseSampleRate_;
    const float release = smoothingCoefficient(0.28f, interval);
    meterLeft_ = meterFollow(meterLeft_, blockPeakLeft, 1.0f, release);
    meterRight_ = meterFollow(meterRight_, blockPeakRight, 1.0f, release);
    displayLevelLeft_.store(meterLeft_, std::memory_order_relaxed);
    displayLevelRight_.store(meterRight_, std::memory_order_relaxed);

    // Prefer the tract of a sounding voice so the curve breathes with the
    // vibrato and the ensemble drift instead of showing a static target.
    const Voice* reference = nullptr;
    for (const auto& voice : voices_)
    {
        if (!voice.active)
            continue;
        if (reference == nullptr || (reference->releasing && !voice.releasing))
            reference = &voice;
        if (!voice.releasing)
            break;
    }

    for (int formant = 0; formant < formantCount; ++formant)
    {
        const auto index = static_cast<std::size_t>(formant);
        const float hz = reference != nullptr && reference->formantHz[index] > 1.0f
            ? reference->formantHz[index] : chunkFormantHz_[index];
        displayFormantHz_[index].store(hz, std::memory_order_relaxed);
        displayFormantBandwidth_[index].store(chunkBandwidth_[index], std::memory_order_relaxed);
        displayFormantGain_[index].store(chunkFormantGain_[index], std::memory_order_relaxed);
    }

    const int vowelIndex = blockParameters_.vowel == Vowel::Ooh
        ? 1 : (blockParameters_.vowel == Vowel::Uuh ? 2 : 0);
    const VowelPoint anchor = presetVowelPosition(vowelIndex);
    const float morph = clampUnit(blockParameters_.vowelMorph);
    displayVowelX_.store(anchor.x + morph * (clampUnit(blockParameters_.vowelX) - anchor.x),
                         std::memory_order_relaxed);
    displayVowelY_.store(anchor.y + morph * (clampUnit(blockParameters_.vowelY) - anchor.y),
                         std::memory_order_relaxed);
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
    state.sampleRate = displaySampleRate_.load(std::memory_order_relaxed);
    state.activeVoices = activeVoiceCount_.load(std::memory_order_relaxed);
    return state;
}

int VoiceEngine::getActiveVoiceCount() const
{
    return activeVoiceCount_.load(std::memory_order_relaxed);
}

} // namespace vocalor
