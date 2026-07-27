#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
float valueOf (const std::atomic<float>* value) noexcept
{
    return value->load (std::memory_order_relaxed);
}

juce::String percentText (float value, int)
{
    return juce::String (juce::roundToInt (value * 100.0f)) + "%";
}

float percentValue (const juce::String& text)
{
    return text.retainCharacters ("0123456789.-").getFloatValue() / 100.0f;
}

juce::String decibelsText (float value, int)
{
    return (value > 0.0f ? "+" : "") + juce::String (value, 1) + "dB";
}

float plainNumericValue (const juce::String& text)
{
    return text.retainCharacters ("0123456789.-").getFloatValue();
}

juce::String timeText (float value, int)
{
    if (value < 1.0f)
        return juce::String (juce::roundToInt (value * 1000.0f)) + " ms";
    return juce::String (value, 2) + " s";
}

// For parameters whose plain value already spans 0..100.
juce::String percentText100 (float value, int)
{
    return juce::String (juce::roundToInt (value)) + "%";
}

float timeValue (const juce::String& text)
{
    const auto value = text.retainCharacters ("0123456789.-").getFloatValue();
    return text.containsIgnoreCase ("ms") ? value / 1000.0f : value;
}

juce::AudioParameterFloatAttributes percentAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withLabel ("%")
        .withStringFromValueFunction (percentText)
        .withValueFromStringFunction (percentValue);
}

// Material/construction morph axes use named solid-body endpoints.
juce::AudioParameterFloatAttributes morphAttributes (const char* lowName,
                                                     const char* highName)
{
    const juce::String low (lowName);
    const juce::String high (highName);
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction (
            [low, high] (float value, int)
            {
                if (value <= 0.02f)
                    return low;
                if (value >= 0.98f)
                    return high;
                return juce::String (juce::roundToInt (value * 100.0f)) + "% "
                     + high;
            })
        .withValueFromStringFunction (percentValue);
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

ElectryAudioProcessor::ElectryAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "ELECTRY_STATE", createParameterLayout())
{
    using namespace electry::parameters;

    parameterPointers.pickupSelector = parameters.getRawParameterValue (pickupSelector);
    parameterPointers.pickupType     = parameters.getRawParameterValue (pickupType);
    parameterPointers.tone           = parameters.getRawParameterValue (tone);
    parameterPointers.bodyWood       = parameters.getRawParameterValue (bodyWood);
    parameterPointers.bodySize       = parameters.getRawParameterValue (bodySize);
    parameterPointers.bodyShape      = parameters.getRawParameterValue (bodyShape);
    parameterPointers.construction   = parameters.getRawParameterValue (construction);
    parameterPointers.scaleLength    = parameters.getRawParameterValue (scaleLength);
    parameterPointers.bodyResonance  = parameters.getRawParameterValue (bodyResonance);
    parameterPointers.stringGauge    = parameters.getRawParameterValue (stringGauge);
    parameterPointers.stringAge      = parameters.getRawParameterValue (stringAge);
    parameterPointers.pickPosition   = parameters.getRawParameterValue (pickPosition);
    parameterPointers.pickHardness   = parameters.getRawParameterValue (pickHardness);
    parameterPointers.pickNoise      = parameters.getRawParameterValue (pickNoise);
    parameterPointers.fingerNoise    = parameters.getRawParameterValue (fingerNoise);
    parameterPointers.releaseNoise   = parameters.getRawParameterValue (releaseNoise);
    parameterPointers.muteDamping    = parameters.getRawParameterValue (muteDamping);
    parameterPointers.bendTime       = parameters.getRawParameterValue (bendTime);
    parameterPointers.velocity       = parameters.getRawParameterValue (velocity);
    parameterPointers.output         = parameters.getRawParameterValue (output);
    parameterPointers.artifacts      = parameters.getRawParameterValue (artifacts);
    parameterPointers.outputMode     = parameters.getRawParameterValue (outputMode);
    parameterPointers.distortion     = parameters.getRawParameterValue (distortion);
    parameterPointers.amp            = parameters.getRawParameterValue (amp);
    parameterPointers.compressor     = parameters.getRawParameterValue (compressor);
    parameterPointers.delay          = parameters.getRawParameterValue (delay);
    parameterPointers.room           = parameters.getRawParameterValue (room);
    parameterPointers.sympathetic    = parameters.getRawParameterValue (sympathetic);
    parameterPointers.palmMute       = parameters.getRawParameterValue (palmMute);
    parameterPointers.strumSpread    = parameters.getRawParameterValue (strumSpread);
    parameterPointers.resonanceDepth = parameters.getRawParameterValue (resonanceDepth);

    jassert (parameterPointers.pickupSelector != nullptr
             && parameterPointers.pickupType != nullptr
             && parameterPointers.tone != nullptr
             && parameterPointers.bodyWood != nullptr
             && parameterPointers.scaleLength != nullptr
             && parameterPointers.bendTime != nullptr
             && parameterPointers.output != nullptr
             && parameterPointers.artifacts != nullptr
             && parameterPointers.outputMode != nullptr
             && parameterPointers.sympathetic != nullptr
             && parameterPointers.palmMute != nullptr
             && parameterPointers.strumSpread != nullptr
             && parameterPointers.resonanceDepth != nullptr);
    keyboardState.addListener (this);
}

ElectryAudioProcessor::~ElectryAudioProcessor()
{
    keyboardState.removeListener (this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
ElectryAudioProcessor::createParameterLayout()
{
    using namespace electry::parameters;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;
    result.reserve (31);

    // Every default below is read from the engine's own struct rather than
    // written out again here. These two lists had drifted apart: the engine's
    // defaults became a specific instrument - thick blank, heaviest set, tone
    // back, pick near the bridge - while this layout still created every new
    // plug-in instance at the old midpoints, so the shipping product and its
    // reset-to-default kept a voicing the README and the demos no longer
    // described. Deriving them makes that divergence impossible rather than
    // merely fixed.
    const electry::EngineParameters engineDefaults {};

    const auto addFloat = [&result] (const char* id, const char* name,
                                     juce::NormalisableRange<float> range, float defaultValue,
                                     juce::AudioParameterFloatAttributes attributes = {})
    {
        result.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultValue, std::move (attributes)));
    };
    const auto addPercent = [&addFloat] (const char* id, const char* name,
                                         float defaultValue)
    {
        addFloat (id, name, { 0.0f, 1.0f, 0.001f }, defaultValue, percentAttributes());
    };
    const auto addMorph = [&addFloat] (const char* id, const char* name,
                                       float defaultValue, const char* lowName,
                                       const char* highName)
    {
        addFloat (id, name, { 0.0f, 1.0f, 0.001f }, defaultValue,
                  morphAttributes (lowName, highName));
    };

    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { pickupSelector, 1 }, "Pickup selector",
        juce::StringArray { "Neck", "Both", "Bridge" },
        static_cast<int> (engineDefaults.pickupSelector)));
    addMorph (pickupType, "Pickup type", engineDefaults.pickupType,
              "Humbucker", "Single coil");
    addPercent (tone, "Tone", engineDefaults.toneKnob);

    addMorph (bodyWood, "Body wood", engineDefaults.bodyWood,
              "Mahogany/maple", "Swamp ash");
    addMorph (bodySize, "Body size", engineDefaults.bodySize,
              "Thick blank", "Thin slab");
    addMorph (bodyShape, "Body shape", engineDefaults.bodyShape,
              "Carved top", "Flat slab");
    addMorph (construction, "Construction", engineDefaults.construction,
              "Set neck", "Bolt-on");
    addFloat (scaleLength, "Scale length", { 0.0f, 1.0f, 0.001f },
              engineDefaults.scaleLength,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("in")
                  .withStringFromValueFunction (
                      [] (float value, int)
                      {
                          return juce::String (25.5f + value * 2.5f, 2) + "\"";
                      })
                  .withValueFromStringFunction (
                      [] (const juce::String& text)
                      {
                          const auto inches = plainNumericValue (text);
                          return juce::jlimit (0.0f, 1.0f, (inches - 25.5f) / 2.5f);
                      }));
    addPercent (bodyResonance, "Body resonance", engineDefaults.bodyResonance);

    addMorph (stringGauge, "String gauge", engineDefaults.stringGauge,
              "9-80 set", "11-98 set");
    addPercent (stringAge, "String age", engineDefaults.stringAge);

    addPercent (pickPosition, "Pick position", engineDefaults.pickPosition);
    addPercent (pickHardness, "Pick hardness", engineDefaults.pickHardness);
    addPercent (pickNoise, "Pick noise", engineDefaults.pickNoise);
    addPercent (fingerNoise, "Finger noise", engineDefaults.fingerNoise);
    addPercent (releaseNoise, "Release noise", engineDefaults.releaseNoise);
    addPercent (muteDamping, "Mute damping", engineDefaults.muteDamping);

    auto bendRange = juce::NormalisableRange<float> { 0.04f, 2.0f, 0.0f };
    bendRange.setSkewForCentre (0.30f);
    addFloat (bendTime, "Bend time", bendRange, engineDefaults.bendTimeSeconds,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("s")
                  .withStringFromValueFunction (timeText)
                  .withValueFromStringFunction (timeValue));
    addPercent (velocity, "Velocity response", engineDefaults.velocityAmount);

    addFloat (output, "Output level", { -24.0f, 6.0f, 0.1f }, -6.0f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("dB")
                  .withStringFromValueFunction (decibelsText)
                  .withValueFromStringFunction (plainNumericValue));

    // Appended after the original version-1 parameter sequence so existing
    // host automation IDs and ordering stay intact.
    addPercent (artifacts, "Artifacts", engineDefaults.artifactAmount);
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { outputMode, 1 }, "Output field",
        juce::StringArray { "Mono", "Stereo" },
        static_cast<int> (engineDefaults.outputMode)));

    // Versioned additions are intentionally appended to preserve automation.
    addPercent (distortion, "Distortion", 0.0f);
    addPercent (amp, "Amp simulation", 0.0f);
    addPercent (compressor, "Compressor", 0.0f);
    addPercent (delay, "Delay", 0.0f);
    addPercent (room, "Room", 0.0f);

    // Version 1.1 additions, again appended so parameter indices 1..27 keep
    // pointing at exactly the same controls in existing host sessions.
    addPercent (sympathetic, "Sympathetic ring", engineDefaults.sympatheticAmount);
    addPercent (palmMute, "Palm mute", engineDefaults.palmMute);
    addFloat (strumSpread, "Strum spread", { 0.0f, 40.0f, 0.1f },
              1000.0f * engineDefaults.strumSpreadSeconds,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("ms")
                  .withStringFromValueFunction (
                      [] (float value, int)
                      {
                          if (value < 0.05f)
                              return juce::String ("Block chord");
                          return juce::String (value, 1) + " ms/string";
                      })
                  .withValueFromStringFunction (plainNumericValue));
    // The 1.2 resonance control lives in the slot the 1.1 vibrato depth used
    // (same stored ID and 0..100 range), so a saved session's value carries
    // over as a sensible resonance depth and every automation index is kept.
    addFloat (resonanceDepth, "Resonance depth", { 0.0f, 100.0f, 1.0f },
              100.0f * engineDefaults.resonanceDepth,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("%")
                  .withStringFromValueFunction (percentText100)
                  .withValueFromStringFunction (plainNumericValue));

    return { result.begin(), result.end() };
}

void ElectryAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engineReady.store (false, std::memory_order_release);
    engine.prepare (sampleRate, samplesPerBlock);
    updateEngineParameters();
    engine.reset();
    engine.setPitchBend (0.0f);
    engine.setSustainPedal (false);
    engine.setResonance (0.0f);
    engine.setPalmMutePressure (0.0f);
    effects.prepare (sampleRate);
    updateEffectParameters();
    effects.reset();
    discardUiMidiEvents();
    panicRequested.store (false, std::memory_order_release);
    displaySampleRate.store (sampleRate, std::memory_order_relaxed);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    sympatheticStringCount.store (0, std::memory_order_relaxed);
    pickStyleIndex.store (
        static_cast<int> (engine.getCurrentPickStyle()), std::memory_order_relaxed);
    playStyleIndex.store (
        static_cast<int> (engine.getCurrentPlayStyle()), std::memory_order_relaxed);
    publishStringVisualState();
    engineReady.store (true, std::memory_order_release);
}

void ElectryAudioProcessor::releaseResources()
{
    engineReady.store (false, std::memory_order_release);
    engine.setPitchBend (0.0f);
    engine.setSustainPedal (false);
    engine.setResonance (0.0f);
    engine.setPalmMutePressure (0.0f);
    engine.allNotesOff();
    engine.reset();
    effects.reset();
    discardUiMidiEvents();
    panicRequested.store (false, std::memory_order_release);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    sympatheticStringCount.store (0, std::memory_order_relaxed);
    displaySampleRate.store (0.0, std::memory_order_relaxed);
    publishStringVisualState();
}

bool ElectryAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ElectryAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (! engineReady.load (std::memory_order_acquire))
        return;

    updateEngineParameters();
    updateEffectParameters();

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
    {
        engine.reset();
        effects.reset();
        discardUiMidiEvents();
    }

    // GUI notes and articulation clicks enter through a bounded lock-free
    // queue and start at the next block boundary. This avoids allocating or
    // locking in the audio callback.
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

    effects.process (buffer.getWritePointer (0), buffer.getWritePointer (1),
                     numSamples);

    // The amplified output is what the loudspeaker plays back at the guitar;
    // the engine consumes it next block, which gives the resonance feedback
    // loop the acoustic latency a real speaker-to-string path has.
    engine.pushAcousticReturn (buffer.getReadPointer (0),
                               buffer.getReadPointer (1), numSamples);

    activeVoiceCount.store (engine.getActiveVoiceCount(), std::memory_order_relaxed);
    sympatheticStringCount.store (engine.getSympatheticStringCount(),
                                  std::memory_order_relaxed);
    pickStyleIndex.store (
        static_cast<int> (engine.getCurrentPickStyle()), std::memory_order_relaxed);
    playStyleIndex.store (
        static_cast<int> (engine.getCurrentPlayStyle()), std::memory_order_relaxed);
    publishStringVisualState();
}

void ElectryAudioProcessor::dispatchMidiData (const juce::uint8* data, int numBytes) noexcept
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
        const auto controllerValue = data[2] & 0x7fu;
        if (controller == 64u)
            engine.setSustainPedal (controllerValue >= 64u);
        else if (controller == 1u)
        {
            // The modulation wheel is the performance resonance: it lifts the
            // sympathetic coupling toward total and opens the acoustic
            // feedback path, so a distorted tone can be played into a howl.
            engine.setResonance (static_cast<float> (controllerValue) / 127.0f);
        }
        else if (controller == 2u)
        {
            // Breath/CC2 is the performable side of the Palm Mute parameter:
            // it adds bridge-hand pressure without needing automation.
            engine.setPalmMutePressure (
                static_cast<float> (controllerValue) / 127.0f);
        }
        else if (controller == 121u)
        {
            engine.setPitchBend (0.0f);
            engine.setResonance (0.0f);
            engine.setPalmMutePressure (0.0f);
            engine.setSustainPedal (false);
        }
        else if (controller == 120u)
        {
            // MIDI All Sound Off is an immediate mute, like the front-panel
            // Panic control. The parameter targets survive the engine reset.
            engine.reset();
        }
        else if (controller == 123u)
        {
            // All Notes Off releases the held strings so the natural damped
            // ring-out remains musical.
            engine.allNotesOff();
        }
    }
    else if (kind == 0xe0u && numBytes >= 3)
    {
        const auto value14 = static_cast<int> (data[1] & 0x7fu)
                           | (static_cast<int> (data[2] & 0x7fu) << 7);
        const auto bend = value14 < 8192
            ? static_cast<float> (value14 - 8192) / 8192.0f
            : static_cast<float> (value14 - 8192) / 8191.0f;
        engine.setPitchBend (juce::jlimit (-1.0f, 1.0f, bend));
    }
}

void ElectryAudioProcessor::updateEngineParameters() noexcept
{
    electry::EngineParameters next;
    const auto selector = juce::jlimit (0, 2,
        juce::roundToInt (valueOf (parameterPointers.pickupSelector)));
    next.pickupSelector = static_cast<electry::PickupSelector> (selector);
    next.pickupType = valueOf (parameterPointers.pickupType);
    next.toneKnob = valueOf (parameterPointers.tone);
    next.bodyWood = valueOf (parameterPointers.bodyWood);
    next.bodySize = valueOf (parameterPointers.bodySize);
    next.bodyShape = valueOf (parameterPointers.bodyShape);
    next.construction = valueOf (parameterPointers.construction);
    next.scaleLength = valueOf (parameterPointers.scaleLength);
    next.bodyResonance = valueOf (parameterPointers.bodyResonance);
    next.stringGauge = valueOf (parameterPointers.stringGauge);
    next.stringAge = valueOf (parameterPointers.stringAge);
    next.pickPosition = valueOf (parameterPointers.pickPosition);
    next.pickHardness = valueOf (parameterPointers.pickHardness);
    next.pickNoise = valueOf (parameterPointers.pickNoise);
    next.fingerNoise = valueOf (parameterPointers.fingerNoise);
    next.releaseNoise = valueOf (parameterPointers.releaseNoise);
    next.muteDamping = valueOf (parameterPointers.muteDamping);
    next.bendTimeSeconds = valueOf (parameterPointers.bendTime);
    next.velocityAmount = valueOf (parameterPointers.velocity);
    next.outputGain = juce::Decibels::decibelsToGain (valueOf (parameterPointers.output));
    next.artifactAmount = valueOf (parameterPointers.artifacts);
    next.sympatheticAmount = valueOf (parameterPointers.sympathetic);
    next.palmMute = valueOf (parameterPointers.palmMute);
    next.strumSpreadSeconds = 0.001f * valueOf (parameterPointers.strumSpread);
    next.resonanceDepth = 0.01f * valueOf (parameterPointers.resonanceDepth);
    const auto mode = juce::jlimit (0, 1,
        juce::roundToInt (valueOf (parameterPointers.outputMode)));
    next.outputMode = static_cast<electry::OutputMode> (mode);
    engine.setParameters (next);
}

void ElectryAudioProcessor::updateEffectParameters() noexcept
{
    electry::FxParameters next;
    next.distortion = valueOf (parameterPointers.distortion);
    next.amp = valueOf (parameterPointers.amp);
    next.compressor = valueOf (parameterPointers.compressor);
    next.delay = valueOf (parameterPointers.delay);
    next.room = valueOf (parameterPointers.room);
    effects.setParameters (next);

    // How loud the rig actually is in the room. The chain manages its own
    // listening level, but acoustically a cranked amplifier is deafening
    // while a clean DI is not in the room at all - and that level is what
    // decides whether the resonance wheel can push the strings into feedback.
    engine.setAcousticReturnLevel (
        juce::jmin (1.0f, next.amp + 0.6f * next.distortion));
}

void ElectryAudioProcessor::publishStringVisualState() noexcept
{
    engine.getStringVisualState (visualScratch);
    for (int stringIndex = 0;
         stringIndex < electry::ElectryEngine::stringCount; ++stringIndex)
    {
        const auto index = static_cast<std::size_t> (stringIndex);
        stringVisuals[index].store (
            electry::visuals::packStringVisual (visualScratch[index]),
            std::memory_order_relaxed);
    }
}

void ElectryAudioProcessor::triggerArticulation (int index)
{
    if (index < 0 || index >= electry::ElectryEngine::keyswitchCount)
        return;
    enqueueUiMidiEvent (electry::ElectryEngine::firstKeyswitchNote + index,
                        1.0f, true);
}

void ElectryAudioProcessor::handleNoteOn (juce::MidiKeyboardState*, int,
                                          int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, true);
}

void ElectryAudioProcessor::handleNoteOff (juce::MidiKeyboardState*, int,
                                           int midiNoteNumber, float velocity)
{
    enqueueUiMidiEvent (midiNoteNumber, velocity, false);
}

void ElectryAudioProcessor::enqueueUiMidiEvent (int note, float velocity, bool isNoteOn) noexcept
{
    const auto write = uiWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1u) % uiQueueCapacity;

    // Dropping an event is preferable to ever waiting on the message or audio thread.
    if (next == uiReadIndex.load (std::memory_order_acquire))
    {
        panicRequested.store (true, std::memory_order_release);
        return;
    }

    uiMidiQueue[write] = { juce::jlimit (0, 127, note),
                           juce::jlimit (0.0f, 1.0f, velocity), isNoteOn };
    uiWriteIndex.store (next, std::memory_order_release);
}

void ElectryAudioProcessor::dispatchUiMidiEvents() noexcept
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

void ElectryAudioProcessor::discardUiMidiEvents() noexcept
{
    uiReadIndex.store (uiWriteIndex.load (std::memory_order_acquire),
                       std::memory_order_release);
}

void ElectryAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destinationData);
}

void ElectryAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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

juce::AudioProcessorEditor* ElectryAudioProcessor::createEditor()
{
    return new ElectryAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ElectryAudioProcessor();
}
