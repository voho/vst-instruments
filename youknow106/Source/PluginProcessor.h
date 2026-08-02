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
                                       private juce::Timer
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
    // The instrument sends nothing of its own, but a requested patch dump
    // leaves on the MIDI output, and a host that asks the processor rather
    // than the wrapper metadata has to be told so or it may not route it.
    bool producesMidi() const override { return true; }
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
    // The patch a program holds, INIT included. Out-of-range gives a default
    // patch, which is what INIT is anyway.
    youknow106::sysex::Patch programPatch (int index) const;
    // Whether the panel has moved away from the program that was last selected.
    // Compared as tone bytes, so it asks the question the hardware would ask:
    // has anything changed that a stored patch actually records? Volume and the
    // bender depths are not part of a patch, so moving them is not an edit.
    bool currentProgramIsEdited() const;

    // Applies a stored patch to the parameters. The controls a patch does not
    // carry -- volume, the bender depths, portamento and the assign mode --
    // are left where the player put them, exactly as on the hardware.
    void applyPatch (const youknow106::sysex::Patch& patch);
    // Applies any system-exclusive event still waiting for the message thread,
    // now, on the calling thread. The queue is normally drained by the async
    // callback; a test has no message loop to run and needs it deterministic.
    void flushPendingSysEx() { drainSysExQueue(); }
    // Runs the legacy-id bridge now, for tests with no timer.
    void forwardLegacyModeParametersForTest() { forwardLegacyModeParameters(); }

    // Events dropped because the handoff queue was full. Only for tests.
    int getSysExDroppedCount() const noexcept
    {
        return sysExDropped.load (std::memory_order_relaxed);
    }
    // The current panel settings as a patch.
    youknow106::sysex::Patch currentPatch() const;
    // The current patch as a system-exclusive message the hardware would accept.
    juce::MidiMessage currentPatchAsSysEx (int channel) const;

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Call from the message thread. The amount is a proportion of every
    // control's normalised legal range.
    void randomizeParameters (float amount);
    void requestPanic() noexcept { panicRequested.store (true, std::memory_order_release); }
    // Asks for the current panel to be sent out as a patch dump on the next
    // block. The message leaves through the plug-in's MIDI output, which is
    // the only route a host actually exposes to a cable.
    void requestSysExDump();

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
    std::array<ParameterPointer, 41> parameterPointers {};

    youknow106::YouKnow106Engine engine;
    // Events arriving over system exclusive. Parameters cannot be written from
    // the audio callback -- that path notifies the host and may allocate -- so
    // the event is handed to the message thread.
    //
    // What is handed over is the *change*, never a snapshot of the panel. An
    // earlier version staged a whole Patch, which meant serialising every
    // untouched control through the hardware's 7-bit format on the way past and
    // keeping an audio-thread mirror of the panel that went stale the moment
    // anything else moved a parameter. A single-parameter message now carries
    // just its number and value, and the message thread reads the base it
    // applies to from the parameters themselves, where it is always current.
    // The queue is drained from a timer rather than an async callback.
    // triggerAsyncUpdate() takes a lock, which is not something the audio
    // callback may do: a librarian transfer would then be able to stall the
    // callback behind the message thread. Polling costs nothing when the queue
    // is empty, which is almost always.
    void timerCallback() override;
    void forwardLegacyModeParameters();
    int lastLegacyKeyMode { 0 };
    int lastLegacyChorus { 0 };

    // The audio thread's own half of the legacy bridge.
    //
    // Moving the host-visible pair is the message thread's job -- it is the only
    // thread allowed to notify the host -- but the audio may not wait for it.
    // An offline render can complete without the message loop running at all,
    // and even live the pair lags by up to a tick. So the audio thread watches
    // the legacy id itself and renders the mode it names until the pair is back
    // in charge.
    //
    // The hold ends on either of two signals, and it needs both. Agreement --
    // the pair now reads what is being held -- covers a forward that happened
    // before this thread ever saw the change. The forward counter covers the
    // opposite order, where the player moves a switch in the gap between the
    // forward and the next block: the pair then never agrees with what is held,
    // and waiting for it would hold unison down until the legacy id moved again.
    struct LegacyBridge
    {
        int seen { 0 };   // the legacy value this thread last observed
        int held { 0 };   // the mode being rendered until the pair takes over
        bool holding { false };
        int releaseSeen { 0 };  // forwardCount at the moment the hold began
    };
    LegacyBridge keyModeBridge, chorusBridge;
    static int resolveLegacyMode (int legacyValue, int fromPair, int forwardCount,
                                  LegacyBridge& bridge) noexcept;
    // Bumped by the message thread once it has moved a pair, and read by the
    // audio thread as "the pair is authoritative again". Written after the
    // parameter writes and read with matching ordering, so a release always
    // sees at least the pair the forward left behind.
    std::atomic<int> legacyForwardCount { 0 };
    // setStateInformation runs on the message thread, so it cannot touch the
    // bridges directly; it asks the audio thread to reseed them instead.
    std::atomic<bool> reseedLegacyBridges { false };
    void drainSysExQueue();
    // Applies one tone parameter to the parameters it actually names, and to
    // no others.
    void applyToneParameter (int parameter, int value);

    enum class SysExEventKind { FullPatch, SingleParameter };
    struct SysExEvent
    {
        SysExEventKind kind { SysExEventKind::FullPatch };
        // FullPatch: the eighteen tone bytes as they arrived, which are already
        // the message's own representation and so lose nothing in transit.
        std::array<std::uint8_t, youknow106::sysex::toneByteCount> bytes {};
        // SingleParameter: the number and value, and nothing else.
        int parameter { 0 };
        int value { 0 };
    };

    void stageSysExEvent (const SysExEvent& event) noexcept;

    // Translates the pre-split `keyMode` and `chorus` choices in a saved
    // session into the independent button pairs that replaced them.
    static void migrateSplitModeParameters (juce::ValueTree& state);

    // A slot is only written while it is empty and only read while it is full,
    // so the audio thread is never writing bytes the message thread is reading.
    // Sized for a whole bank arriving before the message thread next runs; at
    // MIDI's own rate that is most of a second of stall.
    static constexpr int sysExQueueSlots = 64;
    struct SysExSlot
    {
        SysExEvent event {};
        std::atomic<bool> ready { false };
    };
    std::array<SysExSlot, sysExQueueSlots> sysExQueue {};
    std::atomic<int> sysExWriteIndex { 0 };
    int sysExReadIndex { 0 };
    // Counts events dropped because the queue was full. Zero in any normal
    // session; a test asserts a full bank transfer does not raise it.
    std::atomic<int> sysExDropped { 0 };

    std::atomic<bool> panicRequested { false };
    std::atomic<bool> sysExDumpRequested { false };
    // The dump is assembled here, on the message thread, so the audio callback
    // only has to copy fixed bytes: building a juce::MidiMessage allocates.
    std::array<std::uint8_t, youknow106::sysex::patchMessageBytes> sysExDumpBytes {};
    std::atomic<int> sysExDumpSize { 0 };
    // The channel the connected instrument last spoke on. A dump addressed to
    // channel 1 is ignored by hardware set to anything else.
    std::atomic<int> sysExChannel { 0 };
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
