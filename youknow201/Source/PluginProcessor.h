#pragma once

#include <JuceHeader.h>

#include "DSP/YouKnow201Engine.h"
#include "DSP/YouKnow201Presets.h"

#include <array>
#include <atomic>

namespace youknow201::parameters
{
// Parameter IDs mirror the modelled instrument's parameter contract: one
// complete tone block per part ("up_"/"lo_" prefix), the shared patch-common
// block, and the shared delay/reverb blocks.
[[nodiscard]] inline juce::String toneId (bool upper, const char* name)
{
    return juce::String (upper ? "up_" : "lo_") + name;
}
} // namespace youknow201::parameters

class YouKnow201AudioProcessor final : public juce::AudioProcessor
{
public:
    YouKnow201AudioProcessor();
    ~YouKnow201AudioProcessor() override = default;

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
    // Computed from the current patch: amp release plus delay repeats down to
    // -60 dB plus reverb RT60, capped — see the implementation.
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Editor helpers.
    [[nodiscard]] float getOutputLevel (int channel) const noexcept
    {
        return engine.getOutputLevel (channel);
    }
    [[nodiscard]] int getActiveVoiceCount() const noexcept
    {
        return activeVoices.load (std::memory_order_relaxed);
    }
    void triggerFromUi (int note, int velocity) noexcept;
    void releaseFromUi (int note) noexcept;

    // Reads the current parameter values into an engine patch. Shared by the
    // audio thread and tests, so the two can never disagree.
    [[nodiscard]] youknow201::Patch snapshotPatch() const;

    juce::AudioProcessorValueTreeState parameters;

    static juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();

private:
    void handleMidiMessage (const juce::MidiMessage& message);
    void handleController (int controller, int value);
    void applyProgram (int index);
    void applyProgramAsync (int index);
    void cacheParameterPointers();

    // Audio-thread lookups resolved once at construction: raw-value atomics
    // aligned with the binding tables, and ranged parameters for the CC map.
    // processBlock must never build a juce::String.
    std::vector<std::atomic<float>*> upperValues, lowerValues, patchValues;
    std::atomic<float>* masterValue { nullptr };
    struct CachedCc
    {
        int controller { -1 };
        juce::RangedAudioParameter* parameter { nullptr };
        bool signedValue { false };
        bool keyFollow { false };
    };
    std::vector<CachedCc> ccCache;
    std::vector<float> monoScratch;

    youknow201::Engine engine;
    std::atomic<int> activeVoices { 0 };
    std::atomic<int> currentProgram { 0 };

    struct UiNoteEvent
    {
        int note { -1 };
        int velocity { 0 };  // 0 = note-off
    };
    static constexpr unsigned uiQueueCapacity = 64;
    std::array<UiNoteEvent, uiQueueCapacity> uiQueue {};
    std::atomic<unsigned> uiWrite { 0 };
    std::atomic<unsigned> uiRead { 0 };

    JUCE_DECLARE_WEAK_REFERENCEABLE (YouKnow201AudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouKnow201AudioProcessor)
};
