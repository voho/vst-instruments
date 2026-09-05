#pragma once

#include "TaikoEngine.h"

#include <array>
#include <atomic>
#include <memory>

namespace taikor
{
inline constexpr double maximumEnsembleDelaySeconds = 0.030;
// Independently ringing copies of the four drums, feeding one output stage.
// All storage is allocated at construction; scheduling/rendering never allocates.
class EnsembleEngine
{
public:
    EnsembleEngine();
    void prepare (double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;
    void allSoundsOff() noexcept;
    void setParameters (const EngineParameters&) noexcept;
    void trigger (Articulation, int octave, float velocity) noexcept;
    [[nodiscard]] bool triggerMidi (int note, float velocity) noexcept;
    void process (float* left, float* right, int samples) noexcept;
    void setHandDamping (float amount) noexcept;
    void setPitchBend (float amount) noexcept;
    void setStrikePositionOverride (float amount) noexcept;
    void setStrikeAzimuthOverride (float radians) noexcept;
    void clearStrikeOverrides() noexcept;

    [[nodiscard]] int getActiveVoiceCount() const noexcept
    { return activeVoices.load (std::memory_order_relaxed); }
    [[nodiscard]] float getOutputLevel (int channel) const noexcept
    { return players[0]->getOutputLevel (channel); }
    void getVisualState (DrumVisualState& state) const noexcept
    {
        players[0]->getVisualState (state);
        state.activeVoices = getActiveVoiceCount();
    }

private:
    friend struct EnsembleEngineTestAccess;
    static constexpr int blockCapacity = 256;
    static constexpr std::size_t queueCapacity = 1024;
    struct Hit
    {
        std::uint64_t due = 0, order = 0;
        int member = 0, octave = 0;
        Articulation articulation = Articulation::Don;
        float velocity = 0.0f, position = 0.0f, azimuth = 0.0f;
        float radial = 0.0f, tangential = 0.0f;
    };
    static bool later (const Hit& a, const Hit& b) noexcept;
    static std::uint32_t hash (std::uint32_t) noexcept;
    static float stagePosition (int member, int size) noexcept;
    void updateStage (bool snap) noexcept;
    void fire (const Hit&) noexcept;
    void dispatchDue() noexcept;
    void publishVoices() noexcept;

    std::array<std::unique_ptr<TaikoEngine>, maximumEnsembleSize> players;
    std::array<Hit, queueCapacity> pending {};
    std::size_t pendingCount = 0;
    std::uint64_t sampleClock = 0, strokeSequence = 0, eventSequence = 0;
    EngineParameters parameters {};
    double rate = 48000.0;
    double gain = 1.0, gainSmoothing = 0.0;
    bool prepared = false;
    float positionOverride = 0.0f, azimuthOverride = 0.0f;
    bool positionOverridden = false, azimuthOverridden = false;
    std::array<float, blockCapacity> extraLeft {}, extraRight {}, mixGain {};
    std::array<float, blockCapacity> scratchLeft {}, scratchRight {};
    std::array<StereoPan, maximumEnsembleSize> pan {}, panTarget {};
    std::array<StereoPan, blockCapacity> leadPan {};
    std::atomic<int> activeVoices { 0 };
};
} // namespace taikor
