#pragma once

#include <JuceHeader.h>

#include "DSP/YouKnow106Engine.h"
#include "DSP/YouKnow106Presets.h"
#include "DSP/YouKnow106SysEx.h"
#include "DSP/YouKnow106Panel.h"

#include <array>
#include <atomic>
#include <cstdint>

class YouKnow106AudioProcessorEditor;

class YouKnow106AudioProcessor final : public juce::AudioProcessor,
                                       private juce::MidiKeyboardState::Listener,
                                       private juce::AsyncUpdater
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

    // The factory bank is exposed as host programs, numbered the way the
    // modelled instrument numbers its patches.
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    // Applies a stored patch to the parameters. The controls a patch does not
    // carry -- volume, the bender depths, portamento and the assign mode --
    // are left where the player put them, exactly as on the hardware.
    void applyPatch (const sysex::Patch& patch);
    // The current panel settings as a patch.
    sysex::Patch currentPatch() const;
    // The current patch as a system-exclusive message the hardware would accept.
    juce::MidiMessage currentPatchAsSysEx (int channel) const;

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
    int getVoiceLimitForDisplay() const noexcept
    {
        return displayVoiceLimit.load (std::memory_order_relaxed);
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
    // Key releases the queue had no room for. Dropping a press costs a note
    // nobody hears; dropping a release leaves one held down for good.
    std::array<std::atomic<std::uint64_t>, 2> uiPendingNoteOff { };

    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel,
                       int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel,
                        int midiNoteNumber, float velocity) override;
    void enqueueUiMidiEvent (int note, float velocity, bool isNoteOn) noexcept;
    void discardUiMidiEvents() noexcept;
    void dispatchUiMidiEvents() noexcept;
    void updateEngineParameters() noexcept;
    // Which factory program was last selected. Only a display value: the
    // patch itself lives in the parameters once it is applied.
    int currentProgram { 0 };

    float valueOf (const char* parameterId) const noexcept;
    int choiceOf (const char* parameterId, int maximum) const noexcept;

    struct ParameterPointer
    {
        const char* id = nullptr;
        std::atomic<float>* value = nullptr;
    };
    std::array<ParameterPointer, 37> parameterPointers {};

    youknow106::YouKnow106Engine engine;
    // Patches arriving over system exclusive. Parameters cannot be written from
    // the audio callback -- that path notifies the host and may allocate -- so
    // the tone bytes are handed to the message thread instead.
    //
    // A single staging buffer plus a flag is not enough: a bank transfer sends
    // dumps back to back, and the audio thread would overwrite the buffer while
    // the message thread was still reading it, producing a patch assembled from
    // two different dumps. The handoff is a small ring, and each slot is only
    // written when the writer owns it.
    void handleAsyncUpdate() override;
    void stagePatchForMessageThread (const sysex::Patch& patch) noexcept;
    // Translates the pre-split `keyMode` and `chorus` choices in a saved
    // session into the independent button pairs that replaced them.
    static void migrateSplitModeParameters (juce::ValueTree& state);
    static constexpr int pendingPatchSlots = 8;
    struct PendingPatch
    {
        std::array<std::uint8_t, sysex::toneByteCount> bytes {};
        std::atomic<bool> ready { false };
    };
    std::array<PendingPatch, pendingPatchSlots> pendingPatches {};
    std::atomic<int> pendingWriteIndex { 0 };
    int pendingReadIndex { 0 };
    // The panel state the hardware's own single-parameter messages accumulate
    // into. Only the audio thread touches it.
    sysex::Patch liveSysExPatch {};
    bool liveSysExPatchValid { false };

    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineReady { false };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> displayVoiceMask { 0 };
    std::atomic<int> displayVoiceLimit { youknow106::YouKnow106Engine::hardwareVoices };
    std::atomic<float> displayEnvelope { 0.0f };
    std::atomic<float> displayLfo { 0.0f };
    std::atomic<double> displaySampleRate { 0.0 };
    std::atomic<int> displayOversamplingFactor { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouKnow106AudioProcessor)
};
