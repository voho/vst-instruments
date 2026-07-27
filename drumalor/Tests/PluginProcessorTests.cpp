#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
// Must stay in step with PluginEditor.cpp's design constants.
constexpr int editorDesignWidth = 1280;
constexpr int editorDesignHeight = 880;
constexpr int editorMinimumWidth = 1024;
constexpr int editorMinimumHeight = 704;
constexpr int editorMaximumWidth = 1472;
constexpr int editorMaximumHeight = 1012;
constexpr double editorAspectRatio =
    static_cast<double> (editorDesignWidth) / static_cast<double> (editorDesignHeight);
int failureCount = 0;

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

float parameterValue (const DrumalorAudioProcessor& processor, const juce::String& id)
{
    const auto* value = processor.parameters.getRawParameterValue (id);
    expect (value != nullptr, "missing parameter " + id.toStdString());
    return value != nullptr ? value->load (std::memory_order_relaxed) : 0.0f;
}

void setParameterValue (DrumalorAudioProcessor& processor, const juce::String& id,
                        float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    expect (parameter != nullptr, "cannot set missing parameter " + id.toStdString());
    if (parameter != nullptr)
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
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

void render (DrumalorAudioProcessor& processor, juce::AudioBuffer<float>& buffer,
             juce::MidiBuffer& midi)
{
    buffer.setSize (2, blockSize, false, false, true);
    buffer.clear();
    processor.processBlock (buffer, midi);
}

void testParameterLayoutAndDefaults()
{
    DrumalorAudioProcessor processor;
    constexpr auto expectedParameterCount =
        drumalor::instrumentCount * drumalor::parameters::count
        + static_cast<std::size_t> (drumalor::parameters::kitParameterCount);
    expect (processor.getParameters().size() == static_cast<int> (expectedParameterCount),
            "processor parameter count is not 13 * 7 voice controls plus 4 kit controls");

    std::set<std::string> parameterIds;
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto& defaults = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        const std::array expectedValues {
            defaults.characterA, defaults.characterB, defaults.pitch, defaults.decay,
            defaults.level, defaults.pan,
            static_cast<float> (defaults.chokeGroup)
        };

        for (int slot = 0; slot < drumalor::parameters::count; ++slot)
        {
            const auto id = DrumalorAudioProcessor::parameterId (instrument, slot);
            expect (! id.isEmpty(), "empty APVTS parameter id");
            expect (parameterIds.insert (id.toStdString()).second,
                    "duplicate APVTS parameter id " + id.toStdString());
            expect (processor.parameters.getParameter (id) != nullptr,
                    "APVTS does not contain " + id.toStdString());
            expect (approximatelyEqual (
                        parameterValue (processor, id),
                        expectedValues[static_cast<std::size_t> (slot)]),
                    "wrong default for " + id.toStdString());
        }

        // Version 1.0 sessions store the four original controls at these host
        // parameter indices. Keeping the block first is what lets an existing
        // automation lane survive the 1.1 mixer and kit-bus additions.
        for (int slot = 0; slot < drumalor::parameters::originalSlotCount; ++slot)
        {
            const auto expectedIndex = static_cast<int> (index)
                * drumalor::parameters::originalSlotCount + slot;
            const auto* parameter = processor.parameters.getParameter (
                DrumalorAudioProcessor::parameterId (instrument, slot));
            expect (parameter != nullptr
                        && parameter->getParameterIndex() == expectedIndex,
                    "version 1.0 parameter moved away from host index "
                        + std::to_string (expectedIndex));
        }
    }

    expect (approximatelyEqual (
                parameterValue (processor, drumalor::parameters::output), -6.0f),
            "wrong output default");
    const auto* outputParameter = processor.parameters.getParameter (
        drumalor::parameters::output);
    expect (outputParameter != nullptr
                && outputParameter->getParameterIndex()
                       == static_cast<int> (drumalor::instrumentCount)
                              * drumalor::parameters::originalSlotCount,
            "Output moved away from its version 1.0 host parameter index");
    expect (parameterIds.insert (drumalor::parameters::output).second,
            "output id collides with an instrument parameter");

    // Kit-wide controls, all of which must default to preserving the 1.0 sound.
    expect (approximatelyEqual (
                parameterValue (processor, drumalor::parameters::humanise), 0.5f),
            "Humanise does not default to the original fixed variation depth");
    expect (approximatelyEqual (
                parameterValue (processor, drumalor::parameters::busDrive), 0.0f),
            "the bus drive stage does not default to bypassed");
    expect (approximatelyEqual (
                parameterValue (processor, drumalor::parameters::busCompression), 0.0f),
            "the bus compressor does not default to bypassed");
    for (const auto* id : { drumalor::parameters::humanise, drumalor::parameters::busDrive,
                            drumalor::parameters::busCompression })
    {
        expect (processor.parameters.getParameter (id) != nullptr,
                std::string ("APVTS does not contain ") + id);
        expect (parameterIds.insert (id).second,
                std::string ("kit parameter id collides: ") + id);
    }

    // The choke parameter must be a four-way choice with the hi-hat pair
    // sharing group A, so the factory kit still behaves like a pedal link.
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto id = DrumalorAudioProcessor::parameterId (
            instrument, drumalor::parameters::choke);
        const auto* parameter = processor.parameters.getParameter (id);
        expect (parameter != nullptr && parameter->getNumSteps() == 4,
                "choke group is not a four-way choice: " + id.toStdString());
        const auto expectedGroup =
            instrument == drumalor::Instrument::ClosedHat
            || instrument == drumalor::Instrument::OpenHat ? 1.0f : 0.0f;
        expect (approximatelyEqual (parameterValue (processor, id), expectedGroup),
                "wrong default choke group for " + id.toStdString());
    }

    // Ranges of the new continuous controls.
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto levelId = DrumalorAudioProcessor::parameterId (
            instrument, drumalor::parameters::level);
        const auto panId = DrumalorAudioProcessor::parameterId (
            instrument, drumalor::parameters::pan);
        setParameterValue (processor, levelId, 99.0f);
        setParameterValue (processor, panId, 99.0f);
        expect (approximatelyEqual (parameterValue (processor, levelId),
                                    drumalor::maximumVoiceLevelDecibels, 0.051f),
                "channel level exceeded its maximum: " + levelId.toStdString());
        expect (approximatelyEqual (parameterValue (processor, panId), 1.0f, 0.011f),
                "pan exceeded its maximum: " + panId.toStdString());
        setParameterValue (processor, levelId, -99.0f);
        setParameterValue (processor, panId, -99.0f);
        expect (approximatelyEqual (parameterValue (processor, levelId),
                                    drumalor::minimumVoiceLevelDecibels, 0.051f),
                "channel level fell below its minimum: " + levelId.toStdString());
        expect (approximatelyEqual (parameterValue (processor, panId), -1.0f, 0.011f),
                "pan fell below its minimum: " + panId.toStdString());
    }
}

void testNewParametersAffectTheEngine()
{
    DrumalorAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    // Every probe below has to start from silence, otherwise the previous hit's
    // tail is still in the buffer when the next measurement is taken.
    const auto flush = [&processor]
    {
        processor.requestPanic();
        juce::AudioBuffer<float> scratch;
        juce::MidiBuffer none;
        for (int block = 0; block < 4; ++block)
            render (processor, scratch, none);
        expect (processor.getActiveVoiceCount() == 0,
                "panic did not clear the kit between parameter probes");
    };

    const auto renderKick = [&processor, &flush] (juce::AudioBuffer<float>& buffer)
    {
        flush();
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (
                           1, drumalor::getStandardMidiNote (drumalor::Instrument::Kick),
                           static_cast<juce::uint8> (120)),
                       0);
        render (processor, buffer, midi);
    };

    juce::AudioBuffer<float> unityAudio;
    renderKick (unityAudio);
    const auto unityPeak = peakInRange (unityAudio, 0, blockSize);
    expect (unityPeak > 1.0e-4f, "kick level probe produced no audio");

    setParameterValue (processor,
                       DrumalorAudioProcessor::parameterId (
                           drumalor::Instrument::Kick, drumalor::parameters::level),
                       drumalor::minimumVoiceLevelDecibels);
    juce::AudioBuffer<float> quietAudio;
    renderKick (quietAudio);
    expect (peakInRange (quietAudio, 0, blockSize) < 0.25f * unityPeak,
            "the per-voice level parameter did not reach the engine");
    setParameterValue (processor,
                       DrumalorAudioProcessor::parameterId (
                           drumalor::Instrument::Kick, drumalor::parameters::level),
                       0.0f);

    // Pan has to reach the engine as a real stereo placement.
    setParameterValue (processor,
                       DrumalorAudioProcessor::parameterId (
                           drumalor::Instrument::Kick, drumalor::parameters::pan),
                       -1.0f);
    juce::AudioBuffer<float> pannedAudio;
    renderKick (pannedAudio);
    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
    for (int sample = 0; sample < blockSize; ++sample)
    {
        leftPeak = std::max (leftPeak, std::abs (pannedAudio.getSample (0, sample)));
        rightPeak = std::max (rightPeak, std::abs (pannedAudio.getSample (1, sample)));
    }
    expect (leftPeak > 20.0f * rightPeak,
            "the per-voice pan parameter did not reach the engine");
    setParameterValue (processor,
                       DrumalorAudioProcessor::parameterId (
                           drumalor::Instrument::Kick, drumalor::parameters::pan),
                       0.0f);

    // A shared choke group has to cut a ringing voice from a different drum.
    setParameterValue (processor,
                       DrumalorAudioProcessor::parameterId (
                           drumalor::Instrument::Ride, drumalor::parameters::choke),
                       2.0f);
    setParameterValue (processor,
                       DrumalorAudioProcessor::parameterId (
                           drumalor::Instrument::Crash, drumalor::parameters::choke),
                       2.0f);
    flush();
    juce::AudioBuffer<float> chokeAudio;
    juce::MidiBuffer chokeMidi;
    chokeMidi.addEvent (juce::MidiMessage::noteOn (
                            1, drumalor::getStandardMidiNote (drumalor::Instrument::Ride),
                            static_cast<juce::uint8> (127)),
                        0);
    render (processor, chokeAudio, chokeMidi);
    expect (processor.getActiveVoiceCount() == 1,
            "the choke-group probe did not start its ride");
    juce::MidiBuffer crashMidi;
    crashMidi.addEvent (juce::MidiMessage::noteOn (
                            1, drumalor::getStandardMidiNote (drumalor::Instrument::Crash),
                            static_cast<juce::uint8> (127)),
                        0);
    render (processor, chokeAudio, crashMidi);
    expect (processor.getActiveVoiceCount() == 1,
            "a shared choke group set through the host did not cut the ride");

    // The bus compressor must reach the engine and report gain reduction.
    expect (processor.getBusGain() > 0.99f, "an idle bus reported gain reduction");
    setParameterValue (processor, drumalor::parameters::busCompression, 1.0f);
    flush();
    juce::AudioBuffer<float> loudAudio;
    juce::MidiBuffer loudMidi;
    juce::MidiBuffer emptyMidi;
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
        loudMidi.addEvent (juce::MidiMessage::noteOn (
                               1, drumalor::getStandardMidiNote (
                                      static_cast<drumalor::Instrument> (index)),
                               static_cast<juce::uint8> (127)),
                           0);
    render (processor, loudAudio, loudMidi);
    render (processor, loudAudio, emptyMidi);
    expect (processor.getBusGain() < 0.95f,
            "the bus compression parameter did not reach the engine");
    setParameterValue (processor, drumalor::parameters::busCompression, 0.0f);

    // Metering must be published for the editor.
    expect (processor.getOutputLevel (0) > 1.0e-4f
                && processor.getOutputLevel (1) > 1.0e-4f,
            "the processor published no output level while the kit was sounding");
    expect (processor.getInstrumentLevel (drumalor::Instrument::Kick) > 1.0e-4f,
            "the processor published no channel level while the kick was sounding");

    processor.releaseResources();
}

void testStateRoundTrip()
{
    DrumalorAudioProcessor source;
    const auto kickCharacter = DrumalorAudioProcessor::parameterId (
        drumalor::Instrument::Kick, drumalor::parameters::characterA);
    const auto crashPitch = DrumalorAudioProcessor::parameterId (
        drumalor::Instrument::Crash, drumalor::parameters::pitch);
    const auto shakerDecay = DrumalorAudioProcessor::parameterId (
        drumalor::Instrument::Shaker, drumalor::parameters::decay);

    const auto snareLevel = DrumalorAudioProcessor::parameterId (
        drumalor::Instrument::Snare, drumalor::parameters::level);
    const auto ridePan = DrumalorAudioProcessor::parameterId (
        drumalor::Instrument::Ride, drumalor::parameters::pan);
    const auto perc1Choke = DrumalorAudioProcessor::parameterId (
        drumalor::Instrument::Perc1, drumalor::parameters::choke);

    setParameterValue (source, kickCharacter, 0.173f);
    setParameterValue (source, crashPitch, 11.4f);
    setParameterValue (source, shakerDecay, 0.812f);
    setParameterValue (source, drumalor::parameters::output, 2.3f);
    setParameterValue (source, snareLevel, -9.4f);
    setParameterValue (source, ridePan, -0.62f);
    setParameterValue (source, perc1Choke, 3.0f);
    setParameterValue (source, drumalor::parameters::humanise, 0.91f);
    setParameterValue (source, drumalor::parameters::busDrive, 0.37f);
    setParameterValue (source, drumalor::parameters::busCompression, 0.64f);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    expect (state.getSize() > 0, "processor produced empty state");

    DrumalorAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    expect (approximatelyEqual (parameterValue (restored, kickCharacter), 0.173f, 0.0011f),
            "kick character did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, crashPitch), 11.4f, 0.051f),
            "crash pitch did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, shakerDecay), 0.812f, 0.0011f),
            "shaker decay did not survive state round-trip");
    expect (approximatelyEqual (
                parameterValue (restored, drumalor::parameters::output), 2.3f, 0.051f),
            "output did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, snareLevel), -9.4f, 0.051f),
            "snare channel level did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, ridePan), -0.62f, 0.011f),
            "ride pan did not survive state round-trip");
    expect (approximatelyEqual (parameterValue (restored, perc1Choke), 3.0f, 0.011f),
            "perc 1 choke group did not survive state round-trip");
    expect (approximatelyEqual (
                parameterValue (restored, drumalor::parameters::humanise), 0.91f, 0.0011f),
            "humanise did not survive state round-trip");
    expect (approximatelyEqual (
                parameterValue (restored, drumalor::parameters::busDrive), 0.37f, 0.0011f),
            "bus drive did not survive state round-trip");
    expect (approximatelyEqual (
                parameterValue (restored, drumalor::parameters::busCompression),
                0.64f, 0.0011f),
            "bus compression did not survive state round-trip");

    // A version 1.0 session predates every 1.1 parameter. Restoring one - the
    // same replaceState() path setStateInformation() uses - must keep the values
    // it does carry and leave every new control at its default.
    DrumalorAudioProcessor legacyWriter;
    setParameterValue (legacyWriter, kickCharacter, 0.421f);
    auto legacyTree = legacyWriter.parameters.copyState();
    const juce::StringArray newIds {
        snareLevel, ridePan, perc1Choke, drumalor::parameters::humanise,
        drumalor::parameters::busDrive, drumalor::parameters::busCompression
    };
    int removedChildren = 0;
    for (int child = legacyTree.getNumChildren(); --child >= 0;)
    {
        if (newIds.contains (legacyTree.getChild (child).getProperty ("id").toString()))
        {
            legacyTree.removeChild (child, nullptr);
            ++removedChildren;
        }
    }
    expect (removedChildren == newIds.size(),
            "could not build a version 1.0 state without the new parameters");

    // Load it the way a host does. Going through setStateInformation() rather
    // than replaceState() is what exercises the migration.
    juce::MemoryBlock legacyStored;
    if (const auto legacyXml = legacyTree.createXml())
        juce::AudioProcessor::copyXmlToBinary (*legacyXml, legacyStored);
    else
        expect (false, "the version 1.0 state could not be serialised");

    const auto loadLegacyState = [&legacyStored] (DrumalorAudioProcessor& target)
    {
        target.setStateInformation (legacyStored.getData(),
                                    static_cast<int> (legacyStored.getSize()));
    };

    DrumalorAudioProcessor legacyReader;
    loadLegacyState (legacyReader);
    expect (approximatelyEqual (
                parameterValue (legacyReader, kickCharacter), 0.421f, 0.0011f),
            "a version 1.0 session lost its stored value");
    expect (approximatelyEqual (
                parameterValue (legacyReader, drumalor::parameters::humanise), 0.5f),
            "a version 1.0 session did not restore the original variation depth");
    expect (approximatelyEqual (
                parameterValue (legacyReader, drumalor::parameters::busDrive), 0.0f)
                && approximatelyEqual (
                       parameterValue (legacyReader,
                                       drumalor::parameters::busCompression), 0.0f),
            "a version 1.0 session did not leave the kit bus bypassed");
    expect (approximatelyEqual (parameterValue (legacyReader, snareLevel), 0.0f)
                && approximatelyEqual (
                       parameterValue (legacyReader, ridePan),
                       drumalor::getInstrumentMetadata (drumalor::Instrument::Ride)
                           .defaultParameters.pan, 0.011f),
            "a version 1.0 session did not restore the original kit mixer defaults");

    // The same load into an instance that has already been used. APVTS keeps a
    // parameter's live value when the stored tree has no child for it, so
    // without the migration a legacy preset would inherit whatever the player
    // had dialled in -- an old kit recalled with the bus still driven.
    DrumalorAudioProcessor usedReader;
    setParameterValue (usedReader, drumalor::parameters::busDrive, 0.83f);
    setParameterValue (usedReader, drumalor::parameters::busCompression, 0.77f);
    setParameterValue (usedReader, drumalor::parameters::humanise, 0.91f);
    setParameterValue (usedReader, snareLevel, -6.0f);
    loadLegacyState (usedReader);

    DrumalorAudioProcessor pristine;
    for (const auto& id : newIds)
        expect (approximatelyEqual (parameterValue (usedReader, id),
                                    parameterValue (pristine, id), 0.0011f),
                ("a version 1.0 session kept the live value of " + id
                 + " instead of its default").toStdString());
    expect (approximatelyEqual (
                parameterValue (usedReader, kickCharacter), 0.421f, 0.0011f),
            "reloading into a used instance lost the stored value");
}

void testSampleAccurateMidiAndMappings()
{
    DrumalorAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    constexpr int eventSample = 173;
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (
                       1, drumalor::getStandardMidiNote (drumalor::Instrument::Kick),
                       static_cast<juce::uint8> (112)),
                   eventSample);
    render (processor, audio, midi);

    expect (peakInRange (audio, 0, eventSample) == 0.0f,
            "MIDI note rendered before its sample offset");
    expect (peakInRange (audio, eventSample, blockSize) > 1.0e-5f,
            "MIDI note produced no audio after its sample offset");
    expect (processor.getTriggerCounter (drumalor::Instrument::Kick) == 1u,
            "kick MIDI trigger was not registered");

    juce::MidiBuffer allNotes;
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        allNotes.addEvent (juce::MidiMessage::noteOn (
                               1, drumalor::getStandardMidiNote (instrument),
                               static_cast<juce::uint8> (100)),
                           static_cast<int> (index) * 8);
    }
    render (processor, audio, allNotes);

    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto expected = instrument == drumalor::Instrument::Kick ? 2u : 1u;
        expect (processor.getTriggerCounter (instrument) == expected,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " MIDI note was not routed through the processor");
    }

    processor.releaseResources();
}

void testInitialOutputGain()
{
    DrumalorAudioProcessor quiet;
    DrumalorAudioProcessor loud;
    setParameterValue (quiet, drumalor::parameters::output, -24.0f);
    setParameterValue (loud, drumalor::parameters::output, 6.0f);
    quiet.prepareToPlay (sampleRate, blockSize);
    loud.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> quietAudio;
    juce::AudioBuffer<float> loudAudio;
    juce::MidiBuffer quietMidi;
    juce::MidiBuffer loudMidi;
    const auto message = juce::MidiMessage::noteOn (
        1, drumalor::getStandardMidiNote (drumalor::Instrument::Kick),
        static_cast<juce::uint8> (112));
    quietMidi.addEvent (message, 0);
    loudMidi.addEvent (message, 0);
    render (quiet, quietAudio, quietMidi);
    render (loud, loudAudio, loudMidi);

    const auto quietAttack = peakInRange (quietAudio, 0, 64);
    const auto loudAttack = peakInRange (loudAudio, 0, 64);
    expect (quietAttack > 0.0f && loudAttack > quietAttack * 5.0f,
            "initial output gain was not applied before the first rendered hit");

    quiet.releaseResources();
    loud.releaseResources();
}

void testAllNotesOff()
{
    DrumalorAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (
                       1, drumalor::getStandardMidiNote (drumalor::Instrument::Crash),
                       static_cast<juce::uint8> (127)),
                   0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), 128);
    render (processor, audio, midi);

    expect (peakInRange (audio, 0, 128) > 1.0e-5f,
            "all-notes-off test did not render the leading note");
    expect (processor.getTriggerCounter (drumalor::Instrument::Crash) == 1u,
            "all-notes-off test note was not registered");
    expect (processor.getActiveVoiceCount() == 0,
            "MIDI CC123 did not stop all active voices");

    processor.releaseResources();
}

void testUiQueueAndLifecycle()
{
    DrumalorAudioProcessor processor;
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    processor.triggerFromUi (drumalor::Instrument::Snare, 0.9f);
    render (processor, audio, midi);
    expect (processor.getTriggerCounter (drumalor::Instrument::Snare) == 0u,
            "offline UI trigger was accepted");
    expect (peakInRange (audio, 0, blockSize) == 0.0f,
            "unprepared processor emitted audio");

    processor.prepareToPlay (sampleRate, blockSize);
    expect (processor.isEngineReady(), "processor did not become ready after prepare");
    expect (std::abs (processor.getCurrentSampleRateForDisplay() - sampleRate) < 0.01,
            "display sample rate was not updated by prepare");

    processor.triggerFromUi (drumalor::Instrument::Snare, 0.9f);
    render (processor, audio, midi);
    expect (processor.getTriggerCounter (drumalor::Instrument::Snare) == 1u,
            "UI trigger was not dispatched at the next block boundary");
    expect (peakInRange (audio, 0, blockSize) > 1.0e-5f,
            "UI trigger produced no audio");

    processor.requestPanic();
    processor.triggerFromUi (drumalor::Instrument::Ride, 0.9f);
    render (processor, audio, midi);
    expect (processor.getTriggerCounter (drumalor::Instrument::Ride) == 0u,
            "panic did not discard its stale UI queue generation");
    expect (processor.getActiveVoiceCount() == 0,
            "panic did not stop active voices");

    processor.releaseResources();
    expect (! processor.isEngineReady(), "processor remained ready after release");
    expect (processor.getCurrentSampleRateForDisplay() == 0.0,
            "release did not clear the display sample rate");
    processor.triggerFromUi (drumalor::Instrument::Clap, 1.0f);
    render (processor, audio, midi);
    expect (processor.getTriggerCounter (drumalor::Instrument::Clap) == 0u,
            "UI trigger was accepted after release");
}

void testEditorRendering()
{
    DrumalorAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    expect (editor != nullptr, "processor did not create an editor");
    if (editor == nullptr)
        return;

    expect (editor->getWidth() >= editorMinimumWidth
                && editor->getHeight() >= editorMinimumHeight,
            "editor opened below its usable design size");
    auto* constrainer = editor->getConstrainer();
    expect (constrainer != nullptr, "editor has no resize constrainer");
    if (constrainer != nullptr)
    {
        expect (constrainer->getMinimumWidth() == editorMinimumWidth
                    && constrainer->getMinimumHeight() == editorMinimumHeight,
                "editor resize minimum does not protect the control layout");
        expect (constrainer->getMaximumWidth() == editorMaximumWidth
                    && constrainer->getMaximumHeight() == editorMaximumHeight,
                "editor resize maximum does not match the polished layout");
        expect (std::abs (constrainer->getFixedAspectRatio() - editorAspectRatio)
                    < 1.0e-9,
                "editor no longer preserves its design aspect ratio");

        const auto limits = juce::Rectangle<int> (-10000, -10000, 20000, 20000);
        for (const auto requestedSize : std::array {
                 juce::Point<int> { 420, 260 }, juce::Point<int> { 1900, 1200 } })
        {
            auto constrained = juce::Rectangle<int> (
                0, 0, requestedSize.x, requestedSize.y);
            constrainer->checkBounds (constrained, editor->getBounds(), limits,
                                      false, false, true, true);
            expect (constrained.getWidth() >= editorMinimumWidth
                        && constrained.getWidth() <= editorMaximumWidth
                        && constrained.getHeight() >= editorMinimumHeight
                        && constrained.getHeight() <= editorMaximumHeight,
                    "user resize escaped the configured editor limits");
            expect (std::abs (static_cast<double> (constrained.getWidth())
                                 / static_cast<double> (constrained.getHeight())
                                 - editorAspectRatio) < 0.002,
                    "user resize did not preserve the editor aspect ratio");
        }
    }

    juce::Image snapshot (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    juce::Graphics graphics (snapshot);
    editor->paintEntireComponent (graphics, true);

    std::set<juce::uint32> sampledColours;
    int opaqueSamples = 0;
    int sampledPixels = 0;
    for (int y = 4; y < snapshot.getHeight(); y += 8)
    {
        for (int x = 4; x < snapshot.getWidth(); x += 8)
        {
            const auto pixel = snapshot.getPixelAt (x, y);
            sampledColours.insert (pixel.getARGB());
            opaqueSamples += pixel.getAlpha() >= 250 ? 1 : 0;
            ++sampledPixels;
        }
    }

    expect (sampledPixels > 0 && opaqueSamples == sampledPixels,
            "opaque editor left transparent pixels in its rendered surface");
    expect (sampledColours.size() > 512u,
            "editor snapshot lacks the embedded vintage texture or visual detail");

    for (const auto size : std::array {
             juce::Point<int> { editorMinimumWidth, editorMinimumHeight },
             juce::Point<int> { editorMaximumWidth, editorMaximumHeight } })
    {
        editor->setSize (size.x, size.y);
        juce::Image resizedSnapshot (
            juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        juce::Graphics resizedGraphics (resizedSnapshot);
        editor->paintEntireComponent (resizedGraphics, true);

        std::set<juce::uint32> resizeColours;
        bool resizeOpaque = true;
        for (int y = 5; y < resizedSnapshot.getHeight(); y += 13)
        {
            for (int x = 5; x < resizedSnapshot.getWidth(); x += 13)
            {
                const auto pixel = resizedSnapshot.getPixelAt (x, y);
                resizeColours.insert (pixel.getARGB());
                resizeOpaque = resizeOpaque && pixel.getAlpha() >= 250;
            }
        }
        expect (resizeOpaque,
                "resized editor left transparent pixels at "
                    + std::to_string (size.x) + "x" + std::to_string (size.y));
        expect (resizeColours.size() > 256u,
                "resized editor lost visual structure at "
                    + std::to_string (size.x) + "x" + std::to_string (size.y));

        std::vector<juce::Rectangle<int>> knobBounds;
        std::vector<juce::Rectangle<int>> padBounds;
        int padCount = 0;
        for (auto* child : editor->getChildren())
        {
            if (! child->isVisible())
                continue;
            expect (editor->getLocalBounds().contains (child->getBounds()),
                    "visible editor control escaped its bounds at "
                        + std::to_string (size.x) + "x" + std::to_string (size.y));
            if (dynamic_cast<DrumalorPad*> (child) != nullptr)
            {
                ++padCount;
                padBounds.push_back (child->getBounds());
                expect (child->getWidth() >= 110 && child->getHeight() >= 58,
                        "drum pad fell below a practical hit target at "
                            + std::to_string (size.x) + "x" + std::to_string (size.y));
            }
            if (dynamic_cast<DrumalorKnob*> (child) != nullptr)
                knobBounds.push_back (child->getBounds());
        }
        expect (padCount == static_cast<int> (drumalor::instrumentCount),
                "editor did not lay out all 13 drum pads");
        // Five per-voice knobs plus the four kit-bus knobs.
        expect (knobBounds.size() == 9u, "editor did not lay out all nine knobs");
        for (std::size_t first = 0; first < knobBounds.size(); ++first)
            for (std::size_t second = first + 1u; second < knobBounds.size(); ++second)
                expect (! knobBounds[first].intersects (knobBounds[second]),
                        "knob control regions overlap at "
                            + std::to_string (size.x) + "x" + std::to_string (size.y));
        for (std::size_t first = 0; first < padBounds.size(); ++first)
            for (std::size_t second = first + 1u; second < padBounds.size(); ++second)
                expect (! padBounds[first].intersects (padBounds[second]),
                        "drum pad regions overlap at "
                            + std::to_string (size.x) + "x" + std::to_string (size.y));
    }

    std::vector<DrumalorPad*> pads;
    std::vector<juce::Slider*> sliders;
    std::vector<juce::ComboBox*> comboBoxes;
    int meterCount = 0;
    for (auto* child : editor->getChildren())
    {
        if (auto* pad = dynamic_cast<DrumalorPad*> (child))
            pads.push_back (pad);
        if (auto* knob = dynamic_cast<DrumalorKnob*> (child))
            for (auto* knobChild : knob->getChildren())
                if (auto* slider = dynamic_cast<juce::Slider*> (knobChild))
                    sliders.push_back (slider);
        // The pan control is a direct child rather than a rotary knob.
        if (auto* slider = dynamic_cast<juce::Slider*> (child))
            sliders.push_back (slider);
        if (auto* box = dynamic_cast<juce::ComboBox*> (child))
            comboBoxes.push_back (box);
        if (dynamic_cast<DrumalorBusMeter*> (child) != nullptr)
            ++meterCount;
    }
    expect (meterCount == 1, "editor did not lay out its kit bus meter");
    expect (comboBoxes.size() == 1u, "editor did not lay out the choke group selector");
    if (comboBoxes.size() == 1u)
    {
        expect (comboBoxes[0]->getNumItems() == 4,
                "the choke group selector does not offer Off plus three groups");
        expect (comboBoxes[0]->getSelectedItemIndex() == 0,
                "the choke group selector did not follow the selected Kick default");
    }

    const auto sliderNamed = [&sliders] (const juce::String& name)
    {
        const auto found = std::find_if (
            sliders.begin(), sliders.end(), [&name] (const juce::Slider* slider)
            {
                return slider->getName() == name;
            });
        return found != sliders.end() ? *found : nullptr;
    };
    auto* masterOutput = sliderNamed ("MASTER OUTPUT");
    expect (masterOutput != nullptr
                && masterOutput->isDoubleClickReturnEnabled()
                && std::abs (masterOutput->getDoubleClickReturnValue() + 6.0) < 1.0e-6,
            "master output reset no longer returns to -6 dB");
    expect (masterOutput != nullptr && masterOutput->getTooltip().isNotEmpty(),
            "master output has no tooltip");
    expect (pads.size() == drumalor::instrumentCount,
            "could not inspect every drum pad reset contract");
    for (std::size_t index = 0; index < pads.size(); ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto foundPad = std::find_if (
            pads.begin(), pads.end(), [instrument] (const auto* candidate)
        {
            return candidate->getInstrument() == instrument;
        });
        expect (foundPad != pads.end(), "missing drum pad for instrument index "
                    + std::to_string (index));
        if (foundPad == pads.end())
            continue;
        auto* pad = *foundPad;
        if (! pad->getToggleState())
            pad->setToggleState (true, juce::sendNotification);

        const auto& metadata = drumalor::getInstrumentMetadata (instrument);
        const auto labelFor = [] (std::string_view label)
        {
            return juce::String::fromUTF8 (label.data(), static_cast<int> (label.size()))
                .toUpperCase();
        };
        const std::array resetContracts {
            std::pair { labelFor (metadata.characterALabel),
                        static_cast<double> (metadata.defaultParameters.characterA) },
            std::pair { labelFor (metadata.characterBLabel),
                        static_cast<double> (metadata.defaultParameters.characterB) },
            std::pair { juce::String { "PITCH" },
                        static_cast<double> (metadata.defaultParameters.pitch) },
            std::pair { juce::String { "DECAY" },
                        static_cast<double> (metadata.defaultParameters.decay) },
            std::pair { juce::String { "LEVEL" },
                        static_cast<double> (metadata.defaultParameters.level) },
            std::pair { juce::String { "PAN" },
                        static_cast<double> (metadata.defaultParameters.pan) }
        };
        for (const auto& [name, expectedReset] : resetContracts)
        {
            auto* slider = sliderNamed (name);
            expect (slider != nullptr, "missing selected-voice slider " + name.toStdString());
            if (slider != nullptr)
            {
                expect (slider->isDoubleClickReturnEnabled()
                            && std::abs (slider->getDoubleClickReturnValue() - expectedReset)
                                < 1.0e-6,
                        "incorrect double-click reset for "
                            + std::string (metadata.displayName) + " " + name.toStdString());
                expect (slider->getTooltip().isNotEmpty(),
                        "selected-voice slider has no tooltip: " + name.toStdString());
            }
        }
        expect (pad->getTooltip().isNotEmpty(),
                "drum pad has no tooltip: " + std::string (metadata.displayName));
        if (comboBoxes.size() == 1u)
            expect (comboBoxes[0]->getSelectedItemIndex()
                        == metadata.defaultParameters.chokeGroup,
                    "the choke selector did not follow "
                        + std::string (metadata.displayName));
    }

    // The kit-bus deck is not per-voice: it must stay attached and resettable.
    for (const auto& [name, expectedReset] : std::array {
             std::pair { juce::String { "HUMANISE" }, 0.5 },
             std::pair { juce::String { "BUS DRIVE" }, 0.0 },
             std::pair { juce::String { "BUS COMP" }, 0.0 } })
    {
        auto* slider = sliderNamed (name);
        expect (slider != nullptr, "missing kit bus slider " + name.toStdString());
        if (slider != nullptr)
        {
            expect (slider->isDoubleClickReturnEnabled()
                        && std::abs (slider->getDoubleClickReturnValue() - expectedReset)
                               < 1.0e-6,
                    "incorrect double-click reset for " + name.toStdString());
            expect (slider->getTooltip().isNotEmpty(),
                    "kit bus slider has no tooltip: " + name.toStdString());
        }
    }

    const auto snapshotPath = juce::SystemStats::getEnvironmentVariable (
        "DRUMALOR_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty())
    {
        juce::FileOutputStream output { juce::File (snapshotPath) };
        juce::PNGImageFormat png;
        const bool preparedOutput = output.openedOk()
            && output.setPosition (0)
            && output.truncate();
        const bool wroteSnapshot = preparedOutput
            && png.writeImageToStream (snapshot, output);
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
    testNewParametersAffectTheEngine();
    testStateRoundTrip();
    testSampleAccurateMidiAndMappings();
    testInitialOutputGain();
    testAllNotesOff();
    testUiQueueAndLifecycle();
    testEditorRendering();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " processor contract test(s) failed\n";
        return 1;
    }

    std::cout << "All Drumalor processor contract tests passed\n";
    return 0;
}
