#pragma once

#include <JuceHeader.h>

#include "DSP/TaikoEngine.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace taikor::parameters
{
// The drum itself.
inline constexpr auto headDiameter = "size";
inline constexpr auto bodyDepth = "depth";
inline constexpr auto tension = "tension";
inline constexpr auto headMaterial = "headMat";
inline constexpr auto shellMaterial = "shellMat";
inline constexpr auto resonantTension = "resHead";
inline constexpr auto cavityCoupling = "cavity";
inline constexpr auto headDamping = "headDamp";
inline constexpr auto shellResonance = "shellRes";
inline constexpr auto pitch = "pitch";

// How it is struck.
inline constexpr auto bachiHardness = "hardness";
inline constexpr auto strikePosition = "strikePos";
inline constexpr auto velocityDepth = "velDepth";
inline constexpr auto tensionModulation = "tensionMod";
inline constexpr auto strikeNoise = "strikeNoise";
inline constexpr auto humanise = "humanise";
inline constexpr auto octaveBody = "octaveBody";

// The close pair and the output stage.
inline constexpr auto micDistance = "micDist";
inline constexpr auto micSpread = "micSpread";
inline constexpr auto stereoWidth = "width";
inline constexpr auto drive = "drive";
inline constexpr auto output = "output";

inline constexpr int parameterCount = 22;

// Head diameter is presented in centimetres because that is how drums are
// sold; the engine works in metres.
inline constexpr float minimumDiameterCentimetres = 15.0f;
inline constexpr float maximumDiameterCentimetres = 180.0f;
inline constexpr float minimumMicDistanceCentimetres = 3.0f;
inline constexpr float maximumMicDistanceCentimetres = 40.0f;
} // namespace taikor::parameters

class TaikorAudioProcessorEditor;

class TaikorAudioProcessor final : public juce::AudioProcessor
{
public:
    TaikorAudioProcessor();
    ~TaikorAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return taikor::maximumTailSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Auditioning from the editor's stroke pads.
    void triggerFromUi (taikor::Articulation articulation, int octaveOffset,
                        float velocity = 0.85f) noexcept;
    void requestPanic() noexcept;

    [[nodiscard]] std::uint32_t getTriggerCounter (
        taikor::Articulation articulation) const noexcept;
    [[nodiscard]] int getActiveVoiceCount() const noexcept
    {
        return activeVoiceCount.load (std::memory_order_relaxed);
    }
    [[nodiscard]] float getOutputLevel (int channel) const noexcept
    {
        return engine.getOutputLevel (channel);
    }
    void getVisualState (taikor::DrumVisualState& destination) const noexcept
    {
        engine.getVisualState (destination);
    }
    // The drum the current parameters describe, for the editor's readout. Safe
    // to call from the message thread: it re-solves from the parameter values
    // rather than reading engine state.
    [[nodiscard]] taikor::TaikoEngine::DrumMeasurements measureDrum (
        int octaveOffset) const noexcept;
    [[nodiscard]] double getCurrentSampleRateForDisplay() const noexcept
    {
        return displaySampleRate.load (std::memory_order_relaxed);
    }
    [[nodiscard]] bool isEngineReady() const noexcept
    {
        return engineReady.load (std::memory_order_acquire);
    }

    juce::AudioProcessorValueTreeState parameters;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    // Reads the current parameter values into an engine parameter block. Shared
    // by the audio thread and by the editor's physical readout, so the two can
    // never disagree about what drum is configured.
    [[nodiscard]] taikor::EngineParameters snapshotEngineParameters() const noexcept;

private:
    struct UiTriggerEvent
    {
        taikor::Articulation articulation { taikor::Articulation::Don };
        int octaveOffset { 0 };
        float velocity { 0.0f };
        std::uint32_t generation { 0 };
    };

    static constexpr unsigned uiQueueCapacity = 128;

    void enqueueUiTrigger (taikor::Articulation articulation, int octaveOffset,
                           float velocity) noexcept;
    void dispatchUiTriggers() noexcept;
    void discardUiTriggers() noexcept;
    void dispatchMidiData (const juce::uint8* data, int numBytes) noexcept;
    void updateEngineParameters() noexcept;
    void registerTrigger (taikor::Articulation articulation) noexcept;

    std::array<std::atomic<float>*, taikor::parameters::parameterCount>
        parameterPointers {};

    std::array<UiTriggerEvent, uiQueueCapacity> uiTriggerQueue {};
    std::atomic<unsigned> uiWriteIndex { 0 };
    std::atomic<unsigned> uiReadIndex { 0 };
    std::atomic<std::uint32_t> uiQueueGeneration { 1 };

    std::array<std::atomic<std::uint32_t>, taikor::articulationCount> triggerCounters {};

    taikor::TaikoEngine engine;
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineReady { false };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<double> displaySampleRate { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaikorAudioProcessor)
};
