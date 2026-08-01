#pragma once

#include <JuceHeader.h>

#include "DSP/YouKnow106Engine.h"
#include "DSP/YouKnow106Panel.h"

#include <array>
#include <atomic>

class YouKnow106AudioProcessorEditor;

class YouKnow106AudioProcessor final : public juce::AudioProcessor,
                                       private juce::MidiKeyboardState::Listener
{
public:
    YouKnow106AudioProcessor();
    ~YouKnow106AudioProcessor() override;

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
    // The longest release the panel can ask for is 12 s, the voice tolerance
    // can stretch that by a few per cent, and the delay lines and the output
    // coupling still have to settle afterwards.
    static constexpr double maximumTailLengthSeconds = 15.0;
    double getTailLengthSeconds() const override { return maximumTailLengthSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Call from the message thread. The amount is a proportion of every
    // control's normalised legal range.
    void randomizeParameters (float amount);
    void requestPanic() noexcept { panicRequested.store (true, std::memory_order_release); }

    int getActiveVoiceCount() const noexcept
    {
        return activeVoiceCount.load (std::memory_order_relaxed);
    }
    int getVoiceMaskForDisplay() const noexcept
    {
        return displayVoiceMask.load (std::memory_order_relaxed);
    }
    float getEnvelopeForDisplay() const noexcept
    {
        return displayEnvelope.load (std::memory_order_relaxed);
    }
    float getLfoForDisplay() const noexcept
    {
        return displayLfo.load (std::memory_order_relaxed);
    }
    double getCurrentSampleRateForDisplay() const noexcept
    {
        return displaySampleRate.load (std::memory_order_relaxed);
    }
    int getOversamplingFactorForDisplay() const noexcept
    {
        return displayOversamplingFactor.load (std::memory_order_relaxed);
    }
    bool isEngineReady() const noexcept
    {
        return engineReady.load (std::memory_order_acquire);
    }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct UiMidiEvent
    {
        int note = 60;
        float velocity = 0.0f;
        bool noteOn = false;
    };

    static constexpr unsigned uiQueueCapacity = 128;
    std::array<UiMidiEvent, uiQueueCapacity> uiMidiQueue {};
    std::atomic<unsigned> uiWriteIndex { 0 };
    std::atomic<unsigned> uiReadIndex { 0 };

    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel,
                       int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel,
                        int midiNoteNumber, float velocity) override;
    void enqueueUiMidiEvent (int note, float velocity, bool isNoteOn) noexcept;
    void discardUiMidiEvents() noexcept;
    void dispatchUiMidiEvents() noexcept;
    void updateEngineParameters() noexcept;
    float valueOf (const char* parameterId) const noexcept;
    int choiceOf (const char* parameterId, int maximum) const noexcept;

    struct ParameterPointer
    {
        const char* id = nullptr;
        std::atomic<float>* value = nullptr;
    };
    std::array<ParameterPointer, 37> parameterPointers {};

    youknow106::YouKnow106Engine engine;
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineReady { false };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> displayVoiceMask { 0 };
    std::atomic<float> displayEnvelope { 0.0f };
    std::atomic<float> displayLfo { 0.0f };
    std::atomic<double> displaySampleRate { 0.0 };
    std::atomic<int> displayOversamplingFactor { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouKnow106AudioProcessor)
};
