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
    // The on-screen bend/modulation lever, mirroring the hardware's lever
    // left of the keys. Applied on the audio thread each block.
    void setLeverFromUi (float bendMinus1to1, float mod0to1) noexcept
    {
        uiBend.store (bendMinus1to1, std::memory_order_relaxed);
        uiMod.store (mod0to1, std::memory_order_relaxed);
        uiLeverDirty.store (true, std::memory_order_release);
    }

    // Reads the current parameter values into an engine patch. Shared by the
    // audio thread and tests, so the two can never disagree.
    [[nodiscard]] youknow201::Patch snapshotPatch() const;

    juce::AudioProcessorValueTreeState parameters;

    static juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();

private:
    // Both return true when the event edited a patch parameter, so the audio
    // path can refresh the engine patch before rendering the next segment.
    bool handleMidiMessage (const juce::MidiMessage& message);
    bool handleController (int controller, int value);
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
    std::atomic<float> uiBend { 0.0f };
    std::atomic<float> uiMod { 0.0f };
    std::atomic<bool> uiLeverDirty { false };
    // When the UI queue overflows, a note-off must still reach the engine
    // eventually: the release is latched here and applied on the next block.
    std::array<std::atomic<std::uint64_t>, 2> forcedRelease { 0u, 0u };
    // A program selected from the audio path (MIDI program change) or being
    // sprayed into the APVTS on the message thread: while set, the audio
    // path renders the staged factory patch atomically instead of a
    // possibly half-updated parameter snapshot.
    std::atomic<int> stagedProgram { -1 };

    JUCE_DECLARE_WEAK_REFERENCEABLE (YouKnow201AudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouKnow201AudioProcessor)
};
