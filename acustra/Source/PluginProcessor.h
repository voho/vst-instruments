#pragma once

#include <JuceHeader.h>

#include "DSP/AcustraEngine.h"

#include <array>
#include <atomic>

namespace acustra::parameters
{
inline constexpr auto shape = "shape";
inline constexpr auto bodyMaterial = "bodyMaterial";
inline constexpr auto stringMaterial = "stringMaterial";
inline constexpr auto tuning = "tuning";
inline constexpr auto stringAge = "stringAge";
inline constexpr auto pluckPosition = "pluckPosition";
inline constexpr auto touch = "touch";
inline constexpr auto bodyAmount = "bodyAmount";
inline constexpr auto stereoWidth = "stereoWidth";
inline constexpr auto output = "output";
inline constexpr auto capture = "capture";
inline constexpr auto picking = "picking";
inline constexpr auto bridgeModel = "bridgeModel";
inline constexpr auto upperMic = "upperMic";

inline constexpr int parameterCount = 14;
} // namespace acustra::parameters

class AcustraAudioProcessorEditor;

class AcustraAudioProcessor final : public juce::AudioProcessor
{
public:
    AcustraAudioProcessor();
    ~AcustraAudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return maximumTailLengthSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destinationData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void requestPanic() noexcept;

    [[nodiscard]] acustra::EngineParameters snapshotEngineParameters() const noexcept;
    [[nodiscard]] int getActiveVoiceCount() const noexcept
    {
        return activeVoiceCount.load (std::memory_order_relaxed);
    }
    [[nodiscard]] int getSympatheticStringCount() const noexcept
    {
        return sympatheticStringCount.load (std::memory_order_relaxed);
    }
    [[nodiscard]] double getCurrentSampleRateForDisplay() const noexcept
    {
        return displaySampleRate.load (std::memory_order_relaxed);
    }
    [[nodiscard]] bool isEngineReady() const noexcept
    {
        return engineReady.load (std::memory_order_acquire);
    }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    static constexpr double maximumTailLengthSeconds = 30.0;

    void dispatchMidiData (const juce::uint8* data, int numBytes) noexcept;
    bool processRpnController (int midiChannel, int controller,
                               int value) noexcept;
    void setLowerZoneMemberCount (int memberCount) noexcept;
    void refreshPitchBend (int midiChannel) noexcept;
    void resetControllerScope (int midiChannel) noexcept;
    [[nodiscard]] bool channelIsInControllerScope (
        int controllerChannel, int targetChannel) const noexcept;
    void updateEngineParameters() noexcept;

    std::array<std::atomic<float>*, acustra::parameters::parameterCount>
        parameterPointers {};
    acustra::AcustraEngine engine;
    bool legatoDown { false };
    // A strum's stroke alternates; the sample clock tells a rest from a beat.
    bool strumUpstroke { false };
    double currentSampleRate { 48000.0 };
    std::int64_t processedSamples { 0 };
    std::int64_t lastStrumSample { -1 };
    std::array<juce::MidiRPNDetector, 16> rpnDetectors {};
    std::array<float, 16> rawPitchWheels {};
    std::array<float, 16> conventionalPitchBendRanges {};
    float lowerMasterPitchBendRange { 2.0f };
    float lowerMemberPitchBendRange { 48.0f };
    int lowerZoneMemberCount { 0 };
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineReady { false };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> sympatheticStringCount { 0 };
    std::atomic<double> displaySampleRate { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcustraAudioProcessor)
};
