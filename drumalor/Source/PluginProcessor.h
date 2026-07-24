#pragma once

#include <JuceHeader.h>

#include "DSP/DrumEngine.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace drumalor::parameters
{
inline constexpr auto output = "output";
inline constexpr auto humanise = "humanise";
inline constexpr auto busDrive = "busDrive";
inline constexpr auto busCompression = "busComp";

// Kit-wide controls, in the order they are appended to the layout.
inline constexpr int kitParameterCount = 4;

enum Slot
{
    characterA = 0,
    characterB,
    pitch,
    decay,
    level,
    pan,
    choke,
    count
};

// The four original per-voice controls keep host parameter indices 0..51 so
// existing automation lanes and sessions stay valid; the mixer slots are
// appended after the kit controls.
inline constexpr int originalSlotCount = 4;
} // namespace drumalor::parameters

class DrumalorAudioProcessorEditor;

class DrumalorAudioProcessor final : public juce::AudioProcessor
{
public:
    DrumalorAudioProcessor();
    ~DrumalorAudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return drumalor::maximumTailSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void triggerFromUi (drumalor::Instrument instrument, float velocity = 0.86f) noexcept;
    void requestPanic() noexcept;

    [[nodiscard]] std::uint32_t getTriggerCounter (drumalor::Instrument instrument) const noexcept;
    [[nodiscard]] int getActiveVoiceCount() const noexcept
    {
        return activeVoiceCount.load (std::memory_order_relaxed);
    }
    // Metering for the editor. The engine publishes already-smoothed values
    // through relaxed atomics, so polling from the message thread is safe.
    [[nodiscard]] float getOutputLevel (int channel) const noexcept
    {
        return engine.getOutputLevel (channel);
    }
    [[nodiscard]] float getInstrumentLevel (drumalor::Instrument instrument) const noexcept
    {
        return engine.getInstrumentLevel (instrument);
    }
    [[nodiscard]] float getBusGain() const noexcept { return engine.getBusGain(); }
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
    static juce::String parameterId (drumalor::Instrument instrument, int slot);

private:
    using InstrumentParameterPointers =
        std::array<std::atomic<float>*, drumalor::parameters::count>;

    struct UiTriggerEvent
    {
        drumalor::Instrument instrument { drumalor::Instrument::Kick };
        float velocity { 0.0f };
        std::uint32_t generation { 0 };
    };

    static constexpr unsigned uiQueueCapacity = 128;

    void enqueueUiTrigger (drumalor::Instrument instrument, float velocity) noexcept;
    void dispatchUiTriggers() noexcept;
    void discardUiTriggers() noexcept;
    void dispatchMidiData (const juce::uint8* data, int numBytes) noexcept;
    void updateEngineParameters() noexcept;
    void registerTrigger (drumalor::Instrument instrument) noexcept;

    std::array<InstrumentParameterPointers, drumalor::instrumentCount> parameterPointers {};
    std::atomic<float>* outputParameter = nullptr;
    std::atomic<float>* humaniseParameter = nullptr;
    std::atomic<float>* busDriveParameter = nullptr;
    std::atomic<float>* busCompressionParameter = nullptr;

    std::array<UiTriggerEvent, uiQueueCapacity> uiTriggerQueue {};
    std::atomic<unsigned> uiWriteIndex { 0 };
    std::atomic<unsigned> uiReadIndex { 0 };
    std::atomic<std::uint32_t> uiQueueGeneration { 1 };

    std::array<std::atomic<std::uint32_t>, drumalor::instrumentCount> triggerCounters {};

    drumalor::DrumEngine engine;
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineReady { false };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<double> displaySampleRate { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumalorAudioProcessor)
};
