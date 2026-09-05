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
    slotStrikeAzimuth,
    slotPerformer,
    slotVelocityCurve,
    slotOutputHighPass,
    slotEnsembleSize,
    slotEnsembleVariation,
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
    ids::micSpread, ids::stereoWidth, ids::drive, ids::output,
    ids::strikeAzimuth, ids::performer, ids::velocityCurve, ids::outputHighPass,
    ids::ensembleSize, ids::ensembleVariation
};

float bipolarControllerValue (int rawValue) noexcept
{
    // MIDI's conventional centre is the exact value 64: 64 steps below it,
    // 63 above it.
    return rawValue < 64
        ? static_cast<float> (rawValue - 64) / 64.0f
        : static_cast<float> (rawValue - 64) / 63.0f;
}

std::unique_ptr<juce::RangedAudioParameter> makePercentParameter (
    const juce::String& id, const juce::String& name, float defaultValue,
    int versionHint = 1)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, versionHint }, name,
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
        ids::maximumDiameterCentimetres, 150.0f, 0.5f));
    result.push_back (makePercentParameter (ids::bodyDepth, "Body Depth", 0.5f));
    result.push_back (makePercentParameter (ids::tension, "Head Tension", 0.62f));
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

    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        // Keep the established ID, order and normalised endpoints so old
        // projects restore, but advertise a real indexed choice to every host.
        // A legacy value below/above the midpoint snaps to the nearest endpoint.
        juce::ParameterID { ids::octaveBody, 1 }, "Drum Layout",
        juce::StringArray { "1 Drum", "4 Drums" }, 1,
        juce::AudioParameterChoiceAttributes()
            .withStringFromValueFunction ([] (int index, int)
            {
                return index == 0 ? juce::String ("1 Drum")
                                  : juce::String ("4 Drums");
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                const auto trimmed = text.trim().toUpperCase();
                if (trimmed.startsWith ("SINGLE")
                    || trimmed.startsWith ("TUNED")
                    || trimmed.startsWith ("1 DRUM"))
                    return 0;
                if (trimmed.startsWith ("FOUR")
                    || trimmed.startsWith ("FAMILY")
                    || trimmed.startsWith ("4 DRUM"))
                    return 1;
                const float number =
                    text.retainCharacters ("0123456789.-").getFloatValue();
                const float normalised = std::abs (number) > 1.0f
                    ? number * 0.01f : number;
                return normalised < 0.5f ? 0 : 1;
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
        // The model's fixed post-Drive reference calibration owns single-hit
        // headroom. Keep this public range and default stable so existing host
        // automation retains its exact normalised curve.
        juce::NormalisableRange<float> { -24.0f, 6.0f, 0.1f }, -22.5f,
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

    // Appended after the established host slots so every earlier automation
    // index and saved parameter ID keeps its meaning.
    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::strikeAzimuth, 1 }, "Strike Azimuth",
        juce::NormalisableRange<float> { -180.0f, 180.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel (juce::String::fromUTF8 ("\xc2\xb0"))
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 1);
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue();
            })));

    result.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ids::performer, 1 }, "Performer",
        juce::StringArray { "P1", "P2", "P3", "P4" }, 0,
        juce::AudioParameterChoiceAttributes().withAutomatable (false)));

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::velocityCurve, 1 }, "Velocity Curve",
        juce::NormalisableRange<float> { -1.0f, 1.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                const auto amount = juce::roundToInt (std::abs (value) * 100.0f);
                if (amount == 0)
                    return juce::String ("Linear");
                return (value < 0.0f ? juce::String ("Soft ")
                                     : juce::String ("Hard "))
                     + juce::String (amount);
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                const auto trimmed = text.trim().toUpperCase();
                if (trimmed.startsWith ("LINEAR"))
                    return 0.0f;
                const auto amount = std::abs (
                    trimmed.retainCharacters ("0123456789.-").getFloatValue())
                                  * 0.01f;
                return trimmed.startsWith ("SOFT") ? -amount : amount;
            })));

    // Append the new ID and use a higher version hint: AU hosts sort by this
    // before the ID hash, preserving every existing automation index.
    juce::NormalisableRange<float> highPassRange { 0.0f, 500.0f, 1.0f };
    highPassRange.setSkewForCentre (100.0f);
    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::outputHighPass, 2 }, "Output High Pass",
        highPassRange, 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                const auto frequency = juce::roundToInt (value);
                return frequency == 0 ? juce::String ("Off")
                                      : juce::String (frequency) + " Hz";
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue();
            })));

    result.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ids::ensembleSize, 3 }, "Ensemble Size", 1, 8, 1,
        juce::AudioParameterIntAttributes()
            .withStringFromValueFunction ([] (int value, int)
            {
                return value == 1 ? juce::String ("1 player")
                                  : juce::String (value) + " players";
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.getIntValue();
            })));
    result.push_back (makePercentParameter (
        ids::ensembleVariation, "Ensemble Variation", 0.4f, 3));

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
    next.strikeAzimuth = juce::degreesToRadians (read (slotStrikeAzimuth));
    next.performer = juce::roundToInt (read (slotPerformer));
    next.velocityCurve = read (slotVelocityCurve);
    next.outputHighPassHz = read (slotOutputHighPass);
    next.ensembleSize = juce::roundToInt (read (slotEnsembleSize));
    next.ensembleVariation = read (slotEnsembleVariation);
    return next;
}

taikor::TaikoEngine::DrumMeasurements TaikorAudioProcessor::measureDrum (
    int octaveOffset) const noexcept
{
    // The host's rate, because the pitch this reports has to be a partial the
    // engine will actually instantiate at it - see TaikoEngine::soundingMode.
    // Zero means the host has not prepared us yet, and then the measurement's
    // own default stands.
    auto current = snapshotEngineParameters();
    const int azimuth = strikeAzimuthController.load (std::memory_order_relaxed);
    const int position = strikePositionController.load (std::memory_order_relaxed);
    if (azimuth >= 0)
        current.strikeAzimuth = bipolarControllerValue (azimuth)
                              * juce::MathConstants<float>::pi;
    if (position >= 0)
        current.strikePosition = bipolarControllerValue (position);

    const auto rate = displaySampleRate.load (std::memory_order_relaxed);
    return rate > 0.0
             ? taikor::TaikoEngine::measure (current, octaveOffset, 0.0f, rate)
             : taikor::TaikoEngine::measure (current, octaveOffset);
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
    strikeAzimuthController.store (-1, std::memory_order_relaxed);
    strikePositionController.store (-1, std::memory_order_relaxed);
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
    strikeAzimuthController.store (-1, std::memory_order_relaxed);
    strikePositionController.store (-1, std::memory_order_relaxed);
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
        engine.clearStrikeOverrides();
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
        const auto rawValue = static_cast<int> (data[2] & 0x7fu);
        const auto value = static_cast<float> (rawValue) / 127.0f;
        // Standard bipolar MIDI controls have a real centre at 64. Mapping the
        // whole byte through raw/127 would put that value slightly positive,
        // so use the 64 steps below centre and 63 above it explicitly.
        const float bipolarValue = bipolarControllerValue (rawValue);

        if (controller == 1u)
        {
            // A hand laid on the head. It damps what is still ringing without
            // changing anything about the strokes that follow.
            engine.setHandDamping (value);
        }
        else if (controller == 16u)
        {
            strikeAzimuthController.store (rawValue, std::memory_order_relaxed);
            engine.setStrikeAzimuthOverride (
                bipolarValue * juce::MathConstants<float>::pi);
        }
        else if (controller == 17u)
        {
            strikePositionController.store (rawValue, std::memory_order_relaxed);
            engine.setStrikePositionOverride (bipolarValue);
        }
        else if (controller == 121u)
        {
            // Reset All Controllers returns every live gesture to its host
            // parameter: hand off, wheel centred, and strike overrides cleared.
            engine.setHandDamping (0.0f);
            engine.setPitchBend (0.0f);
            engine.clearStrikeOverrides();
            strikeAzimuthController.store (-1, std::memory_order_relaxed);
            strikePositionController.store (-1, std::memory_order_relaxed);
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
    strikeAzimuthController.store (-1, std::memory_order_relaxed);
    strikePositionController.store (-1, std::memory_order_relaxed);
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
