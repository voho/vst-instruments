#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <tuple>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
int failureCount = 0;

struct ParameterExpectation
{
    const char* id;
    float defaultValue;
    float tolerance;
};

constexpr std::array<ParameterExpectation, 55> expectedParameters {{
    { mars::parameters::osc1Wave,         0.0f,   1.0e-5f },
    { mars::parameters::osc1Octave,       0.0f,   1.0e-5f },
    { mars::parameters::osc2Wave,         1.0f,   1.0e-5f },
    { mars::parameters::osc2Octave,       0.0f,   1.0e-5f },
    { mars::parameters::osc2Tune,         0.0f,   1.0e-5f },
    { mars::parameters::osc2Fine,         0.0f,   1.0e-5f },
    { mars::parameters::oscMix,           0.46f,  1.0e-5f },
    { mars::parameters::pulseWidth,       0.50f,  1.0e-5f },
    { mars::parameters::subLevel,         0.16f,  1.0e-5f },
    { mars::parameters::noiseLevel,       0.025f, 1.0e-5f },
    { mars::parameters::crossMod,         0.06f,  1.0e-5f },
    { mars::parameters::filterModel,      0.0f,   1.0e-5f },
    { mars::parameters::cutoff,        4200.0f,   0.1f },
    { mars::parameters::resonance,        0.28f,  1.0e-5f },
    { mars::parameters::filterDrive,      0.22f,  1.0e-5f },
    { mars::parameters::filterShape,      0.35f,  1.0e-5f },
    { mars::parameters::filterEnvAmount,  0.46f,  1.0e-5f },
    { mars::parameters::keyTrack,         0.50f,  1.0e-5f },
    { mars::parameters::fAttack,          0.012f, 1.0e-5f },
    { mars::parameters::fDecay,           0.45f,  1.0e-5f },
    { mars::parameters::fSustain,         0.34f,  1.0e-5f },
    { mars::parameters::fRelease,         0.62f,  1.0e-5f },
    { mars::parameters::aAttack,          0.008f, 1.0e-5f },
    { mars::parameters::aDecay,           0.38f,  1.0e-5f },
    { mars::parameters::aSustain,         0.78f,  1.0e-5f },
    { mars::parameters::aRelease,         0.55f,  1.0e-5f },
    { mars::parameters::lfoWave,          0.0f,   1.0e-5f },
    { mars::parameters::lfoRate,          4.8f,   1.0e-5f },
    { mars::parameters::lfoPitch,         8.0f,   1.0e-5f },
    { mars::parameters::lfoFilter,        0.18f,  1.0e-5f },
    { mars::parameters::lfoPwm,           0.16f,  1.0e-5f },
    { mars::parameters::voiceMode,        0.0f,   1.0e-5f },
    { mars::parameters::unisonVoices,     4.0f,   1.0e-5f },
    { mars::parameters::drift,            0.28f,  1.0e-5f },
    { mars::parameters::spread,           0.58f,  1.0e-5f },
    { mars::parameters::glide,            0.0f,   1.0e-5f },
    { mars::parameters::velocity,         0.68f,  1.0e-5f },
    { mars::parameters::chorusMix,        0.30f,  1.0e-5f },
    { mars::parameters::chorusRate,       0.38f,  1.0e-5f },
    { mars::parameters::output,          -6.0f,   1.0e-5f },
    { mars::parameters::osc1Enabled,      1.0f,   1.0e-5f },
    { mars::parameters::osc2Enabled,      1.0f,   1.0e-5f },
    { mars::parameters::hqOversampling,   1.0f,   1.0e-5f },
    { mars::parameters::osc1Model,        0.0f,   1.0e-5f },
    { mars::parameters::osc2Model,        0.0f,   1.0e-5f },
    { mars::parameters::polyphonyLimit,  16.0f,   1.0e-5f },
    { mars::parameters::monoMode,         0.0f,   1.0e-5f },
    { mars::parameters::chorusCompander,  0.0f,   1.0e-5f },
    { mars::parameters::unisonDetune,     9.6f,   1.0e-5f },
    { mars::parameters::arpEnabled,       0.0f,   1.0e-5f },
    { mars::parameters::arpMode,          0.0f,   1.0e-5f },
    { mars::parameters::arpRate,          5.0f,   1.0e-5f },
    { mars::parameters::arpOctaves,       1.0f,   1.0e-5f },
    { mars::parameters::arpGate,          0.55f,  1.0e-5f },
    { mars::parameters::arpHold,          0.0f,   1.0e-5f },
}};

constexpr std::size_t version1ParameterCount = 40;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool approximatelyEqual (float actual, float expected, float tolerance = 1.0e-5f)
{
    return std::abs (actual - expected) <= tolerance;
}

float parameterValue (const MarsAudioProcessor& processor, const char* id)
{
    const auto* value = processor.parameters.getRawParameterValue (id);
    expect (value != nullptr, std::string ("missing parameter ") + id);
    return value != nullptr ? value->load (std::memory_order_relaxed) : 0.0f;
}

void setParameterValue (MarsAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, std::string ("cannot set missing parameter ") + id);
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

struct GestureCounter final : juce::AudioProcessorParameter::Listener
{
    void parameterValueChanged (int, float) override {}

    void parameterGestureChanged (int, bool gestureIsStarting) override
    {
        if (gestureIsStarting)
            ++starts;
        else
            ++ends;
    }

    int starts = 0;
    int ends = 0;
};

void expectParameterText (MarsAudioProcessor& processor, const char* id, float value,
                          const char* expectedText)
{
    const auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, std::string ("cannot format missing parameter ") + id);
    if (parameter == nullptr)
        return;

    const auto actualText = parameter->getText (parameter->convertTo0to1 (value), 64);
    expect (actualText == expectedText,
            std::string ("wrong formatted text for ") + id + ": expected "
                + expectedText + ", got " + actualText.toStdString());
}

float peakInRange (const juce::AudioBuffer<float>& buffer, int start, int end)
{
    const auto first = juce::jlimit (0, buffer.getNumSamples(), start);
    const auto last = juce::jlimit (first, buffer.getNumSamples(), end);
    float peak = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = first; sample < last; ++sample)
            peak = std::max (peak, std::abs (buffer.getSample (channel, sample)));

    return peak;
}

void renderBlock (MarsAudioProcessor& processor, juce::AudioBuffer<float>& audio,
                  juce::MidiBuffer& midi)
{
    audio.setSize (2, blockSize, false, false, true);
    audio.clear();
    processor.processBlock (audio, midi);
}

void renderBlock (MarsAudioProcessor& processor, juce::AudioBuffer<float>& audio,
                  juce::MidiBuffer& midi, int numSamples)
{
    audio.setSize (2, numSamples, false, false, true);
    audio.clear();
    processor.processBlock (audio, midi);
}

bool renderUntilVoicesStop (MarsAudioProcessor& processor,
                            juce::AudioBuffer<float>& audio, int maximumBlocks = 256)
{
    juce::MidiBuffer emptyMidi;
    for (int block = 0; block < maximumBlocks && processor.getActiveVoiceCount() > 0; ++block)
        renderBlock (processor, audio, emptyMidi);
    return processor.getActiveVoiceCount() == 0;
}

float renderEmptySamples (MarsAudioProcessor& processor,
                          juce::AudioBuffer<float>& audio, int sampleCount)
{
    juce::MidiBuffer emptyMidi;
    float finalBlockPeak = 0.0f;
    int rendered = 0;
    while (rendered < sampleCount)
    {
        const auto samplesThisBlock = std::min (blockSize, sampleCount - rendered);
        renderBlock (processor, audio, emptyMidi, samplesThisBlock);
        finalBlockPeak = peakInRange (audio, 0, audio.getNumSamples());
        rendered += samplesThisBlock;
    }
    return finalBlockPeak;
}

void useShortReleases (MarsAudioProcessor& processor)
{
    setParameterValue (processor, mars::parameters::fRelease, 0.010f);
    setParameterValue (processor, mars::parameters::aRelease, 0.010f);
}

void testParameterLayoutAndDefaults()
{
    MarsAudioProcessor processor;
    expect (expectedParameters.size() == 55u,
            "test parameter manifest does not contain exactly 55 entries");
    expect (processor.getParameters().size() == static_cast<int> (expectedParameters.size()),
            "processor does not expose exactly 55 APVTS parameters");

    std::set<std::string> uniqueIds;
    const auto& hostParameters = processor.getParameters();
    for (std::size_t index = 0; index < expectedParameters.size(); ++index)
    {
        const auto& expected = expectedParameters[index];
        expect (uniqueIds.insert (expected.id).second,
                std::string ("duplicate APVTS id ") + expected.id);

        const auto* parameter = processor.parameters.getParameter (expected.id);
        expect (parameter != nullptr,
                std::string ("APVTS does not contain ") + expected.id);
        if (parameter == nullptr)
            continue;

        const auto declaredDefault = parameter->convertFrom0to1 (parameter->getDefaultValue());
        expect (approximatelyEqual (declaredDefault, expected.defaultValue, expected.tolerance),
                std::string ("wrong declared default for ") + expected.id);
        expect (approximatelyEqual (parameterValue (processor, expected.id),
                                    expected.defaultValue, expected.tolerance),
                std::string ("wrong initial value for ") + expected.id);

        const auto* hostParameter = hostParameters[static_cast<int> (index)];
        const auto* parameterWithId = dynamic_cast<const juce::AudioProcessorParameterWithID*> (
            hostParameter);
        expect (parameterWithId != nullptr,
                std::string ("host parameter has no stable id at index ")
                    + std::to_string (index));
        if (parameterWithId != nullptr)
            expect (parameterWithId->paramID == expected.id,
                    std::string ("host parameter order changed at index ")
                        + std::to_string (index));

        const auto expectedVersionHint = index < version1ParameterCount ? 1
                                       : index < 42u ? 2
                                       : index == 42u ? 3
                                       : index < 47u ? 4 : 5;
        expect (hostParameter->getVersionHint() == expectedVersionHint,
                std::string ("wrong parameter version hint for ") + expected.id);

        const bool shouldBeAutomatable = std::string (expected.id)
                                      != mars::parameters::hqOversampling;
        expect (hostParameter->isAutomatable() == shouldBeAutomatable,
                std::string ("wrong automation contract for ") + expected.id);
    }

    expect (uniqueIds.size() == expectedParameters.size(),
            "the 55-entry APVTS manifest contains duplicate ids");
}

void testParameterTextFormatting()
{
    MarsAudioProcessor processor;
    expectParameterText (processor, mars::parameters::oscMix, 0.46f, "46%");
    expectParameterText (processor, mars::parameters::filterEnvAmount, 0.46f, "+46%");
    expectParameterText (processor, mars::parameters::osc1Octave, 1.0f, "+1oct");
    expectParameterText (processor, mars::parameters::osc2Tune, -7.0f, "-7st");
    expectParameterText (processor, mars::parameters::osc2Fine, 12.3f, "+12.3ct");
    expectParameterText (processor, mars::parameters::filterModel, 1.0f, "SEM");
    expectParameterText (processor, mars::parameters::lfoFilter, 0.18f, "0.180oct");
    expectParameterText (processor, mars::parameters::output, -6.0f, "-6.0dB");
    expectParameterText (processor, mars::parameters::osc1Enabled, 0.0f, "Off");
    expectParameterText (processor, mars::parameters::osc2Enabled, 1.0f, "On");
    expectParameterText (processor, mars::parameters::hqOversampling, 0.0f, "Off");
    expectParameterText (processor, mars::parameters::hqOversampling, 1.0f, "On");
    expectParameterText (processor, mars::parameters::osc1Model, 0.0f, "Moog-like VCO");
    expectParameterText (processor, mars::parameters::osc2Model, 1.0f, "Juno-like DCO");
    expectParameterText (processor, mars::parameters::polyphonyLimit, 8.0f, "8 voices");
    expectParameterText (processor, mars::parameters::monoMode, 0.0f, "Off");
    expectParameterText (processor, mars::parameters::monoMode, 1.0f, "On");
    expectParameterText (processor, mars::parameters::chorusCompander, 0.0f, "Off");
    expectParameterText (processor, mars::parameters::chorusCompander, 1.0f, "On");
    expectParameterText (processor, mars::parameters::unisonDetune, 9.6f, "9.6ct");
    expectParameterText (processor, mars::parameters::arpEnabled, 0.0f, "Off");
    expectParameterText (processor, mars::parameters::arpEnabled, 1.0f, "On");
    expectParameterText (processor, mars::parameters::arpMode, 2.0f, "Up-down");
    expectParameterText (processor, mars::parameters::arpMode, 4.0f, "As played");
    expectParameterText (processor, mars::parameters::arpRate, 5.0f, "5.00 Hz");
    expectParameterText (processor, mars::parameters::arpOctaves, 3.0f, "3 oct");
    expectParameterText (processor, mars::parameters::arpGate, 0.55f, "55%");
    expectParameterText (processor, mars::parameters::arpHold, 1.0f, "On");
}

void testStateRoundTrip()
{
    MarsAudioProcessor source;
    setParameterValue (source, mars::parameters::osc2Wave, 2.0f);
    setParameterValue (source, mars::parameters::osc2Fine, -17.3f);
    setParameterValue (source, mars::parameters::cutoff, 777.0f);
    setParameterValue (source, mars::parameters::filterEnvAmount, -0.37f);
    setParameterValue (source, mars::parameters::voiceMode, 2.0f);
    setParameterValue (source, mars::parameters::output, 2.3f);
    setParameterValue (source, mars::parameters::osc1Enabled, 0.0f);
    setParameterValue (source, mars::parameters::osc2Enabled, 0.0f);
    setParameterValue (source, mars::parameters::hqOversampling, 0.0f);
    setParameterValue (source, mars::parameters::osc1Model, 1.0f);
    setParameterValue (source, mars::parameters::osc2Model, 1.0f);
    setParameterValue (source, mars::parameters::polyphonyLimit, 7.0f);
    setParameterValue (source, mars::parameters::monoMode, 1.0f);
    setParameterValue (source, mars::parameters::chorusCompander, 1.0f);
    setParameterValue (source, mars::parameters::unisonDetune, 33.4f);
    setParameterValue (source, mars::parameters::arpEnabled, 1.0f);
    setParameterValue (source, mars::parameters::arpMode, 3.0f);
    setParameterValue (source, mars::parameters::arpRate, 11.5f);
    setParameterValue (source, mars::parameters::arpOctaves, 4.0f);
    setParameterValue (source, mars::parameters::arpGate, 0.21f);
    setParameterValue (source, mars::parameters::arpHold, 1.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    expect (state.getSize() > 0, "processor produced empty state");

    MarsAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::osc2Wave), 2.0f),
            "VCO II waveform did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::osc2Fine),
                                -17.3f, 0.11f),
            "VCO II fine tune did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::cutoff),
                                777.0f, 0.5f),
            "filter cutoff did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::filterEnvAmount),
                                -0.37f, 0.002f),
            "bipolar filter envelope amount did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::voiceMode), 2.0f),
            "voice mode did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::output),
                                2.3f, 0.051f),
            "output level did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::osc1Enabled), 0.0f),
            "VCO I enabled state did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::osc2Enabled), 0.0f),
            "VCO II enabled state did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::hqOversampling), 0.0f),
            "HQ oversampling state did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::osc1Model), 1.0f)
                && approximatelyEqual (
                    parameterValue (restored, mars::parameters::osc2Model), 1.0f),
            "oscillator models did not survive state round-trip");
    expect (approximatelyEqual (
                parameterValue (restored, mars::parameters::polyphonyLimit), 7.0f),
            "polyphony limit did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::monoMode), 1.0f),
            "mono mode did not survive state round-trip");
    expect (approximatelyEqual (
                parameterValue (restored, mars::parameters::chorusCompander), 1.0f),
            "ensemble compander did not survive state round-trip");
    expect (approximatelyEqual (
                parameterValue (restored, mars::parameters::unisonDetune), 33.4f, 0.11f),
            "unison detune did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::arpEnabled), 1.0f)
                && approximatelyEqual (
                    parameterValue (restored, mars::parameters::arpMode), 3.0f)
                && approximatelyEqual (
                    parameterValue (restored, mars::parameters::arpOctaves), 4.0f)
                && approximatelyEqual (
                    parameterValue (restored, mars::parameters::arpHold), 1.0f),
            "arpeggiator switches did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::arpRate),
                                11.5f, 0.05f),
            "arpeggiator rate did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::arpGate),
                                0.21f, 0.002f),
            "arpeggiator gate did not survive state round-trip");
}

bool isParameterState (const juce::ValueTree& child, const char* parameterId)
{
    return child.hasType ("PARAM")
        && child.getProperty ("id").toString() == parameterId;
}

void removeParameterState (juce::ValueTree& state, const char* parameterId)
{
    for (int index = state.getNumChildren(); --index >= 0;)
        if (isParameterState (state.getChild (index), parameterId))
            state.removeChild (index, nullptr);
}

bool containsParameterState (const juce::ValueTree& state, const char* parameterId)
{
    for (const auto& child : state)
        if (isParameterState (child, parameterId))
            return true;
    return false;
}

void testLegacyStateDefaultsNewParameters()
{
    MarsAudioProcessor legacySource;
    setParameterValue (legacySource, mars::parameters::cutoff, 1234.0f);
    auto legacyState = legacySource.parameters.copyState();
    removeParameterState (legacyState, mars::parameters::osc1Enabled);
    removeParameterState (legacyState, mars::parameters::osc2Enabled);
    removeParameterState (legacyState, mars::parameters::hqOversampling);
    removeParameterState (legacyState, mars::parameters::osc1Model);
    removeParameterState (legacyState, mars::parameters::osc2Model);
    removeParameterState (legacyState, mars::parameters::polyphonyLimit);
    removeParameterState (legacyState, mars::parameters::monoMode);
    removeParameterState (legacyState, mars::parameters::chorusCompander);
    for (const auto* parameterId : { mars::parameters::unisonDetune,
                                     mars::parameters::arpEnabled,
                                     mars::parameters::arpMode,
                                     mars::parameters::arpRate,
                                     mars::parameters::arpOctaves,
                                     mars::parameters::arpGate,
                                     mars::parameters::arpHold })
        removeParameterState (legacyState, parameterId);
    expect (! containsParameterState (legacyState, mars::parameters::osc1Enabled)
                && ! containsParameterState (legacyState, mars::parameters::osc2Enabled)
                && ! containsParameterState (legacyState, mars::parameters::hqOversampling)
                && ! containsParameterState (legacyState, mars::parameters::osc1Model)
                && ! containsParameterState (legacyState, mars::parameters::osc2Model)
                && ! containsParameterState (legacyState, mars::parameters::polyphonyLimit)
                && ! containsParameterState (legacyState, mars::parameters::monoMode)
                && ! containsParameterState (
                    legacyState, mars::parameters::chorusCompander),
            "could not construct a version-1 state without later parameters");

    juce::MemoryBlock binaryState;
    if (const auto xml = legacyState.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, binaryState);
    expect (binaryState.getSize() > 0, "could not serialise version-1 state fixture");

    MarsAudioProcessor restored;
    setParameterValue (restored, mars::parameters::osc1Enabled, 0.0f);
    setParameterValue (restored, mars::parameters::osc2Enabled, 0.0f);
    setParameterValue (restored, mars::parameters::hqOversampling, 0.0f);
    setParameterValue (restored, mars::parameters::osc1Model, 1.0f);
    setParameterValue (restored, mars::parameters::osc2Model, 1.0f);
    setParameterValue (restored, mars::parameters::polyphonyLimit, 3.0f);
    setParameterValue (restored, mars::parameters::monoMode, 1.0f);
    setParameterValue (restored, mars::parameters::chorusCompander, 1.0f);
    setParameterValue (restored, mars::parameters::unisonDetune, 41.0f);
    setParameterValue (restored, mars::parameters::arpEnabled, 1.0f);
    setParameterValue (restored, mars::parameters::arpMode, 3.0f);
    setParameterValue (restored, mars::parameters::arpRate, 17.0f);
    setParameterValue (restored, mars::parameters::arpOctaves, 4.0f);
    setParameterValue (restored, mars::parameters::arpGate, 0.9f);
    setParameterValue (restored, mars::parameters::arpHold, 1.0f);
    restored.setStateInformation (binaryState.getData(), static_cast<int> (binaryState.getSize()));

    expect (approximatelyEqual (parameterValue (restored, mars::parameters::osc1Enabled), 1.0f),
            "version-1 state did not default the missing VCO I enable to On");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::osc2Enabled), 1.0f),
            "version-1 state did not default the missing VCO II enable to On");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::hqOversampling), 1.0f),
            "legacy state did not default missing HQ oversampling to On");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::osc1Model), 0.0f)
                && approximatelyEqual (
                    parameterValue (restored, mars::parameters::osc2Model), 0.0f),
            "legacy state did not default missing oscillator models to Moog-like VCO");
    expect (approximatelyEqual (
                parameterValue (restored, mars::parameters::polyphonyLimit), 16.0f),
            "legacy state did not default the missing polyphony limit to 16");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::monoMode), 0.0f),
            "legacy state did not default missing mono mode to Off");
    expect (approximatelyEqual (
                parameterValue (restored, mars::parameters::chorusCompander), 0.0f),
            "legacy state did not default missing ensemble compander to Off");
    expect (approximatelyEqual (
                parameterValue (restored, mars::parameters::unisonDetune), 9.6f, 0.05f),
            "legacy state did not default unison detune to the previous 9.6 cent spread");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::arpEnabled), 0.0f)
                && approximatelyEqual (
                    parameterValue (restored, mars::parameters::arpHold), 0.0f)
                && approximatelyEqual (
                    parameterValue (restored, mars::parameters::arpMode), 0.0f)
                && approximatelyEqual (
                    parameterValue (restored, mars::parameters::arpOctaves), 1.0f),
            "legacy state did not default the arpeggiator to Off, Up, one octave");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::arpRate),
                                5.0f, 0.02f)
                && approximatelyEqual (
                    parameterValue (restored, mars::parameters::arpGate), 0.55f, 0.002f),
            "legacy state did not default the arpeggiator rate and gate");
    expect (approximatelyEqual (parameterValue (restored, mars::parameters::cutoff),
                                1234.0f, 0.5f),
            "version-1 migration did not restore existing parameter values");

    const auto upgradedState = restored.parameters.copyState();
    expect (containsParameterState (upgradedState, mars::parameters::osc1Enabled)
                && containsParameterState (upgradedState, mars::parameters::osc2Enabled)
                && containsParameterState (upgradedState, mars::parameters::hqOversampling)
                && containsParameterState (upgradedState, mars::parameters::osc1Model)
                && containsParameterState (upgradedState, mars::parameters::osc2Model)
                && containsParameterState (upgradedState, mars::parameters::polyphonyLimit)
                && containsParameterState (upgradedState, mars::parameters::monoMode)
                && containsParameterState (
                    upgradedState, mars::parameters::chorusCompander)
                && containsParameterState (upgradedState, mars::parameters::unisonDetune)
                && containsParameterState (upgradedState, mars::parameters::arpEnabled)
                && containsParameterState (upgradedState, mars::parameters::arpMode)
                && containsParameterState (upgradedState, mars::parameters::arpRate)
                && containsParameterState (upgradedState, mars::parameters::arpOctaves)
                && containsParameterState (upgradedState, mars::parameters::arpGate)
                && containsParameterState (upgradedState, mars::parameters::arpHold),
            "migrated state did not persist the appended parameters");
}

bool isExcludedFromRandomizer (const char* id)
{
    const std::string parameterId { id };
    return parameterId == mars::parameters::output
        || parameterId == mars::parameters::osc1Enabled
        || parameterId == mars::parameters::osc2Enabled
        || parameterId == mars::parameters::hqOversampling
        || parameterId == mars::parameters::polyphonyLimit
        || parameterId == mars::parameters::monoMode
        || parameterId == mars::parameters::arpEnabled
        || parameterId == mars::parameters::arpHold;
}

void testPresetRandomizerRangeAndSafety()
{
    for (const float amount : { 0.01f, 0.10f, 1.0f })
    {
        MarsAudioProcessor processor;
        std::array<float, expectedParameters.size()> before {};
        std::array<juce::RangedAudioParameter*, expectedParameters.size()> parameterObjects {};
        GestureCounter gestures;

        for (std::size_t index = 0; index < expectedParameters.size(); ++index)
        {
            auto* parameter = processor.parameters.getParameter (expectedParameters[index].id);
            parameterObjects[index] = parameter;
            expect (parameter != nullptr,
                    std::string ("randomizer cannot find ") + expectedParameters[index].id);
            if (parameter != nullptr)
            {
                before[index] = parameter->getValue();
                parameter->addListener (&gestures);
            }
        }

        processor.randomizeParameters (amount);

        int changedSoundParameters = 0;
        for (std::size_t index = 0; index < expectedParameters.size(); ++index)
        {
            auto* parameter = parameterObjects[index];
            if (parameter == nullptr)
                continue;

            const float after = parameter->getValue();
            const float movement = std::abs (after - before[index]);
            expect (after >= 0.0f && after <= 1.0f,
                    std::string ("randomizer left the legal range for ")
                        + expectedParameters[index].id);

            if (isExcludedFromRandomizer (expectedParameters[index].id))
            {
                expect (movement == 0.0f,
                        std::string ("randomizer changed safety/operational parameter ")
                            + expectedParameters[index].id);
            }
            else
            {
                // Continuous parameters can move by at most the requested
                // fraction. The small allowance is for their declared snapping
                // interval after the normalised destination is calculated.
                expect (movement <= amount + 0.002f,
                        std::string ("randomizer exceeded its requested range for ")
                            + expectedParameters[index].id);
                changedSoundParameters += movement > 0.0f ? 1 : 0;
            }

            parameter->removeListener (&gestures);
        }

        expect (changedSoundParameters > 0,
                "preset randomizer did not change any sound parameters");
        expect (gestures.starts == changedSoundParameters
                    && gestures.ends == changedSoundParameters,
                "preset randomizer did not bracket every host change with one gesture");
    }

    MarsAudioProcessor noOp;
    std::array<float, expectedParameters.size()> beforeNoOp {};
    for (std::size_t index = 0; index < expectedParameters.size(); ++index)
        beforeNoOp[index] = noOp.parameters.getParameter (
            expectedParameters[index].id)->getValue();
    noOp.randomizeParameters (0.0f);
    noOp.randomizeParameters (std::numeric_limits<float>::quiet_NaN());
    for (std::size_t index = 0; index < expectedParameters.size(); ++index)
        expect (approximatelyEqual (
                    noOp.parameters.getParameter (expectedParameters[index].id)->getValue(),
                    beforeNoOp[index]),
                std::string ("zero/NaN randomization changed ")
                    + expectedParameters[index].id);
}

void testOversamplingConfigurationAndDeferredSwitch()
{
    MarsAudioProcessor defaultQuality;
    useShortReleases (defaultQuality);
    defaultQuality.prepareToPlay (sampleRate, blockSize);
    expect (defaultQuality.getOversamplingFactorForDisplay() == 4,
            "default HQ setting did not prepare the 48 kHz engine at 4x");
    expect (defaultQuality.getLatencySamples() == 51,
            "4x HQ processing did not report its 51-sample cascade latency");

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (
                       1, 48, static_cast<juce::uint8> (112)), 0);
    renderBlock (defaultQuality, audio, midi);
    expect (defaultQuality.getActiveVoiceCount() > 0,
            "oversampling switch test did not start its held note");

    setParameterValue (defaultQuality, mars::parameters::hqOversampling, 0.0f);
    midi.clear();
    renderBlock (defaultQuality, audio, midi);
    expect (defaultQuality.getOversamplingFactorForDisplay() == 4,
            "HQ switch changed the processing rate while a note was held");
    expect (defaultQuality.getActiveVoiceCount() > 0,
            "HQ switch killed the held note instead of deferring");
    expect (peakInRange (audio, 0, blockSize) > 1.0e-7f,
            "held note became silent while the HQ switch was pending");

    midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0);
    renderBlock (defaultQuality, audio, midi);
    expect (renderUntilVoicesStop (defaultQuality, audio),
            "oversampling switch test voice did not finish after note-off");

    // The engine deliberately waits for 25 ms of silence before changing its
    // nonlinear timebase. A handful of 512-sample blocks exceeds that at 48 kHz.
    for (int block = 0;
         block < 8 && defaultQuality.getOversamplingFactorForDisplay() != 1;
         ++block)
    {
        midi.clear();
        renderBlock (defaultQuality, audio, midi);
    }
    expect (defaultQuality.getOversamplingFactorForDisplay() == 1,
            "pending HQ-Off setting did not apply after release and idle");
    expect (defaultQuality.getLatencySamples() == 51,
            "deferred HQ switch changed the fixed per-session latency contract");
    defaultQuality.releaseResources();

    MarsAudioProcessor preparedWithoutHq;
    setParameterValue (preparedWithoutHq, mars::parameters::hqOversampling, 0.0f);
    preparedWithoutHq.prepareToPlay (sampleRate, blockSize);
    expect (preparedWithoutHq.getOversamplingFactorForDisplay() == 1,
            "HQ-Off setting did not prepare the 48 kHz engine at 1x");
    expect (preparedWithoutHq.getLatencySamples() == 51,
            "HQ-Off preparation did not preserve the fixed 48 kHz latency");
    preparedWithoutHq.releaseResources();

    for (const auto& [highRate, expectedFactor, expectedLatency]
         : { std::tuple { 96000.0, 2, 34 }, std::tuple { 192000.0, 1, 0 } })
    {
        MarsAudioProcessor highRateProcessor;
        highRateProcessor.prepareToPlay (highRate, blockSize);
        expect (highRateProcessor.getOversamplingFactorForDisplay() == expectedFactor,
                "rate-aware HQ topology selected the wrong factor");
        expect (highRateProcessor.getLatencySamples() == expectedLatency,
                "rate-aware HQ topology reported the wrong latency");
        highRateProcessor.releaseResources();
    }
}

void testBusAndPluginContract()
{
    MarsAudioProcessor processor;
    expect (processor.getBusCount (true) == 0, "instrument unexpectedly exposes an input bus");
    expect (processor.getBusCount (false) == 1, "instrument does not expose one output bus");
    expect (processor.getTotalNumInputChannels() == 0,
            "instrument unexpectedly exposes input channels");
    expect (processor.getTotalNumOutputChannels() == 2,
            "instrument main output is not stereo");
    expect (processor.acceptsMidi(), "instrument does not accept MIDI");
    expect (! processor.producesMidi(), "instrument unexpectedly produces MIDI");
    expect (! processor.isMidiEffect(), "instrument identifies as a MIDI effect");
    expect (processor.hasEditor(), "instrument does not advertise its editor");
    expect (std::abs (processor.getTailLengthSeconds()
                         - MarsAudioProcessor::maximumTailLengthSeconds) < 1.0e-9,
            "processor tail report drifted from the public Mars tail contract");

    const auto stereoLayout = processor.getBusesLayout();
    expect (processor.isBusesLayoutSupported (stereoLayout),
            "default no-input/stereo-output layout is unsupported");

    auto monoLayout = stereoLayout;
    monoLayout.outputBuses.set (0, juce::AudioChannelSet::mono());
    expect (! processor.isBusesLayoutSupported (monoLayout),
            "unsupported mono instrument output was accepted");

    auto inputLayout = stereoLayout;
    inputLayout.inputBuses.add (juce::AudioChannelSet::stereo());
    expect (! processor.isBusesLayoutSupported (inputLayout),
            "unsupported instrument input bus was accepted");
}

void testMaximumReleaseFitsReportedTail()
{
    MarsAudioProcessor processor;
    setParameterValue (processor, mars::parameters::filterModel, 1.0f);
    setParameterValue (processor, mars::parameters::cutoff, 20000.0f);
    setParameterValue (processor, mars::parameters::resonance, 0.0f);
    setParameterValue (processor, mars::parameters::filterEnvAmount, 0.0f);
    setParameterValue (processor, mars::parameters::aAttack, 0.001f);
    setParameterValue (processor, mars::parameters::aDecay, 0.010f);
    setParameterValue (processor, mars::parameters::aSustain, 1.0f);
    setParameterValue (processor, mars::parameters::aRelease, 12.0f);
    setParameterValue (processor, mars::parameters::drift, 1.0f);
    setParameterValue (processor, mars::parameters::chorusMix, 1.0f);
    setParameterValue (processor, mars::parameters::output, 6.0f);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    for (int note = 48; note < 64; ++note)
        midi.addEvent (juce::MidiMessage::noteOn (
            1, note, static_cast<juce::uint8> (127)), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() == 16,
            "maximum-tail regression did not exercise every voice card");

    midi.clear();
    for (int note = 48; note < 64; ++note)
        midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
    renderBlock (processor, audio, midi);

    constexpr double formerlyReportedTailSeconds = 14.0;
    const auto oldTailSamples = static_cast<int> (
        std::lround (formerlyReportedTailSeconds * sampleRate));
    const auto reportedTailSamples = static_cast<int> (std::lround (
        MarsAudioProcessor::maximumTailLengthSeconds * sampleRate));
    // The note-off event was at offset zero, so its block already advanced the
    // release by blockSize samples. Render only the remainder to land exactly
    // on each advertised boundary rather than one block beyond it.
    const auto peakAtOldTail = renderEmptySamples (
        processor, audio, oldTailSamples - blockSize);
    expect (processor.getActiveVoiceCount() > 0,
            "maximum amp release no longer demonstrates the former 14-second under-report");
    expect (peakAtOldTail > 1.0e-7f,
            "maximum amp release produced no measurable signal at the former host tail");

    const auto peakAtReportedTail = renderEmptySamples (
        processor, audio, reportedTailSamples - oldTailSamples);
    expect (processor.getActiveVoiceCount() == 0,
            "maximum amp release outlasted the reported Mars host tail");
    expect (peakAtReportedTail < 1.0e-7f,
            "Mars still produced measurable output at its reported host tail boundary");

    processor.releaseResources();
}

void testSampleAccurateNoteOn()
{
    MarsAudioProcessor processor;
    useShortReleases (processor);
    processor.prepareToPlay (sampleRate, blockSize);

    constexpr int eventSample = 173;
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (112)),
                   eventSample);
    renderBlock (processor, audio, midi);

    expect (peakInRange (audio, 0, eventSample) == 0.0f,
            "MIDI note rendered before its sample offset");
    expect (peakInRange (audio, eventSample, blockSize) > 1.0e-7f,
            "MIDI note produced no audio after its sample offset");
    expect (processor.getActiveVoiceCount() > 0,
            "sample-accurate note-on did not create an active voice");

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    renderBlock (processor, audio, midi);
    expect (renderUntilVoicesStop (processor, audio),
            "sample-accurate MIDI voice did not finish after note-off");
    processor.releaseResources();
}

void testMidiControllersAndVoiceLifecycle()
{
    MarsAudioProcessor processor;
    useShortReleases (processor);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (118)), 0);
    midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 64);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 96);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 128);
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 160);
    renderBlock (processor, audio, midi);

    expect (peakInRange (audio, 0, blockSize) > 1.0e-7f,
            "pitch-wheel/CC1/CC64 sequence produced no audio");
    expect (processor.getActiveVoiceCount() > 0,
            "sustain pedal did not retain the released voice");

    midi.clear();
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() > 0,
            "sustained voice ended before pedal release");

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    midi.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 32);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 0), 64);
    renderBlock (processor, audio, midi);
    expect (renderUntilVoicesStop (processor, audio),
            "voice did not finish after sustain-pedal release");

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 67, static_cast<juce::uint8> (110)), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 64);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() > 0,
            "all-notes-off setup did not create a voice");

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), 128);
    renderBlock (processor, audio, midi);
    expect (peakInRange (audio, 0, 128) > 1.0e-7f,
            "all-notes-off block did not render the leading voice");
    expect (processor.getActiveVoiceCount() > 0,
            "MIDI CC123 ignored the active sustain pedal");

    midi.clear();
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() > 0,
            "CC123-held voice ended before sustain-pedal release");

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    renderBlock (processor, audio, midi);
    expect (renderUntilVoicesStop (processor, audio),
            "MIDI CC123 voices did not finish after sustain-pedal release");

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 69, static_cast<juce::uint8> (110)), 0);
    renderBlock (processor, audio, midi);
    expect (processor.getActiveVoiceCount() > 0,
            "all-sound-off setup did not create a voice");

    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 120, 0), 128);
    renderBlock (processor, audio, midi);
    expect (peakInRange (audio, 0, 128) > 1.0e-7f,
            "all-sound-off block did not render the leading voice");
    expect (processor.getActiveVoiceCount() == 0,
            "MIDI CC120 did not silence all active voices immediately");

    processor.releaseResources();
}

void testPolyphonyLimitAndMonoMapping()
{
    MarsAudioProcessor limited;
    useShortReleases (limited);
    setParameterValue (limited, mars::parameters::polyphonyLimit, 4.0f);
    limited.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    for (int note = 48; note < 60; ++note)
        midi.addEvent (juce::MidiMessage::noteOn (
            1, note, static_cast<juce::uint8> (100)), 0);
    renderBlock (limited, audio, midi);
    expect (limited.getActiveVoiceCount() == 4,
            "processor did not enforce the configured four-voice limit");

    setParameterValue (limited, mars::parameters::polyphonyLimit, 2.0f);
    midi.clear();
    renderBlock (limited, audio, midi);
    expect (limited.getActiveVoiceCount() <= 2,
            "lowering polyphony did not immediately restore the CPU-safety limit");
    limited.releaseResources();

    MarsAudioProcessor unisonLimited;
    setParameterValue (unisonLimited, mars::parameters::voiceMode, 1.0f);
    setParameterValue (unisonLimited, mars::parameters::unisonVoices, 8.0f);
    setParameterValue (unisonLimited, mars::parameters::polyphonyLimit, 3.0f);
    unisonLimited.prepareToPlay (sampleRate, blockSize);
    midi.addEvent (juce::MidiMessage::noteOn (
        1, 48, static_cast<juce::uint8> (110)), 0);
    renderBlock (unisonLimited, audio, midi);
    expect (unisonLimited.getActiveVoiceCount() > 0
                && unisonLimited.getActiveVoiceCount() <= 3,
            "unison layers escaped the configured physical-voice budget");
    unisonLimited.releaseResources();

    MarsAudioProcessor mono;
    useShortReleases (mono);
    // Mono is an independent switch so existing three-choice automation keeps
    // its exact meaning. It must override even the most expensive voice mode.
    setParameterValue (mono, mars::parameters::voiceMode, 1.0f);
    setParameterValue (mono, mars::parameters::unisonVoices, 8.0f);
    setParameterValue (mono, mars::parameters::monoMode, 1.0f);
    mono.prepareToPlay (sampleRate, blockSize);

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (
        1, 60, static_cast<juce::uint8> (110)), 0);
    renderBlock (mono, audio, midi);
    expect (mono.getActiveVoiceCount() == 1,
            "mono switch did not override unison to one physical voice");

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (
        1, 67, static_cast<juce::uint8> (115)), 0);
    renderBlock (mono, audio, midi);
    expect (mono.getActiveVoiceCount() == 1,
            "legato mono note allocated a second voice");

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
    renderBlock (mono, audio, midi);
    expect (mono.getActiveVoiceCount() == 1
                && peakInRange (audio, 0, blockSize) > 1.0e-7f,
            "mono last-note release did not fall back to the held note");

    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    renderBlock (mono, audio, midi);
    expect (renderUntilVoicesStop (mono, audio),
            "mono voice did not finish after its final physical key release");
    mono.releaseResources();
}

void testOutputGainImpact()
{
    MarsAudioProcessor quiet;
    MarsAudioProcessor loud;
    setParameterValue (quiet, mars::parameters::noiseLevel, 0.0f);
    setParameterValue (loud, mars::parameters::noiseLevel, 0.0f);
    setParameterValue (quiet, mars::parameters::drift, 0.0f);
    setParameterValue (loud, mars::parameters::drift, 0.0f);
    setParameterValue (quiet, mars::parameters::chorusMix, 0.0f);
    setParameterValue (loud, mars::parameters::chorusMix, 0.0f);
    setParameterValue (quiet, mars::parameters::output, -24.0f);
    setParameterValue (loud, mars::parameters::output, 6.0f);
    quiet.prepareToPlay (sampleRate, blockSize);
    loud.prepareToPlay (sampleRate, blockSize);

    const auto note = juce::MidiMessage::noteOn (
        1, 60, static_cast<juce::uint8> (112));
    juce::AudioBuffer<float> quietAudio;
    juce::AudioBuffer<float> loudAudio;
    juce::MidiBuffer quietMidi;
    juce::MidiBuffer loudMidi;
    quietMidi.addEvent (note, 0);
    loudMidi.addEvent (note, 0);
    renderBlock (quiet, quietAudio, quietMidi);
    renderBlock (loud, loudAudio, loudMidi);

    const auto quietPeak = peakInRange (quietAudio, 0, blockSize);
    const auto loudPeak = peakInRange (loudAudio, 0, blockSize);
    expect (quietPeak > 0.0f, "quiet output-gain render produced no signal");
    expect (loudPeak > quietPeak * 10.0f,
            "output level did not materially affect the first rendered block");

    quiet.releaseResources();
    loud.releaseResources();
}

void testPrepareReleaseUiQueueAndPanic()
{
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    MarsAudioProcessor offline;
    offline.keyboardState.noteOn (1, 48, 0.9f);
    renderBlock (offline, audio, midi);
    expect (peakInRange (audio, 0, blockSize) == 0.0f,
            "unprepared processor emitted audio");
    expect (offline.getActiveVoiceCount() == 0,
            "unprepared processor accepted a UI note");

    offline.prepareToPlay (sampleRate, blockSize);
    expect (offline.isEngineReady(), "processor did not become ready after prepare");
    expect (std::abs (offline.getCurrentSampleRateForDisplay() - sampleRate) < 0.01,
            "prepare did not update the display sample rate");
    midi.clear();
    renderBlock (offline, audio, midi);
    expect (offline.getActiveVoiceCount() == 0,
            "UI note queued while offline leaked into the next prepare");
    expect (peakInRange (audio, 0, blockSize) == 0.0f,
            "offline UI queue emitted audio after prepare");
    offline.releaseResources();
    expect (! offline.isEngineReady(), "processor remained ready after releaseResources");
    expect (offline.getCurrentSampleRateForDisplay() == 0.0,
            "releaseResources did not clear the display sample rate");

    MarsAudioProcessor active;
    useShortReleases (active);
    active.prepareToPlay (sampleRate, blockSize);
    active.keyboardState.noteOn (1, 64, 0.9f);
    midi.clear();
    renderBlock (active, audio, midi);
    expect (active.getActiveVoiceCount() > 0,
            "prepared UI note was not dispatched at the next block boundary");
    expect (peakInRange (audio, 0, blockSize) > 1.0e-7f,
            "prepared UI note produced no audio");

    active.keyboardState.noteOn (1, 67, 0.9f);
    active.requestPanic();
    midi.clear();
    renderBlock (active, audio, midi);
    expect (active.getActiveVoiceCount() == 0,
            "panic did not stop voices and discard the stale UI queue");

    active.releaseResources();
    expect (! active.isEngineReady(), "UI processor remained ready after release");
    active.keyboardState.noteOn (1, 72, 1.0f);
    midi.clear();
    renderBlock (active, audio, midi);
    expect (active.getActiveVoiceCount() == 0,
            "UI note was accepted after releaseResources");
    expect (peakInRange (audio, 0, blockSize) == 0.0f,
            "released processor emitted audio");

    active.prepareToPlay (sampleRate, blockSize);
    midi.clear();
    renderBlock (active, audio, midi);
    expect (active.getActiveVoiceCount() == 0,
            "post-release UI queue leaked into a later prepare");
    active.releaseResources();
}

struct SnapshotStats
{
    std::size_t sampledColours = 0;
    int sampledPixels = 0;
    int opaquePixels = 0;
};

void testArpeggiatorThroughMidi()
{
    MarsAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    setParameterValue (processor, mars::parameters::arpEnabled, 1.0f);
    setParameterValue (processor, mars::parameters::arpRate, 20.0f);
    setParameterValue (processor, mars::parameters::arpGate, 0.5f);
    setParameterValue (processor, mars::parameters::arpMode, 0.0f);
    setParameterValue (processor, mars::parameters::arpOctaves, 1.0f);
    useShortReleases (processor);

    juce::AudioBuffer<float> audio (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, static_cast<juce::uint8> (100)), 8);
    midi.addEvent (juce::MidiMessage::noteOn (1, 67, static_cast<juce::uint8> (100)), 16);
    renderBlock (processor, audio, midi);
    midi.clear();

    expect (processor.getArpeggiatorKeyCountForDisplay() == 3,
            "the arpeggiator did not latch three held keys");

    std::set<int> playedNotes;
    float peak = 0.0f;
    for (int block = 0; block < 96; ++block)
    {
        renderBlock (processor, audio, midi);
        const auto note = processor.getArpeggiatorNoteForDisplay();
        if (note >= 0)
            playedNotes.insert (note);
        peak = std::max (peak, peakInRange (audio, 0, audio.getNumSamples()));
        const auto phase = processor.getArpeggiatorPhaseForDisplay();
        expect (phase >= 0.0f && phase <= 1.0f,
                "the arpeggiator reported a phase outside 0..1");
    }
    expect (playedNotes == std::set<int> ({ 60, 64, 67 }),
            "the arpeggiator did not play every held key");
    expect (peak > 0.001f, "the arpeggiator produced no audio");

    // The scope trace must follow the rendered output and stay finite.
    std::array<float, 1024> trace {};
    const auto copied = processor.copyScopeTrace (trace.data(),
                                                  static_cast<int> (trace.size()));
    expect (copied == static_cast<int> (trace.size()),
            "the scope trace did not return the requested sample count");
    bool traceFinite = true;
    float tracePeak = 0.0f;
    for (const auto value : trace)
    {
        traceFinite = traceFinite && std::isfinite (value);
        tracePeak = std::max (tracePeak, std::abs (value));
    }
    expect (traceFinite, "the scope trace contained a non-finite sample");
    expect (tracePeak > 0.0f, "the scope trace stayed silent while the engine played");
    expect (processor.copyScopeTrace (nullptr, 512) == 0
                && processor.copyScopeTrace (trace.data(), 0) == 0,
            "the scope trace did not reject a degenerate request");

    // Releasing every key stops the pattern; the plug-in returns to silence.
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 64), 1);
    midi.addEvent (juce::MidiMessage::noteOff (1, 67), 2);
    renderBlock (processor, audio, midi);
    midi.clear();
    expect (renderUntilVoicesStop (processor, audio),
            "the arpeggiator kept voices alive after every key was released");
    expect (processor.getArpeggiatorKeyCountForDisplay() == 0,
            "the arpeggiator kept its key list after every key was released");

    // Turning it off hands note handling straight back to the keyboard.
    setParameterValue (processor, mars::parameters::arpEnabled, 0.0f);
    midi.addEvent (juce::MidiMessage::noteOn (1, 55, static_cast<juce::uint8> (100)), 0);
    renderBlock (processor, audio, midi);
    midi.clear();
    expect (processor.getActiveVoiceCount() > 0,
            "the keyboard did not resume direct note handling after the arpeggiator");

    processor.releaseResources();
    expect (processor.getArpeggiatorNoteForDisplay() == -1,
            "releasing resources left an arpeggiator note on the display");
}

SnapshotStats inspectSnapshot (const juce::Image& snapshot)
{
    std::set<juce::uint32> colours;
    int sampledPixels = 0;
    int opaquePixels = 0;

    for (int y = 2; y < snapshot.getHeight(); y += 4)
    {
        for (int x = 2; x < snapshot.getWidth(); x += 4)
        {
            const auto pixel = snapshot.getPixelAt (x, y);
            colours.insert (pixel.getARGB());
            opaquePixels += pixel.getAlpha() == 255 ? 1 : 0;
            ++sampledPixels;
        }
    }

    return { colours.size(), sampledPixels, opaquePixels };
}

juce::Image renderEditorSnapshot (juce::AudioProcessorEditor& editor)
{
    juce::Image snapshot (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);
    juce::Graphics graphics (snapshot);
    editor.paintEntireComponent (graphics, true);
    return snapshot;
}

void expectDetailedOpaqueSnapshot (const juce::Image& snapshot, const std::string& sizeName)
{
    expect (snapshot.isValid(), sizeName + " editor snapshot is invalid");
    if (! snapshot.isValid())
        return;

    const auto stats = inspectSnapshot (snapshot);
    expect (stats.sampledPixels > 0 && stats.opaquePixels == stats.sampledPixels,
            sizeName + " editor snapshot contains transparent sampled pixels");
    expect (stats.sampledColours > 512u,
            sizeName + " editor snapshot lacks visual detail ("
                + std::to_string (stats.sampledColours) + " sampled colours)");
}

void testEditorRenderingAtDefaultAndMinimumSize()
{
    MarsAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "processor did not create an editor");
    if (editor == nullptr)
        return;

    expect (editor->isOpaque(), "editor does not advertise an opaque surface");
    expect (editor->getWidth() == 1400 && editor->getHeight() == 900,
            "editor did not open at its 1400x900 default size");
    const auto defaultSnapshot = renderEditorSnapshot (*editor);
    expectDetailedOpaqueSnapshot (defaultSnapshot, "default-size");

    editor->setSize (1240, 820);
    editor->resized();
    expect (editor->getWidth() == 1240 && editor->getHeight() == 820,
            "editor did not honour its 1240x820 minimum size");
    const auto minimumSnapshot = renderEditorSnapshot (*editor);
    expectDetailedOpaqueSnapshot (minimumSnapshot, "minimum-size");

    const auto snapshotPath = juce::SystemStats::getEnvironmentVariable (
        "MARS_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty())
    {
        juce::FileOutputStream output { juce::File (snapshotPath) };
        juce::PNGImageFormat png;
        const bool preparedOutput = output.openedOk()
            && output.setPosition (0)
            && output.truncate();
        const bool wroteSnapshot = preparedOutput
            && png.writeImageToStream (minimumSnapshot, output);
        output.flush();
        expect (wroteSnapshot, "could not write requested editor snapshot");
    }

    editor.reset();
    processor.releaseResources();
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInitialiser;
    testParameterLayoutAndDefaults();
    testParameterTextFormatting();
    testStateRoundTrip();
    testLegacyStateDefaultsNewParameters();
    testOversamplingConfigurationAndDeferredSwitch();
    testBusAndPluginContract();
    testMaximumReleaseFitsReportedTail();
    testSampleAccurateNoteOn();
    testMidiControllersAndVoiceLifecycle();
    testPolyphonyLimitAndMonoMapping();
    testPresetRandomizerRangeAndSafety();
    testOutputGainImpact();
    testPrepareReleaseUiQueueAndPanic();
    testArpeggiatorThroughMidi();
    testEditorRenderingAtDefaultAndMinimumSize();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Mars processor contract test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Mars processor contract tests passed\n";
    return EXIT_SUCCESS;
}
