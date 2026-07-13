#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{
template <typename Text>
juce::String toJuceString (const Text& text)
{
    if constexpr (std::is_pointer_v<std::decay_t<Text>>)
        return text != nullptr ? juce::String::fromUTF8 (text) : juce::String {};
    else
    {
        const std::string_view view { text };
        return juce::String::fromUTF8 (view.data(), static_cast<int> (view.size()));
    }
}

constexpr std::array<const char*, drumalor::parameters::count> parameterSuffixes {
    "CharacterA", "CharacterB", "Pitch", "Decay"
};

constexpr bool isValidInstrument (drumalor::Instrument instrument) noexcept
{
    return static_cast<std::size_t> (instrument) < drumalor::instrumentCount;
}

constexpr std::size_t instrumentIndex (drumalor::Instrument instrument) noexcept
{
    return static_cast<std::size_t> (instrument);
}

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
} // namespace

DrumalorAudioProcessor::DrumalorAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "DRUMALOR_STATE", createParameterLayout())
{
    for (std::size_t instrument = 0; instrument < drumalor::instrumentCount; ++instrument)
    {
        const auto drum = static_cast<drumalor::Instrument> (instrument);
        for (int slot = 0; slot < drumalor::parameters::count; ++slot)
        {
            parameterPointers[instrument][static_cast<std::size_t> (slot)] =
                parameters.getRawParameterValue (parameterId (drum, slot));
            jassert (parameterPointers[instrument][static_cast<std::size_t> (slot)] != nullptr);
        }
    }

    outputParameter = parameters.getRawParameterValue (drumalor::parameters::output);
    jassert (outputParameter != nullptr);
}

juce::String DrumalorAudioProcessor::parameterId (drumalor::Instrument instrument, int slot)
{
    if (! isValidInstrument (instrument)
        || slot < 0
        || slot >= drumalor::parameters::count)
    {
        jassertfalse;
        return {};
    }

    return toJuceString (drumalor::getInstrumentSlug (instrument))
         + parameterSuffixes[static_cast<std::size_t> (slot)];
}

juce::AudioProcessorValueTreeState::ParameterLayout
DrumalorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;
    result.reserve (drumalor::instrumentCount * drumalor::parameters::count + 1);

    for (std::size_t instrument = 0; instrument < drumalor::instrumentCount; ++instrument)
    {
        const auto drum = static_cast<drumalor::Instrument> (instrument);
        const auto& metadata = drumalor::getInstrumentMetadata (drum);
        const auto drumName = toJuceString (drumalor::getInstrumentDisplayName (drum));
        const auto characterAName = toJuceString (drumalor::getCharacterALabel (drum));
        const auto characterBName = toJuceString (drumalor::getCharacterBLabel (drum));

        result.push_back (makePercentParameter (
            parameterId (drum, drumalor::parameters::characterA),
            drumName + " " + characterAName, metadata.defaultParameters.characterA));
        result.push_back (makePercentParameter (
            parameterId (drum, drumalor::parameters::characterB),
            drumName + " " + characterBName, metadata.defaultParameters.characterB));

        result.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { parameterId (drum, drumalor::parameters::pitch), 1 },
            drumName + " Pitch",
            juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
            metadata.defaultParameters.pitch,
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

        result.push_back (makePercentParameter (
            parameterId (drum, drumalor::parameters::decay),
            drumName + " Decay", metadata.defaultParameters.decay));
    }

    result.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { drumalor::parameters::output, 1 }, "Output",
        juce::NormalisableRange<float> { -24.0f, 6.0f, 0.1f }, -6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { result.begin(), result.end() };
}

void DrumalorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engineReady.store (false, std::memory_order_release);
    uiQueueGeneration.fetch_add (1, std::memory_order_acq_rel);
    discardUiTriggers();
    // Set the output target before prepare() resets its smoother, so the first
    // rendered hit starts at the restored/default gain instead of gliding down
    // from the DrumEngine standalone default.
    updateEngineParameters();
    engine.prepare (sampleRate, samplesPerBlock);
    displaySampleRate.store (sampleRate, std::memory_order_relaxed);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    engineReady.store (true, std::memory_order_release);
}

void DrumalorAudioProcessor::releaseResources()
{
    engineReady.store (false, std::memory_order_release);
    uiQueueGeneration.fetch_add (1, std::memory_order_acq_rel);
    discardUiTriggers();
    engine.allSoundsOff();
    engine.reset();
    activeVoiceCount.store (0, std::memory_order_relaxed);
    displaySampleRate.store (0.0, std::memory_order_relaxed);
}

bool DrumalorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DrumalorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

    // Editor pad events are deliberately quantised to the next block boundary.
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

void DrumalorAudioProcessor::dispatchMidiData (const juce::uint8* data,
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
            if (const auto instrument = drumalor::instrumentForMidiNote (midiNote))
                registerTrigger (*instrument);
    }
    else if (kind == 0xb0u && numBytes >= 3)
    {
        const auto controller = data[1] & 0x7fu;
        if (controller == 120u || controller == 123u)
            engine.allSoundsOff();
    }
}

void DrumalorAudioProcessor::triggerFromUi (drumalor::Instrument instrument,
                                            float velocity) noexcept
{
    if (! engineReady.load (std::memory_order_acquire))
        return;
    enqueueUiTrigger (instrument, velocity);
}

void DrumalorAudioProcessor::requestPanic() noexcept
{
    // Events published before this generation change are stale even if their
    // producer races the audio-thread queue flush.
    uiQueueGeneration.fetch_add (1, std::memory_order_acq_rel);
    panicRequested.store (true, std::memory_order_release);
}

void DrumalorAudioProcessor::enqueueUiTrigger (drumalor::Instrument instrument,
                                               float velocity) noexcept
{
    if (! isValidInstrument (instrument) || ! std::isfinite (velocity) || velocity <= 0.0f)
        return;

    const auto write = uiWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1u) % uiQueueCapacity;

    if (next == uiReadIndex.load (std::memory_order_acquire))
    {
        // A one-shot UI audition can be dropped; the message thread must never wait.
        return;
    }

    uiTriggerQueue[write] = { instrument, juce::jlimit (0.0f, 1.0f, velocity),
                              uiQueueGeneration.load (std::memory_order_acquire) };
    uiWriteIndex.store (next, std::memory_order_release);
}

void DrumalorAudioProcessor::dispatchUiTriggers() noexcept
{
    auto read = uiReadIndex.load (std::memory_order_relaxed);
    const auto write = uiWriteIndex.load (std::memory_order_acquire);

    while (read != write)
    {
        const auto event = uiTriggerQueue[read];
        if (event.generation == uiQueueGeneration.load (std::memory_order_acquire))
        {
            engine.trigger (event.instrument, event.velocity);
            registerTrigger (event.instrument);
        }
        read = (read + 1u) % uiQueueCapacity;
    }

    uiReadIndex.store (read, std::memory_order_release);
}

void DrumalorAudioProcessor::discardUiTriggers() noexcept
{
    uiReadIndex.store (uiWriteIndex.load (std::memory_order_acquire),
                       std::memory_order_release);
}

void DrumalorAudioProcessor::registerTrigger (drumalor::Instrument instrument) noexcept
{
    if (isValidInstrument (instrument))
        triggerCounters[instrumentIndex (instrument)].fetch_add (1, std::memory_order_release);
}

std::uint32_t DrumalorAudioProcessor::getTriggerCounter (
    drumalor::Instrument instrument) const noexcept
{
    return isValidInstrument (instrument)
        ? triggerCounters[instrumentIndex (instrument)].load (std::memory_order_acquire)
        : 0u;
}

void DrumalorAudioProcessor::updateEngineParameters() noexcept
{
    for (std::size_t instrument = 0; instrument < drumalor::instrumentCount; ++instrument)
    {
        const auto drum = static_cast<drumalor::Instrument> (instrument);
        const auto& pointers = parameterPointers[instrument];

        drumalor::InstrumentParameters next;
        next.characterA = pointers[drumalor::parameters::characterA]->load (std::memory_order_relaxed);
        next.characterB = pointers[drumalor::parameters::characterB]->load (std::memory_order_relaxed);
        next.pitch = pointers[drumalor::parameters::pitch]->load (std::memory_order_relaxed);
        next.decay = pointers[drumalor::parameters::decay]->load (std::memory_order_relaxed);
        engine.setInstrumentParameters (drum, next);
    }

    engine.setOutputGain (juce::Decibels::decibelsToGain (
        outputParameter->load (std::memory_order_relaxed)));
}

void DrumalorAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destinationData);
}

void DrumalorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
        requestPanic();
    }
}

juce::AudioProcessorEditor* DrumalorAudioProcessor::createEditor()
{
    return new DrumalorAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumalorAudioProcessor();
}
