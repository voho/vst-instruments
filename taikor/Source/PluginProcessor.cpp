#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
namespace ids = taikor::parameters;

// Index into the processor's raw-pointer cache. Kept in the same order as the
// layout below so the two cannot drift apart.
enum Slot
{
    slotHeadDiameter = 0,
    slotBodyDepth,
    slotTension,
    slotHeadMaterial,
    slotShellMaterial,
    slotResonantTension,
    slotCavityCoupling,
    slotHeadDamping,
    slotShellResonance,
    slotPitch,
    slotBachiHardness,
    slotStrikePosition,
    slotVelocityDepth,
    slotTensionModulation,
    slotStrikeNoise,
    slotHumanise,
    slotOctaveBody,
    slotMicDistance,
    slotMicSpread,
    slotStereoWidth,
    slotDrive,
    slotOutput,
    slotCount
};

static_assert (static_cast<int> (slotCount) == ids::parameterCount,
               "the slot table and the declared parameter count must agree");

constexpr std::array<const char*, slotCount> parameterIds {
    ids::headDiameter, ids::bodyDepth, ids::tension, ids::headMaterial,
    ids::shellMaterial, ids::resonantTension, ids::cavityCoupling,
    ids::headDamping, ids::shellResonance, ids::pitch, ids::bachiHardness,
    ids::strikePosition, ids::velocityDepth, ids::tensionModulation,
    ids::strikeNoise, ids::humanise, ids::octaveBody, ids::micDistance,
    ids::micSpread, ids::stereoWidth, ids::drive, ids::output
};

std::unique_ptr<juce::RangedAudioParameter> makePercentParameter (
    const juce::String& id, const juce::String& name, float defaultValue)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, defaultValue,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (juce::roundToInt (value * 100.0f));
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue() / 100.0f;
            }));
}

std::unique_ptr<juce::RangedAudioParameter> makeCentimetreParameter (
    const juce::String& id, const juce::String& name, float minimum, float maximum,
    float defaultValue, float step)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> { minimum, maximum, step }, defaultValue,
        juce::AudioParameterFloatAttributes()
            .withLabel ("cm")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 1);
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue();
            }));
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

constexpr bool isValidArticulation (taikor::Articulation articulation) noexcept
{
    return static_cast<std::size_t> (articulation) < taikor::articulationCount;
}
} // namespace

TaikorAudioProcessor::TaikorAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output",
                                                    juce::AudioChannelSet::stereo(),
                                                    true)),
      parameters (*this, nullptr, "TAIKOR_STATE", createParameterLayout())
{
    for (int slot = 0; slot < slotCount; ++slot)
    {
        parameterPointers[static_cast<std::size_t> (slot)] =
            parameters.getRawParameterValue (parameterIds[static_cast<std::size_t> (slot)]);
        jassert (parameterPointers[static_cast<std::size_t> (slot)] != nullptr);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
TaikorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;
    result.reserve (static_cast<std::size_t> (ids::parameterCount));

    // --- The drum -------------------------------------------------------
    result.push_back (makeCentimetreParameter (
        ids::headDiameter, "Head Diameter", ids::minimumDiameterCentimetres,
        ids::maximumDiameterCentimetres, 95.0f, 0.5f));
    result.push_back (makePercentParameter (ids::bodyDepth, "Body Depth", 0.5f));
    result.push_back (makePercentParameter (ids::tension, "Head Tension", 0.55f));
    result.push_back (makePercentParameter (ids::headMaterial, "Head Material", 0.75f));
    result.push_back (makePercentParameter (ids::shellMaterial, "Shell Material", 0.8f));
    result.push_back (makePercentParameter (
        ids::resonantTension, "Resonant Head", 0.5f));
    result.push_back (makePercentParameter (ids::cavityCoupling, "Air Coupling", 0.85f));
    result.push_back (makePercentParameter (ids::headDamping, "Head Damping", 0.50f));
    result.push_back (makePercentParameter (
        ids::shellResonance, "Shell Resonance", 0.4f));

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::pitch, 1 }, "Pitch",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("st")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 1);
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue();
            })));

    // --- The stroke -----------------------------------------------------
    result.push_back (makePercentParameter (
        ids::bachiHardness, "Bachi Hardness", 0.7f));

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::strikePosition, 1 }, "Strike Position",
        juce::NormalisableRange<float> { -1.0f, 1.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                const auto amount = juce::roundToInt (std::abs (value) * 100.0f);
                if (amount == 0)
                    return juce::String ("As written");
                return (value < 0.0f ? juce::String ("Centre ") : juce::String ("Rim "))
                     + juce::String (amount);
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                const auto trimmed = text.trim().toUpperCase();
                const auto amount =
                    trimmed.retainCharacters ("0123456789.-").getFloatValue() / 100.0f;
                return trimmed.startsWith ("CENTRE") || trimmed.startsWith ("CENTER")
                    ? -std::abs (amount) : amount;
            })));

    result.push_back (makePercentParameter (
        ids::velocityDepth, "Velocity Depth", 0.75f));
    result.push_back (makePercentParameter (
        ids::tensionModulation, "Tension Mod", 0.4f));
    result.push_back (makePercentParameter (ids::strikeNoise, "Strike Noise", 0.35f));
    result.push_back (makePercentParameter (ids::humanise, "Humanise", 0.4f));

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::octaveBody, 1 }, "Octave Body",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.7f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float value, int)
            {
                // The two ends are different instruments, not different
                // amounts of one, so the readout says which.
                if (value <= 0.02f)
                    return juce::String ("Tuned");
                if (value >= 0.98f)
                    return juce::String ("Family");
                return juce::String (juce::roundToInt (value * 100.0f));
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                const auto trimmed = text.trim().toUpperCase();
                if (trimmed.startsWith ("TUNED"))
                    return 0.0f;
                if (trimmed.startsWith ("FAMILY"))
                    return 1.0f;
                return text.retainCharacters ("0123456789.-").getFloatValue() / 100.0f;
            })));

    // --- The close pair and the output ----------------------------------
    result.push_back (makeCentimetreParameter (
        ids::micDistance, "Mic Distance", ids::minimumMicDistanceCentimetres,
        ids::maximumMicDistanceCentimetres, 16.0f, 0.5f));
    result.push_back (makePercentParameter (ids::micSpread, "Mic Spread", 0.55f));
    result.push_back (makePercentParameter (ids::stereoWidth, "Stereo Width", 0.5f));
    result.push_back (makePercentParameter (ids::drive, "Drive", 0.0f));

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::output, 1 }, "Output",
        // The loudest single stroke the instrument can make - a full-velocity
        // rim shot - sits about 18 dB above unity, so this default is chosen to
        // leave that stroke just under full scale rather than to make a middling
        // stroke as loud as possible. A ghost stroke is another thirty-four
        // decibels below it, which is the range the model actually covers.
        juce::NormalisableRange<float> { -24.0f, 6.0f, 0.1f }, -20.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 1);
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue();
            })));

    jassert (static_cast<int> (result.size()) == ids::parameterCount);
    return { result.begin(), result.end() };
}

taikor::EngineParameters TaikorAudioProcessor::snapshotEngineParameters() const noexcept
{
    const auto read = [this] (int slot) noexcept
    {
        const auto* pointer = parameterPointers[static_cast<std::size_t> (slot)];
        return pointer != nullptr ? pointer->load (std::memory_order_relaxed) : 0.0f;
    };

    taikor::EngineParameters next;
    next.headDiameter = read (slotHeadDiameter) * 0.01f; // centimetres to metres
    next.bodyDepth = read (slotBodyDepth);
    next.tension = read (slotTension);
    next.headMaterial = read (slotHeadMaterial);
    next.shellMaterial = read (slotShellMaterial);
    next.resonantTension = read (slotResonantTension);
    next.cavityCoupling = read (slotCavityCoupling);
    next.headDamping = read (slotHeadDamping);
    next.shellResonance = read (slotShellResonance);
    next.pitch = read (slotPitch);
    next.bachiHardness = read (slotBachiHardness);
    next.strikePosition = read (slotStrikePosition);
    next.velocityDepth = read (slotVelocityDepth);
    next.tensionModulation = read (slotTensionModulation);
    next.strikeNoise = read (slotStrikeNoise);
    next.humanise = read (slotHumanise);
    next.octaveBody = read (slotOctaveBody);
    next.micDistance = juce::jmap (read (slotMicDistance),
                                   ids::minimumMicDistanceCentimetres,
                                   ids::maximumMicDistanceCentimetres, 0.0f, 1.0f);
    next.micSpread = read (slotMicSpread);
    next.stereoWidth = read (slotStereoWidth);
    next.drive = read (slotDrive);
    next.outputGain = juce::Decibels::decibelsToGain (read (slotOutput));
    return next;
}

taikor::TaikoEngine::DrumMeasurements TaikorAudioProcessor::measureDrum (
    int octaveOffset) const noexcept
{
    return taikor::TaikoEngine::measure (snapshotEngineParameters(), octaveOffset);
}

void TaikorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engineReady.store (false, std::memory_order_release);
    uiQueueGeneration.fetch_add (1, std::memory_order_acq_rel);
    discardUiTriggers();
    // Publish the parameters before prepare() resets the smoothers, so the
    // first stroke starts at the restored gain rather than gliding to it.
    updateEngineParameters();
    engine.prepare (sampleRate, samplesPerBlock);
    displaySampleRate.store (sampleRate, std::memory_order_relaxed);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    engineReady.store (true, std::memory_order_release);
}

void TaikorAudioProcessor::releaseResources()
{
    engineReady.store (false, std::memory_order_release);
    uiQueueGeneration.fetch_add (1, std::memory_order_acq_rel);
    discardUiTriggers();
    engine.allSoundsOff();
    engine.reset();
    activeVoiceCount.store (0, std::memory_order_relaxed);
    displaySampleRate.store (0.0, std::memory_order_relaxed);
}

bool TaikorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // The instrument is a stereo close pair by construction: the two
    // microphones are different points on the drum, and summing them to mono
    // would throw away the model's own image rather than fold it down.
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void TaikorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (! engineReady.load (std::memory_order_acquire))
        return;

    updateEngineParameters();

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
    {
        discardUiTriggers();
        engine.allSoundsOff();
    }

    // Editor pad strokes are deliberately quantised to the next block boundary.
    // The bounded SPSC queue keeps both message and audio threads lock-free.
    dispatchUiTriggers();

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

void TaikorAudioProcessor::dispatchMidiData (const juce::uint8* data,
                                             int numBytes) noexcept
{
    if (data == nullptr || numBytes < 1)
        return;

    const auto status = static_cast<unsigned> (data[0]);
    const auto kind = status & 0xf0u;

    if (kind == 0x90u && numBytes >= 3 && data[2] != 0)
    {
        const auto midiNote = static_cast<int> (data[1] & 0x7fu);
        const auto velocity = static_cast<float> (data[2] & 0x7fu) / 127.0f;

        if (engine.triggerMidi (midiNote, velocity))
            if (const auto articulation = taikor::articulationForMidiNote (midiNote))
                registerTrigger (*articulation);
    }
    else if (kind == 0xe0u && numBytes >= 3)
    {
        // Pressing the head raises its tension, so the wheel bends the drum up.
        const auto raw = static_cast<int> (data[1] & 0x7fu)
                       | (static_cast<int> (data[2] & 0x7fu) << 7);
        engine.setPitchBend (static_cast<float> (raw - 8192) / 8192.0f);
    }
    else if (kind == 0xb0u && numBytes >= 3)
    {
        const auto controller = data[1] & 0x7fu;
        const auto value = static_cast<float> (data[2] & 0x7fu) / 127.0f;

        if (controller == 1u)
        {
            // A hand laid on the head. It damps what is still ringing without
            // changing anything about the strokes that follow.
            engine.setHandDamping (value);
        }
        else if (controller == 121u)
        {
            // Reset All Controllers. Both of Taikor's performable controls go
            // back to where they started: the hand comes off the head and the
            // wheel returns to centre. Without this a host that resets its
            // controller state left the drum muted or bent, and every stroke
            // after it inherited a gesture the sender believed it had cleared.
            engine.setHandDamping (0.0f);
            engine.setPitchBend (0.0f);
        }
        else if (controller == 120u || controller == 123u)
        {
            engine.allSoundsOff();
        }
    }
}

void TaikorAudioProcessor::triggerFromUi (taikor::Articulation articulation,
                                          int octaveOffset, float velocity) noexcept
{
    if (! engineReady.load (std::memory_order_acquire))
        return;
    enqueueUiTrigger (articulation, octaveOffset, velocity);
}

void TaikorAudioProcessor::requestPanic() noexcept
{
    // Events published before this generation change are stale even if their
    // producer races the audio-thread queue flush.
    uiQueueGeneration.fetch_add (1, std::memory_order_acq_rel);
    panicRequested.store (true, std::memory_order_release);
}

void TaikorAudioProcessor::enqueueUiTrigger (taikor::Articulation articulation,
                                             int octaveOffset, float velocity) noexcept
{
    if (! isValidArticulation (articulation) || ! std::isfinite (velocity)
        || velocity <= 0.0f)
        return;

    const auto write = uiWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1u) % uiQueueCapacity;

    if (next == uiReadIndex.load (std::memory_order_acquire))
    {
        // A one-shot audition can be dropped; the message thread must not wait.
        return;
    }

    uiTriggerQueue[write] = {
        articulation,
        juce::jlimit (taikor::lowestOctaveOffset, taikor::highestOctaveOffset,
                      octaveOffset),
        juce::jlimit (0.0f, 1.0f, velocity),
        uiQueueGeneration.load (std::memory_order_acquire)
    };
    uiWriteIndex.store (next, std::memory_order_release);
}

void TaikorAudioProcessor::dispatchUiTriggers() noexcept
{
    auto read = uiReadIndex.load (std::memory_order_relaxed);
    const auto write = uiWriteIndex.load (std::memory_order_acquire);

    while (read != write)
    {
        const auto event = uiTriggerQueue[read];
        if (event.generation == uiQueueGeneration.load (std::memory_order_acquire))
        {
            engine.trigger (event.articulation, event.octaveOffset, event.velocity);
            registerTrigger (event.articulation);
        }
        read = (read + 1u) % uiQueueCapacity;
    }

    uiReadIndex.store (read, std::memory_order_release);
}

void TaikorAudioProcessor::discardUiTriggers() noexcept
{
    uiReadIndex.store (uiWriteIndex.load (std::memory_order_acquire),
                       std::memory_order_release);
}

void TaikorAudioProcessor::registerTrigger (taikor::Articulation articulation) noexcept
{
    if (isValidArticulation (articulation))
        triggerCounters[static_cast<std::size_t> (articulation)]
            .fetch_add (1, std::memory_order_release);
}

std::uint32_t TaikorAudioProcessor::getTriggerCounter (
    taikor::Articulation articulation) const noexcept
{
    return isValidArticulation (articulation)
        ? triggerCounters[static_cast<std::size_t> (articulation)]
              .load (std::memory_order_acquire)
        : 0u;
}

void TaikorAudioProcessor::updateEngineParameters() noexcept
{
    engine.setParameters (snapshotEngineParameters());
}

void TaikorAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destinationData);
}

void TaikorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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

juce::AudioProcessorEditor* TaikorAudioProcessor::createEditor()
{
    return new TaikorAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TaikorAudioProcessor();
}
