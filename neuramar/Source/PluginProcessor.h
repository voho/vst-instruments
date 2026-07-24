#pragma once

#include <JuceHeader.h>

#include "DSP/ModelVisualisation.h"
#include "DSP/NeuramarEngine.h"
#include "DSP/NeuralModel.h"
#include "DSP/SampleLearner.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace neuramar::parameters
{
inline constexpr auto imprint       = "imprint";
inline constexpr auto bodyLock      = "bodyLock";
inline constexpr auto air           = "air";
inline constexpr auto bone          = "bone";
inline constexpr auto brightness    = "brightness";
inline constexpr auto evolutionRate = "evolutionRate";
inline constexpr auto orbit         = "orbit";
inline constexpr auto mutation      = "mutation";
inline constexpr auto attack        = "attack";
inline constexpr auto release       = "release";
inline constexpr auto spread        = "spread";
inline constexpr auto rootCorrection = "rootCorrection";
inline constexpr auto output        = "output";
inline constexpr auto noise         = "noise";
inline constexpr auto stretch       = "stretch";
inline constexpr auto formant       = "formant";
inline constexpr auto touch         = "touch";
} // namespace neuramar::parameters

class NeuramarAudioProcessorEditor;

class NeuramarAudioProcessor final : public juce::AudioProcessor,
                                      private juce::MidiKeyboardState::Listener
{
public:
    enum class LearningStage
    {
        Empty,
        Reading,
        FindingRoot,
        Analysing,
        Training,
        Ready,
        Cancelled,
        Error
    };

    enum class RandomizationAmount : unsigned
    {
        Subtle1Percent = 1,
        Evolve10Percent = 10,
        Wild100Percent = 100
    };

    struct LearningSnapshot
    {
        LearningStage stage = LearningStage::Empty;
        float progress = 0.0f;
        double rootFrequencyHz = 0.0;
        int rootMidiNote = -1;
        float rootCents = 0.0f;
        float rootConfidence = 0.0f;
        bool generatedModel = false;
        std::uint64_t modelGeneration = 0;
        juce::String sourceName;
        juce::String message;
        std::array<float, 256> waveform {};
    };

    // A display-only reduction of the published memory, rebuilt once per
    // publication on the thread that publishes it. The editor fetches it only
    // when the generation changes, so no per-frame model access is required.
    struct ModelAnatomySnapshot
    {
        std::uint64_t modelGeneration = 0;
        neuramar::ModelAnatomy anatomy;
    };

    NeuramarAudioProcessor();
    ~NeuramarAudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override
    {
        return index == 0 ? juce::String { "Neural Memory" } : juce::String {};
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadSampleFile (const juce::File& file);
    void learnSampleData (std::vector<float> monoSamples, double sourceSampleRate,
                          juce::String sourceName = "Generated sound");
    void requestLearningCancellation();
    void cancelLearning();
    // Message-thread action: generates a playable neural seed when empty, or
    // derives an immutable variation from the currently published memory.
    void randomizeModel (RandomizationAmount amount);
    void requestPanic() noexcept;

    [[nodiscard]] LearningSnapshot getLearningSnapshot() const;
    [[nodiscard]] ModelAnatomySnapshot getModelAnatomySnapshot() const;
    [[nodiscard]] int getActiveVoiceCount() const noexcept
    {
        return activeVoiceCount.load (std::memory_order_relaxed);
    }
    [[nodiscard]] double getCurrentSampleRateForDisplay() const noexcept
    {
        return displaySampleRate.load (std::memory_order_relaxed);
    }
    [[nodiscard]] bool isEngineReady() const noexcept
    {
        return engineReady.load (std::memory_order_acquire);
    }

    static bool isSupportedSampleFile (const juce::File& file);
    static juce::String noteName (int midiNote);
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

private:
    struct ParameterPointers
    {
        std::atomic<float>* imprint = nullptr;
        std::atomic<float>* bodyLock = nullptr;
        std::atomic<float>* air = nullptr;
        std::atomic<float>* bone = nullptr;
        std::atomic<float>* brightness = nullptr;
        std::atomic<float>* evolutionRate = nullptr;
        std::atomic<float>* orbit = nullptr;
        std::atomic<float>* mutation = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* spread = nullptr;
        std::atomic<float>* rootCorrection = nullptr;
        std::atomic<float>* output = nullptr;
        std::atomic<float>* noise = nullptr;
        std::atomic<float>* stretch = nullptr;
        std::atomic<float>* formant = nullptr;
        std::atomic<float>* touch = nullptr;
    } parameterPointers;

    struct UiMidiEvent
    {
        int note = 60;
        float velocity = 0.0f;
        bool noteOn = false;
    };

    static constexpr unsigned uiQueueCapacity = 128;
    static constexpr std::uint32_t stateMagic = 0x5453524eu; // "NRST" little-endian
    static constexpr std::uint32_t stateVersion = 1;
    static constexpr int maximumModelStateBytes = 4 * 1024 * 1024;

    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel,
                       int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel,
                        int midiNoteNumber, float velocity) override;
    void enqueueUiMidiEvent (int note, float velocity, bool isNoteOn) noexcept;
    void discardUiMidiEvents() noexcept;
    void dispatchUiMidiEvents() noexcept;
    void dispatchMidiData (const juce::uint8* data, int numBytes) noexcept;
    void updateEngineParameters() noexcept;
    void installPublishedModelOnAudioThread() noexcept;

    void startLearningWorker (std::vector<float> monoSamples,
                              double sourceSampleRate,
                              juce::String sourceName);
    void runLearning (std::stop_token stopToken,
                      std::vector<float> monoSamples,
                      double sourceSampleRate,
                      juce::String sourceName,
                      std::uint64_t attemptState);
    void publishModel (std::unique_ptr<neuramar::NeuralModel> model,
                       juce::String sourceName,
                       juce::String readyMessage = {});
    void publishRestoredModel (std::unique_ptr<neuramar::NeuralModel> model,
                               juce::String sourceName);
    void clearPublishedModel (juce::String message);
    void setLearningStatus (LearningStage stage, float progress,
                            juce::String message = {},
                            std::uint64_t attemptState = 0);
    void setTerminalLearningStatus (LearningStage stage, juce::String message);
    void makeWaveformPreview (const std::vector<float>& monoSamples,
                              std::uint64_t attemptState = 0);
    void reclaimRetiredModelsLocked();
    [[nodiscard]] std::uint64_t nextRandomizationSeed() noexcept;

    std::array<UiMidiEvent, uiQueueCapacity> uiMidiQueue {};
    std::atomic<unsigned> uiWriteIndex { 0 };
    std::atomic<unsigned> uiReadIndex { 0 };

    neuramar::NeuramarEngine engine;
    std::atomic<const neuramar::NeuralModel*> publishedModel { nullptr };
    std::atomic<const neuramar::NeuralModel*> audioModelHazard { nullptr };
    std::atomic<std::uint64_t> publishedGeneration { 0 };
    std::uint64_t audioModelGeneration = 0;

    struct ArchivedModel
    {
        std::uint64_t generation = 0;
        std::unique_ptr<neuramar::NeuralModel> model;
    };

    // A single hazard pointer protects the immutable model that the callback
    // may still touch. Publication can reclaim every other retired model off
    // the audio thread, keeping pointer swaps lock-free and memory bounded.
    std::mutex modelArchiveMutex;
    std::vector<ArchivedModel> modelArchive;
    juce::String publishedSourceName;
    std::jthread learningThread;
    std::mutex learningThreadMutex;
    // Serialises public load/cancel/state operations. It is recursive because
    // replacement and state restore use the same synchronous cancellation
    // primitive internally; no audio-thread path takes this mutex.
    std::recursive_mutex learningOperationMutex;
    std::atomic<std::uint64_t> learningAttemptCounter { 0 };
    // Even nonzero values are publishable attempts; the worker or canceller
    // atomically closes an attempt by setting its low bit.
    std::atomic<std::uint64_t> learningAttemptState { 0 };
    std::uint64_t randomizationSeedBase = 0;
    std::atomic<std::uint64_t> randomizationCounter { 0 };

    mutable juce::CriticalSection displayLock;
    LearningSnapshot displayState;
    ModelAnatomySnapshot displayAnatomy;

    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineReady { false };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<double> displaySampleRate { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuramarAudioProcessor)
};
