#pragma once

#include <JuceHeader.h>

#include "DSP/MarsEngine.h"
#include "DSP/MarsScope.h"

#include <array>
#include <atomic>

namespace mars::parameters
{
inline constexpr auto osc1Wave        = "osc1Wave";
inline constexpr auto osc1Model       = "osc1Model";
inline constexpr auto osc1Octave      = "osc1Octave";
inline constexpr auto osc2Wave        = "osc2Wave";
inline constexpr auto osc2Model       = "osc2Model";
inline constexpr auto osc2Octave      = "osc2Octave";
inline constexpr auto osc2Tune        = "osc2Tune";
inline constexpr auto osc2Fine        = "osc2Fine";
inline constexpr auto oscMix          = "oscMix";
inline constexpr auto pulseWidth      = "pulseWidth";
inline constexpr auto subLevel        = "subLevel";
inline constexpr auto noiseLevel      = "noiseLevel";
inline constexpr auto crossMod        = "crossMod";
inline constexpr auto filterModel     = "filterModel";
inline constexpr auto cutoff          = "cutoff";
inline constexpr auto resonance       = "resonance";
inline constexpr auto filterDrive     = "filterDrive";
inline constexpr auto filterShape     = "filterShape";
inline constexpr auto filterEnvAmount = "filterEnvAmount";
inline constexpr auto keyTrack        = "keyTrack";
inline constexpr auto fAttack         = "fAttack";
inline constexpr auto fDecay          = "fDecay";
inline constexpr auto fSustain        = "fSustain";
inline constexpr auto fRelease        = "fRelease";
inline constexpr auto aAttack         = "aAttack";
inline constexpr auto aDecay          = "aDecay";
inline constexpr auto aSustain        = "aSustain";
inline constexpr auto aRelease        = "aRelease";
inline constexpr auto lfoWave         = "lfoWave";
inline constexpr auto lfoRate         = "lfoRate";
inline constexpr auto lfoPitch        = "lfoPitch";
inline constexpr auto lfoFilter       = "lfoFilter";
inline constexpr auto lfoPwm          = "lfoPwm";
inline constexpr auto voiceMode       = "voiceMode";
inline constexpr auto monoMode        = "monoMode";
inline constexpr auto polyphonyLimit  = "polyphonyLimit";
inline constexpr auto unisonVoices    = "unisonVoices";
inline constexpr auto drift           = "drift";
inline constexpr auto spread          = "spread";
inline constexpr auto glide           = "glide";
inline constexpr auto velocity        = "velocity";
inline constexpr auto chorusMix       = "chorusMix";
inline constexpr auto chorusRate      = "chorusRate";
inline constexpr auto chorusCompander = "chorusCompander";
inline constexpr auto output          = "output";
inline constexpr auto osc1Enabled     = "osc1Enabled";
inline constexpr auto osc2Enabled     = "osc2Enabled";
inline constexpr auto hqOversampling  = "hqOversampling";
inline constexpr auto unisonDetune    = "unisonDetune";
inline constexpr auto arpEnabled      = "arpEnabled";
inline constexpr auto arpMode         = "arpMode";
inline constexpr auto arpRate         = "arpRate";
inline constexpr auto arpOctaves      = "arpOctaves";
inline constexpr auto arpGate         = "arpGate";
inline constexpr auto arpHold         = "arpHold";
} // namespace mars::parameters

class MarsAudioProcessorEditor;

class MarsAudioProcessor final : public juce::AudioProcessor,
                                 private juce::MidiKeyboardState::Listener
{
public:
    MarsAudioProcessor();
    ~MarsAudioProcessor() override;

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
    // The 12 s maximum amp release reaches -60 dB at its nominal time. Voice-
    // card envelope tolerance can stretch that to 13.68 s, and the engine does
    // not retire the voice until -100 dB (about 22.8 s). The remaining margin
    // covers the feed-forward ensemble delay and output servo settling.
    static constexpr double maximumTailLengthSeconds = 24.0;
    double getTailLengthSeconds() const override { return maximumTailLengthSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Call from the message thread (for example, from a front-panel button).
    // The amount is expressed as a proportion of every control's normalised
    // legal range: 0.01, 0.10, and 1.0 are the intended panel strengths.
    void randomizeParameters (float amount);

    void requestPanic() noexcept { panicRequested.store (true, std::memory_order_release); }
    int getActiveVoiceCount() const noexcept { return activeVoiceCount.load (std::memory_order_relaxed); }
    double getCurrentSampleRateForDisplay() const noexcept
    {
        return displaySampleRate.load (std::memory_order_relaxed);
    }
    int getOversamplingFactorForDisplay() const noexcept
    {
        return displayOversamplingFactor.load (std::memory_order_relaxed);
    }
    int getEffectiveOversamplingFactor() const noexcept
    {
        return getOversamplingFactorForDisplay();
    }
    bool isEngineReady() const noexcept { return engineReady.load (std::memory_order_acquire); }

    // Front-panel scope support. The audio thread writes a decimated mono trace
    // into a fixed lock-free ring; the editor copies the most recent window on
    // its repaint timer. A torn read is invisible on a 12 Hz display, so no
    // lock or allocation is needed on either side.
    static constexpr int scopeRingSize = 4096;
    int copyScopeTrace (float* destination, int maximumSamples) const noexcept;
    int getArpeggiatorNoteForDisplay() const noexcept
    {
        return displayArpNote.load (std::memory_order_relaxed);
    }
    float getArpeggiatorPhaseForDisplay() const noexcept
    {
        return displayArpPhase.load (std::memory_order_relaxed);
    }
    int getArpeggiatorKeyCountForDisplay() const noexcept
    {
        return displayArpKeys.load (std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct ParameterPointers
    {
        std::atomic<float>* osc1Wave = nullptr;
        std::atomic<float>* osc1Model = nullptr;
        std::atomic<float>* osc1Octave = nullptr;
        std::atomic<float>* osc2Wave = nullptr;
        std::atomic<float>* osc2Model = nullptr;
        std::atomic<float>* osc2Octave = nullptr;
        std::atomic<float>* osc2Tune = nullptr;
        std::atomic<float>* osc2Fine = nullptr;
        std::atomic<float>* oscMix = nullptr;
        std::atomic<float>* pulseWidth = nullptr;
        std::atomic<float>* subLevel = nullptr;
        std::atomic<float>* noiseLevel = nullptr;
        std::atomic<float>* crossMod = nullptr;
        std::atomic<float>* filterModel = nullptr;
        std::atomic<float>* cutoff = nullptr;
        std::atomic<float>* resonance = nullptr;
        std::atomic<float>* filterDrive = nullptr;
        std::atomic<float>* filterShape = nullptr;
        std::atomic<float>* filterEnvAmount = nullptr;
        std::atomic<float>* keyTrack = nullptr;
        std::atomic<float>* fAttack = nullptr;
        std::atomic<float>* fDecay = nullptr;
        std::atomic<float>* fSustain = nullptr;
        std::atomic<float>* fRelease = nullptr;
        std::atomic<float>* aAttack = nullptr;
        std::atomic<float>* aDecay = nullptr;
        std::atomic<float>* aSustain = nullptr;
        std::atomic<float>* aRelease = nullptr;
        std::atomic<float>* lfoWave = nullptr;
        std::atomic<float>* lfoRate = nullptr;
        std::atomic<float>* lfoPitch = nullptr;
        std::atomic<float>* lfoFilter = nullptr;
        std::atomic<float>* lfoPwm = nullptr;
        std::atomic<float>* voiceMode = nullptr;
        std::atomic<float>* monoMode = nullptr;
        std::atomic<float>* polyphonyLimit = nullptr;
        std::atomic<float>* unisonVoices = nullptr;
        std::atomic<float>* drift = nullptr;
        std::atomic<float>* spread = nullptr;
        std::atomic<float>* glide = nullptr;
        std::atomic<float>* velocity = nullptr;
        std::atomic<float>* chorusMix = nullptr;
        std::atomic<float>* chorusRate = nullptr;
        std::atomic<float>* chorusCompander = nullptr;
        std::atomic<float>* output = nullptr;
        std::atomic<float>* osc1Enabled = nullptr;
        std::atomic<float>* osc2Enabled = nullptr;
        std::atomic<float>* hqOversampling = nullptr;
        std::atomic<float>* unisonDetune = nullptr;
        std::atomic<float>* arpEnabled = nullptr;
        std::atomic<float>* arpMode = nullptr;
        std::atomic<float>* arpRate = nullptr;
        std::atomic<float>* arpOctaves = nullptr;
        std::atomic<float>* arpGate = nullptr;
        std::atomic<float>* arpHold = nullptr;
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
    void pushScopeSamples (const juce::AudioBuffer<float>& buffer) noexcept;
    void clearScopeRing() noexcept;

    mars::MarsEngine engine;
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineReady { false };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<double> displaySampleRate { 0.0 };
    std::atomic<int> displayOversamplingFactor { 1 };
    std::atomic<int> displayArpNote { -1 };
    std::atomic<float> displayArpPhase { 0.0f };
    std::atomic<int> displayArpKeys { 0 };

    // The audio thread fills this ring while the editor's timer copies it, so
    // the samples themselves are atomic. Relaxed ordering keeps the cost of a
    // plain store on every supported target while removing the data race; the
    // trace is a display, so a wrapped read overtaking the writer is fine.
    std::array<std::atomic<float>, scopeRingSize> scopeRing {};
    std::atomic<int> scopeWriteIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarsAudioProcessor)
};
