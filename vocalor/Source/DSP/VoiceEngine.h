#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace vocalor
{

enum class VoiceProfile { Female, Male };
enum class PerformanceMode { Solo, Choir, Chord };
enum class Vowel { Aah, Ooh, Uuh };
enum class ChordQuality { Major, Minor };

struct EngineParameters
{
    VoiceProfile profile { VoiceProfile::Female };
    PerformanceMode mode { PerformanceMode::Solo };
    Vowel vowel { Vowel::Aah };
    ChordQuality chordQuality { ChordQuality::Major };
    int choirSize { 8 };
    float breath { 0.28f };
    float resonance { 0.72f };
    float vibrato { 0.42f };
    float humanize { 0.55f };
    float spread { 0.65f };
    float tension { 0.48f };
    float room { 0.22f };
    float outputGain { 0.80f };
};

class VoiceEngine
{
public:
    VoiceEngine() noexcept;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    void allSoundOff() noexcept;
    void process(float* left, float* right, int numSamples);
    [[nodiscard]] int getActiveVoiceCount() const;

private:
    static constexpr int tableSize = 2048;
    static constexpr int tableMask = tableSize - 1;
    static constexpr int tableLevels = 9;
    static constexpr int maxVoices = 96;
    static constexpr int singerCount = 12;
    static constexpr int formantCount = 5;
    static constexpr int roomBufferSize = 32768;
    static constexpr int controlPeriod = 16;

    // Harmonic count per band-limited table level; shared by buildTables()
    // (which fills the tables) and updateVoiceControl() (which picks the
    // aliasing-safe level for the current pitch) so the two stay in sync.
    static constexpr std::array<int, tableLevels> harmonicsPerLevel {
        1, 2, 4, 8, 16, 32, 64, 128, 256
    };

    struct AtomicParameters
    {
        std::atomic<int> profile { 0 };
        std::atomic<int> mode { 0 };
        std::atomic<int> vowel { 0 };
        std::atomic<int> chordQuality { 0 };
        std::atomic<int> choirSize { 8 };
        std::atomic<float> breath { 0.28f };
        std::atomic<float> resonance { 0.72f };
        std::atomic<float> vibrato { 0.42f };
        std::atomic<float> humanize { 0.55f };
        std::atomic<float> spread { 0.65f };
        std::atomic<float> tension { 0.48f };
        std::atomic<float> room { 0.22f };
        std::atomic<float> outputGain { 0.80f };
    };

    struct Resonator
    {
        float y1 { 0.0f };
        float y2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
        float b0 { 0.0f };

        float tick(float input) noexcept
        {
            const float value = b0 * input + a1 * y1 + a2 * y2;
            y2 = y1;
            y1 = value;
            return value;
        }

        void clear() noexcept { y1 = y2 = 0.0f; }
    };

    struct SingerIdentity
    {
        float detuneCents { 0.0f };
        float anatomy { 0.0f };
        float pan { 0.0f };
        float onsetOffset { 0.0f };
        float vibratoRate { 5.1f };
        float vibratoDepth { 1.0f };
        float driftPhase { 0.0f };
        float depthPhase { 0.0f };
        float formantPhase { 0.0f };
        float driftIncrement { 0.0f };
        float depthIncrement { 0.0f };
        float formantIncrement { 0.0f };
    };

    struct Voice
    {
        bool active { false };
        bool releasing { false };
        bool alternateCycle { false };
        int rootMidi { -1 };
        int midiNote { 60 };
        int singer { 0 };
        int tableLevel { 0 };
        int delaySamples { 0 };
        int controlCountdown { 0 };
        std::uint64_t generation { 0 };
        std::uint32_t noiseState { 1u };
        std::uint64_t ageSamples { 0 };
        float velocity { 0.0f };
        float groupGain { 1.0f };
        float phase { 0.0f };
        float phaseIncrement { 0.0f };
        float targetPhaseIncrement { 0.0f };
        float phaseIncrementStep { 0.0f };
        float envelope { 0.0f };
        float airEnvelope { 0.0f };
        float onsetAir { 1.0f };
        float pitchScoop { 0.0f };
        float jitter { 0.0f };
        float shimmer { 0.0f };
        float lastNoise { 0.0f };
        float panLeft { 0.7071f };
        float panRight { 0.7071f };
        float fullStageStart { 0.055f };
        float fullStageEnd { 0.145f };
        std::array<float, formantCount> formantHz {};
        std::array<float, formantCount> formantGain {};
        std::array<Resonator, formantCount> tract {};
        std::array<Resonator, 2> early {};
        std::array<Resonator, 2> air {};
    };

    EngineParameters snapshotParameters() const noexcept;
    void buildTables();
    void buildSingerIdentities();
    void initialiseVoice(Voice& voice, int rootMidi, int soundingMidi, int singer,
                         float velocity, float groupGain, int singerTotal,
                         const EngineParameters& parameters);
    void updateVoiceControl(Voice& voice, const EngineParameters& parameters);
    void updateResonator(Resonator& resonator, float frequency, float bandwidth) const noexcept;
    void silenceVoice(Voice& voice) noexcept;
    int voicesForMode(const EngineParameters& parameters) const noexcept;
    int chordMidiForSinger(int rootMidi, int singer, const EngineParameters& parameters) const noexcept;
    int findFreeVoice() const noexcept;
    void makeRoomFor(int required);
    float tableLookup(const std::array<float, tableSize>& table, float phase) const noexcept;
    float sine(float phase) const noexcept;
    float randomBipolar(Voice& voice) noexcept;
    void updateRoom(float inputLeft, float inputRight, float amount,
                    float& wetLeft, float& wetRight) noexcept;
    static float midiToHz(int midiNote) noexcept;

    AtomicParameters atomicParameters_ {};
    EngineParameters blockParameters_ {};
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    int maxBlockSize_ { 512 };
    bool prepared_ { false };
    std::uint64_t generation_ { 0 };

    std::array<Voice, maxVoices> voices_ {};
    std::array<SingerIdentity, singerCount> singers_ {};
    std::array<std::array<float, tableSize>, tableLevels> softTables_ {};
    std::array<std::array<float, tableSize>, tableLevels> tenseTables_ {};
    std::array<float, tableSize> sineTable_ {};

    float sharedPitchPhase_ { 0.0f };
    float sharedRatePhase_ { 0.0f };
    float sharedFormantPhase_ { 0.0f };
    float smoothedRoom_ { 0.0f };
    float smoothedGain_ { 0.8f };
    std::atomic<int> activeVoiceCount_ { 0 };

    std::array<float, roomBufferSize> roomLeft_ {};
    std::array<float, roomBufferSize> roomRight_ {};
    int roomWriteIndex_ { 0 };
    int roomDelayA_ { 1423 };
    int roomDelayB_ { 1789 };
    int roomDelayC_ { 1999 };
    int roomDelayD_ { 2131 };
    float roomDampingLeft_ { 0.0f };
    float roomDampingRight_ { 0.0f };
};

} // namespace vocalor
