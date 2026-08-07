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

// APVTS keeps a parameter at its current value when the replacement tree has
// no child for it. A session saved before a control existed would therefore
// adopt whatever the previously loaded preset left behind rather than that
// control's default, so fill in every default the stored tree omits.
bool containsParameterState (const juce::ValueTree& state,
                             const juce::String& parameterId)
{
    static const juce::Identifier parameterType { "PARAM" };
    static const juce::Identifier idProperty { "id" };

    for (const auto& child : state)
        if (child.hasType (parameterType)
            && child.getProperty (idProperty).toString() == parameterId)
            return true;

    return false;
}

void addMissingParameterDefaults (
    juce::ValueTree& state, juce::AudioProcessorValueTreeState& parameters,
    const juce::Array<juce::AudioProcessorParameter*>& hostParameters)
{
    static const juce::Identifier parameterType { "PARAM" };
    static const juce::Identifier idProperty { "id" };
    static const juce::Identifier valueProperty { "value" };

    for (const auto* hostParameter : hostParameters)
    {
        const auto* ranged =
            dynamic_cast<const juce::RangedAudioParameter*> (hostParameter);
        if (ranged == nullptr
            || parameters.getParameter (ranged->paramID) == nullptr
            || containsParameterState (state, ranged->paramID))
            continue;

        juce::ValueTree parameterState { parameterType };
        parameterState.setProperty (idProperty, ranged->paramID, nullptr);
        parameterState.setProperty (
            valueProperty,
            ranged->convertFrom0to1 (ranged->getDefaultValue()), nullptr);
        state.appendChild (parameterState, nullptr);
    }
}
} // namespace

VocalorAudioProcessor::VocalorAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "VOCALOR_STATE", createParameterLayout())
{
    using namespace vocalor::parameters;

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
    parameterPointers.legato       = parameters.getRawParameterValue (legato);
    parameterPointers.vowelX       = parameters.getRawParameterValue (vowelX);
    parameterPointers.vowelY       = parameters.getRawParameterValue (vowelY);
    parameterPointers.vowelMorph   = parameters.getRawParameterValue (vowelMorph);
    parameterPointers.formantShift = parameters.getRawParameterValue (formantShift);
    parameterPointers.glide        = parameters.getRawParameterValue (glide);
    parameterPointers.roomSize     = parameters.getRawParameterValue (roomSize);
    parameterPointers.dynamics     = parameters.getRawParameterValue (dynamics);
    parameterPointers.intonation   = parameters.getRawParameterValue (intonation);

    jassert (parameterPointers.profile != nullptr && parameterPointers.output != nullptr);
    jassert (parameterPointers.vowelMorph != nullptr && parameterPointers.roomSize != nullptr);
    keyboardState.addListener (this);
}

VocalorAudioProcessor::~VocalorAudioProcessor()
{
    keyboardState.removeListener (this);
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalorAudioProcessor::createParameterLayout()
{
    using namespace vocalor::parameters;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;

    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { profile, 1 }, "Voice profile",
        juce::StringArray { "Female", "Male" }, 0));
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { mode, 1 }, "Performance mode",
        juce::StringArray { "Solo", "Choir", "Chord" }, 0));
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { vowel, 1 }, "Vowel",
        juce::StringArray { "AAH", "OOH", "UUH" }, 0));
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { chordQuality, 1 }, "Chord quality",
        juce::StringArray { "Major", "Minor" }, 0));
    result.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { choirSize, 1 }, "Choir size", 2, 16, 8));

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

    addPercent (breath, "Breath", 0.30f);
    addPercent (resonance, "Resonance", 0.64f);
    addPercent (vibrato, "Vibrato", 0.38f);
    addPercent (humanize, "Humanize", 0.52f);
    addPercent (spread, "Stereo spread", 0.62f);
    addPercent (tension, "Vocal tension", 0.36f);
    addPercent (room, "Room", 0.24f);

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { output, 1 }, "Output",
        juce::NormalisableRange<float> { -24.0f, 6.0f, 0.1f }, -6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // Version 1.1 additions. They are appended so every version-1 parameter
    // keeps its index, and every default reproduces the version-1 sound.
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { legato, 1 }, "Legato",
        juce::StringArray { "Off", "On" }, 0));

    addPercent (vowelX, "Vowel front-back", 0.50f);
    addPercent (vowelY, "Vowel open-close", 0.50f);
    addPercent (vowelMorph, "Vowel morph", 0.0f);

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { formantShift, 1 }, "Formant shift",
        juce::NormalisableRange<float> { -12.0f, 12.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("st")));

    addPercent (glide, "Glide", 0.0f);
    addPercent (roomSize, "Room size", 0.50f);

    // Version 1.2. Dynamics defaults to full so a session that predates it
    // recalls at the level it was written at; the mod wheel takes it over the
    // first time it moves.
    addPercent (dynamics, "Dynamics", 1.0f);
    addPercent (intonation, "Just intonation", 0.0f);

    return { result.begin(), result.end() };
}

void VocalorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engineReady.store (false, std::memory_order_release);
    engine.prepare (sampleRate, samplesPerBlock);
    engine.reset();
    updateEngineParameters();
    displaySampleRate.store (sampleRate, std::memory_order_relaxed);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    engineReady.store (true, std::memory_order_release);
}

void VocalorAudioProcessor::releaseResources()
{
    engineReady.store (false, std::memory_order_release);
    engine.allNotesOff();
    engine.reset();
    activeVoiceCount.store (0, std::memory_order_relaxed);
}

bool VocalorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VocalorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (! engineReady.load (std::memory_order_acquire))
        return;

    updateEngineParameters();

    // GUI notes enter through a bounded lock-free queue and start at the next
    // block boundary. This avoids allocating or locking in the audio callback.
    dispatchUiMidiEvents();

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
        engine.allSoundOff();

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

void VocalorAudioProcessor::dispatchMidiData (const juce::uint8* data, int numBytes) noexcept
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
        const auto value = static_cast<float> (data[2] & 0x7fu) / 127.0f;
        if (controller == 1u)
            engine.setModWheel (value);
        else if (controller == 11u)
            engine.setExpression (value);
        else if (controller == 64u)
            engine.setSustainPedal ((data[2] & 0x7fu) >= 64u);
        else if (controller == 120u)
            engine.allSoundOff();
        else if (controller == 121u)
        {
            // Reset All Controllers lifts the pedal too, which delivers every
            // note-off it was holding before the rest of the state is cleared.
            engine.setSustainPedal (false);
            engine.resetControllers();
        }
        else if (controller == 123u)
            engine.allNotesOff();
    }
    else if (kind == 0xd0u && numBytes >= 2)
    {
        // Channel pressure drives the same dynamic as the wheel, so a
        // controller with only one of the two is fully expressive.
        engine.setModWheel (static_cast<float> (data[1] & 0x7fu) / 127.0f);
    }
    else if (kind == 0xe0u && numBytes >= 3)
    {
        const auto raw = static_cast<int> (data[1] & 0x7fu)
                       | (static_cast<int> (data[2] & 0x7fu) << 7);
        engine.setPitchBend (vocalor::kPitchBendSemitones
                             * static_cast<float> (raw - 8192) / 8192.0f);
    }
}

void VocalorAudioProcessor::updateEngineParameters() noexcept
{
    vocalor::EngineParameters next;
    next.profile = enumFromParameter<vocalor::VoiceProfile> (parameterPointers.profile, 1);
    next.mode = enumFromParameter<vocalor::PerformanceMode> (parameterPointers.mode, 2);
    next.vowel = enumFromParameter<vocalor::Vowel> (parameterPointers.vowel, 2);
    next.chordQuality = enumFromParameter<vocalor::ChordQuality> (parameterPointers.chordQuality, 1);
    // The published range stays 2 - 16 so a host automation lane written
    // against it keeps mapping to the same singer count; VoiceEngine clamps to
    // its own singerCount, so values above 12 render as 12.
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
    next.legato = parameterPointers.legato->load (std::memory_order_relaxed) >= 0.5f;
    next.vowelX = parameterPointers.vowelX->load (std::memory_order_relaxed);
    next.vowelY = parameterPointers.vowelY->load (std::memory_order_relaxed);
    next.vowelMorph = parameterPointers.vowelMorph->load (std::memory_order_relaxed);
    next.formantShift = parameterPointers.formantShift->load (std::memory_order_relaxed);
    next.glide = parameterPointers.glide->load (std::memory_order_relaxed);
    next.roomSize = parameterPointers.roomSize->load (std::memory_order_relaxed);
    next.dynamics = parameterPointers.dynamics->load (std::memory_order_relaxed);
    next.intonation = parameterPointers.intonation->load (std::memory_order_relaxed);
    engine.setParameters (next);
}

void VocalorAudioProcessor::handleNoteOn (juce::MidiKeyboardState*, int,
                                         int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, true);
}

void VocalorAudioProcessor::handleNoteOff (juce::MidiKeyboardState*, int,
                                          int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, false);
}

void VocalorAudioProcessor::enqueueUiMidiEvent (int note, float velocity, bool isNoteOn) noexcept
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

void VocalorAudioProcessor::dispatchUiMidiEvents() noexcept
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

void VocalorAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destinationData);
}

void VocalorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        auto restoredState = juce::ValueTree::fromXml (*xml);
        addMissingParameterDefaults (restoredState, parameters, getParameters());
        parameters.replaceState (restoredState);
        requestPanic();
    }
}

juce::AudioProcessorEditor* VocalorAudioProcessor::createEditor()
{
    return new VocalorAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalorAudioProcessor();
}
