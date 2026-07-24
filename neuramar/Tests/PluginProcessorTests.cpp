#include "PluginProcessor.h"

#include <JuceHeader.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
int failures = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool approximatelyEqual (float actual, float expected, float tolerance = 1.0e-5f)
{
    return std::abs (actual - expected) <= tolerance;
}

float parameterValue (const NeuramarAudioProcessor& processor, const char* id)
{
    const auto* value = processor.parameters.getRawParameterValue (id);
    expect (value != nullptr, std::string ("missing parameter ") + id);
    return value != nullptr ? value->load (std::memory_order_relaxed) : 0.0f;
}

void setParameterValue (NeuramarAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, std::string ("cannot set missing parameter ") + id);
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

bool corruptSerializedModel (juce::MemoryBlock& state)
{
    juce::MemoryInputStream stream (state.getData(), state.getSize(), false);
    constexpr auto expectedMagic = static_cast<std::uint32_t> (0x5453524eu);
    if (static_cast<std::uint32_t> (stream.readInt()) != expectedMagic
        || stream.readInt() != 1)
        return false;

    const auto xmlLength = stream.readInt();
    const auto xmlEnd = stream.getPosition() + static_cast<juce::int64> (xmlLength);
    if (xmlLength <= 0 || xmlEnd > stream.getTotalLength()
        || ! stream.setPosition (xmlEnd))
        return false;

    const auto modelLength = stream.readInt();
    const auto modelOffset = stream.getPosition();
    if (modelLength <= 0
        || static_cast<juce::int64> (modelLength) > stream.getNumBytesRemaining())
        return false;

    // The neural payload owns its checksum. Flipping its final checksum byte
    // keeps the outer processor-state framing valid while guaranteeing that
    // NeuralModel::deserialize rejects the embedded memory.
    auto* bytes = static_cast<std::uint8_t*> (state.getData());
    bytes[static_cast<std::size_t> (modelOffset + modelLength - 1)] ^= 0x80u;
    return true;
}

juce::MemoryBlock removeParameterFromSerializedState (
    const juce::MemoryBlock& state, const char* parameterId)
{
    juce::MemoryInputStream input (state.getData(), state.getSize(), false);
    constexpr auto expectedMagic = static_cast<std::uint32_t> (0x5453524eu);
    const auto magic = static_cast<std::uint32_t> (input.readInt());
    const auto version = input.readInt();
    const auto xmlLength = input.readInt();
    if (magic != expectedMagic || version != 1 || xmlLength <= 0
        || static_cast<juce::int64> (xmlLength) > input.getNumBytesRemaining())
        return {};

    std::vector<char> xmlBytes (static_cast<std::size_t> (xmlLength));
    if (input.read (xmlBytes.data(), xmlLength) != xmlLength)
        return {};
    const auto xml = juce::XmlDocument::parse (
        juce::String::fromUTF8 (xmlBytes.data(), xmlLength));
    if (xml == nullptr)
        return {};
    auto parameterState = juce::ValueTree::fromXml (*xml);
    for (int childIndex = parameterState.getNumChildren() - 1;
         childIndex >= 0; --childIndex)
    {
        const auto child = parameterState.getChild (childIndex);
        if (child.hasType ("PARAM")
            && child.getProperty ("id").toString() == parameterId)
            parameterState.removeChild (childIndex, nullptr);
    }
    const auto migratedXml = parameterState.createXml();
    if (migratedXml == nullptr)
        return {};
    const auto migratedText = migratedXml->toString();
    const auto* migratedUtf8 = migratedText.toRawUTF8();
    const auto migratedLength = static_cast<int> (std::strlen (migratedUtf8));

    juce::MemoryBlock result;
    juce::MemoryOutputStream output (result, false);
    output.writeInt (static_cast<int> (magic));
    output.writeInt (version);
    output.writeInt (migratedLength);
    output.write (migratedUtf8, static_cast<std::size_t> (migratedLength));
    const auto remainderOffset = static_cast<std::size_t> (input.getPosition());
    output.write (
        static_cast<const std::uint8_t*> (state.getData()) + remainderOffset,
        state.getSize() - remainderOffset);
    return result;
}

std::vector<float> makeLearningTone (double sampleRate, double seconds,
                                     double fundamentalHz)
{
    const auto count = static_cast<std::size_t> (sampleRate * seconds);
    std::vector<float> result (count, 0.0f);
    double phase = 0.0;
    std::uint32_t noise = 0x12345678u;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto time = static_cast<double> (i) / sampleRate;
        const auto attack = std::min (1.0, time / 0.018);
        const auto evolution = 0.72 + 0.28 * std::sin (time * 1.7);
        const auto envelope = attack * std::exp (-time * 0.28);
        phase += fundamentalHz / sampleRate;
        phase -= std::floor (phase);
        const auto angle = juce::MathConstants<double>::twoPi * phase;
        noise = noise * 1664525u + 1013904223u;
        const auto air = (static_cast<float> ((noise >> 9u) & 0x7fffffu)
                          / static_cast<float> (0x7fffffu) - 0.5f)
                       * static_cast<float> (std::exp (-time * 9.0));
        const auto tonal = 0.62 * std::sin (angle)
                         + 0.24 * evolution * std::sin (2.0 * angle + 0.3)
                         + 0.10 * std::sin (3.0 * angle - 0.5);
        result[i] = static_cast<float> (envelope * (tonal + 0.035 * air));
    }
    return result;
}

bool waitForLearning (NeuramarAudioProcessor& processor,
                      std::chrono::seconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto snapshot = processor.getLearningSnapshot();
        if (snapshot.stage == NeuramarAudioProcessor::LearningStage::Ready)
            return true;
        if (snapshot.stage == NeuramarAudioProcessor::LearningStage::Error
            || snapshot.stage == NeuramarAudioProcessor::LearningStage::Cancelled)
            return false;
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }
    return false;
}

struct AudioMetrics
{
    double energy = 0.0;
    float peak = 0.0f;
    bool finite = true;
};

AudioMetrics measure (const juce::AudioBuffer<float>& buffer)
{
    AudioMetrics result;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto value = buffer.getSample (channel, i);
            result.finite = result.finite && std::isfinite (value);
            if (std::isfinite (value))
            {
                result.energy += static_cast<double> (value) * value;
                result.peak = std::max (result.peak, std::abs (value));
            }
        }
    return result;
}

AudioMetrics renderNote (NeuramarAudioProcessor& processor, int midiNote,
                         int blockCount)
{
    juce::AudioBuffer<float> buffer (2, 128);
    AudioMetrics result;
    for (int block = 0; block < blockCount; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, 0.82f), 0);
        processor.processBlock (buffer, midi);
        const auto blockMetrics = measure (buffer);
        result.energy += blockMetrics.energy;
        result.peak = std::max (result.peak, blockMetrics.peak);
        result.finite = result.finite && blockMetrics.finite;
    }
    return result;
}

std::vector<float> renderInitialAttack (const juce::MemoryBlock& state,
                                        bool setAttackExplicitly,
                                        float attackSeconds)
{
    NeuramarAudioProcessor processor;
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    setParameterValue (processor, neuramar::parameters::mutation, 0.0f);
    if (setAttackExplicitly)
        setParameterValue (processor, neuramar::parameters::attack, attackSeconds);

    processor.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.82f), 0);
    processor.processBlock (buffer, midi);

    std::vector<float> result;
    result.reserve (static_cast<std::size_t> (buffer.getNumChannels()
                                              * buffer.getNumSamples()));
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        result.insert (result.end(), buffer.getReadPointer (channel),
                       buffer.getReadPointer (channel) + buffer.getNumSamples());
    processor.releaseResources();
    return result;
}

void silenceProcessor (NeuramarAudioProcessor& processor)
{
    processor.requestPanic();
    juce::AudioBuffer<float> buffer (2, 128);
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);
}

double squaredDifference (const std::vector<float>& first,
                          const std::vector<float>& second)
{
    expect (first.size() == second.size(), "attack renders have different lengths");
    const auto count = std::min (first.size(), second.size());
    double result = 0.0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto difference = static_cast<double> (first[index]) - second[index];
        result += difference * difference;
    }
    return result;
}

bool componentTreeContainsLabel (const juce::Component& component,
                                 const juce::String& text)
{
    if (const auto* label = dynamic_cast<const juce::Label*> (&component);
        label != nullptr && label->getText() == text)
        return true;

    for (int childIndex = 0; childIndex < component.getNumChildComponents();
         ++childIndex)
    {
        if (const auto* child = component.getChildComponent (childIndex);
            child != nullptr && componentTreeContainsLabel (*child, text))
            return true;
    }
    return false;
}

void testRandomizationWorkflow()
{
    using Amount = NeuramarAudioProcessor::RandomizationAmount;
    NeuramarAudioProcessor processor;
    expect (processor.getLearningSnapshot().stage
                == NeuramarAudioProcessor::LearningStage::Empty,
            "randomization fixture did not start empty");

    processor.prepareToPlay (48000.0, 128);
    processor.randomizeModel (Amount::Subtle1Percent);
    const auto subtle = processor.getLearningSnapshot();
    expect (subtle.stage == NeuramarAudioProcessor::LearningStage::Ready
                && subtle.generatedModel
                && subtle.rootMidiNote == 60
                && subtle.modelGeneration > 0,
            "1% randomization did not create a playable sample-free model");
    expect (subtle.sourceName.contains ("1%")
                && subtle.message.containsIgnoreCase ("no source sample"),
            "sample-free randomization did not explain its range and origin");

    {
        std::unique_ptr<juce::AudioProcessorEditor> generatedEditor (
            processor.createEditor());
        auto foundGeneratedRoot = false;
        if (generatedEditor != nullptr)
        {
            for (int childIndex = 0;
                 childIndex < generatedEditor->getNumChildComponents();
                 ++childIndex)
            {
                if (const auto* label = dynamic_cast<const juce::Label*> (
                        generatedEditor->getChildComponent (childIndex)))
                    foundGeneratedRoot = foundGeneratedRoot
                        || label->getText() == "GENERATED ROOT";
            }
        }
        expect (foundGeneratedRoot,
                "generated neural model was presented as an inferred sample root");
    }

    for (const int midiNote : { 24, 60, 96 })
    {
        const auto audio = renderNote (processor, midiNote, 60);
        expect (audio.finite && audio.energy > 1.0e-8 && audio.peak < 7.96f,
                "sample-free neural seed failed a wide-register render");
        silenceProcessor (processor);
    }

    juce::MemoryBlock subtleState;
    processor.getStateInformation (subtleState);

    juce::AudioBuffer<float> liveVariationBuffer (2, 128);
    juce::MidiBuffer liveVariationMidi;
    liveVariationMidi.addEvent (
        juce::MidiMessage::noteOn (1, 67, 0.82f), 0);
    processor.processBlock (liveVariationBuffer, liveVariationMidi);
    expect (processor.getActiveVoiceCount() > 0,
            "live randomization fixture did not start a sounding voice");

    processor.randomizeModel (Amount::Evolve10Percent);
    const auto evolved = processor.getLearningSnapshot();
    expect (evolved.stage == NeuramarAudioProcessor::LearningStage::Ready
                && evolved.generatedModel
                && evolved.modelGeneration > subtle.modelGeneration
                && evolved.rootMidiNote == subtle.rootMidiNote,
            "10% randomization did not vary the published neural model");
    expect (evolved.sourceName.contains ("variation 10%")
                && evolved.message.containsIgnoreCase ("weights varied"),
            "existing-model randomization did not report weight variation");
    liveVariationBuffer.clear();
    juce::MidiBuffer noLiveVariationMidi;
    processor.processBlock (liveVariationBuffer, noLiveVariationMidi);
    const auto liveVariationMetrics = measure (liveVariationBuffer);
    expect (liveVariationMetrics.finite && liveVariationMetrics.peak < 7.96f,
            "publishing a neural variation while sounding produced invalid audio");
    silenceProcessor (processor);

    juce::MemoryBlock evolvedState;
    processor.getStateInformation (evolvedState);
    expect (evolvedState != subtleState,
            "1% and 10% randomization serialized identical processor state");

    NeuramarAudioProcessor restored;
    restored.setStateInformation (
        evolvedState.getData(), static_cast<int> (evolvedState.getSize()));
    const auto restoredSnapshot = restored.getLearningSnapshot();
    expect (restoredSnapshot.stage
                == NeuramarAudioProcessor::LearningStage::Ready
                && restoredSnapshot.generatedModel
                && restoredSnapshot.rootMidiNote == evolved.rootMidiNote
                && restoredSnapshot.sourceName == evolved.sourceName,
            "sample-free neural seed did not retain its identity on state restore");
    restored.prepareToPlay (48000.0, 128);
    const auto restoredAudio = renderNote (restored, 72, 60);
    expect (restoredAudio.finite && restoredAudio.energy > 1.0e-8,
            "restored sample-free neural seed was not playable");
    silenceProcessor (restored);

    const auto restoredGeneration = restoredSnapshot.modelGeneration;
    restored.randomizeModel (Amount::Wild100Percent);
    const auto wild = restored.getLearningSnapshot();
    expect (wild.stage == NeuramarAudioProcessor::LearningStage::Ready
                && wild.generatedModel
                && wild.modelGeneration > restoredGeneration
                && wild.sourceName.contains ("variation 100%"),
            "100% randomization did not publish a wild neural variation");
    juce::MemoryBlock wildState;
    restored.getStateInformation (wildState);
    expect (wildState != evolvedState,
            "100% randomization did not change the serialized neural state");

    processor.learnSampleData (
        makeLearningTone (16000.0, 2.0, 329.627557),
        16000.0, "learner that randomize must cancel");
    processor.randomizeModel (Amount::Subtle1Percent);
    const auto afterLearningCancellation = processor.getLearningSnapshot();
    expect (afterLearningCancellation.stage
                == NeuramarAudioProcessor::LearningStage::Ready
                && afterLearningCancellation.generatedModel
                && ! afterLearningCancellation.sourceName.contains ("learner that"),
            "randomization did not replace an in-flight learner coherently");
    std::this_thread::sleep_for (std::chrono::milliseconds (40));
    const auto stableAfterCancellation = processor.getLearningSnapshot();
    expect (stableAfterCancellation.stage
                == NeuramarAudioProcessor::LearningStage::Ready
                && stableAfterCancellation.modelGeneration
                    == afterLearningCancellation.modelGeneration
                && stableAfterCancellation.sourceName
                    == afterLearningCancellation.sourceName,
            "cancelled learner published over a randomized neural memory");

    processor.releaseResources();
    restored.releaseResources();
}

void testContractsAndLearning()
{
    NeuramarAudioProcessor processor;
    expect (processor.getName() == "Neuramar", "processor name is not Neuramar");
    expect (processor.acceptsMidi() && ! processor.producesMidi(),
            "instrument MIDI contract is wrong");
    expect (! processor.isMidiEffect(), "instrument reports itself as a MIDI effect");
    expect (processor.getTailLengthSeconds() >= 8.0,
            "tail contract does not cover maximum release");
    expect (processor.getNumPrograms() == 1
                && processor.getProgramName (0).isNotEmpty(),
            "the VST3 factory program has no host-visible name");
    expect (processor.getParameters().size() == 17,
            "host parameter count does not include the appended Noise, "
            "Stretch, Formant, and Touch controls");
    constexpr std::array<const char*, 17> expectedParameterOrder {
        neuramar::parameters::imprint,
        neuramar::parameters::bodyLock,
        neuramar::parameters::air,
        neuramar::parameters::bone,
        neuramar::parameters::brightness,
        neuramar::parameters::evolutionRate,
        neuramar::parameters::orbit,
        neuramar::parameters::mutation,
        neuramar::parameters::attack,
        neuramar::parameters::release,
        neuramar::parameters::spread,
        neuramar::parameters::rootCorrection,
        neuramar::parameters::output,
        neuramar::parameters::noise,
        neuramar::parameters::stretch,
        neuramar::parameters::formant,
        neuramar::parameters::touch
    };
    for (std::size_t index = 0; index < expectedParameterOrder.size(); ++index)
    {
        expect (
            processor.getParameters()[static_cast<int> (index)]
                == processor.parameters.getParameter (
                    expectedParameterOrder[index]),
            "host parameter order changed at index "
                + std::to_string (index));
    }

    auto* noise = processor.parameters.getParameter (neuramar::parameters::noise);
    expect (noise != nullptr, "Noise parameter is missing");
    if (noise != nullptr)
    {
        const auto range = noise->getNormalisableRange();
        expect (approximatelyEqual (range.start, 0.0f)
                    && approximatelyEqual (range.end, 1.0f)
                    && approximatelyEqual (range.interval, 0.001f),
                "Noise no longer exposes the exact 0-100% contract");
        expect (approximatelyEqual (
                    noise->convertFrom0to1 (noise->getDefaultValue()), 0.0f)
                    && approximatelyEqual (
                        parameterValue (processor, neuramar::parameters::noise),
                        0.0f),
                "Noise no longer defaults to exact model recall");
    }

    auto* stretch = processor.parameters.getParameter (neuramar::parameters::stretch);
    expect (stretch != nullptr, "Stretch parameter is missing");
    if (stretch != nullptr)
    {
        const auto range = stretch->getNormalisableRange();
        expect (approximatelyEqual (range.start, 0.0f)
                    && approximatelyEqual (range.end, 2.0f)
                    && approximatelyEqual (range.interval, 0.001f),
                "Stretch no longer exposes the exact 0-200% contract");
        expect (approximatelyEqual (
                    stretch->convertFrom0to1 (stretch->getDefaultValue()), 1.0f)
                    && approximatelyEqual (
                        parameterValue (processor, neuramar::parameters::stretch),
                        1.0f),
                "Stretch no longer defaults to the fitted partial series");
    }

    auto* formant = processor.parameters.getParameter (neuramar::parameters::formant);
    expect (formant != nullptr, "Formant parameter is missing");
    if (formant != nullptr)
    {
        const auto range = formant->getNormalisableRange();
        expect (approximatelyEqual (range.start, -24.0f)
                    && approximatelyEqual (range.end, 24.0f)
                    && approximatelyEqual (range.interval, 0.01f),
                "Formant no longer exposes the exact +/-24 semitone contract");
        expect (approximatelyEqual (
                    formant->convertFrom0to1 (formant->getDefaultValue()), 0.0f)
                    && approximatelyEqual (
                        parameterValue (processor, neuramar::parameters::formant),
                        0.0f),
                "Formant no longer defaults to the learned body position");
    }

    auto* touch = processor.parameters.getParameter (neuramar::parameters::touch);
    expect (touch != nullptr, "Touch parameter is missing");
    if (touch != nullptr)
    {
        const auto range = touch->getNormalisableRange();
        expect (approximatelyEqual (range.start, 0.0f)
                    && approximatelyEqual (range.end, 1.0f)
                    && approximatelyEqual (range.interval, 0.001f),
                "Touch no longer exposes the exact 0-100% contract");
        expect (approximatelyEqual (
                    touch->convertFrom0to1 (touch->getDefaultValue()), 0.35f),
                "Touch no longer defaults to a moderate velocity response");
    }

    auto* awaken = processor.parameters.getParameter (neuramar::parameters::attack);
    expect (awaken != nullptr, "Awaken parameter is missing");
    if (awaken != nullptr)
    {
        const auto range = awaken->getNormalisableRange();
        expect (approximatelyEqual (range.start, 0.0f)
                    && approximatelyEqual (range.end, 2.0f)
                    && approximatelyEqual (range.interval, 0.0001f),
                "Awaken range no longer exposes an exact 0-2 second contract");
        expect (approximatelyEqual (
                    awaken->convertFrom0to1 (awaken->getDefaultValue()), 0.0f)
                    && approximatelyEqual (
                        parameterValue (processor, neuramar::parameters::attack), 0.0f),
                "Awaken no longer defaults to the learned attack without smoothing");
    }

    const auto stereo = processor.getBusesLayout();
    expect (processor.isBusesLayoutSupported (stereo),
            "stereo instrument layout was rejected");

    processor.prepareToPlay (48000.0, 128);
    juce::AudioBuffer<float> silent (2, 128);
    juce::MidiBuffer noteBeforeModel;
    noteBeforeModel.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    processor.processBlock (silent, noteBeforeModel);
    expect (measure (silent).energy == 0.0,
            "processor rendered audio before a model was learned");

    constexpr auto analysisRate = 16000.0;
    processor.learnSampleData (makeLearningTone (analysisRate, 0.85, 261.625565),
                               analysisRate, "procedural C4");
    expect (waitForLearning (processor, std::chrono::seconds (25)),
            "background sample learning did not complete successfully");

    const auto learned = processor.getLearningSnapshot();
    expect (learned.stage == NeuramarAudioProcessor::LearningStage::Ready,
            "completed learner did not publish a ready model");
    expect (std::abs (learned.rootMidiNote - 60) <= 1,
            "root detector did not identify procedural C4");
    expect (learned.rootConfidence > 0.20f,
            "root detector reported implausibly low confidence for a harmonic tone");
    expect (learned.modelGeneration > 0,
            "ready model did not receive a generation");
    expect (! learned.generatedModel,
            "sample-learned model was mislabeled as a generated neural seed");

    const auto rendered = renderNote (processor, 67, 80);
    expect (rendered.finite, "learned instrument rendered NaN or infinity");
    expect (rendered.energy > 1.0e-8, "learned instrument was silent");
    expect (rendered.peak < 8.0f, "learned instrument exceeded amplitude guardrail");
    expect (processor.getActiveVoiceCount() > 0,
            "engine did not report an active learned voice");

    juce::AudioBuffer<float> releaseBuffer (2, 128);
    juce::MidiBuffer releaseMidi;
    releaseMidi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
    processor.processBlock (releaseBuffer, releaseMidi);

    constexpr auto retainedImprint = 0.31f;
    constexpr auto rejectedImprint = 0.76f;
    constexpr auto retainedNoise = 0.43f;
    constexpr auto retainedStretch = 0.62f;
    constexpr auto retainedFormant = -7.25f;
    constexpr auto retainedTouch = 0.81f;
    setParameterValue (processor, neuramar::parameters::imprint, retainedImprint);
    setParameterValue (processor, neuramar::parameters::noise, retainedNoise);
    setParameterValue (processor, neuramar::parameters::stretch, retainedStretch);
    setParameterValue (processor, neuramar::parameters::formant, retainedFormant);
    setParameterValue (processor, neuramar::parameters::touch, retainedTouch);
    expect (approximatelyEqual (
                parameterValue (processor, neuramar::parameters::imprint), retainedImprint),
            "could not establish the retained parameter-state fixture");
    expect (approximatelyEqual (
                parameterValue (processor, neuramar::parameters::noise), retainedNoise),
            "could not establish the retained Noise parameter fixture");
    expect (approximatelyEqual (
                parameterValue (processor, neuramar::parameters::stretch),
                retainedStretch)
                && approximatelyEqual (
                    parameterValue (processor, neuramar::parameters::formant),
                    retainedFormant)
                && approximatelyEqual (
                    parameterValue (processor, neuramar::parameters::touch),
                    retainedTouch),
            "could not establish the retained Stretch/Formant/Touch fixture");

    const auto anatomySnapshot = processor.getModelAnatomySnapshot();
    expect (anatomySnapshot.modelGeneration == learned.modelGeneration
                && anatomySnapshot.anatomy.valid
                && anatomySnapshot.anatomy.peakAmplitude > 0.0f
                && approximatelyEqual (
                    anatomySnapshot.anatomy.rootFrequencyHz,
                    static_cast<float> (learned.rootFrequencyHz), 0.01f),
            "the editor's model anatomy was not published with the learned memory");

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    expect (state.getSize() > 512 && state.getSize() < 4 * 1024 * 1024,
            "state did not embed one compact, bounded neural model");

    const auto legacyState = removeParameterFromSerializedState (
        state, neuramar::parameters::noise);
    expect (! legacyState.isEmpty(),
            "could not construct the pre-Noise state migration fixture");
    NeuramarAudioProcessor legacyRestored;
    setParameterValue (
        legacyRestored, neuramar::parameters::noise, 0.91f);
    legacyRestored.setStateInformation (
        legacyState.getData(), static_cast<int> (legacyState.getSize()));
    expect (approximatelyEqual (
                parameterValue (
                    legacyRestored, neuramar::parameters::noise),
                0.0f)
                && legacyRestored.getLearningSnapshot().stage
                    == NeuramarAudioProcessor::LearningStage::Ready,
            "a pre-Noise session inherited the live Noise value instead of "
            "migrating to exact recall");

    struct MigrationCase
    {
        const char* id;
        float liveValue;
        float expectedDefault;
    };
    for (const auto& migration : std::array<MigrationCase, 3> {
             MigrationCase { neuramar::parameters::stretch, 0.05f, 1.0f },
             MigrationCase { neuramar::parameters::formant, 19.5f, 0.0f },
             MigrationCase { neuramar::parameters::touch, 0.97f, 0.35f } })
    {
        const auto olderState = removeParameterFromSerializedState (
            state, migration.id);
        expect (! olderState.isEmpty(),
                std::string ("could not construct the pre-")
                    + migration.id + " state migration fixture");
        NeuramarAudioProcessor olderRestored;
        setParameterValue (olderRestored, migration.id, migration.liveValue);
        olderRestored.setStateInformation (
            olderState.getData(), static_cast<int> (olderState.getSize()));
        expect (approximatelyEqual (
                    parameterValue (olderRestored, migration.id),
                    migration.expectedDefault)
                    && olderRestored.getLearningSnapshot().stage
                        == NeuramarAudioProcessor::LearningStage::Ready,
                std::string ("a session saved before ") + migration.id
                    + " inherited the live value instead of its default");
    }

    const auto defaultAttack = renderInitialAttack (state, false, 0.0f);
    const auto explicitZeroAttack = renderInitialAttack (state, true, 0.0f);
    const auto formerSmoothedAttack = renderInitialAttack (state, true, 0.012f);
    const auto defaultVsZero = squaredDifference (defaultAttack, explicitZeroAttack);
    const auto zeroVsFormer = squaredDifference (explicitZeroAttack, formerSmoothedAttack);
    expect (defaultVsZero <= 1.0e-12,
            "default render attack differs from an explicit zero-second attack");
    expect (zeroVsFormer > 1.0e-8,
            "default render still behaves like the former 12 ms smoothing");

    setParameterValue (processor, neuramar::parameters::imprint, rejectedImprint);
    juce::MemoryBlock stateWithRejectedParameters;
    processor.getStateInformation (stateWithRejectedParameters);
    expect (stateWithRejectedParameters != state,
            "corrupt-state fixture did not contain different parameters");
    setParameterValue (processor, neuramar::parameters::imprint, retainedImprint);

    auto corruptedModelState = stateWithRejectedParameters;
    expect (corruptSerializedModel (corruptedModelState),
            "could not locate the serialized neural payload to corrupt");
    const auto beforeCorruptedRestore = processor.getLearningSnapshot();
    processor.setStateInformation (
        corruptedModelState.getData(), static_cast<int> (corruptedModelState.getSize()));
    const auto afterCorruptedRestore = processor.getLearningSnapshot();
    expect (afterCorruptedRestore.stage == NeuramarAudioProcessor::LearningStage::Error
                && afterCorruptedRestore.message.containsIgnoreCase ("invalid"),
            "corrupted neural payload was not explicitly rejected");
    expect (approximatelyEqual (
                parameterValue (processor, neuramar::parameters::imprint), retainedImprint),
            "corrupted neural payload partially committed its parameter state");
    expect (afterCorruptedRestore.modelGeneration == beforeCorruptedRestore.modelGeneration
                && afterCorruptedRestore.rootMidiNote == beforeCorruptedRestore.rootMidiNote
                && afterCorruptedRestore.sourceName == beforeCorruptedRestore.sourceName
                && afterCorruptedRestore.waveform == beforeCorruptedRestore.waveform,
            "corrupted neural payload changed the prior published memory");

    juce::MemoryBlock stateAfterRejectedRestore;
    processor.getStateInformation (stateAfterRejectedRestore);
    expect (stateAfterRejectedRestore == state,
            "corrupted neural payload changed the processor's serializable state");

    processor.requestPanic();
    juce::AudioBuffer<float> panicBuffer (2, 128);
    juce::MidiBuffer noMidi;
    processor.processBlock (panicBuffer, noMidi);
    const auto retainedRender = renderNote (processor, 55, 50);
    expect (retainedRender.finite && retainedRender.energy > 1.0e-8,
            "corrupted neural payload made the prior model unplayable");
    processor.requestPanic();
    processor.processBlock (panicBuffer, noMidi);

    processor.learnSampleData (makeLearningTone (analysisRate, 2.0, 391.995436),
                               analysisRate, "pending G4");
    const auto pending = processor.getLearningSnapshot();
    expect (pending.sourceName == "pending G4"
                && pending.modelGeneration == learned.modelGeneration,
            "second learning fixture did not retain the prior published generation");
    juce::MemoryBlock stateDuringSecondLearning;
    processor.getStateInformation (stateDuringSecondLearning);
    const auto cancelStart = std::chrono::steady_clock::now();
    processor.requestLearningCancellation();
    const auto cancelRequestSeconds = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - cancelStart).count();
    expect (cancelRequestSeconds < 0.25,
            "UI cancellation request blocked while joining the learner");
    const auto cancelledSnapshot = processor.getLearningSnapshot();
    expect (cancelledSnapshot.stage
                == NeuramarAudioProcessor::LearningStage::Cancelled
                && cancelledSnapshot.modelGeneration == learned.modelGeneration
                && cancelledSnapshot.rootMidiNote == learned.rootMidiNote
                && cancelledSnapshot.sourceName == learned.sourceName
                && cancelledSnapshot.waveform == learned.waveform,
            "cancellation did not restore the prior published model metadata");
    // Lifecycle/state callers can still request a definite worker join after
    // the non-blocking UI operation has closed the publication attempt.
    processor.cancelLearning();
    expect (stateDuringSecondLearning == state,
            "state saved during learning mixed the pending source with the prior model");

    // A rejected restore is a terminal state operation: once it returns, an
    // in-flight learner must not be able to publish Ready over the error. This
    // also exercises the path where the worker has started but has not yet
    // reached a cancellation checkpoint.
    processor.learnSampleData (makeLearningTone (analysisRate, 2.0, 349.228231),
                               analysisRate, "racing F4");
    expect (processor.getLearningSnapshot().sourceName == "racing F4",
            "invalid-state race fixture did not start its learner");
    processor.setStateInformation (
        corruptedModelState.getData(), static_cast<int> (corruptedModelState.getSize()));
    const auto rejectedWhileLearning = processor.getLearningSnapshot();
    expect (rejectedWhileLearning.stage
                == NeuramarAudioProcessor::LearningStage::Error
                && rejectedWhileLearning.message.containsIgnoreCase ("invalid"),
            "active learner overwrote the rejected-state error");
    expect (rejectedWhileLearning.modelGeneration == learned.modelGeneration
                && rejectedWhileLearning.rootMidiNote == learned.rootMidiNote
                && rejectedWhileLearning.sourceName == learned.sourceName
                && rejectedWhileLearning.waveform == learned.waveform,
            "rejected state during learning changed the published memory");
    expect (approximatelyEqual (
                parameterValue (processor, neuramar::parameters::imprint), retainedImprint),
            "rejected state during learning partially committed parameters");
    std::this_thread::sleep_for (std::chrono::milliseconds (40));
    expect (processor.getLearningSnapshot().stage
                == NeuramarAudioProcessor::LearningStage::Error,
            "learner published after rejected-state restoration returned");

    NeuramarAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    const auto restoredSnapshot = restored.getLearningSnapshot();
    expect (restoredSnapshot.stage == NeuramarAudioProcessor::LearningStage::Ready,
            "state round trip did not restore a playable model");
    expect (restoredSnapshot.rootMidiNote == learned.rootMidiNote,
            "state round trip changed detected root");
    expect (restoredSnapshot.sourceName == learned.sourceName
                && restoredSnapshot.waveform == learned.waveform,
            "state round trip broke saved source/model coherence");
    expect (approximatelyEqual (
                parameterValue (restored, neuramar::parameters::imprint), retainedImprint),
            "state round trip changed the saved parameter fixture");
    expect (approximatelyEqual (
                parameterValue (restored, neuramar::parameters::noise), retainedNoise),
            "state round trip changed the saved Noise parameter");
    expect (approximatelyEqual (
                parameterValue (restored, neuramar::parameters::stretch),
                retainedStretch)
                && approximatelyEqual (
                    parameterValue (restored, neuramar::parameters::formant),
                    retainedFormant)
                && approximatelyEqual (
                    parameterValue (restored, neuramar::parameters::touch),
                    retainedTouch),
            "state round trip changed the saved Stretch/Formant/Touch values");
    expect (restored.getModelAnatomySnapshot().anatomy.valid,
            "a restored memory did not publish an anatomy for the editor");
    restored.prepareToPlay (48000.0, 128);
    const auto restoredRender = renderNote (restored, 55, 50);
    expect (restoredRender.finite && restoredRender.energy > 1.0e-8,
            "restored neural memory did not render valid audio");

    const std::array<std::uint8_t, 13> malformed {
        0x4e, 0x52, 0x53, 0x54, 1, 0, 0, 0, 0xff, 0xff, 0xff, 0x7f, 0
    };
    restored.setStateInformation (malformed.data(), static_cast<int> (malformed.size()));
    expect (restored.getLearningSnapshot().stage
                == NeuramarAudioProcessor::LearningStage::Ready,
            "malformed state destroyed the current learned memory");

    NeuramarAudioProcessor emptyStateProcessor;
    juce::MemoryBlock emptyState;
    emptyStateProcessor.getStateInformation (emptyState);
    restored.setStateInformation (emptyState.getData(),
                                  static_cast<int> (emptyState.getSize()));
    expect (restored.getLearningSnapshot().stage
                == NeuramarAudioProcessor::LearningStage::Empty,
            "a valid empty state did not clear the learned memory");
    juce::AudioBuffer<float> clearedAudio (2, 128);
    juce::MidiBuffer clearedMidi;
    clearedMidi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    restored.processBlock (clearedAudio, clearedMidi);
    expect (measure (clearedAudio).energy == 0.0,
            "processor rendered the previous model after restoring an empty state");
    expect (! restored.getModelAnatomySnapshot().anatomy.valid,
            "the model anatomy survived a cleared memory");

    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (processor.getLearningSnapshot().stage
                == NeuramarAudioProcessor::LearningStage::Ready,
            "could not restore a ready model for the editor render check");

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "processor did not create its custom editor");
    if (editor != nullptr)
    {
        expect (editor->getWidth() == 1180 && editor->getHeight() == 880,
                "editor default size changed unexpectedly");

        std::set<std::string> buttonTexts;
        auto defaultTenPercentSelected = false;
        for (int childIndex = 0; childIndex < editor->getNumChildComponents();
             ++childIndex)
        {
            if (const auto* button = dynamic_cast<const juce::TextButton*> (
                    editor->getChildComponent (childIndex)))
            {
                buttonTexts.insert (button->getButtonText().toStdString());
                if (button->getButtonText() == "10%")
                    defaultTenPercentSelected = button->getToggleState();
            }
        }
        for (const auto* text : { "RANDOMIZE", "1%", "10%", "100%" })
            expect (buttonTexts.contains (text),
                    std::string ("editor is missing randomization control ") + text);
        expect (defaultTenPercentSelected,
                "editor did not default to the balanced 10% random range");
        expect (componentTreeContainsLabel (*editor, "NOISE"),
                "editor is missing the neural-input Noise control");
        for (const auto* knobTitle : { "STRETCH", "FORMANT", "TOUCH" })
            expect (componentTreeContainsLabel (*editor, knobTitle),
                    std::string ("editor is missing the ") + knobTitle
                        + " control");

        const auto renderAndCheckEditor = [&editor] (int width, int height,
                                                      const char* snapshotVariable)
        {
            editor->setSize (width, height);
            juce::Image snapshot (juce::Image::ARGB, width, height, true);
            juce::Graphics graphics (snapshot);
            editor->paintEntireComponent (graphics, true);

            std::set<juce::uint32> sampledColours;
            auto opaque = true;
            for (int y = 4; y < snapshot.getHeight(); y += 11)
                for (int x = 4; x < snapshot.getWidth(); x += 11)
                {
                    const auto pixel = snapshot.getPixelAt (x, y);
                    sampledColours.insert (pixel.getARGB());
                    opaque = opaque && pixel.getAlpha() == 255;
                }
            const auto size = std::to_string (width) + "x" + std::to_string (height);
            expect (opaque, "editor snapshot contains transparent holes at " + size);
            expect (sampledColours.size() > 80,
                    "editor snapshot lacks visual detail at " + size);

            for (int childIndex = 0; childIndex < editor->getNumChildComponents(); ++childIndex)
            {
                const auto* child = editor->getChildComponent (childIndex);
                if (child == nullptr || ! child->isVisible())
                    continue;
                expect (child->getWidth() > 0 && child->getHeight() > 0,
                        "visible editor child collapsed at " + size);
                expect (editor->getLocalBounds().contains (child->getBounds()),
                        "visible editor child escaped its bounds at " + size);
            }

            const auto snapshotPath = juce::SystemStats::getEnvironmentVariable (
                snapshotVariable, {});
            if (snapshotPath.isNotEmpty())
            {
                juce::FileOutputStream output { juce::File (snapshotPath) };
                juce::PNGImageFormat png;
                const auto prepared = output.openedOk()
                                   && output.setPosition (0)
                                   && output.truncate();
                const auto written = prepared && png.writeImageToStream (snapshot, output);
                output.flush();
                expect (written, "could not write requested " + size
                                     + " editor snapshot");
            }
        };

        renderAndCheckEditor (1180, 880, "NEURAMAR_EDITOR_SNAPSHOT");
        renderAndCheckEditor (960, 716, "NEURAMAR_EDITOR_SNAPSHOT_MIN");
        renderAndCheckEditor (1500, 1119, "NEURAMAR_EDITOR_SNAPSHOT_MAX");
        editor->setSize (1180, 880);
    }

    processor.randomizeModel (
        NeuramarAudioProcessor::RandomizationAmount::Subtle1Percent);
    const auto learnedVariation = processor.getLearningSnapshot();
    expect (learnedVariation.stage
                == NeuramarAudioProcessor::LearningStage::Ready
                && ! learnedVariation.generatedModel
                && learnedVariation.rootMidiNote == learned.rootMidiNote
                && learnedVariation.modelGeneration > learned.modelGeneration
                && learnedVariation.sourceName.contains ("variation 1%"),
            "randomizing a learned memory lost its inferred-root identity");

    processor.releaseResources();
    restored.releaseResources();
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    testRandomizationWorkflow();
    testContractsAndLearning();

    if (failures != 0)
    {
        std::cerr << failures << " Neuramar processor check(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Neuramar processor checks passed.\n";
    return EXIT_SUCCESS;
}
