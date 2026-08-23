#pragma once

#include <JuceHeader.h>

#include "DSP/GhostarEngine.h"
#include "DSP/GhostarPresets.h"

#include <array>
#include <atomic>

namespace ghostar::parameters
{
// One host parameter per modelled panel control, in panel order. The IDs are
// the automation contract: never renumber or reuse them.

// MASTER
inline constexpr auto tune = "tune";
inline constexpr auto octave = "octave";
// OSCILLATOR A
inline constexpr auto oscAWaveform = "oscAWaveform";
inline constexpr auto sync = "sync";
// OSCILLATOR B
inline constexpr auto oscBWaveform = "oscBWaveform";
inline constexpr auto oscBRange = "oscBRange";
inline constexpr auto interval = "interval";
// TRIGGER / GATE SELECT
inline constexpr auto trigger = "trigger";
inline constexpr auto gateKbd = "gateKbd";
inline constexpr auto gateX = "gateX";
inline constexpr auto gateYExt = "gateYExt";
// MOD X
inline constexpr auto arpeggiator = "arpeggiator";
inline constexpr auto modSource = "modSource";
inline constexpr auto lfoRate = "lfoRate";
// SHAPER Y
inline constexpr auto shaperMode = "shaperMode";
inline constexpr auto shaperShape = "shaperShape";
inline constexpr auto shaperRate = "shaperRate";
// WHEEL DESTINATIONS
inline constexpr auto modXTo = "modXTo";
inline constexpr auto shapeXWithY = "shapeXWithY";
inline constexpr auto shaperYTo = "shaperYTo";
// AUDIO MIXER
inline constexpr auto masterVolume = "masterVolume";
inline constexpr auto brightness = "brightness";
inline constexpr auto shaperPathA = "shaperPathA";
inline constexpr auto shaperPathB = "shaperPathB";
inline constexpr auto shaperPathRing = "shaperPathRing";
inline constexpr auto shaperPathNoise = "shaperPathNoise";
inline constexpr auto filterPathA = "filterPathA";
inline constexpr auto filterPathB = "filterPathB";
inline constexpr auto filterPathNoise = "filterPathNoise";
// UPPER FILTER U / LOWER FILTER L
inline constexpr auto cutoff = "cutoff";
inline constexpr auto lowerOnly = "lowerOnly";
inline constexpr auto upperResonance = "upperResonance";
inline constexpr auto resonance = "resonance";
inline constexpr auto slope = "slope";
inline constexpr auto kbAmount = "kbAmount";
inline constexpr auto lowerMode = "lowerMode";
inline constexpr auto tracking = "tracking";
// FILTER ENVELOPE
inline constexpr auto filterEnvAmount = "filterEnvAmount";
inline constexpr auto filterAttack = "filterAttack";
inline constexpr auto filterDecay = "filterDecay";
inline constexpr auto filterSustain = "filterSustain";
inline constexpr auto filterRelease = "filterRelease";
// LOUDNESS ENVELOPE
inline constexpr auto vcaBypass = "vcaBypass";
inline constexpr auto loudnessAttack = "loudnessAttack";
inline constexpr auto loudnessDecay = "loudnessDecay";
inline constexpr auto loudnessSustain = "loudnessSustain";
inline constexpr auto loudnessRelease = "loudnessRelease";
// Performance
inline constexpr auto glide = "glide";
inline constexpr auto glideMode = "glideMode";
// Performance wheels, published so hosts can automate what the hardware
// player rides by hand.
inline constexpr auto xWheel = "xWheel";
inline constexpr auto yWheel = "yWheel";
// Product policy: the hardware's two rear jacks, as a stereo split.
inline constexpr auto splitPaths = "splitPaths";
} // namespace ghostar::parameters

class GhostarAudioProcessor final : public juce::AudioProcessor,
                                  private juce::MidiKeyboardState::Listener
{
public:
    GhostarAudioProcessor();
    ~GhostarAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // The longest the instrument can still be ringing after the last note
    // is released. Taken from the engine's own envelope law rather than
    // written down here, so re-voicing the segment timing cannot leave a
    // host truncating a release this figure no longer covers. (A drone —
    // VCA BYPASS, or a raised Shaper-path slider — is not a tail and no
    // finite figure covers it; the panel stops that, not the host.)
    double getTailLengthSeconds() const override
    {
        return ghostar::GhostarEngine::longestReleaseTailSeconds();
    }

    // Factory programs: the modelled instrument's manual teaches eleven
    // Sound Charts instead of shipping presets, and those charts are the
    // bank, behind an Init program that is the default voice a fresh
    // instance already carries. The table lives in the JUCE-free core so
    // the DSP suite renders every one; this layer only writes those engine
    // parameters into the host parameters that publish them.
    int getNumPrograms() override { return ghostar::factoryPresetCount(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override
    {
        if (const auto* name = ghostar::factoryPresetName(index))
            return name;
        return {};
    }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void requestPanic() noexcept
    {
        // Snapshot the UI queue at the click: the panic discards what
        // preceded it, while a key pressed after the click still sounds.
        // The release store on the flag publishes the snapshot with it.
        panicDropBefore.store(uiWriteIndex.load(std::memory_order_acquire),
                              std::memory_order_relaxed);
        panicRequested.store(true, std::memory_order_release);
    }
    // The editor's spring-loaded bend wheel; applied at the next block.
    void setUiPitchBend(float normalisedBipolar) noexcept
    {
        uiPitchBend.store(juce::jlimit(-1.0f, 1.0f, normalisedBipolar),
                          std::memory_order_relaxed);
    }
    [[nodiscard]] bool isGateOpenForDisplay() const noexcept
    {
        return gateOpenForDisplay.load(std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

    static juce::AudioProcessorValueTreeState::ParameterLayout
    createParameterLayout();

private:
    friend struct GhostarAudioProcessorTestAccess;

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

    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel,
                      int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel,
                       int midiNoteNumber, float velocity) override;
    void enqueueUiMidiEvent(int note, float velocity, bool isNoteOn) noexcept;
    void dispatchUiMidiEvents() noexcept;
    void handleMidiMessage(const juce::MidiMessage& message) noexcept;
    void updateEngineParameters() noexcept;

    ghostar::GhostarEngine engine;
    std::atomic<bool> panicRequested { false };
    // Where the UI queue stood when panic was last requested; events
    // enqueued before this position are dropped by the panic handling.
    std::atomic<unsigned> panicDropBefore { 0 };
    std::atomic<bool> gateOpenForDisplay { false };
    std::atomic<float> uiPitchBend { 0.0f };
    float lastAppliedUiBend { 0.0f };  // audio thread only
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GhostarAudioProcessor)
};
