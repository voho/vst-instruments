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

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
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
        drumalor::instrumentCount * drumalor::parameters::count + 1u;
    expect (processor.getParameters().size() == static_cast<int> (expectedParameterCount),
            "processor parameter count is not 13 * 4 + output");

    std::set<std::string> parameterIds;
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto& defaults = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        const std::array expectedValues {
            defaults.characterA, defaults.characterB, defaults.pitch, defaults.decay
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
    }

    expect (approximatelyEqual (
                parameterValue (processor, drumalor::parameters::output), -6.0f),
            "wrong output default");
    expect (parameterIds.insert (drumalor::parameters::output).second,
            "output id collides with an instrument parameter");
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

    setParameterValue (source, kickCharacter, 0.173f);
    setParameterValue (source, crashPitch, 11.4f);
    setParameterValue (source, shakerDecay, 0.812f);
    setParameterValue (source, drumalor::parameters::output, 2.3f);

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

    expect (editor->getWidth() >= 900 && editor->getHeight() >= 600,
            "editor opened below its usable design size");

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

    for (const auto size : std::array { juce::Point<int> { 900, 640 },
                                        juce::Point<int> { 1600, 1000 } })
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
    }

    const auto snapshotPath = juce::SystemStats::getEnvironmentVariable (
        "DRUMALOR_EDITOR_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty())
    {
        juce::FileOutputStream output { juce::File (snapshotPath) };
        juce::PNGImageFormat png;
        const bool wroteSnapshot = output.openedOk()
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
