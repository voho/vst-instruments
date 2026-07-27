#pragma once

#include <JuceHeader.h>

#include "DSP/ElectryEngine.h"
#include "DSP/ElectryFx.h"
#include "DSP/ElectryVisuals.h"

#include <array>
#include <atomic>
#include <cstdint>

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
inline constexpr auto artifacts      = "artifacts";
inline constexpr auto outputMode     = "outputMode";
inline constexpr auto distortion     = "distortion";
inline constexpr auto amp            = "amp";
inline constexpr auto compressor     = "compressor";
inline constexpr auto delay          = "delay";
inline constexpr auto room           = "room";
// Version 1.1 additions, appended so every existing automation index is kept.
inline constexpr auto sympathetic    = "sympathetic";
inline constexpr auto palmMute       = "palmMute";
inline constexpr auto strumSpread    = "strumSpread";
// Version 1.2 repurposed this slot from the CC1 vibrato to the CC1 resonance
// control. The stored ID is kept so existing sessions and automation lanes
// keep pointing at the same parameter index.
inline constexpr auto resonanceDepth = "vibratoDepth";
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
    int getSympatheticStringCount() const noexcept
    {
        return sympatheticStringCount.load (std::memory_order_relaxed);
    }
    // One packed 32-bit word per string, published by the audio thread and
    // read by the editor's timer. Each word is self-consistent, so the
    // fretboard can never draw one string's note with another string's level.
    electry::StringVisualState getStringVisualState (int stringIndex) const noexcept
    {
        if (stringIndex < 0 || stringIndex >= electry::ElectryEngine::stringCount)
            return {};
        return electry::visuals::unpackStringVisual (
            stringVisuals[static_cast<std::size_t> (stringIndex)].load (
                std::memory_order_relaxed));
    }
    int getCurrentPickStyleIndex() const noexcept
    {
        return pickStyleIndex.load (std::memory_order_relaxed);
    }
    int getCurrentPlayStyleIndex() const noexcept
    {
        return playStyleIndex.load (std::memory_order_relaxed);
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
        std::atomic<float>* artifacts = nullptr;
        std::atomic<float>* outputMode = nullptr;
        std::atomic<float>* distortion = nullptr;
        std::atomic<float>* amp = nullptr;
        std::atomic<float>* compressor = nullptr;
        std::atomic<float>* delay = nullptr;
        std::atomic<float>* room = nullptr;
        std::atomic<float>* sympathetic = nullptr;
        std::atomic<float>* palmMute = nullptr;
        std::atomic<float>* strumSpread = nullptr;
        std::atomic<float>* resonanceDepth = nullptr;
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
    void updateEffectParameters() noexcept;
    void publishStringVisualState() noexcept;

    electry::ElectryEngine engine;
    // The amplifier, cabinet and time effects live in the JUCE-free DSP
    // library alongside the string model, so the complete signal path is
    // regression tested on every platform rather than only inside a host.
    electry::ElectryFx effects;
    std::array<electry::StringVisualState,
               electry::ElectryEngine::stringCount> visualScratch {};
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineReady { false };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> sympatheticStringCount { 0 };
    std::atomic<int> pickStyleIndex { 0 };
    std::atomic<int> playStyleIndex { 0 };
    std::atomic<double> displaySampleRate { 0.0 };
    std::array<std::atomic<std::uint32_t>,
               electry::ElectryEngine::stringCount> stringVisuals {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElectryAudioProcessor)
};
