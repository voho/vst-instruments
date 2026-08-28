#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void expect (bool condition, const std::string& message)
{
    if (condition)
        return;

    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

std::unique_ptr<juce::AudioPluginInstance> instantiate (
    const juce::AudioPluginFormatManager& manager,
    const juce::PluginDescription& description)
{
    juce::String error;
    auto result = manager.createPluginInstance (description, 48000.0, 256, error);
    expect (result != nullptr,
            "could not instantiate the VST3: " + error.toStdString());
    return result;
}

void flushParameters (juce::AudioPluginInstance& plugin)
{
    juce::AudioBuffer<float> audio (std::max (plugin.getTotalNumOutputChannels(), 2),
                                    256);
    juce::MidiBuffer midi;
    audio.clear();
    plugin.processBlock (audio, midi);
}

template <std::size_t Size>
std::vector<juce::AudioProcessorParameter*> findProductParameters (
    juce::AudioPluginInstance& plugin,
    const std::array<juce::String, Size>& expectedNames)
{
    std::vector<juce::AudioProcessorParameter*> result;
    result.reserve (Size);

    // JUCE's VST3 host adapter also exposes synthetic per-channel MIDI
    // controller parameters. The product contract is the exact, unique set of
    // Electry names below, not the wrapper-level getParameters() count.
    for (const auto& expected : expectedNames)
    {
        juce::AudioProcessorParameter* match = nullptr;
        int matches = 0;
        for (auto* parameter : plugin.getParameters())
            if (parameter != nullptr && parameter->getName (128) == expected)
            {
                match = parameter;
                ++matches;
            }

        expect (matches == 1,
                (matches == 0 ? "missing VST3 parameter: "
                              : "duplicate VST3 parameter: ")
                    + expected.toStdString());
        if (matches == 1)
            result.push_back (match);
    }

    expect (result.size() == Size,
            "VST3 does not expose the exact unique 28-parameter Electry surface");
    return result;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    expect (argc == 2, "expected the exact built VST3 bundle path");
    if (argc != 2)
        return 1;

    const juce::File bundle (juce::String::fromUTF8 (argv[1]));
    expect (bundle.exists() && bundle.hasFileExtension (".vst3"),
            "built VST3 bundle does not exist: "
                + bundle.getFullPathName().toStdString());

    juce::AudioPluginFormatManager manager;
    manager.addFormat (std::make_unique<juce::VST3PluginFormatHeadless>());
    auto* const format = manager.getFormat (0);

    juce::OwnedArray<juce::PluginDescription> descriptions;
    if (format != nullptr)
        format->findAllTypesForFile (descriptions, bundle.getFullPathName());

    expect (descriptions.size() == 1,
            "expected one audio component in the VST3 bundle, found "
                + std::to_string (descriptions.size()));
    if (descriptions.size() != 1)
        return 1;

    const auto& description = *descriptions[0];
    expect (description.name == "Electry", "wrong VST3 name");
    expect (description.manufacturerName == "Electry Audio",
            "wrong VST3 manufacturer");
    expect (description.version == ELECTRY_EXPECTED_VERSION,
            "wrong VST3 version: " + description.version.toStdString());
    expect (description.pluginFormatName == "VST3", "wrong plug-in format");
    expect (description.isInstrument, "VST3 is not described as an instrument");
    expect (juce::File (description.fileOrIdentifier) == bundle,
            "scanner did not describe the requested VST3 bundle");

    auto plugin = instantiate (manager, description);
    if (plugin == nullptr)
        return 1;

    expect (plugin->getName() == "Electry", "instantiated processor has wrong name");
    expect (plugin->acceptsMidi() && ! plugin->producesMidi(),
            "instantiated processor has wrong MIDI capabilities");
    expect (plugin->getTotalNumInputChannels() == 0
                && plugin->getTotalNumOutputChannels() == 2,
            "instantiated VST3 is not a zero-input, stereo instrument");

    const std::array<juce::String, 28> expectedParameterNames {
        "Pickup selector", "Pickup type", "Tone", "Guitar build",
        "Body resonance", "String age", "Pick position", "Pick hardness",
        "Pick noise", "Finger noise", "Release noise", "Mute tightness",
        "Bend time", "Velocity response", "Output level", "Artifacts",
        "Output mode", "Distortion", "Amp simulation", "Compressor",
        "Delay", "Room", "Sympathetic ring", "Mute pressure",
        "Strum spread", "Resonance depth", "Tremolo picking rate", "Amp voice"
    };
    const auto productParameters = findProductParameters (*plugin,
                                                          expectedParameterNames);

    expect (plugin->getNumPrograms() == 4,
            "VST3 does not expose four factory programs");
    const std::array<juce::String, 4> expectedPrograms {
        "Factory Default", "Drop-E Metal", "Mute / Dead DI", "Blues Rock Lead"
    };
    for (int index = 0; index < static_cast<int> (expectedPrograms.size()); ++index)
        expect (plugin->getProgramName (index) == expectedPrograms[static_cast<std::size_t> (index)],
                "wrong VST3 program at index " + std::to_string (index));

    plugin->prepareToPlay (48000.0, 256);
    plugin->setNonRealtime (true);

    plugin->setCurrentProgram (3);
    flushParameters (*plugin);

    std::vector<float> programValues;
    programValues.reserve (productParameters.size());
    for (auto* parameter : productParameters)
        programValues.push_back (parameter->getValue());

    std::vector<float> savedValues;
    savedValues.reserve (productParameters.size());
    for (std::size_t index = 0; index < productParameters.size(); ++index)
    {
        auto* const parameter = productParameters[index];
        float requested = static_cast<float> (index + 1)
                        / static_cast<float> (productParameters.size() + 1);
        requested = parameter->getValueForText (parameter->getText (requested, 128));
        parameter->setValueNotifyingHost (requested);
    }
    flushParameters (*plugin);
    for (auto* parameter : productParameters)
        savedValues.push_back (parameter->getValue());
    expect (! std::equal (savedValues.begin(), savedValues.end(),
                          programValues.begin(),
                          [] (float left, float right)
                          {
                              return std::abs (left - right) < 1.0e-5f;
                          }),
            "post-program parameter poison did not change the Electry state");

    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    expect (state.getSize() > 0, "VST3 returned empty state");

    auto restored = instantiate (manager, description);
    if (restored != nullptr)
    {
        const auto restoredProductParameters = findProductParameters (
            *restored, expectedParameterNames);
        restored->prepareToPlay (48000.0, 256);
        restored->setNonRealtime (true);
        restored->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        flushParameters (*restored);

        expect (restored->getCurrentProgram() == 3,
                "factory program did not survive the VST3 state round-trip");
        expect (restoredProductParameters.size() == savedValues.size(),
                "Electry parameter count changed across the VST3 state round-trip");
        if (restoredProductParameters.size() == savedValues.size())
            for (std::size_t index = 0; index < savedValues.size(); ++index)
            {
                const auto actual = restoredProductParameters[index]->getValue();
                expect (std::abs (actual - savedValues[index]) < 1.0e-5f,
                        "Electry parameter "
                            + expectedParameterNames[index].toStdString()
                            + " did not survive the VST3 state round-trip (expected "
                            + std::to_string (savedValues[index]) + ", got "
                            + std::to_string (actual) + ")");
            }
    }

    juce::AudioBuffer<float> audio (std::max (plugin->getTotalNumOutputChannels(), 2),
                                    256);
    double energy = 0.0;
    float peak = 0.0f;
    bool finite = true;
    for (int block = 0; block < 96; ++block)
    {
        audio.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 40, 0.9f), 0);
        if (block == 72)
            midi.addEvent (juce::MidiMessage::noteOff (1, 40), 0);

        plugin->processBlock (audio, midi);
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                const float value = audio.getSample (channel, sample);
                finite = finite && std::isfinite (value);
                peak = std::max (peak, std::abs (value));
                energy += static_cast<double> (value) * value;
            }
    }
    expect (finite, "VST3 render produced a non-finite sample");
    expect (peak > 1.0e-6f && energy > 1.0e-8,
            "VST3 MIDI render was silent");

    if (restored != nullptr)
        restored->releaseResources();
    plugin->releaseResources();

    if (failures == 0)
        std::cout << "Electry built VST3 artifact smoke test passed "
                     "(28 product parameters; JUCE wrapper parameters excluded)\n";
    return failures == 0 ? 0 : 1;
}
