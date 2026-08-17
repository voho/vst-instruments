#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;

struct ExpectedParameter
{
    const char* id;
    const char* name;
};

constexpr auto expectedParameters = std::to_array<ExpectedParameter> ({
    { "volume", "Volume" },
    { "benderDco", "Bender DCO" },
    { "benderVcf", "Bender VCF" },
    { "benderLfo", "Bender LFO" },
    { "portamento", "Portamento" },
    { "keyMode", "Key Mode (legacy)" },
    { "lfoRate", "LFO Rate" },
    { "lfoDelay", "LFO Delay" },
    { "dcoLfo", "DCO LFO" },
    { "pwm", "PWM" },
    { "pwmMode", "PWM Mode" },
    { "range", "Range" },
    { "saw", "Saw" },
    { "pulse", "Pulse" },
    { "sub", "Sub" },
    { "noise", "Noise" },
    { "highPass", "HPF" },
    { "cutoff", "VCF Freq" },
    { "resonance", "VCF Res" },
    { "envPolarity", "VCF Env Polarity" },
    { "vcfEnv", "VCF Env" },
    { "vcfLfo", "VCF LFO" },
    { "keyFollow", "VCF Kybd" },
    { "vcaMode", "VCA Mode" },
    { "vcaLevel", "VCA Level" },
    { "attack", "Attack" },
    { "decay", "Decay" },
    { "sustain", "Sustain" },
    { "release", "Release" },
    { "chorus", "Chorus (legacy)" },
    { "transpose", "Transpose" },
    { "masterTune", "Master Tune" },
    { "velocity", "Velocity" },
    { "calibration", "Unit Character" },
    { "chorusNoise", "Chorus Noise" },
    { "polyphony", "Polyphony" },
    { "poly1", "Poly 1" },
    { "poly2", "Poly 2" },
    { "chorusI", "Chorus I" },
    { "chorusII", "Chorus II" },
    { "hq", "HQ" },
    { "quality", "Quality" },
});

bool expect (bool condition, const juce::String& message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

std::unique_ptr<juce::AudioPluginInstance> createInstance (
    juce::VST3PluginFormatHeadless& format,
    const juce::PluginDescription& description)
{
    juce::String error;
    auto instance = format.createInstanceFromDescription (
        description, sampleRate, blockSize, error);
    if (instance == nullptr)
        std::cerr << "FAIL: VST3 instantiation failed: " << error << '\n';
    return instance;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    if (! expect (argc == 2, "expected one VST3 bundle path"))
        return 1;

    const juce::File bundle { argv[1] };
    if (! expect (bundle.isDirectory() && bundle.hasFileExtension ("vst3"),
                  "the built VST3 bundle is missing"))
        return 1;

    juce::VST3PluginFormatHeadless format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile (descriptions, bundle.getFullPathName());
    if (! expect (descriptions.size() == 1,
                  "the VST3 did not expose exactly one instrument class"))
        return 1;

    const auto& description = *descriptions[0];
    bool passed = true;
    passed &= expect (description.name == "YouKnow106", "wrong VST3 name");
    passed &= expect (description.manufacturerName == "Protocodus",
                      "wrong VST3 vendor");
    passed &= expect (description.version == "1.1.0", "wrong VST3 version");
    passed &= expect (description.isInstrument, "VST3 is not an instrument");
    passed &= expect (description.numInputChannels == 0,
                      "VST3 unexpectedly exposes an audio input");
    if (! passed)
        return 1;

    auto instance = createInstance (format, description);
    if (instance == nullptr)
        return 1;

    passed &= expect (instance->acceptsMidi() && ! instance->producesMidi(),
                      "VST3 MIDI capabilities are wrong");
    passed &= expect (instance->getNumPrograms() == 129,
                      "VST3 factory-program count is wrong");
    const auto& hostedParameters = instance->getParameters();
    juce::AudioProcessorParameter* bypassParameter = nullptr;
    passed &= expect (hostedParameters.size()
                          >= static_cast<int> (expectedParameters.size()) + 2,
                      "VST3 omitted public, bypass, or program parameters");
    if (hostedParameters.size()
        >= static_cast<int> (expectedParameters.size()) + 2)
    {
        for (std::size_t index = 0; index < expectedParameters.size(); ++index)
        {
            const auto* parameter = hostedParameters[static_cast<int> (index)];
            const auto* hosted =
                dynamic_cast<const juce::HostedAudioProcessorParameter*> (parameter);
            const auto& expected = expectedParameters[index];
            const auto expectedVstId = static_cast<std::uint32_t> (
                                           juce::String (expected.id).hashCode())
                                       & 0x7fffffffu;
            passed &= expect (parameter->getName (128) == expected.name,
                              juce::String ("wrong VST3 parameter name at ")
                                  + juce::String (static_cast<int> (index)));
            passed &= expect (hosted != nullptr
                                  && hosted->getParameterID()
                                         == juce::String (expectedVstId),
                              juce::String ("unstable VST3 parameter ID for ")
                                  + expected.id);
        }

        passed &= expect (
            hostedParameters[static_cast<int> (expectedParameters.size())]
                    ->getName (128)
                == "Bypass",
            "VST3 omitted its standard bypass parameter");
        bypassParameter =
            hostedParameters[static_cast<int> (expectedParameters.size())];
        passed &= expect (
            hostedParameters[static_cast<int> (expectedParameters.size()) + 1]
                    ->getName (128)
                == "Program",
            "VST3 omitted its program-change parameter");
    }

    // Hosts are allowed to pass an in-between normalised value even for a
    // stepped parameter. State recall must still restore the controller's
    // public value, including when both values resolve to the same switch
    // position internally. This mirrors pluginval's state-restoration probe.
    juce::MemoryBlock discreteState;
    instance->getStateInformation (discreteState);
    for (const auto [index, changed] :
         { std::pair { 13, 0.413568f }, std::pair { 37, 0.454620f } })
    {
        auto* parameter = hostedParameters[index];
        const float original = parameter->getValue();
        parameter->setValue (changed);
        instance->setStateInformation (discreteState.getData(),
                                       static_cast<int> (discreteState.getSize()));
        passed &= expect (std::abs (parameter->getValue() - original) < 1.0e-6f,
                          parameter->getName (128)
                              + " did not restore its VST3 controller value");
    }

    instance->setPlayConfigDetails (0, 2, sampleRate, blockSize);
    passed &= expect (instance->getTotalNumInputChannels() == 0
                          && instance->getTotalNumOutputChannels() == 2,
                      "VST3 did not accept its mono-instrument/stereo-output layout");
    instance->prepareToPlay (sampleRate, blockSize);
    passed &= expect (instance->getLatencySamples() > 0,
                      "VST3 did not report its processing latency");
    juce::AudioBuffer<float> audio (2, blockSize);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    bool finite = true;
    for (int block = 0; block < 8; ++block)
    {
        audio.clear();
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        instance->processBlock (audio, midi);
        midi.clear();

        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                const float value = audio.getSample (channel, sample);
                finite = finite && std::isfinite (value);
                peak = std::max (peak, std::abs (value));
            }
    }
    passed &= expect (finite, "VST3 rendered a non-finite sample");
    passed &= expect (peak > 1.0e-5f, "VST3 rendered silence for a MIDI note");

    if (bypassParameter != nullptr)
    {
        bypassParameter->setValueNotifyingHost (1.0f);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        bool bypassSilent = true;
        for (int block = 0; block < 4; ++block)
        {
            audio.clear();
            instance->processBlock (audio, midi);
            bypassSilent = bypassSilent && midi.isEmpty();
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                    bypassSilent = bypassSilent
                        && audio.getSample (channel, sample) == 0.0f;
        }
        passed &= expect (bypassSilent,
                          "VST3 bypass emitted audio or leaked input MIDI");

        bypassParameter->setValueNotifyingHost (0.0f);
        midi.addEvent (juce::MidiMessage::noteOn (1, 67, 1.0f), 0);
        float resumedPeak = 0.0f;
        for (int block = 0; block < 4; ++block)
        {
            audio.clear();
            instance->processBlock (audio, midi);
            midi.clear();
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                    resumedPeak = std::max (
                        resumedPeak, std::abs (audio.getSample (channel, sample)));
        }
        passed &= expect (resumedPeak > 1.0e-5f,
                          "VST3 did not resume after standard bypass");
    }
    else
    {
        passed = false;
    }

    instance->setCurrentProgram (1);
    juce::MemoryBlock state;
    instance->getStateInformation (state);
    passed &= expect (! state.isEmpty(), "VST3 returned empty state");

    auto restored = createInstance (format, description);
    if (restored != nullptr && ! state.isEmpty())
    {
        restored->setStateInformation (state.getData(),
                                       static_cast<int> (state.getSize()));
        passed &= expect (restored->getCurrentProgram() == 1,
                          "VST3 did not restore its selected program");
    }
    else
    {
        passed = false;
    }

    instance->releaseResources();
    if (! passed)
        return 1;

    std::cout << "YouKnow106 VST3 bundle smoke test passed.\n";
    return 0;
}
