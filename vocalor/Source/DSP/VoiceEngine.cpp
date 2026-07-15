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
} // namespace

VoiceEngine::VoiceEngine() noexcept
{
    setParameters(EngineParameters {});
}

void VoiceEngine::prepare(double sampleRate, int maxBlockSize)
{
    if (!std::isfinite(sampleRate))
        sampleRate = 48000.0;
    sampleRate_ = std::clamp(sampleRate, 8000.0, 192000.0);
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    maxBlockSize_ = std::max(1, maxBlockSize);

    const auto delayFor = [this](float seconds)
    {
        return std::clamp(static_cast<int>(seconds * static_cast<float>(sampleRate_) + 0.5f),
                          1, roomBufferSize - 1);
    };
    roomDelayA_ = delayFor(0.0297f);
    roomDelayB_ = delayFor(0.0371f);
    roomDelayC_ = delayFor(0.0411f);
    roomDelayD_ = delayFor(0.0437f);

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
    generation_ = 0;
    blockParameters_ = snapshotParameters();
    smoothedRoom_ = blockParameters_.room;
    smoothedGain_ = blockParameters_.outputGain;

    // The singer LFOs keep a repeatable but non-aligned ensemble state.
    for (int i = 0; i < singerCount; ++i)
    {
        singers_[static_cast<std::size_t>(i)].driftPhase = wrapPhase(0.071f + 0.137f * static_cast<float>(i));
        singers_[static_cast<std::size_t>(i)].depthPhase = wrapPhase(0.419f + 0.193f * static_cast<float>(i));
        singers_[static_cast<std::size_t>(i)].formantPhase = wrapPhase(0.733f + 0.113f * static_cast<float>(i));
    }
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
    return p;
}

void VoiceEngine::buildTables()
{
    static constexpr std::array<int, tableLevels> harmonics { 1, 2, 4, 8, 16, 32, 64, 128, 256 };
    for (int i = 0; i < tableSize; ++i)
        sineTable_[static_cast<std::size_t>(i)] = std::sin(twoPi * static_cast<float>(i) / static_cast<float>(tableSize));

    for (int level = 0; level < tableLevels; ++level)
    {
        auto& soft = softTables_[static_cast<std::size_t>(level)];
        auto& tense = tenseTables_[static_cast<std::size_t>(level)];
        soft.fill(0.0f);
        tense.fill(0.0f);
        for (int harmonic = 1; harmonic <= harmonics[static_cast<std::size_t>(level)]; ++harmonic)
        {
            const float h = static_cast<float>(harmonic);
            // Two differentiated glottal-flow spectra: rounded/breathy and firmly adducted.
            const float softAmplitude = std::pow(h, -1.72f) * std::exp(-0.0015f * h);
            const float tenseAmplitude = std::pow(h, -1.15f) * std::exp(-0.0007f * h);
            const float phaseBias = -0.32f / (1.0f + 0.08f * h);
            for (int i = 0; i < tableSize; ++i)
            {
                const float phase = twoPi * h * static_cast<float>(i) / static_cast<float>(tableSize) + phaseBias;
                soft[static_cast<std::size_t>(i)] += softAmplitude * std::sin(phase);
                tense[static_cast<std::size_t>(i)] += tenseAmplitude * std::sin(phase);
            }
        }
        float softPeak = 0.0f;
        float tensePeak = 0.0f;
        for (int i = 0; i < tableSize; ++i)
        {
            softPeak = std::max(softPeak, std::abs(soft[static_cast<std::size_t>(i)]));
            tensePeak = std::max(tensePeak, std::abs(tense[static_cast<std::size_t>(i)]));
        }
        const float softScale = softPeak > 0.0f ? 0.92f / softPeak : 1.0f;
        const float tenseScale = tensePeak > 0.0f ? 0.92f / tensePeak : 1.0f;
        for (int i = 0; i < tableSize; ++i)
        {
            soft[static_cast<std::size_t>(i)] *= softScale;
            tense[static_cast<std::size_t>(i)] *= tenseScale;
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
    const int total = voicesForMode(p);

    // A repeated MIDI root is a true retrigger; old ensemble members cannot leak into it.
    for (auto& voice : voices_)
        if (voice.active && voice.rootMidi == midiNote)
            silenceVoice(voice);
    makeRoomFor(total);

    ++generation_;
    const float modeTrim = total == 1 ? 0.88f : (p.mode == PerformanceMode::Chord ? 0.61f : 0.72f);
    const float groupGain = modeTrim / std::sqrt(static_cast<float>(total));
    for (int singer = 0; singer < total; ++singer)
    {
        const int slot = findFreeVoice();
        if (slot < 0)
            break;
        const int soundingMidi = p.mode == PerformanceMode::Chord ? chordMidiForSinger(midiNote, singer, p) : midiNote;
        initialiseVoice(voices_[static_cast<std::size_t>(slot)], midiNote, soundingMidi,
                        singer, velocity, groupGain, total, p);
    }

    int count = 0;
    for (const auto& voice : voices_)
        count += voice.active ? 1 : 0;
    activeVoiceCount_.store(count, std::memory_order_relaxed);
}

void VoiceEngine::initialiseVoice(Voice& voice, int rootMidi, int soundingMidi, int singerIndex,
                                  float velocity, float groupGain, int singerTotal,
                                  const EngineParameters& p)
{
    voice = Voice {};
    voice.active = true;
    voice.rootMidi = rootMidi;
    voice.midiNote = soundingMidi;
    voice.singer = singerIndex % singerCount;
    voice.generation = generation_;
    voice.velocity = velocity;
    voice.groupGain = groupGain;
    voice.noiseState = hash32(static_cast<std::uint32_t>(generation_) ^
                              static_cast<std::uint32_t>(rootMidi * 977 + singerIndex * 131));
    voice.phase = 0.5f + 0.12f * hashFloat(voice.noiseState + 17u);
    const auto& singer = singers_[static_cast<std::size_t>(voice.singer)];
    const float delay = singerTotal == 1 ? 0.0f : singer.onsetOffset * p.humanize;
    voice.delaySamples = std::max(0, static_cast<int>(delay * static_cast<float>(sampleRate_)));
    voice.pitchScoop = -(7.0f + 19.0f * (0.5f + 0.5f * hashFloat(voice.noiseState + 23u))) * (0.25f + 0.75f * p.humanize);
    voice.fullStageStart = 0.050f + 0.020f * (0.5f + 0.5f * hashFloat(voice.noiseState + 29u));
    voice.fullStageEnd = voice.fullStageStart + 0.065f + 0.060f * (0.5f + 0.5f * hashFloat(voice.noiseState + 31u));
    voice.controlCountdown = 0;
    updateVoiceControl(voice, p);
    voice.phaseIncrement = voice.targetPhaseIncrement;
}

void VoiceEngine::noteOff(int midiNote)
{
    midiNote = std::clamp(midiNote, 0, 127);
    for (auto& voice : voices_)
        if (voice.active && voice.rootMidi == midiNote)
            voice.releasing = true;
}

void VoiceEngine::allNotesOff()
{
    for (auto& voice : voices_)
        if (voice.active)
            voice.releasing = true;
}

void VoiceEngine::allSoundOff() noexcept
{
    for (auto& voice : voices_)
        silenceVoice(voice);
    roomLeft_.fill(0.0f);
    roomRight_.fill(0.0f);
    roomWriteIndex_ = 0;
    roomDampingLeft_ = roomDampingRight_ = 0.0f;
    activeVoiceCount_.store(0, std::memory_order_relaxed);
}

void VoiceEngine::silenceVoice(Voice& voice) noexcept
{
    voice.active = false;
    voice.releasing = false;
    voice.envelope = 0.0f;
    voice.airEnvelope = 0.0f;
    for (auto& resonator : voice.tract)
        resonator.clear();
    for (auto& resonator : voice.early)
        resonator.clear();
    for (auto& resonator : voice.air)
        resonator.clear();
}

float VoiceEngine::midiToHz(int midiNote) noexcept
{
    return 440.0f * std::exp2((static_cast<float>(midiNote) - 69.0f) / 12.0f);
}

void VoiceEngine::updateResonator(Resonator& resonator, float frequency, float bandwidth) const noexcept
{
    frequency = std::clamp(frequency, 25.0f, 0.465f * static_cast<float>(sampleRate_));
    bandwidth = std::clamp(bandwidth, 20.0f, 0.25f * static_cast<float>(sampleRate_));
    const float radius = std::exp(-pi * bandwidth * inverseSampleRate_);
    resonator.a1 = 2.0f * radius * std::cos(twoPi * frequency * inverseSampleRate_);
    resonator.a2 = -radius * radius;
    resonator.b0 = std::max(0.00005f, 1.0f - radius);
}

void VoiceEngine::updateVoiceControl(Voice& voice, const EngineParameters& p)
{
    static constexpr float femaleFormants[3][formantCount] = {
        { 850.0f, 1220.0f, 2810.0f, 3650.0f, 4950.0f },
        { 350.0f,  800.0f, 2650.0f, 3550.0f, 4850.0f },
        { 470.0f, 1120.0f, 2740.0f, 3600.0f, 4860.0f }
    };
    static constexpr float maleFormants[3][formantCount] = {
        { 730.0f, 1090.0f, 2440.0f, 3250.0f, 4300.0f },
        { 300.0f,  700.0f, 2260.0f, 3050.0f, 4100.0f },
        { 405.0f, 1020.0f, 2380.0f, 3150.0f, 4200.0f }
    };
    static constexpr float femaleBw[formantCount] { 75.0f, 90.0f, 125.0f, 185.0f, 260.0f };
    static constexpr float maleBw[formantCount] { 68.0f, 82.0f, 112.0f, 165.0f, 235.0f };
    static constexpr float baseGain[formantCount] { 1.0f, 0.66f, 0.34f, 0.18f, 0.095f };

    const auto& singer = singers_[static_cast<std::size_t>(voice.singer)];
    const int vowel = p.vowel == Vowel::Ooh ? 1 : (p.vowel == Vowel::Uuh ? 2 : 0);
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
    const float vibratoPhase = wrapPhase(static_cast<float>(voice.ageSamples) * inverseSampleRate_ * vibratoRate
                                          + 0.173f * static_cast<float>(voice.singer));

    // Jitter is correlated over tens of milliseconds rather than sample-white pitch noise.
    const float random = randomBipolar(voice);
    voice.jitter += (random - voice.jitter) * (0.016f + 0.014f * p.humanize);
    const float jitterCents = 1.9f * p.humanize * voice.jitter;
    const float identityCents = singer.detuneCents * p.humanize;
    const float wanderCents = p.humanize * (2.1f * singerDrift + 0.75f * sharedPitch);
    const float cents = voice.pitchScoop + identityCents + wanderCents + jitterCents
                      + vibratoDepth * sine(vibratoPhase);
    const float frequency = midiToHz(voice.midiNote) * std::exp2(cents / 1200.0f);
    voice.targetPhaseIncrement = std::clamp(frequency * inverseSampleRate_, 0.0f, 0.48f);
    voice.phaseIncrementStep = (voice.targetPhaseIncrement - voice.phaseIncrement) / static_cast<float>(controlPeriod);

    static constexpr std::array<int, tableLevels> harmonics { 1, 2, 4, 8, 16, 32, 64, 128, 256 };
    const int permissible = std::max(1, static_cast<int>(0.46f * static_cast<float>(sampleRate_) / std::max(frequency, 1.0f)));
    voice.tableLevel = 0;
    for (int level = 1; level < tableLevels; ++level)
        if (harmonics[static_cast<std::size_t>(level)] <= permissible)
            voice.tableLevel = level;

    const float anatomy = 1.0f + singer.anatomy * p.humanize
        + p.humanize * (0.0045f * formantDrift + 0.0022f * sharedFormant);
    const float highAmount = std::max(0.0f, static_cast<float>(voice.midiNote - 69));
    for (int formant = 0; formant < formantCount; ++formant)
    {
        const float base = p.profile == VoiceProfile::Female
            ? femaleFormants[vowel][formant] : maleFormants[vowel][formant];
        const float highTune = 1.0f + highAmount * (formant == 0 ? 0.0032f : (formant == 1 ? 0.00125f : 0.0003f));
        const float targetHz = base * anatomy * highTune;
        const float targetGain = baseGain[static_cast<std::size_t>(formant)]
            * (0.72f + 0.50f * p.resonance) * (1.0f + (formant == 2 ? 0.10f * p.tension : 0.0f));
        if (voice.formantHz[static_cast<std::size_t>(formant)] <= 1.0f)
        {
            voice.formantHz[static_cast<std::size_t>(formant)] = targetHz;
            voice.formantGain[static_cast<std::size_t>(formant)] = targetGain;
        }
        else
        {
            voice.formantHz[static_cast<std::size_t>(formant)] += 0.105f * (targetHz - voice.formantHz[static_cast<std::size_t>(formant)]);
            voice.formantGain[static_cast<std::size_t>(formant)] += 0.105f * (targetGain - voice.formantGain[static_cast<std::size_t>(formant)]);
        }
        const float bw = (p.profile == VoiceProfile::Female ? femaleBw[formant] : maleBw[formant])
            * (1.18f - 0.30f * p.resonance) * (1.0f + 0.12f * p.breath);
        updateResonator(voice.tract[static_cast<std::size_t>(formant)],
                        voice.formantHz[static_cast<std::size_t>(formant)], bw);
        if (formant < 2)
        {
            updateResonator(voice.early[static_cast<std::size_t>(formant)],
                            voice.formantHz[static_cast<std::size_t>(formant)], bw * 1.75f);
            updateResonator(voice.air[static_cast<std::size_t>(formant)],
                            voice.formantHz[static_cast<std::size_t>(formant)] * (formant == 0 ? 1.03f : 1.0f), bw * 2.1f);
        }
    }

    const float pan = std::clamp(singer.pan * p.spread * (voice.singer == 0 && p.mode == PerformanceMode::Solo ? 0.18f : 1.0f), -1.0f, 1.0f);
    voice.panLeft = std::sqrt(0.5f * (1.0f - pan));
    voice.panRight = std::sqrt(0.5f * (1.0f + pan));
}

float VoiceEngine::tableLookup(const std::array<float, tableSize>& table, float phase) const noexcept
{
    phase = wrapPhase(phase);
    const float position = phase * static_cast<float>(tableSize);
    const int index = static_cast<int>(position) & tableMask;
    const float fraction = position - static_cast<float>(static_cast<int>(position));
    const float a = table[static_cast<std::size_t>(index)];
    const float b = table[static_cast<std::size_t>((index + 1) & tableMask)];
    return a + fraction * (b - a);
}

float VoiceEngine::sine(float phase) const noexcept
{
    return tableLookup(sineTable_, phase);
}

float VoiceEngine::randomBipolar(Voice& voice) noexcept
{
    std::uint32_t x = voice.noiseState;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    voice.noiseState = x == 0u ? 1u : x;
    return static_cast<float>(voice.noiseState & 0x00ffffffu) / 8388607.5f - 1.0f;
}

void VoiceEngine::updateRoom(float inputLeft, float inputRight, float amount,
                             float& wetLeft, float& wetRight) noexcept
{
    const auto read = [this](const auto& buffer, int delay)
    {
        int index = roomWriteIndex_ - delay;
        if (index < 0)
            index += roomBufferSize;
        return buffer[static_cast<std::size_t>(index)];
    };

    const float la = read(roomLeft_, roomDelayA_);
    const float lb = read(roomLeft_, roomDelayC_);
    const float ra = read(roomRight_, roomDelayB_);
    const float rb = read(roomRight_, roomDelayD_);
    const float rawLeft = 0.72f * la + 0.28f * rb;
    const float rawRight = 0.72f * ra + 0.28f * lb;
    roomDampingLeft_ += 0.28f * (rawLeft - roomDampingLeft_);
    roomDampingRight_ += 0.28f * (rawRight - roomDampingRight_);

    const float feedback = 0.62f + 0.14f * amount;
    roomLeft_[static_cast<std::size_t>(roomWriteIndex_)] = 0.30f * inputLeft
        + feedback * (0.84f * roomDampingLeft_ + 0.16f * roomDampingRight_);
    roomRight_[static_cast<std::size_t>(roomWriteIndex_)] = 0.30f * inputRight
        + feedback * (0.84f * roomDampingRight_ + 0.16f * roomDampingLeft_);
    roomWriteIndex_ = (roomWriteIndex_ + 1) & (roomBufferSize - 1);
    wetLeft = 0.55f * rawLeft + 0.23f * lb;
    wetRight = 0.55f * rawRight + 0.23f * rb;
}

void VoiceEngine::process(float* left, float* right, int numSamples)
{
    if (numSamples <= 0)
        return;
    if (!prepared_)
        prepare(sampleRate_, std::max(maxBlockSize_, numSamples));

    blockParameters_ = snapshotParameters();
    const float parameterSmoothing = 1.0f - std::exp(-inverseSampleRate_ / 0.025f);
    const float attackBase = 1.0f - std::exp(-inverseSampleRate_ / (0.010f + 0.018f * blockParameters_.humanize));
    const float airAttack = 1.0f - std::exp(-inverseSampleRate_ / 0.004f);
    const float releaseMultiplier = std::exp(-inverseSampleRate_ / (0.105f + 0.19f * blockParameters_.humanize));
    const float scoopMultiplier = std::exp(-inverseSampleRate_ / 0.072f);
    const float onsetAirMultiplier = std::exp(-inverseSampleRate_ / 0.085f);
    const float sharedPitchIncrement = 0.047f * inverseSampleRate_;
    const float sharedRateIncrement = 0.019f * inverseSampleRate_;
    const float sharedFormantIncrement = 0.011f * inverseSampleRate_;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        sharedPitchPhase_ = wrapPhase(sharedPitchPhase_ + sharedPitchIncrement);
        sharedRatePhase_ = wrapPhase(sharedRatePhase_ + sharedRateIncrement);
        sharedFormantPhase_ = wrapPhase(sharedFormantPhase_ + sharedFormantIncrement);
        for (auto& singer : singers_)
        {
            singer.driftPhase = wrapPhase(singer.driftPhase + singer.driftIncrement);
            singer.depthPhase = wrapPhase(singer.depthPhase + singer.depthIncrement);
            singer.formantPhase = wrapPhase(singer.formantPhase + singer.formantIncrement);
        }

        float dryLeft = 0.0f;
        float dryRight = 0.0f;
        for (auto& voice : voices_)
        {
            if (!voice.active)
                continue;
            if (voice.delaySamples > 0)
            {
                --voice.delaySamples;
                continue;
            }

            if (--voice.controlCountdown <= 0)
            {
                updateVoiceControl(voice, blockParameters_);
                voice.controlCountdown = controlPeriod;
            }
            voice.phaseIncrement += voice.phaseIncrementStep;
            voice.phase += voice.phaseIncrement;
            if (voice.phase >= 1.0f)
            {
                voice.phase -= 1.0f;
                voice.alternateCycle = !voice.alternateCycle;
            }

            if (voice.releasing)
            {
                voice.envelope *= releaseMultiplier;
                voice.airEnvelope *= releaseMultiplier * releaseMultiplier;
            }
            else
            {
                voice.envelope += (1.0f - voice.envelope) * attackBase;
                voice.airEnvelope += (1.0f - voice.airEnvelope) * airAttack;
            }
            voice.pitchScoop *= scoopMultiplier;
            voice.onsetAir *= onsetAirMultiplier;

            const float soft = tableLookup(softTables_[static_cast<std::size_t>(voice.tableLevel)], voice.phase);
            const float tense = tableLookup(tenseTables_[static_cast<std::size_t>(voice.tableLevel)], voice.phase);
            float glottal = soft + blockParameters_.tension * (tense - soft);
            const float lowIrregularity = voice.midiNote < 52 ? (1.0f - blockParameters_.tension) * blockParameters_.humanize * 0.035f : 0.0f;
            glottal *= 1.0f + (voice.alternateCycle ? lowIrregularity : -lowIrregularity);

            const float noise = randomBipolar(voice);
            voice.shimmer += (noise - voice.shimmer) * 0.006f;
            glottal *= 1.0f + 0.026f * blockParameters_.humanize * voice.shimmer;
            const float highNoise = noise - 0.92f * voice.lastNoise;
            voice.lastNoise = noise;

            float earlyVoice = 0.0f;
            for (int f = 0; f < 2; ++f)
                earlyVoice += voice.formantGain[static_cast<std::size_t>(f)]
                    * voice.early[static_cast<std::size_t>(f)].tick(glottal);
            float fullVoice = 0.0f;
            for (int f = 0; f < formantCount; ++f)
                fullVoice += voice.formantGain[static_cast<std::size_t>(f)]
                    * voice.tract[static_cast<std::size_t>(f)].tick(glottal);
            float shapedAir = 0.0f;
            for (int f = 0; f < 2; ++f)
                shapedAir += (0.72f - 0.18f * static_cast<float>(f))
                    * voice.air[static_cast<std::size_t>(f)].tick(highNoise);

            const float age = static_cast<float>(voice.ageSamples) * inverseSampleRate_;
            const float fullMix = smoothStep((age - voice.fullStageStart)
                                             / std::max(0.001f, voice.fullStageEnd - voice.fullStageStart));
            const float voiced = earlyVoice + fullMix * (fullVoice - earlyVoice);
            const float breathAmount = blockParameters_.breath
                * (0.22f + 0.78f * voice.onsetAir);
            const float air = shapedAir * breathAmount * voice.airEnvelope;
            const float tonal = voiced * voice.envelope * (0.88f - 0.24f * blockParameters_.breath);
            const float amplitude = voice.groupGain * voice.velocity * (0.67f + 0.33f * std::sqrt(voice.velocity));
            const float output = amplitude * (0.62f * tonal + 0.22f * air);
            dryLeft += output * voice.panLeft;
            dryRight += output * voice.panRight;
            ++voice.ageSamples;

            if (voice.releasing && voice.envelope < 0.00008f && voice.airEnvelope < 0.00008f)
                silenceVoice(voice);
        }

        smoothedRoom_ += parameterSmoothing * (blockParameters_.room - smoothedRoom_);
        smoothedGain_ += parameterSmoothing * (blockParameters_.outputGain - smoothedGain_);
        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        updateRoom(dryLeft, dryRight, smoothedRoom_, wetLeft, wetRight);
        const float dryScale = 1.0f - 0.12f * smoothedRoom_;
        const float wetScale = 0.72f * smoothedRoom_;
        const float outLeft = smoothedGain_ * (dryScale * dryLeft + wetScale * wetLeft);
        const float outRight = smoothedGain_ * (dryScale * dryRight + wetScale * wetRight);

        if (left != nullptr)
            left[sample] = outLeft;
        if (right != nullptr && right != left)
            right[sample] = outRight;
        else if (right != nullptr)
            right[sample] = 0.5f * (outLeft + outRight);
    }

    int count = 0;
    for (const auto& voice : voices_)
        count += voice.active ? 1 : 0;
    activeVoiceCount_.store(count, std::memory_order_relaxed);
}

int VoiceEngine::getActiveVoiceCount() const
{
    return activeVoiceCount_.load(std::memory_order_relaxed);
}

} // namespace vocalor
