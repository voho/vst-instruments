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

    // The message-thread half of a MIDI program change: repeats the values
    // the audio path already wrote, with host/UI notification, skipping any
    // parameter edited since. Normally reached via the queued message-loop
    // callback; public so the harness can stand in for that loop.
    void reconcileProgram (int index);

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
    // Writes a factory program straight into the cached raw-value atomics.
    // Allocation-free, so the audio thread can land a MIDI program change
    // without depending on the message loop ever running; the queued
    // reconcileProgram then repeats the untouched values with host/UI
    // notification.
    void writeProgramToParameters (int index) noexcept;
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
    // Set only while applyProgram sprays a program into the APVTS on the
    // message thread: the audio path then renders that factory patch
    // atomically instead of a half-updated parameter snapshot. MIDI program
    // changes do not stage — they write the raw values directly.
    std::atomic<int> stagedProgram { -1 };
    // Seqlock guard for snapshotPatch against message-thread multi-parameter
    // writes (program sprays, state restores): odd while a write burst is in
    // flight, bumped again when it completes. A snapshot that saw a burst is
    // discarded and the engine keeps the previous block's patch.
    std::atomic<std::uint32_t> patchGeneration { 0 };

    JUCE_DECLARE_WEAK_REFERENCEABLE (YouKnow201AudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouKnow201AudioProcessor)
};
