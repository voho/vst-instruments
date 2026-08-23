#pragma once

#include <JuceHeader.h>

#include "DSP/YouKnow106Engine.h"
#include "DSP/YouKnow106Presets.h"
#include "DSP/YouKnow106SysEx.h"
#include "DSP/YouKnow106Panel.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>

class YouKnow106AudioProcessorEditor;

class YouKnow106AudioProcessor final : public juce::AudioProcessor,
                                       private juce::MidiKeyboardState::Listener,
                                       private juce::AudioProcessorValueTreeState::Listener,
                                       private juce::Timer
{
public:
    YouKnow106AudioProcessor();
    ~YouKnow106AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&,
                               juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // Measured on the shipping engine, six voices with the longest release,
    // from the moment the last key lifts: with the chorus off -- or engaged
    // with Chorus Noise at zero -- the output falls below -140 dBFS after
    // 25.41 s and reaches bit-exact zero after 53.8 s (53.3-53.9 s over
    // 44.1/48 kHz and 1x/4x). With the chorus engaged and its noise present it
    // never reaches zero: the modelled analogue floor is part of the effect,
    // and it is still running after two minutes.
    //
    // 25.55 s is `envelopeReleaseSeconds(1.0f)`, the modelled envelope's own
    // longest release. It is not the instrument's tail, and the output
    // coupling takes roughly as long again to settle after it -- so a tail
    // reported from the envelope alone would truncate the instrument by half.
    //
    // Infinity is therefore reported unconditionally: it is right whenever the
    // chorus is running, and a value that changed with a patch would have to
    // be re-announced to hosts that cache it. The cost is that a host cannot
    // idle-suspend an instance whose patch has the chorus off, which
    // Docs/RELEASE_CHECKLIST.md carries as an explicit accept-or-qualify.
    double getTailLengthSeconds() const override
    {
        return std::numeric_limits<double>::infinity();
    }

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
    // Whether the panel has moved away from the complete product program that
    // was last selected. Hardware patches remain narrower; applyPatch() below
    // deliberately retains the JUNO's 18-byte semantics.
    bool currentProgramIsEdited() const;

    // Applies a stored patch to the parameters. The controls a patch does not
    // carry -- volume, the bender depths, portamento and the assign mode --
    // are left where the player put them, exactly as on the hardware.
    void applyPatch (const youknow106::sysex::Patch& patch);
    // Applies any MIDI event still waiting for the message thread now, on the
    // calling thread. The queue is normally drained by the timer; a test has no
    // message loop to run and needs it deterministic.
    void flushPendingMidiEvents() { drainPendingMidiQueue(); }
    // Runs the legacy-id bridge now, for tests with no timer.
    void forwardLegacyModeParametersForTest() { forwardLegacyModeParameters(); }
    // Installs a one-shot barrier immediately after an audio parameter snapshot
    // has passed its first generation check. This deterministically exercises
    // reflection completing in the narrow retirement window; production never
    // installs it, so the callback performs only two null checks.
    void setMidiReflectionSnapshotBarrierForTest (
        std::atomic<bool>* captured, std::atomic<bool>* resume) noexcept
    {
        midiReflectionSnapshotCapturedForTest = captured;
        midiReflectionSnapshotResumeForTest = resume;
    }

    // Hard event drops after overflow recovery. Only for tests.
    int getMidiEventDroppedCount() const noexcept
    {
        return pendingMidiDropped.load (std::memory_order_relaxed);
    }
    // The current panel settings as a patch.
    youknow106::sysex::Patch currentPatch() const;
    // The current patch as a system-exclusive message the hardware would accept.
    juce::MidiMessage currentPatchAsSysEx (int channel) const;
    // Message thread. Scans a .syx file image for the instrument's own
    // F0 41 30 0n patch dumps, applies the first and counts every one the
    // file carries. Anything else in the stream -- other makers, other
    // opcodes, single-parameter messages -- is passed over, exactly as the
    // live MIDI path passes it over.
    bool importPatchSysExBytes (const void* data, std::size_t size,
                                int& patchesFound);
    // The channel incoming SysEx last spoke on; exports answer on it.
    int sysExMidiChannel() const noexcept
    {
        return sysExChannel.load (std::memory_order_relaxed);
    }

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Call from the message thread. The amount is a proportion of every
    // control's normalised legal range.
    void randomizeParameters (float amount);
    void requestPanic() noexcept { panicRequested.store (true, std::memory_order_release); }
    // The on-screen performance lever is event-like controller input, not a
    // stored parameter. A one-slot latest-value mailbox coalesces dense mouse
    // drags without ever losing the final spring return; after consumption it
    // stays empty, so later external MIDI is not overwritten every block.
    void postUiLeverPosition (float normalisedBipolar, float modulation) noexcept;
    // Re-pressing the currently selected POLY mode is an event on the hardware
    // even though its latched parameter value does not change.
    void requestKeyModeReassert() noexcept
    {
        // A parameter listener cannot observe a value that did not change.
        // Publish the current pair explicitly so this later physical-button
        // event also supersedes any obsolete-id automation still waiting for
        // the compatibility timer.
        publishKeyModePairAuthority();
        keyModeReassertSequence.fetch_add (1, std::memory_order_relaxed);
        keyModeReassertRequested.store (true, std::memory_order_release);
    }
    // Narrow UI regression hook: the Engine suite separately verifies what a
    // consumed event does to held assignments.
    unsigned getKeyModeReassertSequenceForTest() const noexcept
    {
        return keyModeReassertSequence.load (std::memory_order_relaxed);
    }
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
    // The quality ladder as the panel offers it: index 0..2 selects 1x, 2x or
    // 4x. The engine may run a *lower* factor than the selected one when the
    // host rate is already high enough, which is what the display factor above
    // reports; this is the request.
    // Both directions are derived from the engine's own ladder rather than
    // restating it, so a rung added there appears on the panel by itself.
    static constexpr int qualityChoiceCount = static_cast<int> (
        youknow106::YouKnow106Engine::oversampleFactors.size());
    static constexpr int vcfTanhChoiceCount = 2;
    static constexpr int vcfFastEarlyChoiceCount = 2;
    static constexpr int vcfSolverChoiceCount = 3;
    // The rung a new instance -- and a state saved before this parameter
    // existed -- starts on. Not the engine's own default: `EngineParameters`
    // keeps `Merson x2`, so the JUCE-free suites and the frozen fingerprint
    // and work-counter contracts still test the reference kernel. See the
    // note over the layout entry.
    static constexpr int vcfSolverDefaultChoice = 2;
    // Named once, so the layout, the editor and the tests cannot disagree
    // about which rung an ordinal is.
    //
    // These are the player's names, not the method's: what a menu has to
    // convey is how much the rung costs relative to the others, and the
    // tableau that delivers it belongs in the tooltip. Ordinal zero is the
    // most expensive because it is the historical reference, so the list
    // reads downward from Max to the shipped Normal -- the opposite direction
    // from QUALITY, which climbs.
    //
    // They are also short on purpose. JUCE lays a ComboBox's text out in
    // `width + 3 - height`, the Model zone's padding caps the selector at
    // about 92 px, and at the minimum editor size that leaves room for roughly
    // six characters. Longer words drew as "Sta...".
    static constexpr const char* vcfSolverChoiceName (int choice) noexcept
    {
        switch (choice)
        {
            case 1:  return "High";
            case 2:  return "Normal";
            default: return "Max";
        }
    }
    // The tableau behind each name, for the tooltip and for documentation.
    static constexpr const char* vcfSolverChoiceTechnique (int choice) noexcept
    {
        switch (choice)
        {
            case 1:  return "two half-interval classic RK4 steps, "
                            "8 evaluations";
            case 2:  return "one full-interval classic RK4 step, "
                            "4 evaluations";
            default: return "two half-interval five-stage Merson steps, "
                            "10 evaluations";
        }
    }
    static constexpr int oversamplingFactorForChoice (int choice) noexcept
    {
        return youknow106::YouKnow106Engine::oversampleFactors[
            static_cast<std::size_t> (
                std::clamp (choice, 0, qualityChoiceCount - 1))];
    }
    // The rung that runs `factor`, or the deepest one at or below it.
    static constexpr int choiceForOversamplingFactor (int factor) noexcept
    {
        const auto& factors = youknow106::YouKnow106Engine::oversampleFactors;
        int choice = 0;
        for (int index = 1; index < qualityChoiceCount; ++index)
            if (factors[static_cast<std::size_t> (index)] <= factor)
                choice = index;
        return choice;
    }
    int getQualityChoice() const noexcept;
    bool isEngineReady() const noexcept
    {
        return engineReady.load (std::memory_order_acquire);
    }
    float getTemperatureForDisplay() const noexcept
    {
        return displayTemperature.load (std::memory_order_relaxed);
    }
    float getRailDroopForDisplay() const noexcept
    {
        return displayRailDroop.load (std::memory_order_relaxed);
    }
    void getOscilloscopeBuffer (std::array<float, 256>& destination) const noexcept
    {
        const std::size_t currentWrite = scopeWriteIndex.load (std::memory_order_acquire);
        for (std::size_t i = 0; i < 256; ++i)
        {
            const std::size_t srcIdx = (currentWrite + i) % 256;
            destination[i] = scopeBuffer[srcIdx].load (std::memory_order_relaxed);
        }
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
    // A host may call reset() from its processing thread. MidiKeyboardState's
    // reset takes a lock, so its visual cleanup is deferred to the existing
    // message-thread timer while the engine and audio queues reset immediately.
    std::atomic<bool> keyboardResetRequested { false };

    static constexpr std::uint32_t emptyUiLeverMailbox = 0xffffffffu;
    std::atomic<std::uint32_t> uiLeverMailbox { emptyUiLeverMailbox };

    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel,
                       int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel,
                        int midiNoteNumber, float velocity) override;
    void parameterChanged (const juce::String& parameterId,
                           float newValue) override;
    void enqueueUiMidiEvent (int note, float velocity, bool isNoteOn) noexcept;
    void discardUiMidiEvents() noexcept;
    void dispatchUiMidiEvents() noexcept;
    void dispatchUiPerformanceControls() noexcept;
    // Returns true only when one complete, stable parameter snapshot reached
    // the engine. Multi-parameter writers leave the previous snapshot in place.
    bool updateEngineParameters() noexcept;
    // Which factory program was last selected. Only a display value: the
    // patch itself lives in the parameters once it is applied.
    std::atomic<int> currentProgram { 0 };

    float valueOf (const char* parameterId) const noexcept;
    int choiceOf (const char* parameterId, int maximum) const noexcept;

    struct ParameterPointer
    {
        const char* id = nullptr;
        std::atomic<float>* value = nullptr;
    };
    // Direct indices for the audio-thread snapshot. The constructor binds each
    // index explicitly; cold callers retain valueOf(id), while processBlock
    // avoids searching that table for every parameter on each callback.
    enum class ParameterIndex : std::size_t
    {
        volume,
        benderDco,
        benderVcf,
        benderLfo,
        portamento,
        legacyKeyMode,
        lfoRate,
        lfoDelay,
        dcoLfo,
        pwm,
        pwmMode,
        range,
        saw,
        pulse,
        sub,
        noise,
        highPass,
        cutoff,
        resonance,
        envPolarity,
        vcfEnv,
        vcfLfo,
        keyFollow,
        vcaMode,
        vcaLevel,
        attack,
        decay,
        sustain,
        release,
        legacyChorus,
        transpose,
        masterTune,
        velocity,
        calibration,
        chorusNoise,
        polyphony,
        poly1,
        poly2,
        chorusI,
        chorusII,
        legacyHq,
        quality,
        vcfTanhMode,
        vcfFastEarlyMode,
        vcfSolverMode,
        count
    };
    static constexpr std::size_t parameterPointerCount =
        static_cast<std::size_t> (ParameterIndex::count);
    std::array<ParameterPointer, parameterPointerCount> parameterPointers {};
    float valueOf (ParameterIndex parameter) const noexcept;
    int choiceOf (ParameterIndex parameter, int maximum) const noexcept;

    youknow106::YouKnow106Engine engine;
    // The last complete APVTS/performance snapshot accepted by the audio
    // thread. Incoming patch MIDI overlays only its stored-tone fields on this
    // base, so volume, bend, assign mode and product controls remain live.
    youknow106::EngineParameters audioBaseParameters {};
    // Incoming MIDI changes which need to write parameters or select a host
    // program. Neither operation belongs in the audio callback -- it may notify
    // the host or allocate -- so the event is handed to the message thread.
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
    // A pair edit is newer than any legacy value already sitting in its
    // compatibility parameter. Publish that ordering without rewriting the
    // old id itself, so automation remains where the host left it but cannot
    // overwrite a later UI, host-pair, randomiser, patch or SysEx change.
    void publishKeyModePairAuthority() noexcept;
    void publishChorusPairAuthority() noexcept;
    std::atomic<int> lastLegacyKeyMode { 0 };
    std::atomic<int> lastLegacyChorus { 0 };

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
        int seenForwardGeneration { 0 };
    };
    LegacyBridge keyModeBridge, chorusBridge;
    static int resolveLegacyMode (int legacyValue, int fromPair,
                                  int forwardedLegacyValue,
                                  int forwardGeneration,
                                  LegacyBridge& bridge) noexcept;
    // Each compatibility id has its own publication. Sharing one generation
    // let a chorus forward accidentally release a held key-mode bridge (and
    // vice versa). The value is written after its pair, then the generation is
    // published with release ordering. A later authoritative pair write may
    // publish the legacy value too: that tells the audio bridge the value was
    // deliberately superseded rather than being an unseen automation edit.
    std::atomic<int> forwardedLegacyKeyMode { 0 };
    std::atomic<int> forwardedLegacyChorus { 0 };
    std::atomic<int> keyModeForwardGeneration { 0 };
    std::atomic<int> chorusForwardGeneration { 0 };
    // setStateInformation runs on the message thread, so it cannot touch the
    // bridges directly; it asks the audio thread to reseed them instead.
    std::atomic<bool> reseedLegacyBridges { false };
    // The restore baseline is snapshotted at replaceState time. Reading the
    // live legacy parameter later would swallow automation written between a
    // restore and the first offline-render block.
    std::atomic<int> restoredLegacyKeyMode { 0 };
    std::atomic<int> restoredLegacyChorus { 0 };
    std::atomic<int> restoredKeyModeForwardGeneration { 0 };
    std::atomic<int> restoredChorusForwardGeneration { 0 };

    // Even means stable, odd means applyPatch/replaceState is midway through a
    // multi-parameter recall. The audio thread skips an unstable snapshot and
    // keeps the previous engine settings for one block instead of rendering a
    // hybrid of two patches.
    std::atomic<unsigned> parameterWriteGeneration { 0 };
    struct ScopedParameterWrite final
    {
        explicit ScopedParameterWrite (YouKnow106AudioProcessor&) noexcept;
        ~ScopedParameterWrite();
        ScopedParameterWrite (const ScopedParameterWrite&) = delete;
        ScopedParameterWrite& operator= (const ScopedParameterWrite&) = delete;

        YouKnow106AudioProcessor& processor;
        ScopedParameterWrite* previous { nullptr };
        std::array<float, parameterPointerCount> values {};
        int program { 0 };
        bool ownsGeneration { false };
    };
    static thread_local ScopedParameterWrite* activeParameterWrite;
    const ScopedParameterWrite* activeWriteForThisThread() const noexcept;
    void serialiseWriteSnapshot (const ScopedParameterWrite&,
                                 juce::MemoryBlock& destinationData);
    static void serialiseStateSnapshot (juce::ValueTree state, int program,
                                        juce::MemoryBlock& destinationData);
    std::atomic<bool>* midiReflectionSnapshotCapturedForTest { nullptr };
    std::atomic<bool>* midiReflectionSnapshotResumeForTest { nullptr };
    void drainPendingMidiQueue();
    // Applies one tone parameter to the parameters it actually names, and to
    // no others.
    void applyToneParameter (int parameter, int value);
    void applyToneParameterValues (int parameter, int value);

    enum class PendingMidiEventKind
    {
        FullPatch,
        SingleParameter,
        ProgramChange,
        ResyncSnapshot
    };
    struct PendingMidiEvent
    {
        PendingMidiEventKind kind { PendingMidiEventKind::FullPatch };
        // Audio-thread publication order. The message thread acknowledges this
        // only after the matching APVTS/host writes are complete; until then
        // the audio-owned shadow remains authoritative over stale reflections.
        std::uint64_t sequence { 0 };
        // FullPatch: the eighteen tone bytes as they arrived, which are already
        // the message's own representation and so lose nothing in transit.
        std::array<std::uint8_t, youknow106::sysex::toneByteCount> bytes {};
        // SingleParameter: the number and value, and nothing else.
        int parameter { 0 };
        int value { 0 };
        // ProgramChange: the plug-in's host-facing index (INIT is index zero).
        // ResyncSnapshot: the newest coalesced Program Change, or -1 when the
        // overflow contained tone edits only and must leave the selector alone.
        int programIndex { -1 };
        std::uint64_t programSequence { 0 };
        // ResyncSnapshot: the complete, unquantised stored-tone state after all
        // overflowed events. Keeping Patch floats preserves untouched controls
        // across a run of single-parameter hardware edits.
        youknow106::sysex::Patch snapshot {};
        bool replacesTone { false };
        std::uint32_t touchedToneBytes { 0 };
    };

    bool stagePendingMidiEvent (const PendingMidiEvent& event) noexcept;
    void stageAndApplyPendingMidiEvent (PendingMidiEvent event) noexcept;
    void tryStagePendingMidiResync() noexcept;
    void publishPendingMidiResyncMailbox() noexcept;
    bool readPendingMidiResyncMailbox (PendingMidiEvent&) const noexcept;
    void reflectPendingMidiEvent (const PendingMidiEvent&);
    void applyPendingMidiEventToEngine (const PendingMidiEvent& event) noexcept;
    static youknow106::sysex::Patch patchFromEngineParameters (
        const youknow106::EngineParameters&) noexcept;
    static void applyPatchToEngineParameters (
        youknow106::EngineParameters&, const youknow106::sysex::Patch&) noexcept;
    static int hostProgramIndexForMidiProgram (int midiProgram) noexcept;

    // Writes the stored-tone fields without opening/closing the multi-write
    // generation. Public patch application and program recall bracket it in
    // different ways so the selected index and recalled controls can publish
    // as one coherent state-save generation.
    void applyPatchValues (const youknow106::sysex::Patch& patch);
    void applyProgramValues (
        const youknow106::sysex::Patch& patch,
        const youknow106::presets::Preset::Controls& controls);
    // A MIDI Program Change is the hardware patch-select operation. It updates
    // the selected program and tone while leaving performance/product controls
    // alone, matching the immediate audio-thread shadow used for incoming MIDI.
    void applyMidiProgramSelection (int index);

    // Translates the pre-split `keyMode` and `chorus` choices in a saved
    // session into the independent button pairs that replaced them.
    static void migrateSplitModeParameters (juce::ValueTree& state);

    // A slot is only written while it is empty and only read while it is full,
    // so the audio thread is never writing bytes the message thread is reading.
    // Sized for a whole bank arriving before the message thread next runs; at
    // MIDI's own rate that is most of a second of stall.
    static constexpr int pendingMidiQueueSlots = 64;
    struct PendingMidiSlot
    {
        PendingMidiEvent event {};
        std::atomic<bool> ready { false };
    };
    std::array<PendingMidiSlot, pendingMidiQueueSlots> pendingMidiQueue {};
    std::atomic<int> pendingMidiWriteIndex { 0 };
    int pendingMidiReadIndex { 0 };
    // Counts hard drops. Queue pressure is normally recovered by the bounded
    // coalesced snapshot below, so this remains zero even if reflection history
    // temporarily exceeds the fixed FIFO.
    std::atomic<int> pendingMidiDropped { 0 };
    // Audio-owned representation of every accepted but not-yet-reflected tone
    // event. One patch plus a sequence is bounded regardless of how long the
    // message thread is starved; the fixed event queue above remains the
    // lossless history needed to update APVTS in the same order later.
    youknow106::sysex::Patch pendingMidiToneShadow {};
    bool pendingMidiToneShadowActive { false };
    std::uint64_t pendingMidiToneShadowSequence { 0 };
    std::uint64_t nextPendingMidiSequence { 0 };
    std::atomic<std::uint64_t> reflectedMidiSequence { 0 };
    // Once the event-history FIFO fills, the audio thread continues applying
    // every incoming event to the shadow but stops publishing granular history.
    // At the next free FIFO slot it inserts one complete snapshot, then resumes
    // ordinary events behind it. These fields are audio-thread-owned.
    bool pendingMidiNeedsResync { false };
    bool pendingMidiResyncReplacesTone { false };
    std::uint32_t pendingMidiResyncTouchedToneBytes { 0 };
    // Latest Program Change seen by the audio thread, retained across epochs.
    // Every snapshot carries it; the message side compares its sequence before
    // publishing, so an already-reflected selector is not notified twice.
    int pendingMidiLatestProgramIndex { -1 };
    std::uint64_t pendingMidiLatestProgramSequence { 0 };
    // A stopped transport may never call processBlock again after the message
    // thread frees a FIFO slot. This single overwriteable atomic mailbox makes
    // the latest complete coalesced snapshot available to the timer anyway.
    // Atomic fields plus the generation seqlock avoid both allocation and any
    // reader/writer data race while the audio thread republishes a newer tone.
    std::array<std::atomic<std::uint32_t>, 16>
        pendingMidiResyncContinuous {};
    std::atomic<std::uint32_t> pendingMidiResyncSwitches { 0 };
    std::atomic<std::uint32_t> pendingMidiResyncMailboxFlags { 0 };
    std::atomic<std::uint32_t> pendingMidiResyncMailboxTouchedToneBytes { 0 };
    std::atomic<int> pendingMidiResyncMailboxProgramIndex { -1 };
    std::atomic<std::uint64_t> pendingMidiResyncMailboxProgramSequence { 0 };
    std::atomic<std::uint64_t> pendingMidiResyncMailboxSequence { 0 };
    std::atomic<std::uint64_t> pendingMidiResyncMailboxGeneration { 0 };
    std::uint64_t reflectedMidiProgramSequence { 0 }; // message-thread-owned

    std::atomic<bool> panicRequested { false };
    std::atomic<bool> keyModeReassertRequested { false };
    std::atomic<unsigned> keyModeReassertSequence { 0 };
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
    std::atomic<float> displayTemperature { 25.0f };
    std::atomic<float> displayRailDroop { 0.0f };

    static constexpr std::size_t scopeBufferSize = 256;
    static_assert (std::atomic<float>::is_always_lock_free);
    std::array<std::atomic<float>, scopeBufferSize> scopeBuffer {};
    std::atomic<std::size_t> scopeWriteIndex { 0 };

    // Set only for the duration of a processBlockBypassed call, on the audio
    // thread that made it, so the render can silence the buffer before the
    // panel telemetry is taken from it.
    bool renderingBypassed { false };

    // Retires every panel readout the audio thread publishes. Shared by the
    // host reset and the resource teardown, which both stop the instrument.
    void clearDisplayTelemetry() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouKnow106AudioProcessor)
};
