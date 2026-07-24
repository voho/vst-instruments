#pragma once

#include <JuceHeader.h>

#include "DSP/VoiceEngine.h"

#include <array>
#include <atomic>

namespace vocalor::parameters
{
inline constexpr auto profile      = "profile";
inline constexpr auto mode         = "mode";
inline constexpr auto vowel        = "vowel";
inline constexpr auto chordQuality = "chordQuality";
inline constexpr auto choirSize    = "choirSize";
inline constexpr auto breath       = "breath";
inline constexpr auto resonance    = "resonance";
inline constexpr auto vibrato      = "vibrato";
inline constexpr auto humanize     = "humanize";
inline constexpr auto spread       = "spread";
inline constexpr auto tension      = "tension";
inline constexpr auto room         = "room";
inline constexpr auto output       = "output";
// Added in 1.1. These IDs are appended after the version-1 set so existing
// host automation lanes keep pointing at the same parameters.
inline constexpr auto legato       = "legato";
inline constexpr auto vowelX       = "vowelX";
inline constexpr auto vowelY       = "vowelY";
inline constexpr auto vowelMorph   = "vowelMorph";
inline constexpr auto formantShift = "formantShift";
inline constexpr auto glide        = "glide";
inline constexpr auto roomSize     = "roomSize";
} // namespace vocalor::parameters

class VocalorAudioProcessorEditor;

class VocalorAudioProcessor final : public juce::AudioProcessor,
                                   private juce::MidiKeyboardState::Listener
{
public:
    VocalorAudioProcessor();
    ~VocalorAudioProcessor() override;

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
    // The release envelope runs for about two seconds and the largest room
    // setting keeps ringing after it, so the advertised tail has headroom.
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void requestPanic() noexcept { panicRequested.store (true, std::memory_order_release); }
    int getActiveVoiceCount() const noexcept { return activeVoiceCount.load (std::memory_order_relaxed); }
    double getCurrentSampleRateForDisplay() const noexcept
    {
        return displaySampleRate.load (std::memory_order_relaxed);
    }
    bool isEngineReady() const noexcept { return engineReady.load (std::memory_order_acquire); }

    /** Lock-free snapshot of the running tract, meters and vowel position.
        Safe to call from the message thread while audio is rendering. */
    vocalor::EngineDisplayState getDisplayState() const noexcept
    {
        return engine.getDisplayState();
    }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct ParameterPointers
    {
        std::atomic<float>* profile = nullptr;
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* vowel = nullptr;
        std::atomic<float>* chordQuality = nullptr;
        std::atomic<float>* choirSize = nullptr;
        std::atomic<float>* breath = nullptr;
        std::atomic<float>* resonance = nullptr;
        std::atomic<float>* vibrato = nullptr;
        std::atomic<float>* humanize = nullptr;
        std::atomic<float>* spread = nullptr;
        std::atomic<float>* tension = nullptr;
        std::atomic<float>* room = nullptr;
        std::atomic<float>* output = nullptr;
        std::atomic<float>* legato = nullptr;
        std::atomic<float>* vowelX = nullptr;
        std::atomic<float>* vowelY = nullptr;
        std::atomic<float>* vowelMorph = nullptr;
        std::atomic<float>* formantShift = nullptr;
        std::atomic<float>* glide = nullptr;
        std::atomic<float>* roomSize = nullptr;
    } parameterPointers;

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
    void dispatchUiMidiEvents() noexcept;
    void dispatchMidiData (const juce::uint8* data, int numBytes) noexcept;
    void updateEngineParameters() noexcept;

    vocalor::VoiceEngine engine;
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineReady { false };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<double> displaySampleRate { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalorAudioProcessor)
};
