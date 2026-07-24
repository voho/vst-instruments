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
    parameterPointers.vibratoDepth   = parameters.getRawParameterValue (vibratoDepth);

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
             && parameterPointers.vibratoDepth != nullptr);
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
        juce::StringArray { "Neck", "Both", "Bridge" }, 2));
    addMorph (pickupType, "Pickup type", 0.5f, "Humbucker", "Single coil");
    addPercent (tone, "Tone", 0.8f);

    addMorph (bodyWood, "Body wood", 0.5f, "Mahogany/maple", "Swamp ash");
    addMorph (bodySize, "Body size", 0.5f, "Thick blank", "Thin slab");
    addMorph (bodyShape, "Body shape", 0.5f, "Carved top", "Flat slab");
    addMorph (construction, "Construction", 0.5f, "Set neck", "Bolt-on");
    addFloat (scaleLength, "Scale length", { 0.0f, 1.0f, 0.001f }, 0.5f,
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
    addPercent (bodyResonance, "Body resonance", 0.35f);

    addMorph (stringGauge, "String gauge", 0.5f, "9-80 set", "11-98 set");
    addPercent (stringAge, "String age", 0.15f);

    addPercent (pickPosition, "Pick position", 0.35f);
    addPercent (pickHardness, "Pick hardness", 0.6f);
    addPercent (pickNoise, "Pick noise", 0.5f);
    addPercent (fingerNoise, "Finger noise", 0.4f);
    addPercent (releaseNoise, "Release noise", 0.4f);
    addPercent (muteDamping, "Mute damping", 0.55f);

    auto bendRange = juce::NormalisableRange<float> { 0.04f, 2.0f, 0.0f };
    bendRange.setSkewForCentre (0.30f);
    addFloat (bendTime, "Bend time", bendRange, 0.28f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("s")
                  .withStringFromValueFunction (timeText)
                  .withValueFromStringFunction (timeValue));
    addPercent (velocity, "Velocity response", 0.65f);

    addFloat (output, "Output level", { -24.0f, 6.0f, 0.1f }, -6.0f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("dB")
                  .withStringFromValueFunction (decibelsText)
                  .withValueFromStringFunction (plainNumericValue));

    // Appended after the original version-1 parameter sequence so existing
    // host automation IDs and ordering stay intact.
    addPercent (artifacts, "Artifacts", 0.18f);
    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { outputMode, 1 }, "Output field",
        juce::StringArray { "Mono", "Stereo" }, 0));

    // Versioned additions are intentionally appended to preserve automation.
    addPercent (distortion, "Distortion", 0.0f);
    addPercent (amp, "Amp simulation", 0.0f);
    addPercent (compressor, "Compressor", 0.0f);
    addPercent (delay, "Delay", 0.0f);
    addPercent (room, "Room", 0.0f);

    // Version 1.1 additions, again appended so parameter indices 1..27 keep
    // pointing at exactly the same controls in existing host sessions.
    addPercent (sympathetic, "Sympathetic ring", 0.20f);
    addPercent (palmMute, "Palm mute", 0.0f);
    addFloat (strumSpread, "Strum spread", { 0.0f, 40.0f, 0.1f }, 0.0f,
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
    addFloat (vibratoDepth, "Vibrato depth", { 0.0f, 100.0f, 1.0f }, 35.0f,
              juce::AudioParameterFloatAttributes()
                  .withLabel ("cents")
                  .withStringFromValueFunction (
                      [] (float value, int)
                      {
                          return juce::String (juce::roundToInt (value))
                               + " cents";
                      })
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
    engine.setVibratoAmount (0.0f);
    engine.setPalmMutePressure (0.0f);
    effectSampleRate = juce::jlimit (8000.0, 384000.0, sampleRate);
    const auto delaySize = static_cast<std::size_t> (effectSampleRate * 1.25) + 1u;
    for (auto& line : delayLines)
        line.assign (delaySize, 0.0f);
    ampLowpass = {};
    roomState = {};
    compressorEnvelope = 0.0f;
    delayWriteIndex = 0;
    discardUiMidiEvents();
    panicRequested.store (false, std::memory_order_release);
    displaySampleRate.store (sampleRate, std::memory_order_relaxed);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    sympatheticStringCount.store (0, std::memory_order_relaxed);
    articulationIndex.store (
        static_cast<int> (engine.getCurrentArticulation()), std::memory_order_relaxed);
    publishStringVisualState();
    engineReady.store (true, std::memory_order_release);
}

void ElectryAudioProcessor::releaseResources()
{
    engineReady.store (false, std::memory_order_release);
    engine.setPitchBend (0.0f);
    engine.setSustainPedal (false);
    engine.setPalmMutePressure (0.0f);
    engine.allNotesOff();
    engine.reset();
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

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
    {
        engine.reset();
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

    processEffects (buffer, 0, numSamples);

    activeVoiceCount.store (engine.getActiveVoiceCount(), std::memory_order_relaxed);
    sympatheticStringCount.store (engine.getSympatheticStringCount(),
                                  std::memory_order_relaxed);
    articulationIndex.store (
        static_cast<int> (engine.getCurrentArticulation()), std::memory_order_relaxed);
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
            engine.setVibratoAmount (static_cast<float> (controllerValue) / 127.0f);
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
            engine.setVibratoAmount (0.0f);
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
    next.vibratoDepthCents = valueOf (parameterPointers.vibratoDepth);
    const auto mode = juce::jlimit (0, 1,
        juce::roundToInt (valueOf (parameterPointers.outputMode)));
    next.outputMode = static_cast<electry::OutputMode> (mode);
    engine.setParameters (next);
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

void ElectryAudioProcessor::processEffects (juce::AudioBuffer<float>& buffer,
                                            int startSample, int numSamples) noexcept
{
    if (delayLines[0].empty() || numSamples <= 0)
        return;

    const float distortionMix = valueOf (parameterPointers.distortion);
    const float ampMix = valueOf (parameterPointers.amp);
    const float compressorMix = valueOf (parameterPointers.compressor);
    const float delayMix = valueOf (parameterPointers.delay);
    const float roomMix = valueOf (parameterPointers.room);
    const float attack = std::exp (-1.0f / static_cast<float> (0.003 * effectSampleRate));
    const float release = std::exp (-1.0f / static_cast<float> (0.090 * effectSampleRate));
    const float cabinetCoefficient = std::exp (
        -juce::MathConstants<float>::twoPi * (1800.0f + 2600.0f * (1.0f - ampMix))
        / static_cast<float> (effectSampleRate));
    const int delaySamples = juce::jlimit (
        1, static_cast<int> (delayLines[0].size()) - 1,
        static_cast<int> (0.36 * effectSampleRate));
    const int roomSamples = juce::jmax (1, static_cast<int> (0.029 * effectSampleRate));
    const int lineSize = static_cast<int> (delayLines[0].size());

    for (int i = startSample; i < startSample + numSamples; ++i)
    {
        const float detector = juce::jmax (std::abs (buffer.getSample (0, i)),
                                           std::abs (buffer.getSample (1, i)));
        compressorEnvelope = (detector > compressorEnvelope ? attack : release)
                           * compressorEnvelope
                           + (1.0f - (detector > compressorEnvelope ? attack : release))
                           * detector;
        const float threshold = juce::Decibels::decibelsToGain (-20.0f);
        const float compressedGain = compressorEnvelope > threshold
            ? std::pow (threshold / compressorEnvelope, 0.72f) : 1.0f;

        for (int channel = 0; channel < 2; ++channel)
        {
            float sample = buffer.getSample (channel, i);
            const float driven = std::tanh (sample * (1.0f + 24.0f * distortionMix))
                               / std::tanh (1.0f + 24.0f * distortionMix);
            sample = juce::jmap (distortionMix, sample, driven);

            const float ampDriven = std::tanh (sample * (1.0f + 12.0f * ampMix));
            ampLowpass[static_cast<std::size_t> (channel)] =
                cabinetCoefficient * ampLowpass[static_cast<std::size_t> (channel)]
                + (1.0f - cabinetCoefficient) * ampDriven;
            sample = juce::jmap (ampMix, sample,
                1.35f * ampLowpass[static_cast<std::size_t> (channel)]);
            sample *= juce::jmap (compressorMix, 1.0f, compressedGain);

            auto& line = delayLines[static_cast<std::size_t> (channel)];
            const int delayedIndex = (delayWriteIndex - delaySamples + lineSize) % lineSize;
            const int roomIndex = (delayWriteIndex - roomSamples + lineSize) % lineSize;
            const float delayed = line[static_cast<std::size_t> (delayedIndex)];
            roomState[static_cast<std::size_t> (channel)] = 0.72f
                * roomState[static_cast<std::size_t> (channel)]
                + 0.28f * line[static_cast<std::size_t> (roomIndex)];
            line[static_cast<std::size_t> (delayWriteIndex)] = sample
                + delayed * (0.18f + 0.42f * delayMix)
                + roomState[static_cast<std::size_t> (1 - channel)] * 0.24f * roomMix;
            sample += delayed * 0.55f * delayMix
                    + roomState[static_cast<std::size_t> (channel)] * 0.38f * roomMix;
            buffer.setSample (channel, i, juce::jlimit (-2.0f, 2.0f, sample));
        }
        delayWriteIndex = (delayWriteIndex + 1) % lineSize;
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
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
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
