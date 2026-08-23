#pragma once

#include <JuceHeader.h>

#include "DSP/SeptumEngine.h"
#include "DSP/SeptumPresets.h"

#include <array>
#include <atomic>
#include <memory>

namespace septum::parameters
{
// Parameter IDs mirror the modelled instrument's parameter contract: one
// complete tone block per part ("up_"/"lo_" prefix), the shared patch-common
// block, and the shared delay/reverb blocks.
[[nodiscard]] inline juce::String toneId (bool upper, const char* name)
{
    return juce::String (upper ? "up_" : "lo_") + name;
}
} // namespace septum::parameters

class SeptumAudioProcessor final : public juce::AudioProcessor
{
public:
    SeptumAudioProcessor();
    ~SeptumAudioProcessor() override = default;

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
    [[nodiscard]] septum::Patch snapshotPatch() const;

    // The external-input path's settings, read the same way. Kept out of the
    // patch because the instrument keeps them out of it (OM pp. 49-51).
    [[nodiscard]] septum::ExternalInput snapshotExternalInput() const;

    // The message-thread half of a MIDI program change: repeats the values
    // the audio path already wrote, with host/UI notification, skipping any
    // parameter edited since. Normally reached via the queued message-loop
    // callback; public so the harness can stand in for that loop.
    void reconcileProgram (int index);

    // The message-thread half of a received panel CC: republishes the raw
    // values the audio path wrote, with host and UI notification. Public so
    // the harness can stand in for the message loop.
    void reconcileControlChanges();

    // Loads an entire structured patch into the processor and APVTS.
    void loadPatch (const septum::Patch& patch);

    // The two halves of that, split so a patch arriving on the audio thread
    // (a received SysEx dump) writes only atomics there and is republished to
    // the host and the UI from the message loop. Public so the harness can
    // stand in for that loop.
    // `publishGrid` puts the arpeggio grid into the same generation-odd
    // window as the parameters, so a concurrent state save cannot pair one
    // patch revision's parameters with another's grid.
    void writePatchToParameters (const septum::Patch& patch,
                                 bool publishGrid = false) noexcept;
    void republishPatchParameters();
    // Point the shadows back at whatever the parameters now hold and drop any
    // pending republish. Called by the writers that run on the message thread,
    // so a program change or a preset load that lands between a dump and its
    // republish is not undone by it: last writer wins.
    void syncPatchShadows() noexcept;

    // Everything a queued republish could still put back, dropped. A state
    // restore replaces the whole parameter tree, so a CC, a dump or a
    // device-control message whose republish has not run yet must not land on
    // top of the session that was just loaded.
    void cancelPendingRepublishes() noexcept;

    // The same half for the three Universal Realtime device-control messages.
    // Public so the harness can stand in for the message loop.
    void republishSystemParameters();

    // Parses and loads SysEx .syx bytes into the active patch.
    void loadSysExData (const void* data, std::size_t sizeInBytes);

    // Encodes the current patch into a Roland SH-201 SysEx .syx byte buffer.
    [[nodiscard]] std::vector<std::uint8_t> createSysExDataForCurrentPatch() const;

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
    // Pushes the SYSTEM COMMON settings at the engine. Allocation-free.
    void applySystemSettings() noexcept;

    // Audio-thread lookups resolved once at construction: raw-value atomics
    // aligned with the binding tables, and ranged parameters for the CC map.
    // processBlock must never build a juce::String.
    std::vector<std::atomic<float>*> upperValues, lowerValues, patchValues;
    // The audio thread's own copy of everything a received SysEx dump writes,
    // and which of those it has written since the message thread last looked.
    //
    // The vectors above are the parameter objects' own storage, and
    // setValueNotifyingHost writes it — so republishing a value read a moment
    // earlier put that older value back over whatever a later packet of the
    // same dump had stored in between. A dump is 22 packets and a whole block
    // of them could be lost that way. The shadows are what the republish
    // reads, and the mask keeps it from touching anything the audio thread has
    // not written, so a program change or a preset load on the message thread
    // is never republished over.
    std::unique_ptr<std::atomic<float>[]> upperShadow, lowerShadow, patchShadow;
    std::atomic<bool> patchDirty { false };

    // The arpeggio grid a received patch dump carried.
    //
    // It is the one piece of documented patch data with no plug-in parameter
    // to live in: a 32 x 16 grid of cells with one Original Note per row,
    // sixteen SysEx blocks of it. Without somewhere to keep it, every decoded
    // row was thrown away by the next `snapshotPatch()` — which rebuilds the
    // style from the selector — so a pattern imported from real hardware
    // neither played nor survived a re-export. It is kept here and used in
    // place of the selected template while the selector stays where it was
    // when the dump arrived; moving the selector picks a template, which is
    // what the hardware's panel does too.
    //
    // Published into a ring of slots, because the audio thread writes it (a
    // dump is decoded in the render callback) and reads it, and the message
    // thread reads it to save the session and writes it to restore one.
    //
    // A plain seqlock over one buffer would let a reader's copy overlap a
    // writer's — it detects the tear and retries, but the overlapping access
    // is a data race in its own right. A ring means the slot being written is
    // never the slot being published, so a reader would have to be overtaken
    // by `slotCount` further publishes before it even shared memory with a
    // writer; the published counter is still checked afterwards, so a reader
    // that was overtaken retries rather than returning a torn grid.
    struct ImportedArpeggioStyle
    {
        // Comfortably more than the 22 blocks of a whole patch dump, so even
        // a reader preempted across an entire dump is not lapped. It is a
        // bound rather than a proof — see the note on the reader below.
        static constexpr std::size_t slotCount = 32;
        struct Slot
        {
            septum::ArpeggioStyle style {};
            // The selector this grid belongs to travels *in* the slot. Held
            // in an atomic of its own it could be observed a moment ahead of
            // its payload — a reader passing the selector check and then
            // copying the previous slot as though it were the new one.
            int selector { -1 };
        };
        std::array<Slot, slotCount> slots {};
        // Handed out to writers, so two of them never pick the same slot.
        std::atomic<std::uint32_t> reserved { 0 };
        // How many publishes have completed; the newest is slot
        // (published - 1) % slotCount, and zero means nothing is published.
        // This one store publishes the grid and its selector together.
        std::atomic<std::uint32_t> published { 0 };
        std::atomic<bool> valid { false };
    };
    // Mutable because `snapshotPatch()` is const and retires the grid when it
    // finds the selector has moved: the store is two atomics, and the object
    // is a cache in front of the parameters rather than part of them.
    mutable ImportedArpeggioStyle importedArpeggio;

    void publishImportedArpeggioStyle (const septum::ArpeggioStyle& style,
                                       int selector) noexcept;
    // Drop it. A factory program carries its own style, and the selector is
    // only a key: without this, a program whose style index happened to match
    // the one an imported grid arrived under played the imported grid instead
    // of its own template.
    void invalidateImportedArpeggioStyle() const noexcept;
    [[nodiscard]] bool readImportedArpeggioStyle (
        int selector, septum::ArpeggioStyle& out) const noexcept;
    void writeImportedArpeggioToState (juce::ValueTree& state) const;
    void readImportedArpeggioFromState (const juce::ValueTree& state);
    std::atomic<float>* masterValue { nullptr };
    // SYSTEM COMMON: master tune in Hz, then key shift, keyboard octave and
    // transpose in the order systemParameterIds() lists them.
    std::atomic<float>* systemTuneValue { nullptr };
    std::vector<std::atomic<float>*> systemValues;
    std::vector<std::atomic<float>*> externalValues;
    // The input bus arrives in the same buffer the output is written to, so
    // it is copied out before that buffer is cleared.
    std::vector<float> externalInputL, externalInputR;
    struct CachedCc
    {
        int controller { -1 };
        juce::RangedAudioParameter* parameter { nullptr };
        // The parameter's own raw value, which is what the engine snapshots.
        // The audio thread writes this and nothing else; the parameter object
        // and the host are caught up on the message thread.
        std::atomic<float>* raw { nullptr };
        bool signedValue { false };
        bool keyFollow { false };
    };
    std::vector<CachedCc> ccCache;
    // The value the audio thread last wrote, kept where only it writes.
    //
    // `CachedCc::raw` is the parameter object's own storage, and
    // setValueNotifyingHost writes it: publishing a value read a moment
    // earlier put that older value back over anything a CC had stored in
    // between, and the dirty bit the newer CC had set then made the next pass
    // republish the stale value it had just been overwritten with. The
    // controller's value was lost outright, not merely delayed. The shadow is
    // what the message-thread pass reads, so the newest value always wins.
    std::unique_ptr<std::atomic<float>[]> ccShadow;
    // Which cached CCs the audio thread has written since the message thread
    // last looked. One bit per entry in ccCache.
    std::array<std::atomic<std::uint64_t>, 2> ccDirty { 0u, 0u };
    // Catches the parameter objects and the host up with the raw values a
    // received CC wrote on the audio thread. Coalescing, so a knob sweep of
    // 128 messages a second costs one message-thread pass per frame rather
    // than 128.
    struct CcReconciler final : public juce::AsyncUpdater
    {
        explicit CcReconciler (SeptumAudioProcessor& o) : owner (o) {}
        ~CcReconciler() override { cancelPendingUpdate(); }
        void handleAsyncUpdate() override { owner.reconcileControlChanges(); }
        SeptumAudioProcessor& owner;
    };
    CcReconciler ccReconciler { *this };
    // The same shape for a whole patch, after a SysEx dump lands on the audio
    // path.
    struct PatchReconciler final : public juce::AsyncUpdater
    {
        explicit PatchReconciler (SeptumAudioProcessor& o) : owner (o) {}
        ~PatchReconciler() override { cancelPendingUpdate(); }
        void handleAsyncUpdate() override { owner.republishPatchParameters(); }
        SeptumAudioProcessor& owner;
    };
    PatchReconciler patchReconciler { *this };

    // [settled] Universal Realtime device control: "the Universal Realtime
    // messages ... will be set automatically" (MIDI Implementation v1.00
    // p. 2). Three of them name SYSTEM COMMON parameters this replica
    // publishes -- Master Volume, Master Fine Tuning, Master Coarse Tuning --
    // so they are received onto those parameters. They arrive on the audio
    // thread and take the same split every other received message does: raw
    // atomics here, host and UI notification from the queued pass.
    bool handleDeviceControlSysEx (const std::uint8_t* data,
                                   std::size_t size) noexcept;
    // MASTER LEVEL, MASTER TUNE, MASTER KEY SHIFT, in that order.
    static constexpr std::size_t deviceControlCount = 3;
    std::array<juce::RangedAudioParameter*, deviceControlCount>
        deviceControlParameters { nullptr, nullptr, nullptr };
    std::array<std::atomic<float>*, deviceControlCount>
        deviceControlValues { nullptr, nullptr, nullptr };
    // The audio thread's own copy, for the reason `ccShadow` exists: the
    // pointers above are the parameter objects' storage and
    // setValueNotifyingHost writes it, so publishing a value read a moment
    // earlier put it back over a message that had arrived in between.
    std::array<std::atomic<float>, deviceControlCount> deviceControlShadow {};
    std::atomic<unsigned> deviceControlDirty { 0u };
    struct SystemReconciler final : public juce::AsyncUpdater
    {
        explicit SystemReconciler (SeptumAudioProcessor& o) : owner (o) {}
        ~SystemReconciler() override { cancelPendingUpdate(); }
        void handleAsyncUpdate() override { owner.republishSystemParameters(); }
        SeptumAudioProcessor& owner;
    };
    SystemReconciler systemReconciler { *this };
    std::vector<float> monoScratch;

    septum::Engine engine;
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
    // Seqlock guard for multi-parameter write bursts — message-thread program
    // sprays and state restores, and the audio path's own program writes:
    // odd while a burst is in flight, bumped again when it completes. The
    // audio thread discards a patch snapshot that saw a burst and keeps the
    // previous block's patch; a state save retries its raw-value copy.
    std::atomic<std::uint32_t> patchGeneration { 0 };

    JUCE_DECLARE_WEAK_REFERENCEABLE (SeptumAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeptumAudioProcessor)
};
