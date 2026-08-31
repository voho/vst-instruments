#pragma once

#include "GeneratedBankData.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace acustra::dense {

struct ZoneView {
    Bank bank{};
    const char* name{};
    std::uint8_t lowKey{};
    std::uint8_t highKey{};
    std::uint8_t rootMidi{};
    float rootHz{};
    std::uint32_t sampleRate{};
    std::uint8_t channels{};
    std::uint32_t frames{};
    std::uint32_t onsetFrame{};
    std::int32_t peak{};
    std::uint8_t lowVelocity{1};
    std::uint8_t highVelocity{127};
    std::uint8_t roundRobin{};
    std::uint32_t terminalFadeFrames{};
    std::int32_t endJump{};
    std::uint64_t decodedHash{};
    const std::int16_t* samples{};
    float playbackTrim{1.0f};
    std::int8_t physicalStringIndex{-1};
    std::int8_t capturedOpenMidi{-1};
    std::int8_t capturedFret{-1};
};

struct LibraryTestAccess;

class Library {
public:
    // Call outside the audio callback. This is the only decoding/allocation step.
    bool prepare(std::string* error = nullptr);

    [[nodiscard]] bool ready() const noexcept { return !samples_.empty(); }
    [[nodiscard]] const ZoneView* find(Bank bank, int midiNote,
                                       int midiVelocity = 127,
                                       std::uint8_t roundRobin = 0,
                                       int physicalStringIndex = -1,
                                       int openMidi = -1,
                                       int fret = -1) const noexcept;
    [[nodiscard]] const ZoneView* zone(std::size_t index) const noexcept;
    [[nodiscard]] std::size_t zoneCount() const noexcept { return zones_.size(); }
    [[nodiscard]] std::size_t decodedBytes() const noexcept { return samples_.size() * sizeof(std::int16_t); }
    [[nodiscard]] static std::size_t packedBytes() noexcept;

private:
    friend struct LibraryTestAccess;

    std::vector<std::int16_t> samples_;
    std::vector<ZoneView> zones_;
};

class Sampler {
public:
    // Acustra models one independently fretted note per physical string.
    static constexpr std::size_t kVoiceCount = 6;

    Sampler() noexcept = default;
    explicit Sampler(const Library& library) noexcept : library_(&library) {}

    void attachLibrary(const Library* library) noexcept { library_ = library; }
    void setOutputSampleRate(double sampleRate) noexcept;
    void setPitchBendSemitones(float semitones) noexcept;
    [[nodiscard]] bool noteOn(std::size_t voiceIndex, Bank bank, int midiNote,
                              float velocity, float pan,
                              float stringAge,
                              int openMidi = -1) noexcept;
    void noteOff(std::size_t voiceIndex,
                 float releaseMilliseconds = 60.0f) noexcept;
    void releaseAll(float releaseMilliseconds = 60.0f) noexcept;
    void allNotesOff() noexcept;
    [[nodiscard]] bool isActive(std::size_t voiceIndex) const noexcept;
    [[nodiscard]] const ZoneView* activeZone(std::size_t voiceIndex) const noexcept;
    [[nodiscard]] std::size_t activeVoiceCount() const noexcept;

    // process() clears its outputs; renderAdd() adds to existing outputs.
    void process(float* left, float* right, std::size_t frames) noexcept;
    void renderAdd(float* left, float* right, std::size_t frames) noexcept;

private:
    static constexpr std::size_t kRetiringVoiceCount = kVoiceCount * 2;

    struct Voice {
        const ZoneView* zone{};
        double position{};
        double baseStep{1.0};
        double step{1.0};
        float gain{};
        float pan{};
        float attackGain{};
        float attackIncrement{};
        float attackSeconds{0.00075f};
        float decayGain{1.0f};
        float decayMultiplier{1.0f};
        float releaseGain{1.0f};
        float releaseMultiplier{1.0f};
        std::uint64_t age{};
        int midiNote{-1};
        bool releasing{};
        bool active{};
    };

    [[nodiscard]] float interpolate(const Voice& voice, std::uint8_t channel) const noexcept;
    void beginRelease(Voice& voice, float releaseMilliseconds) noexcept;
    void updateStep(Voice& voice) noexcept;

    const Library* library_{};
    std::array<Voice, kVoiceCount> voices_{};
    std::array<Voice, kRetiringVoiceCount> retiringVoices_{};
    double outputSampleRate_{48000.0};
    float pitchBendSemitones_{};
    std::uint64_t ageCounter_{};
    std::array<std::array<std::uint8_t, 128>, 3> roundRobin_{};
};

} // namespace acustra::dense
