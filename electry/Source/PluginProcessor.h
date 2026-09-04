#pragma once

#include <JuceHeader.h>

#include "DSP/ElectryEngine.h"
#include "DSP/ElectryFx.h"
#include "DSP/ElectryVisuals.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace electry::parameters
{
inline constexpr auto pickupSelector = "pickupSelector";
inline constexpr auto pickupType     = "pickupType";
inline constexpr auto tone           = "tone";
inline constexpr auto guitarBuild    = "guitarBuild";
inline constexpr auto bodyResonance  = "bodyResonance";
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
inline constexpr auto sympathetic    = "sympathetic";
inline constexpr auto palmMute       = "palmMute";
inline constexpr auto strumSpread    = "strumSpread";
inline constexpr auto tremoloRate    = "tremoloRate";
inline constexpr auto resonanceDepth = "resonanceDepth";
inline constexpr auto ampModel       = "ampModel";
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
    bool supportsMPE() const override { return true; }
    // The longest open-string decay reaches -60 dB in about 9 s; the engine
    // retires a voice near -100 dB, and the release stage adds a bounded
    // damped tail. 18 s covers the complete audible ring-out.
    static constexpr double maximumTailLengthSeconds = 18.0;
    double getTailLengthSeconds() const override { return maximumTailLengthSeconds; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Message-thread entry points used by the editor. Both route through the
    // same bounded lock-free queue as on-screen keyboard notes.
    void triggerArticulation (int articulationIndex);
    void triggerStringRepick (int stringIndex, float velocity = 1.0f);
    void setPlayStyleKeysHold (bool shouldHold);
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
    int getEffectivePlayStyleIndex() const noexcept
    {
        return effectivePlayStyleIndex.load (std::memory_order_relaxed);
    }
    bool getPlayStyleKeysHold() const noexcept
    {
        return playStyleKeysHold.load (std::memory_order_relaxed);
    }
    std::uint8_t getSoloStringMask() const noexcept
    {
        return soloStringMaskForDisplay.load (std::memory_order_relaxed);
    }
    int getMidiMutePressureForDisplay() const noexcept
    {
        return midiMutePressureForDisplay.load (std::memory_order_relaxed);
    }
    int getVibratoGestureForDisplay() const noexcept
    {
        return vibratoGestureForDisplay.load (std::memory_order_relaxed);
    }
    int getTremoloGestureForDisplay() const noexcept
    {
        return tremoloGestureForDisplay.load (std::memory_order_relaxed);
    }
    double getCurrentSampleRateForDisplay() const noexcept
    {
        return displaySampleRate.load (std::memory_order_relaxed);
    }
    bool isEngineReady() const noexcept
    {
        return engineReady.load (std::memory_order_acquire);
    }

    // The pitch wheel's raw MIDI reconstruction: two 7-bit data bytes packed
    // into a 14-bit position, then normalised to +/-1 with the MIDI spec's
    // asymmetric range (the excursion below centre is scaled by 8192, the
    // excursion above it by 8191). Exposed statically, rather than kept
    // buried in dispatchMidiData(), so this byte-level arithmetic can be
    // asserted on exactly instead of only inferred from rendered audio.
    static float decodePitchBend14 (juce::uint8 data1, juce::uint8 data2) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct ParameterPointers
    {
        std::atomic<float>* pickupSelector = nullptr;
        std::atomic<float>* pickupType = nullptr;
        std::atomic<float>* tone = nullptr;
        std::atomic<float>* guitarBuild = nullptr;
        std::atomic<float>* bodyResonance = nullptr;
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
        std::atomic<float>* tremoloRate = nullptr;
        std::atomic<float>* resonanceDepth = nullptr;
        std::atomic<float>* ampModel = nullptr;
    } parameterPointers;

    struct UiMidiEvent
    {
        int note = 60;
        float velocity = 0.0f;
        bool noteOn = false;
        bool selectsBaseArticulation = false;
    };

    struct NoteOnBatch
    {
        std::array<electry::ElectryEngine::NoteOnEvent,
                   electry::ElectryEngine::maximumChordEvents> events {};
        std::size_t size = 0;
    };

    struct OutstandingNoteOwner
    {
        std::uint8_t midiChannel = 0;
        electry::ElectryEngine::ExpressionId expressionId =
            electry::ElectryEngine::legacyExpressionId;
    };

    static constexpr std::size_t maximumOutstandingOwnersPerNote = 128;
    struct OutstandingNoteQueue
    {
        std::array<OutstandingNoteOwner, maximumOutstandingOwnersPerNote>
            owners {};
        std::uint8_t size = 0;
    };

    static constexpr unsigned uiQueueCapacity = 128;
    std::array<UiMidiEvent, uiQueueCapacity> uiMidiQueue {};
    std::atomic<unsigned> uiWriteIndex { 0 };
    std::atomic<unsigned> uiReadIndex { 0 };

    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel,
                       int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel,
                        int midiNoteNumber, float velocity) override;
    void latchArticulation (int articulationIndex) noexcept;
    void enqueueUiMidiEvent (int note, float velocity, bool isNoteOn,
                             bool selectsBaseArticulation = false) noexcept;
    void discardUiMidiEvents() noexcept;
    void dispatchUiMidiEventPass (unsigned begin, unsigned end,
                                  bool conditioning,
                                  NoteOnBatch& batch) noexcept;
    void batchOrDispatchNoteOn (int note, float velocity,
                                electry::ElectryEngine::ExpressionId expressionId,
                                int midiChannel,
                                bool selectsBaseArticulation,
                                NoteOnBatch& batch) noexcept;
    void flushNoteOnBatch (NoteOnBatch& batch) noexcept;
    void applyVibratoGesture (float amount) noexcept;
    void clearVibratoGesture() noexcept;
    void applyTremoloGesture (float velocity) noexcept;
    void clearTremoloGesture() noexcept;
    void dispatchNoteOn (int note, float velocity,
                         bool selectsBaseArticulation) noexcept;
    void dispatchNoteOff (int note) noexcept;
    electry::ElectryEngine::ExpressionId expressionIdForNoteOn (
        int midiChannel) const noexcept;
    void dispatchHostNoteOff (int note, int midiChannel) noexcept;
    bool processMpeController (const juce::uint8* data, int numBytes) noexcept;
    void dispatchPitchWheel (int midiChannel, float bend) noexcept;
    void refreshMpePitchBend (int midiChannel) noexcept;
    void refreshMpePitchBends() noexcept;
    bool recordNoteOwnership (
        int note, int midiChannel,
        electry::ElectryEngine::ExpressionId expressionId) noexcept;
    int takeNoteOwnership (int note, int midiChannel,
                           bool allowAnyLegacy) noexcept;
    void clearMpeNoteOwnership() noexcept;
    void resetPitchBends() noexcept;
    void applyPlayStyle (int styleIndex) noexcept;
    int latestHeldPlayStyle() const noexcept;
    void clearHeldPlayStyles() noexcept;
    void applySoloStringMask (std::uint8_t mask) noexcept;
    std::uint8_t computeHeldSoloStringMask() const noexcept;
    void clearHeldSoloStrings() noexcept;
    void synchronisePlayStyleKeyMode() noexcept;
    void dispatchMidiData (const juce::uint8* data, int numBytes) noexcept;
    void resetEngineWithArticulations (int pickStyle, int playStyle) noexcept;
    void renderEngines (float* left, float* right, int numSamples) noexcept;
    void updateEngineParameters() noexcept;
    void updateEffectParameters() noexcept;
    void publishStringVisualState() noexcept;

    electry::ElectryEngine engine;
    // DOUBLE is two performances, not a delayed or widened copy. The second
    // engine receives the same score with its own deterministic player draws.
    std::unique_ptr<electry::ElectryEngine> doubleEngine {
        std::make_unique<electry::ElectryEngine>()
    };
    // The amplifier, cabinet and time effects live in the JUCE-free DSP
    // library alongside the string model, so the complete signal path is
    // regression tested on every platform rather than only inside a host.
    electry::ElectryFx effects;
    std::array<electry::StringVisualState,
               electry::ElectryEngine::stringCount> visualScratch {};
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineReady { false };
    bool doubleModeActive = false;
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> sympatheticStringCount { 0 };
    std::atomic<int> currentProgram { 0 };
    // Requested performance latches outlive the replaceable DSP engine state.
    std::atomic<int> pickStyleIndex { 0 };
    // The strip and session state retain this base style. A HOLD keyswitch may
    // temporarily change the effective style without overwriting it.
    std::atomic<int> playStyleIndex { 0 };
    std::atomic<int> effectivePlayStyleIndex { 0 };
    std::atomic<bool> playStyleKeysHold { false };
    bool appliedPlayStyleKeysHold = false;
    int appliedBasePlayStyleIndex = 0;
    std::array<std::uint16_t,
               electry::ElectryEngine::playStyleKeyswitchCount>
        heldPlayStyleCounts {};
    std::array<std::uint64_t,
               electry::ElectryEngine::playStyleKeyswitchCount>
        heldPlayStyleOrder {};
    std::uint64_t heldPlayStyleSequence = 0;
    std::atomic<std::uint8_t> soloStringMaskForDisplay { 0 };
    std::atomic<std::uint8_t> latchedSoloStringMask { 0 };
    std::array<std::uint16_t,
               electry::ElectryEngine::stringCount>
        heldSoloStringCounts {};
    std::uint16_t vibratoGestureOwners = 0;
    std::uint16_t tremoloGestureOwners = 0;
    bool sustainPedalDown = false;
    juce::MPEZoneLayout mpeZoneLayout;
    juce::MidiRPNDetector mpeRpnDetector;
    // JUCE keeps these four routing ranges as integers. RPN 0's Data Entry
    // LSB is cents, so retain the performed range separately for tuning while
    // leaving JUCE's layout as the authority for zone/channel membership.
    float lowerPerNotePitchBendRange { 48.0f };
    float lowerMasterPitchBendRange { 2.0f };
    float upperPerNotePitchBendRange { 48.0f };
    float upperMasterPitchBendRange { 2.0f };
    std::array<float, 16> midiPitchBends {};
    std::array<OutstandingNoteQueue, 128> outstandingNoteOwners {};
    std::array<std::uint16_t, 16> outstandingMpeChannelNotes {};
    // Raw CC2 is kept beside the other display-only atomics. The Mute Pressure
    // knob shows the host parameter; this value makes its live MIDI addition
    // visible without feeding UI state back into the engine.
    std::atomic<int> midiMutePressureForDisplay { 0 };
    std::atomic<int> vibratoGestureForDisplay { 0 };
    std::atomic<int> tremoloGestureForDisplay { 0 };
    std::atomic<double> displaySampleRate { 0.0 };
    std::array<std::atomic<std::uint32_t>,
               electry::ElectryEngine::stringCount> stringVisuals {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElectryAudioProcessor)
};
