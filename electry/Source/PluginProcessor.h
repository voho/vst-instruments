#pragma once

#include <JuceHeader.h>

#include "DSP/ElectryEngine.h"

#include <array>
#include <atomic>

namespace electry::parameters
{
inline constexpr auto pickupSelector = "pickupSelector";
inline constexpr auto pickupType     = "pickupType";
inline constexpr auto tone           = "tone";
inline constexpr auto bodyWood       = "bodyWood";
inline constexpr auto bodySize       = "bodySize";
inline constexpr auto bodyShape      = "bodyShape";
inline constexpr auto construction   = "construction";
inline constexpr auto scaleLength    = "scaleLength";
inline constexpr auto bodyResonance  = "bodyResonance";
inline constexpr auto stringGauge    = "stringGauge";
inline constexpr auto stringAge      = "stringAge";
inline constexpr auto pickPosition   = "pickPosition";
inline constexpr auto pickHardness   = "pickHardness";
inline constexpr auto pickNoise      = "pickNoise";
inline constexpr auto fingerNoise    = "fingerNoise";
inline constexpr auto releaseNoise   = "releaseNoise";
inline constexpr auto muteDamping    = "muteDamping";
inline constexpr auto bendTime       = "bendTime";
inline constexpr auto velocity       = "velocity";
inline constexpr auto output         = "output";
} // namespace electry::parameters

class ElectryAudioProcessor final : public juce::AudioProcessor,
                                    private juce::MidiKeyboardState::Listener
{
public:
    ElectryAudioProcessor();
    ~ElectryAudioProcessor() override;

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
    // The longest open-string decay reaches -60 dB in about 9 s; the engine
    // retires a voice near -100 dB, and the release stage adds a bounded
    // damped tail. 18 s covers the complete audible ring-out.
    static constexpr double maximumTailLengthSeconds = 18.0;
    double getTailLengthSeconds() const override { return maximumTailLengthSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Message-thread entry points used by the editor. Both route through the
    // same bounded lock-free queue as on-screen keyboard notes.
    void triggerArticulation (int articulationIndex);
    void requestPanic() noexcept { panicRequested.store (true, std::memory_order_release); }

    int getActiveVoiceCount() const noexcept
    {
        return activeVoiceCount.load (std::memory_order_relaxed);
    }
    int getCurrentArticulationIndex() const noexcept
    {
        return articulationIndex.load (std::memory_order_relaxed);
    }
    double getCurrentSampleRateForDisplay() const noexcept
    {
        return displaySampleRate.load (std::memory_order_relaxed);
    }
    bool isEngineReady() const noexcept
    {
        return engineReady.load (std::memory_order_acquire);
    }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct ParameterPointers
    {
        std::atomic<float>* pickupSelector = nullptr;
        std::atomic<float>* pickupType = nullptr;
        std::atomic<float>* tone = nullptr;
        std::atomic<float>* bodyWood = nullptr;
        std::atomic<float>* bodySize = nullptr;
        std::atomic<float>* bodyShape = nullptr;
        std::atomic<float>* construction = nullptr;
        std::atomic<float>* scaleLength = nullptr;
        std::atomic<float>* bodyResonance = nullptr;
        std::atomic<float>* stringGauge = nullptr;
        std::atomic<float>* stringAge = nullptr;
        std::atomic<float>* pickPosition = nullptr;
        std::atomic<float>* pickHardness = nullptr;
        std::atomic<float>* pickNoise = nullptr;
        std::atomic<float>* fingerNoise = nullptr;
        std::atomic<float>* releaseNoise = nullptr;
        std::atomic<float>* muteDamping = nullptr;
        std::atomic<float>* bendTime = nullptr;
        std::atomic<float>* velocity = nullptr;
        std::atomic<float>* output = nullptr;
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
    void discardUiMidiEvents() noexcept;
    void dispatchUiMidiEvents() noexcept;
    void dispatchMidiData (const juce::uint8* data, int numBytes) noexcept;
    void updateEngineParameters() noexcept;

    electry::ElectryEngine engine;
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineReady { false };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> articulationIndex { 0 };
    std::atomic<double> displaySampleRate { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElectryAudioProcessor)
};
