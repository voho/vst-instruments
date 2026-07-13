#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
template <typename Enum>
Enum enumFromParameter (const std::atomic<float>* value, int maximum) noexcept
{
    return static_cast<Enum> (juce::jlimit (0, maximum,
                                           juce::roundToInt (value->load (std::memory_order_relaxed))));
}
} // namespace

MarsAudioProcessor::MarsAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "MARS_STATE", createParameterLayout())
{
    using namespace mars::parameters;

    parameterPointers.profile      = parameters.getRawParameterValue (profile);
    parameterPointers.mode         = parameters.getRawParameterValue (mode);
    parameterPointers.vowel        = parameters.getRawParameterValue (vowel);
    parameterPointers.chordQuality = parameters.getRawParameterValue (chordQuality);
    parameterPointers.choirSize    = parameters.getRawParameterValue (choirSize);
    parameterPointers.breath       = parameters.getRawParameterValue (breath);
    parameterPointers.resonance    = parameters.getRawParameterValue (resonance);
    parameterPointers.vibrato      = parameters.getRawParameterValue (vibrato);
    parameterPointers.humanize     = parameters.getRawParameterValue (humanize);
    parameterPointers.spread       = parameters.getRawParameterValue (spread);
    parameterPointers.tension      = parameters.getRawParameterValue (tension);
    parameterPointers.room         = parameters.getRawParameterValue (room);
    parameterPointers.output       = parameters.getRawParameterValue (output);

    jassert (parameterPointers.profile != nullptr && parameterPointers.output != nullptr);
    keyboardState.addListener (this);
}

MarsAudioProcessor::~MarsAudioProcessor()
{
    keyboardState.removeListener (this);
}

juce::AudioProcessorValueTreeState::ParameterLayout MarsAudioProcessor::createParameterLayout()
{
    using namespace mars::parameters;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;

    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { profile, 1 }, "Oscillator alloy",
        juce::StringArray { "Saturn", "Phobos" }, 0));
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { mode, 1 }, "Voice mode",
        juce::StringArray { "Single", "Stack", "Fifth" }, 0));
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { vowel, 1 }, "FilterType",
        juce::StringArray { "Ladder", "SVF", "Poles" }, 0));
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { chordQuality, 1 }, "Drift mode",
        juce::StringArray { "Free", "Locked" }, 0));
    result.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { choirSize, 1 }, "Stack depth", 2, 16, 8));

    const auto addPercent = [&result] (const char* id, const char* name, float defaultValue)
    {
        result.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, defaultValue,
            juce::AudioParameterFloatAttributes().withLabel ("%")
                                                   .withStringFromValueFunction ([] (float v, int)
                                                   {
                                                       return juce::String (juce::roundToInt (v * 100.0f));
                                                   })
                                                   .withValueFromStringFunction ([] (const juce::String& s)
                                                   {
                                                       return s.getFloatValue() / 100.0f;
                                                   })));
    };

    addPercent (breath, "Osc 2 blend", 0.30f);
    addPercent (resonance, "Filter resonance", 0.64f);
    addPercent (vibrato, "LFO vibrato", 0.38f);
    addPercent (humanize, "Component age", 0.52f);
    addPercent (spread, "Stereo width", 0.62f);
    addPercent (tension, "Circuit drive", 0.36f);
    addPercent (room, "Plate bloom", 0.24f);

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { output, 1 }, "Output",
        juce::NormalisableRange<float> { -24.0f, 6.0f, 0.1f }, -6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { result.begin(), result.end() };
}

void MarsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engineReady.store (false, std::memory_order_release);
    engine.prepare (sampleRate, samplesPerBlock);
    engine.reset();
    updateEngineParameters();
    displaySampleRate.store (sampleRate, std::memory_order_relaxed);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    engineReady.store (true, std::memory_order_release);
}

void MarsAudioProcessor::releaseResources()
{
    engineReady.store (false, std::memory_order_release);
    engine.allNotesOff();
    engine.reset();
    activeVoiceCount.store (0, std::memory_order_relaxed);
}

bool MarsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MarsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (! engineReady.load (std::memory_order_acquire))
        return;

    updateEngineParameters();

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
        engine.allNotesOff();

    // GUI notes enter through a bounded lock-free queue and start at the next
    // block boundary. This avoids allocating or locking in the audio callback.
    dispatchUiMidiEvents();

    const auto numSamples = buffer.getNumSamples();
    int renderedTo = 0;

    for (const auto metadata : midiMessages)
    {
        const auto eventSample = juce::jlimit (0, numSamples, metadata.samplePosition);

        if (eventSample > renderedTo)
        {
            engine.process (buffer.getWritePointer (0, renderedTo),
                            buffer.getWritePointer (1, renderedTo),
                            eventSample - renderedTo);
            renderedTo = eventSample;
        }

        dispatchMidiData (metadata.data, metadata.numBytes);
    }

    if (renderedTo < numSamples)
        engine.process (buffer.getWritePointer (0, renderedTo),
                        buffer.getWritePointer (1, renderedTo),
                        numSamples - renderedTo);

    activeVoiceCount.store (engine.getActiveVoiceCount(), std::memory_order_relaxed);
}

void MarsAudioProcessor::dispatchMidiData (const juce::uint8* data, int numBytes) noexcept
{
    if (data == nullptr || numBytes < 1)
        return;

    const auto status = static_cast<unsigned> (data[0]);
    const auto kind = status & 0xf0u;

    if (kind == 0x90u && numBytes >= 3)
    {
        if (data[2] != 0)
            engine.noteOn (static_cast<int> (data[1] & 0x7fu),
                           static_cast<float> (data[2] & 0x7fu) / 127.0f);
        else
            engine.noteOff (static_cast<int> (data[1] & 0x7fu));
    }
    else if (kind == 0x80u && numBytes >= 3)
    {
        engine.noteOff (static_cast<int> (data[1] & 0x7fu));
    }
    else if (kind == 0xb0u && numBytes >= 3)
    {
        const auto controller = data[1] & 0x7fu;
        if (controller == 120u || controller == 123u)
            engine.allNotesOff();
    }
}

void MarsAudioProcessor::updateEngineParameters() noexcept
{
    mars::EngineParameters next;
    next.profile = enumFromParameter<mars::VoiceProfile> (parameterPointers.profile, 1);
    next.mode = enumFromParameter<mars::PerformanceMode> (parameterPointers.mode, 2);
    next.vowel = enumFromParameter<mars::FilterType> (parameterPointers.vowel, 2);
    next.chordQuality = enumFromParameter<mars::FifthQuality> (parameterPointers.chordQuality, 1);
    next.choirSize = juce::jlimit (2, 16, juce::roundToInt (
        parameterPointers.choirSize->load (std::memory_order_relaxed)));
    next.breath = parameterPointers.breath->load (std::memory_order_relaxed);
    next.resonance = parameterPointers.resonance->load (std::memory_order_relaxed);
    next.vibrato = parameterPointers.vibrato->load (std::memory_order_relaxed);
    next.humanize = parameterPointers.humanize->load (std::memory_order_relaxed);
    next.spread = parameterPointers.spread->load (std::memory_order_relaxed);
    next.tension = parameterPointers.tension->load (std::memory_order_relaxed);
    next.room = parameterPointers.room->load (std::memory_order_relaxed);
    next.outputGain = juce::Decibels::decibelsToGain (
        parameterPointers.output->load (std::memory_order_relaxed));
    engine.setParameters (next);
}

void MarsAudioProcessor::handleNoteOn (juce::MidiKeyboardState*, int,
                                         int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, true);
}

void MarsAudioProcessor::handleNoteOff (juce::MidiKeyboardState*, int,
                                          int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, false);
}

void MarsAudioProcessor::enqueueUiMidiEvent (int note, float velocity, bool isNoteOn) noexcept
{
    const auto write = uiWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1u) % uiQueueCapacity;

    // Dropping an event is preferable to ever waiting on the message or audio thread.
    if (next == uiReadIndex.load (std::memory_order_acquire))
    {
        panicRequested.store (true, std::memory_order_release);
        return;
    }

    uiMidiQueue[write] = { juce::jlimit (0, 127, note), juce::jlimit (0.0f, 1.0f, velocity), isNoteOn };
    uiWriteIndex.store (next, std::memory_order_release);
}

void MarsAudioProcessor::dispatchUiMidiEvents() noexcept
{
    auto read = uiReadIndex.load (std::memory_order_relaxed);
    const auto write = uiWriteIndex.load (std::memory_order_acquire);

    while (read != write)
    {
        const auto event = uiMidiQueue[read];
        if (event.noteOn)
            engine.noteOn (event.note, event.velocity);
        else
            engine.noteOff (event.note);
        read = (read + 1u) % uiQueueCapacity;
    }

    uiReadIndex.store (read, std::memory_order_release);
}

void MarsAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destinationData);
}

void MarsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
        requestPanic();
    }
}

juce::AudioProcessorEditor* MarsAudioProcessor::createEditor()
{
    return new MarsAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MarsAudioProcessor();
}
